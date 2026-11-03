#!/usr/bin/env python3
"""Weekly trends in the RR and mirror Apache error logs (monthly qateam cron, refs #33007).

Reads every node's weekly error_log.YYYYMMDD.gz once, counts the metrics in METRICS,
and keeps the result for each week in a JSON cache, so a monthly run only reads the
weeks it has not seen. Writes, for the last 12 complete weeks:
    index.html      the numbers, the graphs, and the message types that changed most
    values.tsv      week, node, metric, value (node "all" = every node together)
    byCgi.tsv       week, metric, cgi, count, for requests, hogExit and stack dumps
    signatures.tsv  week, cgi, count, message type (numbers and ids replaced by N and X)
    <metric>.png    one graph per metric, all nodes together plus one line per node

Counting rules, so each number means one thing:
  - A request is one "CGI_TIME:" line; every C CGI writes exactly one as it exits.
    Python CGIs (hgGeneGraph) write none, so they only appear through hogExit.
  - Unique IPs and subnets come from request lines and hogExit lines only. Counting
    every line with a [client] field makes the number follow whatever trace lines are
    being logged that month (CAPTCHA_PRINT in 2026, see #38247 and #38683).
  - Stack dumps count the "Stack dump:" line itself, not the gdb frame that repeats it.
  - The weekly file name is the week's first day. A week is used once it is over.
"""

import argparse
import datetime
import html
import io
import itertools
import json
import os
import re
import shutil
import subprocess
import sys
from collections import Counter
from multiprocessing import Pool

import matplotlib
matplotlib.use('Agg')  # no display on the cron host
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import matplotlib.ticker as mticker

WWWSTATS = '/hive/data/inside/wwwstats'
# node name -> log directory under WWWSTATS; {year} is the year of the week's first day
NODES = {
    'hgw0': 'RR/{year}/hgw0',
    'hgw1': 'RR/{year}/hgw1',
    'hgw2': 'RR/{year}/hgw2',
    'euro': 'euroNode/{year}',
    'asia': 'asiaNode/{year}',
}
REFERENCE_NODE = 'hgw1'  # the node whose file list decides which weeks exist
CACHE_VERSION = 2        # bump when a metric definition changes, so old weeks are recounted
TOP_SIGNATURES = 60      # message types kept per week
PRUNE_AT = 400000        # signature counter size that triggers dropping the rare ones

# (key, label, description). For node "all", unique counts are taken across all nodes.
METRICS = [
    ('requests', 'CGI requests',
     'One CGI_TIME line per request that reached a C CGI.'),
    ('lines', 'Log lines',
     'All lines. Mostly follows how much the CGIs write per request, see the message table.'),
    ('uniqueIps', 'Unique client IPs',
     'Distinct client IPs on request and hogExit lines.'),
    ('uniqueSubnets', 'Unique /16 subnets',
     'Distinct first two parts of those IPv4 addresses.'),
    ('uniqueHgsids', 'Unique hgsids',
     'Distinct cart ids seen anywhere in a line. A visitor without cookies gets a new cart '
     'on every page, so crawlers inflate this.'),
    ('captchaSolved', 'Captchas solved',
     'CAPTCHA_VALID lines: a Turnstile token that Cloudflare accepted.'),
    ('hogExit', 'hogExit (blocked requests)',
     'Requests refused by the bottleneck, one line each (C CGIs and hgLib.py).'),
    ('stackDumps', 'Stack dumps',
     'errAbort stack dumps, one per dump.'),
    ('allocLimit', 'needMem over limit',
     'needMem: trying to allocate N bytes: more than the hard-wired limit, or 0 bytes.'),
    ('outOfMemory', 'Out of memory',
     'needMem: Out of memory - malloc failed or hit the hg.conf maxMem limit.'),
    ('ctErrors', 'Custom track load errors',
     'hgCustom load error lines.'),
    ('ctSaved', 'Custom tracks saved',
     'customTrack: saved, excluding hgRenderTracks, whose tracks are temporary.'),
    ('sessionLoads', 'Session loads',
     'hgTracks requests whose referer is hgSession.'),
    ('collectionsExpire', 'Expired track collections',
     'Track Collections expire 48 lines.'),
    ('warnTiming', 'warnTiming',
     'Requests slower than the hg.conf warnSeconds limit.'),
]
BY_CGI = ('requests', 'hogExit', 'stackDumps')

