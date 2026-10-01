#!/usr/bin/env python3
"""
Daily check that every otto job is still running.  Refs #38101.

Otto jobs are silent when the source has published nothing, which is the point
of the design and also the reason a job that stops running is invisible.  This
script answers one question per job, "did it run when it was supposed to", by
reading a run stamp that the job leaves behind whether or not the data changed.

It reads three things and writes one:

  ottoOwners.tsv          who owns each job, whether to watch it, and its source
                          URL.  Kept in genecats, edited by the team.
  ottoMonitorStamps.tsv   where the run stamp for each job lives, and how much
                          grace to allow.  Kept beside this script.
  otto.crontab            not read directly.  The schedule comes from the cron
                          column of ottoOwners.tsv, which is copied from it.
  ottoMonitorPublic.tsv   the few tracks to compare with the public site that
                          cannot be found from /gbdb, mostly SQL tables.  The
                          rest are found at run time.  Kept beside this script.
  state.json              written.  One entry per job: the last run seen, the
                          last verdict, how many runs in a row have failed the
                          same way, and the ticket number if one is open.  Also
                          when each public track was first seen behind hgwdev.

A job whose run is late is not automatically somebody's bug.  The source may be
down, in which case the next run will catch up on its own.  So a late job with a
source URL gets that URL fetched, and only a late job whose source answers is
called a real failure.  A source that does not answer has to fail twice in a row
before it becomes a ticket, which is the slack asked for on the ticket.

A job that runs on time can still leave users with old data, because the copy
to the RR is a separate root cron that the otto run never sees.  DECIPHER ran
every week from 2022 to 2026 while its push was switched off (#38436).  So a
second check compares the "Data last updated" date that hgTrackUi shows on
hgwdev with the one on genome.ucsc.edu, and reports a track whose public copy
has stayed behind for longer than its lagDays.

Silent when everything is on time, so cron mails nothing.  Filing tickets is off
unless --file is given.
"""

import argparse
import fnmatch
import glob
import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime, timedelta

selfDir = os.path.dirname(os.path.abspath(__file__))

defaultOwners = "/hive/data/outside/otto/ottoMonitor/ottoOwners.tsv"
defaultStamps = os.path.join(selfDir, "ottoMonitorStamps.tsv")
defaultState = "/hive/data/outside/otto/ottoMonitor/state.json"
defaultPublic = os.path.join(selfDir, "ottoMonitorPublic.tsv")
redmineCli = os.path.expanduser("~/kent/src/utils/redmineCli")

# The two hosts the public check compares.  hgwdev is where otto installs;
# genome.ucsc.edu is what users see.
devHost = "https://hgwdev.gi.ucsc.edu"
publicHost = "https://genome.ucsc.edu"

ottoDir = "/hive/data/outside/otto"
gbdbDir = "/gbdb"
hgsql = "/cluster/bin/x86_64/hgsql"

# How long a public copy may stay behind hgwdev before it is reported.  The
# pushes for otto jobs run weekly or more often.
defaultLagDays = 8.0

# Pause between page fetches, so a daily run of thirty pages stays well clear of
# the RR's bot delay.
publicFetchPause = 1.0

# hui.c prints this line for both a table and a bigBed/bigWig.  The date is
# "YYYY-MM-DD HH:MM:SS" for a big file and "YYYY-MM-DD" for a table.
dateLineRe = re.compile(r"Data last updated at UCSC:&nbsp;</B>\s*([0-9-]+(?: [0-9:]+)?)")

# A run stamp is allowed to be this stale before the job counts as late, on top
# of the job's own graceHours.  Covers clock skew and a cron that starts slow.
extraGraceMinutes = 15

# A job with no individual owner carries this in the owner column of
# ottoOwners.tsv, and belongs to whoever the ottoOnDuty header names.  Lou set
# that rule for civic on #38101, 2026-09-08.
onDutyOwner = "ottoOnDuty"

dowNames = {"sun": 0, "mon": 1, "tue": 2, "wed": 3, "thu": 4, "fri": 5, "sat": 6}


