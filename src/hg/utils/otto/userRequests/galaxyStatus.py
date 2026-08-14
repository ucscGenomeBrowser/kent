#!/usr/bin/env python3
# galaxyStatus.py - report Galaxy job queue counts for the planemo profile user
#
# usage: galaxyStatus.py <profileJson>
#
# Emits one JSON object on stdout:
#   {"running": N, "queued": N, "new": N, "ts": "<UTC iso>"}
# Exits non-zero on any failure with the reason on stderr.
#
# Reads galaxy_url and galaxy_user_key from the planemo profile JSON,
# matching galaxyState.py / galaxyCleanup.py.

import json
import sys
import urllib.parse
import urllib.request
from datetime import datetime, timezone


def apiGet(url, apiKey, timeoutSec=8):
    req = urllib.request.Request(url, headers={"x-api-key": apiKey})
    with urllib.request.urlopen(req, timeout=timeoutSec) as r:
        return json.load(r)


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: galaxyStatus.py <profileJson>")
    profileJson = sys.argv[1]
    with open(profileJson) as f:
        prof = json.load(f)
    galaxyUrl = prof["galaxy_url"].rstrip("/")
    apiKey = prof["galaxy_user_key"]
    if not galaxyUrl or not apiKey:
        sys.exit(f"ERROR: missing galaxy_url or galaxy_user_key in {profileJson}")

    me = apiGet(f"{galaxyUrl}/api/users/current", apiKey)
    userId = me.get("id")
    if not userId:
        sys.exit("ERROR: /api/users/current returned no id")

    counts = {}
    for state in ("running", "queued", "new"):
        q = urllib.parse.urlencode({"user_id": userId, "state": state})
        jobs = apiGet(f"{galaxyUrl}/api/jobs?{q}", apiKey)
        counts[state] = len(jobs)
    counts["ts"] = datetime.now(timezone.utc).isoformat(timespec="seconds")
    print(json.dumps(counts))


if __name__ == "__main__":
    main()
