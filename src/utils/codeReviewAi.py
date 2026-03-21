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
import subprocess
import argparse
import requests
from datetime import datetime, timedelta
from email.mime.text import MIMEText
from collections import defaultdict

# Configuration
REDMINE_URL = "https://redmine.gi.ucsc.edu"
GIT_REPORTS_PATH = "/hive/groups/qa/git-reports-history"
GIT_REPO_PATH = "/data/git/kent.git"
OUTPUT_DIR = "/hive/users/lrnassar/codeReview"
MLQ_CONF_PATH = os.path.expanduser("~/.hg.conf")
DEFAULT_CC = "browser-code-reviews-group@ucsc.edu"
GMAIL_TOKEN_PATH = os.path.expanduser("~/.gmail_token.json")
GMAIL_CREDS_PATH = os.path.expanduser("~/.gmail_credentials.json")
GMAIL_SCOPES = [
    'https://www.googleapis.com/auth/gmail.send',
]
CLAUDE_CLI = os.path.expanduser('~/.local/bin/claude')

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

h3. Verdict: APPROVED / FEEDBACK

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

h3. Status: APPROVED / FEEDBACK

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

    if 'DAILY CODE REVIEW' not in response:
        return False, "Missing DAILY CODE REVIEW header"

    if 'APPROVED' not in response and 'FEEDBACK' not in response:
        return False, "Missing verdict section"

    if len(response) < 500:
        return False, f"Response too short ({len(response)} chars) - may be incomplete"

    return True, "OK"

def call_claude_cli(prompt, timeout=600, retries=1, validator=None):
    """Call Claude Code CLI with a prompt and return the response"""
    if validator is None:
        validator = validate_review_output
    for attempt in range(retries + 1):
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
            print(f"  WARNING: Claude CLI timed out after {timeout}s")
            if attempt < retries:
                print(f"  Retrying...")
                continue
            return None
        except Exception as e:
            print(f"  ERROR calling Claude CLI: {e}")
            if attempt < retries:
                print(f"  Retrying...")
                continue
            return None

    return None

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
    print(f"  Calling Claude CLI (this may take a few minutes)...")

    response = call_claude_cli(prompt, timeout=600)

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

h3. Verdict: APPROVED / FEEDBACK

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

h3. Verdict: APPROVED / FEEDBACK

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

def get_commits_since(hours):
    """Get all commits from the last N hours, grouped by author"""
    since = datetime.now() - timedelta(hours=hours)
    since_str = since.strftime('%Y-%m-%d %H:%M:%S')

    # Get commits with author name, email, hash, and subject
    result = subprocess.run(
        ['git', f'--git-dir={GIT_REPO_PATH}', 'log',
         f'--since={since_str}', '--format=%H%n%an%n%ae%n%s', '--no-merges'],
        capture_output=True, text=True, timeout=60
    )
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


def build_daily_review_prompt(author_name, commits):
    """Build a prompt for reviewing all of one author's daily commits"""

    commits_list = []
    commit_hashes = []
    for i, c in enumerate(commits, 1):
        refs = ', '.join('#' + r for r in c['referenced_issues']) or 'None'
        commits_list.append(f"  {i}. @{c['short_hash']}@ - {c['message'][:80]}")
        commits_list.append(f"     Referenced issues: {refs}")
        commit_hashes.append(c['hash'])

    commits_section = "\n".join(commits_list)
    hashes_section = " ".join(commit_hashes)

    prompt = f"""You are performing a daily code review of all commits by {author_name} in the UCSC Genome Browser kent repository from the last 24 hours.

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

Verdict: APPROVED / FEEDBACK
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
OVERALL STATUS: APPROVED / FEEDBACK
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

BEGIN YOUR REVIEW NOW. Use your tools to investigate thoroughly.
"""
    return prompt


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


def send_review_email(gmail_service, to_email, author_name, review_text, cc=None):
    """Send a code review email to the author"""
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


def review_daily_author(author_name, commits, log_dir):
    """Review all commits by one author for the daily digest.
    Temp files (prompts/responses) are written to log_dir and returned for cleanup."""
    print(f"\n{'='*60}")
    print(f"REVIEWING DAILY COMMITS: {author_name}")
    print(f"Commits: {len(commits)}")
    print(f"{'='*60}")

    prompt = build_daily_review_prompt(author_name, commits)

    # Save prompt to log_dir for debugging (cleaned up on success)
    safe_name = re.sub(r'[^a-zA-Z0-9]', '_', author_name)
    date_str = datetime.now().strftime('%Y%m%d')
    temp_files = []

    prompt_file = os.path.join(log_dir, f".tmp_daily_prompt_{safe_name}_{date_str}.txt")
    with open(prompt_file, 'w') as f:
        f.write(prompt)
    temp_files.append(prompt_file)
    print(f"  Prompt saved to: {prompt_file}")
    print(f"  Calling Claude CLI (this may take a few minutes)...")

    response = call_claude_cli(prompt, timeout=600, validator=validate_daily_review_output)

    if response:
        response_file = os.path.join(log_dir, f".tmp_daily_response_{safe_name}_{date_str}.txt")
        with open(response_file, 'w') as f:
            f.write(response)
        temp_files.append(response_file)
        # Strip any preamble before "DAILY CODE REVIEW"
        match = re.search(r'^DAILY CODE REVIEW', response, re.MULTILINE)
        if match:
            response = response[match.start():]
        print(f"  Review complete")
    else:
        print(f"  WARNING: No response received")
        response = f"DAILY CODE REVIEW - {author_name}\n\nError: Review generation failed - no response from Claude CLI.\n"

    return response, temp_files