def parseCronField(field, lo, hi, names=None):
    """Expand one cron field into a set of ints.  Handles *, a,b,c, a-b and */n."""
    values = set()
    for part in field.split(","):
        step = 1
        if "/" in part:
            part, stepText = part.split("/", 1)
            step = int(stepText)
        if part == "*":
            first, last = lo, hi
        elif "-" in part.strip("-"):
            firstText, lastText = part.split("-", 1)
            first = cronNumber(firstText, names)
            last = cronNumber(lastText, names)
        else:
            first = last = cronNumber(part, names)
        values.update(range(first, last + 1, step))
    return(values)


def cronNumber(text, names):
    """One cron value, which may be a name such as mon."""
    text = text.strip().lower()
    if names and text in names:
        return(names[text])
    return(int(text))


def parseCron(spec):
    """Five cron fields to (minutes, hours, doms, months, dows), or None."""
    fields = spec.split()
    if len(fields) != 5:
        return(None)
    try:
        minutes = parseCronField(fields[0], 0, 59)
        hours = parseCronField(fields[1], 0, 23)
        doms = parseCronField(fields[2], 1, 31)
        months = parseCronField(fields[3], 1, 12)
        dows = parseCronField(fields[4], 0, 7, dowNames)
    except ValueError:
        return(None)
    # cron accepts 7 for Sunday as well as 0
    if 7 in dows:
        dows = (dows - {7}) | {0}
    return(minutes, hours, doms, months, dows, fields[2] == "*", fields[4] == "*")


def dayMatches(day, doms, months, dows, domIsStar, dowIsStar):
    """cron's day rule: when BOTH day-of-month and day-of-week are restricted
    the day matches if EITHER matches, not both.  Getting this backwards would
    make a job like '14 13 * * mon' look like it never runs."""
    if day.month not in months:
        return(False)
    domHit = day.day in doms
    dowHit = ((day.weekday() + 1) % 7) in dows
    if domIsStar and dowIsStar:
        return(True)
    if domIsStar:
        return(dowHit)
    if dowIsStar:
        return(domHit)
    return(domHit or dowHit)


def prevScheduledRun(specs, now):
    """The most recent time this job was due, at or before now.  A job with more
    than one crontab line has them joined with ";" in ottoOwners.tsv, and the
    answer is whichever of them fired last.  ottoLastLog needs this: cron has no
    way to say "the last day of the month", so it takes four lines to say it."""
    best = None
    for spec in specs.split(";"):
        when = prevScheduledRunOne(spec.strip(), now)
        if when is not None and (best is None or when > best):
            best = when
    return(best)


def prevScheduledRunOne(spec, now):
    """The most recent time one cron spec was due, at or before now."""
    parsed = parseCron(spec)
    if parsed is None:
        return(None)
    minutes, hours, doms, months, dows, domIsStar, dowIsStar = parsed
    day = now.replace(hour=0, minute=0, second=0, microsecond=0)
    # 400 days back covers a yearly schedule, and nothing in otto.crontab is
    # rarer than monthly
    for _ in range(400):
        if dayMatches(day, doms, months, dows, domIsStar, dowIsStar):
            for hour in sorted(hours, reverse=True):
                for minute in sorted(minutes, reverse=True):
                    when = day.replace(hour=hour, minute=minute)
                    if when <= now:
                        return(when)
        day -= timedelta(days=1)
    return(None)


def newestMtime(pattern):
    """Newest mtime among the glob matches, or None when nothing matches."""
    newest = None
    for path in glob.glob(pattern):
        try:
            when = datetime.fromtimestamp(os.path.getmtime(path))
        except OSError:
            continue
        if newest is None or when > newest:
            newest = when
    return(newest)


def curl(args, timeout=30):
    """Run curl and return its HTTP code as an int, or 0 when curl itself failed.
    An ftp listing has no HTTP code, so curl reports 226 there."""
    cmd = ["curl", "-sS", "--max-time", str(timeout),
           "-o", "/dev/null", "-w", "%{http_code}"] + args
    try:
        done = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 10)
    except subprocess.TimeoutExpired:
        return(0)
    try:
        return(int(done.stdout.strip() or 0))
    except ValueError:
        return(0)


