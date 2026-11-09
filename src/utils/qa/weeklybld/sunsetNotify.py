#!/usr/bin/env python3
"""sunsetNotify.py - email each engineer the hg.conf release gates they own
that are overdue for deletion or stalled in QA.  Run by autoBuild.sh wrapup
(step sunset-notify, refs #37925), after the sunset-report step has rebuilt
the age cache.

usage: sunsetNotify.py --cache AGES.json [--cc ADDR] [--send | --to ADDR]

  (default)   print each email to stdout, send nothing
  --send      mail each engineer, cc'ing --cc
  --to ADDR   send every email to ADDR only (to test the real mail path)

Which gates and why come from hgConfCatalog.py itself (gate_lifecycle and
gate_states), so this notice and `hgConfCatalog.py --sunset` cannot disagree.
This file only adds two things the catalog does not track:

  owner   the git author of the commit that turned the default on (for a
          shipped gate) or that added the read (for one still FALSE).  The
          commit ids come from the age cache.
  confs   what the production hg.conf copies in kent/confs set the gate to.
          A gate "stalled" with a FALSE code default is often switched on by
          hand on the RR; the action for that is to flip the code default,
          not to delete the feature, and the email says so.

A gate whose owner cannot be found goes in the build meister's own copy.
"""
import argparse
import os
import subprocess
import sys
from collections import defaultdict

KENT_SRC = os.environ.get("KENT_SRC") or os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
CATALOG_DIR = os.path.join(KENT_SRC, "hg", "utils", "hgConfCatalog")
CONFS_DIR = os.path.join(KENT_SRC, "..", "confs")
# The public-facing servers, then hgwdev.  hgwdev alone means "never shipped".
PROD_CONFS = ["rr", "euro", "asia", "hgwbeta"]
DEV_CONFS = ["hgwdev"]


def git(*args):
    return subprocess.run(["git", "-C", KENT_SRC] + list(args),
                          capture_output=True, text=True).stdout.strip()


def author(commit):
    """(name, email) of a commit, or (None, None)."""
    if not commit:
        return (None, None)
    out = git("log", "-1", "--format=%an\t%ae", commit)
    if "\t" not in out:
        return (None, None)
    name, email = out.split("\t", 1)
    return (name, email.lower())


def conf_settings(cc, names):
    """{gate: {conf: value}} for the gates set in each kent/confs copy."""
    out = defaultdict(dict)
    for conf in PROD_CONFS + DEV_CONFS:
        path = os.path.join(CONFS_DIR, "%s.hg.conf" % conf)
        for _, name, val in cc.parse_conf(path):
            if name in names:
                out[name][conf] = val.lower()
    return out


def is_on(val, cc):
    return val in cc.TRUE_WORDS


def classify(g, state, confs, cc):
    """Which section of the email a gate goes in."""
    if state == "overdue":
        return "overdue"
    on_prod = [c for c in PROD_CONFS if is_on(confs.get(c, ""), cc)]
    if on_prod:
        return "liveByConf"
    if any(is_on(confs.get(c, ""), cc) for c in DEV_CONFS):
        return "devOnly"
    return "nowhere"


SECTIONS = [
    ("overdue",
     "Past the deadline: delete the flag and every branch that reads it, and "
     "the line that sets it in any hg.conf"),
    ("liveByConf",
     "On in production hg.conf, but the code default is still FALSE: flip the "
     "default to TRUE in the code (the flag is then deleted %d releases "
     "later).  If it is really a per-site setting that mirrors should leave "
     "off, reply and it will be marked as a permanent setting in the catalog "
     "instead"),
    ("devOnly",
     "Set only on hgwdev, %d+ releases after it landed: release it or remove "
     "the feature"),
    ("nowhere",
     "Not set in any server hg.conf, %d+ releases after it landed: release it "
     "or remove the feature"),
]


def describe(g, kind, confs):
    tik = ", ".join("#%d" % t for t in g["tickets"])
    sites = g.get("sites")
    bits = []
    if kind == "overdue":
        bits.append("default TRUE since v%s, deadline v%s"
                    % (g["flipped"], g["sunset"]))
    else:
        bits.append("added v%s" % g["added"])
        if g["reverted"]:
            bits.append("flip v%s reverted" % g["flipped"])
        if kind == "liveByConf":
            bits.append("on in " + "/".join(c for c in PROD_CONFS
                                            if c in confs))
    if sites:
        bits.append("%d call site%s" % (sites, "" if sites == 1 else "s"))
    return "  %-26s %s%s" % (g["name"], ", ".join(bits),
                             ("  " + tik) if tik else "")


