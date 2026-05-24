#!/usr/bin/env python3
"""ottoRequestView.cgi - web view of hgcentraltest.ottoRequest.

Read-only display of every row in the table, plus a per-row 'reset
status' control that is the only write path exposed.

Access is restricted to a single IP (UCSC VPN, 128.114.198.5).
Any other REMOTE_ADDR gets a 403.
"""

import cgi
import html
import json
import os
import re
import subprocess
import sys
import time
import urllib.parse
from datetime import datetime

ALLOWED_IP   = '128.114.198.5'
HGDB_CONF    = '/usr/local/apache/cgi-bin/hg.conf'
TRASH    = '/data/apache/trash'
DB           = 'hgcentraltest'
TABLE        = 'ottoRequest'

# Galaxy queue status panel - snapshot is refreshed by ottoRequestWatch.sh
# (cron, every 11 minutes), CGI just reads it.
CACHE_PATH = '/data/apache/trash/ottoRequestGalaxyStatus.json'
CACHE_TTL  = 1800  # seconds; older than this -> show "stale" instead

# featureBits coverage snapshot - append-only file maintained by
# featureBitsSnapshot.py (cron, via ottoRequestWatch.sh).  fb.*.txt
# values are immutable once an alignment completes so no TTL is needed;
# featureBitsPct() falls back to an NFS read on a snapshot miss.
FB_SNAPSHOT_PATH = '/data/apache/trash/ottoRequestFeatureBitsPct.json'

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

# featureBits coverage lookup roots
HIVE_GENOMES = '/hive/data/genomes'
ASMHUB_ROOT  = HIVE_GENOMES + '/asmHubs'

# in-process caches; one CGI invocation only, but rows reuse same accessions
_buildDirCache = {}
_fbPctCache    = {}
_genarkAsmName = {}    # populated up-front by loadGenarkNames()
_fbSnapshot    = {}    # populated up-front by loadFeatureBitsSnapshot()


def forbidden(msg):
    sys.stdout.write("Status: 403 Forbidden\r\n")
    sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n\r\n")
    sys.stdout.write(msg + "\n")
    sys.exit(0)


def checkIp():
    remote = os.environ.get('REMOTE_ADDR', '')
    if remote != ALLOWED_IP:
        forbidden(f"Access denied for {remote!r}; this page is restricted.")


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
    cmd = ['/cluster/bin/x86_64/hgsql', '-profile=central', DB, '-N', '-B', '-e', sql]
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


def loadGenarkNames(accessions):
    """Populate _genarkAsmName: {gcAccession: asmName} for the given
    accessions in one bulk hgsql call against the genark table.  Lets
    hubBuildDir() construct paths directly instead of NFS-listdir'ing
    /hive/data/genomes/asmHubs/.../<XXX>/<XXX>/<XXX>/ to discover the
    asmName suffix on each accession."""
    if not accessions:
        return
    quoted = ",".join("'%s'" % a for a in sorted(accessions))
    sql = (f"SELECT gcAccession, asmName FROM genark "
           f"WHERE gcAccession IN ({quoted});")
    ok, out, _err = hgsqlRun(sql)
    if not ok or not out.strip():
        return
    for line in out.rstrip('\n').split('\n'):
        parts = line.split('\t')
        if len(parts) >= 2:
            _genarkAsmName[parts[0]] = parts[1]