RE_LEADING_BRACKETS = re.compile(r'^(?:\[[^\]]*\] ?)+')
RE_APACHE_CODE = re.compile(r'^AH\d+: ')
RE_LONG_TOKEN = re.compile(r'[A-Za-z0-9_\-]{20,}')
RE_DIGITS = re.compile(r'\d+')
RE_HGSID = re.compile(r'hgsid=(\d+)_')
RE_ALLOC = re.compile(r'needMem: trying to allocate \d')
RE_OOM = re.compile(r'needMem: Out of memory - request size \d')


def logPath(node, week):
    """Return the path of a node's weekly error log."""
    return os.path.join(WWWSTATS, NODES[node].format(year=week[:4]), 'error_log.%s.gz' % week)


def completeWeeks(count, today):
    """Return the last count weeks (YYYYMMDD) that are over, read from the reference node."""
    weeks = set()
    for year in (today.year - 1, today.year):
        logDir = os.path.dirname(logPath(REFERENCE_NODE, '%d0101' % year))
        if not os.path.isdir(logDir):
            continue
        for name in os.listdir(logDir):
            m = re.match(r'error_log\.(\d{8})\.gz$', name)
            if m:
                start = datetime.datetime.strptime(m.group(1), '%Y%m%d').date()
                if start + datetime.timedelta(days=7) <= today:
                    weeks.add(m.group(1))
    return sorted(weeks)[-count:]


def clientIp(line):
    """Return the client IP of a log line without its port, or None."""
    i = line.find('[client ')
    if i < 0:
        return None
    ip = line[i + 8:line.find(']', i)]
    if ip.count('.') == 3 and ':' in ip:
        ip = ip.rsplit(':', 1)[0]
    return ip


def messageOf(line):
    """Return (cgi, message) for a log line, without the Apache prefix and the referer."""
    i = line.find(', referer: ')
    if i >= 0:
        line = line[:i]
    j = line.find('stderr from ')
    if j >= 0:
        k = line.find(': ', j)
        cgi = line[j + 12:k].rsplit('/', 1)[-1]
        return cgi, line[k + 2:].rstrip()
    return '-', RE_APACHE_CODE.sub('', RE_LEADING_BRACKETS.sub('', line)).rstrip()


def signatureOf(message):
    """Reduce a message to its type: ids become X, numbers N, the rest is cut to 80 chars."""
    if message.startswith('['):
        return '[error context line: date, cgi, client, hgsid, url]'
    return RE_DIGITS.sub('N', RE_LONG_TOKEN.sub('X', message[:120]))[:80]


