#!/usr/bin/env python3
"""
mlqTriage: the replacement for mlqAutomate.py (RM #38234).

Files mailing-list email (genome@, genome-mirror@, genome-www@) into Redmine MLQ
tickets. The design is in the RM #38234 plan; this file grows one build step at a
time (section 9 of the plan).

It reads the last few days of list mail and the Google Groups pending queue and, for
every message it has not handled before: looks up its ticket exactly (thread, reply
headers), classifies and cleans it (Stage 1), matches it by subject when needed
(Stage 2), decides any reopen (Stage 3) and hub routing (Stage 4), drafts a reply for
a new ticket, and records all of it in a state file and a decision log.
  --shadow  writes nothing to Gmail or Redmine, and compares itself with mlqAutomate.py
  --live    approves or holds, files tickets and comments, reopens, posts drafts

Run as qateam (Gmail token, Redmine key and Claude token come from qateam's files):
    mlqTriage.py --shadow                 # one shadow pass
    mlqTriage.py --live                   # one live pass (after cutover)
    mlqTriage.py --shadow --selftest      # check Gmail, Redmine and claude -p, then exit
"""

import os
import sys
import re
import json
import time
import fcntl
import base64
import signal
import socket
import sqlite3
import logging
import argparse
import subprocess
import email
import html as htmlModule
from email import policy
from email.utils import parseaddr
from email.mime.text import MIMEText
from datetime import datetime, timedelta, timezone

import pytz
import requests
from google.oauth2.credentials import Credentials
from google.auth.transport.requests import Request
from googleapiclient.discovery import build

CONFIG = {
    'REDMINE_URL': 'https://redmine.gi.ucsc.edu',
    'REDMINE_PROJECT': 'maillists',
    'CALENDAR_ID': 'ucsc.edu_anbl4254jlssgo3gc2l5c8un5c@group.calendar.google.com',
    'MODERATED_LISTS': ['genome@soe.ucsc.edu', 'genome-mirror@soe.ucsc.edu'],
    'UNMODERATED_LISTS': ['genome-www@soe.ucsc.edu'],
    'ALERT_TO': 'browserqa-group@ucsc.edu',

    # Calendar first names to Redmine names (copied from mlqAutomate.py)
    'NAME_MAPPING': {
        'Jairo': 'Jairo Navarro',
        'Lou': 'Lou Nassar',
        'Gerardo': 'Gerardo Perez',
        'Gera': 'Gerardo Perez',
        'Clay': 'Clay Fischer',
        'Matt': 'Matt Speir',
    },

    # How far back each run reads. Anything not yet in the state file inside this
    # window is processed, so an outage shorter than this loses nothing.
    'FETCH_DAYS': 3,

    # claude -p settings. Aliases, not pinned ids, so a model retirement is not an
    # outage (plan section 7). Each call can fall back to the other model.
    'CLAUDE_CLI': os.path.expanduser('~/.local/bin/claude'),
    'MODEL_JUDGE': 'sonnet',
    'MODEL_JUDGE_FALLBACK': 'opus',
    'MODEL_DRAFT': 'opus',
    'MODEL_DRAFT_FALLBACK': 'sonnet',
    'ASK_TIMEOUT': 180,          # seconds per claude -p call
    'CALL_CAP': 60,              # claude -p calls per run before the run stops and alerts

    'LOCK_STALE_SECONDS': 2 * 3600,
    'ALERT_REPEAT_SECONDS': 24 * 3600,
    'DECISION_LOG_MONTHS': 12,
    'GOOGLE_TIMEOUT': 60,        # seconds, applied to every Google API socket
}

GMAIL_TOKEN_PATH = os.path.expanduser('~/.gmail_token.json')
HG_CONF_PATH = os.path.expanduser('~/.hg.conf')
HG_CONF = {}

# Redmine database: read-only, the same hgcat login redmineCli sql uses. Its
# timestamps are naive local Pacific time.
DB_DEFAULTS = {'host': 'redmine.gi.ucsc.edu', 'name': 'redmine', 'port': 3306}
STATUS_REJECTED = 6
# An active-account list older than this, or shorter than this, is not trusted:
# the run then reopens nothing (plan section 3, "fails closed").
ACCOUNTS_MAX_AGE_HOURS = 48
ACCOUNTS_REFRESH_HOURS = 20
ACCOUNTS_MIN_ROWS = 25
# How long after a message arrives the old script's action on it is looked for,
# and when to stop looking. A held message can sit days before someone approves
# it (2026-10 certificate-outage reports), so the window is a week; matching
# beyond a lone new ticket needs the thread or the subject to agree.
OBSERVE_WINDOW_HOURS = 7 * 24
OBSERVE_GIVE_UP_HOURS = 7 * 24
PST = pytz.timezone('America/Los_Angeles')
SCRIPT_NAME = 'mlqTriage.py'

logger = logging.getLogger('mlqTriage')

# Set from the command line. Only shadow mode exists in step 0.
SHADOW = True


# ---------------------------------------------------------------------------
# Configuration and logging

def loadHgConf():
    """Read redmine.apiKey and claude.oauthToken from ~/.hg.conf (must be mode 600)."""
    if not os.path.exists(HG_CONF_PATH):
        sys.exit(f"{HG_CONF_PATH} not found")
    mode = os.stat(HG_CONF_PATH).st_mode & 0o777
    if mode != 0o600:
        sys.exit(f"{HG_CONF_PATH} has mode {oct(mode)}; it must be 600")
    values = HG_CONF
    with open(HG_CONF_PATH) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#') or '=' not in line:
                continue
            key, value = line.split('=', 1)
            values[key.strip()] = value.strip()
    if not values.get('redmine.apiKey'):
        sys.exit(f"redmine.apiKey missing from {HG_CONF_PATH}")
    CONFIG['REDMINE_API_KEY'] = values['redmine.apiKey']
    # Same precedence as codeReviewAi.py's ensure_claude_auth(): an environment
    # token wins, then claude.oauthToken, then the CLI's own local login.
    if not os.environ.get('CLAUDE_CODE_OAUTH_TOKEN') and values.get('claude.oauthToken'):
        os.environ['CLAUDE_CODE_OAUTH_TOKEN'] = values['claude.oauthToken']


def setupLogging(stateDir, verbose):
    """Log to <stateDir>/mlqTriage.log and to stdout."""
    level = logging.DEBUG if verbose else logging.INFO
    fmt = logging.Formatter('%(asctime)s [%(levelname)s] %(message)s')
    for handler in (logging.FileHandler(os.path.join(stateDir, 'mlqTriage.log')),
                    logging.StreamHandler(sys.stdout)):
        handler.setFormatter(fmt)
        logger.addHandler(handler)
    logger.setLevel(level)


def makeStateDir(stateDir):
    """Create the state directory readable by its owner only (it holds user mail)."""
    os.makedirs(stateDir, exist_ok=True)
    os.chmod(stateDir, 0o700)


# ---------------------------------------------------------------------------
# Single-instance lock

