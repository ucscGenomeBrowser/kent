#!/usr/bin/env python3
"""
Automated Code Review Script for UCSC Genome Browser
Per-ticket review using Claude Code CLI with full tool access for thorough investigation.

Modes:
  Ticket mode (default): Reviews all commits for a coder together (per-ticket)
  Commit mode: Use --commit to review a single specific commit (with or without ticket)
  Daily mode: Use --daily to review all commits from the last N hours, bundled by author,
              and email each author with the review (designed to run as a daily cron)

Usage:
    python3 codeReviewAi.py [--dry-run] [--ticket TICKET_ID]
    python3 codeReviewAi.py --ticket TICKET_ID --commit COMMIT_HASH [--dry-run]
    python3 codeReviewAi.py --commit COMMIT_HASH [--dry-run]
    python3 codeReviewAi.py --daily [--hours 24] [--cc list@example.com] [--dry-run]
"""

import os
import sys
import re
import json
import base64
import getpass
import subprocess
import argparse
import time
import requests
from datetime import datetime, timedelta
from email.mime.text import MIMEText
from collections import defaultdict

# Configuration
REDMINE_URL = "https://redmine.gi.ucsc.edu"
GIT_REPORTS_PATH = "/hive/groups/qa/git-reports-history"
GIT_REPO_PATH = "/data/git/kent.git"
OUTPUT_DIR = f"/hive/users/{getpass.getuser()}/codeReview"
MLQ_CONF_PATH = os.path.expanduser("~/.hg.conf")
DEFAULT_CC = "browser-code-reviews-group@ucsc.edu"
DEFAULT_ALERT_EMAIL = "browserqa-group@ucsc.edu"
GMAIL_TOKEN_PATH = os.path.expanduser("~/.gmail_token.json")
GMAIL_CREDS_PATH = os.path.expanduser("~/.gmail_credentials.json")
GMAIL_SCOPES = [
    'https://www.googleapis.com/auth/gmail.send',
]
CLAUDE_CLI = os.path.expanduser('~/.local/bin/claude')
# Daily reviews get a timeout scaled to the batch size. A flat 600s limit timed
# out repeatedly on 8-11 commit days, and since the retry reused the same limit
# those authors' commits went unreviewed with no later window to catch them.
# The floor keeps small batches at the old flat 600s rather than shrinking them.
DAILY_TIMEOUT_BASE = 300
DAILY_TIMEOUT_PER_COMMIT = 120
DAILY_TIMEOUT_MIN = 600
DAILY_TIMEOUT_MAX = 2400
# A timeout is never retried on the same budget - that just burns the same wall
# clock to fail the same way.  Instead a batch that times out is split into
# per-commit reviews (which also hands the one expensive commit a budget it no
# longer shares with the rest), and a batch too small or too large to split gets
# one attempt at an escalated timeout.
TIMEOUT_ESCALATION = 2.0
SPLIT_PER_COMMIT_TIMEOUT = 600
SPLIT_MAX_COMMITS = 15
# Ceiling on one author's split.  Commits left over when it runs out are reported as
# not reviewed rather than silently dropped.
SPLIT_TOTAL_BUDGET = 3600
# Deadline for STARTING another author.  Authors are reviewed one after another, so
# without this a bad night could still be going when the next midnight cron starts, and
# the cron takes no lock.  It is a start deadline, not a ceiling: the author already
# running when it passes still finishes, so the true worst case is this plus one author's
# worst case (batch, its invalid-output retry, and the escalation).  Anyone not started is
# reported as not reviewed.
DAILY_RUN_BUDGET = 18000
# How many non-blank lines into a commit block to look for its "COMMIT n:" header.
# Bounded because this file's own source contains lines that look like the header, so a
# review OF this file used to have its whole body discarded as preamble.  Generous enough
# to clear a model that prefixed a chatty sentence, a code fence and a whole digest
# envelope before getting to the block - measured at 8 lines for that case, so a tighter
# bound stopped trimming real preambles.
PREAMBLE_SCAN_LINES = 25
# Sentinel in the split digest's footer.  Only assemble_split_digest() writes it, so it is
# how the rest of the code can tell a locally assembled digest from a model-written one.
SPLIT_FOOTER_MARKER = "reviewed individually after the batch timed out"

def scaled_timeout(num_commits):
    """Timeout in seconds for reviewing a batch of num_commits commits together.
    The floor keeps small batches at the old flat 600s rather than shrinking them."""
    return min(DAILY_TIMEOUT_MAX,
               max(DAILY_TIMEOUT_MIN,
                   DAILY_TIMEOUT_BASE + DAILY_TIMEOUT_PER_COMMIT * num_commits))

def commit_stats(commit_hash):
    """Return (files, added, removed, binary) line counts for one commit, for the
    cost log.  Commit count alone is a poor predictor of how long a review takes,
    so these are recorded next to the elapsed time to find one that fits.
    Returns zeros if git fails - this only feeds logging and must never break a
    review."""
    cmd = ['git', f'--git-dir={GIT_REPO_PATH}', 'show', '--numstat', '--format=', commit_hash]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    except Exception:
        return 0, 0, 0, 0
    if result.returncode != 0:
        return 0, 0, 0, 0
    files = added = removed = binary = 0
    for line in result.stdout.splitlines():
        fields = line.split('\t')
        if len(fields) < 3:
            continue
        files += 1
        # git writes "-" for both counts on a binary file.
        if fields[0] == '-' or fields[1] == '-':
            binary += 1
            continue
        try:
            added += int(fields[0])
            removed += int(fields[1])
        except ValueError:
            pass
    return files, added, removed, binary

def batch_stats(commits):
    """Sum commit_stats over a list of commit records."""
    files = added = removed = binary = 0
    for c in commits:
        f, a, r, b = commit_stats(c['hash'])
        files += f
        added += a
        removed += r
        binary += b
    return files, added, removed, binary

def load_config():
    """Load API keys from ~/.hg.conf"""
    config = {}
    if not os.path.exists(MLQ_CONF_PATH):
        print(f"ERROR: Config file not found: {MLQ_CONF_PATH}")
        sys.exit(1)

    with open(MLQ_CONF_PATH, 'r') as f:
        for line in f:
            line = line.strip()
            if '=' in line and not line.startswith('#'):
                key, value = line.split('=', 1)
                config[key.strip()] = value.strip()

    return config

def ensure_claude_auth():
    """Make the Claude CLI auth resilient for unattended/cron runs, where no
    login shell is sourced. Returns a short label naming the method in effect.

    Preference order (mirrors the CLI's own precedence, high to low):
      1. CLAUDE_CODE_OAUTH_TOKEN already in the environment - respected as-is.
      2. A long-lived token stored as claude.oauthToken in ~/.hg.conf - exported
         so the CLI can authenticate in cron. A setup-token value outranks the
         interactive login, so this keeps working after the local login lapses.
      3. Fall back to the local ~/.claude/.credentials.json login.

    Any residual auth failure is still caught and alerted downstream, so a
    lapsed credential surfaces loudly rather than silently."""
    if os.environ.get('CLAUDE_CODE_OAUTH_TOKEN'):
        return "CLAUDE_CODE_OAUTH_TOKEN (environment)"
    try:
        with open(MLQ_CONF_PATH, 'r') as f:
            for line in f:
                line = line.strip()
                if line.startswith('#') or '=' not in line:
                    continue
                key, value = line.split('=', 1)
                if key.strip() == 'claude.oauthToken' and value.strip():
                    os.environ['CLAUDE_CODE_OAUTH_TOKEN'] = value.strip()
                    return "CLAUDE_CODE_OAUTH_TOKEN (from ~/.hg.conf)"
    except OSError:
        pass
    return "local login (~/.claude/.credentials.json)"

def redmine_get(endpoint, api_key, params=None):
    """Make a GET request to Redmine API"""
    url = f"{REDMINE_URL}{endpoint}"
    headers = {'X-Redmine-API-Key': api_key}
    resp = requests.get(url, headers=headers, params=params)
    resp.raise_for_status()
    return resp.json()

def redmine_put(endpoint, api_key, data):
    """Make a PUT request to Redmine API"""
    url = f"{REDMINE_URL}{endpoint}"
    headers = {
        'X-Redmine-API-Key': api_key,
        'Content-Type': 'application/json'
    }
    resp = requests.put(url, headers=headers, json=data)
    return resp.status_code in (200, 204)

def parse_git_reports_url(url):
    """Convert git-reports URL to local path components"""
    match = re.search(r'/git-reports-history/([^/]+)/([^/]+)/user', url)
    if match:
        return match.group(1), match.group(2)
    return None, None

def get_open_cr_tickets(api_key):
    """Get all open code review tickets"""
    data = redmine_get('/issues.json', api_key, {
        'project_id': 'codereview',
        'status_id': 'open',
        'limit': 100
    })
    return data.get('issues', [])

def get_ticket_details(ticket_id, api_key):
    """Get full ticket details including custom fields"""
    data = redmine_get(f'/issues/{ticket_id}.json', api_key, {
        'include': 'custom_fields,journals'
    })
    return data.get('issue', {})

def get_coder_from_ticket(ticket):
    """Extract coder name from ticket custom fields"""
    for cf in ticket.get('custom_fields', []):
        if cf.get('name') == 'Coder' and cf.get('value'):
            return cf['value']
    return None

def get_git_reports_url(ticket):
    """Extract git-reports URL from ticket description"""
    desc = ticket.get('description', '')
    match = re.search(r'https://genecats\.gi\.ucsc\.edu/git-reports-history/[^\s]+', desc)
    return match.group(0) if match else None

def read_coder_commits(version, period, coder):
    """Read commits from local git-reports index.html"""
    index_path = f"{GIT_REPORTS_PATH}/{version}/{period}/user/{coder}/index.html"

    if not os.path.exists(index_path):
        return [], f"Git reports not found: {index_path}"

    with open(index_path, 'r') as f:
        html = f.read()

    commits = []

    # Match hash in <span class='details'> followed by the commit message <li>
    # This ensures we pair each hash with its actual commit message, not nested file <li>s
    # HTML structure: <span class='details'>HASH DATE <br>\n</span>\n<li>COMMIT MESSAGE
    # Note: span contains <br> tag, so we use .*? instead of [^<]*
    commit_pattern = r"<span class='details' >([a-f0-9]{40}).*?</span>\s*<li>([^<\n]+)"

    matches = re.findall(commit_pattern, html, re.DOTALL)

    for commit_hash, message in matches:
        refs = re.findall(r'#(\d+)', message)
        commits.append({
            'hash': commit_hash,
            'short_hash': commit_hash[:10],
            'message': message.strip(),
            'referenced_issues': refs
        })

    return commits, None

def get_referenced_issue(issue_id, api_key):
    """Fetch a referenced Redmine issue for context"""
    try:
        data = redmine_get(f'/issues/{issue_id}.json', api_key)
        issue = data.get('issue', {})
        return {
            'id': issue_id,
            'subject': issue.get('subject', ''),
            'description': issue.get('description', ''),
            'status': issue.get('status', {}).get('name', '')
        }
    except Exception:
        return {'id': issue_id, 'subject': 'Could not fetch', 'description': '', 'status': ''}

def get_commit_from_git(commit_hash):
    """Get commit info directly from git (for standalone commit review)"""
    try:
        result = subprocess.run(
            ['git', f'--git-dir={GIT_REPO_PATH}', 'log', '-1',
             '--format=%H%n%an%n%ae%n%ad%n%s', commit_hash],
            capture_output=True, text=True, timeout=30
        )
        if result.returncode != 0:
            return None, f"Commit not found: {commit_hash}"

        lines = result.stdout.strip().split('\n')
        if len(lines) < 5:
            return None, f"Could not parse commit: {commit_hash}"

        full_hash = lines[0]
        author = lines[1]
        message = lines[4]

        # Extract referenced issues from commit message
        refs = re.findall(r'#(\d+)', message)

        return {
            'hash': full_hash,
            'short_hash': full_hash[:10],
            'author': author,
            'message': message,
            'referenced_issues': refs
        }, None
    except Exception as e:
        return None, f"Error getting commit: {e}"

def gather_ticket_data(ticket_id, api_key):
    """Gather all data for a single ticket"""
    print(f"\n--- Gathering data for ticket #{ticket_id} ---")

    ticket = get_ticket_details(ticket_id, api_key)
    coder = get_coder_from_ticket(ticket)
    git_url = get_git_reports_url(ticket)

    if not coder:
        print(f"  WARNING: No coder found")
        return None

    if not git_url:
        print(f"  WARNING: No git-reports URL found")
        return None

    print(f"  Coder: {coder}")
    print(f"  Git reports: {git_url}")

    version, period = parse_git_reports_url(git_url)
    if not version:
        print(f"  WARNING: Could not parse git-reports URL")
        return None

    commits, error = read_coder_commits(version, period, coder)
    if error:
        print(f"  WARNING: {error}")
        return None

    print(f"  Found {len(commits)} commit(s)")

    # Collect all referenced issues
    all_refs = set()
    for c in commits:
        all_refs.update(c['referenced_issues'])

    # Fetch referenced issues
    referenced_issues = {}
    for ref_id in all_refs:
        print(f"  Fetching referenced issue #{ref_id}")
        referenced_issues[ref_id] = get_referenced_issue(ref_id, api_key)

    return {
        'ticket_id': ticket_id,
        'subject': ticket.get('subject', ''),
        'coder': coder,
        'version': version,
        'period': period,
        'commits': commits,
        'referenced_issues': referenced_issues
    }