def sourceIsUp(sourceUrl):
    """Fetch a job's source and say whether it answered.

    Four shapes, because one does not fit.  Thirteen of the twenty-eight sources
    refuse a plain HEAD, redirect, or hide behind a config file that carries a
    credential.  Measured 2026-09-07, see the survey named in the module
    docstring.  Returns (up, detail)."""
    if not sourceUrl or sourceUrl in ("none", "unknown", "-"):
        return(None, "no source url")

    # omim and decipher keep the URL, and a credential, in a config file.  Read
    # it at probe time; never copy either URL into a table or a ticket.
    if sourceUrl.startswith("file:"):
        path = sourceUrl[len("file:"):]
        if not os.access(path, os.R_OK):
            return(None, "cannot read " + path)
        if path.endswith("curl.config"):
            code = curl(["-K", path, "-r", "0-0"])
            return(code == 200 or code == 206, "curl -K %s -> %d" % (path, code))
        with open(path) as fh:
            for line in fh:
                line = line.strip()
                if line.startswith("http") or line.startswith("ftp"):
                    code = curl(["-L", "-r", "0-0", line])
                    return(code in (200, 206), "config url -> %d" % code)
        return(None, "no url found in " + path)

    if sourceUrl.startswith("ftp:"):
        # An ftp listing has no HTTP code; curl reports 226 for a completed
        # transfer.  Anything else, 0 included, means the source did not answer.
        code = curl([sourceUrl])
        return(code == 226, "ftp -> %d" % code)

    code = curl(["-I", sourceUrl])
    if code == 200:
        return(True, "HEAD -> 200")
    # HEAD is refused by clinGenCspec, insight and lovd, and g2p and omim
    # redirect.  A ranged GET answers all five.
    code2 = curl(["-L", "-r", "0-0", sourceUrl])
    return(code2 in (200, 206), "HEAD -> %d, GET -> %d" % (code, code2))


def readTable(path, wanted):
    """Read a tab separated table with a leading comment block.  Returns a dict
    keyed on the first column, plus the comment lines, so the ottoOnDuty header
    can be found without a second pass."""
    rows = {}
    comments = []
    with open(path) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.startswith("#"):
                comments.append(line)
                continue
            if not line.strip():
                continue
            fields = line.split("\t")
            if len(fields) < wanted:
                continue
            rows[fields[0]] = fields
    return(rows, comments)


def findOnDuty(comments):
    """The ottoOnDuty header line names whoever is currently running otto."""
    for line in comments:
        if line.lower().startswith("# ottoonduty:"):
            return(line.split(":", 1)[1].strip())
    return(None)


def findWatchers(comments):
    """The ottoWatchers header line names people added to every ticket, comma
    separated."""
    for line in comments:
        if line.lower().startswith("# ottowatchers:"):
            return([n.strip() for n in line.split(":", 1)[1].split(",") if n.strip()])
    return([])


def loadState(path):
    if os.path.exists(path):
        with open(path) as fh:
            return(json.load(fh))
    return({})


def saveState(path, state):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w") as fh:
        json.dump(state, fh, indent=2, sort_keys=True)
    os.rename(tmp, path)