def takeLock(stateDir):
    """Return an open, flock'ed lock file, or None if another run holds it.

    A run that holds the lock for longer than LOCK_STALE_SECONDS is treated as
    hung: alert once, terminate it if it is one of ours, and let the next run in."""
    path = os.path.join(stateDir, 'lock')
    lockFile = open(path, 'a+')
    try:
        fcntl.flock(lockFile, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        lockFile.seek(0)
        holder = lockFile.read().split()
        lockFile.close()
        if len(holder) == 2:
            pid, started = int(holder[0]), float(holder[1])
            age = time.time() - started
            if age > CONFIG['LOCK_STALE_SECONDS']:
                ours = isOurProcess(pid)
                msg = (f"pid {pid} has held the lock for {age / 60:.0f} minutes. "
                       + ("Treating it as a hung run and terminating it." if ours else
                          "It is not an mlqTriage process, so it was left alone."))
                logger.error(msg)
                sendAlert('stale-lock', 'mlqTriage run hung', msg)
                if ours:
                    os.kill(pid, signal.SIGTERM)
                return None
        logger.info("Another run holds the lock; exiting.")
        return None
    lockFile.seek(0)
    lockFile.truncate()
    lockFile.write(f"{os.getpid()} {time.time()}\n")
    lockFile.flush()
    return lockFile


def isOurProcess(pid):
    """True if pid is a running mlqTriage.py process."""
    try:
        with open(f"/proc/{pid}/cmdline", 'rb') as f:
            return SCRIPT_NAME.encode() in f.read()
    except OSError:
        return False


# ---------------------------------------------------------------------------
# State file

STATE_SCHEMA = """
create table if not exists messages (
    messageId text primary key,     -- RFC Message-Id, or 'gmail:<id>' when missing
    gmailId text,
    gmailThreadId text,
    lists text,                     -- comma-separated list addresses
    source text,                    -- 'pending' or 'delivered'
    fromAddr text,
    subject text,
    receivedAt text,                -- when Gmail received it (UTC ISO)
    firstSeen text,
    action text,                    -- 'facts', then 'classified' or 'error'
    attempts integer default 0,     -- Stage 1 attempts
    ticketId integer,               -- the ticket this message belongs to
    oldAction text,                 -- shadow: what mlqAutomate.py did (created/appended/none)
    oldTicketId integer,
    oldReopened integer,
    oldRef text,                    -- 'issue:<id>' or 'journal:<id>' it was matched to
    observedAt text,
    moderationDone text,            -- live: 'approved' or 'held', once done
    writtenTicket integer,          -- live: the ticket this message was filed on, once done
    writeAttempts integer default 0
);
create table if not exists threads (
    gmailThreadId text primary key,
    ticketId integer
);
create table if not exists judgments (
    key text primary key,           -- e.g. 'disposition:sender:<addr>'
    value text,                     -- JSON
    model text,
    createdAt text,
    expiresAt text
);
create table if not exists accounts (
    address text primary key,       -- lower-cased address on an active Redmine account
    login text,
    refreshedAt text
);
create table if not exists meta (
    key text primary key,
    value text
);
"""


def openState(stateDir):
    """Open (and if needed create) the SQLite state file."""
    conn = sqlite3.connect(os.path.join(stateDir, 'state.sqlite'))
    conn.executescript(STATE_SCHEMA)
    # Columns added after a state file was first created
    have = {row[1] for row in conn.execute("pragma table_info(messages)")}
    for column, ctype in (('attempts', 'integer default 0'),
                          ('oldAction', 'text'), ('oldTicketId', 'integer'),
                          ('oldReopened', 'integer'), ('oldRef', 'text'),
                          ('observedAt', 'text'), ('moderationDone', 'text'),
                          ('writtenTicket', 'integer'), ('writeAttempts', 'integer default 0')):
        if column not in have:
            conn.execute(f"alter table messages add column {column} {ctype}")
    conn.commit()
    return conn


def metaGet(conn, key):
    row = conn.execute("select value from meta where key=?", (key,)).fetchone()
    return row[0] if row else None


def metaSet(conn, key, value):
    conn.execute("insert or replace into meta(key, value) values (?, ?)", (key, value))
    conn.commit()


def utcNow():
    return datetime.now(timezone.utc).isoformat(timespec='seconds')


# ---------------------------------------------------------------------------
# Decision log: one JSON line per decision, monthly files, pruned after a year

DECISION_LOG_DIR = None


def logDecision(kind, messageId, **fields):
    """Append one decision to this month's decision log.

    The log is how the shadow is judged and how any later "why did it do that"
    is answered, so every decision lands here with its inputs and reason.
    Record kinds (plan section 5, rail 1):
      run          one per run: counts, accounts trusted, model calls, seconds
      accounts     active-account list refreshed: how many addresses
      seen         one per new message: headers that matter, then the facts:
                   staffLogin, matchedBy, ticketId, ticketStatus, ticketOpener,
                   notes (Rejected rule), factsDecide
      observed     shadow only: what mlqAutomate.py did with the message
      disposition  (step 2) Stage 1 verdict: disposition, reason, confidence,
                   model, cached, cacheKey, action. Spam is a disposition like
                   any other, so every spam call is logged with its reason and
                   whether the message was held (moderated) or dropped (www)
      reopen       (step 5) conversationState, shouldReopen, reason, vetoes
      match        (step 6) candidate tickets, verdict, reason, confidence
      draft        (step 4) the draft, URLs cited, tickets used, check results
      action       the final thing done (or, in shadow, that would be done)
      alert        every alert, sent or (shadow) only logged
    """
    record = {'time': utcNow(), 'mode': 'shadow' if SHADOW else 'live',
              'kind': kind, 'messageId': messageId}
    record.update(fields)
    path = os.path.join(DECISION_LOG_DIR, f"decisions-{datetime.now(PST):%Y-%m}.jsonl")
    with open(path, 'a') as f:
        f.write(json.dumps(record, ensure_ascii=False) + '\n')


def pruneDecisionLogs():
    """Delete monthly decision logs older than DECISION_LOG_MONTHS."""
    now = datetime.now(PST)
    cutoff = (now.year * 12 + now.month - 1) - CONFIG['DECISION_LOG_MONTHS']
    for name in os.listdir(DECISION_LOG_DIR):
        m = re.match(r'decisions-(\d{4})-(\d{2})\.jsonl$', name)
        if m and int(m.group(1)) * 12 + int(m.group(2)) - 1 < cutoff:
            os.remove(os.path.join(DECISION_LOG_DIR, name))
            logger.info(f"Pruned old decision log {name}")


# ---------------------------------------------------------------------------
# Alerts

STATE_CONN = None
ALERT_TO_OVERRIDE = None
STATE_DIR = None
INDEX_OK = False
ACCOUNTS_OK = False


def sendAlert(key, subject, body):
    """Email the team about a failure, at most once per ALERT_REPEAT_SECONDS per key.

    In shadow mode nothing is sent unless --alert-to names a recipient, because a
    shadow run must not write to Gmail on the team's behalf."""
    if DECISION_LOG_DIR:
        logDecision('alert', None, key=key, subject=subject, body=body)
    if STATE_CONN is not None:
        last = metaGet(STATE_CONN, f"alert:{key}")
        if last and time.time() - float(last) < CONFIG['ALERT_REPEAT_SECONDS']:
            logger.info(f"Alert '{key}' already sent recently; not repeating")
            return
    recipient = ALERT_TO_OVERRIDE if SHADOW else CONFIG['ALERT_TO']
    if not recipient:
        logger.warning(f"[shadow] would alert: {subject}: {body}")
        return
    try:
        service = gmailService()
        message = MIMEText(body + "\n\n-- mlqTriage.py automated alert")
        message['To'] = recipient
        message['From'] = 'gbauto@ucsc.edu'
        message['Subject'] = f"[MLQ Triage] {subject}"
        raw = base64.urlsafe_b64encode(message.as_bytes()).decode('utf-8')
        service.users().messages().send(userId='me', body={'raw': raw}).execute()
        logger.info(f"Sent alert to {recipient}: {subject}")
    except Exception as e:
        logger.error(f"Failed to send alert '{subject}': {e}")
        return
    if STATE_CONN is not None:
        metaSet(STATE_CONN, f"alert:{key}", str(time.time()))


# ---------------------------------------------------------------------------
# Google APIs

_googleCreds = None


def googleCredentials():
    """Load the shared gbauto token with the scopes recorded in the file.

    Never narrow the scopes here: the file is shared with mlqAutomate.py and
    codeReviewAi.py, and a refresh writes the token back for all of them.
    There is no interactive login path; a missing or revoked token is an error."""
    global _googleCreds
    if _googleCreds is not None and _googleCreds.valid:
        return _googleCreds
    if not os.path.exists(GMAIL_TOKEN_PATH):
        raise RuntimeError(f"{GMAIL_TOKEN_PATH} not found")
    creds = Credentials.from_authorized_user_file(GMAIL_TOKEN_PATH)
    if not creds.valid:
        if not creds.refresh_token:
            raise RuntimeError(f"{GMAIL_TOKEN_PATH} has no refresh token")
        creds.refresh(Request())
        with open(GMAIL_TOKEN_PATH, 'w') as f:
            f.write(creds.to_json())
    _googleCreds = creds
    return creds


def gmailService():
    return build('gmail', 'v1', credentials=googleCredentials(), cache_discovery=False)


def currentMlm():
    """Return the MLM on duty by the calendar rules copied from mlqAutomate.py."""
    service = build('calendar', 'v3', credentials=googleCredentials(), cache_discovery=False)
    target = datetime.now(PST)
    if target.hour >= 17:                     # after 5pm, tomorrow's MLM
        target += timedelta(days=1)
    if target.weekday() == 5:                 # Fri 5pm+ through Mon 5pm uses Monday's
        target += timedelta(days=2)
    elif target.weekday() == 6:
        target += timedelta(days=1)
    if target.weekday() == 1:                 # Tuesday is always Matt
        return 'Matt Speir'
    start = target.replace(hour=0, minute=0, second=0, microsecond=0)
    end = target.replace(hour=23, minute=59, second=59, microsecond=0)
    events = service.events().list(calendarId=CONFIG['CALENDAR_ID'], timeMin=start.isoformat(),
                                   timeMax=end.isoformat(), singleEvents=True
                                   ).execute().get('items', [])
    for event in events:
        m = re.search(r'MLM(?:\s+Rotating)?:\s*(\w+)', event.get('summary', ''), re.IGNORECASE)
        if m:
            return CONFIG['NAME_MAPPING'].get(m.group(1), m.group(1))
    logger.warning(f"No MLM on the calendar for {target.date()}")
    return None


# ---------------------------------------------------------------------------
# Message parsing

def htmlToText(html):
    """Plain text from an HTML body (copied from mlqAutomate.py)."""
    if not html:
        return ''
    text = re.sub(r'<script[^>]*>.*?</script>', '', html, flags=re.DOTALL | re.IGNORECASE)
    text = re.sub(r'<style[^>]*>.*?</style>', '', text, flags=re.DOTALL | re.IGNORECASE)
    text = re.sub(r'<br\s*/?>', '\n', text, flags=re.IGNORECASE)
    text = re.sub(r'</p>', '\n\n', text, flags=re.IGNORECASE)
    text = re.sub(r'</div>', '\n', text, flags=re.IGNORECASE)
    text = re.sub(r'</li>', '\n', text, flags=re.IGNORECASE)
    text = re.sub(r'<[^>]+>', '', text)
    text = htmlModule.unescape(text)
    text = re.sub(r'[ \t]+', ' ', text)
    text = re.sub(r'\n[ \t]+', '\n', text)
    text = re.sub(r'[ \t]+\n', '\n', text)
    text = re.sub(r'\n{3,}', '\n\n', text)
    return text.strip()


def headerAddress(value):
    """Lower-cased bare address from a From-style header."""
    return parseaddr(value or '')[1].lower()


def originalSender(headers):
    """Google Groups may rewrite From to the list; prefer the original sender."""
    return headers.get('X-Original-Sender') or headers.get('Reply-To') or headers.get('From', '')


def gmailPartText(payload, mimeType):
    """First body of the given MIME type in a Gmail API payload tree."""
    if payload.get('mimeType') == mimeType and payload.get('body', {}).get('data'):
        return base64.urlsafe_b64decode(payload['body']['data']).decode('utf-8', errors='ignore')
    for part in payload.get('parts', []):
        found = gmailPartText(part, mimeType)
        if found:
            return found
    return ''


def gmailAttachmentInfo(payload):
    """Names, types, sizes and Content-Ids of attachments; nothing is downloaded."""
    found = []

    def walk(part):
        body = part.get('body', {})
        if body.get('attachmentId') and part.get('filename'):
            headers = {h['name'].lower(): h['value'] for h in part.get('headers', [])}
            found.append({'filename': part['filename'], 'mimeType': part.get('mimeType', ''),
                          'size': body.get('size', 0), 'attachmentId': body['attachmentId'],
                          'contentId': headers.get('content-id', '').strip('<>')})
        for sub in part.get('parts', []):
            walk(sub)

    walk(payload)
    return found


def headerDict(headerList):
    """Gmail API header list to a dict; the first occurrence of a name wins."""
    result = {}
    for h in headerList:
        result.setdefault(h['name'], h['value'])
    # Message-Id is spelled several ways in the wild
    for spelling in ('Message-ID', 'Message-id', 'message-id'):
        if spelling in result and 'Message-Id' not in result:
            result['Message-Id'] = result[spelling]
    return result


def messageRecord(headers, gmailId, threadId, internalDate, listAddr, source,
                  bodyText, bodyHtml, attachments):
    """The one shape every fetched message takes, whichever path it came from."""
    messageId = (headers.get('Message-Id') or '').strip()
    return {
        'messageId': messageId or f"gmail:{gmailId}",
        'hasMessageId': bool(messageId),
        'gmailId': gmailId,
        'gmailThreadId': threadId,
        'lists': [listAddr],
        'source': source,
        'from': originalSender(headers),
        'fromAddr': headerAddress(originalSender(headers)),
        'to': headers.get('To', ''),
        'cc': headers.get('Cc', ''),
        'subject': headers.get('Subject', ''),
        'date': headers.get('Date', ''),
        'inReplyTo': headers.get('In-Reply-To', ''),
        'references': headers.get('References', ''),
        'autoSubmitted': headers.get('Auto-Submitted', ''),
        'precedence': headers.get('Precedence', ''),
        'receivedAt': datetime.fromtimestamp(internalDate / 1000, timezone.utc
                                             ).isoformat(timespec='seconds'),
        'bodyText': bodyText or htmlToText(bodyHtml),
        'bodyHtml': bodyHtml,
        'attachments': attachments,
    }


# ---------------------------------------------------------------------------
# Fetching

def listAll(service, query):
    """Every Gmail message id matching query (follows pagination)."""
    ids, pageToken = [], None
    while True:
        resp = service.users().messages().list(userId='me', q=query, maxResults=100,
                                               pageToken=pageToken).execute()
        ids.extend(m['id'] for m in resp.get('messages', []))
        pageToken = resp.get('nextPageToken')
        if not pageToken:
            return ids


def isModerationTraffic(headers):
    """Moderation notices and our own outgoing mail, which are not list messages."""
    subject = headers.get('Subject', '')
    sender = originalSender(headers)
    return ('Message Pending' in subject or 'Moderation response' in subject
            or 'gbauto@ucsc.edu' in sender or '+msgappr@' in sender or '+msgrej@' in sender)


def fetchDelivered(service, listAddr, days):
    """Messages delivered to listAddr in the last `days` days."""
    after = int((datetime.now(timezone.utc) - timedelta(days=days)).timestamp())
    records = []
    for gmailId in listAll(service, f"to:{listAddr} after:{after}"):
        msg = service.users().messages().get(userId='me', id=gmailId, format='full').execute()
        headers = headerDict(msg['payload']['headers'])
        if isModerationTraffic(headers):
            continue
        records.append(messageRecord(
            headers, gmailId, msg['threadId'], int(msg['internalDate']), listAddr, 'delivered',
            gmailPartText(msg['payload'], 'text/plain'),
            gmailPartText(msg['payload'], 'text/html'),
            gmailAttachmentInfo(msg['payload'])))
    return records


def parseRawEmail(raw):
    """Headers, text body and HTML body of a raw RFC 822 message."""
    msg = email.message_from_string(raw, policy=policy.default)
    headers = {k: str(v) for k, v in msg.items()}
    headers = headerDict([{'name': k, 'value': v} for k, v in headers.items()])
    text, html = '', ''
    for part in (msg.walk() if msg.is_multipart() else [msg]):
        ctype = part.get_content_type()
        if ctype not in ('text/plain', 'text/html') or part.get_filename():
            continue
        try:
            content = part.get_content()
        except Exception:
            payload = part.get_payload(decode=True) or b''
            content = payload.decode(part.get_content_charset() or 'utf-8', errors='ignore')
        if ctype == 'text/plain' and not text:
            text = content
        elif ctype == 'text/html' and not html:
            html = content
    return headers, text, html


def nestedHeaders(part):
    """Headers of the embedded original message in a pre-parsed message/rfc822 part."""
    if 'headers' in part:
        headers = headerDict(part['headers'])
        if 'Subject' in headers or 'From' in headers:
            return headers
    for sub in part.get('parts', []):
        found = nestedHeaders(sub)
        if found:
            return found
    return {}


def fetchPending(service, listAddr):
    """Messages waiting on the Google Groups moderation queue for listAddr.

    Each comes as a "Message Pending" notification with the original message
    embedded. Unlike mlqAutomate.py, this keeps the original's own Message-Id,
    which is what ties it to the copy delivered to the list after approval."""
    group = listAddr.split('@')[0]
    # -in:sent: mlqAutomate.py's own approval replies ("Re: ... Message Pending") match
    # the subject too, and are never trashed; there were 221 of them in Nov 2026.
    query = f'subject:"{group} - soe.ucsc.edu admins: Message Pending" -in:trash -in:sent'
    records = []
    for gmailId in listAll(service, query):
        msg = service.users().messages().get(userId='me', id=gmailId, format='full').execute()
        for part in msg['payload'].get('parts', []):
            if part.get('mimeType') != 'message/rfc822':
                continue
            if 'attachmentId' in part.get('body', {}):
                att = service.users().messages().attachments().get(
                    userId='me', messageId=gmailId, id=part['body']['attachmentId']).execute()
                raw = base64.urlsafe_b64decode(att['data']).decode('utf-8', errors='ignore')
                headers, text, html = parseRawEmail(raw)
                attachments = []
            else:
                headers = nestedHeaders(part)
                text = gmailPartText(part, 'text/plain')
                html = gmailPartText(part, 'text/html')
                attachments = gmailAttachmentInfo(part)
            record = messageRecord(headers, gmailId, msg['threadId'], int(msg['internalDate']),
                                   listAddr, 'pending', text, html, attachments)
            record['notificationGmailId'] = gmailId
            # What approving needs: the +msgappr@ address and the notification's own
            # subject and Message-Id, so the approval threads as a reply (as mlqAutomate.py).
            note = headerDict(msg['payload']['headers'])
            m = re.search(r'<([^>]+\+msgappr@[^>]+)>', note.get('From', ''))
            record['approveAddr'] = m.group(1) if m else ''
            record['notificationSubject'] = note.get('Subject', '')
            record['notificationMessageId'] = note.get('Message-Id', '')
            records.append(record)
            break
    return records


def fetchAll(days):
    """Pending and delivered messages from every list, one record per Message-Id.

    The same message can arrive twice (sent to two lists, or seen both pending and
    delivered). The pending copy wins, since that is the one a decision acts on."""
    service = gmailService()
    byId = {}
    batches = []
    for listAddr in CONFIG['MODERATED_LISTS']:
        batches.append(fetchPending(service, listAddr))
    for listAddr in CONFIG['MODERATED_LISTS'] + CONFIG['UNMODERATED_LISTS']:
        batches.append(fetchDelivered(service, listAddr, days))
    for batch in batches:
        for rec in batch:
            seen = byId.get(rec['messageId'])
            if seen is None:
                byId[rec['messageId']] = rec
            elif rec['lists'][0] not in seen['lists']:
                seen['lists'].append(rec['lists'][0])
    return sorted(byId.values(), key=lambda r: r['receivedAt'])


# ---------------------------------------------------------------------------
# Redmine (read-only in step 0)

def redmineGet(path, params=None, attempts=3):
    """GET a Redmine API path, retrying transient failures."""
    url = f"{CONFIG['REDMINE_URL']}{path}"
    headers = {'X-Redmine-API-Key': CONFIG['REDMINE_API_KEY']}
    for attempt in range(1, attempts + 1):
        try:
            resp = requests.get(url, headers=headers, params=params, timeout=30)
            if resp.status_code < 500:
                resp.raise_for_status()
                return resp.json()
            err = f"HTTP {resp.status_code}"
        except requests.exceptions.ConnectionError as e:
            err = str(e)
        except requests.exceptions.Timeout as e:
            err = str(e)
        if attempt == attempts:
            raise RuntimeError(f"Redmine GET {path} failed after {attempts} attempts: {err}")
        time.sleep(2 * attempt)


# ---------------------------------------------------------------------------
# Redmine database (read-only)

_db = None


def importPymysql():
    """pymysql, preferring an installed copy, else the one bundled in the kent tree."""
    try:
        import pymysql
        return pymysql
    except ImportError:
        pass
    for pyLib in (os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', '..',
                               'hg', 'pyLib'),
                  os.path.expanduser('~/kent/src/hg/pyLib')):
        if os.path.isdir(pyLib):
            sys.path.insert(0, os.path.normpath(pyLib))
            try:
                import pymysql
                return pymysql
            except ImportError:
                sys.path.pop(0)
    raise RuntimeError("cannot import pymysql (expected in kent/src/hg/pyLib)")


def dbQuery(sql, args=()):
    """Run one read-only query against the Redmine database; rows as dicts."""
    global _db
    if _db is None:
        pymysql = importPymysql()
        user = HG_CONF.get('redmine.db.user') or HG_CONF.get('db.user')
        password = HG_CONF.get('redmine.db.password') or HG_CONF.get('db.password')
        if not user or not password:
            raise RuntimeError("no Redmine database login (db.user/db.password) in ~/.hg.conf")
        _db = pymysql.connect(host=HG_CONF.get('redmine.db.host', DB_DEFAULTS['host']),
                              port=int(HG_CONF.get('redmine.db.port', DB_DEFAULTS['port'])),
                              user=user, password=password,
                              database=HG_CONF.get('redmine.db.name', DB_DEFAULTS['name']),
                              charset='utf8mb4', connect_timeout=10, read_timeout=60,
                              cursorclass=pymysql.cursors.DictCursor)
    with _db.cursor() as cur:
        cur.execute(sql, args)
        return list(cur.fetchall())


def toDbTime(isoUtc, deltaHours=0.0):
    """A UTC ISO timestamp as the naive Pacific string the Redmine tables use."""
    t = datetime.fromisoformat(isoUtc) + timedelta(hours=deltaHours)
    return t.astimezone(PST).strftime('%Y-%m-%d %H:%M:%S')


# ---------------------------------------------------------------------------
# Facts (step 1): who is staff, which ticket a message belongs to

def refreshAccounts(conn, force=False):
    """Reload every address on an active Redmine account, if the copy is stale."""
    last = metaGet(conn, 'accountsRefreshedAt')
    if not force and last and (datetime.now(timezone.utc) - datetime.fromisoformat(last)
                               < timedelta(hours=ACCOUNTS_REFRESH_HOURS)):
        return
    rows = dbQuery("select lower(e.address) address, u.login from users u "
                   "join email_addresses e on e.user_id=u.id "
                   "where u.status=1 and u.type='User'")
    now = utcNow()
    conn.execute("delete from accounts")
    conn.executemany("insert or replace into accounts(address, login, refreshedAt) "
                     "values (?,?,?)", [(r['address'], r['login'], now) for r in rows])
    metaSet(conn, 'accountsRefreshedAt', now)
    logDecision('accounts', None, count=len(rows))
    logger.info(f"Refreshed active Redmine accounts: {len(rows)} addresses")


def accountsUsable(conn):
    """(True, '') if the account list can be trusted for the staff veto."""
    last = metaGet(conn, 'accountsRefreshedAt')
    rows = conn.execute("select count(*) from accounts").fetchone()[0]
    if not last:
        return False, 'never refreshed'
    age = datetime.now(timezone.utc) - datetime.fromisoformat(last)
    if age > timedelta(hours=ACCOUNTS_MAX_AGE_HOURS):
        return False, f"last refreshed {age.total_seconds() / 3600:.0f}h ago"
    if rows < ACCOUNTS_MIN_ROWS:
        return False, f"only {rows} addresses"
    return True, ''


def staffLogin(conn, addr):
    """The Redmine login owning addr if it is on an active account, else None."""
    row = conn.execute("select login from accounts where address=?", (addr,)).fetchone()
    return row[0] if row else None


def referencedIds(rec):
    """Message-Ids this message replies to, nearest first."""
    ids = re.findall(r'<[^<>\s]+>', rec['inReplyTo'] or '')
    refs = re.findall(r'<[^<>\s]+>', rec['references'] or '')
    return ids + [r for r in reversed(refs) if r not in ids]


def exactMatch(conn, rec):
    """(ticketId, how) from the thread and reply-header indexes, or (None, None)."""
    row = conn.execute("select ticketId from threads where gmailThreadId=?",
                       (rec['gmailThreadId'],)).fetchone()
    if row and row[0]:
        return row[0], 'gmailThread'
    for ref in referencedIds(rec):
        row = conn.execute("select ticketId from messages where messageId=? "
                           "and ticketId is not null", (ref,)).fetchone()
        if row:
            return row[0], 'replyHeader'
    return None, None


def ticketFacts(ticketId):
    """Status, opener and duplicate target of an MLQ ticket, from the database."""
    rows = dbQuery("select i.id, i.status_id, s.name status, s.is_closed, i.description, "
                   "i.subject, i.created_on, cv.value email from issues i "
                   "join issue_statuses s on s.id=i.status_id "
                   "left join custom_values cv on cv.customized_id=i.id "
                   "  and cv.customized_type='Issue' and cv.custom_field_id=40 "
                   "where i.id=%s", (ticketId,))
    if not rows:
        return None
    r = rows[0]
    m = re.match(r'\s*(?:\*Summary:\*.*\n\s*)?\*?From:\*?\s*(\S+)', r['description'] or '')
    opener = headerAddress(m.group(1)) if m else ''
    if not opener and r['email']:
        opener = r['email'].split(',')[0].strip().lower()
    facts = {'ticketId': r['id'], 'status': r['status'], 'isClosed': bool(r['is_closed']),
             'statusId': r['status_id'], 'opener': opener, 'subject': r['subject'],
             'created': str(r['created_on'])}
    if r['status_id'] == STATUS_REJECTED:
        dup = dbQuery("select issue_to_id from issue_relations where issue_from_id=%s "
                      "and relation_type='duplicates'", (ticketId,))
        facts['duplicateOf'] = dup[0]['issue_to_id'] if dup else None
        if not dup:
            facts['relatesOnly'] = [x['other'] for x in dbQuery(
                "select if(issue_from_id=%s, issue_to_id, issue_from_id) other "
                "from issue_relations where relation_type='relates' "
                "and (issue_from_id=%s or issue_to_id=%s)", (ticketId, ticketId, ticketId))]
    return facts


def resolveTicket(conn, rec):
    """Exact match, then the Rejected rule: never resolve to a Rejected ticket.

    Returns (ticketFacts or None, matchedBy, notes)."""
    ticketId, how = exactMatch(conn, rec)
    if not ticketId:
        return None, None, []
    notes = []
    facts = ticketFacts(ticketId)
    if facts and facts['statusId'] == STATUS_REJECTED:
        if facts.get('duplicateOf'):
            notes.append(f"#{ticketId} is Rejected; followed its duplicate relation to "
                         f"#{facts['duplicateOf']}")
            facts = ticketFacts(facts['duplicateOf'])
        else:
            notes.append(f"#{ticketId} is Rejected with no duplicate relation"
                         + (f" (relates to {facts['relatesOnly']})"
                            if facts.get('relatesOnly') else ''))
            return None, how, notes
    return facts, how, notes


# ---------------------------------------------------------------------------
# Shadow: what did mlqAutomate.py do with each message?

