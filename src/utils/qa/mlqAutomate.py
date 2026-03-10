#!/usr/bin/env python3
"""
MLQ Automation Script
Monitors Gmail for mailing list emails, moderates pending messages,
and creates/updates Redmine tickets.
"""

import os
import sys
import argparse
import base64
import re
import logging
import html as html_module
import time
from datetime import datetime, timedelta
from difflib import SequenceMatcher
import email
from email import policy
from email.utils import parseaddr
from email.mime.text import MIMEText
from functools import wraps
import pytz
import requests
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from google.auth.transport.requests import Request
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
import anthropic

# Configuration
CONFIG = {
    'REDMINE_URL': 'https://redmine.gi.ucsc.edu',
    'REDMINE_API_KEY': '',
    'REDMINE_PROJECT': 'maillists',
    'CALENDAR_ID': 'ucsc.edu_anbl4254jlssgo3gc2l5c8un5c@group.calendar.google.com',
    'CLAUDE_API_KEY': '',

    # Mailing lists
    'MODERATED_LISTS': ['genome@soe.ucsc.edu', 'genome-mirror@soe.ucsc.edu'],
    'UNMODERATED_LISTS': ['genome-www@soe.ucsc.edu'],

    # Name mapping from calendar to Redmine
    'NAME_MAPPING': {
        'Jairo': 'Jairo Navarro',
        'Lou': 'Lou Nassar',
        'Gerardo': 'Gerardo Perez',
        'Gera': 'Gerardo Perez',
        'Clay': 'Clay Fischer',
        'Matt': 'Matt Speir',
    },

    # Redmine User IDs
    'USER_IDS': {
        'Jairo Navarro': 163,
        'Lou Nassar': 171,
        'Gerardo Perez': 179,
        'Clay Fischer': 161,
        'Matt Speir': 150,
    },

    # Redmine field IDs
    'TRACKER_ID': 7,        # MLQ
    'PRIORITY_ID': 12,      # Unprioritized
    'STATUS_ID': 1,         # New
    'CUSTOM_FIELDS': {
        'MLQ Category - primary': 28,
        'Email': 40,
        'MLM': 9,
    },
}

SCOPES = [
    'https://www.googleapis.com/auth/gmail.readonly',
    'https://www.googleapis.com/auth/gmail.send',
    'https://www.googleapis.com/auth/gmail.modify',
    'https://www.googleapis.com/auth/calendar.readonly',
]

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

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PST = pytz.timezone('America/Los_Angeles')
DRY_RUN = False

# Setup logging
LOG_FILE = os.environ.get('MLQ_LOG_FILE', os.path.join(SCRIPT_DIR, 'mlq_automate.log'))
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.FileHandler(LOG_FILE),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

# Suppress httpx INFO logs (Anthropic client HTTP request logging)
logging.getLogger('httpx').setLevel(logging.WARNING)

# Config file path
MLQ_CONF_PATH = os.path.expanduser('~/.hg.conf')


def load_config_file():
    """
    Load configuration from ~/.hg.conf file.
    Uses the standard UCSC kent configuration file.
    Requires 600 permissions for security.
    """
    if not os.path.exists(MLQ_CONF_PATH):
        logger.error(f"Configuration file not found: {MLQ_CONF_PATH}")
        logger.error("Please add the following keys to ~/.hg.conf:")
        logger.error("  redmine.apiKey=YOUR_REDMINE_API_KEY")
        logger.error("  claude.apiKey=YOUR_CLAUDE_API_KEY")
        sys.exit(1)

    # Check file permissions (must be 600 for security)
    file_stat = os.stat(MLQ_CONF_PATH)
    file_mode = file_stat.st_mode & 0o777
    if file_mode != 0o600:
        logger.error(f"Configuration file {MLQ_CONF_PATH} has insecure permissions: {oct(file_mode)}")
        logger.error("For security, this file must have 600 permissions.")
        logger.error(f"Run: chmod 600 {MLQ_CONF_PATH}")
        sys.exit(1)

    # Parse key=value pairs
    config_values = {}
    with open(MLQ_CONF_PATH, 'r') as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            # Skip empty lines and comments
            if not line or line.startswith('#'):
                continue
            if '=' not in line:
                logger.warning(f"Skipping invalid line {line_num} in {MLQ_CONF_PATH}: {line}")
                continue
            key, value = line.split('=', 1)
            config_values[key.strip()] = value.strip()

    # Map config file keys to CONFIG dict
    key_mapping = {
        'redmine.apiKey': 'REDMINE_API_KEY',
        'claude.apiKey': 'CLAUDE_API_KEY',
    }

    for conf_key, config_key in key_mapping.items():
        if conf_key in config_values:
            CONFIG[config_key] = config_values[conf_key]

    # Validate required keys
    if not CONFIG['REDMINE_API_KEY']:
        logger.error(f"Missing redmine.apiKey in {MLQ_CONF_PATH}")
        sys.exit(1)
    if not CONFIG['CLAUDE_API_KEY']:
        logger.error(f"Missing claude.apiKey in {MLQ_CONF_PATH}")
        sys.exit(1)

    logger.info(f"Loaded configuration from {MLQ_CONF_PATH}")


def retry(max_attempts=3, delay=2, backoff=2, exceptions=(Exception,)):
    """Retry decorator with exponential backoff."""
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            attempts = 0
            current_delay = delay
            while attempts < max_attempts:
                try:
                    return func(*args, **kwargs)
                except exceptions as e:
                    attempts += 1
                    if attempts == max_attempts:
                        logger.error(f"{func.__name__} failed after {max_attempts} attempts: {e}")
                        raise
                    logger.warning(f"{func.__name__} attempt {attempts} failed: {e}. Retrying in {current_delay}s...")
                    time.sleep(current_delay)
                    current_delay *= backoff
        return wrapper
    return decorator


def get_google_credentials():
    """Get or refresh Google API credentials."""
    creds = None
    token_path = os.path.expanduser('~/.gmail_token.json')
    creds_path = os.path.expanduser('~/.gmail_credentials.json')

    if os.path.exists(token_path):
        creds = Credentials.from_authorized_user_file(token_path, SCOPES)

    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
        else:
            flow = InstalledAppFlow.from_client_secrets_file(creds_path, SCOPES)
            creds = flow.run_local_server(port=8080)
        with open(token_path, 'w') as f:
            f.write(creds.to_json())

    return creds


def get_current_mlm():
    """Get the current MLM based on PST time rules."""
    creds = get_google_credentials()
    service = build('calendar', 'v3', credentials=creds, cache_discovery=False)

    now = datetime.now(PST)
    target_date = now

    # After 5pm, use next day's MLM
    if now.hour >= 17:
        target_date += timedelta(days=1)

    # Weekend handling: Fri 5pm+ through Mon 5pm uses Monday's MLM
    weekday = target_date.weekday()
    if weekday == 5:  # Saturday
        target_date += timedelta(days=2)
    elif weekday == 6:  # Sunday
        target_date += timedelta(days=1)

    # Tuesday is always Matt
    if target_date.weekday() == 1:  # Tuesday
        return 'Matt Speir'

    start = target_date.replace(hour=0, minute=0, second=0, microsecond=0)
    end = target_date.replace(hour=23, minute=59, second=59, microsecond=0)

    events = service.events().list(
        calendarId=CONFIG['CALENDAR_ID'],
        timeMin=start.isoformat(),
        timeMax=end.isoformat(),
        singleEvents=True
    ).execute().get('items', [])

    for event in events:
        title = event.get('summary', '')
        match = re.search(r'MLM(?:\s+Rotating)?:\s*(\w+)', title, re.IGNORECASE)
        if match:
            cal_name = match.group(1)
            return CONFIG['NAME_MAPPING'].get(cal_name, cal_name)

    logger.warning(f"No MLM found for {target_date.date()}")
    return None