# =============================================================================
# PER-TICKET REVIEW (Default Mode)
# =============================================================================

def build_per_ticket_prompt(ticket_data):
    """Build a prompt for reviewing all commits for one coder together"""

    # Build commits list
    commits_list = []
    for i, c in enumerate(ticket_data['commits'], 1):
        refs = ', '.join('#' + r for r in c['referenced_issues']) or 'None'
        commits_list.append(f"  {i}. @{c['short_hash']}@ - {c['message'][:70]}")
        commits_list.append(f"     Referenced issues: {refs}")

    commits_section = "\n".join(commits_list)

    # Build referenced issues context
    ref_issues_section = ""
    if ticket_data['referenced_issues']:
        ref_issues_section = "## REFERENCED ISSUES (for context)\n\n"
        for ref_id, ref in ticket_data['referenced_issues'].items():
            ref_issues_section += f"### Issue #{ref_id}: {ref['subject']}\n"
            ref_issues_section += f"Status: {ref['status']}\n"
            ref_issues_section += f"Description:\n{ref['description'][:1500]}\n\n"

    # Build the commit hashes for git commands
    commit_hashes = " ".join([c['hash'] for c in ticket_data['commits']])

    prompt = f"""You are performing a code review for UCSC Genome Browser ticket #{ticket_data['ticket_id']}.

## TICKET INFORMATION

**Ticket:** #{ticket_data['ticket_id']} - {ticket_data['subject']}
**Coder:** {ticket_data['coder']}
**Version:** {ticket_data['version']}
**Number of commits:** {len(ticket_data['commits'])}

## COMMITS TO REVIEW

{commits_section}

{ref_issues_section}

## YOUR TASK

Review ALL commits for this coder thoroughly. You have full tool access - USE IT.

### Step 1: Get the diffs for all commits

For each commit, get the full diff:
```
git --git-dir=/data/git/kent.git show <commit_hash>
```

The commit hashes are:
{commit_hashes}

### Step 2: For EACH commit, check for:

- Does the change correctly address the referenced issue(s)?
- Security issues (buffer overflows, SQL injection, XSS, command injection)
- Kent codebase patterns (freez vs freeMem, safef vs sprintf, sqlSafef)
- Logic errors, off-by-one errors, null pointer risks
- **IMPORTANT: For documentation/HTML/text changes, read the content word-by-word and check for:**
  - Typos (doubled words like "the the", wrong words like "of" vs "or")
  - Grammar errors
  - Unclosed HTML tags
  - Missing or incomplete sentences

### Step 3: Read files for context when needed

```
git --git-dir=/data/git/kent.git show HEAD:src/path/to/file.c
```

### Step 4: Check if issues still exist in HEAD

If you find any issues in a commit, verify whether they still exist in HEAD:
```
git --git-dir=/data/git/kent.git show HEAD:src/path/to/file
```
- If FIXED in HEAD (by a later commit in this review), note: "Issue found but FIXED in later commit" → APPROVED
- If STILL EXISTS in HEAD → FEEDBACK required

### Step 5: Look for cross-commit patterns

Since you're reviewing all commits together, note:
- Consistent good practices (or bad practices) across commits
- How commits relate to each other
- Whether earlier commits' issues are fixed in later commits

## OUTPUT FORMAT

Provide your review in Redmine Textile format. IMPORTANT Textile syntax rules:
- Inline code uses @code@ - ALWAYS close with a second @, never leave @ unclosed
- Don't start lines with spaces (creates unwanted code blocks)
- Use @short_hash@ for commit hashes, not backticks
- Headers: h1. h2. h3. (with period and space)
- Bold: *text* | Italic: _text_
- Tables: |_. header |_. header | then | cell | cell |

Format:

```
h1. Code Review: Ticket #{ticket_data['ticket_id']} - {ticket_data['subject']}

*Coder:* {ticket_data['coder']}
*Review Date:* {datetime.now().strftime('%Y-%m-%d')}
*Redmine Ticket:* #{ticket_data['ticket_id']}

---

h2. Summary

[Number] commits reviewed: [brief description of what these commits do overall]

|_. # |_. Commit |_. Issue |_. Description |
[Table rows for each commit]

---

h2. Commit 1: [short_hash] - [Brief Title]

*Message:* [commit message]
*Referenced Issues:* [issues]

*Files Changed:*
[List files]

*Analysis:*
[Your detailed analysis]

*Issues Found:*
[List issues or "None". Indicate if issues still exist in HEAD or were fixed in later commits.]

*Verified:* [Yes/No/Partial] - Does change address referenced issue(s)?

h3. Verdict: <write APPROVED or write FEEDBACK, not both>

---

[Repeat for each commit]

---

h2. Cross-Commit Observations

[Note any patterns, relationships between commits, or overall code quality observations]

---

h2. Risk Assessment

|_. Area |_. Risk |_. Notes |
| Security | Low/Med/High | [explanation] |
| Regression | Low/Med/High | [explanation] |

---

h2. Final Recommendation

h3. Status: <write APPROVED or write FEEDBACK, not both>

[Summary. If FEEDBACK, list all items that need to be addressed before approval.]

---

_Review: {datetime.now().strftime('%Y-%m-%d')} | Commits: {len(ticket_data['commits'])} | Per-ticket review with full tool access_
```

IMPORTANT:
- Give FEEDBACK only if issues STILL EXIST in HEAD
- Give APPROVED if all issues were fixed in later commits (note this in your review)
- Be thorough - check every commit, read every diff

OUTPUT REQUIREMENTS:
- You MUST output the COMPLETE review in Textile format as shown above
- Start your output with "h1. Code Review:" - no preamble text before this
- Include ALL sections: Summary, each Commit analysis, Cross-Commit Observations, Risk Assessment, Final Recommendation
- Do NOT output just a summary sentence - output the FULL FORMATTED REVIEW
- Do NOT say "The review is complete" - instead output the actual review content

BEGIN YOUR REVIEW NOW. Use your tools to investigate thoroughly, then output the COMPLETE formatted review starting with "h1. Code Review:"
"""
    return prompt

def validate_review_output(response):
    """Check if the response contains a valid Textile-formatted review"""
    if not response:
        return False, "Empty response"

    # Must contain h1. header
    if 'h1.' not in response:
        return False, "Missing h1. header - Claude may have returned a summary instead of full review"

    # Must contain a verdict
    if 'Verdict:' not in response and 'APPROVED' not in response and 'FEEDBACK' not in response:
        return False, "Missing verdict section"

    # Should be reasonably long (at least 500 chars for a minimal review)
    if len(response) < 500:
        return False, f"Response too short ({len(response)} chars) - may be incomplete"

    return True, "OK"

def validate_daily_review_output(response):
    """Check if the response contains a valid daily review in plain text format"""
    if not response:
        return False, "Empty response"

    # Matched through undecorate_line: a raw substring test rejected a header the model
    # wrote as "Daily Code Review - Bob" or "**DAILY CODE REVIEW - Bob**", throwing away a
    # complete review and alerting the maintainer with a misleading reason.
    if not any(undecorate_line(l).upper().startswith('DAILY CODE REVIEW')
               for l in response.splitlines()):
        return False, "Missing DAILY CODE REVIEW header"

    if 'APPROVED' not in response.upper() and 'FEEDBACK' not in response.upper():
        return False, "Missing verdict section"

    if len(response) < 500:
        return False, f"Response too short ({len(response)} chars) - may be incomplete"

    return True, "OK"

# Every place this file recognises a line by its text has to cope with the model
# decorating that line with markdown - "**Verdict:** APPROVED", "## SUMMARY",
# "- OVERALL STATUS: FEEDBACK".  Several rounds of review found the same bug in matcher
# after matcher, so the decoration is stripped by one of the two functions below and every
# matcher compares against the result.
#
# WHICH ONE TO USE IS A REAL INVARIANT, and picking wrong is how the last regression got
# in:
#   * a pattern with a VALUE to check (OVERALL STATUS: X, Verdict: X, Review Date: <date>)
#     takes normalize_marker_line - the value is what proves it is the envelope, so peeling
#     decoration around it is safe;
#   * a pattern with NO value (a bare SUMMARY heading, the COMMIT n: anchor) takes
#     undecorate_line and must additionally sit at column 0 - see that function.
#
# Both are for MATCHING ONLY.  When a line is kept the original is kept, never the
# normalized form.  The one deliberate exception is the renumbered COMMIT header, which is
# rewritten from the undecorated text on purpose and says so locally.
def normalize_marker_line(line):
    """Strip markdown decoration so a line can be compared against what it meant to say."""
    s = line.strip()
    # Peel leading blockquote, heading and bullet markers in any order or combination
    # ("> ", "##", "- ", "> - "). Looping rather than one pattern so a bare "> " with no
    # bullet after it is handled too.
    # '+' is deliberately NOT peeled: in this file's world a leading '+' is a diff
    # marker, not emphasis, and peeling it meant "+OVERALL STATUS: FEEDBACK" quoted from
    # a hunk was treated as an envelope line and deleted, while the '-' side of the same
    # hunk survived - so a review of a diff showed the author only half of it.
    # 'h3. ' is Textile's heading marker.  The ticket and standalone reviews are Textile,
    # so their verdict line is "h3. Verdict: APPROVED"; without peeling it, every approved
    # Textile review read as having no verdict at all.
    previous = None
    while previous != s:
        previous = s
        s = re.sub(r'^\s*(?:>+|[*#]+|h[1-6]\.(?=\s)|[-*](?=\s)|\d+[.)](?=\s))\s*', '', s)
    s = s.replace('**', '').replace('__', '')       # emphasis pairs anywhere
    s = s.strip().strip('*_`').strip()
    return re.sub(r'\s+', ' ', s)                   # collapse runs of whitespace

def undecorate_line(line):
    """Like normalize_marker_line but WITHOUT peeling bullets, blockquotes or numbers.

    For the envelope patterns that have no value to check - a bare 'SUMMARY' heading and
    the 'COMMIT n:' anchor.  Peeling decoration off those turns "is this the envelope?"
    into "does any bullet in this review happen to say SUMMARY?", and a review that listed
    the digest's own section names as bullets lost every one of them:

        The digest is built from these sections, in order:
        - SUMMARY
        - DETAILED REVIEW
        and only the last one is conditional.

    A heading the model decorated as '## SUMMARY' therefore survives into a block. That is
    cosmetic, and any-FEEDBACK-wins means it cannot change who gets emailed."""
    s = line.strip().replace('**', '').replace('__', '').strip()
    # Only a WRAPPING emphasis pair comes off.  Stripping any leading marker would undo the
    # point of this function: '* SUMMARY' is a bullet in a prose list, not a heading.
    for char in ('*', '_', '`'):
        while len(s) >= 2 and s.startswith(char) and s.endswith(char):
            s = s[1:-1].strip()
    return re.sub(r'\s+', ' ', s)

# The label may carry leftover emphasis on either side of the colon once the pairs are
# gone: "*Verdict:* APPROVED", "*Verdict*: APPROVED", "`Verdict:` APPROVED".  Matching a
# bare "^Verdict\s*:" instead read those as having no verdict at all, which reported an
# approved commit as "did not state a clear verdict" - or, when the emphasis sat before the
# colon, failed validation outright and threw a complete review away as truncated.
VERDICT_LINE_RE = r'^Verdict\s*[*_`]*\s*:\s*(.+)$'

def verdict_value(text):
    """The APPROVED/FEEDBACK decision a verdict value states, or None.

    Reads the first alphabetic word rather than requiring the value to begin with one, so
    quoting and emphasis the model adds around it ("*APPROVED*", '"APPROVED"',
    "(APPROVED)") do not lose the decision.  Anything else is None, which callers treat as
    "no clear verdict" and resolve toward telling a human."""
    if STATUS_PLACEHOLDER_RE.search(text.strip(' *_`"\'()[]')):
        return None
    word = re.search(r'[A-Za-z]+', text)
    if not word:
        return None
    upper = word.group(0).upper()
    return upper if upper in ('APPROVED', 'FEEDBACK') else None

def validate_daily_commit_output(response):
    """Check one commit's block from a split review.  Deliberately does NOT look for
    the DAILY CODE REVIEW header: this path asks for a single commit block, and the
    surrounding digest is assembled locally rather than by the model.  Requiring the
    Verdict line is what catches a block that was cut off part way through."""
    if not response:
        return False, "Empty response"

    if not any(re.match(VERDICT_LINE_RE, normalize_marker_line(l), re.IGNORECASE)
               for l in response.splitlines()):
        return False, "Missing Verdict: line - block may be truncated"

    # Case-insensitive, like the sibling check in validate_daily_review_output: a block
    # whose line read "Verdict: approved" was rejected here and reported to the author as
    # [NOT REVIEWED], even though verdict_value() one call later reads it fine.
    if 'APPROVED' not in response.upper() and 'FEEDBACK' not in response.upper():
        return False, "Missing verdict section"

    # Lower floor than the full digest: a one-file commit can be reviewed briefly.
    if len(response) < 200:
        return False, f"Response too short ({len(response)} chars) - may be incomplete"

    return True, "OK"

# Substrings that indicate the Claude CLI could not authenticate or otherwise
# failed to produce a usable review. Used to alert instead of failing silently.
CLI_AUTH_ERROR_MARKERS = (
    'Failed to authenticate',
    'authentication_error',
    'Invalid authentication credentials',
    'API Error: 401',
)