def observeOldScript(conn):
    """Find, for each recorded message, the ticket the old script filed it on.

    Looks for a gbauto ticket created, or a gbauto "New Email Update" journal
    posted, from the message's sender within OBSERVE_WINDOW_HOURS of its arrival.
    Messages are matched oldest first and each ticket/journal is claimed by one
    message only. After OBSERVE_GIVE_UP_HOURS with no match the old script is
    recorded as having done nothing (held spam, a skipped duplicate, a drop)."""
    pending = conn.execute("select messageId from messages where oldAction is null "
                           "order by receivedAt").fetchall()
    counts = {'created': 0, 'appended': 0, 'none': 0, 'waiting': 0}
    for (messageId,) in pending:
        counts[observeOne(conn, messageId)] += 1
    return counts


def observeSubject(subject):
    """Subject reduced for comparing a message with a ticket, for the observer only.

    This is measurement code, not a decision the bot acts on, so a plain strip of
    list tags and reply tokens is enough here."""
    s = (subject or '').lower()
    s = re.sub(r'\[[^\]]{1,15}\]', ' ', s)          # [genome-www], [EXT], [EXTERNAL]
    s = re.sub(r'\bext msg\b', ' ', s)
    s = re.sub(r'^(\s*([a-z]{1,8}(\[\d+\])?|ext msg|\[external\])\s*:\s*)+', '', s)
    s = re.sub(r'\b(re|fw|fwd|aw|sv|r|ynt)\s*:\s*', '', s)
    return re.sub(r'\s+', ' ', s).strip()


def observeOne(conn, messageId):
    """Look for the old script's action on one message; returns what was found."""
    claimed = {r[0] for r in conn.execute("select oldRef from messages where oldRef is not null")}
    fromAddr, receivedAt, threadId, subject, ourTicket = conn.execute(
        "select fromAddr, receivedAt, gmailThreadId, subject, ticketId from messages "
        "where messageId=?", (messageId,)).fetchone()
    lo, hi = toDbTime(receivedAt, -0.05), toDbTime(receivedAt, OBSERVE_WINDOW_HOURS)
    like = f"%{fromAddr}%"
    hits = []
    for r in dbQuery("select i.id, i.created_on, i.subject from issues i "
                     "join users u on u.id=i.author_id "
                     "where u.login='gbauto' and i.tracker_id=7 and i.created_on between %s "
                     "and %s and i.description like %s order by i.created_on",
                     (lo, hi, like)):
        hits.append((r['created_on'], f"issue:{r['id']}", 'created', r['id'], 0, r['subject']))
    for r in dbQuery("select j.id, j.journalized_id, j.created_on, i.subject, "
                     "  exists(select 1 from journal_details d where d.journal_id=j.id "
                     "    and d.prop_key='status_id' and d.value='1') reopened "
                     "from journals j join users u on u.id=j.user_id "
                     "join issues i on i.id=j.journalized_id "
                     "where u.login='gbauto' and i.tracker_id=7 and j.created_on between %s "
                     "and %s and j.notes like %s and j.notes like %s order by j.created_on",
                     (lo, hi, '%--- New Email Update ---%', like)):
        hits.append((r['created_on'], f"journal:{r['id']}", 'appended', r['journalized_id'],
                     int(r['reopened']), r['subject']))
    hits = [h for h in hits if h[1] not in claimed]
    # A sender who answers several threads in one burst has several candidate
    # posts. Take the ticket this message's own thread points to, else one whose
    # subject matches. Time and sender alone are trusted only for a lone new
    # ticket: a lone appended post may belong to a different reply by the same
    # person (seen with a staff burst during the 2026-10 certificate outage).
    exact = [h for h in hits if ourTicket and h[3] == ourTicket]
    similar = [h for h in hits if observeSubject(h[5]) == observeSubject(subject)]
    lone = [h for h in hits if len(hits) == 1 and h[2] == 'created']
    hits = sorted(exact or similar or lone)
    if hits:
        _, ref, action, ticketId, reopened, _ = hits[0]
        conn.execute("update messages set oldAction=?, oldTicketId=?, oldReopened=?, "
                     "oldRef=?, observedAt=?, ticketId=coalesce(ticketId, ?) "
                     "where messageId=?",
                     (action, ticketId, reopened, ref, utcNow(), ticketId, messageId))
        conn.execute("insert or ignore into threads(gmailThreadId, ticketId) values (?,?)",
                     (threadId, ticketId))
        conn.commit()
        logDecision('observed', messageId, oldAction=action, oldTicketId=ticketId,
                    oldReopened=bool(reopened), oldRef=ref, fromAddr=fromAddr,
                    subject=subject)
        return action
    elif datetime.now(timezone.utc) - datetime.fromisoformat(receivedAt) > \
            timedelta(hours=OBSERVE_GIVE_UP_HOURS):
        conn.execute("update messages set oldAction='none', observedAt=? where messageId=?",
                     (utcNow(), messageId))
        conn.commit()
        logDecision('observed', messageId, oldAction='none', fromAddr=fromAddr,
                    subject=subject)
        return 'none'
    return 'waiting'


# ---------------------------------------------------------------------------
# The one place a model is called

class AskError(Exception):
    """A claude -p call failed; the message says how."""


class AskAuthError(AskError):
    """The CLI could not authenticate: an expired or revoked token."""


class CallCapReached(AskError):
    """This run has made CALL_CAP model calls already."""


class AskLimitError(AskError):
    """The subscription's usage limit is reached. Not the message's fault: stop the
    run and try again later; the 3-day fetch window keeps the message."""


LIMIT_TEXT = re.compile(r'hit your .{0,20}limit|usage limit|rate limit|limit reached|'
                        r'resets \d', re.IGNORECASE)


_callCount = 0


def ask(prompt, schema, model, fallbackModel=None, systemPrompt='', tools=None, cwd=None,
        allowedTools=None, timeout=None):
    """Run one claude -p call and return its schema-validated answer as a dict.

    The prompt goes in on stdin (a single argv string is capped at 128 KB). No
    tools unless the caller names them; no user settings, MCP servers, skills or
    CLAUDE.md, so the call carries only what is passed here."""
    global _callCount
    if _callCount >= CONFIG['CALL_CAP']:
        raise CallCapReached(f"call cap of {CONFIG['CALL_CAP']} reached")
    _callCount += 1
    cmd = [CONFIG['CLAUDE_CLI'], '-p',
           '--output-format', 'json',
           '--json-schema', json.dumps(schema),
           '--model', model,
           '--system-prompt', systemPrompt or 'Answer only through the provided JSON schema.',
           '--tools', ','.join(tools) if tools else '',
           '--setting-sources', '',
           '--strict-mcp-config',
           '--disable-slash-commands',
           '--no-session-persistence']
    if fallbackModel:
        cmd += ['--fallback-model', fallbackModel]
    if allowedTools:
        cmd += ['--allowedTools', ','.join(allowedTools)]
    started = time.time()
    try:
        result = subprocess.run(cmd, input=prompt, capture_output=True, text=True,
                                timeout=timeout or CONFIG['ASK_TIMEOUT'],
                                cwd=cwd or os.path.expanduser('~'))
    except subprocess.TimeoutExpired:
        raise AskError(f"claude -p timed out after {timeout or CONFIG['ASK_TIMEOUT']}s")
    try:
        envelope = json.loads(result.stdout)
    except ValueError:
        detail = (result.stderr or result.stdout or '').strip()[:500]
        if re.search(r'auth|login|token|401|403', detail, re.IGNORECASE):
            raise AskAuthError(f"claude -p could not authenticate: {detail}")
        raise AskError(f"claude -p exited {result.returncode} without JSON: {detail}")
    if envelope.get('is_error') or envelope.get('subtype') != 'success':
        detail = str(envelope.get('result') or envelope.get('subtype'))[:500]
        if LIMIT_TEXT.search(detail):
            raise AskLimitError(f"Claude usage limit: {detail}")
        if re.search(r'auth|login|token|401|403', detail, re.IGNORECASE):
            raise AskAuthError(f"claude -p could not authenticate: {detail}")
        raise AskError(f"claude -p failed ({envelope.get('subtype')}): {detail}")
    answer = envelope.get('structured_output')
    if not isinstance(answer, dict):
        raise AskError("claude -p returned no structured_output; has the CLI changed?")
    logger.debug(f"ask: {model} {time.time() - started:.1f}s "
                 f"cost=${envelope.get('total_cost_usd', 0):.4f}")
    return answer


# ---------------------------------------------------------------------------
# Selftest

def selftest():
    """Check each outside dependency once and report; exit non-zero on any failure."""
    ok = True
    try:
        service = gmailService()
        profile = service.users().getProfile(userId='me').execute()
        logger.info(f"Gmail: OK ({profile.get('emailAddress')})")
    except Exception as e:
        logger.error(f"Gmail: FAILED {e}")
        ok = False
    try:
        logger.info(f"Calendar: OK (MLM on duty: {currentMlm()})")
    except Exception as e:
        logger.error(f"Calendar: FAILED {e}")
        ok = False
    try:
        user = redmineGet('/users/current.json')['user']
        logger.info(f"Redmine: OK (as {user.get('login')})")
    except Exception as e:
        logger.error(f"Redmine: FAILED {e}")
        ok = False
    try:
        schema = {'type': 'object', 'properties': {'ok': {'type': 'boolean'}},
                  'required': ['ok'], 'additionalProperties': False}
        answer = ask('Reply with ok set to true.', schema, CONFIG['MODEL_JUDGE'],
                     CONFIG['MODEL_JUDGE_FALLBACK'])
        logger.info(f"claude -p: OK ({answer})")
    except AskError as e:
        logger.error(f"claude -p: FAILED {e}")
        ok = False
    return ok


# ---------------------------------------------------------------------------
# Stage 1 (step 2): classify and clean one message with one model call

# Copied from mlqAutomate.py; the category behaviour is unchanged (plan section 1).
MLQ_CATEGORIES = [
    "Other", "Alignments", "BLAT", "Bug Report", "CAPTCHA", "Command-line Utilities",
    "Conservation", "Custom Track", "Data - Availability (when)", "Data - Interpretation (what)",
    "Data - Location (where)", "Data Contribution", "Data Integrator", "Data Requests", "dbSNP",
    "Downloads", "ENCODE", "External Tools", "Feature Request", "GBiB", "GBiC",
    "Gene Interactions (hgGeneGraph)", "Gene Tracks", "Help Docs (Info)", "Hubs", "IP blocked",
    "JSON hubApi", "Licenses", "LiftOver", "Login", "Mirror - Asia", "Mirror - Europe",
    "Mirror Site & Utilities", "Multi-region", "MySQL", "PCR", "Publications & Citing",
    "Sessions", "Slow Performance", "Table Browser", "Track Collection Builder", "User Accounts",
    "Variant Annotation Integrator", "Widget"
]

DISPOSITIONS = ["question", "reply", "acknowledgement", "digest_reply", "spam",
                "machine_notification", "internal_test", "out_of_office", "message_recall",
                "our_own_notification"]
# Dispositions that are never filed: held on a moderated list, dropped (and
# logged) on genome-www. Plan section 3, Stage 1 action table.
NOT_FILED = {"spam", "machine_notification", "internal_test", "out_of_office",
             "our_own_notification"}
ATTACHMENT_ROLES = ["content", "signature_or_logo", "legal_or_disclaimer_image",
                    "crypto_signature", "calendar_invite", "other"]

# Below this self-reported confidence the message is treated as "not sure"
# (section 7b: hold on a moderated list, file nothing on genome-www). Provisional:
# the plan sets the real cut-off from shadow data.
UNSURE_BELOW = 0.6
SENDER_VERDICT_DAYS = 30
MAX_SOURCE_CHARS = 40000

STAGE1_SCHEMA = {
    'type': 'object',
    'additionalProperties': False,
    'required': ['disposition', 'dispositionReason', 'confidence', 'sensitivePersonalInfo',
                 'canonicalSubject', 'digestTopic', 'summaryLine', 'cleanBodyTextile',
                 'category', 'playbooks', 'hubName', 'hubUrl', 'assemblies', 'urls',
                 'attachmentRoles'],
    'properties': {
        'disposition': {'type': 'string', 'enum': DISPOSITIONS},
        'dispositionReason': {'type': 'string'},
        'confidence': {'type': 'number', 'minimum': 0, 'maximum': 1},
        'sensitivePersonalInfo': {'type': 'boolean'},
        'canonicalSubject': {'type': 'string'},
        'digestTopic': {'type': 'string'},
        'summaryLine': {'type': 'string'},
        'cleanBodyTextile': {'type': 'string'},
        'category': {'type': 'string', 'enum': MLQ_CATEGORIES},
        'playbooks': {'type': 'array', 'maxItems': 2, 'items': {
            'type': 'string', 'enum': ['LiftOver', 'Login', 'User Accounts', 'IP blocked',
                                       'Sessions', 'CAPTCHA']}},
        'hubName': {'type': 'string'},
        'hubUrl': {'type': 'string'},
        'assemblies': {'type': 'array', 'items': {'type': 'string'}},
        'urls': {'type': 'array', 'items': {'type': 'string'}},
        'attachmentRoles': {'type': 'array', 'items': {
            'type': 'object', 'additionalProperties': False,
            'required': ['filename', 'role'],
            'properties': {'filename': {'type': 'string'},
                           'role': {'type': 'string', 'enum': ATTACHMENT_ROLES}}}},
    },
}

STAGE1_SYSTEM = """You triage email sent to the UCSC Genome Browser support mailing lists
(genome@soe.ucsc.edu and genome-mirror@soe.ucsc.edu are public and archived;
genome-www@soe.ucsc.edu is private to the Browser team). Each email you get becomes, or is
added to, a ticket in the team's Redmine tracker. Answer only through the JSON schema.

disposition, one of:
- question: someone asks something new, reports a problem or makes a request.
- reply: a message continuing an existing conversation that adds information, asks a
  follow-up, or is a staff member answering. Messages from UCSC staff answering someone are
  replies.
- acknowledgement: only thanks or confirmation that something worked, with nothing new that
  needs a response ("Thanks, that solved it"). If it thanks AND asks or reports something
  new, it is a reply.
- digest_reply: a reply to a Google Groups digest email (subject like "Digest for
  genome@soe.ucsc.edu - N updates in N topics"). Put the topic being answered in
  digestTopic.
- spam: conference or journal solicitations, calls for papers, promotions for workshops,
  courses, webinars or training, marketing of services or products, phishing or scams,
  mass-mailed announcements unrelated to Genome Browser support.
- machine_notification: written by a program, not a person: monitoring alerts, ticketing
  systems, bounces and delivery failures, GitHub or CI notifications.
- internal_test: a test message.
- out_of_office: an automatic absence reply.
- message_recall: an Outlook "Recall:" request for an earlier message.
- our_own_notification: an announcement or automated notice sent by the Genome Browser
  team itself (release announcements, Redmine notification emails), not a reply to one.
A genuine question about the Genome Browser or genomic data is never spam, even if it is
long, formal or comes from a company.

sensitivePersonalInfo: true only if the email contains sensitive personal medical
information about an identifiable person (names with genetic test results, medical
conditions, family medical history, personal health details). General questions about
genetic data are not.

confidence: how sure you are of the disposition, 0 to 1.

canonicalSubject: the subject with list tags ([genome], [genome-www]) and every reply,
forward or recall prefix removed, in any language (Re:, RE:, Fwd:, AW:, SV:, Sv:, R:, Ynt:,
Re[2]:, EXT MSG, [EXTERNAL], tracker ids like ZFINHELP-5245). For a digest_reply, the
digest topic.

summaryLine: one plain sentence, at most 25 words, saying what the sender wants or reports
(for a staff reply, what they answered). No names of the sender, no email addresses.

cleanBodyTextile: the new content of this one message, formatted as Redmine Textile.
- Keep every word the sender wrote in this message, in order. Do not reword, summarize,
  translate or correct anything.
- Remove: quoted earlier messages in any format or language ("On ... wrote:", Outlook
  "From:/Sent:/To:/Cc:/Subject:" blocks and their translations such as "Da:/Inviato:",
  lines starting with ">"), signature blocks (job titles, departments, postal addresses,
  phone numbers, social links), legal and confidentiality notices, environmental footers,
  and mailing-list footers ("You received this message because you are subscribed...").
  Keep a short sign-off with the sender's name.
- Textile, not Markdown: never use **, ##, backticks or backslash escapes. Paragraphs are
  plain lines with no leading spaces. A bulleted list is one line per item starting with
  "* "; a numbered list is one line per item starting with "# ". Rebuild lists that the
  plain-text version scattered across lines (a lone "1." or "-" line followed by an
  indented item is one list item).
- Wrap code, commands, configuration files, log or error output, stack traces and
  tabular data in <pre> and </pre>, keeping their exact characters and indentation. Do
  not escape anything inside <pre>.
- Write URLs plainly. If a URL appears twice as "text<URL>", keep it once.
- Where the email shows an inline image (a [cid:...] or [image: ...] reference), write
  {{image:ID}} on its own line, with ID the content-id or file name.
- If the message has no new content at all (only quoted text), return an empty string.

category: the closest MLQ category; "Other" if none fits.
playbooks: which of the team's playbooks cover what this email is about, at most two, even
if the category differs: LiftOver (chain files, converting coordinates), Login (signing in,
activation, passwords), User Accounts (account changes and deletion, Hub Space quota), IP
blocked (cannot reach the site, rate limits, blocked addresses), Sessions (saved and shared
sessions), CAPTCHA (the bot check, Cloudflare, scripts or tools blocked by it, API keys).
Empty if none applies.
hubName, hubUrl: if the email is about one specific track hub or assembly hub that someone
runs (not a request for a new GenArk assembly), its name and hub.txt URL; else "".
assemblies: genome assemblies named (hg38, mm39, GCA_...). urls: URLs in the new content.
attachmentRoles: one entry per attachment listed, with its role."""


