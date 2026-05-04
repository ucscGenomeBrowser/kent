#!/usr/bin/env python3
# galaxyCleanup.py - delete a Galaxy invocation and purge its history
#
# usage: galaxyCleanup.py <profileJson> <invocationId>
#
# Use only after the workflow is complete and results have been
# downloaded -- purging the history is irreversible and frees the
# Galaxy-side disk space the workflow consumed.
#
# Reads galaxy_url and galaxy_user_key from the planemo profile JSON.
# Exits 0 on success (or if the invocation is already gone), non-zero
# on API error.

import json
import sys
import urllib.error
import urllib.request


def apiCall(method, url, apiKey):
    req = urllib.request.Request(
        url, method=method, headers={"x-api-key": apiKey})
    try:
        with urllib.request.urlopen(req) as r:
            body = r.read()
            return json.loads(body) if body else {}
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return None
        raise


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: galaxyCleanup.py <profileJson> <invocationId>")
    profileJson, invocationId = sys.argv[1], sys.argv[2]

    with open(profileJson) as f:
        prof = json.load(f)
    galaxyUrl = prof["galaxy_url"].rstrip("/")
    apiKey = prof["galaxy_user_key"]
    if not galaxyUrl or not apiKey:
        sys.exit(f"ERROR: could not read galaxy_url or galaxy_user_key "
                 f"from {profileJson}")

    inv = apiCall("GET",
                  f"{galaxyUrl}/api/invocations/{invocationId}", apiKey)
    if inv is None:
        print(f"# invocation {invocationId} already gone", file=sys.stderr)
        return
    historyId = inv.get("history_id")

    # purge first -- this is the step that actually frees disk
    if historyId:
        try:
            apiCall("DELETE",
                    f"{galaxyUrl}/api/histories/{historyId}?purge=true",
                    apiKey)
#           print(f"# purged history {historyId}", file=sys.stderr)
        except urllib.error.HTTPError as e:
            sys.exit(f"ERROR: failed to purge history {historyId}: {e}")
    else:
        print(f"# WARNING: invocation {invocationId} has no history_id",
              file=sys.stderr)

    try:
        apiCall("DELETE",
                f"{galaxyUrl}/api/invocations/{invocationId}", apiKey)
#       print(f"# deleted invocation {invocationId}", file=sys.stderr)
    except urllib.error.HTTPError as e:
        # history is already purged, so disk is reclaimed -- the
        # invocation row remaining is cosmetic
        print(f"# WARNING: invocation delete returned {e.code}, "
              f"history already purged", file=sys.stderr)


if __name__ == "__main__":
    main()
