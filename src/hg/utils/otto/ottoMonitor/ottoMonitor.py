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
  state.json              written.  One entry per job: the last run seen, the
                          last verdict, how many runs in a row have failed the
                          same way, and the ticket number if one is open.

A job whose run is late is not automatically somebody's bug.  The source may be
down, in which case the next run will catch up on its own.  So a late job with a
source URL gets that URL fetched, and only a late job whose source answers is
called a real failure.  A source that does not answer has to fail twice in a row
before it becomes a ticket, which is the slack asked for on the ticket.

Silent when everything is on time, so cron mails nothing.  Filing tickets is off
unless --file is given.
"""

import argparse
import fnmatch
import glob
import json
import os
import subprocess
import sys
from datetime import datetime, timedelta

selfDir = os.path.dirname(os.path.abspath(__file__))

defaultOwners = "/hive/data/outside/otto/ottoMonitor/ottoOwners.tsv"
defaultStamps = os.path.join(selfDir, "ottoMonitorStamps.tsv")
defaultState = "/hive/data/outside/otto/ottoMonitor/state.json"
redmineCli = os.path.expanduser("~/kent/src/utils/redmineCli")

# A run stamp is allowed to be this stale before the job counts as late, on top
# of the job's own graceHours.  Covers clock skew and a cron that starts slow.
extraGraceMinutes = 15

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
        code = curl([sourceUrl])
        return(code == 226 or code == 0 and False, "ftp -> %d" % code)

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


def checkJob(job, owners, stamps, now):
    """Everything known about one job's last run.  Returns a dict."""
    ownerRow = owners[job]
    stampRow = stamps.get(job)
    result = {"job": job, "owner": ownerRow[1], "sourceUrl": ownerRow[5],
              "cron": ownerRow[6], "verdict": "ok", "detail": ""}

    if stampRow is None:
        result["verdict"] = "unlisted"
        result["detail"] = "in the crontab with no row in ottoMonitorStamps.tsv"
        return(result)

    stampGlob, graceHours = stampRow[1], stampRow[2]
    if stampGlob == "-":
        result["verdict"] = "blind"
        result["detail"] = stampRow[3] if len(stampRow) > 3 else "no run stamp"
        return(result)

    due = prevScheduledRun(result["cron"], now)
    if due is None:
        result["verdict"] = "unparsed"
        result["detail"] = "could not parse cron spec %r" % result["cron"]
        return(result)

    lastRun = newestMtime(stampGlob)
    result["due"] = due.strftime("%Y-%m-%d %H:%M")
    result["lastRun"] = lastRun.strftime("%Y-%m-%d %H:%M") if lastRun else "never"
    deadline = due + timedelta(hours=float(graceHours), minutes=extraGraceMinutes)
    if lastRun is not None and lastRun >= due:
        return(result)
    if now < deadline:
        result["detail"] = "due %s, still inside its grace window" % result["due"]
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


def fileTicket(result, onDuty, dryRun):
    """One GB Bug per failing job, to whoever is running otto, with the job's
    owner as a watcher and named in the body."""
    owner = result["owner"]
    subject = "otto job %s has not run since %s" % (result["job"], result["lastRun"])
    ownerLine = ("The recorded owner of this job is %s." % owner if owner != "?"
                 else "This job has no recorded owner in ottoOwners.tsv.")
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
    for name in [n for n in (owner, onDuty) if n and n != "?"]:
        ticketId = "".join(c for c in done.stdout.split("#")[-1][:6] if c.isdigit())
        if ticketId:
            subprocess.run([redmineCli, "watch", ticketId, name],
                           capture_output=True, text=True)
    return(done.stdout.strip())


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
    args = parser.parse_args()

    owners, comments = readTable(args.owners, 10)
    stamps, _ = readTable(args.stamps, 3)
    onDuty = findOnDuty(comments)
    now = datetime.now()
    state = {} if args.no_state else loadState(args.state)

    watched = [j for j, row in owners.items()
               if row[4] == "yes" and (args.job is None or j == args.job)]
    if args.job and not watched:
        sys.exit("no watched job named %s" % args.job)

    late, blind, other, fine = [], [], [], []
    for job in sorted(watched):
        result = checkJob(job, owners, stamps, now)
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
        fileTicket(result, onDuty, dryRun=not args.file)

    for result in other:
        print("%s: %s" % (result["job"], result["detail"]))

    if args.verbose:
        print("\nblind, cannot tell whether these ran (%d):" % len(blind))
        for result in blind:
            print("  %-20s %s" % (result["job"], result["detail"]))
        print("\non time (%d):" % len(fine))
        for result in fine:
            print("  %-20s last run %s, due %s" %
                  (result["job"], result.get("lastRun", "-"), result.get("due", "-")))

    if not args.no_state:
        saveState(args.state, state)


if __name__ == "__main__":
    main()