def body(first_name, rows, cur, cc):
    lines = ["Hi %s," % first_name if first_name else "Hi,", "",
             "This is the automated hg.conf release-gate notice from the v%s "
             "wrap-up (refs #37925)." % cur,
             "Policy: a release gate is deleted %d releases after its default "
             "flips TRUE, and" % cc.KEEP_AFTER_FLIP,
             "a gate still defaulting FALSE %d releases after it landed "
             "should be turned on or removed." % cc.QA_GRACE,
             "These gates are yours (you added the flag, or turned it on):"]
    for key, title in SECTIONS:
        mine = [r for r in rows if r[1] == key]
        if not mine:
            continue
        if "%d" in title:
            title = title % (cc.KEEP_AFTER_FLIP if key == "liveByConf"
                             else cc.QA_GRACE)
        lines += ["", title + ":"]
        lines += [describe(g, k, confs) for g, k, confs in mine]
    lines += ["",
              "Full list: hgConfCatalog.py --sunset "
              "(kent/src/hg/utils/hgConfCatalog).",
              "You will get this notice again at each release until the "
              "gate is resolved.", "",
              "-- the Build Meister"]
    return "\n".join(lines) + "\n"


def mail(subject, text, to, cc):
    cmd = ["mail", "-s", subject]
    if cc and cc != to:
        cmd += ["-c", cc]
    cmd.append(to)
    return subprocess.run(cmd, input=text, text=True).returncode == 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cache", required=True,
                    help="the age cache the sunset-report step rebuilt")
    ap.add_argument("--cc", default="braney@ucsc.edu")
    ap.add_argument("--send", action="store_true")
    ap.add_argument("--to", help="send every email here instead")
    args = ap.parse_args()

    os.environ["KENT_SRC"] = KENT_SRC
    sys.path.insert(0, CATALOG_DIR)
    import hgConfCatalog as cc
    hh = cc.load_harvester()
    if hh is None:
        sys.exit("harvestHgConf.py not importable from %s" % CATALOG_DIR)
    hh.CACHE = args.cache
    found, _ = hh.harvest()
    ages = hh.harvest_ages(names=sorted(hh.by_name(found)), refresh=False)
    sites = {n: len(d["sites"]) for n, d in hh.by_name(found).items()}

    cat = cc.build()
    cur = ages.get("current")
    life = cc.gate_lifecycle(cat, ages)
    states = cc.gate_states(life, cur)
    if not states:
        print("no overdue or stalled release gates at v%s" % cur)
        return 0
    confs = conf_settings(cc, set(states))

    by_owner = defaultdict(list)       # email -> [(gate, kind, confs)]
    names = {}
    for g in life:
        state = states.get(g["name"])
        if not state:
            continue
        g["sites"] = sites.get(g["name"])
        which = "firstTrue" if (state == "overdue" and g["flipped"]) else "first"
        commit = (ages.get(which, {}).get(g["name"]) or {}).get("commit")
        name, email = author(commit)
        if not email or "@" not in email:
            name, email = ("Build Meister", args.cc)
        names.setdefault(email, name)
        kind = classify(g, state, confs.get(g["name"], {}), cc)
        by_owner[email].append((g, kind, confs.get(g["name"], {})))

    order = {k: i for i, (k, _) in enumerate(SECTIONS)}
    failed = 0
    for email in sorted(by_owner):
        rows = sorted(by_owner[email],
                      key=lambda r: (order[r[1]], r[0]["name"]))
        first = (names.get(email) or "").split()[0] if names.get(email) else ""
        if first.islower():
            first = first.capitalize()
        n = len(rows)
        subject = ("v%s: %d hg.conf release gate%s to clean up"
                   % (cur, n, "" if n == 1 else "s"))
        text = body(first, rows, cur, cc)
        to = args.to or email
        if args.send or args.to:
            ok = mail(subject, text, to, None if args.to else args.cc)
            print("%s %s (%d gates)%s" % ("sent" if ok else "FAILED", to, n,
                  "" if args.to else ", cc " + args.cc))
            failed += 0 if ok else 1
        else:
            print("=== To: %s  Cc: %s\n=== Subject: %s\n%s"
                  % (to, args.cc, subject, text))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