def detect_cli_failure(response, validator):
    """Return an error description if the CLI response indicates a hard failure
    (no output, an authentication error, or output that fails validation),
    otherwise return None. Lets the caller alert rather than silently save a
    broken review."""
    if not response:
        return "No response from Claude CLI (timeout, crash, or empty output)"
    # A well-formed review is a success even when its text quotes auth-error
    # strings - e.g. a review of the auth-handling code itself. Only scan for
    # auth markers when the output is NOT a valid review, so such reviews are
    # not misflagged as authentication failures.
    is_valid, msg = validator(response)
    if is_valid:
        return None
    for marker in CLI_AUTH_ERROR_MARKERS:
        if marker in response:
            first_line = next((l for l in response.strip().splitlines() if l.strip()), response)
            return f"Claude CLI authentication failure: {first_line.strip()[:300]}"
    return f"Invalid or incomplete review output: {msg}"

def call_claude_cli(prompt, timeout=600, retries=1, validator=None, diag=None):
    """Call Claude Code CLI with a prompt and return the response.

    A timeout is NOT retried.  The prompt hands the CLI a set of commit hashes and
    lets it fetch the diffs itself, so a run that ran out of time will do the same
    work again and run out of time again - retrying only doubles the wall clock.
    'retries' still governs invalid or incomplete output, which a re-run genuinely
    can fix.  Callers that want a second, longer attempt do it themselves with a
    bigger timeout, because only the caller knows whether splitting the work is an
    option instead.

    If 'diag' is a dict it is filled in with 'timed_out', 'elapsed' and 'attempts'.
    Note 'elapsed' covers every attempt, so compare it against 'attempts' before
    reading it as the cost of one run."""
    if validator is None:
        validator = validate_review_output
    if diag is None:
        diag = {}
    diag.setdefault('timed_out', False)
    diag.setdefault('elapsed', 0.0)
    diag.setdefault('attempts', 0)

    started = time.time()
    try:
        for attempt in range(retries + 1):
            diag['attempts'] = attempt + 1
            try:
                result = subprocess.run(
                    [CLAUDE_CLI, '-p', prompt, '--output-format', 'text',
                     '--allowedTools', 'Bash,Read,Glob,Grep,Agent'],
                    capture_output=True,
                    text=True,
                    timeout=timeout
                )

                if result.returncode != 0:
                    print(f"  WARNING: Claude CLI returned non-zero: {result.returncode}")
                    if result.stderr:
                        print(f"  stderr: {result.stderr[:500]}")

                response = result.stdout

                # Check if response is valid (not empty/trivial)
                is_valid, msg = validator(response)
                if not is_valid and attempt < retries:
                    print(f"  WARNING: Invalid response ({msg}) - retrying...")
                    continue

                return response

            except subprocess.TimeoutExpired:
                # A timeout leaves no evidence behind, which makes it the one failure
                # nobody can diagnose: we cannot tell whether the run was converging
                # and merely needed longer, or was stuck pulling more and more into
                # context (10 new PNGs in one commit is the suspected case) where a
                # bigger budget only buys a later failure.
                #
                # Capturing TimeoutExpired.stdout does NOT recover it.  That was tried
                # and measured: with '--output-format text' the CLI buffers and writes
                # nothing until it finishes, so both e.stdout and e.stderr come back
                # empty (verified at timeout=25s against a real per-commit prompt).
                #
                # The way to get partial output is '--output-format stream-json', which
                # emits one NDJSON event per turn as the run proceeds - tool calls,
                # their results, and assistant text.  Tailing that stream would show
                # exactly which files a stuck run kept reading.  It is not done here
                # because it changes the success path for every mode: the response is
                # no longer plain text on stdout, so each caller would have to parse
                # NDJSON and concatenate the assistant text events (and '--verbose' is
                # required alongside it for the per-turn detail to appear).  Worth doing
                # as its own change, with the validators updated to match.
                print(f"  WARNING: Claude CLI timed out after {timeout}s")
                diag['timed_out'] = True
                return None
            except Exception as e:
                print(f"  ERROR calling Claude CLI: {e}")
                if attempt < retries:
                    print(f"  Retrying...")
                    continue
                return None

        return None
    finally:
        diag['elapsed'] = time.time() - started

def clean_review_output(response):
    """Strip any preamble before the actual Textile review content"""
    if not response:
        return response

    # First validate we have a proper review
    is_valid, msg = validate_review_output(response)
    if not is_valid:
        print(f"  WARNING: Invalid review output - {msg}")
        # Return as-is with a warning header so it's obvious something went wrong
        return f"h1. Code Review - ERROR\n\n*Warning: Review generation may have failed.*\n\nClaude's response:\n{response}\n"

    # Find where the actual review starts (h1. header)
    # Look for patterns like "h1. Code Review" or just "h1."
    patterns = [
        r'^h1\. Code Review',
        r'^h1\.',
        r'^\s*h1\. Code Review',
        r'^\s*h1\.',
    ]

    for pattern in patterns:
        match = re.search(pattern, response, re.MULTILINE)
        if match:
            response = response[match.start():]
            break
    else:
        # If no h1. found, try to find the start of Textile markup
        lines = response.split('\n')
        for i, line in enumerate(lines):
            if line.strip().startswith('h1.') or line.strip().startswith('h2.'):
                response = '\n'.join(lines[i:])
                break

    # Fix common Textile formatting issues
    response = fix_textile_formatting(response)

    return response

def fix_textile_formatting(text):
    """Fix common Textile formatting issues that break Redmine rendering"""
    if not text:
        return text

    # Fix 1: Ensure @ symbols for inline code are properly paired
    # Count @ symbols per line and fix unclosed ones
    lines = text.split('\n')
    fixed_lines = []

    for line in lines:
        # Count @ symbols (excluding @@)
        # Replace @@ temporarily to not count them
        temp_line = line.replace('@@', '\x00\x00')
        at_count = temp_line.count('@')

        # If odd number of @, there's an unclosed one
        if at_count % 2 == 1:
            # Try to find the unclosed @ and close it or remove it
            # Common case: "@something" at end of line without closing
            # Add closing @ at end of the word
            fixed_line = re.sub(r'@(\w+)(?!\w*@)', r'@\1@', line)
            # If that didn't fix it, try to escape just the lone @ at end
            temp_fixed = fixed_line.replace('@@', '\x00\x00')
            if temp_fixed.count('@') % 2 == 1:
                # Still odd - find and escape only the lone @ (likely at end of line)
                # Look for @ not followed by a word char, or @ at end of line
                fixed_line = re.sub(r'@(?=\s|$|[^\w])', '&#64;', fixed_line)
                # If STILL odd (edge case), escape just the last @
                temp_fixed2 = fixed_line.replace('@@', '\x00\x00')
                if temp_fixed2.count('@') % 2 == 1:
                    # Find last @ and escape it
                    last_at = fixed_line.rfind('@')
                    if last_at >= 0:
                        fixed_line = fixed_line[:last_at] + '&#64;' + fixed_line[last_at+1:]
            line = fixed_line

        fixed_lines.append(line)

    text = '\n'.join(fixed_lines)

    # Fix 2: Remove leading spaces from lines that shouldn't be code blocks
    # (lines starting with h1., h2., h3., |, *, -, etc. shouldn't have leading spaces)
    lines = text.split('\n')
    fixed_lines = []
    for line in lines:
        stripped = line.lstrip()
        # If it's a Textile formatting line, remove leading whitespace
        if re.match(r'^(h[1-6]\.|[|*#-]|\*\*|_)', stripped):
            line = stripped
        fixed_lines.append(line)

    text = '\n'.join(fixed_lines)

    return text

def review_ticket_per_ticket(ticket_data):
    """Review all commits for a ticket together (default mode)"""
    print(f"\n{'='*60}")
    print(f"REVIEWING TICKET #{ticket_data['ticket_id']}: {ticket_data['coder']}")
    print(f"Commits: {len(ticket_data['commits'])}")
    print(f"Mode: Per-ticket (all commits together)")
    print(f"{'='*60}")

    prompt = build_per_ticket_prompt(ticket_data)

    # Save prompt for debugging
    prompt_file = os.path.join(OUTPUT_DIR, f".last_ticket_prompt_{ticket_data['ticket_id']}.txt")
    with open(prompt_file, 'w') as f:
        f.write(prompt)
    print(f"  Prompt saved to: {prompt_file}")

    # This path reviews every commit on the ticket in one call, so it needs the same
    # size-scaled budget the daily path got - it was left on a flat 600s, which meant
    # a ten-commit ticket had no more time than a one-commit ticket.
    timeout = scaled_timeout(len(ticket_data['commits']))
    print(f"  Calling Claude CLI (timeout {timeout}s for "
          f"{len(ticket_data['commits'])} commit(s))...")
    diag = {}
    response = call_claude_cli(prompt, timeout=timeout, diag=diag)
    print(f"  COST elapsed={diag.get('elapsed', 0):.0f}s timeout={timeout}s "
          f"attempts={diag.get('attempts', 0)} "
          f"commits={len(ticket_data['commits'])} "
          f"timedOut={diag.get('timed_out', False)}")

    if diag.get('timed_out'):
        # retries=0: at this size a further invalid-output re-run would double an
        # already large budget. This path is not split - a CR ticket's review is one
        # Textile document with its own envelope, so splitting it needs a second
        # assembler and is left for a follow-up change.
        retry_timeout = int(timeout * TIMEOUT_ESCALATION)
        print(f"  Retrying once with an escalated timeout ({retry_timeout}s)...")
        retry_diag = {}
        response = call_claude_cli(prompt, timeout=retry_timeout, retries=0,
                                   diag=retry_diag)
        print(f"  COST elapsed={retry_diag.get('elapsed', 0):.0f}s "
              f"timeout={retry_timeout}s attempts={retry_diag.get('attempts', 0)} "
              f"escalated=True timedOut={retry_diag.get('timed_out', False)}")

    if response:
        # Save raw response for debugging
        response_file = os.path.join(OUTPUT_DIR, f".last_ticket_response_{ticket_data['ticket_id']}.txt")
        with open(response_file, 'w') as f:
            f.write(response)
        # Clean up any preamble
        response = clean_review_output(response)
        print(f"  Review complete")
    else:
        print(f"  WARNING: No response received")
        response = f"h1. Code Review: Ticket #{ticket_data['ticket_id']}\n\n*Error: Review failed - no response from Claude CLI*\n"

    return response

# =============================================================================
# PER-COMMIT REVIEW (when --commit is specified)
# =============================================================================

def build_single_commit_prompt(commit, ticket_data):
    """Build a prompt for reviewing a single specific commit"""

    # Get referenced issue context for this commit
    ref_context = ""
    for ref_id in commit['referenced_issues']:
        if ref_id in ticket_data['referenced_issues']:
            ref = ticket_data['referenced_issues'][ref_id]
            ref_context += f"""
### Referenced Issue #{ref_id}: {ref['subject']}
Status: {ref['status']}
Description:
{ref['description'][:2000]}
"""

    prompt = f"""You are reviewing a SINGLE commit for UCSC Genome Browser code review ticket #{ticket_data['ticket_id']}.

## COMMIT INFORMATION

**Commit:** {commit['hash']}
**Coder:** {ticket_data['coder']}
**Message:** {commit['message']}
**Referenced Issues:** {', '.join('#' + r for r in commit['referenced_issues']) or 'None'}

{ref_context}

## YOUR TASK

Review this ONE commit thoroughly. You have full tool access - USE IT.

1. **Get the full diff:**
   ```
   git --git-dir=/data/git/kent.git show {commit['hash']}
   ```

2. **Read any modified files in full** if needed for context:
   ```
   git --git-dir=/data/git/kent.git show HEAD:src/path/to/file.c
   ```

3. **Check for:**
   - Does the change correctly address the referenced issue(s)?
   - Security issues (buffer overflows, SQL injection, XSS, command injection)
   - Kent codebase patterns (freez vs freeMem, safef vs sprintf, sqlSafef)
   - Logic errors, off-by-one errors, null pointer risks
   - **IMPORTANT: For documentation/HTML/text changes, read the content word-by-word and check for:**
     - Typos (doubled words like "the the", wrong words like "of" vs "or")
     - Grammar errors
     - Unclosed HTML tags
     - Missing or incomplete sentences

4. **Investigate** any uncertainties using git grep, git blame, or reading related files.

5. **Check if issues still exist in HEAD:**
   If you find any issues, check if they still exist in the current HEAD version:
   ```
   git --git-dir=/data/git/kent.git show HEAD:src/path/to/file
   ```
   - If FIXED in HEAD, note: "Issue found but FIXED in later commit" → APPROVED
   - If STILL EXISTS in HEAD → FEEDBACK required

## OUTPUT FORMAT

Provide your review in Redmine Textile format. IMPORTANT Textile syntax rules:
- Inline code uses @code@ - ALWAYS close with a second @, never leave @ unclosed
- Don't start lines with spaces (creates unwanted code blocks)
- Use @short_hash@ for commit hashes, not backticks
- Headers: h1. h2. h3. (with period and space)
- Bold: *text* | Italic: _text_

Format:

```
h1. Code Review: Commit {commit['short_hash']}

*Coder:* {ticket_data['coder']}
*Review Date:* {datetime.now().strftime('%Y-%m-%d')}
*Ticket:* #{ticket_data['ticket_id']}

---

h2. Commit: {commit['short_hash']} - Brief Title

*Message:* {commit['message']}
*Referenced Issues:* {', '.join('#' + r for r in commit['referenced_issues']) or 'None'}

*Files Changed:*
[List the files modified]

*Analysis:*
[Your detailed analysis of the changes. Be specific about what you found.]

*Issues Found:*
[List any issues, or "None". Indicate if issues still exist in HEAD or were fixed.]

*Verified:* [Yes/No/Partial] - Does change correctly address referenced issue(s)?

h3. Verdict: <write APPROVED or write FEEDBACK, not both>

[If FEEDBACK, explain exactly what needs to be fixed]

---

_Review: {datetime.now().strftime('%Y-%m-%d')} | Single commit review_
```

OUTPUT REQUIREMENTS:
- You MUST output the COMPLETE review in Textile format as shown above
- Start your output with "h1. Code Review:" - no preamble text before this
- Include ALL sections: Commit analysis, Issues Found, Verdict
- Do NOT output just a summary sentence - output the FULL FORMATTED REVIEW
- Do NOT say "The review is complete" - instead output the actual review content

BEGIN YOUR REVIEW NOW. Use your tools to investigate thoroughly, then output the COMPLETE formatted review starting with "h1. Code Review:"
"""
    return prompt