def countFile(path, testLines, allIps, allSubnets, allHgsids, signatures):
    """Count one node's week. Adds to the week's unique sets and signature counter."""
    counts = Counter()
    byCgi = {key: Counter() for key in BY_CGI}
    ips, subnets, hgsids = set(), set(), set()
    unzip = shutil.which('pigz') or 'gzip'
    proc = subprocess.Popen([unzip, '-dc', path], stdout=subprocess.PIPE)
    stream = io.TextIOWrapper(proc.stdout, encoding='utf-8', errors='replace')
    if testLines:
        stream = itertools.islice(stream, testLines)
    for line in stream:
        counts['lines'] += 1
        cgi, message = messageOf(line)
        isRequest = message.startswith('CGI_TIME: ')
        if isRequest:
            cgi = message[10:].split(' ', 1)[0].rstrip(':')
            counts['requests'] += 1
            byCgi['requests'][cgi] += 1
            if cgi == 'hgTracks' and '/cgi-bin/hgSession?' in line:
                counts['sessionLoads'] += 1
        isHog = message.startswith('hgLib.py hogExit') or (isRequest and ' hogExit:' in message)
        if isHog:
            counts['hogExit'] += 1
            byCgi['hogExit'][cgi] += 1
        if isRequest or isHog:
            ip = clientIp(line)
            if ip:
                ips.add(ip)
                if ip.count('.') == 3:
                    subnets.add(ip.rsplit('.', 2)[0])
        if 'hgsid=' in line:
            hgsids.update(int(h) for h in RE_HGSID.findall(line))
        if message.startswith('Stack dump:'):
            counts['stackDumps'] += 1
            byCgi['stackDumps'][cgi] += 1
        elif message.startswith('CAPTCHA_VALID'):
            counts['captchaSolved'] += 1
        elif 'needMem: ' in message:
            if RE_ALLOC.search(message):
                counts['allocLimit'] += 1
            elif RE_OOM.search(message):
                counts['outOfMemory'] += 1
        elif 'hgCustom load error' in message:
            counts['ctErrors'] += 1
        elif message.startswith('customTrack: saved') and cgi != 'hgRenderTracks':
            counts['ctSaved'] += 1
        elif 'Track Collections expire 48' in message:
            counts['collectionsExpire'] += 1
        elif 'warnTiming' in message:
            counts['warnTiming'] += 1
        signatures[cgi + '\t' + signatureOf(message)] += 1
        if len(signatures) > PRUNE_AT:
            kept = signatures.most_common(PRUNE_AT // 4)
            signatures.clear()
            signatures.update(dict(kept))
    if testLines:
        proc.kill()
        proc.wait()
    elif proc.wait() != 0:
        raise RuntimeError('decompressing %s failed with exit code %d' % (path, proc.returncode))
    counts['uniqueIps'] = len(ips)
    counts['uniqueSubnets'] = len(subnets)
    counts['uniqueHgsids'] = len(hgsids)
    allIps.update(ips)
    allSubnets.update(subnets)
    allHgsids.update(hgsids)
    return counts, byCgi


def countWeek(job):
    """Count all nodes of one week and return the week's result as a dict."""
    week, testLines = job
    allIps, allSubnets, allHgsids = set(), set(), set()
    signatures = Counter()
    byCgi = {key: Counter() for key in BY_CGI}
    result = {'week': week, 'nodes': {}, 'missing': []}
    for node in NODES:
        path = logPath(node, week)
        if not os.path.exists(path):
            result['missing'].append(node)
            continue
        counts, nodeByCgi = countFile(path, testLines, allIps, allSubnets, allHgsids, signatures)
        result['nodes'][node] = dict(counts)
        for key in BY_CGI:
            byCgi[key].update(nodeByCgi[key])
    total = Counter()
    for counts in result['nodes'].values():
        total.update(counts)
    total['uniqueIps'] = len(allIps)
    total['uniqueSubnets'] = len(allSubnets)
    total['uniqueHgsids'] = len(allHgsids)
    result['nodes']['all'] = dict(total)
    result['byCgi'] = {key: dict(c.most_common(40)) for key, c in byCgi.items()}
    result['signatures'] = signatures.most_common(TOP_SIGNATURES)
    return result


def loadWeeks(weeks, cacheDir, testLines, workers):
    """Return the result for every week, counting only those not cached with every node."""
    results, todo = {}, []
    for week in weeks:
        cacheFile = os.path.join(cacheDir, week + '.json')
        if not testLines and os.path.exists(cacheFile):
            with open(cacheFile) as f:
                cached = json.load(f)
            if not cached['missing']:
                results[week] = cached
                continue
        todo.append((week, testLines))
    if todo:
        with Pool(min(workers, len(todo))) as pool:
            for result in pool.imap_unordered(countWeek, todo):
                results[result['week']] = result
                if not testLines:
                    with open(os.path.join(cacheDir, result['week'] + '.json'), 'w') as f:
                        json.dump(result, f)
    return [results[w] for w in weeks]


def value(result, node, key):
    """Return a metric for a node in a week, or None if the node has no log that week."""
    counts = result['nodes'].get(node)
    return None if counts is None else counts.get(key, 0)


def ratio(new, old):
    """Return new/old as text for the change columns."""
    if not old:
        return 'new' if new else ''
    return '%.1fx' % (new / old)


def drawGraphs(results, outDir):
    """Write one PNG per metric: all nodes together, plus a thin line per node."""
    dates = [datetime.datetime.strptime(r['week'], '%Y%m%d') for r in results]
    for key, label, description in METRICS:
        fig, ax = plt.subplots(figsize=(7, 4))
        ax.plot(dates, [value(r, 'all', key) for r in results], marker='o', linewidth=2.5,
                color='black', label='all nodes')
        for node in NODES:
            ys = [value(r, node, key) for r in results]
            ax.plot(dates, [float('nan') if y is None else y for y in ys], marker='.',
                    linewidth=1, label=node)
        ax.set_title(label)
        ax.set_xticks(dates)
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))
        ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda v, pos: '{:,.0f}'.format(v)))
        fig.autofmt_xdate()
        ax.legend(fontsize=7, ncol=3)
        fig.text(0.5, -0.02, description, ha='center', fontsize=8, wrap=True)
        fig.savefig(os.path.join(outDir, key + '.png'), bbox_inches='tight')
        plt.close(fig)


