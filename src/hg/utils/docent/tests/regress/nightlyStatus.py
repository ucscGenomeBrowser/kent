#!/usr/bin/env python3
"""Write the Docent nightly status page.  refs #38252

nightly.sh runs this after the tests.  It reads what the job already keeps: the nightly
logs (one per run, named YYYY-MM-DD_HHMM.txt), flips.log, and each committed script's
proof: lines.  It writes one static page with a row per script, anchored by the script's
name, so a ticket comment can link to <url>#rm38236.

Each row carries a reproduce-it-yourself link: the script's steps, as far as one URL can
rebuild them.  track: steps are resolved by docent.js's own DOCENT_DERIVE mode, so the URL
turns on the same containers the script does.  Every link is fetched from genome-test with
a fresh cart before it is offered, and only a link that draws what its expect: step names
is called verified.

Nothing here decides pass or fail; the page only reports what the log says.  A problem in
this script must never change the night's verdict, so nightly.sh treats its exit status
as a note in the mail and nothing more.
"""
import argparse
import concurrent.futures
import datetime
import glob
import html
import json
import os
import re
import subprocess
import sys
import urllib.parse
import urllib.request

import yaml

HERE = os.path.dirname(os.path.abspath(__file__))
DOCENT = os.path.join(HERE, '..', '..', 'docent.js')
PW = '/hive/groups/browser/uiTest/pw'
GITHUB = 'https://github.com/ucscGenomeBrowser/kent'
REDMINE = 'https://redmine.gi.ucsc.edu/issues'
TEST = 'https://genome-test.gi.ucsc.edu'
BETA = 'https://hgwbeta.soe.ucsc.edu'
NIGHTS = 14
PIX = 'pix=1100'
# nightly.sh writes this line into the log of every --update run, which is what cron runs.
MARK = 'updated:  this checkout was reset to origin/master'

# Weakest first, the same order as proof.js.
LEVELS = ['assertion-only', 'xfail', 'sandbox-ab', 'release-ab', 'server-flip',
          'caught-regression']
PLAIN = {
    'assertion-only': 'Checks the fixed behavior. Never seen to fail.',
    'xfail': 'Fails now, as expected. The fix has not reached genome-test.',
    'sandbox-ab': 'Seen to fail on a build without the fix, and pass with it.',
    'release-ab': 'Seen to fail on the release before the fix, and pass on the release '
                  'with it.',
    'server-flip': 'Seen to fail on genome-test, then pass when the fix arrived.',
    'caught-regression': 'Went red for a real regression, which was then fixed.',
}

# Verbs that change nothing a URL has to carry.
PASSIVE = ('shot', 'caption', 'say', 'pause', 'wait', 'note', 'title', 'session')


def parseLog(path):
    """Return (commit, {script: 'pass'|'fail'}, {script: text of its failure})."""
    commit, res, text, cur, buf = None, {}, {}, None, []

    def close():
        if cur and res.get(cur) == 'fail':
            text[cur] = ''.join(buf)

    for line in open(path, errors='replace'):
        m = re.match(r'commit:\s+(\S+)', line)
        if m and commit is None:
            commit = m.group(1)
        m = re.match(r'=== (\S+)', line)
        if m:
            close()
            cur, buf = m.group(1), []
            continue
        if cur is None:
            continue
        if line.startswith(('docent tests', 'make:', 'full log', '--- ')):
            close()
            cur = None
            continue
        buf.append(line)
        if cur not in res:
            if line.startswith('  ok'):
                res[cur] = 'pass'
            elif line.startswith('  FAILED'):
                res[cur] = 'fail'
    close()
    return commit, res, text


def strongest(proofs):
    best = -1
    for p in proofs or []:
        words = str(p).split()
        if words and words[0] in LEVELS:
            best = max(best, LEVELS.index(words[0]))
    return LEVELS[best] if best >= 0 else None


def norm(step):
    if isinstance(step, str):
        verb, *rest = step.split()
        return verb, ' '.join(rest) if rest else True
    verb = next(iter(step))
    return verb, step[verb]


def enc(s):
    return urllib.parse.quote(str(s), safe=':,/')


def derive(yamlPath):
    """{step number: {var: value}} for the script's track: steps, every round merged."""
    env = dict(os.environ, DOCENT_DERIVE='1', NODE_PATH=PW + '/node_modules')
    out = subprocess.run(['node', DOCENT, yamlPath], env=env, capture_output=True,
                         text=True, timeout=120).stdout
    steps, cur = {}, None
    for line in out.splitlines():
        m = re.match(r'step (\d+) track ', line)
        if m:
            cur = int(m.group(1))
            steps[cur] = {}
            continue
        m = re.match(r'\s+round \d+ \(\d+ vars\): (.*)', line)
        if m and cur:
            for kv in m.group(1).split():
                k, _, v = kv.partition('=')
                steps[cur][k] = v
    return steps