@retry(max_attempts=3, delay=2, exceptions=(anthropic.APIError,))
def analyze_email_with_claude(subject, body, sender):
    """
    Use Claude to analyze an email in a single call.
    Returns dict with: is_spam, category, draft_response
    """
    client = anthropic.Anthropic(api_key=CONFIG['CLAUDE_API_KEY'])
    categories_list = ", ".join(MLQ_CATEGORIES)

    prompt = f"""Analyze this email for the UCSC Genome Browser support team.

From: {sender}
Subject: {subject}
Body:
{body[:3000]}

Provide your analysis in this exact format:

SPAM: [YES or NO]
CATEGORY: [Pick one from: {categories_list}]
DRAFT_RESPONSE: [If not spam, write a helpful, professional response under 200 words. If spam, write "N/A"]

Important:
- Mark as SPAM if it is:
  - Conference/journal solicitations asking for paper submissions
  - Promotions for workshops, courses, training programs, or webinars
  - Marketing or promotional emails advertising services or products
  - Mass-sent announcements unrelated to genome browser support
  - Contains sensitive personal medical information (specific names with genetic test results, medical conditions, family medical history, or personal health details) - these are privacy concerns
- Mark as NOT SPAM if it is a genuine question about using the UCSC Genome Browser (general genetics questions without personal identifying info are OK)
- For CATEGORY, pick the most specific match. Use "Other" if unsure.
- For DRAFT_RESPONSE, be helpful and concise. Ask clarifying questions if needed. Point to relevant documentation when appropriate."""

    response = client.messages.create(
        model="claude-sonnet-4-20250514",
        max_tokens=800,
        messages=[{"role": "user", "content": prompt}]
    )

    result_text = response.content[0].text.strip()

    # Parse the response
    is_spam = False
    category = "Other"
    draft_response = None

    for line in result_text.split('\n'):
        line = line.strip()
        if line.upper().startswith('SPAM:'):
            is_spam = 'YES' in line.upper()
        elif line.upper().startswith('CATEGORY:'):
            cat = line.split(':', 1)[1].strip()
            # Validate category
            if cat in MLQ_CATEGORIES:
                category = cat
            else:
                for c in MLQ_CATEGORIES:
                    if c.lower() == cat.lower():
                        category = c
                        break
        elif line.upper().startswith('DRAFT_RESPONSE:'):
            draft_response = line.split(':', 1)[1].strip()
            # Capture multi-line response
            idx = result_text.find('DRAFT_RESPONSE:')
            if idx != -1:
                draft_response = result_text[idx + len('DRAFT_RESPONSE:'):].strip()
                if draft_response.upper() == 'N/A':
                    draft_response = None

    return {
        'is_spam': is_spam,
        'category': category,
        'draft_response': draft_response
    }


@retry(max_attempts=3, delay=2, exceptions=(anthropic.APIError,))
def batch_check_spam_with_claude(messages):
    """
    Use Claude to determine spam status for multiple emails in one call.
    Returns dict mapping message index to True (spam) or False (not spam).
    """
    if not messages:
        return {}

    client = anthropic.Anthropic(api_key=CONFIG['CLAUDE_API_KEY'])

    # Build the prompt with all messages
    emails_text = ""
    for i, msg in enumerate(messages, 1):
        emails_text += f"""
--- EMAIL {i} ---
From: {msg['original_from']}
Subject: {msg['original_subject']}
Body:
{msg['original_body'][:1500]}
"""

    prompt = f"""Analyze these emails and determine if each is spam. This is for a genome browser technical support mailing list.

{emails_text}

Mark as SPAM if it is:
- Conference/journal solicitations asking for paper submissions
- Promotions for workshops, courses, training programs, or webinars
- Marketing or promotional emails advertising services or products
- Phishing or scam attempts
- Mass-sent unsolicited emails unrelated to genome browser support
- Announcements about events, courses, or programs (not questions about the browser)
- Contains sensitive personal medical information (specific names with genetic test results, medical conditions, family medical history, or personal health details) - these are privacy concerns

Mark as NOT SPAM if it is:
- A genuine question about the UCSC Genome Browser
- A technical support request
- A follow-up to an existing conversation
- Someone asking how to use browser features for their research
- General questions about genetic data without personal identifying information

Reply with one line per email in this exact format:
EMAIL 1: SPAM or NOT SPAM
EMAIL 2: SPAM or NOT SPAM
(etc.)"""

    response = client.messages.create(
        model="claude-sonnet-4-20250514",
        max_tokens=100,
        messages=[{"role": "user", "content": prompt}]
    )

    result_text = response.content[0].text.strip().upper()
    results = {}

    for line in result_text.split('\n'):
        line = line.strip()
        match = re.match(r'EMAIL\s*(\d+):\s*(SPAM|NOT SPAM)', line)
        if match:
            idx = int(match.group(1)) - 1  # Convert to 0-based index
            is_spam = match.group(2) == 'SPAM'
            results[idx] = is_spam

    # Default to not spam for any missing results
    for i in range(len(messages)):
        if i not in results:
            logger.warning(f"No spam result for message {i+1}, defaulting to not spam")
            results[i] = False

    return results


def parse_raw_email_headers(raw_email):
    """Parse Subject, From, and body from raw email text.

    Properly handles multipart MIME emails by extracting just the text/plain part.
    """
    # Parse using Python's email module for proper MIME handling
    msg = email.message_from_string(raw_email, policy=policy.default)

    # Get headers
    subject = msg.get('subject', '')
    # Priority: x-original-sender > reply-to > from
    sender = (
        msg.get('x-original-sender') or
        msg.get('reply-to') or
        msg.get('from', '')
    )

    # Extract body - prefer text/plain, fall back to text/html converted to text
    body = ''
    html_body = ''

    if msg.is_multipart():
        for part in msg.walk():
            content_type = part.get_content_type()
            if content_type == 'text/plain' and not body:
                try:
                    body = part.get_content()
                except Exception:
                    # Fallback: try to decode manually
                    payload = part.get_payload(decode=True)
                    if payload:
                        charset = part.get_content_charset() or 'utf-8'
                        body = payload.decode(charset, errors='ignore')
            elif content_type == 'text/html' and not html_body:
                try:
                    html_body = part.get_content()
                except Exception:
                    payload = part.get_payload(decode=True)
                    if payload:
                        charset = part.get_content_charset() or 'utf-8'
                        html_body = payload.decode(charset, errors='ignore')
    else:
        # Not multipart - check content type
        content_type = msg.get_content_type()
        try:
            content = msg.get_content()
        except Exception:
            payload = msg.get_payload(decode=True)
            if payload:
                charset = msg.get_content_charset() or 'utf-8'
                content = payload.decode(charset, errors='ignore')
            else:
                content = msg.get_payload()

        if content_type == 'text/plain':
            body = content
        elif content_type == 'text/html':
            html_body = content

    # If no text/plain, convert HTML to text
    if not body and html_body:
        body = html_to_text(html_body)

    return subject, sender, body


@retry(max_attempts=3, delay=2, exceptions=(HttpError,))
def get_pending_moderation_emails(group_name):
    """Get pending moderation notification emails for a group."""
    creds = get_google_credentials()
    service = build('gmail', 'v1', credentials=creds, cache_discovery=False)

    # Search for pending moderation emails for this group (exclude trash)
    query = f'subject:"{group_name} - soe.ucsc.edu admins: Message Pending" -in:trash'
    results = service.users().messages().list(userId='me', q=query, maxResults=50).execute()

    pending = []
    for msg_ref in results.get('messages', []):
        msg = service.users().messages().get(userId='me', id=msg_ref['id'], format='full').execute()
        headers = {h['name']: h['value'] for h in msg['payload']['headers']}

        # Extract the approval address from the From header
        from_addr = headers.get('From', '')
        approve_addr = None
        if '+msgappr@' in from_addr:
            match = re.search(r'<([^>]+\+msgappr@[^>]+)>', from_addr)
            if match:
                approve_addr = match.group(1)

        # Extract the attached original message
        original_subject = ''
        original_from = ''
        original_body = ''
        original_attachments = []  # Will store attachment info for later extraction

        for part in msg['payload'].get('parts', []):
            if part['mimeType'] == 'message/rfc822':
                # Check if original email is in an attachment (needs separate fetch)
                if 'attachmentId' in part.get('body', {}):
                    att_id = part['body']['attachmentId']
                    att = service.users().messages().attachments().get(
                        userId='me', messageId=msg_ref['id'], id=att_id
                    ).execute()
                    raw_email = base64.urlsafe_b64decode(att['data']).decode('utf-8', errors='ignore')
                    original_subject, original_from, original_body = parse_raw_email_headers(raw_email)
                    # Note: Attachments in raw RFC822 would need MIME parsing - skip for now
                else:
                    # Fallback: nested parts format (Gmail pre-parsed the RFC822)
                    # Helper to recursively find content of specific MIME type
                    def find_content_recursive(p, mime_type):
                        if p.get('mimeType') == mime_type and p.get('body', {}).get('data'):
                            return base64.urlsafe_b64decode(p['body']['data']).decode('utf-8', errors='ignore')
                        for sp in p.get('parts', []):
                            result = find_content_recursive(sp, mime_type)
                            if result:
                                return result
                        return ''

                    # Helper to recursively find email headers (Subject/From) in nested MIME structure
                    def find_headers_recursive(p):
                        if 'headers' in p:
                            headers = {h['name']: h['value'] for h in p['headers']}
                            # Only return if we found actual email headers (not just Content-Type)
                            if 'Subject' in headers or 'From' in headers:
                                return headers
                        for sp in p.get('parts', []):
                            result = find_headers_recursive(sp)
                            if result:
                                return result
                        return {}

                    nested_headers = find_headers_recursive(part)
                    if nested_headers:
                        original_subject = nested_headers.get('Subject', original_subject)
                        original_from = (
                            nested_headers.get('X-Original-Sender') or
                            nested_headers.get('Reply-To') or
                            nested_headers.get('From') or
                            original_from
                        )

                    # Try text/plain first, fall back to HTML converted to text
                    original_body = find_content_recursive(part, 'text/plain')
                    if not original_body:
                        html_body = find_content_recursive(part, 'text/html')
                        if html_body:
                            original_body = html_to_text(html_body)

                    # Extract attachments from nested RFC822 structure
                    original_attachments = extract_email_attachments(service, msg_ref['id'], part)

        if approve_addr:
            # Get the notification's Message-Id and Subject for proper reply threading
            notification_message_id = headers.get('Message-Id') or headers.get('Message-ID', '')
            notification_subject = headers.get('Subject', '')

            pending.append({
                'gmail_id': msg_ref['id'],
                'approve_addr': approve_addr,
                'original_subject': original_subject,
                'original_from': original_from,
                'original_body': original_body,
                'original_attachments': original_attachments,
                'notification_message_id': notification_message_id,
                'notification_subject': notification_subject,
            })

    return pending