def hubBuildDir(acc):
    """Locate the hive build directory for a fromDb/toDb value.
    GenArk accession (GCA_/GCF_) -> asmHubs/{genbank,refseq}Build/<XXX>/<XXX>/<XXX>/<acc>_<asmName>
        asmName comes from _genarkAsmName, populated up-front by
        loadGenarkNames() from the genark table.
    UCSC native db (e.g. hg38)   -> /hive/data/genomes/<db>
    Returns absolute path or None."""
    if not acc:
        return None
    if acc in _buildDirCache:
        return _buildDirCache[acc]
    result = None
    if (acc.startswith('GCF_') or acc.startswith('GCA_')) and len(acc) >= 13:
        asmName = _genarkAsmName.get(acc)
        if asmName:
            src    = acc[:3]
            sub    = 'refseqBuild' if src == 'GCF' else 'genbankBuild'
            digits = acc[4:].split('.', 1)[0]
            if len(digits) >= 9:
                result = (f'{ASMHUB_ROOT}/{sub}/{src}/'
                          f'{digits[0:3]}/{digits[3:6]}/{digits[6:9]}/'
                          f'{acc}_{asmName}')
    else:
        candidate = f'{HIVE_GENOMES}/{acc}'
        if os.path.isdir(candidate):
            result = candidate
    _buildDirCache[acc] = result
    return result


def loadFeatureBitsSnapshot():
    """Populate _fbSnapshot from the JSON file written by
    featureBitsSnapshot.py via cron.  Silent no-op if the file is
    missing or malformed - featureBitsPct() falls back to an NFS read
    on a snapshot miss, so the page still renders correctly."""
    try:
        with open(FB_SNAPSHOT_PATH) as f:
            data = json.load(f)
    except (OSError, ValueError):
        return
    _fbSnapshot.update(data.get('pct') or {})


def featureBitsPct(srcAcc, qryAcc):
    """Return percentage from fb.<srcAcc>.chain<qryAcc>Link.txt (% of srcAcc
    covered by chains to qryAcc), or '' if unavailable.

    Two-tier lookup: the precomputed cron snapshot first (pure dict
    lookup, no I/O); on miss falls back to the NFS file read so
    freshly-completed rows still show a value before the next cron
    tick promotes them."""
    if not srcAcc or not qryAcc:
        return ''
    key = (srcAcc, qryAcc)
    if key in _fbPctCache:
        return _fbPctCache[key]
    snapKey = f'{srcAcc}\t{qryAcc}'
    if snapKey in _fbSnapshot:
        pct = _fbSnapshot[snapKey]
        _fbPctCache[key] = pct
        return pct
    bdir = hubBuildDir(srcAcc)
    pct  = ''
    if bdir:
        # GenArk builds keep lastz under trackData/, UCSC native under bed/
        sub = 'trackData' if '/asmHubs/' in bdir else 'bed'
        # chain<QryAcc>Link.txt: first letter of query is capitalized
        # (matches the ${dstDb^} convention in installLinks).  No-op for
        # GCA_*/GCF_* accessions; converts hg38 -> Hg38 for native dbs.
        QryAcc = qryAcc[:1].upper() + qryAcc[1:]
        path = (f'{bdir}/{sub}/lastz.{qryAcc}/'
                f'fb.{srcAcc}.chain{QryAcc}Link.txt')
        try:
            with open(path) as f:
                txt = f.read()
            m = re.search(r'\(([\d.]+)%\)', txt)
            if m:
                pct = m.group(1) + '%'
        except OSError:
            pass
    _fbPctCache[key] = pct
    return pct


def elapsedStr(reqTime, doneTime):
    """Human-readable elapsed time between two MySQL datetimes.
    Empty string if either side is missing/NULL/unparseable."""
    if not reqTime or not doneTime or reqTime == 'NULL' or doneTime == 'NULL':
        return ''
    try:
        t0 = datetime.strptime(reqTime,  '%Y-%m-%d %H:%M:%S')
        t1 = datetime.strptime(doneTime, '%Y-%m-%d %H:%M:%S')
    except ValueError:
        return ''
    secs = int((t1 - t0).total_seconds())
    if secs < 0:
        return ''
    d, secs = divmod(secs, 86400)
    h, secs = divmod(secs, 3600)
    m, s    = divmod(secs, 60)
    if d: return f'{d}d {h}h {m}m'
    if h: return f'{h}h {m}m'
    if m: return f'{m}m {s}s'
    return f'{s}s'