def review_single_commit(commit, ticket_data):
    """Review a single specific commit"""
    print(f"\n{'='*60}")
    print(f"REVIEWING SINGLE COMMIT: {commit['short_hash']}")
    print(f"Ticket: #{ticket_data['ticket_id']} | Coder: {ticket_data['coder']}")
    print(f"Mode: Single commit")
    print(f"{'='*60}")

    prompt = build_single_commit_prompt(commit, ticket_data)

    # Save prompt for debugging
    prompt_file = os.path.join(OUTPUT_DIR, f".last_commit_prompt_{commit['short_hash']}.txt")
    with open(prompt_file, 'w') as f:
        f.write(prompt)
    print(f"  Prompt saved to: {prompt_file}")
    print(f"  Calling Claude CLI...")

    response = call_claude_cli(prompt, timeout=300)

    if response:
        response_file = os.path.join(OUTPUT_DIR, f".last_commit_response_{commit['short_hash']}.txt")
        with open(response_file, 'w') as f:
            f.write(response)
        # Clean up any preamble
        response = clean_review_output(response)
        print(f"  Review complete")
    else:
        print(f"  WARNING: No response received")
        response = f"h1. Code Review: Commit {commit['short_hash']}\n\n*Error: Review failed - no response from Claude CLI*\n"

    return response

# =============================================================================
# STANDALONE COMMIT REVIEW (when only --commit is specified, no ticket)
# =============================================================================

def build_standalone_commit_prompt(commit, referenced_issues):
    """Build a prompt for reviewing a commit without ticket context"""

    # Get referenced issue context
    ref_context = ""
    for ref_id in commit['referenced_issues']:
        if ref_id in referenced_issues:
            ref = referenced_issues[ref_id]
            ref_context += f"""
### Referenced Issue #{ref_id}: {ref['subject']}
Status: {ref['status']}
Description:
{ref['description'][:2000]}
"""

    prompt = f"""You are reviewing a commit for the UCSC Genome Browser project.

## COMMIT INFORMATION

**Commit:** {commit['hash']}
**Author:** {commit['author']}
**Message:** {commit['message']}
**Referenced Issues:** {', '.join('#' + r for r in commit['referenced_issues']) or 'None'}

{ref_context}

## RESOURCES AVAILABLE

You have full tool access. USE THESE RESOURCES:

### Central Git Repository
- Path: `/data/git/kent.git`
- Usage: `git --git-dir=/data/git/kent.git <command>`
- Examples:
  - `git --git-dir=/data/git/kent.git show {commit['hash']}` - full commit diff
  - `git --git-dir=/data/git/kent.git show HEAD:src/path/file.c` - read current file
  - `git --git-dir=/data/git/kent.git log --oneline -20 -- src/path/file.c` - file history
  - `git --git-dir=/data/git/kent.git grep "pattern"` - search entire codebase
  - `git --git-dir=/data/git/kent.git blame src/path/file.c -L 100,120` - who wrote each line

### Kent Codebase Conventions
- Reference: `~/kent/src/README` - contains code conventions, indentation standards, source tree organization

## YOUR TASK

Review this commit thoroughly. **NEVER speculate about code you haven't read.**

### Step 1: Get the full diff
```
git --git-dir=/data/git/kent.git show {commit['hash']}
```

### Step 2: Read modified files for full context
```
git --git-dir=/data/git/kent.git show HEAD:src/path/to/file.c
```

### Step 3: Check for security issues (C code)

**Buffer overflow risks:**
- `gets()` → ALWAYS vulnerable, must use `fgets()`
- `strcpy()`, `strcat()` → use `safecpy()`, kent safe equivalents
- `sprintf()`, `vsprintf()` → use `safef()` or `snprintf()`
- Check all array indexing for bounds validation
- Watch for off-by-one errors (using `<=` instead of `<`)

**Format string vulnerabilities:**
- `printf(userInput)` → NEVER pass user input as format string
- Always use `printf("%s", userInput)` pattern

**Memory safety:**
- Use-after-free: check freed pointers aren't used later
- Double-free: ensure memory isn't freed twice
- Memory leaks: allocated memory should be freed on all paths

**Command/SQL injection:**
- `system()`, `popen()`, `exec*()` → command injection risk
- SQL queries → must use `sqlSafef()`, never string concatenation
- User input in file paths → path traversal risk (check for `..`)

**Web output:**
- HTML output → XSS risk, ensure proper escaping
- URL parameters → validate and sanitize

### Step 4: Check kent codebase patterns

| Unsafe | Safe Kent Equivalent |
|--------|---------------------|
| sprintf | safef |
| strcpy | safecpy |
| strcat | safecat |
| malloc/free | needMem/freez |
| freeMem | freez (sets pointer to NULL) |
| SQL string concat | sqlSafef |

### Step 5: Check for bugs and logic errors
- Logic errors, off-by-one errors
- Null pointer risks (check return values)
- Unclosed tags/brackets
- Typos in strings or variable names

### Step 6: For documentation/HTML/text changes
**Read the content word-by-word and check for:**
- Typos (doubled words like "the the", wrong words like "of" vs "or")
- Grammar errors
- Unclosed HTML tags
- Missing or incomplete sentences
- Documentation changes are NOT low-effort reviews - text quality matters

### Step 7: Investigate when needed

- **Unfamiliar function** → Read its implementation
- **Wondering if pattern exists elsewhere** → `git grep "functionName"`
- **Need to see how something is used** → `git grep "functionCall("`
- **Understanding existing code** → `git blame` to see who wrote it and when

### Step 8: Check if issues still exist in HEAD
If you find any issues, verify whether they still exist:
```
git --git-dir=/data/git/kent.git show HEAD:src/path/to/file
```
- If FIXED in HEAD → note "Issue found but FIXED in later commit" → APPROVED
- If STILL EXISTS in HEAD → FEEDBACK required

## OUTPUT FORMAT

Provide your review in Redmine Textile format. IMPORTANT Textile syntax rules:
- Inline code uses @code@ - ALWAYS close with a second @, never leave @ unclosed
- Don't start lines with spaces (creates unwanted code blocks)
- Use @short_hash@ for commit hashes, not backticks
- Headers: h1. h2. h3. (with period and space)
- Bold: *text* | Italic: _text_

Format:

```
h1. Code Review: Commit {commit['short_hash']}

*Author:* {commit['author']}
*Review Date:* {datetime.now().strftime('%Y-%m-%d')}

---

h2. Commit: {commit['short_hash']} - Brief Title

*Message:* {commit['message']}
*Referenced Issues:* {', '.join('#' + r for r in commit['referenced_issues']) or 'None'}

*Files Changed:*
[List the files modified]

*Analysis:*
[Your detailed analysis of the changes. Be specific about what you found.]

*Issues Found:*
[List any issues, or "None". Indicate if issues still exist in HEAD or were fixed.]

*Verified:* [Yes/No/Partial] - Does change correctly address referenced issue(s)?

h3. Verdict: <write APPROVED or write FEEDBACK, not both>

[If FEEDBACK, explain exactly what needs to be fixed]

---

_Review: {datetime.now().strftime('%Y-%m-%d')} | Standalone commit review_
```

## VERDICT GUIDELINES

**Give APPROVED when:**
- Change correctly addresses its stated purpose
- No security vulnerabilities found
- No bugs that would affect users
- Code follows kent patterns (minor deviations okay with note)

**Give FEEDBACK when:**
- Security vulnerability present
- Bug that would cause incorrect behavior or crash
- Typos or grammar errors in documentation/user-facing text
- Missing required error handling

OUTPUT REQUIREMENTS:
- You MUST output the COMPLETE review in Textile format as shown above
- Start your output with "h1. Code Review:" - no preamble text before this
- Include ALL sections: Commit analysis, Issues Found, Verdict
- Do NOT output just a summary sentence - output the FULL FORMATTED REVIEW
- Do NOT say "The review is complete" - instead output the actual review content

BEGIN YOUR REVIEW NOW. Use your tools to investigate thoroughly, then output the COMPLETE formatted review starting with "h1. Code Review:"
"""
    return prompt

def review_standalone_commit(commit, referenced_issues):
    """Review a commit without ticket context"""
    print(f"\n{'='*60}")
    print(f"REVIEWING COMMIT: {commit['short_hash']}")
    print(f"Author: {commit['author']}")
    print(f"Mode: Standalone commit (no ticket)")
    print(f"{'='*60}")

    prompt = build_standalone_commit_prompt(commit, referenced_issues)

    # Save prompt for debugging
    prompt_file = os.path.join(OUTPUT_DIR, f".last_commit_prompt_{commit['short_hash']}.txt")
    with open(prompt_file, 'w') as f:
        f.write(prompt)
    print(f"  Prompt saved to: {prompt_file}")
    print(f"  Calling Claude CLI...")

    response = call_claude_cli(prompt, timeout=300)

    if response:
        response_file = os.path.join(OUTPUT_DIR, f".last_commit_response_{commit['short_hash']}.txt")
        with open(response_file, 'w') as f:
            f.write(response)
        # Clean up any preamble
        response = clean_review_output(response)
        print(f"  Review complete")
    else:
        print(f"  WARNING: No response received")
        response = f"h1. Code Review: Commit {commit['short_hash']}\n\n*Error: Review failed - no response from Claude CLI*\n"

    return response

# =============================================================================
# DAILY REVIEW MODE (when --daily is specified)
# =============================================================================

def get_commits_since(hours, since=None, until=None):
    """Get commits grouped by author. By default covers the last N hours; if
    'since' (and optionally 'until') are given, covers that explicit window
    instead. 'since'/'until' are passed straight to git log, so any date git
    understands works (e.g. '2026-06-20' or '2026-06-20 00:00:00')."""
    since_str = since or (datetime.now() - timedelta(hours=hours)).strftime('%Y-%m-%d %H:%M:%S')

    # Get commits with author name, email, hash, and subject
    cmd = ['git', f'--git-dir={GIT_REPO_PATH}', 'log',
           f'--since={since_str}', '--format=%H%n%an%n%ae%n%s', '--no-merges']
    if until:
        cmd.append(f'--until={until}')
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    if result.returncode != 0:
        print(f"ERROR: git log failed: {result.stderr}")
        return {}

    lines = result.stdout.strip().split('\n')
    if not lines or lines == ['']:
        return {}

    # Parse into commit records, grouped by author email
    authors = defaultdict(lambda: {'name': '', 'email': '', 'commits': []})
    i = 0
    while i + 3 < len(lines):
        commit_hash = lines[i]
        author_name = lines[i + 1]
        author_email = lines[i + 2]
        message = lines[i + 3]
        i += 4

        # Skip blank separator lines between records
        while i < len(lines) and lines[i] == '':
            i += 1

        refs = re.findall(r'#(\d+)', message)
        authors[author_email]['name'] = author_name
        authors[author_email]['email'] = author_email
        authors[author_email]['commits'].append({
            'hash': commit_hash,
            'short_hash': commit_hash[:10],
            'message': message.strip(),
            'referenced_issues': refs
        })

    return dict(authors)


