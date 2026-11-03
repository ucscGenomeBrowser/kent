#!/usr/bin/env python3
"""
mlqTriage: the replacement for mlqAutomate.py (RM #38234).

Files mailing-list email (genome@, genome-mirror@, genome-www@) into Redmine MLQ
tickets. The design is in the RM #38234 plan; this file grows one build step at a
time (section 9 of the plan).

Step 0, this version: the skeleton. It reads the last few days of list mail and
the Google Groups pending queue, records every message it has not seen before in
its own state file and decision log, and does nothing else. Only --shadow mode
exists: it never writes to Gmail or Redmine. Live mode arrives at cutover.

Run as qateam (Gmail token, Redmine key and Claude token come from qateam's files):
    mlqTriage.py --shadow                 # one shadow pass
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
    observedAt text
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
                          ('observedAt', 'text')):
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
                          'size': body.get('size', 0),
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
                   "i.subject, cv.value email from issues i "
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
             'statusId': r['status_id'], 'opener': opener, 'subject': r['subject']}
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
                 'category', 'hubName', 'hubUrl', 'assemblies', 'urls', 'attachmentRoles'],
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
        cached.update({'sensitivePersonalInfo': False, 'canonicalSubject': '',
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

    Thread matching by subject (Stage 2), the reopen judgment (Stage 3) and hub
    routing (Stage 4) are later steps; where they would decide, the action says so."""
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
    if d in ('reply', 'digest_reply', 'acknowledgement'):
        act['ticket'] = 'Stage 2 subject match (step 6), else new ticket'
    else:
        act['ticket'] = 'new ticket (Stage 2 subject check in step 6)'
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
    except (AskAuthError, CallCapReached):
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
    act = decideAction(rec, verdict, facts)
    logDecision('disposition', rec['messageId'], fromAddr=rec['fromAddr'],
                subject=rec['subject'], lists=rec['lists'], source=rec['source'],
                disposition=verdict['disposition'], reason=verdict['dispositionReason'],
                confidence=verdict['confidence'],
                sensitivePersonalInfo=verdict['sensitivePersonalInfo'],
                category=verdict['category'], canonicalSubject=verdict['canonicalSubject'],
                digestTopic=verdict['digestTopic'], summaryLine=verdict['summaryLine'],
                hubName=verdict['hubName'], hubUrl=verdict['hubUrl'],
                assemblies=verdict['assemblies'], urls=verdict['urls'],
                attachmentRoles=verdict['attachmentRoles'],
                wordsPreserved=ok, wordsUnmatched=unmatched, wordsTotal=total, **meta)
    logDecision('action', rec['messageId'], fromAddr=rec['fromAddr'], subject=rec['subject'],
                **act)
    return act


# ---------------------------------------------------------------------------
# One run

def messageFacts(conn, rec, accountsOk):
    """Step 1: the facts about one new message, and what they alone decide."""
    login = staffLogin(conn, rec['fromAddr'])
    ticket, matchedBy, notes = resolveTicket(conn, rec)
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
    conn.execute("update messages set attempts=coalesce(attempts, 0)+1 where messageId=?",
                 (rec['messageId'],))
    act = stage1(conn, rec, facts)
    conn.execute("update messages set action=? where messageId=?",
                 ('classified' if act else 'error', rec['messageId']))
    conn.commit()
    counts['done' if act else 'failed'] += 1
    if act:
        logger.info(f"  Stage 1: {act['disposition']}"
                    + (' (unsure)' if act['unsure'] else '')
                    + f"; moderation {act['moderation']}; ticket {act['ticket']}"
                    + (f"; reopen {act['reopen']}" if act['reopen'] != 'n/a' else '')
                    + (f"; why: {act['why']}" if act['why'] else ''))


def runShadow(conn, days, started):
    """Steps 0-2: record each new message with its facts, classify it (Stage 1),
    log what would be done, then see what the old script did with earlier ones."""
    refreshAccounts(conn)
    accountsOk, accountsWhy = accountsUsable(conn)
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
        row = conn.execute("select action, attempts from messages where messageId=?",
                           (rec['messageId'],)).fetchone()
        if row:
            known += 1
            # Retry Stage 1 for a message whose classification failed (or that was
            # recorded before Stage 1 existed), up to 3 attempts.
            if row[0] in ('seen', 'facts', 'error') and (row[1] or 0) < 3:
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
    observed = observeOldScript(conn)
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
    print(f"\n{len(seen)} message(s) in the last {sinceDays} day(s)")


def reportStage1(conn, seen):
    """Stage 1 part of the report: what each message was judged to be, every
    message held or dropped and why, and where filing differs from the old script."""
    dispositions = {r['messageId']: r for r in readDecisions({'disposition'})
                    if r['messageId'] in seen and 'disposition' in r}
    actions = {r['messageId']: r for r in readDecisions({'action'}) if r['messageId'] in seen}
    errors = [r for r in readDecisions({'disposition'}) if r['messageId'] in seen and 'error' in r]
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
    global SHADOW, DECISION_LOG_DIR, STATE_CONN, ALERT_TO_OVERRIDE
    parser = argparse.ArgumentParser(description='File mailing-list email into Redmine MLQ '
                                     'tickets (RM #38234). Step 0: shadow skeleton only.')
    parser.add_argument('--shadow', action='store_true',
                        help='Read-only: never write to Gmail or Redmine (required for now)')
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
    parser.add_argument('--verbose', action='store_true')
    args = parser.parse_args()

    if not args.shadow:
        sys.exit("Only --shadow mode exists in this build step; live mode comes at cutover.")
    SHADOW = True
    ALERT_TO_OVERRIDE = args.alert_to
    CONFIG['CALL_CAP'] = args.call_cap

    makeStateDir(args.state_dir)
    setupLogging(args.state_dir, args.verbose)
    loadHgConf()
    socket.setdefaulttimeout(CONFIG['GOOGLE_TIMEOUT'])
    DECISION_LOG_DIR = args.state_dir

    if args.report:
        report(openState(args.state_dir), args.report)
        return 0
    lockFile = takeLock(args.state_dir)
    if lockFile is None:
        return 0
    STATE_CONN = openState(args.state_dir)
    started = time.time()
    logger.info(f"=== mlqTriage shadow run, {datetime.now(PST):%Y-%m-%d %H:%M %Z} ===")
    try:
        if args.selftest:
            return 0 if selftest() else 1
        pruneDecisionLogs()
        runShadow(STATE_CONN, args.days, started)
    except AskAuthError as e:
        sendAlert('claude-auth', 'Claude login failed',
                  f"{e}\n\nMost likely the qateam claude.oauthToken in ~/.hg.conf has expired. "
                  f"Mint a new one with 'claude setup-token', then check with: claude -p 'say ok'")
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