def loadGalaxyStatus():
    """Return the Galaxy queue snapshot written by ottoRequestWatch.sh
    (which calls galaxyStatus.py from cron).  Returns the parsed dict
    with an added 'stale' flag when the file is older than CACHE_TTL,
    or None if the file is missing/unreadable."""
    try:
        mtime = os.path.getmtime(CACHE_PATH)
        with open(CACHE_PATH) as f:
            data = json.load(f)
    except (OSError, ValueError):
        return None
    data['stale'] = (time.time() - mtime) > CACHE_TTL
    return data


def renderPage(rows, info=None, error=None, galaxyStatus=None):
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
        '.legend{font-size:15px;color:#333;margin:.4em 0}\n'
        '.legend code{background:#eee;padding:0 3px;font-size:14px}\n'
        '.refreshBtn{font-size:14px;padding:3px 10px;margin-left:6px;'
        'cursor:pointer}\n'
        '.toggleBtn{font-size:14px;padding:3px 10px;margin-left:6px;'
        'cursor:pointer;background:#f0f0f0;border:1px solid #ccc}\n'
        '.hide-complete tr.s8{display:none}\n'
        '</style></head><body>\n')

    out(f'<h2>{DB}.{TABLE}</h2>\n')
    if galaxyStatus:
        staleNote = ' <b>[stale]</b>' if galaxyStatus.get('stale') else ''
        out('<div class="legend">Galaxy queue: '
            f'<b>{galaxyStatus.get("running", "?")}</b> running &middot; '
            f'<b>{galaxyStatus.get("queued",  "?")}</b> queued &middot; '
            f'<b>{galaxyStatus.get("new",     "?")}</b> new '
            f'<small>(as of {html.escape(galaxyStatus.get("ts", ""))})</small>'
            f'{staleNote}</div>\n')
    else:
        out('<div class="legend">Galaxy queue: '
            '<i>status unavailable</i></div>\n')
    if info:
        out(f'<div class="banner info">{html.escape(info)}</div>\n')
    if error:
        out(f'<div class="banner error">{html.escape(error)}</div>\n')

    out('<div class="legend">status: ')
    out(' &middot; '.join(f'<code>{k}</code>={html.escape(v)}'
                          for k, v in STATUS_NAMES.items()))
    # Count completed rows for the toggle button label
    completed_count = sum(1 for r in rows if len(r) > 7 and r[7] == '8')
    out(f' &middot; <b>{len(rows)}</b> row(s)'
        '<button class="refreshBtn" type="button" '
        'onclick="location.reload()">refresh</button>'
        f'<button class="toggleBtn" type="button" id="toggleComplete" '
        f'onclick="toggleCompleted()">hide completed ({completed_count})</button></div>\n')
    out(f'<div class="legend">cron times: 9,20,31,42,53 for ottoRequestWatch.sh, and 4,26,46 for ottoRequestPush and 1,8,15,22,29,36,43,50,57 for the first acknowledgement</div>\n')

    out('<table class="sortable">\n<tr>')
    for c in COLS:
        out(f'<th>{c}</th>')
    out('<th title="% of fromDb covered by chains to toDb / '
        '% of toDb covered by chains to fromDb">'
        'coverage<br><small>from / to</small></th>'
        '<th>elapsed</th><th>set status</th></tr>\n')

    reqIdx  = COLS.index('requestTime')
    doneIdx = COLS.index('completeTime')
    fromIdx = COLS.index('fromDb')
    toIdx   = COLS.index('toDb')

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
            elif c in ('fromDb', 'toDb') and cell:
                href = ('https://genome-test.gi.ucsc.edu/cgi-bin/hgTracks?db='
                        + urllib.parse.quote(cell, safe=''))
                out(f'<td><a href="{html.escape(href)}" target="_blank">'
                    f'{html.escape(cell)}</a></td>')
            elif c == 'email' and '@' in cell:
                user = cell.split('@', 1)[0]
                out(f'<td title="{html.escape(cell)}">'
                    f'{html.escape(user)}</td>')
            else:
                out(f'<td>{html.escape(cell)}</td>')
        fromAcc = r[fromIdx] if fromIdx < len(r) else ''
        toAcc   = r[toIdx]   if toIdx   < len(r) else ''
        fwd     = featureBitsPct(fromAcc, toAcc)
        rev     = featureBitsPct(toAcc, fromAcc)
        if fwd or rev:
            out(f'<td>{html.escape(fwd or "-")} / '
                f'{html.escape(rev or "-")}</td>')
        else:
            out('<td></td>')

        elapsed = elapsedStr(r[reqIdx]  if reqIdx  < len(r) else '',
                             r[doneIdx] if doneIdx < len(r) else '')
        out(f'<td>{html.escape(elapsed)}</td>')
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
    out('</table>\n')
    out('<script src="/js/sorttable.js"></script>\n')
    out('<script>\n'
        'function toggleCompleted() {\n'
        '    var table = document.querySelector("table");\n'
        '    var btn = document.getElementById("toggleComplete");\n'
        '    var isHidden = table.classList.contains("hide-complete");\n'
        '    if (isHidden) {\n'
        '        table.classList.remove("hide-complete");\n'
        f'        btn.textContent = "hide completed ({completed_count})";\n'
        '        localStorage.setItem("hideCompleted", "false");\n'
        '    } else {\n'
        '        table.classList.add("hide-complete");\n'
        '        btn.textContent = "show completed";\n'
        '        localStorage.setItem("hideCompleted", "true");\n'
        '    }\n'
        '}\n'
        '// Restore toggle state from localStorage\n'
        'window.addEventListener("load", function() {\n'
        '    if (localStorage.getItem("hideCompleted") === "true") {\n'
        '        var table = document.querySelector("table");\n'
        '        var btn = document.getElementById("toggleComplete");\n'
        '        table.classList.add("hide-complete");\n'
        '        btn.textContent = "show completed";\n'
        '    }\n'
        '});\n'
        '</script>\n')
    out('</body></html>\n')