def build_daily_review_prompt(author_name, commits, window_label="the last 24 hours"):
    """Build a prompt for reviewing all of one author's commits in a window"""

    commits_list = []
    commit_hashes = []
    for i, c in enumerate(commits, 1):
        refs = ', '.join('#' + r for r in c['referenced_issues']) or 'None'
        commits_list.append(f"  {i}. @{c['short_hash']}@ - {c['message'][:80]}")
        commits_list.append(f"     Referenced issues: {refs}")
        commit_hashes.append(c['hash'])

    commits_section = "\n".join(commits_list)
    hashes_section = " ".join(commit_hashes)

    prompt = f"""You are performing a daily code review of all commits by {author_name} in the UCSC Genome Browser kent repository from {window_label}.

## AUTHOR: {author_name}
## COMMITS TO REVIEW ({len(commits)} total)

{commits_section}

## YOUR TASK

Review ALL commits by this author. You have full tool access - USE IT.

### Step 1: Get the diffs for all commits

For each commit, get the full diff:
```
git --git-dir=/data/git/kent.git show <commit_hash>
```

The commit hashes are:
{hashes_section}

### Step 2: For EACH commit, check for:

- Does the change correctly address the referenced issue(s)?
- Security issues (buffer overflows, SQL injection, XSS, command injection)
- Kent codebase patterns (freez vs freeMem, safef vs sprintf, sqlSafef)
- Logic errors, off-by-one errors, null pointer risks
- **IMPORTANT: For documentation/HTML/text changes, read the content word-by-word and check for:**
  - Typos (doubled words like "the the", wrong words like "of" vs "or")
  - Grammar errors
  - Unclosed HTML tags
  - Missing or incomplete sentences

### Step 3: Read files for context when needed

```
git --git-dir=/data/git/kent.git show HEAD:src/path/to/file.c
```

Do NOT read binary files or images (.png, .jpg, .gif, .ico, .pdf, .bb, .bw, .2bit).
Judge those from the diff stat alone - loading them wastes the whole time budget.

### Step 4: Look for cross-commit patterns

Since you're reviewing all commits by the same author, note:
- Consistent good practices (or bad practices) across commits
- How commits relate to each other
- Whether earlier commits' issues are fixed in later commits

## OUTPUT FORMAT

Provide your review as a **plain text email body** (NOT Textile, NOT Markdown). Use simple formatting that reads well in email:
- Use CAPS or dashes for section headers
- Use simple indentation for lists
- No special markup syntax

Format:

```
DAILY CODE REVIEW - {author_name}
Review Date: {datetime.now().strftime('%Y-%m-%d')}
Commits Reviewed: {len(commits)}

========================================
SUMMARY
========================================

[Brief overview of what these commits do overall]

Commit  | Description
--------|--------------------------------------------
[short_hash] | [brief description for each commit]

========================================
DETAILED REVIEW
========================================

COMMIT 1: [short_hash] - [Brief Title]
Message: [commit message]
Referenced Issues: [issues]
Files Changed: [list files]

Analysis:
[Your detailed analysis]

Issues Found:
[List issues or "None"]

Verdict: <write APPROVED or write FEEDBACK, not both>
[If FEEDBACK, explain what needs fixing]

----------------------------------------

[Repeat for each commit]

========================================
CROSS-COMMIT OBSERVATIONS (omit if only one commit or nothing notable)
========================================

[Note any patterns, relationships, or overall code quality observations.
 Omit this entire section if there is only one commit or nothing substantive to say.]

========================================
RISK ASSESSMENT (omit if all Low with no concerns)
========================================

Security:   Low/Med/High - [explanation]
Regression: Low/Med/High - [explanation]
[Omit this entire section if everything is Low risk with no notable concerns.]

========================================
OVERALL STATUS: <write APPROVED or write FEEDBACK, not both>
========================================

[Summary. If FEEDBACK, list all items that need attention.]

---
Automated daily code review | {datetime.now().strftime('%Y-%m-%d')} | {len(commits)} commits
```

IMPORTANT:
- Be thorough - check every commit, read every diff
- Give FEEDBACK only for real issues that need attention
- Be constructive - this email goes directly to the author

OUTPUT REQUIREMENTS:
- Output the COMPLETE review in the plain text email format shown above
- Start with "DAILY CODE REVIEW" - no preamble
- Include ALL sections, but omit CROSS-COMMIT OBSERVATIONS and RISK ASSESSMENT if they would only contain boilerplate (e.g., "only one commit", "all low risk with no concerns")
- OVERALL STATUS is NOT omittable: end every review with the OVERALL STATUS line, even for a single commit whose Verdict says the same thing. A review without it is treated as possible FEEDBACK and emailed.

BEGIN YOUR REVIEW NOW. Use your tools to investigate thoroughly.
"""
    return prompt


def build_daily_commit_prompt(author_name, commit, index, total):
    """Build a prompt for reviewing ONE commit during a split review.

    Asks for a single commit block and nothing else.  The digest header, summary,
    overall status and footer are assembled locally in assemble_split_digest(), so
    the model must not emit them here - otherwise stitching the blocks together
    would produce an email with repeated headers and several conflicting status
    lines."""
    refs = ', '.join('#' + r for r in commit['referenced_issues']) or 'None'

    prompt = f"""You are reviewing a single commit by {author_name} in the UCSC Genome Browser kent repository.

## COMMIT {index} of {total}

  @{commit['short_hash']}@ - {commit['message'][:80]}
  Referenced issues: {refs}

## YOUR TASK

Get the diff:
```
git --git-dir={GIT_REPO_PATH} show {commit['hash']}
```

Check for:

- Does the change correctly address the referenced issue(s)?
- Security issues (buffer overflows, SQL injection, XSS, command injection)
- Kent codebase patterns (freez vs freeMem, safef vs sprintf, sqlSafef)
- Logic errors, off-by-one errors, null pointer risks
- **IMPORTANT: For documentation/HTML/text changes, read the content word-by-word and check for:**
  - Typos (doubled words like "the the", wrong words like "of" vs "or")
  - Grammar errors
  - Unclosed HTML tags
  - Missing or incomplete sentences

Read files for context with:
```
git --git-dir={GIT_REPO_PATH} show HEAD:src/path/to/file.c
```

Do NOT read binary files or images (.png, .jpg, .gif, .ico, .pdf, .bb, .bw, .2bit).
Judge those from the diff stat alone - loading them wastes the whole time budget.

## OUTPUT FORMAT

Output ONLY the block shown below, as plain text. Do NOT add a "DAILY CODE REVIEW"
header, a SUMMARY section, an "OVERALL STATUS" line, a footer, or any preamble - those
are added separately. Do NOT wrap your output in a code fence or any other markup.
Start your very first line with "COMMIT {index}:".

The block, with no fence around it:

COMMIT {index}: {commit['short_hash']} - [Brief Title]
Message: {commit['message'][:80]}
Referenced Issues: {refs}
Files Changed: [list files]

Analysis:
[Your detailed analysis]

Issues Found:
[List issues or "None"]

Verdict: <write APPROVED or write FEEDBACK, not both>
[If FEEDBACK, explain what needs fixing]

The "Verdict:" line is required. The word immediately after the colon must be either
APPROVED or FEEDBACK - pick one, do not write both, and do not decorate the line with
markdown.

IMPORTANT:
- Be thorough - read the whole diff
- Give FEEDBACK only for real issues that need attention
- Be constructive - this review goes directly to the author

BEGIN YOUR REVIEW NOW. Use your tools to investigate thoroughly.
"""
    return prompt


# Lines that belong to the digest envelope, not to an individual commit block.  A
# split review builds the envelope locally, so any of these coming back from the
# model are stripped before the blocks are stitched together.
#
# Each value-bearing pattern mirrors what assemble_split_digest() actually writes, value
# shape and all, so such a line is only removed when it really IS the envelope.  The
# valueless patterns below have no shape to check and so lean on column 0 instead.
#
# Two weaker rules were tried and both lost real review text.  Matching the marker
# anywhere in the line deleted "- Typo in the SUMMARY header".  Matching it at the start
# of the line still deleted "Review Date: in the footer is hardcoded to 2020." - both
# leaving an "Issues Found:" heading with nothing under it and a FEEDBACK verdict whose
# explanation had been thrown away.  Requiring the value shape as well keeps both, while
# still catching the real thing.
# Split in two by whether the pattern has a value to check.
#
# Value-bearing lines are matched against the fully normalized line, because decoration
# around them is what the model adds and the value is what proves it is the envelope.
VALUE_ENVELOPE_RES = tuple(re.compile(p, re.IGNORECASE) for p in (
    r'DAILY CODE REVIEW\s*[-–—:]\s*.+$',
    # A date shape, not just one token: '\S+' ate real findings like
    # "Review Date: hardcoded" and "Review Date: 2020".
    r'Review Date\s*[:\-]\s*\d{1,4}[-/]\d{1,2}[-/]\d{1,4}\.?$',
    r'Commits Reviewed\s*[:\-]\s*\d+(\s+of\s+\d+)?\.?$',
    r'OVERALL STATUS\s*[:\-]\s*(APPROVED|FEEDBACK)\W*$',
    r'Automated daily code review\s*[|:].*$',
))
# Valueless lines are matched against the UNDECORATED line only - see undecorate_line().
BARE_ENVELOPE_RES = tuple(re.compile(p, re.IGNORECASE) for p in (
    r'DAILY CODE REVIEW$',
    r'SUMMARY:?$',
    r'DETAILED REVIEW:?$',
    r'CROSS-COMMIT OBSERVATIONS:?$',
    r'RISK ASSESSMENT:?$',
    r'REVIEW INCOMPLETE:?$',
))
# A status line whose value copies the template's placeholder instead of choosing.  Not a
# decision, so it must not be read as one - but it is also not review prose, so it is
# still an envelope line for stripping purposes.
STATUS_PLACEHOLDER_RE = re.compile(
    r'^(APPROVED|FEEDBACK)\s*(?:/|\||,|\bor\b|\band\b)\s*(APPROVED|FEEDBACK)\b',
    re.IGNORECASE)

def is_envelope_line(line):
    """True if this line is one of the digest's own envelope lines rather than review
    text that happens to mention the same words."""
    normalized = normalize_marker_line(line)
    if any(pattern.match(normalized) for pattern in VALUE_ENVELOPE_RES):
        return True
    # The bare patterns additionally require column 0.  The assembler writes its headings
    # flush left, while an indented bare 'SUMMARY' is far more likely a line inside a prose
    # list.  An indented heading from the model therefore survives - cosmetic, and the safe
    # direction, since the alternative is deleting somebody's finding.
    if line[:1].isspace():
        return False
    undecorated = undecorate_line(line)
    return any(pattern.match(undecorated) for pattern in BARE_ENVELOPE_RES)

def verdict_lines(text):
    """The decision of every readable 'Verdict:' line in text, in order, one entry per
    line.  A list rather than a set because digest_wants_email() checks the count
    against the number of commits reviewed, not just which decisions appear."""
    decisions = []
    for raw in text.splitlines():
        match = re.match(VERDICT_LINE_RE, normalize_marker_line(raw), re.IGNORECASE)
        if not match:
            continue
        # The first word of the value decides this line.  Testing "FEEDBACK in line"
        # instead read "Verdict: APPROVED (no feedback required)" as FEEDBACK, which mailed
        # the author a "needs attention" digest over a review that approved their commit.
        word = verdict_value(match.group(1))
        if word:
            decisions.append(word)
    return decisions

def verdict_words(text):
    """Every decision a 'Verdict:' line in text states, as a set of APPROVED/FEEDBACK.

    Collects rather than returning the first, because a block can contain more than one
    such line - a quoted diff hunk, or the template quoted back - and reading whichever
    came first let a quoted "+Verdict: APPROVED" override the real "Verdict: FEEDBACK"
    below it.  Placeholder lines that name both words state nothing and are skipped."""
    return set(verdict_lines(text))

def read_verdict(text):
    """The verdict a block states, or None when it states nothing or contradicts itself.

    None is not a silent failure: callers route it to the "did not state a clear verdict"
    path, which forces the digest to FEEDBACK so a human still looks."""
    words = verdict_words(text)
    return words.pop() if len(words) == 1 else None

def digest_is_incomplete(review):
    """True if a digest carries a REVIEW INCOMPLETE section, i.e. some of the author's
    commits were never reviewed.

    Requires the split footer as well as the section header.  Only assemble_split_digest()
    ever emits this section, so on a batch digest the phrase can only be review prose -
    and a self-review listing the assembler's own section names was raising a "some commits
    were never reviewed" alert on a night when every commit had been reviewed."""
    if SPLIT_FOOTER_MARKER not in review:
        return False
    # Column 0, like the other valueless headings: an indented "REVIEW INCOMPLETE" is more
    # likely a line in a prose list than the assembler's own flush-left section header.
    return any(not l[:1].isspace()
               and undecorate_line(l).upper().rstrip(':') == 'REVIEW INCOMPLETE'
               for l in review.splitlines())

def digest_overall_status(review):
    """The status of an assembled digest: 'FEEDBACK' if any status line says so,
    'APPROVED' only if every one does, None if none is readable.

    Aggregating matters, and taking the first match was a real regression.  A digest
    contains quoted diffs, quoted source, and summary prose, so more than one line can
    look like a status.  With first-match, a summary opening

        Overall status: approved for the docs, but see the C change below.

    outranked the real 'OVERALL STATUS: FEEDBACK' further down, and a review reporting a
    SQL injection was silently dropped - worse than the bare substring test it replaced.
    Any-FEEDBACK-wins can only ever over-send, which is the recoverable direction."""
    statuses = set()
    for raw in review.splitlines():
        match = re.match(r'^OVERALL STATUS\s*[:\-]\s*(.+)$', normalize_marker_line(raw),
                         re.IGNORECASE)
        if not match:
            continue
        word = verdict_value(match.group(1))
        if word:
            statuses.add(word)
    if 'FEEDBACK' in statuses:
        return 'FEEDBACK'
    if 'APPROVED' in statuses:
        return 'APPROVED'
    return None

def textile_review_verdict(review):
    """The verdict of a Textile review (the ticket and standalone modes), or None.

    Those templates label the line "h3. Verdict:" or "h3. Status:" and never write
    "OVERALL STATUS:", so digest_overall_status() cannot read them.  Reusing the daily
    readers here reported every approved Textile review as FEEDBACK."""
    verdict = read_verdict(review)
    if verdict:
        return verdict
    for raw in review.splitlines():
        match = re.match(r'^Status\s*[*_`]*\s*:\s*(.+)$', normalize_marker_line(raw),
                         re.IGNORECASE)
        if match:
            word = verdict_value(match.group(1))
            if word:
                return word
    return None