def moderate_message(pending_msg, approve=True):
    """Approve a pending message by sending email reply."""
    if DRY_RUN:
        logger.info(f"  [DRY RUN] Would approve: {pending_msg['original_subject'][:50]}")
        return True

    creds = get_google_credentials()
    service = build('gmail', 'v1', credentials=creds, cache_discovery=False)

    to_addr = pending_msg['approve_addr']

    # Create a properly formatted reply with threading headers
    message = MIMEText('')
    message['To'] = to_addr
    message['From'] = 'gbauto@ucsc.edu'

    # Use Re: prefix on original subject to indicate reply
    orig_subject = pending_msg.get('notification_subject', '')
    if orig_subject and not orig_subject.startswith('Re:'):
        message['Subject'] = f'Re: {orig_subject}'
    else:
        message['Subject'] = orig_subject or 'Re: Moderation'

    # Add threading headers to make this a proper reply
    msg_id = pending_msg.get('notification_message_id', '')
    if msg_id:
        message['In-Reply-To'] = msg_id
        message['References'] = msg_id

    encoded = base64.urlsafe_b64encode(message.as_bytes()).decode('utf-8')

    try:
        service.users().messages().send(
            userId='me',
            body={'raw': encoded}
        ).execute()
        return True
    except Exception as e:
        logger.error(f"  Error approving message: {e}")
        return False


def send_error_notification(subject, body):
    """Send an error notification email to the QA team.

    Used to alert the team when Redmine API operations fail after retries.
    """
    if DRY_RUN:
        logger.info(f"  [DRY RUN] Would send error notification: {subject}")
        return True

    try:
        creds = get_google_credentials()
        service = build('gmail', 'v1', credentials=creds, cache_discovery=False)

        message = MIMEText(body)
        message['To'] = 'browserqa-group@ucsc.edu'
        message['From'] = 'gbauto@ucsc.edu'
        message['Subject'] = f'[MLQ Automation Error] {subject}'

        encoded = base64.urlsafe_b64encode(message.as_bytes()).decode('utf-8')

        service.users().messages().send(
            userId='me',
            body={'raw': encoded}
        ).execute()
        logger.info(f"Sent error notification email: {subject}")
        return True
    except Exception as e:
        logger.error(f"Failed to send error notification email: {e}")
        return False


def delete_moderation_email(gmail_id):
    """Delete/archive the moderation notification after processing."""
    if DRY_RUN:
        return

    creds = get_google_credentials()
    service = build('gmail', 'v1', credentials=creds, cache_discovery=False)
    try:
        service.users().messages().trash(userId='me', id=gmail_id).execute()
    except Exception as e:
        logger.error(f"  Error trashing notification: {e}")


def get_emails_from_gmail(group_email, minutes_ago=60):
    """Get recent emails sent to a mailing list."""
    creds = get_google_credentials()
    service = build('gmail', 'v1', credentials=creds, cache_discovery=False)

    cutoff = datetime.now(PST) - timedelta(minutes=minutes_ago)
    query = f"to:{group_email} after:{int(cutoff.timestamp())}"

    results = service.users().messages().list(
        userId='me', q=query, maxResults=50
    ).execute()

    messages = []
    for msg_ref in results.get('messages', []):
        msg = service.users().messages().get(
            userId='me', id=msg_ref['id'], format='full'
        ).execute()

        headers = {h['name']: h['value'] for h in msg['payload']['headers']}
        subject = headers.get('Subject', '(no subject)')

        # Get original sender - Google Groups may rewrite From to the list address
        # Priority: X-Original-Sender > Reply-To > From
        from_addr = (
            headers.get('X-Original-Sender') or
            headers.get('Reply-To') or
            headers.get('From', '')
        )

        # Skip moderation-related emails (notifications and our own replies)
        if 'Message Pending' in subject:
            continue
        if 'Moderation response' in subject:
            continue
        if 'gbauto@ucsc.edu' in from_addr:
            continue
        if '+msgappr@' in from_addr or '+msgrej@' in from_addr:
            continue

        # Extract body - prefer text/plain, fall back to text/html converted to text
        def extract_body_content(payload, target_type):
            """Recursively find content of target_type in email payload."""
            if payload.get('mimeType') == target_type:
                data = payload.get('body', {}).get('data', '')
                if data:
                    return base64.urlsafe_b64decode(data).decode('utf-8', errors='ignore')
            if 'parts' in payload:
                for part in payload['parts']:
                    result = extract_body_content(part, target_type)
                    if result:
                        return result
            return ''

        # Try text/plain first, fall back to HTML converted to text
        body = extract_body_content(msg['payload'], 'text/plain')
        if not body:
            html_body = extract_body_content(msg['payload'], 'text/html')
            if html_body:
                body = html_to_text(html_body)

        # Extract attachments
        attachments = extract_email_attachments(service, msg['id'], msg['payload'])

        messages.append({
            'id': msg['id'],
            'thread_id': msg['threadId'],
            'subject': subject,
            'from': from_addr,
            'to': headers.get('To', ''),
            'cc': headers.get('Cc', ''),
            'date': headers.get('Date', ''),
            'body': body,
            'attachments': attachments,
            'timestamp': int(msg['internalDate']) / 1000,
        })

    return messages


def html_to_text(html):
    """Convert HTML to plain text by stripping tags and decoding entities.

    Used as a fallback when an email only has HTML content (no text/plain part).
    """
    if not html:
        return ''

    # Remove script and style tags with their content
    text = re.sub(r'<script[^>]*>.*?</script>', '', html, flags=re.DOTALL | re.IGNORECASE)
    text = re.sub(r'<style[^>]*>.*?</style>', '', text, flags=re.DOTALL | re.IGNORECASE)

    # Convert <br> and </p> to newlines
    text = re.sub(r'<br\s*/?>', '\n', text, flags=re.IGNORECASE)
    text = re.sub(r'</p>', '\n\n', text, flags=re.IGNORECASE)
    text = re.sub(r'</div>', '\n', text, flags=re.IGNORECASE)
    text = re.sub(r'</li>', '\n', text, flags=re.IGNORECASE)

    # Remove all other HTML tags
    text = re.sub(r'<[^>]+>', '', text)

    # Decode HTML entities
    text = html_module.unescape(text)

    # Normalize whitespace (collapse multiple spaces/newlines)
    text = re.sub(r'[ \t]+', ' ', text)  # Collapse horizontal whitespace
    text = re.sub(r'\n[ \t]+', '\n', text)  # Remove leading spaces on lines
    text = re.sub(r'[ \t]+\n', '\n', text)  # Remove trailing spaces on lines
    text = re.sub(r'\n{3,}', '\n\n', text)  # Collapse multiple newlines

    return text.strip()