def run_daily_mode(hours, cc_address, dry_run, log_dir):
    """Run daily review mode: get recent commits, review per author, email results"""
    os.makedirs(log_dir, exist_ok=True)

    print("=" * 60)
    print(f"DAILY CODE REVIEW MODE")
    print(f"Looking back: {hours} hours")
    print(f"CC: {cc_address or 'None'}")
    print(f"Log dir: {log_dir}")
    print(f"Dry run: {dry_run}")
    print("=" * 60)

    # Phase 1: Gather commits
    print(f"\nPhase 1: Gathering commits from the last {hours} hours...")
    authors = get_commits_since(hours)

    if not authors:
        print("No commits found in the specified time window.")
        return

    total_commits = sum(len(a['commits']) for a in authors.values())
    print(f"Found {total_commits} commit(s) from {len(authors)} author(s):")
    for email, data in authors.items():
        print(f"  {data['name']} <{email}>: {len(data['commits'])} commit(s)")

    # Phase 2: Review each author's commits
    print(f"\nPhase 2: Reviewing commits...")
    reviews = {}
    all_temp_files = []
    for author_email, data in authors.items():
        review, temp_files = review_daily_author(data['name'], data['commits'], log_dir)
        all_temp_files.extend(temp_files)
        reviews[author_email] = {
            'name': data['name'],
            'email': author_email,
            'review': review,
            'num_commits': len(data['commits']),
        }

        # Save review to log_dir
        safe_name = re.sub(r'[^a-zA-Z0-9]', '_', data['name'])
        date_str = datetime.now().strftime('%Y%m%d')
        filepath = os.path.join(log_dir, f"daily_review_{safe_name}_{date_str}.txt")
        with open(filepath, 'w') as f:
            f.write(review)
        reviews[author_email]['file'] = filepath
        print(f"  Saved: {filepath}")

    # Phase 3: Send emails (only for reviews with FEEDBACK)
    print(f"\nPhase 3: Sending emails (FEEDBACK only)...")
    if dry_run:
        print("[DRY RUN] Emails not sent. Reviews saved locally:")
        for author_email, data in reviews.items():
            verdict = 'FEEDBACK' if 'OVERALL STATUS: FEEDBACK' in data['review'] else 'APPROVED'
            print(f"  {data['name']} <{author_email}>: {verdict} - {data['file']}")
    else:
        gmail_service = get_gmail_service()
        for author_email, data in reviews.items():
            has_feedback = 'OVERALL STATUS: FEEDBACK' in data['review']
            if not has_feedback:
                print(f"  {data['name']}: APPROVED - skipping email")
                continue
            print(f"  Emailing {data['name']} <{author_email}> (FEEDBACK)...")
            try:
                send_review_email(gmail_service, author_email, data['name'], data['review'], cc=cc_address)
                print(f"    SENT")
            except Exception as e:
                print(f"    FAILED: {e}")

    # Clean up temp files
    for f in all_temp_files:
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
        verdict = 'FEEDBACK' if 'OVERALL STATUS: FEEDBACK' in data['review'] else 'APPROVED'
        print(f"  {data['name']}: {data['num_commits']} commits - {verdict}")


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
    parser.add_argument('--cc', type=str, default=DEFAULT_CC,
                        help=f'CC address for --daily emails (default: {DEFAULT_CC})')
    parser.add_argument('--log-dir', type=str,
                        default=os.path.expanduser('~/codeReviewLogs'),
                        help='Directory for daily review logs and output (default: ~/codeReviewLogs)')
    args = parser.parse_args()

    # =================================================================
    # DAILY MODE (--daily)
    # =================================================================
    if args.daily:
        run_daily_mode(args.hours, args.cc, args.dry_run, args.log_dir)
        return

    # Load configuration
    config = load_config()
    redmine_key = config.get('redmine.apiKey')

    if not redmine_key:
        print("ERROR: redmine.apiKey not found in config")
        sys.exit(1)

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
        verdict = 'FEEDBACK' if 'Verdict: FEEDBACK' in review else 'APPROVED'
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
        verdict = 'FEEDBACK' if 'Status: FEEDBACK' in review else 'APPROVED'

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