def digest_wants_email(review, num_commits=None):
    """True if this digest should be emailed to its author.

    Sends unless the digest definitively says APPROVED, and keeps the old bare substring
    test as a backstop so this can only ever send more than production did, never less.
    An unreadable status sends: between mailing a review nobody needed and silently
    binning a real one, the first is the recoverable mistake.

    One narrowing of that rule: the model drops the OVERALL STATUS block roughly one
    digest in six, nearly always alongside the sections the prompt invites it to omit,
    and in the first three weeks every one of those was an approved review mailed as
    "possible FEEDBACK".  So when the status is missing, the per-commit Verdict lines
    decide instead - but only on a full accounting: exactly one readable verdict per
    commit reviewed and every one APPROVED.  A missing or unreadable verdict, an extra
    one (e.g. quoted from the template), or any FEEDBACK still sends."""
    if 'OVERALL STATUS: FEEDBACK' in review:
        return True
    status = digest_overall_status(review)
    if status is not None:
        return status != 'APPROVED'
    if num_commits:
        verdicts = verdict_lines(review)
        if len(verdicts) == num_commits and set(verdicts) == {'APPROVED'}:
            return False
    return True

def sanitize_commit_block(block, label=''):
    """Strip envelope lines and separator rules out of one commit block.

    build_daily_commit_prompt() tells the model not to emit them, but the model can
    ignore that, and a stray header or OVERALL STATUS line inside a block would show
    up as a duplicate in the assembled email.  Also drops any preamble before the
    COMMIT line, and the code fence the model tends to wrap the block in.

    Reports what it removed.  Every round of this function's history has had a bug where
    it silently ate real review text, and the reason each one took a code review to find
    is that deletion left no trace.  A count and a preview in the log turns the next one
    into something anybody can spot in the nightly output."""
    if not block:
        return ''
    original_lines = block.splitlines()

    # Drop any preamble before the first "COMMIT n:" line ("Sure! Here is the review",
    # an opening code fence, and so on).
    #
    # Two guards, because every looser version of this deleted real review text.  A quoted
    # header - this file's own template line, or a bulleted or indented copy of it - used to
    # become the anchor and discard everything above it, which is the whole review.  So the
    # search stops once the block has plainly started its own content, and in any case after
    # PREAMBLE_SCAN_LINES non-blank lines.  A block whose header is genuinely further in than
    # that keeps its preamble, which is only cosmetic.
    lines = block.splitlines()
    seen = 0
    for i, line in enumerate(lines):
        bare = undecorate_line(line)
        if line.strip():
            seen += 1
            if seen > PREAMBLE_SCAN_LINES:
                break
        if re.match(r'^COMMIT\s+\d+\s*:', bare, re.IGNORECASE):
            block = '\n'.join(lines[i:])
            break
        # Content has started, so any COMMIT line after this is quoted, not the header.
        if re.match(r'^(Analysis|Issues Found|Verdict|Message|Files Changed|'
                    r'Referenced Issues)\s*:', bare, re.IGNORECASE):
            break

    kept = []
    for line in block.splitlines():
        stripped = line.strip()
        # Separator rules: the assembler emits its own.
        if stripped and set(stripped) == {'='}:
            continue
        # A code fence, with or without a language tag, from the echoed template.
        if re.match(r'^(```|~~~)\w*$', stripped):
            continue
        if is_envelope_line(line):
            continue
        kept.append(line)

    # Count only lines with content: blank lines and rule characters are noise.
    dropped = [l for l in original_lines
               if l.strip() and l not in kept and set(l.strip()) != {'='}]
    if dropped:
        preview = ' | '.join(l.strip()[:60] for l in dropped[:3])
        print(f"      sanitize{label}: dropped {len(dropped)} line(s) of "
              f"{len([l for l in original_lines if l.strip()])}: {preview}")

    return '\n'.join(kept).strip()

def commit_block_verdict(block):
    """Read the verdict out of one sanitized commit block.  Returns 'FEEDBACK',
    'APPROVED', or None when the Verdict line is missing or ambiguous."""
    return read_verdict(block)

def assemble_split_digest(author_name, commits, blocks, failed):
    """Build the digest email for a split review out of the per-commit blocks.

    'blocks' is a list of (commit, sanitized_block, verdict) for the commits that
    were reviewed; 'failed' is a list of (commit, error) for the ones that were not.
    Exactly one header, one OVERALL STATUS and one footer are emitted here, which is
    the only place they come from on this path."""
    date = datetime.now().strftime('%Y-%m-%d')
    reviewed = len(blocks)
    feedback = [b for b in blocks if b[2] == 'FEEDBACK']
    # No usable Verdict line is not an approval - treat it as needing a look.
    unclear = [b for b in blocks if b[2] is None]

    # An unreviewed commit has to reach the author, and run_daily_mode only sends
    # mail when the digest says FEEDBACK.  Anything less than a clean full pass is
    # therefore FEEDBACK, so a commit nobody managed to review cannot go out silently
    # as an approval.
    status = 'FEEDBACK' if (feedback or unclear or failed) else 'APPROVED'

    out = [
        f"DAILY CODE REVIEW - {author_name}",
        f"Review Date: {date}",
        f"Commits Reviewed: {reviewed} of {len(commits)}",
        "",
        "=" * 40,
        "SUMMARY",
        "=" * 40,
        "",
        "Reviewing these commits together ran out of time, so each commit was",
        "reviewed on its own instead. Cross-commit observations were skipped.",
        "",
        "Commit  | Description",
        "--------|--------------------------------------------",
    ]
    for commit, _block, verdict in blocks:
        label = verdict or 'UNCLEAR'
        out.append(f"{commit['short_hash']} | [{label}] {commit['message'][:60]}")
    for commit, _err in failed:
        out.append(f"{commit['short_hash']} | [NOT REVIEWED] {commit['message'][:60]}")

    out += [
        "",
        "=" * 40,
        "DETAILED REVIEW",
        "=" * 40,
        "",
    ]
    for position, (commit, block, _verdict) in enumerate(blocks, 1):
        # Renumber to position among the commits that were actually reviewed. The model
        # was told its batch index, so a digest whose first commit failed would otherwise
        # open its detailed review at "COMMIT 2".  Rewritten line by line through the
        # normalizer, because matching the raw text no-opped on a header the model had
        # decorated as "**COMMIT 2: ...**" - and that also leaves raw markdown in what is
        # meant to be a plain-text email.
        block_lines = block.splitlines()
        for j, block_line in enumerate(block_lines):
            header = re.match(r'^COMMIT\s+\d+\s*:\s*(.*)$', undecorate_line(block_line),
                              re.IGNORECASE)
            if header:
                block_lines[j] = f"COMMIT {position}: {header.group(1)}"
                break
        block = '\n'.join(block_lines)
        out.append(block)
        out += ["", "-" * 40, ""]

    if failed:
        out += [
            "=" * 40,
            "REVIEW INCOMPLETE",
            "=" * 40,
            "",
            "These commits could not be reviewed automatically and still need a look:",
            "",
        ]
        for commit, err in failed:
            out.append(f"  {commit['short_hash']} - {commit['message'][:70]}")
            out.append(f"      {err}")
        out.append("")

    out += [
        "=" * 40,
        f"OVERALL STATUS: {status}",
        "=" * 40,
        "",
    ]
    if feedback:
        out.append("Commits needing attention:")
        for commit, _block, _verdict in feedback:
            out.append(f"  {commit['short_hash']} - {commit['message'][:70]}")
        out.append("")
    if unclear:
        out.append("Commits whose review did not state a clear verdict:")
        for commit, _block, _verdict in unclear:
            out.append(f"  {commit['short_hash']} - {commit['message'][:70]}")
        out.append("")
    if not feedback and not unclear and not failed:
        out.append("No issues found.")
        out.append("")

    out += [
        "---",
        f"Automated daily code review | {date} | {len(commits)} commits "
        f"({SPLIT_FOOTER_MARKER})",
    ]
    return '\n'.join(out) + '\n'


def get_gmail_service():
    """Get authenticated Gmail API service"""
    from google.oauth2.credentials import Credentials
    from google.auth.transport.requests import Request
    from googleapiclient.discovery import build as google_build

    creds = None
    if os.path.exists(GMAIL_TOKEN_PATH):
        creds = Credentials.from_authorized_user_file(GMAIL_TOKEN_PATH, GMAIL_SCOPES)

    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
            with open(GMAIL_TOKEN_PATH, 'w') as f:
                f.write(creds.to_json())
        else:
            print("ERROR: Gmail token not found or invalid. Run the MLQ automation script first to authenticate.")
            sys.exit(1)

    return google_build('gmail', 'v1', credentials=creds, cache_discovery=False)


def send_review_email(gmail_service, to_email, author_name, review_text, cc=None,
                      review_file=None):
    """Send a code review email to the author.
    review_file is the path to the saved text copy of this same review; it is quoted
    at the bottom so the author can point a tool at the file instead of copying the
    body out of the email."""
    if review_file:
        review_text = (review_text.rstrip('\n') + "\n\n" + "-" * 70 + "\n" +
                       "Text version of this review, readable on hgwdev:\n" +
                       f"  {review_file}\n")
    message = MIMEText(review_text)
    message['To'] = to_email
    message['From'] = 'gbauto@ucsc.edu'
    message['Subject'] = f'Daily Code Review - {author_name} - {datetime.now().strftime("%Y-%m-%d")}'
    if cc:
        message['Cc'] = cc

    encoded = base64.urlsafe_b64encode(message.as_bytes()).decode('utf-8')
    gmail_service.users().messages().send(
        userId='me',
        body={'raw': encoded}
    ).execute()


def send_alert_email(gmail_service, to_email, failures, window_label, log_dir,
                     incomplete=None, skipped=None):
    """Send a maintainer alert when reviews failed, came back incomplete, or were never
    started.  This is what keeps the daily cron from failing silently: a broken review is
    never sent to the author, so without this nobody would notice the tool had stopped
    working.

    The three lists are kept apart because they need different words, and because the
    remedy differs.  A failure produced no usable review and often means the CLI token
    expired.  An incomplete one was produced and delivered, but some of the author's
    commits were never reviewed and nothing will retry them.  A skipped one was never
    started because the run ran out of time - nothing is broken at all.  Reporting all
    three under one "FAILED / did not produce valid reviews / your token has probably
    expired" banner sent the maintainer hunting a problem that did not exist."""
    incomplete = incomplete or []
    skipped = skipped or []
    if failures:
        opening = ["The automated daily code review (codeReviewAi.py --daily) hit errors",
                   "and did not produce valid reviews for one or more authors."]
        subject = f"[ALERT] Daily Code Review FAILED - {datetime.now().strftime('%Y-%m-%d')}"
    elif skipped:
        opening = ["The automated daily code review (codeReviewAi.py --daily) ran out of",
                   "time before it reached every author. The reviews it did produce were",
                   "sent. Nothing is broken, but the authors below were not reviewed and",
                   "nothing will retry them: the review window has passed."]
        subject = (f"[ALERT] Daily Code Review RAN LONG - "
                   f"{datetime.now().strftime('%Y-%m-%d')}")
    else:
        opening = ["The automated daily code review (codeReviewAi.py --daily) finished and",
                   "the reviews were sent, but some commits were never reviewed. Nothing",
                   "will retry them: the review window has passed."]
        subject = (f"[ALERT] Daily Code Review INCOMPLETE - "
                   f"{datetime.now().strftime('%Y-%m-%d')}")

    lines = opening + [
        "",
        f"Review date:      {datetime.now().strftime('%Y-%m-%d %H:%M')}",
        f"Review window:    {window_label}",
        f"Failed reviews:   {len(failures)}",
        f"Incomplete:       {len(incomplete)}",
        f"Never started:    {len(skipped)}",
    ]
    if failures:
        lines += ["", "Failed:"]
        for name, err in failures:
            lines.append(f"  - {name}: {err}")
    if incomplete:
        lines += ["", "Incomplete (review delivered, some commits skipped):"]
        for name, err in incomplete:
            lines.append(f"  - {name}: {err}")
    if skipped:
        lines += ["", "Never started (the run ran out of time):"]
        for name, err in skipped:
            lines.append(f"  - {name}: {err}")
    if failures:
        lines += [
            "",
            "Most common cause: the 'claude' CLI OAuth token for the cron user has",
            "expired. Re-run 'claude' interactively as that user and /login, or mint a",
            "long-lived token with 'claude setup-token', then confirm with:",
            "  claude -p 'say ok'",
        ]
    lines += [
        "",
        f"Logs: {log_dir}/",
        "",
        "-- codeReviewAi.py automated alert",
    ]
    message = MIMEText("\n".join(lines))
    message['To'] = to_email
    message['From'] = 'gbauto@ucsc.edu'
    message['Subject'] = subject
    encoded = base64.urlsafe_b64encode(message.as_bytes()).decode('utf-8')
    gmail_service.users().messages().send(
        userId='me',
        body={'raw': encoded}
    ).execute()