def main():
#   checkIp()

    # POST/Redirect/GET: handle the write, then 303 to a GET of the same URL
    # so a browser reload doesn't re-submit the form and re-run the UPDATE.
    if os.environ.get('REQUEST_METHOD', 'GET') == 'POST':
        form = cgi.FieldStorage()
        action = form.getfirst('action', '')
        if action == 'resetStatus':
            info, error = doResetStatus(form)
        else:
            info, error = None, f"unknown action: {action!r}"
        params = {}
        if info:  params['info']  = info
        if error: params['error'] = error
        qs = ('?' + urllib.parse.urlencode(params)) if params else ''
        sys.stdout.write(f"Status: 303 See Other\r\n"
                         f"Location: {os.environ.get('SCRIPT_NAME','')}{qs}"
                         f"\r\n\r\n")
        return

    # GET: pick up banner messages left by the PRG redirect, if any
    qs = cgi.FieldStorage()
    info  = qs.getfirst('info')  or None
    error = qs.getfirst('error') or None

    try:
        rows = fetchRows()
    except RuntimeError as e:
        rows = []
        error = (error + ' / ' if error else '') + f"fetch failed: {e}"

    # one bulk lookup of GenArk asmNames so hubBuildDir() avoids NFS readdir
    fromIdx = COLS.index('fromDb')
    toIdx   = COLS.index('toDb')
    gcAccs = set()
    for r in rows:
        for idx in (fromIdx, toIdx):
            if idx < len(r):
                v = r[idx]
                if v.startswith('GCA_') or v.startswith('GCF_'):
                    gcAccs.add(v)
    loadGenarkNames(gcAccs)
    loadFeatureBitsSnapshot()

    galaxyStatus = loadGalaxyStatus()

    renderPage(rows, info=info, error=error, galaxyStatus=galaxyStatus)


if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n\r\n")
        sys.stdout.write(f"ottoRequestView.cgi error: {e}\n")
