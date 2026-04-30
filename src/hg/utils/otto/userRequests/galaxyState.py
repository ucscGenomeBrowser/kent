#!/usr/bin/env python3
# galaxyState.py - check Galaxy invocation state, print one status word
#
# usage: galaxyState.py <profileJson> <invocationId>
#
# Prints exactly one of:
#   pending  - still running or starting up; caller should exit 0 and retry
#   complete - all jobs ok; caller should download results
#   failed   - cancelled / failed / job errors; details on stderr
#
# Reads galaxy_url and galaxy_user_key from the planemo profile JSON.

import json
import sys
import urllib.request


def apiGet(url, apiKey):
    req = urllib.request.Request(url, headers={"x-api-key": apiKey})
    with urllib.request.urlopen(req) as r:
        return json.load(r)


def reportFailures(galaxyUrl, apiKey, inv):
    for step in inv.get("steps", []):
        label = step.get("workflow_step_label") or "unlabeled"
        orderIdx = step.get("order_index")
        jobId = step.get("job_id")
        if jobId:
            job = apiGet(f"{galaxyUrl}/api/jobs/{jobId}", apiKey)
            if job.get("state") == "error":
                print(f"  FAILED step {orderIdx}: {label} (job {jobId})",
                      file=sys.stderr)
        subId = step.get("subworkflow_invocation_id")
        if subId:
            subSum = apiGet(
                f"{galaxyUrl}/api/invocations/{subId}/jobs_summary", apiKey)
            subErr = subSum.get("states", {}).get("error", 0)
            if subErr > 0:
                print(f"  FAILED step {orderIdx}: {label} "
                      f"(sub-workflow {subId}, {subErr} error(s))",
                      file=sys.stderr)


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: galaxyState.py <profileJson> <invocationId>")
    profileJson, invocationId = sys.argv[1], sys.argv[2]

    with open(profileJson) as f:
        prof = json.load(f)
    galaxyUrl = prof["galaxy_url"].rstrip("/")
    apiKey = prof["galaxy_user_key"]
    if not galaxyUrl or not apiKey:
        print(f"ERROR: could not read galaxy_url or galaxy_user_key from "
              f"{profileJson}", file=sys.stderr)
        print("failed")
        return

    inv = apiGet(f"{galaxyUrl}/api/invocations/{invocationId}", apiKey)
    state = inv.get("state", "unknown")
    print(f"# invocation state: {state}", file=sys.stderr)

    if state in ("cancelled", "failed"):
        print(f"ERROR: workflow {state} -- invocation {invocationId}",
              file=sys.stderr)
        print("failed")
        return
    if state in ("new", "ready"):
        print("# workflow still starting up, will check again later",
              file=sys.stderr)
        print("pending")
        return
    if state not in ("scheduled", "completed"):
        print(f"# unexpected state '{state}', will check again later",
              file=sys.stderr)
        print("pending")
        return

    # "scheduled" / "completed" mean all steps dispatched, but jobs
    # may still be running.  Check the jobs_summary aggregate counts.
    summary = apiGet(
        f"{galaxyUrl}/api/invocations/{invocationId}/jobs_summary", apiKey)
    states = summary.get("states", {})
    terminal = {"ok", "error", "deleted", "skipped", "paused"}
    active = {k: v for k, v in states.items() if k not in terminal}

    if sum(active.values()) > 0:
        desc = ", ".join(f"{v} {k}" for k, v in active.items())
        print(f"# {sum(active.values())} jobs still active ({desc}), "
              f"will check again later", file=sys.stderr)
        print("pending")
        return

    errorCount = states.get("error", 0)
    if errorCount > 0:
        print(f"ERROR: {errorCount} job(s) had errors in invocation "
              f"{invocationId}", file=sys.stderr)
        reportFailures(galaxyUrl, apiKey, inv)
        print("failed")
        return

    print("complete")


if __name__ == "__main__":
    main()