def htmlToStructuredText(html):
    """Plain text from an HTML body that keeps list structure as Textile markers.

    The text/plain part Gmail sends for an HTML list scatters each item over
    several lines (#38641); the HTML part says exactly what is a list item."""
    from html.parser import HTMLParser

    class Walker(HTMLParser):
        def __init__(self):
            super().__init__(convert_charrefs=True)
            self.out, self.lists, self.skip, self.inPre = [], [], 0, 0
            self.itemStart = False      # just wrote a list marker, no text yet

        def handle_starttag(self, tag, attrs):
            if tag in ('script', 'style'):
                self.skip += 1
            elif tag in ('ul', 'ol'):
                self.lists.append('#' if tag == 'ol' else '*')
            elif tag == 'li':
                marker = (self.lists[-1] if self.lists else '*') * max(1, len(self.lists))
                self.out.append('\n' + marker + ' ')
                self.itemStart = True
            elif tag in ('br',):
                self.out.append('\n')
            elif tag in ('p', 'div', 'tr', 'table', 'blockquote', 'h1', 'h2', 'h3', 'h4'):
                if not self.itemStart:      # <li><p>text keeps text on the marker line
                    self.out.append('\n')
            elif tag == 'pre':
                self.inPre += 1
                self.out.append('\n<pre>\n')
            elif tag in ('td', 'th'):
                self.out.append(' | ')

        def handle_endtag(self, tag):
            if tag in ('script', 'style'):
                self.skip = max(0, self.skip - 1)
            elif tag in ('ul', 'ol') and self.lists:
                self.lists.pop()
                self.out.append('\n')
            elif tag == 'pre':
                self.inPre = max(0, self.inPre - 1)
                self.out.append('\n</pre>\n')
            elif tag in ('p', 'div', 'blockquote'):
                self.out.append('\n')

        def handle_data(self, data):
            if self.skip:
                return
            if data.strip():
                self.itemStart = False
            self.out.append(data if self.inPre else re.sub(r'\s+', ' ', data))

    walker = Walker()
    try:
        walker.feed(html)
        walker.close()
    except Exception:
        return htmlToText(html)
    text = ''.join(walker.out)
    text = '\n'.join(line.rstrip() for line in text.split('\n'))
    return re.sub(r'\n{3,}', '\n\n', text).strip()


def stage1Prompt(rec, facts, matchedSubject):
    """Everything Stage 1 sees about one message."""
    attachments = '\n'.join(f"- {a['filename']} ({a['mimeType']}, {a['size']} bytes"
                            + (f", inline content-id {a['contentId']}" if a['contentId'] else '')
                            + ')' for a in rec['attachments']) or '(none)'
    htmlView = htmlToStructuredText(rec['bodyHtml']) if rec['bodyHtml'] else ''
    parts = [
        f"List: {', '.join(rec['lists'])} ({'awaiting moderation' if rec['source'] == 'pending' else 'delivered'})",
        f"From: {rec['from']}",
        f"Sender is UCSC Genome Browser staff: {'yes (' + facts['staffLogin'] + ')' if facts['staffLogin'] else 'no'}",
        f"To: {rec['to']}",
        f"Cc: {rec['cc']}",
        f"Subject: {rec['subject']}",
        f"Date: {rec['date']}",
        f"Auto-Submitted: {rec['autoSubmitted'] or '(not set)'}",
        f"Is a reply to an earlier message (In-Reply-To set): {'yes' if rec['inReplyTo'] else 'no'}",
    ]
    if matchedSubject:
        parts.append(f"Belongs to existing ticket: {matchedSubject}")
    parts.append(f"Attachments:\n{attachments}")
    parts.append("Plain-text body:\n<<<\n" + rec['bodyText'][:MAX_SOURCE_CHARS] + "\n>>>")
    if htmlView:
        parts.append("Same body from the HTML part, with list structure kept (prefer this "
                     "for structure):\n<<<\n" + htmlView[:MAX_SOURCE_CHARS] + "\n>>>")
    return '\n'.join(parts)


def wordTokens(text):
    return re.findall(r'[a-z0-9]+', (text or '').lower())


