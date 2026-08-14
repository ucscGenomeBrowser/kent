#!/bin/bash
#
# smoke-instance.sh [tip|beta|beta-arm64|rel ...] [--version NN]
#
# Quick smoke test of one or more docker browser QA instances on hgwdev. With no
# instance arguments it tests the two beta instances (beta + beta-arm64) -- the
# ones a weekly build stands up at do_final. Name one or more instances to test
# just those.
#
# For each instance it checks, over the instance's loopback port, that:
#   - the kent-<name> container is running,
#   - hgGateway serves the gateway page,
#   - hgTracks renders a real image for hg38 AND hg19 -- the drawn PNG plus the
#     expected track names prove Apache + the CGI + MariaDB + trackDb all work
#     end to end (a bare HTTP 200 does not: CGI error dumps are 200 too, so we
#     also assert the page is NOT a UCSC HGERROR / "Very Early Error" page),
#   - hgBlat and hgTables load,
#   - and it reports the CGI version parsed from the hgTracks title. Pass
#     --version NN (e.g. --version 501) to make a version mismatch a failure.
#
# Exit status is 0 only if every check on every instance passed. refs #37655
#
set -u

# ---- args -------------------------------------------------------------------
want_version=""
instances=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --version|-v) want_version="${2:?--version needs a number}"; shift 2 ;;
        tip|beta|beta-arm64|rel) instances+=("$1"); shift ;;
        -h|--help)
            echo "usage: $(basename "$0") [tip|beta|beta-arm64|rel ...] [--version NN]" >&2
            exit 1 ;;
        *) echo "unknown argument: $1" >&2; exit 1 ;;
    esac
done
[[ ${#instances[@]} -eq 0 ]] && instances=(beta beta-arm64)

port_of() {
    case "$1" in
        tip)        echo 8081 ;;
        beta)       echo 8082 ;;
        rel)        echo 8083 ;;
        beta-arm64) echo 8084 ;;
    esac
}

# generous timeout: the arm64 instance runs emulated (QEMU) and is slow.
CURL="curl -s -m 90"
overall=0

# check <label> <url> <positive-grep-ERE> [min-bytes]
#   passes when: HTTP 200, body has no UCSC error sentinel, body matches the
#   positive pattern, and body is at least min-bytes (default 1).
check() {
    local label="$1" url="$2" want="$3" min="${4:-1}"
    local body code size
    body="$($CURL -w '\n%{http_code}' "$url" 2>/dev/null)"
    code="${body##*$'\n'}"
    body="${body%$'\n'*}"
    size=${#body}
    if [[ "$code" != 200 ]]; then
        printf '    FAIL  %-16s HTTP %s\n' "$label" "$code"; overall=1; return 1
    fi
    # UCSC error pages wrap the message in an HGERROR comment or a "Very Early
    # Error" title. NB: the literal "-- ERROR --" also appears mid-page in every
    # hgTracks page's standard warn-box JavaScript, so it is only a real error
    # when it TRAILS the body (a late errAbort appends it at the very end) --
    # match it only in the last 40 chars, not anywhere in the body.
    if grep -qE 'HGERROR-START|Very Early Error' <<<"$body" \
       || [[ "${body: -40}" == *"-- ERROR --"* ]]; then
        local msg; msg="$(grep -oE '<!-- HGERROR-START -->[^<]*' <<<"$body" | head -c 120)"
        printf '    FAIL  %-16s UCSC error page: %s\n' "$label" "${msg:-error sentinel present}"
        overall=1; return 1
    fi
    if [[ "$size" -lt "$min" ]]; then
        printf '    FAIL  %-16s body too small (%s < %s bytes)\n' "$label" "$size" "$min"
        overall=1; return 1
    fi
    if ! grep -qE "$want" <<<"$body"; then
        printf '    FAIL  %-16s missing expected content /%s/\n' "$label" "$want"
        overall=1; return 1
    fi
    printf '    ok    %-16s HTTP 200, %s bytes\n' "$label" "$size"
    return 0
}

for name in "${instances[@]}"; do
    port="$(port_of "$name")"
    container="kent-$name"
    base="http://127.0.0.1:${port}"
    echo "== $container  ($base) =="

    if ! docker container inspect "$container" >/dev/null 2>&1; then
        printf '    FAIL  %-16s no such container (not built/started yet?)\n' "container"
        overall=1; echo; continue
    fi
    if [[ "$(docker inspect -f '{{.State.Running}}' "$container" 2>/dev/null)" != true ]]; then
        printf '    FAIL  %-16s container exists but is not running\n' "container"
        overall=1; echo; continue
    fi
    printf '    ok    %-16s running\n' "container"

    check gateway    "$base/cgi-bin/hgGateway"                                "Genome Browser Gateway"
    # full render: real drawn image (hgt/hgt_*.png) + a known track from the DB
    check hgTracks-hg38 "$base/cgi-bin/hgTracks?db=hg38&pix=1200&hgt.reset=1" "hgt/hgt_[a-z0-9_]+\.png" 50000
    check hgTracks-hg38-trk "$base/cgi-bin/hgTracks?db=hg38&pix=1200"         "side_knownGene|side_ncbiRefSeq" 50000
    check hgTracks-hg19 "$base/cgi-bin/hgTracks?db=hg19&pix=1200&hgt.reset=1" "hgt/hgt_[a-z0-9_]+\.png" 50000
    check hgBlat     "$base/cgi-bin/hgBlat"                                   "BLAT"
    check hgTables   "$base/cgi-bin/hgTables?db=hg38"                         "Table Browser"

    # version from the hgTracks page title: "... UCSC Genome Browser vNNN"
    ver="$($CURL "$base/cgi-bin/hgTracks?db=hg38" 2>/dev/null \
            | grep -oE 'UCSC Genome Browser v[0-9]+' | head -1 | grep -oE 'v[0-9]+')"
    if [[ -z "$ver" ]]; then
        printf '    FAIL  %-16s could not read CGI version from title\n' "version"; overall=1
    elif [[ -n "$want_version" && "$ver" != "v$want_version" ]]; then
        printf '    FAIL  %-16s %s but expected v%s\n' "version" "$ver" "$want_version"; overall=1
    else
        printf '    ok    %-16s %s\n' "version" "$ver"
    fi
    echo
done

if [[ "$overall" -eq 0 ]]; then
    echo "SMOKE PASS: all checks passed."
else
    echo "SMOKE FAIL: one or more checks failed (see above)."
fi
exit "$overall"
