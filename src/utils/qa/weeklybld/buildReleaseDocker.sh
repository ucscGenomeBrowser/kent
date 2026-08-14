#!/bin/bash
#
# buildReleaseDocker.sh <stage>      e.g.  buildReleaseDocker.sh v499
#
# Build and push the PUBLIC release docker images that mirrors pull:
#   genomebrowser/server:<stage>-amd64, :<stage>-arm64, the :<stage> manifest,
#   and the :latest manifest.
#
# amd64: build the base image from src/product/installer/docker/Dockerfile
#   (browserSetup.sh -b install rsyncs the public release CGIs from
#   hgdownload.soe.ucsc.edu::cgi-bin), then OVERLAY the local hgwdev beta CGIs
#   (/usr/local/apache/cgi-bin-beta) plus the matching beta js/style as a COPY
#   layer. This is the same seed overlay-cgi.sh does for the kent-beta QA
#   container, but baked into the published image instead of streamed into a
#   running one. It means the release image reflects the just-built beta code
#   immediately, with NO wait for the RR/production push to propagate to
#   hgdownload (which is what gated the docker step before). Only the top-level
#   CGI executables are overlaid (no data subdirs like otto/, hgPhyloPlaceData/),
#   and hg.conf/hg.conf.local/RNAplot are left as the base image shipped them, so
#   nothing private leaks in and the image keeps its own DB config.
#
# arm64: built normally -- browserSetup compiles the beta branch from source, so
#   the arm64 image already contains the beta code; there is no prebuilt-binary
#   shortcut and no cross-arch overlay, so it needs no seeding.
#
# Mirrors the selection rules of overlay-cgi.sh. refs #37655 #37750
#
set -eEu -o pipefail

stage="${1:?usage: $(basename "$0") <stage, e.g. v499>}"
repo=genomebrowser/server

selfDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKERDIR="$(cd "$selfDir/../../../product/installer/docker" && pwd)"

cgiSrc=/usr/local/apache/cgi-bin-beta
jsSrc=/usr/local/apache/htdocs-beta/js
styleSrc=/usr/local/apache/htdocs-beta/style

log() { echo "[$(date '+%F %T')] buildReleaseDocker: $*"; }

# arm64 cross-build needs the QEMU binfmt handlers (cleared on reboot); idempotent.
log "Registering QEMU binfmt handlers..."
docker run --privileged --rm tonistiigi/binfmt --install all >/dev/null 2>&1 \
    || log "WARNING: binfmt registration failed; arm64 build may fail"

# ---- stage the beta overlay into a throwaway build context ----
ctx="$(mktemp -d)"
trap 'rm -rf "$ctx"' EXIT
mkdir -p "$ctx/cgi-bin" "$ctx/js" "$ctx/style"
# top-level CGI executables/symlinks only; exclude DB config and the UCSC-only RNAplot
( cd "$cgiSrc" && find . -maxdepth 1 \( -type f -o -type l \) \
        ! -name hg.conf ! -name hg.conf.local ! -name RNAplot -print0 \
    | tar --create --null --no-recursion --files-from=- -f - ) | tar -C "$ctx/cgi-bin" -xf -
( cd "$jsSrc"    && tar --ignore-failed-read -cf - . ) | tar -C "$ctx/js"    -xf -
( cd "$styleSrc" && tar --ignore-failed-read -cf - . ) | tar -C "$ctx/style" -xf -
log "Staged $(find "$ctx/cgi-bin" -maxdepth 1 | tail -n +2 | wc -l) CGIs, $(find "$ctx/js" -type f | wc -l) js, $(find "$ctx/style" -type f | wc -l) style files for overlay."

cat > "$ctx/Dockerfile.overlay" <<EOF
FROM ${repo}:${stage}-amd64-base
COPY cgi-bin/ /usr/local/apache/cgi-bin/
COPY js/      /usr/local/apache/htdocs/js/
COPY style/   /usr/local/apache/htdocs/style/
EOF

# ---- build amd64 (base from public CGIs, then beta overlay) ----
log "Building amd64 base image..."
docker build --no-cache --platform linux/amd64 -t "${repo}:${stage}-amd64-base" "$DOCKERDIR"
log "Building amd64 image with beta overlay..."
docker build --platform linux/amd64 -t "${repo}:${stage}-amd64" -f "$ctx/Dockerfile.overlay" "$ctx"

# ---- build arm64 (compiles beta branch from source) ----
log "Building arm64 image (beta source compile)..."
docker build --no-cache --platform linux/arm64 -t "${repo}:${stage}-arm64" "$DOCKERDIR"

# ---- verify the overlay landed before pushing anything ----
localSize=$(stat -c %s "$cgiSrc/hgTracks")
imgSize=$(docker run --rm --entrypoint /usr/bin/stat "${repo}:${stage}-amd64" -c %s /usr/local/apache/cgi-bin/hgTracks)
if [[ "$imgSize" != "$localSize" ]]; then
    echo "ERROR: amd64 image hgTracks ($imgSize) != local cgi-bin-beta hgTracks ($localSize); overlay did not take. Not pushing." >&2
    exit 1
fi
log "Verified amd64 image hgTracks matches cgi-bin-beta ($imgSize bytes)."

# ---- push images + manifests ----
log "Pushing amd64 + arm64 images..."
docker push "${repo}:${stage}-amd64"
docker push "${repo}:${stage}-arm64"

log "Creating + pushing manifests :${stage} and :latest..."
docker manifest rm "${repo}:${stage}" >/dev/null 2>&1 || true
docker manifest create "${repo}:${stage}" "${repo}:${stage}-amd64" "${repo}:${stage}-arm64"
docker manifest push "${repo}:${stage}"
docker manifest rm "${repo}:latest" >/dev/null 2>&1 || true
docker manifest create "${repo}:latest" "${repo}:${stage}-amd64" "${repo}:${stage}-arm64"
docker manifest push "${repo}:latest"

# tidy the intermediate base tag (layers stay, referenced by the overlay image)
docker rmi "${repo}:${stage}-amd64-base" >/dev/null 2>&1 || true
log "Done. Pushed ${repo}:${stage} (+ :latest) for amd64 and arm64."