def wordsPreserved(clean, sources):
    """Check that the cleaned body only drops text and never adds or rewords it.

    The cleaned words must appear in one of the source texts in the same order.
    Markup the cleaning is allowed to add (Textile markers, <pre>, {{image:}}) is
    ignored. Returns (ok, unmatched, total)."""
    clean = re.sub(r'\{\{image:[^}]*\}\}', ' ', clean or '')
    clean = re.sub(r'</?pre>', ' ', clean)
    words = wordTokens(clean)
    if not words:
        return True, 0, 0
    best = len(words)
    for source in sources:
        tokens = wordTokens(source)
        i = unmatched = 0
        for w in words:
            j = i
            while j < len(tokens) and tokens[j] != w:
                j += 1
            if j < len(tokens):
                i = j + 1
            else:
                unmatched += 1
        best = min(best, unmatched)
    # Allow a little slack for words split or joined across lines (hyphenation, a URL
    # rejoined from two lines): at most 2 words or 1%.
    return best <= max(2, len(words) // 100), best, len(words)


def tidyTextile(clean):
    """Deterministic last pass: no leading spaces outside <pre>, no Markdown bold."""
    out, inPre = [], False
    for line in (clean or '').split('\n'):
        if re.match(r'\s*<pre>', line):
            inPre = True
        if inPre:
            out.append(line)
        else:
            out.append(line.lstrip().replace('**', '*'))
        if '</pre>' in line:
            inPre = False
    return '\n'.join(out).strip()


def classify(conn, rec, facts, matchedSubject):
    """Stage 1 for one message: a cached sender verdict, else one model call.

    Returns (verdict, meta) where meta says where the verdict came from."""
    cacheKey = f"disposition:sender:{rec['fromAddr']}"
    row = conn.execute("select value, model, expiresAt from judgments where key=?",
                       (cacheKey,)).fetchone()
    if row and row[2] > utcNow() and not facts['staffLogin']:
        verdict = json.loads(row[0])
        return verdict, {'cached': True, 'cacheKey': cacheKey, 'model': row[1]}
    verdict = ask(stage1Prompt(rec, facts, matchedSubject), STAGE1_SCHEMA,
                  CONFIG['MODEL_JUDGE'], CONFIG['MODEL_JUDGE_FALLBACK'], STAGE1_SYSTEM)
    verdict['cleanBodyTextile'] = tidyTextile(verdict['cleanBodyTextile'])
    if (verdict['disposition'] == 'machine_notification' and verdict['confidence'] >= 0.8
            and not facts['staffLogin']):
        cached = {k: verdict[k] for k in ('disposition', 'dispositionReason', 'confidence',
                                           'category')}
        cached.update({'sensitivePersonalInfo': False, 'canonicalSubject': '', 'playbooks': [],
                       'digestTopic': '', 'summaryLine': '', 'cleanBodyTextile': '',
                       'hubName': '', 'hubUrl': '', 'assemblies': [], 'urls': [],
                       'attachmentRoles': []})
        expires = (datetime.now(timezone.utc) + timedelta(days=SENDER_VERDICT_DAYS)
                   ).isoformat(timespec='seconds')
        conn.execute("insert or replace into judgments(key, value, model, createdAt, expiresAt) "
                     "values (?,?,?,?,?)", (cacheKey, json.dumps(cached),
                                            CONFIG['MODEL_JUDGE'], utcNow(), expires))
        conn.commit()
    return verdict, {'cached': False, 'cacheKey': None, 'model': CONFIG['MODEL_JUDGE']}


def listTag(rec):
    return '[' + rec['lists'][0].split('@')[0] + ']'


def decideAction(rec, verdict, facts):
    """What the script does with one classified message (Stage 1 action table).

    Called after Stage 2, so a message with no ticket here is a new ticket. The
    reopen (Stage 3) and hub routing (Stage 4) are added to the result afterwards."""
    d = verdict['disposition']
    moderated = any(l in CONFIG['MODERATED_LISTS'] for l in rec['lists'])
    unsure = verdict['confidence'] < UNSURE_BELOW
    sensitive = moderated and verdict['sensitivePersonalInfo']
    act = {'disposition': d, 'unsure': unsure, 'moderated': moderated,
           'moderation': 'n/a', 'ticket': 'none', 'reopen': 'n/a', 'why': ''}
    if d in NOT_FILED or sensitive or unsure:
        act['why'] = ('not sure (confidence %.2f)' % verdict['confidence'] if unsure
                      else 'sensitive personal information on a public list' if sensitive
                      else d)
        if moderated:
            act['moderation'] = 'hold'
        else:
            act['ticket'] = 'none (left in place)' if unsure else 'none (dropped, logged)'
        return act
    if moderated:
        act['moderation'] = 'approve'
    if d == 'message_recall':
        if moderated:
            act['moderation'] = 'hold'
        act['ticket'] = f"note on #{facts['ticketId']}" if facts['ticketId'] else 'none (logged)'
        act['reopen'] = 'never'
        return act
    if facts['ticketId']:
        act['ticket'] = f"append to #{facts['ticketId']}"
        if facts['ticketStatus'] and 'append (ticket open)' in facts['factsDecide']:
            act['reopen'] = 'n/a (open)'
        elif d == 'acknowledgement':
            act['reopen'] = 'never (acknowledgement)'
        elif 'no reopen' in facts['factsDecide']:
            act['reopen'] = 'never (' + facts['factsDecide'].split('(', 1)[1]
        else:
            act['reopen'] = 'Stage 3 decides (step 5)'
        act['render'] = (f"--- New Email Update ---\n\nFrom: {rec['fromAddr']}\n\n"
                         f"{verdict['cleanBodyTextile']}")
        return act
    act['ticket'] = 'new ticket'
    act['newSubject'] = f"{listTag(rec)} {verdict['canonicalSubject']}".strip()
    act['render'] = (f"*Summary:* {verdict['summaryLine']}\n\n*From:* {rec['fromAddr']}\n\n"
                     f"{verdict['cleanBodyTextile']}")
    return act


def stage1(conn, rec, facts):
    """Classify one message, decide its action, and log both."""
    matchedSubject = None
    if facts['ticketId']:
        matchedSubject = f"#{facts['ticketId']} ({facts['ticketStatus']})"
    try:
        verdict, meta = classify(conn, rec, facts, matchedSubject)
    except (AskAuthError, AskLimitError, CallCapReached):
        raise
    except AskError as e:
        logDecision('disposition', rec['messageId'], error=str(e), fromAddr=rec['fromAddr'],
                    subject=rec['subject'])
        logger.error(f"  Stage 1 failed for {rec['messageId']}: {e}")
        return None
    sources = [rec['bodyText'], htmlToStructuredText(rec['bodyHtml']) if rec['bodyHtml'] else '']
    ok, unmatched, total = (True, 0, 0) if meta['cached'] else \
        wordsPreserved(verdict['cleanBodyTextile'], sources)
    if not ok:
        # Fall back to the raw text, never to a body that may have been reworded.
        verdict['cleanBodyTextile'] = '<pre>\n' + rec['bodyText'].strip() + '\n</pre>'
    if not facts['ticketId'] and verdict['disposition'] not in NOT_FILED:
        facts = subjectMatchStage(conn, rec, verdict, facts)
    act = decideAction(rec, verdict, facts)
    if act['reopen'].startswith('Stage 3'):
        reopenStage(rec, verdict, facts, act)
    if verdict['hubName'] or verdict['hubUrl']:
        hubStage(conn, rec, verdict, facts, act)
    logDecision('disposition', rec['messageId'], fromAddr=rec['fromAddr'],
                subject=rec['subject'], lists=rec['lists'], source=rec['source'],
                disposition=verdict['disposition'], reason=verdict['dispositionReason'],
                confidence=verdict['confidence'],
                sensitivePersonalInfo=verdict['sensitivePersonalInfo'],
                category=verdict['category'], playbooks=verdict.get('playbooks', []),
                canonicalSubject=verdict['canonicalSubject'],
                digestTopic=verdict['digestTopic'], summaryLine=verdict['summaryLine'],
                hubName=verdict['hubName'], hubUrl=verdict['hubUrl'],
                assemblies=verdict['assemblies'], urls=verdict['urls'],
                attachmentRoles=verdict['attachmentRoles'],
                wordsPreserved=ok, wordsUnmatched=unmatched, wordsTotal=total, **meta)
    logDecision('action', rec['messageId'], fromAddr=rec['fromAddr'], subject=rec['subject'],
                **act)
    act['_verdict'] = verdict
    return act


# ---------------------------------------------------------------------------
# Stage 3 (step 5): does a reply on a closed ticket need us again?

MODEL_REOPEN = 'opus'
MODEL_REOPEN_FALLBACK = 'sonnet'
HISTORY_EVENTS = 12             # most recent ticket events shown
HISTORY_NOTE_CHARS = 1200

REOPEN_SCHEMA = {
    'type': 'object',
    'additionalProperties': False,
    'required': ['conversationState', 'shouldReopen', 'reason', 'confidence'],
    'properties': {
        'conversationState': {'type': 'string',
                              'enum': ['awaiting_us', 'awaiting_user', 'resolved']},
        'shouldReopen': {'type': 'boolean'},
        'reason': {'type': 'string'},
        'confidence': {'type': 'number', 'minimum': 0, 'maximum': 1},
    },
}

REOPEN_SYSTEM = """You decide whether a new email on a CLOSED support ticket needs the UCSC
Genome Browser support team to act again. The email is always added to the ticket; your
answer only decides whether the ticket is reopened so the person on duty sees it. Answer
only through the JSON schema.

Reopen (shouldReopen true, conversationState awaiting_us) when the email asks something new
or follow-up, reports that the problem is not fixed or has come back, sends information the
team asked for, or otherwise expects an answer or an action from the team.

Do not reopen (shouldReopen false) when the email only thanks the team, confirms that
something worked, says the person will try it and report back later, closes the
conversation, or is information the team does not need to act on (conversationState
resolved or awaiting_user).

Judge from the ticket history: what the team last said and whether it left the person with
something to do. A thank-you that also carries something for the team is a reopen: a real
question, an answer to a question the team asked, a choice between options the team
offered, files or details the team requested, or a difficulty that remains, even if
mentioned in passing ("it works, but it is hard to find again"). Only a message with
nothing the team could act on or want to know stays closed.
reason: one sentence a team member can check against the email. confidence: how sure you
are, 0 to 1."""


def ticketHistory(ticketId, before):
    """A closed ticket as it stood when a message arrived: the question, then its most
    recent emails, notes and status changes. AI drafts are left out."""
    issue = dbQuery("select i.subject, i.description, i.created_on from issues i where i.id=%s",
                    (ticketId,))
    if not issue:
        return None
    cutoff = toDbTime(before)
    names = {str(r['id']): r['name'] for r in dbQuery("select id, name from issue_statuses")}
    events = []
    for j in dbQuery(
            "select j.id, j.created_on, j.notes, u.login, "
            "  (select group_concat(concat(d.old_value, '>', d.value)) from journal_details d "
            "   where d.journal_id=j.id and d.prop_key='status_id') status "
            "from journals j join users u on u.id=j.user_id "
            "where j.journalized_id=%s and j.created_on < %s order by j.id", (ticketId, cutoff)):
        when = str(j['created_on'])[:16]
        note = (j['notes'] or '').strip()
        if j['status']:
            changes = ', '.join(f"{names.get(a, a)} -> {names.get(b, b)}"
                                for a, b in (c.split('>') for c in j['status'].split(',')))
            events.append(f"[{when}] status changed by {j['login']}: {changes}")
        if not note or note.startswith('--- AI Suggested Response'):
            continue
        m = re.match(r'--- New Email Update ---\s*\n\s*From:\s*(.+)\n', note)
        label = f"email from {m.group(1).strip()}" if m else f"note by staff member {j['login']}"
        body = note[m.end():].strip() if m else note
        events.append(f"[{when}] {label}:\n{body[:HISTORY_NOTE_CHARS]}")
    head = (f"Ticket #{ticketId}: {issue[0]['subject']} (opened {str(issue[0]['created_on'])[:10]})\n"
            f"Original question:\n{questionText(issue[0]['description'])[:2000]}")
    shown = events[-HISTORY_EVENTS:]
    skipped = len(events) - len(shown)
    return head + (f"\n\n({skipped} earlier events not shown)" if skipped else '') + \
        "\n\n" + '\n\n'.join(shown)


def reopenJudgment(history, sender, isOpener, body):
    """One Stage 3 call on a ticket history from ticketHistory(). Returns the decision
    dict, or raises AskError. No database access, so the trial can run it in threads."""
    prompt = (f"TICKET HISTORY (the ticket is closed):\n<<<\n{history}\n>>>\n\n"
              f"NEW EMAIL from {sender}"
              f"{' (the person who opened the ticket)' if isOpener else ''}:\n<<<\n{body}\n>>>")
    return ask(prompt, REOPEN_SCHEMA, MODEL_REOPEN, MODEL_REOPEN_FALLBACK, REOPEN_SYSTEM)


def reopenStage(rec, verdict, facts, act):
    """Decide the reopen for a reply on a closed ticket and record it in act.

    Uncertain means reopen: a wrong reopen is visible and costs a click, a wrong
    non-reopen leaves a question unseen on a closed ticket."""
    try:
        decision = reopenJudgment(ticketHistory(facts['ticketId'], rec['receivedAt']),
                                  rec['fromAddr'], rec['fromAddr'] == facts.get('ticketOpener'),
                                  verdict['cleanBodyTextile'])
    except (AskAuthError, AskLimitError, CallCapReached):
        raise
    except AskError as e:
        act['reopen'] = 'reopen (Stage 3 failed, so reopen to be safe)'
        logDecision('reopen', rec['messageId'], error=str(e), ticketId=facts['ticketId'])
        return
    unsure = decision['confidence'] < UNSURE_BELOW
    reopen = decision['shouldReopen'] or unsure
    act['reopen'] = (('reopen' if reopen else 'no reopen')
                     + (' (not sure, so reopen)' if unsure and not decision['shouldReopen'] else '')
                     + f": {decision['reason']}")
    logDecision('reopen', rec['messageId'], ticketId=facts['ticketId'],
                fromAddr=rec['fromAddr'], subject=rec['subject'], reopen=reopen,
                unsure=unsure, model=MODEL_REOPEN, **decision)


# ---------------------------------------------------------------------------
# Stage 2 (step 6): which existing ticket, if any, a message without an exact match
# belongs to. Candidates come from the subject and the sender; a model decides.

CANDIDATES_PER_SOURCE = 6
MIN_SUBJECT_CHARS = 8

MATCH_SCHEMA = {
    'type': 'object',
    'additionalProperties': False,
    'required': ['ticketId', 'confidence', 'reason'],
    'properties': {
        'ticketId': {'type': 'integer'},
        'confidence': {'type': 'number', 'minimum': 0, 'maximum': 1},
        'reason': {'type': 'string'},
    },
}

MATCH_SYSTEM = """You decide whether an email to the UCSC Genome Browser support lists
continues one of a few existing support tickets, or starts a new conversation. Answer
only through the JSON schema: ticketId is the number of the ticket it continues, or 0 if
none.

It continues a ticket when it is part of that same conversation: it quotes or answers
that thread, follows up on the same problem, or sends what that thread asked for. A
shared subject is not enough. A generic subject ("bugs", "question", "help") or the same
sender writing about something else is a new conversation. A reply to a mailing-list
digest belongs to the ticket for the topic it quotes, if one is listed. The quoted text
in the email is the best evidence. reason: one sentence a person can check. confidence:
how sure you are, 0 to 1."""


def matchCandidates(rec, verdict):
    """MLQ tickets the message might continue: by subject, then by sender. A Rejected
    ticket is replaced by the ticket it duplicates, or dropped."""
    subject = (verdict.get('digestTopic') or verdict['canonicalSubject'] or '').strip()
    ids = []
    if len(subject) >= MIN_SUBJECT_CHARS:
        ids += [r['id'] for r in dbQuery(
            "select id from issues where tracker_id=7 and subject like %s order by id desc "
            "limit %s", ('%' + subject.replace('%', '') + '%', CANDIDATES_PER_SOURCE))]
    # The sender's other tickets are evidence only for something Stage 1 judged a reply:
    # for a new question, the same person writing again is usually a new topic (a
    # Retrogenes question was matched to the sender's lincRNA ticket before this rule).
    if verdict['disposition'] != 'question':
        ids += [r['id'] for r in dbQuery(
            "select i.id from issues i join custom_values cv on cv.customized_id=i.id "
            "and cv.customized_type='Issue' and cv.custom_field_id=40 "
            "where i.tracker_id=7 and cv.value like %s order by i.id desc limit %s",
            ('%' + rec['fromAddr'] + '%', CANDIDATES_PER_SOURCE))]
    # Only tickets that existed when the email arrived: otherwise a shadow or backfill run
    # finds the ticket mlqAutomate.py created from this very email and "matches" it.
    arrived = toDbTime(rec['receivedAt'])
    candidates = []
    for tid in dict.fromkeys(ids):
        facts = ticketFacts(tid)
        if facts and facts['statusId'] == STATUS_REJECTED:
            facts = ticketFacts(facts['duplicateOf']) if facts.get('duplicateOf') else None
        if facts and facts['created'] < arrived and \
                all(c['ticketId'] != facts['ticketId'] for c in candidates):
            candidates.append(facts)
    return candidates


def candidateBlock(ticket):
    """What the model sees of one candidate ticket."""
    rows = dbQuery("select i.created_on, i.updated_on, i.description from issues i where i.id=%s",
                   (ticket['ticketId'],))
    last = dbQuery("select j.notes from journals j join users u on u.id=j.user_id "
                   "where j.journalized_id=%s and j.notes like %s order by j.id desc limit 1",
                   (ticket['ticketId'], '%--- New Email Update ---%'))
    r = rows[0]
    return (f"#{ticket['ticketId']} [{ticket['status']}, opened {str(r['created_on'])[:10]}, "
            f"last activity {str(r['updated_on'])[:10]}] {ticket['subject']}\n"
            f"Opened by: {ticket['opener']}\n"
            f"Question: {questionText(r['description'])[:600]}\n"
            + (f"Latest email: {last[0]['notes'][:400]}" if last else ''))


def subjectMatchStage(conn, rec, verdict, facts):
    """Stage 2: return facts for the ticket this message continues, or the facts
    unchanged when it is new. Every subject-only match is checked by the model,
    however many candidates there are and however old they are (plan section 3)."""
    candidates = matchCandidates(rec, verdict)
    if not candidates:
        logDecision('match', rec['messageId'], candidates=[], ticketId=None,
                    reason='no candidate tickets', fromAddr=rec['fromAddr'],
                    subject=rec['subject'])
        return facts
    prompt = ("CANDIDATE TICKETS:\n\n" + '\n\n'.join(candidateBlock(c) for c in candidates)
              + f"\n\nEMAIL\nFrom: {rec['from']}\nSubject: {rec['subject']}\nDate: {rec['date']}\n"
              + (f"Digest topic: {verdict['digestTopic']}\n" if verdict.get('digestTopic') else '')
              + f"Triage judged it: {verdict['disposition']}, about: {verdict['summaryLine']}\n"
              + f"\n{rec['bodyText'][:4000]}")
    try:
        verdictMatch = ask(prompt, MATCH_SCHEMA, CONFIG['MODEL_JUDGE'],
                           CONFIG['MODEL_JUDGE_FALLBACK'], MATCH_SYSTEM)
    except (AskAuthError, AskLimitError, CallCapReached):
        raise
    except AskError as e:
        logDecision('match', rec['messageId'], error=str(e),
                    candidates=[c['ticketId'] for c in candidates])
        return facts
    chosen = next((c for c in candidates if c['ticketId'] == verdictMatch['ticketId']), None)
    logDecision('match', rec['messageId'], candidates=[c['ticketId'] for c in candidates],
                ticketId=chosen['ticketId'] if chosen else None,
                modelTicketId=verdictMatch['ticketId'], confidence=verdictMatch['confidence'],
                reason=verdictMatch['reason'], fromAddr=rec['fromAddr'], subject=rec['subject'],
                model=CONFIG['MODEL_JUDGE'])
    if not chosen:
        return facts
    return factsForTicket(conn, rec, chosen, 'subjectMatch', facts['notes'], ACCOUNTS_OK)


# ---------------------------------------------------------------------------
# Stage 4 (step 6): hub routing. A message about a hub that has a ticket on the GB Hub
# tracker also goes to that ticket (Lou, 2026-09-21): update it, relate the two, leave
# a pointer on the MLQ, keep the MLQ. Shadow mode only logs what would be done.

HUB_TRACKER = 45
HUB_SCHEMA = {
    'type': 'object',
    'additionalProperties': False,
    'required': ['hubTicketId', 'confidence', 'reason'],
    'properties': {
        'hubTicketId': {'type': 'integer'},
        'confidence': {'type': 'number', 'minimum': 0, 'maximum': 1},
        'reason': {'type': 'string'},
    },
}

HUB_SYSTEM = """You match an email about a UCSC Genome Browser track hub or assembly hub to
the team's Hub tracker ticket for that same hub, if there is one. Answer only through the
JSON schema: hubTicketId is the ticket number, or 0 if none of the listed tickets is about
this hub. Match on the hub itself (its name, its hub.txt URL, the group running it), not on
a general topic. A request for a new GenArk assembly is not a hub ticket. reason: one
sentence a person can check. confidence: 0 to 1."""


def hubTickets():
    """Every Hub tracker ticket: number, subject and any hub.txt URLs in its description."""
    out = []
    for r in dbQuery("select i.id, i.subject, i.description, s.name status from issues i "
                     "join issue_statuses s on s.id=i.status_id where i.tracker_id=%s "
                     "order by i.id desc", (HUB_TRACKER,)):
        urls = sorted(set(re.findall(r'https?://\S+?hub\.txt', r['description'] or '')))[:3]
        out.append(f"#{r['id']} [{r['status']}] {r['subject']}"
                   + (f"  hubs: {' '.join(urls)}" if urls else ''))
    return out


def hubStage(conn, rec, verdict, facts, act):
    """Stage 4: find the Hub ticket for a hub-related message and log what would be done.
    The returned ticket must exist and be on the Hub tracker before anything is planned
    (plan section 5, the two cross-tracker guards)."""
    prompt = (f"HUB TRACKER TICKETS:\n" + '\n'.join(hubTickets())
              + f"\n\nEMAIL\nFrom: {rec['from']}\nSubject: {rec['subject']}\n"
              f"Hub named: {verdict['hubName']}\nHub URL: {verdict['hubUrl']}\n\n"
              f"{verdict['cleanBodyTextile'][:3000]}")
    try:
        hub = ask(prompt, HUB_SCHEMA, CONFIG['MODEL_JUDGE'], CONFIG['MODEL_JUDGE_FALLBACK'],
                  HUB_SYSTEM)
    except (AskAuthError, AskLimitError, CallCapReached):
        raise
    except AskError as e:
        logDecision('hub', rec['messageId'], error=str(e))
        return
    plan = []
    hubId = hub['hubTicketId']
    if hubId:
        row = dbQuery("select tracker_id from issues where id=%s", (hubId,))
        if not row or row[0]['tracker_id'] != HUB_TRACKER:
            logDecision('hub', rec['messageId'], hubTicketId=hubId, refused=True,
                        reason=f"#{hubId} is not a Hub tracker ticket", modelReason=hub['reason'])
            return
        plan.append(f"post the message on Hub ticket #{hubId}")
        mlq = facts.get('ticketId')
        if mlq:
            related = dbQuery("select 1 from issue_relations where (issue_from_id=%s and "
                              "issue_to_id=%s) or (issue_from_id=%s and issue_to_id=%s)",
                              (mlq, hubId, hubId, mlq))
            if not related:
                plan.append(f"relate MLQ #{mlq} to Hub #{hubId}")
            pointer = dbQuery("select 1 from journals where journalized_id=%s and notes like %s",
                              (mlq, f'%#{hubId}%'))
            if not pointer:
                plan.append(f"leave a pointer to Hub #{hubId} on MLQ #{mlq}")
        else:
            plan.append(f"relate the new MLQ to Hub #{hubId} and leave a pointer on it")
        act['hub'] = '; '.join(plan)
        act['hubTicketId'] = hubId
    logDecision('hub', rec['messageId'], hubTicketId=hubId or None, plan=plan,
                confidence=hub['confidence'], reason=hub['reason'], hubName=verdict['hubName'],
                hubUrl=verdict['hubUrl'], fromAddr=rec['fromAddr'], subject=rec['subject'])


# ---------------------------------------------------------------------------
# Index of answered MLQs (step 3): what "similar past tickets" are drawn from

INDEX_SINCE = '2021-01-01'      # older answers describe a browser that has since changed
INDEX_CHUNK = 400               # tickets per database query
INDEX_TEXT_CHARS = 4000         # per question and per answer
GREETING = re.compile(r"^\s*(hi|hello|dear|good (morning|afternoon|evening)|greetings)\b"
                      r"[^\n]{0,60}$", re.I | re.M)
QUOTE_CUT = re.compile(r"^\s*(>|On .{0,200}wrote:\s*$|-----\s*Original Message|"
                       r"From: .*@|________________)", re.I)
STOPWORDS = set("""a about after all also am an and any are as at be been but by can could did
do does for from get got had has have hello hi how i if in into is it its just me my need no
not of on or our please re so some than thank thanks that the their them then there these
they this to too ucsc genome browser us use using was we were what when where which while
who why will with would you your""".split())


def indexPath(stateDir):
    return os.path.join(stateDir, 'index.sqlite')


def userFacingReply(note, staffFirstNames, question):
    """The part of a staff note written to the user, or None for internal discussion.

    Same rule as the draft baseline (RM #38234): the note must contain a greeting
    line, and a greeting naming a staff member the question does not mention is
    staff talking to staff. Quoted text after the reply is cut."""
    text = (note or '').replace('\r', '')
    m = GREETING.search(text)
    if not m:
        return None
    words = re.findall(r'[a-z]+', m.group(0).lower())
    if len(words) > 1 and words[1] in staffFirstNames and words[1] not in question.lower():
        return None
    out = []
    for line in text[m.start():].split('\n'):
        if QUOTE_CUT.match(line):
            break
        out.append(line)
    return '\n'.join(out).strip() or None


def questionText(description):
    """A ticket's question without the bot's Summary/From header lines."""
    text = (description or '').replace('\r', '')
    text = re.sub(r'^\s*\*Summary:\*[^\n]*\n', '', text)
    text = re.sub(r'^\s*\*?From:\*?[^\n]*\n', '', text.lstrip())
    return text.strip()


def buildIndex(stateDir):
    """Rebuild the answered-MLQ index from the Redmine database into a new file,
    then swap it in, so a reader never sees a half-built index."""
    started = time.time()
    staffAddrs = {r['address'] for r in dbQuery(
        "select lower(e.address) address from email_addresses e join users u on u.id=e.user_id "
        "where u.type='User' and u.login<>'gbauto'")}
    staffFirst = {r['f'] for r in dbQuery(
        "select distinct lower(firstname) f from users where type='User' and firstname<>''")}
    tmp = indexPath(stateDir) + '.new'
    if os.path.exists(tmp):
        os.remove(tmp)
    idx = sqlite3.connect(tmp)
    idx.executescript("""
        create table tickets (id integer primary key, created text, category text,
                              subject text, question text, answer text, status text);
        create virtual table ticketsFts using fts5(subject, question, answer,
            content='tickets', content_rowid='id', tokenize='porter unicode61');
        create table meta (key text primary key, value text);
    """)
    ids = [r['id'] for r in dbQuery(
        "select i.id from issues i join issue_statuses s on s.id=i.status_id "
        "where i.tracker_id=7 and i.created_on>=%s and s.is_closed=1 order by i.id",
        (INDEX_SINCE,))]
    kept = 0
    for start in range(0, len(ids), INDEX_CHUNK):
        chunk = ids[start:start + INDEX_CHUNK]
        marks = ','.join(['%s'] * len(chunk))
        issues = {r['id']: r for r in dbQuery(
            f"select i.id, i.created_on, i.subject, i.description, s.name status, cv.value cat "
            f"from issues i join issue_statuses s on s.id=i.status_id "
            f"left join custom_values cv on cv.customized_id=i.id and cv.customized_type='Issue' "
            f"  and cv.custom_field_id=28 where i.id in ({marks})", chunk)}
        answers = {}
        for j in dbQuery(
                f"select j.journalized_id id, j.notes, u.login from journals j "
                f"join users u on u.id=j.user_id where j.journalized_id in ({marks}) "
                f"and j.notes<>'' order by j.id", chunk):
            note = j['notes']
            if note.lstrip().startswith('--- AI Suggested Response'):
                continue
            question = questionText(issues[j['id']]['description'])
            if j['login'] == 'gbauto':
                m = re.match(r'\s*--- New Email Update ---\s*\n\s*From:\s*(.+)', note)
                sender = headerAddress(m.group(1)) if m else ''
                if not (sender in staffAddrs or re.search(r'@(soe\.)?ucsc\.edu$', sender)):
                    continue
                reply = note[m.end():].strip() if m else ''
            else:
                reply = userFacingReply(note, staffFirst, question)
            if reply:
                answers.setdefault(j['id'], []).append(reply)
        for tid, issue in issues.items():
            if tid not in answers:
                continue
            answer = '\n\n---\n\n'.join(answers[tid][:3])[:INDEX_TEXT_CHARS]
            idx.execute("insert into tickets values (?,?,?,?,?,?,?)",
                        (tid, str(issue['created_on']), issue['cat'] or 'Other',
                         issue['subject'] or '',
                         questionText(issue['description'])[:INDEX_TEXT_CHARS],
                         answer, issue['status']))
            kept += 1
    idx.execute("insert into ticketsFts(ticketsFts) values ('rebuild')")
    idx.execute("insert into meta values ('builtAt', ?)", (utcNow(),))
    idx.commit()
    idx.close()
    os.replace(tmp, indexPath(stateDir))
    logDecision('index', None, closedTickets=len(ids), indexed=kept,
                seconds=round(time.time() - started, 1))
    logger.info(f"Built answered-MLQ index: {kept} of {len(ids)} closed tickets since "
                f"{INDEX_SINCE} have a staff answer ({time.time() - started:.0f}s)")
    return kept


def similarTickets(stateDir, text, category, k, exclude=(), before=None):
    """The k answered tickets most like `text`, same category first.

    BM25 over subject, question and answer (subject weighted highest). Tickets
    in `exclude` are never returned, and with `before` only tickets created
    earlier are (the trial uses both, so a test ticket never sees its own answer
    or one written after it)."""
    words = [w for w in dict.fromkeys(re.findall(r'[a-z0-9][a-z0-9_.-]+', text.lower()))
             if w not in STOPWORDS and len(w) > 2][:40]
    if not words:
        return []
    query = ' OR '.join('"' + w.replace('"', '') + '"' for w in words)
    idx = sqlite3.connect(indexPath(stateDir))
    exclude = set(exclude)
    found = []
    for sameCategory in (True, False):
        sql = ("select t.id, t.created, t.category, t.subject, t.question, t.answer "
               "from ticketsFts f join tickets t on t.id=f.rowid where ticketsFts match ? "
               + ("and t.category=? " if sameCategory else "")
               + "order by bm25(ticketsFts, 4.0, 1.0, 1.0) limit ?")
        limit = k * 3 + len(exclude) + (500 if before else 0)
        args = (query, category, limit) if sameCategory else (query, limit)
        for row in idx.execute(sql, args):
            if row[0] in exclude or any(f['id'] == row[0] for f in found):
                continue
            if before and row[1] >= before:
                continue
            found.append(dict(zip(('id', 'created', 'category', 'subject', 'question',
                                   'answer'), row)))
            if len(found) == k:
                break
        if len(found) == k:
            break
    idx.close()
    return found


def writeCorpusDump(stateDir, dumpDir, exclude=()):
    """One text file per indexed ticket, for a draft call that may Grep past answers."""
    os.makedirs(dumpDir, exist_ok=True)
    idx = sqlite3.connect(indexPath(stateDir))
    exclude = set(exclude)
    n = 0
    for tid, created, cat, subject, question, answer in idx.execute(
            "select id, created, category, subject, question, answer from tickets"):
        if tid in exclude:
            continue
        with open(os.path.join(dumpDir, f"{tid}.txt"), 'w') as f:
            f.write(f"Ticket #{tid} ({created[:10]}, {cat})\nSubject: {subject}\n\n"
                    f"QUESTION:\n{question}\n\nSTAFF ANSWER:\n{answer}\n")
        n += 1
    idx.close()
    return n


# ---------------------------------------------------------------------------
# Draft reply (step 4; built in step 3 so the trial tests the production code)

INTERNAL_HOSTS = re.compile(r'hgwdev|genome-test|hgwbeta|hgwalpha|/hive/|/cluster/|'
                            r'\bgenecats\b', re.I)
PAST_TICKET_CHARS = 2500        # per past ticket: question and answer, each

DRAFT_SCHEMA = {
    'type': 'object',
    'additionalProperties': False,
    'required': ['draftTextile', 'ticketsUsed', 'urlsCited', 'confidence'],
    'properties': {
        'draftTextile': {'type': 'string'},
        'ticketsUsed': {'type': 'array', 'items': {'type': 'integer'}},
        'urlsCited': {'type': 'array', 'items': {'type': 'string'}},
        'confidence': {'type': 'number', 'minimum': 0, 'maximum': 1},
    },
}

DRAFT_SYSTEM = """You draft a reply to an email sent to the UCSC Genome Browser support
team. A team member (the MLM on duty) reads your draft, edits it and sends it, so write
the reply they would send, not notes to them. Answer only through the JSON schema.

What you are given:
- The PLAYBOOK for this kind of question: the team's own guidelines (links we give,
  questions we ask, answers by situation, things we never say). Follow it. Where a past
  ticket disagrees with the playbook, the playbook wins; past answers can be out of date.
- PAST TICKETS: similar questions and how the team answered them. Use them for what we
  actually say and link; do not copy details that belonged to that other person's case.
- Possibly a folder of more past tickets you can search with Grep, Glob and Read.
- The EMAIL to answer.

Rules:
- Stay within using the Genome Browser and its data and tools. Do not interpret biology
  or clinical findings, do not give medical advice, do not promise dates or that
  something will be built or fixed, and do not speak for other organizations.
- If something needed to answer is missing (assembly, exact steps, a session link, an
  error message), ask for it the way the playbook says rather than guessing.
- Only use URLs that appear in the playbook, in the past tickets or in the email. Never
  make a URL up. Never mention internal servers or paths (hgwdev, genome-test, hgwbeta,
  /hive, /cluster) unless the email itself does.
- Start "Hello, <first name>." when the sender's first name is known, else "Hello,".
  Be brief and direct: a few short paragraphs. End with the team's usual closing about
  further questions if the playbook or past answers give one. Do not sign a name.
- Write Redmine Textile, not Markdown: no **, ##, backticks or backslash escapes. Bullet
  lists use "* ", numbered lists "# ". Commands and file contents go in <pre></pre>.
- ticketsUsed: the numbers of the past tickets you actually relied on. urlsCited: every
  URL in your draft. confidence: how likely a team member could send your draft with
  only light edits, 0 to 1."""


def pastTicketBlock(t):
    return (f"#{t['id']} ({t['created'][:10]}, {t['category']}) {t['subject']}\n"
            f"QUESTION:\n{t['question'][:PAST_TICKET_CHARS]}\n"
            f"STAFF ANSWER:\n{t['answer'][:PAST_TICKET_CHARS]}")


def draftPrompt(playbook, email, pastTickets, withDump):
    parts = [f"PLAYBOOK:\n<<<\n{playbook or '(no playbook for this category yet)'}\n>>>"]
    if pastTickets:
        parts.append("PAST TICKETS:\n" + '\n\n=====\n\n'.join(pastTicketBlock(t)
                                                            for t in pastTickets))
    if withDump:
        parts.append("The current folder holds more past tickets, one file per ticket "
                     "(<number>.txt with QUESTION and STAFF ANSWER). Search them with Grep "
                     "if the past tickets above do not cover the question.")
    parts.append(f"EMAIL:\nFrom: {email['from']}\nSubject: {email['subject']}\n\n"
                 f"{email['body']}")
    return '\n\n'.join(parts)


def urlLoads(url):
    """True if a URL the draft cites answers with a success status."""
    try:
        resp = requests.get(url, timeout=20, allow_redirects=True, stream=True,
                            headers={'User-Agent': 'mlqTriage link check'})
        resp.close()
        return resp.status_code < 400
    except requests.exceptions.RequestException:
        return False


def draftProblems(draft, email):
    """What is wrong with a draft, checked by the script rather than trusted to the model."""
    problems = []
    text = draft['draftTextile']
    for host in set(m.group(0) for m in INTERNAL_HOSTS.finditer(text)):
        if host.lower() not in email['body'].lower():
            problems.append(f"names an internal host or path the email did not: {host}")
    for url in set(re.findall(r'https?://[^\s<>"\')\]]+', text)):
        url = url.rstrip('.,;:')
        if not urlLoads(url):
            problems.append(f"URL does not load: {url}")
    # "# " starts a Textile numbered list, so only ** and backticks mark Markdown.
    if re.search(r'\*\*|`', text):
        problems.append("uses Markdown (** or backticks); write Textile")
    return problems


def draftReply(playbook, email, pastTickets, dumpDir=None, model=None):
    """Write one draft; check it; if it fails a check, retry once with the reasons.

    Returns (draft, problems, attempts). `problems` is empty for a clean draft."""
    tools = ['Read', 'Grep', 'Glob'] if dumpDir else None
    prompt = draftPrompt(playbook, email, pastTickets, bool(dumpDir))
    problems = []
    for attempt in (1, 2):
        if problems:
            prompt += ("\n\nYOUR PREVIOUS DRAFT HAD THESE PROBLEMS; fix them:\n- "
                       + '\n- '.join(problems))
        draft = ask(prompt, DRAFT_SCHEMA, model or CONFIG['MODEL_DRAFT'],
                    CONFIG['MODEL_DRAFT_FALLBACK'], DRAFT_SYSTEM, tools=tools, cwd=dumpDir,
                    timeout=600 if dumpDir else 300)
        draft['draftTextile'] = tidyTextile(draft['draftTextile'])
        problems = draftProblems(draft, email)
        if not problems:
            return draft, [], attempt
    return draft, problems, 2


# ---------------------------------------------------------------------------
# Draft stage (step 4): playbook + 5 similar past tickets, no tools (trial, plan 6c)

DRAFT_PAST_TICKETS = 5
INDEX_MAX_AGE_HOURS = 20
# MLQ category -> playbook file. Categories without a playbook draft from past tickets only.
PLAYBOOK_FILES = {
    'LiftOver': 'liftOver.md', 'Login': 'login.md', 'User Accounts': 'userAccounts.md',
    'IP blocked': 'ipBlocked.md', 'Sessions': 'sessions.md', 'CAPTCHA': 'captcha.md',
}
# New questions get a draft, and so do replies and digest replies that Stage 2 found
# belong to no existing ticket.
DRAFTED = {'question', 'reply', 'digest_reply'}


def ensureIndex(stateDir):
    """Rebuild the answered-MLQ index when missing or older than INDEX_MAX_AGE_HOURS.
    A rebuild takes a few seconds, so it rides along in a normal run once a day."""
    path = indexPath(stateDir)
    if os.path.exists(path):
        row = sqlite3.connect(path).execute("select value from meta where key='builtAt'").fetchone()
        if row and datetime.now(timezone.utc) - datetime.fromisoformat(row[0]) < \
                timedelta(hours=INDEX_MAX_AGE_HOURS):
            return
    buildIndex(stateDir)


def loadPlaybooks(names):
    """The approved playbooks for the given categories, joined, and which were found.
    A question can need a playbook outside its own category (a download blocked by the
    bot check is filed under Downloads but answered by the CAPTCHA playbook)."""
    texts, used = [], []
    for name in dict.fromkeys(names):
        fileName = PLAYBOOK_FILES.get(name)
        path = os.path.join(CONFIG.get('PLAYBOOK_DIR') or '', fileName or '')
        if fileName and CONFIG.get('PLAYBOOK_DIR') and os.path.exists(path):
            texts.append(open(path).read())
            used.append(name)
    return '\n\n=====\n\n'.join(texts), used


def staffAlreadyAnswered(conn, rec):
    """True if someone on an active Redmine account wrote in this thread earlier.
    A thread that becomes a new ticket after staff replied needs no draft (1c #8)."""
    return conn.execute(
        "select 1 from messages m join accounts a on a.address=m.fromAddr "
        "where m.gmailThreadId=? and m.receivedAt<? limit 1",
        (rec['gmailThreadId'], rec['receivedAt'])).fetchone() is not None


def relatedTicketsBlock(stateDir, ticketIds):
    """The "Related RM tickets" list under a draft: the tickets the draft relied on."""
    if not ticketIds:
        return ''
    idx = sqlite3.connect(indexPath(stateDir))
    lines = []
    for tid in ticketIds:
        row = idx.execute("select subject from tickets where id=?", (tid,)).fetchone()
        lines.append(f"* #{tid} {row[0] if row else ''}".rstrip())
    idx.close()
    return "\n\n*Related RM tickets:*\n" + '\n'.join(lines)


def draftStage(conn, stateDir, rec, verdict, act):
    """Write the draft a new ticket would carry, check it, and log it. Returns a
    short outcome string for the run log, or None when no draft is due."""
    if verdict['disposition'] not in DRAFTED or not act.get('render', '').startswith('*Summary:*'):
        return None
    if staffLogin(conn, rec['fromAddr']):
        return None                     # staff writing in need no draft
    if staffAlreadyAnswered(conn, rec):
        logDecision('draft', rec['messageId'], skipped='staff already answered in this thread',
                    fromAddr=rec['fromAddr'], subject=rec['subject'])
        return 'skipped (staff already answered)'
    playbook, playbooksUsed = loadPlaybooks([verdict['category']] + verdict.get('playbooks', []))
    queryText = '\n'.join([verdict['canonicalSubject'], verdict['summaryLine'],
                           verdict['cleanBodyTextile'][:1500]])
    # As of the email's arrival, so a backfill or test cannot draw on tickets opened later.
    past = similarTickets(stateDir, queryText, verdict['category'], DRAFT_PAST_TICKETS,
                          before=toDbTime(rec['receivedAt']))
    email = {'from': rec['from'], 'subject': verdict['canonicalSubject'] or rec['subject'],
             'body': verdict['cleanBodyTextile']}
    try:
        draft, problems, attempts = draftReply(playbook, email, past)
    except (AskAuthError, AskLimitError, CallCapReached):
        raise
    except AskError as e:
        logDecision('draft', rec['messageId'], error=str(e), fromAddr=rec['fromAddr'],
                    subject=rec['subject'])
        return f"failed: {e}"
    used = [t for t in draft['ticketsUsed'] if t in {p['id'] for p in past}]
    post = ("--- AI Suggested Response (Draft) ---\n\n" + draft['draftTextile']
            + relatedTicketsBlock(stateDir, used))
    act['_draftPost'] = post
    logDecision('draft', rec['messageId'], fromAddr=rec['fromAddr'], subject=rec['subject'],
                category=verdict['category'], playbooksUsed=playbooksUsed,
                pastGiven=[p['id'] for p in past], ticketsUsed=used,
                ticketsClaimedNotGiven=[t for t in draft['ticketsUsed'] if t not in used],
                urlsCited=draft['urlsCited'], confidence=draft['confidence'],
                problems=problems, attempts=attempts, model=CONFIG['MODEL_DRAFT'], post=post)
    return (f"drafted (confidence {draft['confidence']:.2f}, {len(used)} related ticket(s)"
            + (f", {len(problems)} check(s) still failing" if problems else '') + ')')


# ---------------------------------------------------------------------------
# Draft usage score: the baseline method (RM #38234 baseline/METHOD.md), ported
# unchanged so shadow drafts are scored exactly like the 5.6% baseline and the trial.

USED_SHINGLE, USED_SENT = 0.40, 0.50
PARTLY_SHINGLE, PARTLY_SENT, PARTLY_URLS = 0.08, 0.15, 1
GENERIC_URLS = {"genome.ucsc.edu", "genome.ucsc.edu/contacts.html", "genome.ucsc.edu/faq",
                "genome.ucsc.edu/training/index.html",
                "genome.ucsc.edu/goldenpath/help/contacts.html", "genome.ucsc.edu/license",
                "genome-euro.ucsc.edu", "genome-asia.ucsc.edu"}
BOILER = re.compile(r"^\s*(i hope this (is|was) helpful|if you have any (further|other|additional) "
                    r"questions|all messages sent to that address|(all )?messages sent to that "
                    r"address|if your question includes sensitive|please (don.t|do not) hesitate|"
                    r"please let us know if|let us know if you|looking forward)", re.I)
COURTESY = re.compile(r"^\s*(thank(s| you)\b|we appreciate|we apologi[sz]e|sorry)", re.I)


def _norm(t):
    t = re.sub(r"https?://", "", t.lower())
    return re.sub(r"[^\w./:#?=&-]+", " ", t).split()


def _urls(t):
    out = set()
    for u in re.findall(r"https?://[^\s)\]>\"']+", t):
        u = re.sub(r"^https?://", "", u.lower()).rstrip(".,;:/")
        if u not in GENERIC_URLS:
            out.add(u)
    return out


def _sentences(t):
    parts = re.split(r"(?<=[.!?])\s+|\n+", t)
    return [p for p in (" ".join(_norm(s)) for s in parts) if len(p.split()) >= 5]


def _dropBoiler(t):
    parts = re.split(r"(?<=[.!?])\s+|\n+", t)
    return "\n".join(p for p in parts if p.strip() and not BOILER.match(p)
                     and not (COURTESY.match(p) and len(p.split()) <= 9))


def draftUsage(draft, reply, question=''):
    """USED / PARTLY USED / NOT USED for one draft against the staff reply sent."""
    import difflib
    draft, reply = _dropBoiler(draft), _dropBoiler(reply)
    qw = _norm(question)
    qsh = {tuple(qw[i:i + 8]) for i in range(len(qw) - 7)}
    qtext = " ".join(qw)
    dw, rw = _norm(draft), _norm(reply)
    dsh = {tuple(dw[i:i + 8]) for i in range(len(dw) - 7)}
    rsh = [s for s in (tuple(rw[i:i + 8]) for i in range(len(rw) - 7)) if s not in qsh]
    shingle = sum(s in dsh for s in rsh) / len(rsh) if rsh else 0.0
    ds, rs = _sentences(draft), [x for x in _sentences(reply) if x not in qtext]
    hit = sum(1 for s in rs if any(difflib.SequenceMatcher(None, s, x, autojunk=False).ratio()
                                   >= 0.8 for x in ds))
    sent = hit / len(rs) if rs else 0.0
    shared = len((_urls(draft) & _urls(reply)) - _urls(question))
    if shingle >= USED_SHINGLE or (sent >= USED_SENT and len(rs) >= 3):
        return 'USED'
    if shingle >= PARTLY_SHINGLE or sent >= PARTLY_SENT or shared >= PARTLY_URLS:
        return 'PARTLY USED'
    return 'NOT USED'


def firstStaffReply(ticketId):
    """The first reply staff sent the user on a ticket, as the old script recorded it."""
    staffAddrs = {r['address'] for r in dbQuery(
        "select lower(e.address) address from email_addresses e join users u on u.id=e.user_id "
        "where u.type='User' and u.login<>'gbauto'")}
    for j in dbQuery("select j.notes, u.login from journals j join users u on u.id=j.user_id "
                     "where j.journalized_id=%s and j.notes<>'' order by j.id", (ticketId,)):
        note = j['notes']
        if note.lstrip().startswith('--- AI Suggested Response'):
            continue
        if j['login'] == 'gbauto':
            m = re.match(r'\s*--- New Email Update ---\s*\n\s*From:\s*(.+)', note)
            sender = headerAddress(m.group(1)) if m else ''
            if m and (sender in staffAddrs or re.search(r'@(soe\.)?ucsc\.edu$', sender)):
                return note[m.end():].strip()
        else:
            reply = userFacingReply(note, set(), '')
            if reply:
                return reply
    return None


# ---------------------------------------------------------------------------
# Live writer: carries out the decided action on Gmail and Redmine. Only --live
# reaches here. Each step is recorded in the state file as soon as it succeeds, so a
# rerun after a crash or failure never approves or posts twice; every write, done or
# failed, is a 'write' line in the decision log.

TRACKER_MLQ = 7
PRIORITY_UNPRIORITIZED = 12
STATUS_NEW = 1
CF_CATEGORY, CF_EMAIL, CF_MLM = 28, 40, 9
NO_SUBJECT = '<No Subject>'
MAX_ATTACHMENT_BYTES = 10 * 1024 * 1024
UPLOAD_ROLES = {'content', 'calendar_invite', 'other'}
WRITE_ATTEMPTS = 3

_mlm = {'name': None, 'looked': False}


class WriteError(Exception):
    """A Gmail or Redmine write failed after its retries."""


def noFourByte(text):
    """This Redmine stores utf8, not utf8mb4: drop emoji and other 4-byte characters."""
    return re.sub(r'[\U00010000-\U0010FFFF]', '', text or '')


def mlmOnDuty():
    """This run's MLM from the calendar, looked up once. None means unknown."""
    if not _mlm['looked']:
        _mlm['looked'] = True
        try:
            _mlm['name'] = currentMlm()
        except Exception as e:
            logger.error(f"MLM calendar lookup failed: {e}")
        if not _mlm['name']:
            sendAlert('mlm', 'No MLM on duty found',
                      'The calendar gave no MLM; tickets are filed with the MLM field empty.')
    return _mlm['name']


def logWrite(rec, kind, ok, **fields):
    logDecision('write', rec['messageId'], write=kind, ok=ok, fromAddr=rec['fromAddr'],
                subject=rec['subject'], **fields)
    (logger.info if ok else logger.error)(
        f"  write {kind}: {'ok' if ok else 'FAILED'} "
        + ' '.join(f"{k}={v}" for k, v in fields.items() if k != 'body')[:200])


def redmineWrite(method, path, **kwargs):
    """POST/PUT to Redmine, retrying server and network errors (not 4xx).
    Returns the parsed JSON body ({} for an empty 2xx); raises WriteError."""
    url = f"{CONFIG['REDMINE_URL']}{path}"
    headers = {'X-Redmine-API-Key': CONFIG['REDMINE_API_KEY']}
    headers.update(kwargs.pop('headers', {}))
    delay, err = 5, ''
    for attempt in range(1, 4):
        try:
            resp = requests.request(method, url, headers=headers, timeout=60, **kwargs)
            if resp.status_code < 300:
                return resp.json() if resp.content.strip() else {}
            err = f"HTTP {resp.status_code}: {resp.text[:300]}"
            if resp.status_code < 500:
                break
        except requests.exceptions.RequestException as e:
            err = f"network error: {e}"
        if attempt < 3:
            time.sleep(delay)
            delay *= 2
    raise WriteError(f"{method} {path}: {err}")


def approveMessage(rec):
    """Approve a pending message the way mlqAutomate.py does: an empty reply to the
    +msgappr@ address, threaded on the notification."""
    if not rec.get('approveAddr'):
        raise WriteError("no +msgappr@ address on the pending notification")
    message = MIMEText('')
    message['To'] = rec['approveAddr']
    message['From'] = 'gbauto@ucsc.edu'
    subject = rec.get('notificationSubject') or 'Moderation'
    message['Subject'] = subject if subject.startswith('Re:') else f"Re: {subject}"
    if rec.get('notificationMessageId'):
        message['In-Reply-To'] = rec['notificationMessageId']
        message['References'] = rec['notificationMessageId']
    raw = base64.urlsafe_b64encode(message.as_bytes()).decode('utf-8')
    try:
        gmailService().users().messages().send(userId='me', body={'raw': raw}).execute()
    except Exception as e:
        raise WriteError(f"approval send failed: {e}")


def trashNotification(rec):
    try:
        gmailService().users().messages().trash(userId='me', id=rec['notificationGmailId']).execute()
    except Exception as e:
        raise WriteError(f"trash failed: {e}")


def existingDigests(ticketId):
    """SHA-256 and MD5 digests of the files already on a ticket."""
    if not ticketId:
        return set()
    issue = redmineGet(f"/issues/{ticketId}.json", {'include': 'attachments'})['issue']
    return {a.get('digest', '').split(':')[-1] for a in issue.get('attachments', []) if a.get('digest')}


def uploadAttachments(rec, verdict, ticketId):
    """Download this message's attachments and upload the ones worth keeping.

    Skips what Stage 1 called a signature, logo, disclaimer image or cryptographic
    signature, anything over 10 MB, and any file already on the ticket (by digest),
    which stops the same quoted image being re-attached on every reply (1c #6).
    Returns Redmine upload dicts; a failed file is logged and skipped, not fatal."""
    import hashlib
    roles = {r['filename']: r['role'] for r in verdict.get('attachmentRoles', [])}
    have = existingDigests(ticketId)
    uploads = []
    service = gmailService()
    for att in rec['attachments']:
        name, role = att['filename'], roles.get(att['filename'], 'content')
        if role not in UPLOAD_ROLES or att['size'] > MAX_ATTACHMENT_BYTES or \
                att['mimeType'] in ('application/pkcs7-signature', 'application/x-pkcs7-signature'):
            logWrite(rec, 'attachment-skipped', True, filename=name, role=role, size=att['size'])
            continue
        try:
            data = service.users().messages().attachments().get(
                userId='me', messageId=rec['gmailId'], id=att['attachmentId']).execute()
            blob = base64.urlsafe_b64decode(data['data'])
            if hashlib.sha256(blob).hexdigest() in have or hashlib.md5(blob).hexdigest() in have:
                logWrite(rec, 'attachment-skipped', True, filename=name, reason='already on ticket')
                continue
            token = redmineWrite('POST', '/uploads.json', params={'filename': noFourByte(name)},
                                 data=blob,
                                 headers={'Content-Type': 'application/octet-stream'})['upload']['token']
            uploads.append({'token': token, 'filename': noFourByte(name),
                            'content_type': att['mimeType'], 'contentId': att['contentId']})
            logWrite(rec, 'attachment-uploaded', True, filename=name, size=len(blob))
        except Exception as e:
            logWrite(rec, 'attachment-upload', False, filename=name, error=str(e)[:300])
    return uploads


def placeImages(text, uploads):
    """{{image:ID}} placeholders from Stage 1 become Redmine inline images when the
    file was uploaded (matched by Content-Id, then file name); otherwise [image]."""
    byKey = {}
    for u in uploads:
        byKey[u['filename'].lower()] = u['filename']
        if u.get('contentId'):
            byKey[u['contentId'].lower()] = u['filename']

    def sub(m):
        key = m.group(1).strip().lower()
        name = byKey.get(key) or byKey.get(key.split('@')[0])
        return f"!{name}!" if name else '[image]'
    return re.sub(r'\{\{image:([^}]*)\}\}', sub, text or '')


def uploadField(uploads):
    return [{'token': u['token'], 'filename': u['filename'], 'content_type': u['content_type']}
            for u in uploads]


def createTicket(rec, verdict, act, uploads):
    subject = noFourByte(act.get('newSubject') or '').strip()
    if not verdict.get('canonicalSubject'):
        subject = NO_SUBJECT
    issue = {'project_id': CONFIG['REDMINE_PROJECT'], 'tracker_id': TRACKER_MLQ,
             'priority_id': PRIORITY_UNPRIORITIZED, 'status_id': STATUS_NEW,
             'subject': subject[:255],
             'description': noFourByte(placeImages(act['render'], uploads)),
             'custom_fields': [{'id': CF_CATEGORY, 'value': verdict['category']},
                               {'id': CF_EMAIL, 'value': rec['fromAddr']},
                               {'id': CF_MLM, 'value': mlmOnDuty() or ''}]}
    if uploads:
        issue['uploads'] = uploadField(uploads)
    return redmineWrite('POST', '/issues.json', json={'issue': issue})['issue']['id']


def postNote(ticketId, notes, uploads=None, reopen=False):
    issue = {'notes': noFourByte(notes)}
    if uploads:
        issue['uploads'] = uploadField(uploads)
    if reopen:
        # A real reopen sets this week's MLM and clears the assignee (Lou, 2026-11-03).
        issue['status_id'] = STATUS_NEW
        issue['assigned_to_id'] = ''
        issue['custom_fields'] = [{'id': CF_MLM, 'value': mlmOnDuty() or ''}]
    redmineWrite('PUT', f"/issues/{ticketId}.json", json={'issue': issue})


def hubWrites(rec, act, mlqId):
    """Stage 4's plan, done idempotently: the message on the Hub ticket, the relation,
    and one pointer on the MLQ. Each is checked again here, just before it is done."""
    hubId = act.get('hubTicketId')
    if not hubId or not mlqId:
        return
    row = dbQuery("select tracker_id from issues where id=%s", (hubId,))
    if not row or row[0]['tracker_id'] != HUB_TRACKER:
        logWrite(rec, 'hub', False, hubTicketId=hubId, error='not a Hub tracker ticket')
        return
    body = act.get('render', '').split('\n\n', 2)[-1]
    try:
        postNote(hubId, f"--- Email on MLQ #{mlqId} ---\n\nFrom: {rec['fromAddr']}\n\n{body}")
        logWrite(rec, 'hub-post', True, hubTicketId=hubId, ticketId=mlqId)
        if not dbQuery("select 1 from issue_relations where (issue_from_id=%s and issue_to_id=%s) "
                       "or (issue_from_id=%s and issue_to_id=%s)", (mlqId, hubId, hubId, mlqId)):
            redmineWrite('POST', f"/issues/{mlqId}/relations.json",
                         json={'relation': {'issue_to_id': hubId, 'relation_type': 'relates'}})
            logWrite(rec, 'hub-relate', True, hubTicketId=hubId, ticketId=mlqId)
        if not dbQuery("select 1 from journals where journalized_id=%s and notes like %s",
                       (mlqId, f'%Hub ticket #{hubId}%')):
            postNote(mlqId, f"This conversation is also on Hub ticket #{hubId}.")
            logWrite(rec, 'hub-pointer', True, hubTicketId=hubId, ticketId=mlqId)
    except WriteError as e:
        logWrite(rec, 'hub', False, hubTicketId=hubId, ticketId=mlqId, error=str(e))


def writeMessage(conn, rec, act):
    """Carry out one message's action. Returns True when everything that matters is
    done (moderation, and the ticket or note); False to retry on a later run."""
    verdict = act['_verdict']
    row = conn.execute("select moderationDone, writtenTicket from messages where messageId=?",
                       (rec['messageId'],)).fetchone()
    moderationDone, writtenTicket = row if row else (None, None)

    # 1. Moderation: only a message still on the Google Groups queue.
    if rec['source'] == 'pending' and not moderationDone:
        try:
            if act['moderation'] == 'approve':
                approveMessage(rec)
            trashNotification(rec)
            moderationDone = 'approved' if act['moderation'] == 'approve' else 'held'
            conn.execute("update messages set moderationDone=? where messageId=?",
                         (moderationDone, rec['messageId']))
            conn.commit()
            logWrite(rec, 'moderation', True, result=moderationDone, why=act.get('why', ''))
        except WriteError as e:
            logWrite(rec, 'moderation', False, error=str(e))
            return False
    # A held message is not filed, except a recall, which still gets its note.
    if act['ticket'].startswith('none') or \
            (act['moderation'] == 'hold' and not act['ticket'].startswith('note on')):
        logWrite(rec, 'not-filed', True, ticket=act['ticket'], why=act.get('why', ''))
        return True
    if writtenTicket:
        return True

    # 2. The ticket: create, append, or a recall note.
    try:
        if act['ticket'] == 'new ticket':
            uploads = uploadAttachments(rec, verdict, None)
            ticketId = createTicket(rec, verdict, act, uploads)
            kind = 'create'
        else:
            ticketId = int(re.search(r'#(\d+)', act['ticket']).group(1))
            if act['ticket'].startswith('note on'):
                uploads, kind = [], 'recall-note'
                postNote(ticketId, f"--- Recall request ---\n\nFrom: {rec['fromAddr']}\n\n"
                                   f"The sender asked to recall an earlier message.")
            else:
                uploads = uploadAttachments(rec, verdict, ticketId)
                reopen = act['reopen'].startswith('reopen')
                postNote(ticketId, placeImages(act['render'], uploads), uploads, reopen)
                kind = 'append-reopen' if reopen else 'append'
    except (WriteError, AttributeError, ValueError) as e:
        logWrite(rec, 'ticket', False, action=act['ticket'], error=str(e))
        return False
    conn.execute("update messages set writtenTicket=?, ticketId=? where messageId=?",
                 (ticketId, ticketId, rec['messageId']))
    conn.execute("insert or replace into threads(gmailThreadId, ticketId) values (?,?)",
                 (rec['gmailThreadId'], ticketId))
    conn.commit()
    logWrite(rec, kind, True, ticketId=ticketId, attachments=len(uploads), reopen=act['reopen'])

    # 3. Extras that must not block or repeat the filing: the draft and hub routing.
    if kind == 'create' and act.get('_draftPost'):
        try:
            postNote(ticketId, act['_draftPost'])
            logWrite(rec, 'draft', True, ticketId=ticketId)
        except WriteError as e:
            logWrite(rec, 'draft', False, ticketId=ticketId, error=str(e))
    hubWrites(rec, act, ticketId)
    return True


# ---------------------------------------------------------------------------
# One run

def messageFacts(conn, rec, accountsOk):
    """Step 1: the facts about one new message, and what they alone decide."""
    ticket, matchedBy, notes = resolveTicket(conn, rec)
    return factsForTicket(conn, rec, ticket, matchedBy, notes, accountsOk)


def factsForTicket(conn, rec, ticket, matchedBy, notes, accountsOk):
    """What the facts decide for a message once its ticket is known, whether it was
    found exactly (step 1) or by subject (Stage 2)."""
    login = staffLogin(conn, rec['fromAddr'])
    facts = {'staffLogin': login, 'matchedBy': matchedBy, 'notes': notes,
             'ticketId': ticket['ticketId'] if ticket else None,
             'ticketStatus': ticket['status'] if ticket else None,
             'ticketOpener': ticket['opener'] if ticket else None}
    if not ticket:
        facts['factsDecide'] = 'no exact match: Stage 1-2 decide new ticket or subject match'
    elif not ticket['isClosed']:
        facts['factsDecide'] = 'append (ticket open)'
    elif not accountsOk:
        facts['factsDecide'] = 'append, no reopen (account list not trusted: fail closed)'
    elif login and rec['fromAddr'] != ticket['opener']:
        facts['factsDecide'] = 'append, no reopen (staff reply in a thread they did not start)'
    else:
        facts['factsDecide'] = 'append; reopen decided by Stage 3'
    if ticket:
        conn.execute("update messages set ticketId=? where messageId=?",
                     (ticket['ticketId'], rec['messageId']))
        conn.execute("insert or ignore into threads(gmailThreadId, ticketId) values (?,?)",
                     (rec['gmailThreadId'], ticket['ticketId']))
        conn.commit()
    return facts


def runStage1(conn, rec, facts, counts):
    """Run Stage 1 on one message and record the outcome in the state file."""
    act = stage1(conn, rec, facts)      # a limit or auth failure raises before counting
    conn.execute("update messages set attempts=coalesce(attempts, 0)+1 where messageId=?",
                 (rec['messageId'],))
    drafted = None
    if act and INDEX_OK:
        drafted = draftStage(conn, STATE_DIR, rec, act['_verdict'], act)
        if drafted:
            counts['drafts'] = counts.get('drafts', 0) + 1
    status = 'classified' if act else 'error'
    if act and not SHADOW:
        conn.execute("update messages set writeAttempts=coalesce(writeAttempts, 0)+1 "
                     "where messageId=?", (rec['messageId'],))
        if writeMessage(conn, rec, act):
            status = 'written'
            counts['written'] = counts.get('written', 0) + 1
        else:
            status = 'write-error'
            counts['writeFailed'] = counts.get('writeFailed', 0) + 1
            sendAlert(f"write:{rec['messageId']}", 'A message could not be filed',
                      f"From {rec['fromAddr']}, subject {rec['subject']!r}: the write failed "
                      f"and will be retried on the next runs (up to {WRITE_ATTEMPTS}). See the "
                      f"'write' lines in the decision log.")
    conn.execute("update messages set action=? where messageId=?", (status, rec['messageId']))
    conn.commit()
    counts['done' if act else 'failed'] += 1
    if act:
        logger.info(f"  Stage 1: {act['disposition']}"
                    + (' (unsure)' if act['unsure'] else '')
                    + f"; moderation {act['moderation']}; ticket {act['ticket']}"
                    + (f"; reopen {act['reopen']}" if act['reopen'] != 'n/a' else '')
                    + (f"; why: {act['why']}" if act['why'] else '')
                    + (f"; draft {drafted}" if drafted else ''))


def runShadow(conn, days, started):
    global INDEX_OK
    try:
        ensureIndex(STATE_DIR)
        INDEX_OK = True
    except Exception as e:
        # Drafts need the index; classification does not, so carry on without drafts.
        logger.exception("Answered-MLQ index refresh failed; no drafts this run")
        sendAlert('index', 'Answered-MLQ index refresh failed', f"{type(e).__name__}: {e}")
    return runShadowInner(conn, days, started)


def runShadowInner(conn, days, started):
    """Steps 0-2: record each new message with its facts, classify it (Stage 1),
    log what would be done, then see what the old script did with earlier ones."""
    global ACCOUNTS_OK
    refreshAccounts(conn)
    accountsOk, accountsWhy = accountsUsable(conn)
    ACCOUNTS_OK = accountsOk
    if not accountsOk:
        logger.error(f"Active-account list not trusted ({accountsWhy}): no reopens this run")
        sendAlert('accounts', 'Active-account list not trusted',
                  f"{accountsWhy}. The run treats every closed ticket as not reopenable "
                  f"until the list refreshes.")
    messages = fetchAll(days)
    new, known = 0, 0
    observedNow = {'created': 0, 'appended': 0, 'none': 0, 'waiting': 0}
    classified = {'done': 0, 'failed': 0, 'retried': 0}
    for rec in messages:
        row = conn.execute("select action, attempts, writeAttempts, oldAction from messages "
                           "where messageId=?", (rec['messageId'],)).fetchone()
        if row:
            known += 1
            # Retry Stage 1 for a message whose classification failed (or that was
            # recorded before Stage 1 existed), up to 3 attempts.
            # At cutover: a message the shadow classified but mlqAutomate.py never acted
            # on (it arrived after the old script's last run) is filed now.
            cutoverGap = not SHADOW and row[0] == 'classified' and row[3] is None
            if (row[0] in ('seen', 'facts', 'error') and (row[1] or 0) < 3) or cutoverGap or \
                    (row[0] == 'write-error' and (row[2] or 0) < WRITE_ATTEMPTS):
                classified['retried'] += 1
                runStage1(conn, rec, messageFacts(conn, rec, accountsOk), classified)
            continue
        new += 1
        conn.execute(
            "insert into messages(messageId, gmailId, gmailThreadId, lists, source, fromAddr,"
            " subject, receivedAt, firstSeen, action) values (?,?,?,?,?,?,?,?,?,?)",
            (rec['messageId'], rec['gmailId'], rec['gmailThreadId'], ','.join(rec['lists']),
             rec['source'], rec['fromAddr'], rec['subject'], rec['receivedAt'], utcNow(),
             'facts'))
        conn.commit()
        facts = messageFacts(conn, rec, accountsOk)
        # Observe right away (on a first or backfill run the old script acted long
        # ago), so later messages in this same run can match through this one.
        if SHADOW:
            observedNow[observeOne(conn, rec['messageId'])] += 1
        logDecision('seen', rec['messageId'],
                    hasMessageId=rec['hasMessageId'], lists=rec['lists'], source=rec['source'],
                    fromAddr=rec['fromAddr'], subject=rec['subject'],
                    receivedAt=rec['receivedAt'], gmailThreadId=rec['gmailThreadId'],
                    inReplyTo=rec['inReplyTo'], references=len(referencedIds(rec)),
                    autoSubmitted=rec['autoSubmitted'], precedence=rec['precedence'],
                    attachments=[a['filename'] for a in rec['attachments']],
                    bodyChars=len(rec['bodyText']), **facts)
        logger.info(f"New: [{rec['source']}] {rec['fromAddr']} | {rec['subject'][:60]} "
                    f"-> {facts['factsDecide']}"
                    + (f" #{facts['ticketId']} via {facts['matchedBy']}" if facts['ticketId']
                       else ''))
        runStage1(conn, rec, facts, classified)
    observed = observeOldScript(conn) if SHADOW else {k: 0 for k in observedNow}
    for k, v in observedNow.items():
        observed[k] += v
    observed['waiting'] = conn.execute("select count(*) from messages "
                                       "where oldAction is null").fetchone()[0]
    summary = {'fetched': len(messages), 'new': new, 'known': known,
               'classified': classified, 'observed': observed, 'accountsOk': accountsOk, 'days': days,
               'modelCalls': _callCount, 'seconds': round(time.time() - started, 1)}
    logDecision('run', None, **summary)
    logger.info(f"Fetched {len(messages)} message(s) from the last {days} day(s): "
                f"{new} new, {known} already recorded; Stage 1 {classified}; "
                f"old script observed {observed}")


def readDecisions(kinds):
    """Every decision-log record of the given kinds, oldest first."""
    records = []
    for name in sorted(os.listdir(DECISION_LOG_DIR)):
        if re.match(r'decisions-\d{4}-\d{2}\.jsonl$', name):
            with open(os.path.join(DECISION_LOG_DIR, name)) as f:
                for line in f:
                    rec = json.loads(line)
                    if rec.get('kind') in kinds:
                        records.append(rec)
    return records


def report(conn, sinceDays):
    """Compare the shadow's facts with what mlqAutomate.py did, message by message.

    Prints each disagreement, grouped by kind, then the counts. Agreements are
    counted, not listed."""
    cutoff = (datetime.now(timezone.utc) - timedelta(days=sinceDays)).isoformat()
    seen = {r['messageId']: r for r in readDecisions({'seen'}) if r['receivedAt'] >= cutoff}
    groups = {}
    for messageId, s in seen.items():
        row = conn.execute("select oldAction, oldTicketId, oldReopened from messages "
                           "where messageId=?", (messageId,)).fetchone()
        if not row or row[0] is None:
            groups.setdefault('old script not observed yet', []).append((s, row))
            continue
        oldAction, oldTicket, oldReopened = row
        ours = s.get('ticketId')
        if oldReopened and 'staff reply' in s.get('factsDecide', ''):
            key = 'old reopened on a staff reply; new would not'
        elif oldAction == 'created' and ours:
            key = 'old made a new ticket; new matches an existing one exactly'
        elif oldAction == 'appended' and ours and ours != oldTicket:
            key = 'old appended to a different ticket'
        elif oldAction == 'none' and not s.get('staffLogin'):
            key = 'old filed nothing from an outside sender (held, dropped or skipped)'
        elif oldAction == 'none':
            key = 'old filed nothing from a staff sender (often pasted into the ticket by hand)'
        else:
            key = 'agree' if (ours == oldTicket or not ours) else 'other'
        groups.setdefault(key, []).append((s, row))
    for key in sorted(groups, key=lambda k: (k == 'agree', k)):
        items = groups[key]
        print(f"\n{key}: {len(items)}")
        if key == 'agree':
            continue
        for s, row in items:
            old = (f"old={row[0]} #{row[1]}" if row and row[1]
                   else f"old={row[0] if row else '?'}")
            new = (f"new #{s['ticketId']} via {s['matchedBy']}" if s.get('ticketId')
                   else 'new: no exact match')
            print(f"  {s['receivedAt'][:16]} {s['fromAddr'][:30]:30} {s['subject'][:45]:45} "
                  f"{old}; {new}")
    reportStage1(conn, seen)
    reportMatches(conn, seen)
    reportReopens(conn, seen)
    reportDrafts(conn, seen)
    print(f"\n{len(seen)} message(s) in the last {sinceDays} day(s)")


def reportMatches(conn, seen):
    """Stage 2 subject matches next to the ticket the old script used, and Stage 4 hub plans."""
    matches = [r for r in readDecisions({'match'}) if r['messageId'] in seen]
    print(f"\nSubject matching (Stage 2): {len(matches)} message(s) without an exact match")
    for m in matches:
        row = conn.execute("select oldAction, oldTicketId from messages where messageId=?",
                           (m['messageId'],)).fetchone()
        old = (f"old {row[0]} #{row[1]}" if row and row[1] else
               f"old {row[0]}" if row and row[0] else 'old not observed yet')
        new = 'ERROR' if 'error' in m else (f"#{m['ticketId']}" if m.get('ticketId') else 'new ticket')
        same = row and row[1] and m.get('ticketId') == row[1]
        flag = '' if same or (not m.get('ticketId') and row and row[0] == 'created') else '  <--'
        print(f"  {m.get('fromAddr', '')[:28]:28} {m.get('subject', '')[:42]:42} new={new:10} "
              f"{old}{flag}")
        if m.get('reason') and m.get('candidates'):
            print(f"      candidates {m['candidates']}: {m['reason'][:130]}")
    hubs = [r for r in readDecisions({'hub'}) if r['messageId'] in seen]
    print(f"\nHub routing (Stage 4): {len(hubs)} hub-related message(s)")
    for h in hubs:
        print(f"  {h.get('subject', '')[:50]:50} "
              + (f"Hub #{h['hubTicketId']}: {'; '.join(h.get('plan', []))}" if h.get('hubTicketId')
                 else 'no Hub ticket' if 'error' not in h else 'ERROR'))
        if h.get('reason'):
            print(f"      {h['reason'][:140]}")


def reportReopens(conn, seen):
    """Every Stage 3 decision next to what the old script did with the same message."""
    decisions = [r for r in readDecisions({'reopen'}) if r['messageId'] in seen]
    print(f"\nReopen decisions on closed tickets (Stage 3): {len(decisions)}")
    for d in decisions:
        row = conn.execute("select oldReopened from messages where messageId=?",
                           (d['messageId'],)).fetchone()
        old = ('old reopened' if row and row[0] else
               'old did not reopen' if row and row[0] is not None else 'old not observed yet')
        new = 'ERROR' if 'error' in d else ('reopen' if d['reopen'] else 'no reopen')
        flag = '' if ('reopened' in old) == (new == 'reopen') or 'not observed' in old else '  <--'
        print(f"  #{d.get('ticketId')} {d.get('fromAddr', '')[:28]:28} new={new:9} {old}{flag}")
        if 'reason' in d:
            print(f"      {d['conversationState']} ({d['confidence']:.2f}): {d['reason'][:140]}")


def reportDrafts(conn, seen):
    """Drafts written, and how much of each one staff reused in the reply they sent,
    scored exactly like the 2026 baseline (5.6% any use)."""
    drafts = [r for r in readDecisions({'draft'}) if r['messageId'] in seen]
    actions = {r['messageId']: r for r in readDecisions({'action'}) if r['messageId'] in seen}
    written = [d for d in drafts if 'post' in d]
    print(f"\nDrafts: {len(written)} written, "
          f"{sum(1 for d in drafts if 'skipped' in d)} skipped (staff already answered), "
          f"{sum(1 for d in drafts if 'error' in d)} failed, "
          f"{sum(1 for d in written if d['problems'])} still failing a check after the retry")
    scored = {'USED': 0, 'PARTLY USED': 0, 'NOT USED': 0}
    waiting = 0
    for d in written:
        row = conn.execute("select oldTicketId from messages where messageId=?",
                           (d['messageId'],)).fetchone()
        reply = firstStaffReply(row[0]) if row and row[0] else None
        if not reply:
            waiting += 1
            continue
        draftText = d['post'].split('\n\n', 1)[-1].split('\n\n*Related RM tickets:*')[0]
        question = actions.get(d['messageId'], {}).get('render', '')
        cls = draftUsage(draftText, reply, question)
        scored[cls] += 1
        print(f"  {cls:11} #{row[0]} {d['subject'][:55]} (confidence {d['confidence']:.2f})")
    n = sum(scored.values())
    if n:
        print(f"  any use: {(scored['USED'] + scored['PARTLY USED']) / n:.0%} of {n} with a "
              f"staff reply (baseline 5.6%); {waiting} still waiting for a staff reply")
    elif written:
        print(f"  {waiting} draft(s) waiting for a staff reply to score against")


def reportStage1(conn, seen):
    """Stage 1 part of the report: what each message was judged to be, every
    message held or dropped and why, and where filing differs from the old script."""
    dispositions = {r['messageId']: r for r in readDecisions({'disposition'})
                    if r['messageId'] in seen and 'disposition' in r}
    actions = {r['messageId']: r for r in readDecisions({'action'}) if r['messageId'] in seen}
    # Only messages whose Stage 1 never succeeded: a failure later retried successfully
    # (a usage limit, say) is not an outstanding error.
    errors = [r for r in readDecisions({'disposition'}) if r['messageId'] in seen and 'error' in r
              and r['messageId'] not in dispositions]
    counts = {}
    for d in dispositions.values():
        counts[d['disposition']] = counts.get(d['disposition'], 0) + 1
    print("\nStage 1 dispositions: " + ', '.join(f"{k} {v}" for k, v in
                                              sorted(counts.items(), key=lambda kv: -kv[1])))
    cached = sum(1 for d in dispositions.values() if d.get('cached'))
    unsure = sum(1 for a in actions.values() if a.get('unsure'))
    print(f"  from the sender cache: {cached}; below the 'not sure' cut-off: {unsure}; "
          f"Stage 1 errors: {len(errors)}")
    held = [(dispositions.get(m), a) for m, a in actions.items()
            if a.get('moderation') == 'hold' or a.get('ticket', '').startswith('none')]
    print(f"\nHeld or not filed by the new script: {len(held)}")
    for d, a in held:
        print(f"  {a['fromAddr'][:30]:30} {a['subject'][:45]:45} {a['disposition']}"
              f" ({d['confidence']:.2f}{', cached' if d.get('cached') else ''}): "
              f"{d['reason'][:110]}")
    lost = [d for d in dispositions.values() if not d.get('wordsPreserved', True)]
    print(f"\nCleaned body failed the word check (raw text would be posted): {len(lost)}")
    for d in lost:
        print(f"  {d['fromAddr'][:30]:30} {d['subject'][:45]:45} "
              f"{d['wordsUnmatched']}/{d['wordsTotal']} words not found")
    diff = []
    for m, a in actions.items():
        row = conn.execute("select oldAction from messages where messageId=?", (m,)).fetchone()
        oldFiled = row and row[0] in ('created', 'appended')
        newFiles = not (a.get('moderation') == 'hold' or a.get('ticket', '').startswith('none'))
        if row and row[0] and oldFiled != newFiles:
            diff.append((a, row[0]))
    print(f"\nFiled by one script and not the other: {len(diff)}")
    for a, old in diff:
        print(f"  {a['fromAddr'][:30]:30} {a['subject'][:45]:45} old={old}; "
              f"new={a['disposition']}, {a['ticket']}")


def main():
    global SHADOW, DECISION_LOG_DIR, STATE_CONN, ALERT_TO_OVERRIDE, STATE_DIR
    parser = argparse.ArgumentParser(description='File mailing-list email into Redmine MLQ '
                                     'tickets (RM #38234). Step 0: shadow skeleton only.')
    parser.add_argument('--shadow', action='store_true',
                        help='Read-only: never write to Gmail or Redmine; compare with mlqAutomate.py')
    parser.add_argument('--live', action='store_true',
                        help='Approve, hold and file for real (replaces mlqAutomate.py at cutover)')
    parser.add_argument('--state-dir', default=os.path.expanduser('~/mlqTriage'),
                        help='Where the state file, logs and lock live (default ~/mlqTriage)')
    parser.add_argument('--days', type=int, default=CONFIG['FETCH_DAYS'],
                        help=f"How many days of mail to read (default {CONFIG['FETCH_DAYS']})")
    parser.add_argument('--alert-to', help='In shadow mode, send alerts here instead of only '
                        'logging them')
    parser.add_argument('--selftest', action='store_true',
                        help='Check Gmail, Calendar, Redmine and claude -p, then exit')
    parser.add_argument('--call-cap', type=int, default=CONFIG['CALL_CAP'],
                        help=f"Model calls allowed per run (default {CONFIG['CALL_CAP']}); "
                        "raise it for a first or backfill run")
    parser.add_argument('--report', type=int, metavar='DAYS',
                        help='Compare the shadow with mlqAutomate.py over the last DAYS days, '
                        'then exit (reads the state file and decision log only)')
    parser.add_argument('--playbooks', help='Directory of approved playbooks (<category>.md); '
                        'without it drafts use past tickets only')
    parser.add_argument('--build-index', action='store_true',
                        help='Rebuild the index of answered MLQs (for drafts), then exit')
    parser.add_argument('--verbose', action='store_true')
    args = parser.parse_args()

    if args.shadow == args.live:
        sys.exit("Give exactly one of --shadow or --live.")
    SHADOW = args.shadow
    ALERT_TO_OVERRIDE = args.alert_to
    CONFIG['CALL_CAP'] = args.call_cap
    CONFIG['PLAYBOOK_DIR'] = args.playbooks

    makeStateDir(args.state_dir)
    setupLogging(args.state_dir, args.verbose)
    loadHgConf()
    socket.setdefaulttimeout(CONFIG['GOOGLE_TIMEOUT'])
    DECISION_LOG_DIR = args.state_dir
    STATE_DIR = args.state_dir

    if args.report:
        report(openState(args.state_dir), args.report)
        return 0
    lockFile = takeLock(args.state_dir)
    if lockFile is None:
        return 0
    STATE_CONN = openState(args.state_dir)
    started = time.time()
    logger.info(f"=== mlqTriage {'shadow' if SHADOW else 'LIVE'} run, "
                f"{datetime.now(PST):%Y-%m-%d %H:%M %Z} ===")
    try:
        if args.selftest:
            return 0 if selftest() else 1
        if args.build_index:
            buildIndex(args.state_dir)
            return 0
        pruneDecisionLogs()
        runShadow(STATE_CONN, args.days, started)
    except AskAuthError as e:
        sendAlert('claude-auth', 'Claude login failed',
                  f"{e}\n\nMost likely the qateam claude.oauthToken in ~/.hg.conf has expired. "
                  f"Mint a new one with 'claude setup-token', then check with: claude -p 'say ok'")
        return 1
    except AskLimitError as e:
        sendAlert('claude-limit', 'Claude usage limit reached',
                  f"{e}\n\nThe run stopped; messages are kept and processed on a later run "
                  f"once the limit resets. If this repeats, the qateam subscription is too "
                  f"small for the MLQ and code-review crons together.")
        return 1
    except CallCapReached as e:
        sendAlert('call-cap', 'Run stopped at the call cap', str(e))
        return 1
    except Exception as e:
        logger.exception("Run failed")
        sendAlert('run-failed', 'Run failed', f"{type(e).__name__}: {e}")
        return 1
    finally:
        logger.info(f"=== done in {time.time() - started:.1f}s, {_callCount} model call(s) ===")
        lockFile.close()
    return 0


if __name__ == '__main__':
    sys.exit(main())