def urlOf(db, pos, base, cgiVars):
    v = dict(cgiVars)
    # hideTracks=1 in the same request as hubUrl hides the hub's own rows too, so a hub
    # keeps the default tracks rather than losing the ones the script is about.
    if 'hubUrl' in v:
        v.pop('hideTracks', None)
    if base is None:
        q = [f'db={enc(db)}'] + ([f'position={enc(pos)}'] if pos else [])
        base = '/cgi-bin/hgTracks?' + '&'.join(q)
    extra = '&'.join(f'{k}={enc(x)}' for k, x in v.items())
    url = base + ('&' if '?' in base else '?') + extra if extra else base
    url = url.replace('&&', '&').rstrip('&')
    if url.startswith('/cgi-bin/hgTracks') and 'pix=' not in url:
        url += '&' + PIX
    return url


def candidates(yamlPath, doc):
    """One URL per expect: step that a single request can reach, in step order."""
    db = doc.get('db') or 'hg38'
    pos = doc.get('position')
    steps = doc.get('steps') or []
    nExpect = sum(norm(s)[0] == 'expect' for s in steps)
    base, cgiVars, tracks, cands, seen, stop = None, {}, None, [], 0, None
    for i, s in enumerate(steps, 1):
        verb, arg = norm(s)
        if verb == 'expect':
            seen += 1
            cands.append({'url': urlOf(db, pos, base, cgiVars), 'reached': seen,
                          'expect': arg if isinstance(arg, dict) else {}})
            continue
        if verb in PASSIVE:
            continue
        if verb == 'go':
            if arg is True:
                continue
            pos, base = arg, None
        elif verb == 'goto':
            base, cgiVars = str(arg), {}
        elif verb == 'hide' and arg in ('all', True):
            cgiVars['hideTracks'] = '1'
        elif verb == 'track' and isinstance(arg, dict):
            if tracks is None:
                tracks = derive(yamlPath)
            cgiVars.update(tracks.get(i, {}))
        elif verb == 'hub':
            o = arg if isinstance(arg, dict) else {'url': arg}
            cgiVars['hubUrl'] = o['url']
            db = o.get('db', db)
            pos = o.get('position', pos)
        elif verb == 'loadSession' and isinstance(arg, dict) and arg.get('user'):
            cgiVars.update({'hgS_doOtherUser': 'submit', 'hgS_otherUserName': arg['user'],
                            'hgS_otherUserSessionName': arg.get('name')})
        else:
            stop = verb
            break
    for c in cands:
        c['total'], c['stop'] = nExpect, stop
    if not cands:
        return None, (f'"{stop}:" comes before the first check' if stop
                      else 'it has no expect: step')
    return cands, None


def verify(c):
    """Fetch with a fresh cart and look for what the expect: step names."""
    exp = c.pop('expect')
    rows = exp.get('rows') or []
    texts = [t for t in [exp.get('text')] if isinstance(t, str)]
    if not rows and not texts:
        c['ok'] = None
        return c
    try:
        page = urllib.request.urlopen(TEST + c['url'], timeout=90).read()
        page = page.decode('utf-8', 'replace')
    except Exception:
        c['ok'] = None
        return c
    drawn = set(re.findall(r"id=['\"]tr_([A-Za-z0-9_]+)", page))
    missRow = [t for t in rows if not any(d == t or d.endswith('_' + t) for d in drawn)]
    missText = [t for t in texts if t not in page]
    c['ok'] = not missRow and not missText
    return c


def repro(yamlPath, doc):
    """The deepest check one URL really reproduces, or the reason there is none."""
    cands, why = candidates(yamlPath, doc)
    if cands is None:
        return {'reason': why}
    last = None
    for c in reversed(cands):
        last = verify(c)
        if last['ok'] is not False:
            return last
    return last


def tryIt(name, r):
    e = html.escape
    if 'url' not in r:
        return f'<span class="note">Run the script: {e(r["reason"])}.</span>'
    u = e(r['url'])
    links = f'<a href="{TEST}{u}">genome-test</a> · <a href="{BETA}{u}">hgwbeta</a>'
    n, m = r['reached'], r['total']
    if n == m:
        reach = f'Reaches the last check ({n} of {m}).'
    elif r.get('stop'):
        reach = f'Reaches check {n} of {m}. Then "{e(r["stop"])}:" needs the browser UI.'
    else:
        reach = f'Reaches check {n} of {m}. Later checks need more than one request.'
    if name.endswith('.xfail'):
        return f'{links}<div class="note">Shows the open bug. The fix is not on ' \
               f'genome-test yet.</div>'
    if r['ok'] is True:
        return f'{links}<div class="note">{reach} Verified on genome-test.</div>'
    if r['ok'] is False:
        return '<span class="note">Run the script: one URL does not rebuild what its ' \
               'steps do.</span>'
    return f'{links}<div class="note">{reach} Not verified: nothing a fetch can see.</div>'