def review_daily_author(author_name, commits, log_dir,
                        window_label="the last 24 hours", file_suffix=None):
    """Review all commits by one author for the daily digest.
    Temp files (prompts/responses) are written to log_dir and returned for cleanup.
    file_suffix keys the temp filenames (defaults to today's date); pass an
    explicit window so same-day backfill runs do not clobber each other."""
    print(f"\n{'='*60}")
    print(f"REVIEWING DAILY COMMITS: {author_name}")
    print(f"Commits: {len(commits)}")
    print(f"{'='*60}")

    prompt = build_daily_review_prompt(author_name, commits, window_label)

    # Save prompt to log_dir for debugging (cleaned up on success)
    safe_name = re.sub(r'[^a-zA-Z0-9]', '_', author_name)
    suffix = file_suffix or datetime.now().strftime('%Y%m%d')
    temp_files = []

    prompt_file = os.path.join(log_dir, f".tmp_daily_prompt_{safe_name}_{suffix}.txt")
    with open(prompt_file, 'w') as f:
        f.write(prompt)
    temp_files.append(prompt_file)
    print(f"  Prompt saved to: {prompt_file}")

    timeout = scaled_timeout(len(commits))
    print(f"  Calling Claude CLI (timeout {timeout}s for {len(commits)} commit(s))...")
    diag = {}
    raw_response = call_claude_cli(prompt, timeout=timeout,
                                   validator=validate_daily_review_output, diag=diag)
    error = detect_cli_failure(raw_response, validate_daily_review_output)

    # Record what the batch cost alongside every candidate predictor of that cost.
    # Commit count turned out to be a poor one (a single commit of 10 binary images
    # outran a batch of eight much larger text commits), so log them all and let a
    # couple of weeks of runs settle which one actually fits.
    files, added, removed, binary = batch_stats(commits)
    print(f"  COST elapsed={diag.get('elapsed', 0):.0f}s timeout={timeout}s "
          f"attempts={diag.get('attempts', 0)} commits={len(commits)} files={files} "
          f"added={added} removed={removed} binary={binary} "
          f"timedOut={diag.get('timed_out', False)}")

    # Save whatever we got back for debugging, even on failure.
    if raw_response:
        response_file = os.path.join(log_dir, f".tmp_daily_response_{safe_name}_{suffix}.txt")
        with open(response_file, 'w') as f:
            f.write(raw_response)
        temp_files.append(response_file)

    if diag.get('timed_out'):
        if 1 < len(commits) <= SPLIT_MAX_COMMITS:
            # Reviewing them together ran out of time. Review each commit on its own
            # so the ones that would have gone through are not lost with the one that
            # blew the budget - and so that expensive commit gets a whole budget to
            # itself instead of a share of the batch's.
            response, error = review_daily_author_split(
                author_name, commits, log_dir, safe_name, suffix)
            return response, temp_files, error

        # Nothing to split (a single commit), or too many commits to fan out
        # sensibly. Give it one more attempt with a bigger budget.  retries=0 here
        # because at this size a further invalid-output re-run would double an
        # already large budget.
        retry_timeout = int(timeout * TIMEOUT_ESCALATION)
        print(f"  Retrying once with an escalated timeout ({retry_timeout}s)...")
        retry_diag = {}
        raw_response = call_claude_cli(prompt, timeout=retry_timeout, retries=0,
                                       validator=validate_daily_review_output,
                                       diag=retry_diag)
        error = detect_cli_failure(raw_response, validate_daily_review_output)
        print(f"  COST elapsed={retry_diag.get('elapsed', 0):.0f}s "
              f"timeout={retry_timeout}s attempts={retry_diag.get('attempts', 0)} "
              f"commits={len(commits)} escalated=True "
              f"timedOut={retry_diag.get('timed_out', False)}")
        # No first-attempt response file exists to collide with: reaching here means the
        # first attempt timed out, so call_claude_cli returned None.
        if raw_response:
            response_file = os.path.join(log_dir,
                                         f".tmp_daily_response_{safe_name}_{suffix}.txt")
            with open(response_file, 'w') as f:
                f.write(raw_response)
            temp_files.append(response_file)

    if error:
        print(f"  WARNING: review failed - {error}")
        response = f"DAILY CODE REVIEW - {author_name}\n\nError: {error}\n"
    else:
        response = raw_response
        # Strip any preamble before the digest header. Matched per line through
        # undecorate_line, because a header the model wrote as "**DAILY CODE REVIEW - X**"
        # did not match the raw anchor and left its chatty preamble in the email.
        response_lines = response.splitlines()
        for i, response_line in enumerate(response_lines):
            if undecorate_line(response_line).upper().startswith('DAILY CODE REVIEW'):
                response = '\n'.join(response_lines[i:])
                break
        print(f"  Review complete")

    return response, temp_files, error


def review_daily_author_split(author_name, commits, log_dir, safe_name, suffix):
    """Review each of one author's commits separately after the batch review timed
    out, then assemble the per-commit blocks into a single digest.

    Returns (response, error).  'error' is set only when nothing at all could be
    reviewed; a partial success returns no error so the author still gets the commits
    that did come through, with the rest called out in the digest.

    Raw per-commit blocks are written straight to log_dir under non-temp names rather
    than being returned for cleanup - they are the evidence for a night that went wrong,
    so they must survive run_daily_mode's temp-file sweep."""
    print(f"  Splitting {len(commits)} commits into individual reviews...")
    blocks = []
    failed = []

    started = time.time()
    for i, commit in enumerate(commits, 1):
        # Stop before starting a review that would push this author past the budget.
        # Testing "already over" instead let the split overrun its stated ceiling by up
        # to a third, because the check ran after the overshoot rather than before it.
        # Whatever is left is reported as not reviewed, which the digest surfaces and
        # which forces the email to go out.
        spent = time.time() - started
        if spent + SPLIT_PER_COMMIT_TIMEOUT > SPLIT_TOTAL_BUDGET:
            for skipped in commits[i - 1:]:
                failed.append((skipped, f"Not reviewed: no room left in the split's "
                                        f"{SPLIT_TOTAL_BUDGET}s budget"))
            print(f"    Split budget of {SPLIT_TOTAL_BUDGET}s exhausted after "
                  f"{i - 1} commit(s); {len(commits) - i + 1} left unreviewed")
            break

        print(f"    Commit {i}/{len(commits)}: {commit['short_hash']}")
        prompt = build_daily_commit_prompt(author_name, commit, i, len(commits))
        files, added, removed, binary = commit_stats(commit['hash'])
        diag = {}
        raw = call_claude_cli(prompt, timeout=SPLIT_PER_COMMIT_TIMEOUT,
                              validator=validate_daily_commit_output, diag=diag)
        print(f"      COST elapsed={diag.get('elapsed', 0):.0f}s "
              f"timeout={SPLIT_PER_COMMIT_TIMEOUT}s attempts={diag.get('attempts', 0)} "
              f"split=True commit={commit['short_hash']} files={files} "
              f"added={added} removed={removed} binary={binary} "
              f"timedOut={diag.get('timed_out', False)}")

        # One escalated attempt for the commit that blew its budget, if there is room
        # left.  Without this the split's per-commit budget is SPLIT_PER_COMMIT_TIMEOUT
        # and nothing more, which is DAILY_TIMEOUT_MIN - a budget already known to be
        # too small for a single expensive commit.  The 24h window has passed by the
        # next run, so a commit given up on here is never reviewed at all.
        if diag.get('timed_out'):
            spent = time.time() - started
            escalated = int(SPLIT_PER_COMMIT_TIMEOUT * TIMEOUT_ESCALATION)
            if spent + escalated <= SPLIT_TOTAL_BUDGET:
                print(f"      Retrying this commit with an escalated timeout "
                      f"({escalated}s)...")
                retry_diag = {}
                raw = call_claude_cli(prompt, timeout=escalated, retries=0,
                                      validator=validate_daily_commit_output,
                                      diag=retry_diag)
                print(f"      COST elapsed={retry_diag.get('elapsed', 0):.0f}s "
                      f"timeout={escalated}s attempts={retry_diag.get('attempts', 0)} "
                      f"split=True escalated=True commit={commit['short_hash']} "
                      f"timedOut={retry_diag.get('timed_out', False)}")
            else:
                print(f"      Not enough split budget left to retry "
                      f"({SPLIT_TOTAL_BUDGET - int(spent)}s remaining)")

        commit_error = detect_cli_failure(raw, validate_daily_commit_output)

        # Written before the error check on purpose: a block rejected by the validator
        # is exactly the one worth reading afterwards to see why.
        if raw:
            block_file = os.path.join(
                log_dir, f"daily_block_{safe_name}_{commit['short_hash']}_{suffix}.txt")
            with open(block_file, 'w') as f:
                f.write(raw)

        if commit_error:
            print(f"      WARNING: {commit_error}")
            failed.append((commit, commit_error))
            continue

        block = sanitize_commit_block(raw, label=f" {commit['short_hash']}")
        if not block:
            failed.append((commit, "Review produced no usable content"))
            continue
        blocks.append((commit, block, commit_block_verdict(block)))

    if not blocks:
        reason = failed[0][1] if failed else "no reviews produced"
        error = (f"Split review failed for all {len(commits)} commit(s) after the "
                 f"batch review timed out (first: {reason})")
        print(f"  WARNING: {error}")
        return f"DAILY CODE REVIEW - {author_name}\n\nError: {error}\n", error

    response = assemble_split_digest(author_name, commits, blocks, failed)
    print(f"  Split review complete: {len(blocks)} reviewed, {len(failed)} failed")
    return response, None


def run_daily_mode(hours, cc_address, dry_run, log_dir, alert_email=DEFAULT_ALERT_EMAIL,
                   since=None, until=None):
    """Run daily review mode: get recent commits, review per author, email results.
    By default covers the last N hours; pass since/until to review an explicit
    window instead (e.g. to backfill a period the cron missed).
    Returns True on full success, False if any author's review failed (so the
    caller can exit non-zero)."""
    os.makedirs(log_dir, exist_ok=True)
    auth_method = ensure_claude_auth()

    # Human- and filename-friendly descriptions of the review window.
    if since:
        window_label = f"the window {since} to {until}" if until else f"the window since {since}"
        file_suffix = re.sub(r'[^0-9]', '', since) + ("-" + re.sub(r'[^0-9]', '', until) if until else "")
    else:
        window_label = f"the last {hours} hours"
        file_suffix = datetime.now().strftime('%Y%m%d')

    print("=" * 60)
    print(f"DAILY CODE REVIEW MODE")
    print(f"Window: {window_label}")
    print(f"CC: {cc_address or 'None'}")
    print(f"Alert: {alert_email or 'None'}")
    print(f"Auth: {auth_method}")
    print(f"Log dir: {log_dir}")
    print(f"Dry run: {dry_run}")
    print("=" * 60)

    # Phase 1: Gather commits
    print(f"\nPhase 1: Gathering commits from {window_label}...")
    authors = get_commits_since(hours, since=since, until=until)

    if not authors:
        print("No commits found in the specified time window.")
        return True

    total_commits = sum(len(a['commits']) for a in authors.values())
    print(f"Found {total_commits} commit(s) from {len(authors)} author(s):")
    for email, data in authors.items():
        print(f"  {data['name']} <{email}>: {len(data['commits'])} commit(s)")

    # Phase 2: Review each author's commits
    print(f"\nPhase 2: Reviewing commits...")
    reviews = {}
    all_temp_files = []
    run_started = time.time()
    for author_email, data in authors.items():
        # Authors are reviewed one after another, so without a deadline here a single
        # bad night can still be running when tomorrow's cron fires - and the cron takes
        # no lock. Anyone past the deadline is reported rather than quietly skipped.
        run_spent = time.time() - run_started
        if run_spent > DAILY_RUN_BUDGET:
            error = (f"Not reviewed: the run passed its {DAILY_RUN_BUDGET}s budget "
                     f"after {len(reviews)} author(s)")
            print(f"\n  {data['name']}: {error}")
            reviews[author_email] = {
                'name': data['name'],
                'email': author_email,
                'review': f"DAILY CODE REVIEW - {data['name']}\n\nError: {error}\n",
                'num_commits': len(data['commits']),
                'error': error,
                'file': None,
                # Distinguishes "we ran out of night" from "the review broke", so this
                # author is not reported under the expired-OAuth-token banner.
                'skipped': True,
            }
            continue

        review, temp_files, error = review_daily_author(
            data['name'], data['commits'], log_dir,
            window_label=window_label, file_suffix=file_suffix)
        all_temp_files.extend(temp_files)
        reviews[author_email] = {
            'name': data['name'],
            'email': author_email,
            'review': review,
            'num_commits': len(data['commits']),
            'error': error,
        }

        # Save review to log_dir
        safe_name = re.sub(r'[^a-zA-Z0-9]', '_', data['name'])
        # Absolute, because this path is quoted in the author's email and has to be
        # usable from wherever they happen to be sitting.
        filepath = os.path.abspath(
            os.path.join(log_dir, f"daily_review_{safe_name}_{file_suffix}.txt"))
        with open(filepath, 'w') as f:
            f.write(review)
        reviews[author_email]['file'] = filepath
        print(f"  Saved: {filepath}")

    # Collect any failures (broken/auth-failed reviews). A failed review is
    # never emailed to an author; instead we alert the maintainer below.
    failures = [(d['name'], d['error']) for d in reviews.values()
                if d['error'] and not d.get('skipped')]
    # Authors the run never got to. Their own category: nothing is broken and no token has
    # expired, the night simply ran out, so they need different words in the alert.
    skipped = [(d['name'], d['error']) for d in reviews.values() if d.get('skipped')]

    # A split that only got through some of an author's commits is not an outright
    # failure - the author still gets the reviews that worked, and the digest is
    # forced to FEEDBACK so it actually reaches them. But nothing will ever retry the
    # commits it gave up on, since the review window has passed by the next run, so
    # the maintainer needs to hear about it too. Reported alongside the failures
    # without counting as one, so the exit status still means "a review broke".
    incomplete = [(d['name'], "Reviewed, but some commits were left unreviewed "
                              "(see REVIEW INCOMPLETE in the digest)")
                  for d in reviews.values()
                  if not d['error'] and digest_is_incomplete(d['review'])]

    def verdict_of(data):
        if data.get('skipped'):
            return 'NOT STARTED'
        if data['error']:
            return 'FAILED'
        # Report the send decision, not the parsed status. They can differ - the substring
        # backstop in digest_wants_email fires on a digest whose own status line reads
        # APPROVED - and a dry run that printed the status was telling the operator "no
        # email" on a night the live run mails.
        return ('FEEDBACK' if digest_wants_email(data['review'], data.get('num_commits'))
                else 'APPROVED')

    # Phase 3: Send emails (only for reviews with FEEDBACK; never for failures)
    print(f"\nPhase 3: Sending emails (FEEDBACK only)...")
    if dry_run:
        print("[DRY RUN] Emails not sent. Reviews saved locally:")
        for author_email, data in reviews.items():
            print(f"  {data['name']} <{author_email}>: {verdict_of(data)} - {data['file']}")
    else:
        gmail_service = get_gmail_service()
        for author_email, data in reviews.items():
            if data['error']:
                print(f"  {data['name']}: FAILED - skipping author email (will alert maintainer)")
                continue
            if not digest_wants_email(data['review'], data.get('num_commits')):
                if digest_overall_status(data['review']) is None:
                    print(f"  {data['name']}: APPROVED (no readable OVERALL STATUS, but "
                          f"every per-commit verdict is APPROVED) - skipping email")
                else:
                    print(f"  {data['name']}: APPROVED - skipping email")
                continue
            if digest_overall_status(data['review']) is None:
                print(f"  {data['name']}: WARNING - could not read OVERALL STATUS or "
                      f"account for every per-commit verdict, emailing anyway rather "
                      f"than dropping a possible FEEDBACK")
            print(f"  Emailing {data['name']} <{author_email}> (FEEDBACK)...")
            try:
                send_review_email(gmail_service, author_email, data['name'],
                                  data['review'], cc=cc_address,
                                  review_file=data.get('file'))
                print(f"    SENT")
            except Exception as e:
                # This email is the only channel that tells an author a commit of
                # theirs went unreviewed, so a send failure has to reach the
                # maintainer rather than being swallowed into the log.
                print(f"    FAILED: {e}")
                failures.append((data['name'], f"Review completed but the email could "
                                               f"not be sent: {e}"))

    # Phase 3b: Alert the maintainer if anything failed, so it does not fail silently
    if failures or incomplete or skipped:
        print(f"\nPhase 3b: {len(failures)} review(s) FAILED, "
              f"{len(incomplete)} incomplete, {len(skipped)} not started "
              f"- alerting maintainer...")
        for name, err in failures + incomplete + skipped:
            print(f"  {name}: {err}")
        if dry_run:
            print(f"[DRY RUN] Alert email not sent (would go to {alert_email}).")
        elif alert_email:
            try:
                gmail_service = get_gmail_service()
                send_alert_email(gmail_service, alert_email, failures, window_label,
                                 log_dir, incomplete=incomplete, skipped=skipped)
                print(f"  Alert sent to {alert_email}")
            except Exception as e:
                print(f"  WARNING: failed to send alert email: {e}")

    # Clean up temp files, but keep them for any author whose review failed - those
    # are the ones worth reading afterwards, and deleting them was defeating the
    # "saved for debugging even on failure" intent above.  Names come from the failures
    # list rather than from d['error'] so that a review which was produced but could not
    # be emailed also keeps its evidence.  The '_name_' form matters: a bare substring
    # test meant a failure by "Bob" also retained every file belonging to "Bobby Smith".
    failed_names = {re.sub(r'[^a-zA-Z0-9]', '_', name)
                    for name, _err in failures + skipped}
    for f in all_temp_files:
        base = os.path.basename(f)
        if any(f"_{safe}_" in base for safe in failed_names):
            print(f"  Keeping {f} (review failed)")
            continue
        try:
            os.remove(f)
        except OSError:
            pass

    # Summary
    print(f"\n{'='*60}")
    print("DAILY REVIEW COMPLETE")
    print(f"{'='*60}")
    print(f"Authors reviewed: {len(reviews)}")
    print(f"Total commits: {total_commits}")
    for author_email, data in reviews.items():
        print(f"  {data['name']}: {data['num_commits']} commits - {verdict_of(data)}")

    return not (failures or skipped)


