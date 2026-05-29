#!/usr/bin/env python3
"""ottoRequest.py - check ottoRequest table for pending requests
and send email notification for each one found.

Intended to run from cron.  Reads the notification email address and
table name from an hg.conf file.  Uses hgsql for database access.

Usage:
    ottoRequest.py [-c /path/to/hg.conf]

Options:
    -c, --conf    Path to hg.conf  [default: /usr/local/apache/cgi-bin/hg.conf]

cron entry in the otto user:
1,8,15,22,29,36,43,50,57 * * * * /hive/data/outside/otto/liftRequest/ottoRequest.py

"""

import argparse
import fcntl
import os
import re
import subprocess
import sys

NOTIFY_FROM = 'genome-www@soe.ucsc.edu'
BCC_BY_TYPE = {
    'liftOver': 'chain-file-request-group@ucsc.edu',
    'assembly': 'genark-request-group@ucsc.edu',
}

scriptDir = os.path.dirname(os.path.abspath(__file__))
lockPath  = os.path.join(scriptDir, "ottoRequest.lock")


def acquireSingletonLock():
    """Ensure only one instance of this script runs at a time.  Holds an
    exclusive flock on lockPath for the lifetime of the process; the
    kernel releases it on exit (including crash / kill -9), so no stale
    lock cleanup is needed.  Returns the open file handle, which the
    caller must keep alive."""
    # "a+" opens read+write without truncating (and creates if missing),
    # so a second instance that fails to lock doesn't wipe the running
    # instance's PID from the file before exiting.
    fh = open(lockPath, "a+")
    try:
        fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        sys.exit(0)
    # we own the lock truncate and write our PID for information
    fh.seek(0)
    fh.truncate()
    fh.write("%d\n" % os.getpid())
    fh.flush()
    return fh
    ### FYI: can also see the locking process via: lsof ottoRequest.lock


def parseHgConf(path):
    """Return a dict of key=value pairs from an hg.conf file.
    Handles 'include' directives with paths relative to the
    directory of the file containing the include."""
    conf = {}
    confDir = os.path.dirname(os.path.abspath(path))
    try:
        with open(path) as fh:
            for line in fh:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                if line.startswith('include '):
                    inclPath = line.split(None, 1)[1]
                    if not os.path.isabs(inclPath):
                        inclPath = os.path.join(confDir, inclPath)
                    inclConf = parseHgConf(inclPath)
                    conf.update(inclConf)
                    continue
                if '=' in line:
                    key, value = line.split('=', 1)
                    conf[key.strip()] = value.strip()
    except FileNotFoundError:
        sys.exit(f"Error: config file not found: {path}")
    except PermissionError:
        sys.exit(f"Error: cannot read config file: {path}")
    return conf


def unescapeMysql(s):
    """Reverse mysql -B batch-mode escaping: \\n -> newline,
    \\t -> tab, \\\\ -> backslash, \\0 -> NUL.  Single pass so
    \\\\n stays a literal backslash followed by 'n'."""
    out = []
    i = 0
    n = len(s)
    while i < n:
        if s[i] == '\\' and i + 1 < n:
            c = s[i+1]
            if c == 'n':
                out.append('\n')
            elif c == 't':
                out.append('\t')
            elif c == '\\':
                out.append('\\')
            elif c == '0':
                out.append('\0')
            else:
                out.append(s[i:i+2])
            i += 2
        else:
            out.append(s[i])
            i += 1
    return ''.join(out)


def hgsqlQuery(host, db, sql):
    """Run a SQL query via hgsql and return rows as list of dicts.
    hgsql -B emits tabs/newlines/backslashes inside field values as
    literal \\t / \\n / \\\\ so each row stays on one line undo that
    on each field before returning."""
    cmd = ['/cluster/bin/x86_64/hgsql', '-h', host, db, '-N', '-B', '-e', sql]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"hgsql error: {result.stderr.strip()}")
    rows = []
    if not result.stdout.strip():
        return rows
    for line in result.stdout.strip().split('\n'):
        fields = [unescapeMysql(f) for f in line.split('\t')]
        rows.append({
            'id':          fields[0],
            'requestType': fields[1],
            'fromDb':      fields[2],
            'toDb':        fields[3],
            'email':       fields[4],
            'comment':     fields[5],
            'requestTime': fields[6],
        })
    return rows


def hgsqlUpdate(db, sql):
    """Run a SQL update/insert statement via hgsql."""
    cmd = ['/cluster/bin/x86_64/hgsql', db, '-N', '-B', '-e', sql]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"hgsql update error: {result.stderr.strip()}", file=sys.stderr)
        return False
    return True