def render(scripts, days, lastRun, commit, mediaUrl):
    e = html.escape
    nPass = sum(s['state'] == 'pass' for s in scripts)
    nFail = sum(s['state'] == 'fail' for s in scripts)
    byLevel = ' · '.join(f'{l} {sum(s["level"] == l for s in scripts)}' for l in LEVELS)
    rows = []
    for s in scripts:
        cells = ''.join(
            f'<span class="c {h}" title="{d}: {h.replace("none", "not in suite")}"></span>'
            for d, h in zip(days, s['hist']))
        if s['fail']:
            state = f'<a href="#fail-{s["name"]}" class="st fail">FAIL</a>'
        else:
            state = f'<span class="st {s["state"]}">{e(s["state"])}</span>'
        conf = PLAIN.get(s['level'], 'No proof line.')
        if s['flip']:
            conf += f' Flipped on genome-test {s["flip"]}.'
        proofs = ''.join(f'<li>{e(str(p))}</li>' for p in s['proofs'])
        links = [f'<a href="{GITHUB}/blob/{commit}/src/hg/utils/docent/tests/regress/'
                 f'{s["file"]}">script</a>']
        if s['media']:
            links.append(f'<a href="{mediaUrl}/rm{s["ticket"]}compare.png">before/after</a>')
        rows.append(f'''<tr id="{s["name"]}" class="{s["state"]}">
<td class="nowrap">{s["name"]} <a href="#{s["name"]}" class="anchor" title="link to this row">#</a></td>
<td><a href="{REDMINE}/{s["ticket"]}">#{s["ticket"]}</a></td>
<td>{state}</td><td class="nowrap">{s["since"]}</td>
<td class="nowrap">{cells}</td>
<td><span class="lvl">{e(s["level"] or "none")}</span> {e(conf)}
<details><summary>proof lines</summary><ul>{proofs}</ul></details></td>
<td>{tryIt(s["name"], s["repro"])}</td>
<td class="nowrap">{" · ".join(links)}</td></tr>''')
    fails = ''.join(f'<h3 id="fail-{s["name"]}">{s["name"]}, failing since {s["since"]}</h3>'
                    f'<pre>{e(s["fail"])}</pre>' for s in scripts if s['fail'])
    made = datetime.datetime.now().strftime('%Y-%m-%d %H:%M')
    page = f'''<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Docent Nightly Status</title>
<style>
body {{ max-width: 1400px; font-family: Arial, Helvetica, sans-serif; font-size: 14px;
       margin: 16px; color: #000; background: #fff; }}
h1 {{ font-size: 20px; margin-bottom: 4px; }}
.sub {{ color: #444; margin-bottom: 12px; line-height: 1.5; }}
table {{ border-collapse: collapse; width: 100%; }}
th, td {{ border-bottom: 1px solid #ddd; padding: 4px 6px; text-align: left;
         vertical-align: top; }}
th {{ background: #e8e8f0; position: sticky; top: 0; }}
tr:target {{ background: #fff6c8; }}
tr.fail {{ background: #fdecea; }}
.nowrap {{ white-space: nowrap; }}
.st.pass {{ color: #1a7f37; font-weight: bold; }}
.st.fail {{ color: #b3261e; font-weight: bold; }}
.c {{ display: inline-block; width: 9px; height: 14px; margin-right: 1px; }}
.c.pass {{ background: #3fa35b; }}
.c.fail {{ background: #d9443b; }}
.c.none {{ background: #eee; }}
.c.norun {{ background: repeating-linear-gradient(45deg, #fff, #fff 2px, #bbb 2px, #bbb 4px); }}
.anchor {{ color: #999; text-decoration: none; font-size: 12px; }}
.anchor:hover {{ color: #00c; }}
.note {{ color: #555; font-size: 12px; }}
.lvl {{ font-family: monospace; background: #eef; padding: 0 3px; }}
details summary {{ color: #555; cursor: pointer; font-size: 12px; }}
details ul {{ margin: 4px 0; padding-left: 18px; font-size: 12px; }}
pre {{ background: #f6f6f6; padding: 8px; overflow-x: auto; font-size: 12px; }}
a {{ color: #00c; }}
.wrap {{ overflow-x: auto; }}
</style></head><body>
<h1>Docent Nightly Status</h1>
<div class="sub">Run of {lastRun} against genome-test, at commit
<a href="{GITHUB}/commit/{commit}">{commit[:11]}</a>.
{len(scripts)} scripts: <b>{nPass} pass</b>, <b style="color:#b3261e">{nFail} fail</b>.<br>
The squares are the last {len(days)} days, oldest on the left. A striped square is a night
with no run.<br>
Confidence, the strongest proof: level per script: {byLevel}.<br>
Written {made} by nightlyStatus.py after the nightly run (refs
<a href="{REDMINE}/38252">#38252</a>).</div>
<div class="wrap"><table>
<tr><th>script</th><th>ticket</th><th>state</th><th>since</th><th>last {len(days)} days</th>
<th>confidence</th><th>try it</th><th>links</th></tr>
{"".join(rows)}
</table></div>
<h2>Failures in the last run</h2>
{fails or "<p>None.</p>"}
</body></html>
'''
    # Every link that leaves the page opens a new tab; the in-page row links do not.
    return re.sub(r'<a href="(https?:)', r'<a target="_blank" rel="noopener" href="\1', page)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--logs', required=True, help='directory of nightly logs')
    ap.add_argument('--flips', required=True, help='flips.log')
    ap.add_argument('--out', required=True, help='directory to write index.html into')
    ap.add_argument('--media-dir', default=os.path.expanduser('~/docentTours'),
                    help='where rm<ticket>compare.png pictures live')
    ap.add_argument('--media-url', default='https://hgwdev.gi.ucsc.edu/~braney/docentTours',
                    help='the URL that serves --media-dir')
    args = ap.parse_args()

    # A night is a log written by the cron, and the cron always runs with --update, which
    # stamps its log.  Not the log's name: a run can start minutes late (09-14 began at
    # 04:36), and a hand run of the script without --update is not a night.  The last such
    # log of each day stands for that day.
    latest = {}
    for p in sorted(glob.glob(os.path.join(args.logs, '*.txt'))):
        with open(p, errors='replace') as fh:
            if MARK in fh.read(4096):
                latest[os.path.basename(p)[:10]] = p
    runs = []
    for day in sorted(latest):
        commit, res, text = parseLog(latest[day])
        runs.append((day, commit, res, text))
    if not runs:
        sys.exit(f'no nightly logs in {args.logs}')
    lastDay, lastCommit, lastRes, lastText = runs[-1]
    byDay = {d: r for d, _, r, _ in runs}
    end = datetime.date.fromisoformat(lastDay)
    days = [str(end - datetime.timedelta(n)) for n in range(NIGHTS - 1, -1, -1)]

    flips = {}
    if os.path.exists(args.flips):
        for line in open(args.flips):
            if line.strip() and not line.startswith('#'):
                f = line.split()
                flips[f[1].replace('.xfail', '')] = f[0]

    # The committed scripts, as the nightly run lists them.
    files = subprocess.run(['git', 'ls-files', '*.docent.yaml'], cwd=HERE,
                           capture_output=True, text=True).stdout.split()
    docs = {}
    for f in files:
        name = f[:-len('.docent.yaml')]
        docs[name] = (f, yaml.safe_load(open(os.path.join(HERE, f))) or {})

    with concurrent.futures.ThreadPoolExecutor(6) as ex:
        repros = dict(zip(docs, ex.map(lambda n: repro(os.path.join(HERE, docs[n][0]),
                                                       docs[n][1]), docs)))

    scripts = []
    for name, (f, doc) in docs.items():
        ticket = re.match(r'rm(\d+)', name).group(1) if name.startswith('rm') else ''
        state = lastRes.get(name, 'not run')
        since = lastDay
        for d, _, r, _ in reversed(runs):
            if r.get(name) != state:
                break
            since = d
        hist = []
        for d in days:
            if d not in byDay:
                hist.append('norun')
            else:
                hist.append(byDay[d].get(name, 'none'))
        proofs = doc.get('proof') or []
        scripts.append(dict(
            name=name, ticket=ticket, state=state, since=since, hist=hist,
            level=strongest(proofs), proofs=proofs, fail=lastText.get(name),
            flip=flips.get(name.replace('.xfail', '')), file=f, repro=repros[name],
            media=os.path.exists(os.path.join(args.media_dir, f'rm{ticket}compare.png'))))

    # Failures first, then weakest confidence first, so what needs a person is on top.
    scripts.sort(key=lambda s: (s['state'] != 'fail',
                                LEVELS.index(s['level']) if s['level'] else -1, s['name']))
    page = render(scripts, days, lastDay, lastCommit or 'master', args.media_url)
    os.makedirs(args.out, exist_ok=True)
    tmp = os.path.join(args.out, '.index.html.tmp')
    with open(tmp, 'w') as fh:
        fh.write(page)
    os.chmod(tmp, 0o664)
    os.replace(tmp, os.path.join(args.out, 'index.html'))
    print(f'{len(scripts)} scripts, {sum(s["state"] == "fail" for s in scripts)} failing, '
          f'{sum(s["repro"].get("ok") is True for s in scripts)} verified try-it links')


if __name__ == '__main__':
    main()