def checkJob(job, owners, stamps, now, onDuty=None):
    """Everything known about one job's last run.  Returns a dict."""
    ownerRow = owners[job]
    stampRow = stamps.get(job)
    owner = ownerRow[1]
    ownerIsOnDuty = owner == onDutyOwner
    if ownerIsOnDuty:
        owner = onDuty or "?"
    result = {"job": job, "owner": owner, "ownerIsOnDuty": ownerIsOnDuty,
              "sourceUrl": ownerRow[5],
              "cron": ownerRow[6], "verdict": "ok", "detail": ""}

    if stampRow is None:
        result["verdict"] = "unlisted"
        result["detail"] = "in the crontab with no row in ottoMonitorStamps.tsv"
        return(result)

    stampGlob, graceHours = stampRow[1], float(stampRow[2])
    if stampGlob == "-":
        result["verdict"] = "blind"
        result["detail"] = stampRow[3] if len(stampRow) > 3 else "no run stamp"
        return(result)

    # Check against the most recent scheduled time whose grace window has
    # already CLOSED, not against the latest one.  Measuring the grace forward
    # from the latest scheduled time leaves a job permanently unflaggable
    # whenever its scheduled hour is less than graceHours before this script's
    # own run time, because every check then lands inside a fresh grace window.
    # Six of the forty were in that hole at the 12:15 cron: clinGen,
    # genArkPushRR, grcIncidentDb, liftRequest, omim and pubtatorDbSnp.
    due = prevScheduledRun(result["cron"],
                           now - timedelta(hours=graceHours, minutes=extraGraceMinutes))
    if due is None:
        result["verdict"] = "unparsed"
        result["detail"] = "could not parse cron spec %r" % result["cron"]
        return(result)

    lastRun = newestMtime(stampGlob)
    result["due"] = due.strftime("%Y-%m-%d %H:%M")
    result["lastRun"] = lastRun.strftime("%Y-%m-%d %H:%M") if lastRun else "never"
    if lastRun is not None and lastRun >= due:
        return(result)

    result["verdict"] = "late"
    late = now - due
    result["detail"] = "no run stamp since %s, due %s, %d hours late" % (
        result["lastRun"], result["due"], late.total_seconds() // 3600)
    return(result)


def classifyLate(result):
    """A late job is only somebody's bug if its source is actually up."""
    up, detail = sourceIsUp(result["sourceUrl"])
    result["probe"] = detail
    if up is False:
        result["verdict"] = "sourceDown"
    else:
        result["verdict"] = "realFailure"
    return(result)


def ottoLinks():
    """Every symlink in /gbdb that points into the otto area, as a list of
    (db, gbdb path, target).  A direct link is how every otto bigBed reaches
    the browser.  About 2,800 links in 700 databases; a cold scan takes a minute
    and a half, a warm one under a second."""
    cmd = ["find", gbdbDir + "/", "-mindepth", "2", "-maxdepth", "4", "-type", "l",
           "-lname", ottoDir + "/*", "-printf", "%p\t%l\n"]
    done = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    links = []
    for line in done.stdout.splitlines():
        path, target = line.split("\t", 1)
        db = path[len(gbdbDir) + 1:].split("/", 1)[0]
        links.append((db, path, target))
    return(links)


def bigDataUrls(db):
    """{gbdb path: [track, ...]} for one database's trackDb on hgwdev."""
    query = ("select tableName, substring_index(substring_index(settings, "
             "'bigDataUrl ', -1), '\\n', 1) from trackDb "
             "where settings like '%bigDataUrl /gbdb/%'")
    done = subprocess.run([hgsql, "-N", db, "-e", query], capture_output=True, text=True,
                          errors="replace")
    urls = {}
    for line in done.stdout.splitlines():
        fields = line.split("\t")
        if len(fields) != 2:
            continue
        url = fields[1].strip().replace("$D", db)
        urls.setdefault(url, []).append(fields[0])
    return(urls)


def discoverPublicTracks(owners, watched):
    """The bigBed tracks each watched job feeds, found from /gbdb and trackDb
    rather than listed by hand, so a new file or a new assembly is picked up
    on its own.

    A link belongs to the job whose directory, taken from its script path in
    ottoOwners.tsv, is the longest prefix of the link's target.  Two jobs that
    share a directory (omim and omimUpload, uniprot and uniprotWuhCor1) give
    it to the first by name; they have the same owner.  Links into a directory
    no watched job runs from are left out.

    A push copies a directory, so one track per job and /gbdb directory is
    enough.  uniprot alone has links in 140 databases, though, so each of those
    gets hg38 and hg19 when it has them, and otherwise the single database
    whose file is newest.  Within that, the track whose file is newest, because
    it is the one most likely to be ahead of the public copy."""
    jobDirs = []
    for job in sorted(watched):
        script = owners[job][7]
        if script.startswith(ottoDir + "/"):
            jobDirs.append((os.path.dirname(script) + "/", job))
    # longest directory first, so clinGen/clinGenCspec/ wins over clinGen/
    jobDirs.sort(key=lambda d: -len(d[0]))

    groups = {}
    for db, path, target in ottoLinks():
        job = next((j for d, j in jobDirs if target.startswith(d)), None)
        if job is None:
            continue
        try:
            mtime = os.path.getmtime(target)
        except OSError:
            continue
        subDir = os.path.dirname(path[len(gbdbDir) + 1:].split("/", 1)[1])
        groups.setdefault((job, subDir), {}).setdefault(db, []).append((mtime, path))

    urlCache = {}
    rows = []
    for (job, subDir), byDb in sorted(groups.items()):
        dbs = [db for db in ("hg38", "hg19") if db in byDb]
        if not dbs:
            dbs = [max(byDb, key=lambda db: max(byDb[db]))]
        for db in dbs:
            if db not in urlCache:
                urlCache[db] = bigDataUrls(db)
            for mtime, path in sorted(byDb[db], reverse=True):
                tracks = urlCache[db].get(path)
                if tracks:
                    rows.append({"job": job, "db": db, "track": sorted(tracks)[0],
                                 "lagDays": defaultLagDays, "note": "found in /gbdb"})
                    break
    return(rows)


def readPublicTable(path):
    """Rows of ottoMonitorPublic.tsv as dicts, in file order.  Not readTable,
    because one job can feed several tracks and readTable keys on the job."""
    rows = []
    with open(path) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.startswith("#") or not line.strip():
                continue
            fields = line.split("\t")
            if len(fields) < 4:
                continue
            rows.append({"job": fields[0], "db": fields[1], "track": fields[2],
                         "lagDays": float(fields[3]),
                         "note": fields[4] if len(fields) > 4 else ""})
    return(rows)


def trackDate(host, db, track, timeout=60):
    """The "Data last updated at UCSC" date hgTrackUi shows for one track, as a
    datetime, or None with the reason when the page has no such line."""
    url = "%s/cgi-bin/hgTrackUi?db=%s&g=%s" % (host, db, track)
    cmd = ["curl", "-sS", "--max-time", str(timeout), url]
    try:
        done = subprocess.run(cmd, capture_output=True, text=True, errors="replace",
                              timeout=timeout + 10)
    except subprocess.TimeoutExpired:
        return(None, "timed out")
    if done.returncode != 0:
        return(None, "curl exit %d" % done.returncode)
    match = dateLineRe.search(done.stdout)
    if match is None:
        if "Can't find" in done.stdout:
            return(None, "no such track")
        return(None, "no date on the page")
    text = match.group(1)
    for fmt in ("%Y-%m-%d %H:%M:%S", "%Y-%m-%d"):
        try:
            return(datetime.strptime(text, fmt), text)
        except ValueError:
            pass
    return(None, "unreadable date %r" % text)


def checkPublic(row, entry, now):
    """Compare one track on hgwdev and on the public site.  entry is this
    track's slot in state.json and is updated in place.  Returns a dict."""
    result = dict(row)
    result["name"] = "%s %s" % (row["db"], row["track"])
    devDate, devText = trackDate(devHost, row["db"], row["track"])
    time.sleep(publicFetchPause)
    pubDate, pubText = trackDate(publicHost, row["db"], row["track"])
    time.sleep(publicFetchPause)
    result["dev"] = devText
    result["public"] = pubText
    # An alpha track such as clinvarMainAlpha is on hgwdev only, on purpose.
    if devDate is not None and pubText == "no such track":
        result["verdict"] = "notOnPublic"
        return(result)
    if devDate is None or pubDate is None:
        result["verdict"] = "publicUnknown"
        result["detail"] = "hgwdev: %s, genome.ucsc.edu: %s" % (devText, pubText)
        return(result)

    # The public copy can match hgwdev, or even be newer when someone pushed by
    # hand and hgwdev was rebuilt since with the same data.  Only older counts.
    if pubDate >= devDate:
        entry.pop("behindSince", None)
        entry["publicSeen"] = pubText
        result["verdict"] = "ok"
        return(result)

    # The public copy is older.  That is normal for up to a week, until the next
    # push.  How long it has been behind is when the monitor first saw it so.  On
    # the first sighting the earliest moment hgwdev is known to have been ahead
    # is its own date, so start the clock there rather than at now.
    #
    # Also restart the clock when the public date has moved since the last run.
    # A job that rebuilds daily and is pushed weekly is never equal at the time
    # this runs, but a public date that moves shows the push is working.
    since = entry.get("behindSince")
    if since is None or entry.get("publicSeen", pubText) != pubText:
        since = devDate.strftime("%Y-%m-%d %H:%M")
        entry["behindSince"] = since
    entry["publicSeen"] = pubText
    result["behindSince"] = since
    behind = now - datetime.strptime(since, "%Y-%m-%d %H:%M")
    if behind < timedelta(days=row["lagDays"]):
        result["verdict"] = "ok"
        return(result)
    result["verdict"] = "notPublic"
    result["detail"] = ("genome.ucsc.edu has data from %s, hgwdev has %s, "
                        "behind since %s (%d days)" %
                        (pubText, devText, since, behind.days))
    return(result)


def fileTicket(result, onDuty, watchers, dryRun):
    """One GB Bug per failing job, to whoever is running otto, with the job's
    owner and the ottoWatchers people as watchers, and the owner named in the
    body."""
    owner = result["owner"]
    if result["verdict"] == "notPublic":
        subject = "otto job %s: %s on genome.ucsc.edu is older than on hgwdev" % (
            result["job"], result["name"])
    else:
        subject = "otto job %s has not run since %s" % (result["job"], result["lastRun"])
    if result.get("ownerIsOnDuty"):
        ownerLine = "This job has no individual owner, so it belongs to whoever is running otto."
    elif owner != "?":
        ownerLine = "The recorded owner of this job is %s." % owner
    else:
        ownerLine = "This job has no recorded owner in ottoOwners.tsv."
    if result["verdict"] == "notPublic":
        body = "\n".join([
            "The otto failure monitor found that this track has not reached the "
            "public site. The otto job runs, but the copy to the RR does not. "
            "Refs #38101.",
            "",
            ownerLine,
            "",
            "Track: %s" % result["name"],
            "Data date on hgwdev: %s" % result["dev"],
            "Data date on genome.ucsc.edu: %s" % result["public"],
            "Behind since: %s" % result["behindSince"],
        ])
    else:
        body = "\n".join([
            "The otto failure monitor found this job late. Refs #38101.",
            "",
            ownerLine,
            "",
            "Schedule: %s" % result["cron"],
            "Last run stamp: %s" % result["lastRun"],
            "Due: %s" % result.get("due", "-"),
            "Source check: %s" % result.get("probe", "-"),
        ])
    cmd = [redmineCli, "create", "--project", "genomebrowser", "--tracker", "Bug",
           "--subject", subject, "--description", body]
    if onDuty:
        cmd += ["--assigned-to", onDuty]
    if dryRun:
        print("    would file a ticket, run with --file to do it: %s" % subject)
        return(None)
    done = subprocess.run(cmd, capture_output=True, text=True)
    print(done.stdout.strip())
    # redmineCli prints "Created #NNNNN: <url>"
    match = re.search(r"Created #(\d+)", done.stdout)
    if not match:
        return(None)
    ticketId = match.group(1)
    # dict.fromkeys keeps the order and drops a name that is in the list twice,
    # such as an owner who is also the person on duty
    for name in dict.fromkeys(n for n in [owner, onDuty] + watchers if n and n != "?"):
        subprocess.run([redmineCli, "watch", ticketId, name],
                       capture_output=True, text=True)
    return(ticketId)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--owners", default=defaultOwners, help="ottoOwners.tsv")
    parser.add_argument("--stamps", default=defaultStamps, help="ottoMonitorStamps.tsv")
    parser.add_argument("--state", default=defaultState, help="state.json")
    parser.add_argument("--file", action="store_true",
                        help="file a ticket for a real failure.  Off by default")
    parser.add_argument("--job", help="check one job and say everything about it")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="report the jobs that are fine and the ones we are blind to")
    parser.add_argument("--no-state", action="store_true",
                        help="do not read or write state.json")
    parser.add_argument("--public", default=defaultPublic, help="ottoMonitorPublic.tsv")
    parser.add_argument("--no-public", action="store_true",
                        help="skip the comparison of hgwdev with genome.ucsc.edu")
    args = parser.parse_args()

    owners, comments = readTable(args.owners, 10)
    stamps, _ = readTable(args.stamps, 3)
    publicRows = []
    onDuty = findOnDuty(comments)
    watchers = findWatchers(comments)
    now = datetime.now()
    state = {} if args.no_state else loadState(args.state)

    watched = [j for j, row in owners.items()
               if row[4] == "yes" and (args.job is None or j == args.job)]
    if args.job and not watched:
        sys.exit("no watched job named %s" % args.job)

    late, blind, other, fine = [], [], [], []
    for job in sorted(watched):
        result = checkJob(job, owners, stamps, now, onDuty)
        if result["verdict"] == "late":
            result = classifyLate(result)
        entry = state.setdefault(job, {})
        previous = entry.get("verdict")
        if result["verdict"] in ("sourceDown", "realFailure"):
            entry["strikes"] = entry.get("strikes", 0) + 1 if previous == result["verdict"] else 1
            late.append(result)
        else:
            entry["strikes"] = 0
        entry["verdict"] = result["verdict"]
        entry["lastRun"] = result.get("lastRun", "-")
        entry["checked"] = now.strftime("%Y-%m-%d %H:%M")
        result["strikes"] = entry["strikes"]
        if result["verdict"] == "blind":
            blind.append(result)
        elif result["verdict"] in ("unlisted", "unparsed"):
            other.append(result)
        elif result["verdict"] == "ok":
            entry.pop("ticket", None)
            fine.append(result)

    for result in late:
        print("%s: %s" % (result["job"], result["detail"]))
        print("    source: %s" % result.get("probe", "-"))
        print("    owner: %s   strike %d" % (result["owner"], result["strikes"]))
        # a source that is down fixes itself overnight often enough that one bad
        # night should not become a ticket
        if result["verdict"] == "sourceDown" and result["strikes"] < 2:
            print("    source is down, waiting for a second strike before filing")
            continue
        entry = state.get(result["job"], {})
        if entry.get("ticket"):
            print("    ticket #%s is already open" % entry["ticket"])
            continue
        ticketId = fileTicket(result, onDuty, watchers, dryRun=not args.file)
        if ticketId:
            entry["ticket"] = ticketId

    for result in other:
        print("%s: %s" % (result["job"], result["detail"]))

    # The public-site check.  Tracks found in /gbdb, plus the few that
    # ottoMonitorPublic.tsv lists because /gbdb cannot find them.  Its state
    # lives under one key of its own, keyed by db and track, because one job
    # can feed several tracks.
    if not args.no_public:
        seen = set()
        for row in readPublicTable(args.public) + discoverPublicTracks(owners, watched):
            if row["job"] in watched and (row["db"], row["track"]) not in seen:
                seen.add((row["db"], row["track"]))
                publicRows.append(row)
    publicState = state.setdefault("_publicSite", {})
    publicFine, publicAbsent = [], []
    for row in publicRows:
        entry = publicState.setdefault("%s.%s" % (row["db"], row["track"]), {})
        result = checkPublic(row, entry, now)
        entry["checked"] = now.strftime("%Y-%m-%d %H:%M")
        if result["verdict"] == "ok":
            entry.pop("ticket", None)
            publicFine.append(result)
            continue
        if result["verdict"] == "notOnPublic":
            publicAbsent.append(result)
            continue
        print("%s: %s: %s" % (row["job"], result["name"], result["detail"]))
        if result["verdict"] != "notPublic":
            continue
        owner = owners[row["job"]][1]
        result["ownerIsOnDuty"] = owner == onDutyOwner
        result["owner"] = (onDuty or "?") if result["ownerIsOnDuty"] else owner
        print("    owner: %s" % result["owner"])
        if entry.get("ticket"):
            print("    ticket #%s is already open" % entry["ticket"])
            continue
        ticketId = fileTicket(result, onDuty, watchers, dryRun=not args.file)
        if ticketId:
            entry["ticket"] = ticketId

    if args.verbose:
        print("\nblind, cannot tell whether these ran (%d):" % len(blind))
        for result in blind:
            print("  %-20s %s" % (result["job"], result["detail"]))
        print("\non time (%d):" % len(fine))
        for result in fine:
            print("  %-20s last run %s, due %s" %
                  (result["job"], result.get("lastRun", "-"), result.get("due", "-")))
        if publicRows:
            print("\npublic site up to date (%d):" % len(publicFine))
            for result in publicFine:
                waiting = ""
                if result.get("behindSince"):
                    waiting = ", behind since %s, within %g days" % (
                        result["behindSince"], result["lagDays"])
                print("  %-20s %-32s hgwdev %s, public %s%s" %
                      (result["job"], result["name"], result["dev"],
                       result["public"], waiting))
            print("\non hgwdev only, not checked (%d):" % len(publicAbsent))
            for result in publicAbsent:
                print("  %-20s %s" % (result["job"], result["name"]))

    if not args.no_state:
        saveState(args.state, state)


if __name__ == "__main__":
    main()