def sanitize_for_redmine(text):
    """Clean up text for Redmine textile compatibility."""
    # Remove emojis and other 4-byte UTF-8 characters that cause MySQL utf8 encoding issues
    # MySQL utf8 is 3-byte max; utf8mb4 supports 4-byte but many Redmine installs use utf8
    text = re.sub(r'[\U00010000-\U0010FFFF]', '', text)

    # Remove Outlook duplicate URL format: URL<URL> -> URL
    # Outlook often includes the URL twice: once as display text, once in angle brackets
    text = re.sub(
        r'(https?://[^\s<>\[\]]+)<https?://[^\s<>\[\]>]+>',
        r'\1',
        text
    )

    # Note: We do NOT auto-link URLs here. Redmine uses Textile format by default
    # and has its own auto-linking for bare URLs. Adding Markdown-style [URL](URL)
    # links causes display issues in Textile mode.

    lines = text.split('\n')
    cleaned = []

    for line in lines:
        # Remove leading whitespace that would trigger code blocks (4+ spaces or tabs)
        stripped = line.lstrip(' \t')
        leading = len(line) - len(stripped)

        if leading >= 4:
            # Reduce to 2 spaces to preserve some structure without triggering code block
            line = '  ' + stripped
        elif leading > 0 and line.startswith('\t'):
            # Convert tabs to spaces
            line = '  ' + stripped

        # Escape # at start of line to prevent header formatting
        if line.startswith('#'):
            line = '\\' + line

        # Escape --- or *** that would become horizontal rules
        if re.match(r'^[-*_]{3,}\s*$', line):
            line = '\\' + line

        # Escape leading - or * followed by space that would become list items
        # (only if it doesn't look intentional, i.e., not followed by more text structure)
        if re.match(r'^[-*]\s+[a-z]', line, re.IGNORECASE):
            # Looks like prose starting with dash, not a list - escape it
            if not re.match(r'^[-*]\s+\S+\s*$', line):  # Single word items are likely lists
                line = '\\' + line

        cleaned.append(line)

    return '\n'.join(cleaned)


def strip_quoted_content(body):
    """Remove quoted reply content and Google Groups footer from email body.

    Handles three reply styles:
    1. Top-posted: New content at top, quoted content below (most common)
    2. Inline/interleaved: Short quotes with replies below each (less common)
    3. Bottom-posted: Quoted content at top, new content below (e.g., some
       Thunderbird/Linux mail clients)

    For bottom-posted replies, when a quote header ("On ... wrote:") appears
    within the first 3 lines, the quoted block is skipped and only the new
    content that follows is kept.
    """
    lines = body.split('\n')
    cleaned = []
    i = 0

    # Pattern to strip Unicode control characters (LTR/RTL marks, etc.)
    # These can appear at the start or end of lines in emails with mixed-direction text
    unicode_control_chars = '[\u200e\u200f\u202a-\u202e\u2066-\u2069]'
    unicode_control_pattern = re.compile(f'^{unicode_control_chars}+|{unicode_control_chars}+$')

    while i < len(lines):
        line = lines[i]
        # Strip Unicode control characters for pattern matching
        line_stripped = unicode_control_pattern.sub('', line)
        # Also strip leading quote markers ("> ") to catch quoted "On...wrote:" patterns
        # e.g., "> On Jan 15, 2026, at 12:01 AM, Name wrote:"
        line_unquoted = re.sub(r'^>\s*', '', line_stripped)

        # Detect "On <date> <person> wrote:" quote headers
        is_quote_header = False
        quote_header_lines = 1  # How many lines this header spans

        if re.match(r'^On .+wrote:\s*$', line_unquoted, re.IGNORECASE):
            is_quote_header = True
        elif re.match(r'^On .+, at .+wrote:\s*$', line_unquoted, re.IGNORECASE):
            is_quote_header = True
        elif re.match(r'^On .+@.+>\s*$', line_unquoted) or re.match(r'^On .*\d{4}.*[AP]M .+', line_unquoted):
            if i + 1 < len(lines):
                next_stripped = unicode_control_pattern.sub('', lines[i + 1])
                next_unquoted = re.sub(r'^>\s*', '', next_stripped).strip().lower()
                if next_unquoted.endswith('wrote:'):
                    is_quote_header = True
                    quote_header_lines = 2
        elif line_unquoted.strip().lower().endswith('wrote:') and i > 0:
            prev = lines[i - 1]
            prev_stripped = unicode_control_pattern.sub('', prev)
            prev_unquoted = re.sub(r'^>\s*', '', prev_stripped)
            if re.match(r'^On .+', prev_unquoted, re.IGNORECASE):
                if cleaned and cleaned[-1] == prev:
                    cleaned.pop()
                is_quote_header = True

        if is_quote_header:
            # Bottom-posted reply: quote header near the start means new content
            # is below the quoted block. Skip the quoted block, keep what follows.
            # Allow at most 1 non-blank line above (e.g., a greeting like "Hi,")
            non_blank_above = sum(1 for l in cleaned if l.strip())
            if non_blank_above <= 1:
                # Skip past the quote header
                i += quote_header_lines
                # Skip the quoted lines (starting with ">")
                while i < len(lines):
                    l = lines[i]
                    l_stripped = unicode_control_pattern.sub('', l).strip()
                    if l_stripped.startswith('>') or l_stripped == '':
                        i += 1
                    else:
                        break
                # Reset cleaned — anything before the quote header was blank/trivial
                cleaned = []
                continue
            else:
                # Top-posted reply: we already have real content above, stop here
                break

        if re.match(r'^-{4,}\s*Original Message\s*-{4,}', line_stripped, re.IGNORECASE):
            break
        # Gmail forwarded message format: "---------- Forwarded message ---------"
        if re.match(r'^-{4,}\s*Forwarded message\s*-{4,}', line_stripped, re.IGNORECASE):
            break
        # Outlook single-line forward format
        if re.match(r'^From:.*Sent:.*To:', line_stripped, re.IGNORECASE):
            break
        # Outlook multi-line forward format:
        # From: Name
        # Sent: Date
        # To: Recipients
        if re.match(r'^From:\s*.+', line_stripped, re.IGNORECASE):
            # Look ahead for Sent: and To: on subsequent lines
            if i + 2 < len(lines):
                next1 = unicode_control_pattern.sub('', lines[i + 1])
                next2 = unicode_control_pattern.sub('', lines[i + 2])
                if re.match(r'^Sent:\s*.+', next1, re.IGNORECASE) and re.match(r'^To:\s*.+', next2, re.IGNORECASE):
                    break
        # Stop at Google Groups footer
        if 'You received this message because you are subscribed to the Google Groups' in line:
            break
        # Skip Google Groups unsubscribe line
        if 'To unsubscribe from this group and stop receiving emails from it' in line:
            i += 1
            continue
        if line.strip() == '---' and len(cleaned) > 0:
            i += 1
            continue

        # Handle quoted lines (starting with >)
        # Keep quoted lines for inline reply context - they provide important context
        # The "On ... wrote:" and other patterns above will stop at the full original message
        cleaned.append(line)
        i += 1

    # Remove trailing dashes and whitespace
    while cleaned and cleaned[-1].strip() in ('', '---', '--'):
        cleaned.pop()

    return '\n'.join(cleaned).rstrip()


# Attachment handling constants
SIGNATURE_ATTACHMENT_PATTERNS = [
    r'^logo',
    r'^signature',
    r'^banner',
    r'^icon',
    r'^footer',
    r'^divider',
    # Note: Removed image\d* pattern - Outlook uses this for legitimate inline images.
    # Size filter (MIN_ATTACHMENT_SIZE) handles small signature icons instead.
]
MIN_ATTACHMENT_SIZE = 1024  # Skip attachments smaller than 1KB (likely icons)
MAX_ATTACHMENT_SIZE = 10 * 1024 * 1024  # 10MB max per attachment


def is_signature_attachment(filename, size_bytes):
    """Check if an attachment is likely a signature/icon that should be skipped."""
    if not filename:
        return True

    # Skip very small images (likely icons/spacers)
    if size_bytes < MIN_ATTACHMENT_SIZE:
        return True

    # Check filename against signature patterns
    name_lower = filename.lower().rsplit('.', 1)[0]  # Remove extension
    for pattern in SIGNATURE_ATTACHMENT_PATTERNS:
        if re.match(pattern, name_lower, re.IGNORECASE):
            return True

    return False