def writeTables(results, outDir):
    """Write values.tsv, byCgi.tsv and signatures.tsv."""
    with open(os.path.join(outDir, 'values.tsv'), 'w') as f:
        f.write('#week\tnode\tmetric\tvalue\n')
        for r in results:
            for node in ['all'] + list(NODES):
                for key, label, description in METRICS:
                    v = value(r, node, key)
                    if v is not None:
                        f.write('%s\t%s\t%s\t%d\n' % (r['week'], node, key, v))
    with open(os.path.join(outDir, 'byCgi.tsv'), 'w') as f:
        f.write('#week\tmetric\tcgi\tcount\n')
        for r in results:
            for key in BY_CGI:
                for cgi, n in r['byCgi'][key].items():
                    f.write('%s\t%s\t%s\t%d\n' % (r['week'], key, cgi, n))
    with open(os.path.join(outDir, 'signatures.tsv'), 'w') as f:
        f.write('#week\tcgi\tcount\tmessage type\n')
        for r in results:
            for sig, n in r['signatures']:
                cgi, message = sig.split('\t', 1)
                f.write('%s\t%s\t%d\t%s\n' % (r['week'], cgi, n, message))


def writeHtml(results, outDir):
    """Write index.html: the numbers, the message types, per-CGI counts and the graphs."""
    last = results[-1]
    prev = results[-2] if len(results) > 1 else last
    weeks = [r['week'] for r in results]
    esc = html.escape
    out = ['<html><head><meta charset="utf-8"><title>Error log weekly trends</title>',
           '<style>body{font-family:Helvetica,Arial,sans-serif;margin:20px}'
           'table{border-collapse:collapse;margin-bottom:24px}'
           'td,th{border:1px solid #ccc;padding:3px 6px;font-size:14px}'
           'td.n{text-align:right}</style></head><body>',
           f'<h2>Error log weekly trends, {weeks[0]} to {weeks[-1]}</h2>',
           f'<p>Nodes: {", ".join(NODES)}. Each week starts on the date shown. Raw numbers: '
           '<a href="values.tsv">values.tsv</a>, <a href="byCgi.tsv">byCgi.tsv</a>, '
           '<a href="signatures.tsv">signatures.tsv</a>. Hover a metric for its definition.</p>']
    missing = [f'{r["week"]}: {", ".join(r["missing"])}' for r in results if r['missing']]
    if missing:
        out.append(f'<p><b>Logs missing</b> (not counted): {"; ".join(missing)}</p>')

    out.append('<h3>All nodes</h3><table><tr><th>Metric</th>'
               + ''.join(f'<th>{w}</th>' for w in weeks) + '</tr>')
    for key, label, description in METRICS:
        cells = ''.join(f'<td class="n">{value(r, "all", key):,}</td>' for r in results)
        out.append(f'<tr><td title="{esc(description)}">{label}</td>{cells}</tr>')
    out.append('</table>')

    out.append(f'<h3>Most common message types, week of {last["week"]} and the week before</h3>'
               f'<table><tr><th>CGI</th><th>Message type</th><th>{prev["week"]}</th>'
               f'<th>{last["week"]}</th><th>Change</th></tr>')
    prevSigs = dict((sig, n) for sig, n in prev['signatures'])
    for sig, n in last['signatures'][:30]:
        cgi, message = sig.split('\t', 1)
        old = prevSigs.get(sig, 0)
        out.append(f'<tr><td>{esc(cgi)}</td><td>{esc(message)}</td><td class="n">{old:,}</td>'
                   f'<td class="n">{n:,}</td><td class="n">{ratio(n, old)}</td></tr>')
    out.append('</table>')

    out.append(f'<h3>Top CGIs, week of {last["week"]} (change from the week before)</h3>'
               '<table><tr>' + ''.join(f'<th>CGI</th><th>{k}</th>' for k in BY_CGI) + '</tr>')
    columns = [sorted(last['byCgi'][k].items(), key=lambda kv: -kv[1])[:15] for k in BY_CGI]
    for i in range(max(len(c) for c in columns)):
        row = ''
        for key, column in zip(BY_CGI, columns):
            if i < len(column):
                cgi, n = column[i]
                change = ratio(n, prev['byCgi'][key].get(cgi, 0))
                row += f'<td>{esc(cgi)}</td><td class="n">{n:,} ({change})</td>'
            else:
                row += '<td></td><td></td>'
        out.append(f'<tr>{row}</tr>')
    out.append('</table><h3>Graphs</h3>')
    out.extend(f'<img src="{key}.png">' for key, label, description in METRICS)
    out.append('</body></html>')
    with open(os.path.join(outDir, 'index.html'), 'w') as f:
        f.write('\n'.join(out))


