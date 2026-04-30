#!/usr/bin/env python3
"""ottoRequestView.cgi - web view of hgcentraltest.ottoRequest.

Read-only display of every row in the table, plus a per-row 'reset
status' control that is the only write path exposed.

Access is restricted to a single IP (UCSC VPN, 128.114.198.5).
Any other REMOTE_ADDR gets a 403.
"""

import cgi
import html
import os
import subprocess
import sys

ALLOWED_IP   = '128.114.198.5'
HGDB_CONF    = '/usr/local/apache/cgi-bin/hg.conf'
TRASH    = '/data/apache/trash'
DB           = 'hgcentraltest'
TABLE        = 'ottoRequest'

# from README.txt in this directory
STATUS_NAMES = {
    0: 'received by API',
    1: 'acknowledged, email sent',
    2: 'galaxy job started',
    3: 'galaxy done, download started',
    4: 'downloaded, track files made',
    5: 'symlinks ready, awaiting push',
    6: 'push complete',
    7: 'ERROR',
    8: 'COMPLETE (final email sent)',
}

COLS = ['id', 'requestType', 'fromDb', 'toDb', 'email', 'comment',
        'requestTime', 'status', 'buildDir', 'completeTime']


def forbidden(msg):
    sys.stdout.write("Status: 403 Forbidden\r\n")
    sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n\r\n")
    sys.stdout.write(msg + "\n")
    sys.exit(0)


def checkIp():
    remote = os.environ.get('REMOTE_ADDR', '')
    if remote != ALLOWED_IP:
        forbidden(f"Access denied for {remote!r}; this page is restricted "
                  f"to {ALLOWED_IP}.")


def unescapeMysql(s):
    """Reverse `hgsql -B` escaping (\\n, \\t, \\\\, \\0). One pass so
    \\\\n stays a literal backslash + 'n'."""
    out, i, n = [], 0, len(s)
    while i < n:
        if s[i] == '\\' and i + 1 < n:
            c = s[i+1]
            if   c == 'n':  out.append('\n')
            elif c == 't':  out.append('\t')
            elif c == '\\': out.append('\\')
            elif c == '0':  out.append('\0')
            else:           out.append(s[i:i+2])
            i += 2
        else:
            out.append(s[i]); i += 1
    return ''.join(out)


def hgsqlRun(sql):
    """Run sql via hgsql against DB. Returns (ok, stdout, stderr).
    Running under Apache the process has no ~/.hg.conf, so point hgsql
    at the cgi-bin hg.conf via HGDB_CONF."""
    env = dict(os.environ)
    env['HGDB_CONF'] = HGDB_CONF
    env['HOME'] = TRASH
    cmd = ['/cluster/bin/x86_64/hgsql', DB, '-N', '-B', '-e', sql]
    r = subprocess.run(cmd, capture_output=True, text=True, env=env)
    return (r.returncode == 0, r.stdout, r.stderr)


def fetchRows():
    sql = f"SELECT {','.join(COLS)} FROM {TABLE} ORDER BY id DESC"
    ok, out, err = hgsqlRun(sql)
    if not ok:
        raise RuntimeError(err.strip() or 'hgsql failed')
    rows = []
    if out.strip():
        for line in out.rstrip('\n').split('\n'):
            rows.append([unescapeMysql(f) for f in line.split('\t')])
    return rows


def doResetStatus(form):
    rid  = form.getfirst('id', '')
    stat = form.getfirst('status', '')
    if not rid.isdigit():
        return None, f"bad id: {rid!r}"
    if not stat.isdigit() or int(stat) not in STATUS_NAMES:
        return None, f"bad status: {stat!r}"
    sql = (f"UPDATE {TABLE} SET status = {int(stat)} "
           f"WHERE id = {int(rid)}")
    ok, _out, err = hgsqlRun(sql)
    if not ok:
        return None, err.strip() or 'hgsql update failed'
    return (f"id={rid} status set to {stat} "
            f"({STATUS_NAMES[int(stat)]})"), None