def extract_email_attachments(gmail_service, message_id, payload):
    """
    Extract all attachments from an email.
    Returns list of dicts with: filename, mimeType, data, size
    Filters out signature images and oversized files.
    """
    attachments = []

    def find_attachments_recursive(part):
        mime_type = part.get('mimeType', '')
        filename = part.get('filename', '')
        body = part.get('body', {})
        attachment_id = body.get('attachmentId')
        size = body.get('size', 0)

        # Check if this part is an attachment (has attachmentId or filename with size)
        if attachment_id and filename:
            # Skip signature/icon attachments
            if is_signature_attachment(filename, size):
                logger.debug(f"Skipping signature attachment: {filename} ({size} bytes)")
            elif size > MAX_ATTACHMENT_SIZE:
                logger.warning(f"Skipping oversized attachment: {filename} ({size} bytes)")
            else:
                # Download the attachment
                try:
                    att_data = gmail_service.users().messages().attachments().get(
                        userId='me', messageId=message_id, id=attachment_id
                    ).execute()
                    file_data = base64.urlsafe_b64decode(att_data['data'])
                    attachments.append({
                        'filename': filename,
                        'mimeType': mime_type,
                        'data': file_data,
                        'size': len(file_data)
                    })
                except Exception as e:
                    logger.warning(f"Failed to download attachment {filename}: {e}")

        # Recurse into nested parts
        for subpart in part.get('parts', []):
            find_attachments_recursive(subpart)

    find_attachments_recursive(payload)
    return attachments


def upload_attachments_to_redmine(attachments):
    """
    Upload attachments to Redmine and return tokens.
    Returns list of dicts with: filename, token, content_type
    """
    if not attachments:
        return []

    uploaded = []
    for att in attachments:
        try:
            upload_url = f"{CONFIG['REDMINE_URL']}/uploads.json?filename={att['filename']}"
            headers = {
                'X-Redmine-API-Key': CONFIG['REDMINE_API_KEY'],
                'Content-Type': 'application/octet-stream'
            }

            resp = requests.post(upload_url, data=att['data'], headers=headers, timeout=60)

            if resp.status_code == 201:
                token = resp.json()['upload']['token']
                uploaded.append({
                    'filename': att['filename'],
                    'token': token,
                    'content_type': att['mimeType']
                })
                logger.debug(f"Uploaded attachment: {att['filename']}")
            elif resp.status_code == 422:
                logger.warning(f"Attachment too large for Redmine: {att['filename']}")
            else:
                logger.warning(f"Failed to upload {att['filename']}: {resp.status_code}")
        except Exception as e:
            logger.warning(f"Error uploading attachment {att['filename']}: {e}")

    return uploaded


def replace_inline_images(body, uploaded_attachments):
    """
    Replace inline image placeholders with Redmine inline image syntax.
    Handles:
    - Gmail format: [image: filename.png]
    - Outlook CID format: [cid:filename.png@identifier]
    """
    if not uploaded_attachments:
        return body

    # Build a map of filenames (case-insensitive)
    filename_map = {att['filename'].lower(): att['filename'] for att in uploaded_attachments}

    def replace_gmail_placeholder(match):
        """Handle Gmail [image: filename] format."""
        placeholder_name = match.group(1).strip()
        lookup = placeholder_name.lower()
        if lookup in filename_map:
            actual_filename = filename_map[lookup]
            return f'!{actual_filename}!'
        return match.group(0)

    def replace_cid_placeholder(match):
        """Handle Outlook [cid:filename@identifier] format."""
        cid_content = match.group(1)
        # Extract filename from cid:filename@identifier or cid:filename
        if '@' in cid_content:
            filename_part = cid_content.split('@')[0]
        else:
            filename_part = cid_content

        lookup = filename_part.lower()
        if lookup in filename_map:
            actual_filename = filename_map[lookup]
            return f'!{actual_filename}!'
        return match.group(0)

    # Replace Gmail [image: filename.png] patterns
    body = re.sub(r'\[image:\s*([^\]]+)\]', replace_gmail_placeholder, body, flags=re.IGNORECASE)

    # Replace Outlook [cid:filename.png@identifier] patterns
    body = re.sub(r'\[cid:([^\]]+)\]', replace_cid_placeholder, body, flags=re.IGNORECASE)

    return body


def extract_email_address(from_header):
    """Extract just the email address from a From header."""
    _, email = parseaddr(from_header)
    return email.lower()


def extract_all_email_addresses(header_value):
    """Extract all email addresses from a header (To, CC can have multiple)."""
    if not header_value:
        return []
    # Handle multiple addresses separated by commas
    # parseaddr only handles single address, so we split first
    addresses = []
    for part in header_value.split(','):
        _, email = parseaddr(part.strip())
        if email:
            addresses.append(email.lower())
    return addresses


def extract_hgusersuggestion_email(subject):
    """Extract user email from hgUserSuggestion form subjects.

    hgUserSuggestion form submissions have subjects like:
    'hgUserSuggestion mmcgary44@gmail.com 2026-01-28 05:59:21'

    Returns the extracted email, or None if not an hgUserSuggestion subject.
    """
    if not subject:
        return None

    # Check if this is an hgUserSuggestion subject
    if 'hgusersuggestion' not in subject.lower():
        return None

    # Extract email using pattern: hgUserSuggestion <email> <date> <time>
    # Email pattern should match common email formats
    match = re.search(r'hgusersuggestion\s+([^\s]+@[^\s]+)\s+\d{4}-\d{2}-\d{2}', subject, re.IGNORECASE)
    if match:
        return match.group(1).lower()

    return None


def normalize_subject(subject):
    """Normalize subject for matching by removing common prefixes and list tags.

    Removes these patterns from ANYWHERE in the subject (not just the start)
    to handle cases like 'TICKET-123 Re: [genome] Original subject'.
    """
    if not subject:
        return ''

    s = subject.strip()

    # Remove mailing list tags from anywhere
    # Generate patterns from CONFIG to stay in sync with configured lists
    for addr in CONFIG['MODERATED_LISTS'] + CONFIG['UNMODERATED_LISTS']:
        list_name = addr.split('@')[0]
        # Escape the brackets for regex and remove case-insensitively
        pattern = re.escape(f'[{list_name}]')
        s = re.sub(pattern, '', s, flags=re.IGNORECASE)

    # Remove [External] tags added by email gateways
    s = re.sub(r'\[external\]', '', s, flags=re.IGNORECASE)
    s = re.sub(r'\bexternal:\s*', '', s, flags=re.IGNORECASE)

    # Remove reply/forward prefixes from anywhere
    # Use word boundary \b to avoid matching inside words (e.g., "Re-install")
    reply_forward_patterns = [
        r'\bre:\s*',      # Re: RE:
        r'\bfwd?:\s*',    # Fwd: FW: Fw:
        r'\baw:\s*',      # AW: (German "Antwort")
    ]
    for pattern in reply_forward_patterns:
        s = re.sub(pattern, '', s, flags=re.IGNORECASE)

    # Strip leading punctuation and whitespace (e.g., ": Subject" -> "Subject")
    s = re.sub(r'^[\s:,\-]+', '', s)

    # Collapse multiple whitespace and strip
    s = ' '.join(s.split())

    return s