def emailSummary(results):
    """Return the plain-text summary printed for the cron mail."""
    last = results[-1]
    prev = results[-2] if len(results) > 1 else last
    lines = ['%-28s %14s %14s %8s' % ('All nodes', prev['week'], last['week'], 'change')]
    for key, label, description in METRICS:
        new, old = value(last, 'all', key), value(prev, 'all', key)
        lines.append('%-28s %14s %14s %8s' % (label, f'{old:,}', f'{new:,}', ratio(new, old)))
    missing = [f'{r["week"]}: {", ".join(r["missing"])}' for r in results if r['missing']]
    if missing:
        lines.append('\nLogs missing: ' + '; '.join(missing))
    return '\n'.join(lines)


def linkIntoWebDir(outDir, webDir):
    """Symlink every report file into a web-served directory."""
    os.makedirs(webDir, exist_ok=True)
    for name in os.listdir(outDir):
        link = os.path.join(webDir, name)
        if os.path.lexists(link):
            os.remove(link)
        os.symlink(os.path.join(outDir, name), link)


def main():
    """Parse options, count the weeks, and write the report."""
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--weeks', type=int, default=12, help='complete weeks to show (12)')
    parser.add_argument('--workers', type=int, default=6, help='weeks counted at once (6)')
    parser.add_argument('--outDir', help='write the report here and do not link it to the web')
    parser.add_argument('--testLines', type=int, default=0,
                        help='read only this many lines per file, and bypass the cache')
    args = parser.parse_args()

    user = os.environ.get('USER') or os.getlogin()
    resultRoot = '/hive/users/%s/errorLogSearchCronResults' % user
    cacheDir = os.path.join(resultRoot, 'weeklyCache.v%d' % CACHE_VERSION)
    os.makedirs(cacheDir, exist_ok=True)

    weeks = completeWeeks(args.weeks, datetime.date.today())
    if not weeks:
        sys.exit('No complete weeks found for %s under %s' % (REFERENCE_NODE, WWWSTATS))
    results = loadWeeks(weeks, cacheDir, args.testLines, args.workers)

    dateRange = '%s-%s' % (weeks[0], weeks[-1])
    outDir = args.outDir or os.path.join(resultRoot, dateRange)
    os.makedirs(outDir, exist_ok=True)
    drawGraphs(results, outDir)
    writeTables(results, outDir)
    writeHtml(results, outDir)

    if args.outDir:
        url = outDir
    elif user == 'qateam':
        linkIntoWebDir(outDir, '/usr/local/apache/htdocs-genecats/qa/test-results/'
                       'errorLogSearchResults/' + dateRange)
        url = 'https://genecats.gi.ucsc.edu/qa/test-results/errorLogSearchResults/%s/' % dateRange
    else:
        linkIntoWebDir(outDir, '/cluster/home/%s/public_html/cronResults/errorLogSearchResults/%s'
                       % (user, dateRange))
        url = 'https://hgwdev.gi.ucsc.edu/~%s/cronResults/errorLogSearchResults/%s/' % (
            user, dateRange)

    print('Error log trends for the last %d complete weeks:\n%s\n' % (len(weeks), url))
    print(emailSummary(results))


if __name__ == '__main__':
    main()