def parseAssemblyComment(comment):
    """Assembly requests pack fields into the ottoRequest.comment column
    via hubApi/findGenome.c:apiAssemblyRequest:
        name: '<name>'[; betterName: '<bn>'][; comment: '<user comment>']
    The user comment may itself contain newlines and apostrophes, so the
    inner quote pair is matched greedily up to the trailing closing quote.
    re.DOTALL lets '.' span newlines.
    Return (name, betterName, userComment)."""
    name = ''
    betterName = ''
    userComment = ''

    m = re.match(r"name: '([^']*)'(.*)$", comment, re.DOTALL)
    if not m:
        return name, betterName, comment
    name = m.group(1)
    rest = m.group(2)

    m2 = re.match(r"; betterName: '([^']*)'(.*)$", rest, re.DOTALL)
    if m2:
        betterName = m2.group(1)
        rest = m2.group(2)

    m3 = re.match(r"; comment: '(.*)'\s*$", rest, re.DOTALL)
    if m3:
        userComment = m3.group(1)
    else:
        userComment = rest.lstrip('; ')

    return name, betterName, userComment


def sendMail(toAddr, subject, body, fromAddr=None, bccAddr=None, bounceAddr=None):
    """Send email via /usr/sbin/sendmail.
    If fromAddr is provided it is used as the envelope sender (-f)
    and the From: header so that bounces return to that address.
    If bccAddr is provided, sendmail -t reads it from the header,
    delivers a copy, and strips the Bcc: line before transmission."""
    headers = f"To: {toAddr}\nSubject: {subject}"
    if bccAddr:
        headers = f"Bcc: {bccAddr}\n{headers}"
    if fromAddr:
        headers = (f"From: {fromAddr}\n"
                   f"Reply-To: {fromAddr}\n"
                   f"Return-Path: {fromAddr}\n"
                   f"{headers}")
    message = f"{headers}\n\n{body}\n"
    cmd = ['/usr/sbin/sendmail', '-t']
    if bounceAddr:
        cmd += ['-f', bounceAddr]
    result = subprocess.run(cmd, input=message, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Warning: sendmail failed: {result.stderr.strip()}",
              file=sys.stderr)
        return False
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Process pending ottoRequest entries.')
    parser.add_argument('-c', '--conf',
                        default='/usr/local/apache/cgi-bin/hg.conf',
                        help='Path to hg.conf [default: %(default)s]')
    args = parser.parse_args()

    # bind to a local so the FD stays open for the lifetime of main()
    _lock = acquireSingletonLock()  # noqa: F841

    conf = parseHgConf(args.conf)

    dbHost = conf.get('db.host')
    if not dbHost:
        sys.exit("Error: db.host not defined in config")
    dbUser = conf.get('db.user')
    if not dbUser:
        sys.exit("Error: db.user not defined in config")

    ottoDb = conf.get('ottoDb', 'hgcentraltest')
    ottoTable = conf.get('ottoTable', 'ottoRequest')

    # find pending requests
    sql = (f"SELECT id, requestType, fromDb, toDb, email, comment, "
           f"requestTime FROM {ottoTable} WHERE status = 0")
    pending = hgsqlQuery(dbHost, ottoDb, sql)

    if not pending:
        return  # nothing to do -- silent for cron

    for req in pending:
        reqType = req['requestType']
        bccAddr = BCC_BY_TYPE.get(reqType)
        if not bccAddr:
            print(f"Warning: unknown requestType '{reqType}' for"
                  f" request #{req['id']}, skipping",
                  file=sys.stderr)
            continue

        userEmail = req.get('email', '')
        if not userEmail:
            print(f"Warning: no user email for request #{req['id']},"
                  f" skipping", file=sys.stderr)
            continue

        subject = (f"UCSC Genome Browser: your {reqType}"
                   f" request has been received")
        if reqType == 'assembly':
            name, betterName, userComment = parseAssemblyComment(req['comment'])
            body = (
                f"Your assembly request has been received and is being\n"
                f"processed.\n"
                f"\n"
                f"name: '{name}'\n"
                f"email: '{userEmail}'\n"
                f"asmId: '{req['fromDb']}'\n"
                f"betterName: '{betterName}'\n"
                f"comment: '{userComment.rstrip()}'\n"
                f"date: '{req['requestTime']}'\n"
                f"\n"
                f"Will advise when this assembly is available in the genome browser.\n"
                f"\n"
                f"-- UCSC Genome Browser\n"
            )
        else:
            body = (
                f"Your alignment request has been received and is being\n"
                f"processed.\n"
                f"\n"
                f"Request details:\n"
                f"  From:      {req['fromDb']}\n"
                f"  To:        {req['toDb']}\n"
                f"  Comment:   {req['comment'].rstrip()}\n"
                f"  Submitted: {req['requestTime']}\n"
                f"\n"
                f"Will advise when this alignment is available in the genome browser.\n"
                f"\n"
                f"-- UCSC Genome Browser\n"
            )
        bitParts = ["gb", "aut", "o", "@", "uc", "sc.", "ed", "u"]
        if sendMail(userEmail, subject, body,
           fromAddr=NOTIFY_FROM, bccAddr=bccAddr, bounceAddr="".join(bitParts)):
               hgsqlUpdate(ottoDb, f"UPDATE {ottoTable} SET status = 1"
                   f" WHERE id = {req['id']}")
        else:
            print(f"Failed to send notification for request #{req['id']}",
                  file=sys.stderr)


if __name__ == '__main__':
    main()