@retry(max_attempts=3, delay=2, exceptions=(requests.RequestException,))
def find_existing_ticket(subject, thread_emails):
    """Find an existing Redmine ticket by subject and email match.

    Requires both:
    1. Normalized subject match
    2. At least one email from thread_emails matches the ticket's Email field

    Exception: if any thread participant is a @ucsc.edu address (staff), the
    email match is skipped and subject match alone is sufficient. Staff replies
    to mailing list threads typically go to the list, not the original sender,
    so the original sender's email won't appear in To/CC.

    thread_emails should include all participants (From, To, CC) to handle
    replies where the original sender appears in To/CC fields.
    """
    normalized = normalize_subject(subject)

    url = f"{CONFIG['REDMINE_URL']}/issues.json"
    params = {
        'project_id': CONFIG['REDMINE_PROJECT'],
        'subject': f"~{normalized}",
        'status_id': '*',
        'limit': 100,
    }
    headers = {'X-Redmine-API-Key': CONFIG['REDMINE_API_KEY']}

    resp = requests.get(url, params=params, headers=headers, timeout=30)
    resp.raise_for_status()
    data = resp.json()

    email_list = [e.lower() for e in thread_emails]
    has_staff_participant = any(e.endswith('@ucsc.edu') for e in email_list)

    for issue in data.get('issues', []):
        if normalize_subject(issue['subject']).lower() != normalized.lower():
            continue

        # Staff replies to mailing list threads don't need email match —
        # subject match is sufficient since staff wouldn't start a new
        # unrelated thread with the same subject
        if has_staff_participant:
            return issue['id']

        # For external senders, require email match to avoid false positives
        # on generic subjects
        email_field = next(
            (f for f in issue.get('custom_fields', [])
             if f['id'] == CONFIG['CUSTOM_FIELDS']['Email']),
            None
        )
        if email_field and email_field.get('value'):
            ticket_emails = [e.strip().lower() for e in email_field['value'].split(',')]
            if any(te in ee or ee in te for te in email_list for ee in ticket_emails):
                return issue['id']

    return None


def normalize_for_comparison(text):
    """Normalize text for duplicate detection by removing formatting."""
    # Remove Redmine/markdown formatting characters
    text = re.sub(r'^>\s*', '', text, flags=re.MULTILINE)  # Quote markers
    text = re.sub(r'!\S+!', '', text)  # Redmine inline images: !filename.png!
    text = re.sub(r'[*_`~]', '', text)  # Bold, italic, code markers (keep ! for punctuation)
    text = re.sub(r'---\s*(New Email Update|AI Suggested Response).*?---', '', text, flags=re.DOTALL)
    text = re.sub(r'^From:.*$', '', text, flags=re.MULTILINE)  # Remove From: lines
    # Collapse whitespace and lowercase
    return ' '.join(text.split()).lower()


def text_similarity(text1, text2):
    """Calculate similarity ratio between two texts using SequenceMatcher."""
    return SequenceMatcher(None, text1, text2).ratio()


def text_containment(text1, text2, threshold=0.85):
    """Check if the shorter text is substantially contained within the longer text.

    This handles cases where a draft reply is posted to Redmine, then sent via email
    with additional content (greeting, signature, etc.). The draft would be contained
    within the email even though overall similarity is low.

    Returns True if at least `threshold` (default 85%) of the shorter text is found
    as a contiguous match within the longer text.
    """
    if not text1 or not text2:
        return False

    shorter, longer = (text1, text2) if len(text1) <= len(text2) else (text2, text1)

    if len(shorter) < 20:
        return False

    # Find the longest contiguous match
    matcher = SequenceMatcher(None, shorter, longer)
    match = matcher.find_longest_match(0, len(shorter), 0, len(longer))

    # Calculate what percentage of the shorter text is contained in the longer
    containment_ratio = match.size / len(shorter)
    return containment_ratio >= threshold


def content_exists_in_ticket(ticket, email_body, similarity_threshold=0.80, containment_threshold=0.85):
    """Check if email content already exists in ticket description or journals.

    Uses two methods to detect duplicates:
    1. Overall similarity (default 85% threshold) - catches near-identical content
    2. Containment check (default 85% threshold) - catches cases where a draft
       reply was posted to Redmine, then sent via email with additional content
       (greeting, signature, etc.)

    This prevents the script from reopening tickets when staff post their draft
    reply to Redmine and then send it via email.
    """
    stripped = strip_quoted_content(email_body).strip()
    if len(stripped) < 20:
        # Very short content, skip duplicate check
        return False

    email_normalized = normalize_for_comparison(stripped)

    # Quick check: if email content is very short after normalization, skip
    if len(email_normalized) < 20:
        return False

    def is_duplicate(text1, text2, context_name):
        """Check if two texts are duplicates using similarity or containment."""
        # Check overall similarity
        similarity = text_similarity(text1, text2)
        if similarity >= similarity_threshold:
            logger.debug(f"Found similar content in {context_name} ({similarity*100:.0f}% similarity)")
            return True

        # Check containment (shorter text contained in longer)
        if text_containment(text1, text2, containment_threshold):
            logger.debug(f"Found contained content in {context_name}")
            return True

        return False

    # Check ticket description
    desc = ticket.get('description', '')
    if desc:
        desc_normalized = normalize_for_comparison(desc)
        if desc_normalized and is_duplicate(email_normalized, desc_normalized, "ticket description"):
            return True

    # Check all journal notes
    for journal in ticket.get('journals', []):
        notes = journal.get('notes', '')
        if notes:
            notes_normalized = normalize_for_comparison(notes)
            if notes_normalized and is_duplicate(email_normalized, notes_normalized, f"journal #{journal.get('id')}"):
                return True

    logger.debug("No similar content found in ticket")
    return False


@retry(max_attempts=3, delay=2, exceptions=(requests.RequestException,))
def get_ticket_journals(ticket_id):
    """Get journal (comment) history for a ticket."""
    url = f"{CONFIG['REDMINE_URL']}/issues/{ticket_id}.json?include=journals"
    headers = {'X-Redmine-API-Key': CONFIG['REDMINE_API_KEY']}

    resp = requests.get(url, headers=headers, timeout=30)
    resp.raise_for_status()
    return resp.json().get('issue', {})


def create_ticket(subject, body, sender_emails, mlm_name, category='Other', attachments=None):
    """Create a new Redmine ticket with optional attachments.

    Includes retry logic for transient server errors (5xx) and network issues.
    Sends email notification to QA team if all retries fail.
    """
    if DRY_RUN:
        att_info = f" with {len(attachments)} attachment(s)" if attachments else ""
        logger.info(f"  [DRY RUN] Would create ticket: {subject[:50]}{att_info}")
        logger.info(f"    Category: {category}, MLM: {mlm_name}")
        return None

    url = f"{CONFIG['REDMINE_URL']}/issues.json"
    headers = {
        'X-Redmine-API-Key': CONFIG['REDMINE_API_KEY'],
        'Content-Type': 'application/json',
    }

    # Strip emojis from subject (body should already be sanitized via sanitize_for_redmine)
    clean_subject = re.sub(r'[\U00010000-\U0010FFFF]', '', subject)

    data = {
        'issue': {
            'project_id': CONFIG['REDMINE_PROJECT'],
            'subject': clean_subject,
            'description': f"From: {sender_emails[0]}\n\n{body}",
            'tracker_id': CONFIG['TRACKER_ID'],
            'priority_id': CONFIG['PRIORITY_ID'],
            'status_id': CONFIG['STATUS_ID'],
            'custom_fields': [
                {'id': CONFIG['CUSTOM_FIELDS']['MLQ Category - primary'], 'value': category},
                {'id': CONFIG['CUSTOM_FIELDS']['Email'], 'value': ', '.join(sender_emails)},
                {'id': CONFIG['CUSTOM_FIELDS']['MLM'], 'value': mlm_name},
            ],
        }
    }

    # Add attachments if provided (from upload_attachments_to_redmine)
    if attachments:
        data['issue']['uploads'] = [
            {'token': att['token'], 'filename': att['filename'], 'content_type': att['content_type']}
            for att in attachments
        ]

    # Retry logic for transient errors
    max_attempts = 3
    retry_delay = 5  # seconds
    last_error = None

    for attempt in range(1, max_attempts + 1):
        try:
            resp = requests.post(url, json=data, headers=headers, timeout=30)

            if resp.status_code == 201:
                ticket_id = resp.json()['issue']['id']
                att_info = f" with {len(attachments)} attachment(s)" if attachments else ""
                logger.info(f"Created ticket #{ticket_id}: {subject[:50]}{att_info}")
                return ticket_id

            # Log detailed error info
            logger.error(f"Redmine API error creating ticket (attempt {attempt}/{max_attempts}): "
                        f"HTTP {resp.status_code} - {resp.text[:500]}")

            # Don't retry client errors (4xx) - something wrong with our request
            if 400 <= resp.status_code < 500:
                last_error = f"HTTP {resp.status_code}: {resp.text[:500]}"
                break

            # Server error (5xx) - retry after delay
            if resp.status_code >= 500:
                last_error = f"HTTP {resp.status_code}: {resp.text[:500]}"
                if attempt < max_attempts:
                    logger.info(f"  Retrying in {retry_delay} seconds...")
                    time.sleep(retry_delay)
                    retry_delay *= 2  # Exponential backoff
                continue

        except requests.RequestException as e:
            logger.error(f"Network error creating ticket (attempt {attempt}/{max_attempts}): {e}")
            last_error = f"Network error: {e}"
            if attempt < max_attempts:
                logger.info(f"  Retrying in {retry_delay} seconds...")
                time.sleep(retry_delay)
                retry_delay *= 2
                continue

    # All retries failed - send notification email
    error_body = f"""The MLQ automation script failed to create a Redmine ticket after {max_attempts} attempts.

Subject: {subject}
From: {', '.join(sender_emails)}
Category: {category}
MLM: {mlm_name}

Error: {last_error}

This email needs to be manually processed or the issue investigated.

Timestamp: {datetime.now(PST).strftime('%Y-%m-%d %H:%M:%S %Z')}
"""
    send_error_notification(f"Failed to create ticket: {subject[:50]}", error_body)
    return None