# =============================================================================
# MAIN
# =============================================================================

def save_review(review, ticket_data=None, single_commit=None, standalone_commit=None):
    """Save review to local file"""
    if standalone_commit:
        # Standalone commit review (no ticket)
        filename = f"code_review_commit_{standalone_commit['short_hash']}.md"
    elif single_commit:
        # Single commit within a ticket
        filename = f"code_review_{ticket_data['ticket_id']}_{ticket_data['coder']}_{single_commit['short_hash']}.md"
    else:
        # Per-ticket review
        filename = f"code_review_{ticket_data['ticket_id']}_{ticket_data['coder']}_{ticket_data['version']}.md"

    filepath = os.path.join(OUTPUT_DIR, filename)

    with open(filepath, 'w') as f:
        f.write(review)

    print(f"  Saved: {filename}")
    return filepath

def display_summary(results):
    """Display summary of all reviews"""
    print("\n" + "=" * 60)
    print("REVIEW SUMMARY")
    print("=" * 60)

    for ticket_id, data in results.items():
        print(f"\nTicket #{ticket_id}: {data['coder']}")
        print(f"  Commits: {data['num_commits']}")
        print(f"  Verdict: {data['verdict']}")
        print(f"  File: {data['file']}")

def main():
    parser = argparse.ArgumentParser(
        description='Code Review Automation for UCSC Genome Browser',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Review all open tickets (per-ticket mode):
    python3 codeReviewAi.py --dry-run

  Review a specific ticket:
    python3 codeReviewAi.py --ticket 36933 --dry-run

  Review a specific commit within a ticket:
    python3 codeReviewAi.py --ticket 36933 --commit c7c977ef --dry-run

  Review any commit directly (no ticket needed):
    python3 codeReviewAi.py --commit c7c977ef --dry-run

  Daily review (cron mode) - review last 24h of commits, email authors:
    python3 codeReviewAi.py --daily --dry-run
    python3 codeReviewAi.py --daily --hours 24 --cc browser-code-reviews-group@ucsc.edu

  Backfill an explicit window (e.g. one week the cron missed), email authors:
    python3 codeReviewAi.py --daily --since 2026-06-20 --until 2026-06-27 --dry-run
        """
    )
    parser.add_argument('--dry-run', action='store_true',
                        help='Generate reviews but do not post to Redmine / send emails')
    parser.add_argument('--ticket', type=int,
                        help='Review only this ticket ID')
    parser.add_argument('--commit', type=str,
                        help='Review a specific commit (can be used with or without --ticket)')
    parser.add_argument('--daily', action='store_true',
                        help='Daily mode: review recent commits by all authors and email results')
    parser.add_argument('--hours', type=int, default=24,
                        help='Hours to look back for --daily mode (default: 24)')
    parser.add_argument('--since', type=str,
                        help='Start of an explicit review window for --daily mode, '
                             'any date git understands (e.g. 2026-06-20). Overrides '
                             '--hours. Use for backfilling a missed period.')
    parser.add_argument('--until', type=str,
                        help='End of the --since window (e.g. 2026-06-27). '
                             'Optional; defaults to now.')
    parser.add_argument('--cc', type=str, default=DEFAULT_CC,
                        help=f'CC address for --daily emails (default: {DEFAULT_CC})')
    parser.add_argument('--alert-email', type=str, default=DEFAULT_ALERT_EMAIL,
                        help=f'Address to alert if a --daily review fails, e.g. an '
                             f'expired auth token (default: {DEFAULT_ALERT_EMAIL})')
    parser.add_argument('--log-dir', type=str,
                        default=os.path.expanduser('~/codeReviewLogs'),
                        help='Directory for daily review logs and output (default: ~/codeReviewLogs)')
    args = parser.parse_args()

    # =================================================================
    # DAILY MODE (--daily)
    # =================================================================
    if args.daily:
        ok = run_daily_mode(args.hours, args.cc, args.dry_run, args.log_dir,
                            alert_email=args.alert_email,
                            since=args.since, until=args.until)
        sys.exit(0 if ok else 1)

    # Load configuration
    config = load_config()
    redmine_key = config.get('redmine.apiKey')

    if not redmine_key:
        print("ERROR: redmine.apiKey not found in config")
        sys.exit(1)

    # Ticket/commit modes save reviews and debug files under OUTPUT_DIR
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"Auth: {ensure_claude_auth()}")

    # =================================================================
    # STANDALONE COMMIT MODE (--commit without --ticket)
    # =================================================================
    if args.commit and not args.ticket:
        print("=" * 60)
        print("STANDALONE COMMIT REVIEW")
        print("=" * 60)

        # Get commit info from git
        commit, error = get_commit_from_git(args.commit)
        if error:
            print(f"ERROR: {error}")
            sys.exit(1)

        print(f"Commit: {commit['short_hash']}")
        print(f"Author: {commit['author']}")
        print(f"Message: {commit['message']}")

        # Fetch any referenced issues
        referenced_issues = {}
        for ref_id in commit['referenced_issues']:
            print(f"Fetching referenced issue #{ref_id}...")
            referenced_issues[ref_id] = get_referenced_issue(ref_id, redmine_key)

        # Review the commit
        review = review_standalone_commit(commit, referenced_issues)

        # Save
        filepath = save_review(review, standalone_commit=commit)

        # Summary
        verdict = textile_review_verdict(review) or 'FEEDBACK'
        print(f"\n" + "=" * 60)
        print("REVIEW SUMMARY")
        print("=" * 60)
        print(f"\nCommit: {commit['short_hash']}")
        print(f"  Author: {commit['author']}")
        print(f"  Verdict: {verdict}")
        print(f"  File: {filepath}")

        if args.dry_run:
            print("\n[DRY RUN] Review saved locally.")
        return

    # =================================================================
    # TICKET-BASED MODES (per-ticket or single commit within ticket)
    # =================================================================

    # Determine which tickets to process
    if args.ticket:
        ticket_ids = [args.ticket]
        print(f"Processing ticket: #{args.ticket}")
    else:
        print("=" * 60)
        print("PHASE 1: Finding open code review tickets")
        print("=" * 60)
        open_tickets = get_open_cr_tickets(redmine_key)
        ticket_ids = [t['id'] for t in open_tickets]
        print(f"Found {len(ticket_ids)} open ticket(s)")

    if not ticket_ids:
        print("\nNo tickets to review.")
        return

    # Process each ticket
    results = {}
    reviews = {}

    mode = "single-commit" if args.commit else "per-ticket"
    print(f"\n" + "=" * 60)
    print(f"PHASE 2: Code review ({mode} mode)")
    print("=" * 60)

    for ticket_id in ticket_ids:
        ticket_data = gather_ticket_data(ticket_id, redmine_key)

        if not ticket_data:
            print(f"  Skipping ticket #{ticket_id} - could not gather data")
            continue

        # Single commit mode
        if args.commit:
            matching_commits = [
                c for c in ticket_data['commits']
                if c['hash'].startswith(args.commit)
            ]
            if not matching_commits:
                print(f"  Commit {args.commit} not found in ticket #{ticket_id}")
                continue

            commit = matching_commits[0]
            review = review_single_commit(commit, ticket_data)
            reviews[ticket_id] = review
            filepath = save_review(review, ticket_data, single_commit=commit)
            num_commits = 1

        # Per-ticket mode (default)
        else:
            review = review_ticket_per_ticket(ticket_data)
            reviews[ticket_id] = review
            filepath = save_review(review, ticket_data)
            num_commits = len(ticket_data['commits'])

        # Determine verdict for summary
        verdict = textile_review_verdict(review) or 'FEEDBACK'

        results[ticket_id] = {
            'coder': ticket_data['coder'],
            'num_commits': num_commits,
            'verdict': verdict,
            'file': filepath
        }

    if not results:
        print("\nNo reviews generated.")
        return

    # Display summary
    display_summary(results)

    if args.dry_run:
        print("\n[DRY RUN] Reviews saved locally but not posted to Redmine.")
        return

    # Confirm and post
    print("\n" + "=" * 60)
    print("PHASE 3: Confirmation")
    print("=" * 60)

    ticket_list = ', '.join(f'#{t}' for t in results.keys())
    print(f"\nReady to post reviews for tickets: {ticket_list}")

    response = input("\nPost reviews to Redmine? Enter ticket numbers separated by commas, 'all', or 'none': ").strip().lower()

    if response == 'none':
        print("No reviews posted.")
        return
    elif response == 'all':
        to_post = list(results.keys())
    else:
        try:
            to_post = [int(t.strip().replace('#', '')) for t in response.split(',')]
        except ValueError:
            print("Invalid input. No reviews posted.")
            return

    # Post reviews
    print("\nPosting reviews...")
    for tid in to_post:
        if tid in reviews:
            print(f"  Posting to ticket #{tid}...")
            success = redmine_put(
                f'/issues/{tid}.json',
                redmine_key,
                {'issue': {'notes': reviews[tid]}}
            )
            print(f"    {'SUCCESS' if success else 'FAILED'}")
        else:
            print(f"  No review found for ticket #{tid}")

    print("\n" + "=" * 60)
    print("COMPLETE")
    print("=" * 60)

if __name__ == '__main__':
    main()