def renderPage(rows, info=None, error=None):
    sys.stdout.write("Content-Type: text/html; charset=utf-8\r\n\r\n")
    out = sys.stdout.write

    out('<!DOCTYPE html>\n<html><head><meta charset="utf-8">\n')
    out(f'<title>{TABLE}</title>\n')
    out('<style>\n'
        'body{font-family:sans-serif;margin:1em;font-size:13px}\n'
        'h2{margin:.2em 0}\n'
        'table{border-collapse:collapse;width:100%;margin-top:.5em}\n'
        'th,td{border:1px solid #ccc;padding:3px 6px;vertical-align:top;'
        'font-size:12px}\n'
        'th{background:#eee;text-align:left;position:sticky;top:0}\n'
        'tr:nth-child(even){background:#f8f8f8}\n'
        'td.comment{max-width:28em;white-space:pre-wrap;'
        'word-break:break-word}\n'
        'tr.s7 td{background:#ffe0e0}\n'
        'tr.s8 td{background:#e0f0e0;color:#555}\n'
        'select,button{font-size:12px}\n'
        '.banner{padding:.5em;margin:.4em 0;border-radius:4px}\n'
        '.info {background:#dfd;border:1px solid #5a5}\n'
        '.error{background:#fdd;border:1px solid #a55}\n'
        '.legend{font-size:12px;color:#555;margin:.4em 0}\n'
        '.legend code{background:#eee;padding:0 3px}\n'
        '</style></head><body>\n')

    out(f'<h2>{DB}.{TABLE}</h2>\n')
    if info:
        out(f'<div class="banner info">{html.escape(info)}</div>\n')
    if error:
        out(f'<div class="banner error">{html.escape(error)}</div>\n')

    out('<div class="legend">status: ')
    out(' &middot; '.join(f'<code>{k}</code>={html.escape(v)}'
                          for k, v in STATUS_NAMES.items()))
    out(f' &middot; <b>{len(rows)}</b> row(s) &middot; '
        f'<a href="">refresh</a></div>\n')

    out('<table>\n<tr>')
    for c in COLS:
        out(f'<th>{c}</th>')
    out('<th>set status</th></tr>\n')

    for r in rows:
        rid = r[0]
        try:
            stnum = int(r[7])
        except (ValueError, IndexError):
            stnum = -1
        cls = f's{stnum}' if stnum in (7, 8) else ''
        out(f'<tr class="{cls}">')
        for i, c in enumerate(COLS):
            cell = r[i] if i < len(r) else ''
            if c == 'comment':
                out(f'<td class="comment">{html.escape(cell)}</td>')
            elif c == 'status':
                label = STATUS_NAMES.get(stnum, '?')
                out(f'<td><b>{html.escape(cell)}</b> '
                    f'<small>{html.escape(label)}</small></td>')
            else:
                out(f'<td>{html.escape(cell)}</td>')
        # reset form
        out('<td><form method="post" '
            'onsubmit="return confirm(\'Reset status of id=' +
            html.escape(rid) + ' to \' + this.status.value + \'?\')">'
            '<input type="hidden" name="action" value="resetStatus">'
            f'<input type="hidden" name="id" value="{html.escape(rid)}">'
            '<select name="status">')
        for k in sorted(STATUS_NAMES):
            sel = ' selected' if k == stnum else ''
            out(f'<option value="{k}"{sel}>{k}</option>')
        out('</select> <button type="submit">set</button></form></td>')
        out('</tr>\n')
    out('</table>\n</body></html>\n')


def main():
    checkIp()

    info = error = None
    if os.environ.get('REQUEST_METHOD', 'GET') == 'POST':
        form = cgi.FieldStorage()
        action = form.getfirst('action', '')
        if action == 'resetStatus':
            info, error = doResetStatus(form)
        else:
            error = f"unknown action: {action!r}"

    try:
        rows = fetchRows()
    except RuntimeError as e:
        rows = []
        error = (error + ' / ' if error else '') + f"fetch failed: {e}"

    renderPage(rows, info=info, error=error)


if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n\r\n")
        sys.stdout.write(f"ottoRequestView.cgi error: {e}\n")