def update_ticket(ticket_id, comment, reopen=False, new_mlm=None, attachments=None):
    """Add a comment to an existing ticket, optionally reopening it and attaching files.

    Includes retry logic for transient server errors (5xx) and network issues.
    Sends email notification to QA team if all retries fail.
    """
    if DRY_RUN:
        action = "reopen and update" if reopen else "update"
        att_info = f" with {len(attachments)} attachment(s)" if attachments else ""
        logger.info(f"  [DRY RUN] Would {action} ticket #{ticket_id}{att_info}")
        return True

    url = f"{CONFIG['REDMINE_URL']}/issues/{ticket_id}.json"
    headers = {
        'X-Redmine-API-Key': CONFIG['REDMINE_API_KEY'],
        'Content-Type': 'application/json',
    }

    # Strip emojis from comment (may not have gone through sanitize_for_redmine)
    clean_comment = re.sub(r'[\U00010000-\U0010FFFF]', '', comment)

    data = {'issue': {'notes': clean_comment}}
    if reopen:
        data['issue']['status_id'] = CONFIG['STATUS_ID']
        data['issue']['assigned_to_id'] = ''  # Clear assignee
        if new_mlm:
            data['issue']['custom_fields'] = [
                {'id': CONFIG['CUSTOM_FIELDS']['MLM'], 'value': new_mlm}
            ]

    # Add attachments if provided (from upload_attachments_to_redmine)
    if attachments:
        data['issue']['uploads'] = [
            {'token': att['token'], 'filename': att['filename'], 'content_type': att['content_type']}
            for att in attachments
        ]

    # Retry logic for transient errors
    max_attempts = 3
    retry_delay = 5  # seconds
    last_error = None

    for attempt in range(1, max_attempts + 1):
        try:
            resp = requests.put(url, json=data, headers=headers, timeout=30)

            if resp.status_code in (200, 204):
                action = "Reopened and updated" if reopen else "Updated"
                att_info = f" with {len(attachments)} attachment(s)" if attachments else ""
                logger.info(f"{action} ticket #{ticket_id}{att_info}")
                return True

            # Log detailed error info
            logger.error(f"Redmine API error updating ticket #{ticket_id} (attempt {attempt}/{max_attempts}): "
                        f"HTTP {resp.status_code} - {resp.text[:500]}")

            # Don't retry client errors (4xx) - something wrong with our request
            if 400 <= resp.status_code < 500:
                last_error = f"HTTP {resp.status_code}: {resp.text[:500]}"
                break

            # Server error (5xx) - retry after delay
            if resp.status_code >= 500:
                last_error = f"HTTP {resp.status_code}: {resp.text[:500]}"
                if attempt < max_attempts:
                    logger.info(f"  Retrying in {retry_delay} seconds...")
                    time.sleep(retry_delay)
                    retry_delay *= 2  # Exponential backoff
                continue

        except requests.RequestException as e:
            logger.error(f"Network error updating ticket #{ticket_id} (attempt {attempt}/{max_attempts}): {e}")
            last_error = f"Network error: {e}"
            if attempt < max_attempts:
                logger.info(f"  Retrying in {retry_delay} seconds...")
                time.sleep(retry_delay)
                retry_delay *= 2
                continue

    # All retries failed - send notification email
    comment_preview = comment[:200] + '...' if len(comment) > 200 else comment
    error_body = f"""The MLQ automation script failed to update Redmine ticket #{ticket_id} after {max_attempts} attempts.

Ticket: #{ticket_id}
Action: {'Reopen and update' if reopen else 'Update'}
Comment preview: {comment_preview}

Error: {last_error}

This update needs to be manually applied or the issue investigated.

Timestamp: {datetime.now(PST).strftime('%Y-%m-%d %H:%M:%S %Z')}
"""
    send_error_notification(f"Failed to update ticket #{ticket_id}", error_body)
    return False


def create_tickets_for_approved(approved_messages):
    """Create or update Redmine tickets for approved moderation messages."""
    if not approved_messages:
        return

    mlm_name = get_current_mlm()
    if not mlm_name:
        logger.error("Cannot determine MLM, skipping ticket creation for approved messages")
        return

    mlm_user_id = CONFIG['USER_IDS'].get(mlm_name)
    if not mlm_user_id:
        logger.error(f"No Redmine user ID for MLM: {mlm_name}")
        return

    logger.info(f"Creating tickets for {len(approved_messages)} approved message(s), MLM: {mlm_name}")

    for msg in approved_messages:
        subject = msg['original_subject']
        sender = msg['original_from']
        body = msg['original_body']
        group_email = msg['group_email']
        attachments = msg.get('original_attachments', [])

        # Add list prefix to subject if not present (for consistency with delivered emails)
        group_name = group_email.split('@')[0]
        prefix = f"[{group_name}]"
        if not subject.lower().startswith(prefix.lower()):
            subject = f"{prefix} {subject}"

        # Extract sender email address
        sender_email = extract_email_address(sender)
        sender_emails = [sender_email]

        # For hgUserSuggestion forms, the actual user's email is in the subject
        # Add it to sender_emails so ticket matching works when team replies to user
        user_email = extract_hgusersuggestion_email(msg['original_subject'])
        if user_email and user_email not in sender_emails:
            sender_emails.append(user_email)
            logger.info(f"  hgUserSuggestion: added user email {user_email} to ticket")

        # Upload attachments to Redmine first (we need filenames for inline replacement)
        uploaded_attachments = []
        if attachments:
            logger.info(f"  Uploading {len(attachments)} attachment(s)")
            uploaded_attachments = upload_attachments_to_redmine(attachments)

        # Process body: replace inline image placeholders, strip quotes, sanitize
        # Order matters: replace images BEFORE stripping to keep them in context
        processed_body = body
        if uploaded_attachments:
            processed_body = replace_inline_images(processed_body, uploaded_attachments)
        processed_body = sanitize_for_redmine(strip_quoted_content(processed_body))

        # Check for existing ticket (handles follow-up messages in threads)
        existing_ticket = find_existing_ticket(subject, sender_emails)

        if existing_ticket:
            # Update existing ticket
            logger.info(f"  Found existing ticket #{existing_ticket} for: {subject[:50]}")
            ticket = get_ticket_journals(existing_ticket)

            # Check if content already exists to avoid duplicates
            if content_exists_in_ticket(ticket, body):
                logger.info(f"  Skipping duplicate content for ticket #{existing_ticket}")
                continue

            # Skip empty updates (e.g., email was entirely quoted content)
            if not processed_body.strip() and not uploaded_attachments:
                logger.info(f"  Skipping empty update for ticket #{existing_ticket}")
                continue

            ticket_status = ticket.get('status', {}).get('name', '').lower()
            is_closed = 'closed' in ticket_status or 'resolved' in ticket_status

            comment = f"--- New Email Update ---\n\nFrom: {sender}\n\n{processed_body}"
            update_ticket(existing_ticket, comment, reopen=is_closed,
                         new_mlm=mlm_name if is_closed else None,
                         attachments=uploaded_attachments)
        else:
            # Analyze with Claude for category and draft response
            analysis = analyze_email_with_claude(subject, body, sender)

            logger.info(f"  Category: {analysis['category']}")

            # Create new ticket with attachments
            ticket_id = create_ticket(
                subject,
                processed_body,
                sender_emails,
                mlm_name,
                category=analysis['category'],
                attachments=uploaded_attachments
            )

            if ticket_id and analysis['draft_response']:
                draft_note = f"--- AI Suggested Response (Draft) ---\n\n{analysis['draft_response']}"
                update_ticket(ticket_id, draft_note)


