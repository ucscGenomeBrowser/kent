#!/usr/bin/env python3
"""ottoRequest.py - check ottoRequest table for pending requests
and send email notification for each one found.

Intended to run from cron.  Reads the notification email address and
table name from an hg.conf file.  Uses hgsql for database access.

Usage:
    ottoRequest.py [-c /path/to/hg.conf]

Options:
    -c, --conf    Path to hg.conf  [default: /usr/local/apache/cgi-bin/hg.conf]
"""

import argparse
import os
import subprocess
import sys


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


def hgsqlQuery(db, sql):
    """Run a SQL query via hgsql and return rows as list of dicts."""
    cmd = ['hgsql', db, '-N', '-B', '-e', sql]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"hgsql error: {result.stderr.strip()}")
    rows = []
    if not result.stdout.strip():
        return rows
    for line in result.stdout.strip().split('\n'):
        fields = line.split('\t')
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
    cmd = ['hgsql', db, '-N', '-B', '-e', sql]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"hgsql update error: {result.stderr.strip()}", file=sys.stderr)
        return False
    return True


def sendMail(toAddr, subject, body):
    """Send email via /usr/sbin/sendmail."""
    message = f"To: {toAddr}\nSubject: {subject}\n\n{body}\n"
    result = subprocess.run(['/usr/sbin/sendmail', '-t'],
                            input=message, capture_output=True, text=True)
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

    conf = parseHgConf(args.conf)

    dbName = conf.get('central.db')
    if not dbName:
        sys.exit("Error: central.db not defined in config")

    table = conf.get('ottoTable', 'ottoRequest')

    notifyEmail = conf.get('chainFileRequestEmail')
    if not notifyEmail:
        sys.exit("Error: chainFileRequestEmail not defined in config")

    # find pending requests
    sql = (f"SELECT id, requestType, fromDb, toDb, email, comment, "
           f"requestTime FROM {table} WHERE doneStatus = 0")
    pending = hgsqlQuery(dbName, sql)

    if not pending:
        return  # nothing to do -- silent for cron

    for req in pending:
        subject = f"ottoRequest #{req['id']}: {req['requestType']} pending"
        body = (
            f"##################################################\n"
            f"Pending {req['requestType']} request #{req['id']}\n"
            f"\n"
            f"  From:    {req['fromDb']}\n"
            f"  To:      {req['toDb']}\n"
            f"  Email:   {req['email']}\n"
            f"  Comment: {req['comment']}\n"
            f"  Time:    {req['requestTime']}\n"
            f"##################################################\n"
            f"testing ottoRequest watch cron job\n"
            f"##################################################\n"
        )
        if sendMail(notifyEmail, subject, body):
#           print(f"Notified {notifyEmail} about request #{req['id']}")
            hgsqlUpdate(dbName,
                f"UPDATE {table} SET doneStatus = 1"
                f" WHERE id = {req['id']}")
        else:
            print(f"Failed to notify about request #{req['id']}",
                  file=sys.stderr)


if __name__ == '__main__':
    main()