def process_moderated_lists():
    """Process pending messages in moderated mailing lists."""
    # Collect all pending messages from all moderated lists
    all_pending = []
    for group_email in CONFIG['MODERATED_LISTS']:
        group_name = group_email.split('@')[0]
        logger.info(f"Checking pending messages for {group_email}")

        pending = get_pending_moderation_emails(group_name)
        logger.info(f"  Found {len(pending)} pending message(s)")

        for msg in pending:
            msg['group_email'] = group_email
            all_pending.append(msg)

    if not all_pending:
        return

    # Batch spam check all pending messages in one API call
    logger.info(f"Batch checking {len(all_pending)} message(s) for spam")
    spam_results = batch_check_spam_with_claude(all_pending)

    # Process results and collect approved messages
    approved_messages = []
    for i, msg in enumerate(all_pending):
        is_spam = spam_results.get(i, False)

        if is_spam:
            logger.info(f"  SPAM detected (not approving): {msg['original_subject'][:50]}")
            delete_moderation_email(msg['gmail_id'])
        else:
            logger.info(f"  Approving: {msg['original_subject'][:50]}")
            if moderate_message(msg, approve=True):
                delete_moderation_email(msg['gmail_id'])
                approved_messages.append(msg)

    # Create/update tickets for approved messages immediately
    create_tickets_for_approved(approved_messages)


def process_emails():
    """Process emails and create/update Redmine tickets."""
    mlm_name = get_current_mlm()
    if not mlm_name:
        logger.error("Cannot determine MLM, skipping email processing")
        return

    mlm_user_id = CONFIG['USER_IDS'].get(mlm_name)
    if not mlm_user_id:
        logger.error(f"No Redmine user ID for MLM: {mlm_name}")
        return

    logger.info(f"Current MLM: {mlm_name} (ID: {mlm_user_id})")

    all_lists = CONFIG['MODERATED_LISTS'] + CONFIG['UNMODERATED_LISTS']

    # Group emails by thread
    threads = {}
    for group_email in all_lists:
        emails = get_emails_from_gmail(group_email)
        for email in emails:
            thread_id = email['thread_id']
            if thread_id not in threads:
                threads[thread_id] = {
                    'subject': email['subject'],
                    'emails': [],
                    'group': group_email,
                }
            threads[thread_id]['emails'].append(email)

    for thread_id, thread in threads.items():
        thread['emails'].sort(key=lambda x: x['timestamp'])
        first_email = thread['emails'][0]

        # Sender emails - just the From addresses (for ticket creation)
        sender_emails = list(set(extract_email_address(e['from']) for e in thread['emails']))

        # All participant emails (From, To, CC) for matching existing tickets
        # This handles replies where original sender appears in To/CC
        # Note: We include mailing list addresses because some tickets (like hgUserSuggestion
        # form submissions) have the mailing list as their Email field
        thread_participants = set()
        for e in thread['emails']:
            thread_participants.add(extract_email_address(e['from']))
            thread_participants.update(extract_all_email_addresses(e.get('to', '')))
            thread_participants.update(extract_all_email_addresses(e.get('cc', '')))

        # Remove empty strings but keep mailing list addresses for matching
        thread_participants = [e for e in thread_participants if e]

        existing_ticket = find_existing_ticket(thread['subject'], thread_participants)

        if existing_ticket:
            # Check for new messages to add
            ticket = get_ticket_journals(existing_ticket)
            last_update = datetime.fromisoformat(
                ticket.get('created_on', '2000-01-01T00:00:00Z').replace('Z', '+00:00')
            )

            ticket_status = ticket.get('status', {}).get('name', '').lower()
            is_closed = 'closed' in ticket_status or 'resolved' in ticket_status

            for journal in ticket.get('journals', []):
                notes = journal.get('notes', '')
                if '--- New Email Update ---' in notes or 'From:' in notes:
                    journal_time = datetime.fromisoformat(
                        journal['created_on'].replace('Z', '+00:00')
                    )
                    if journal_time > last_update:
                        last_update = journal_time

            first_update = True
            for email in thread['emails']:
                email_time = datetime.fromtimestamp(email['timestamp'], tz=pytz.UTC)
                if email_time > last_update:
                    # Check if this content already exists in the ticket
                    if content_exists_in_ticket(ticket, email['body']):
                        logger.info(f"  Skipping duplicate content for ticket #{existing_ticket}")
                        continue

                    # Upload attachments from this email
                    email_attachments = email.get('attachments', [])
                    uploaded_attachments = []
                    if email_attachments:
                        logger.info(f"  Uploading {len(email_attachments)} attachment(s)")
                        uploaded_attachments = upload_attachments_to_redmine(email_attachments)

                    # Process body with inline image replacement
                    processed_body = email['body']
                    if uploaded_attachments:
                        processed_body = replace_inline_images(processed_body, uploaded_attachments)
                    processed_body = sanitize_for_redmine(strip_quoted_content(processed_body))

                    # Skip empty updates (e.g., email was entirely quoted content)
                    if not processed_body.strip() and not uploaded_attachments:
                        logger.info(f"  Skipping empty update for ticket #{existing_ticket}")
                        continue

                    comment = f"--- New Email Update ---\n\nFrom: {email['from']}\n\n{processed_body}"
                    reopen = is_closed and first_update
                    update_ticket(existing_ticket, comment, reopen=reopen,
                                 new_mlm=mlm_name if reopen else None,
                                 attachments=uploaded_attachments)
                    first_update = False
        else:
            # Analyze email with Claude (single call for spam, category, draft)
            analysis = analyze_email_with_claude(
                first_email['subject'],
                first_email['body'],
                first_email['from']
            )

            if analysis['is_spam']:
                logger.info(f"Skipping spam: {first_email['subject'][:50]}")
                continue

            logger.info(f"  Category: {analysis['category']}")

            # Upload attachments from the first email
            first_attachments = first_email.get('attachments', [])
            uploaded_attachments = []
            if first_attachments:
                logger.info(f"  Uploading {len(first_attachments)} attachment(s)")
                uploaded_attachments = upload_attachments_to_redmine(first_attachments)

            # Process body with inline image replacement
            processed_body = first_email['body']
            if uploaded_attachments:
                processed_body = replace_inline_images(processed_body, uploaded_attachments)
            processed_body = sanitize_for_redmine(strip_quoted_content(processed_body))

            ticket_id = create_ticket(
                thread['subject'],
                processed_body,
                sender_emails,
                mlm_name,
                category=analysis['category'],
                attachments=uploaded_attachments
            )

            if ticket_id:
                if len(thread['emails']) > 1:
                    for email in thread['emails'][1:]:
                        # Upload attachments from follow-up emails
                        email_attachments = email.get('attachments', [])
                        email_uploaded = []
                        if email_attachments:
                            logger.info(f"  Uploading {len(email_attachments)} attachment(s)")
                            email_uploaded = upload_attachments_to_redmine(email_attachments)

                        email_body = email['body']
                        if email_uploaded:
                            email_body = replace_inline_images(email_body, email_uploaded)
                        email_body = sanitize_for_redmine(strip_quoted_content(email_body))

                        comment = f"Message from: {email['from']}\n\n{email_body}"
                        update_ticket(ticket_id, comment, attachments=email_uploaded)

                if analysis['draft_response']:
                    draft_note = f"--- AI Suggested Response (Draft) ---\n\n{analysis['draft_response']}"
                    update_ticket(ticket_id, draft_note)


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(description='MLQ Automation Script')
    parser.add_argument('--dry-run', action='store_true', help='Run without making changes')
    args = parser.parse_args()

    global DRY_RUN
    DRY_RUN = args.dry_run

    if DRY_RUN:
        logger.info("=== DRY RUN MODE - No changes will be made ===")

    # Load configuration from ~/.hg.conf
    load_config_file()

    logger.info(f"=== MLQ Automation - {datetime.now(PST).strftime('%Y-%m-%d %H:%M:%S %Z')} ===")

    # Process emails first - this catches direct posts from staff (not moderated)
    # Must run BEFORE moderation approval to avoid timestamp issues
    logger.info("--- Processing Emails ---")
    process_emails()

    # Process moderated list pending messages (approve and create tickets)
    logger.info("--- Processing Moderated Lists ---")
    process_moderated_lists()

    logger.info("=== Complete ===")


if __name__ == '__main__':
    main()
