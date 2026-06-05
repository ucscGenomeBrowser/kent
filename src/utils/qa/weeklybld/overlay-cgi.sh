#!/bin/bash
#
# overlay-cgi.sh <tip|beta>
#
# Copy the matching hgwdev CGIs over the public-release CGIs the image shipped
# with, so kent-tip reflects master (hgwdev's alpha cgi-bin) and kent-beta
# reflects the branch beta (cgi-bin-beta). Also copies the matching JS and CSS
# (htdocs/style) so the browser's JavaScript and stylesheets stay in sync with
# the CGI C code.
#
# Only the top-level CGI executables are copied, not the assorted data and
# dev-only subdirectories (otto/, crom_dir/, hgPhyloPlaceData/, ...). Those
# come with the image's published cgi-bin, and some hold private configs or
# files this account cannot read; copying just the binaries keeps the overlay
# to actual code and keeps anything private out of the container.
#
# The transfer is a tar stream piped into the running container -- a copy, not
# a mount -- so nothing from the hgwdev filesystem is mounted inside the
# container. The container's own hg.conf is left untouched, so it keeps
# pointing at its own MariaDB rather than hgwdev's.
# refs #37655
#
set -eEu

usage() {
    echo "usage: $(basename "$0") tip|beta" >&2
    exit 1
}

[[ $# -eq 1 ]] || usage
name="$1"
case "$name" in
    tip)  cgiSrc=/usr/local/apache/cgi-bin;      jsSrc=/usr/local/apache/htdocs/js;      styleSrc=/usr/local/apache/htdocs/style ;;
    beta) cgiSrc=/usr/local/apache/cgi-bin-beta; jsSrc=/usr/local/apache/htdocs-beta/js; styleSrc=/usr/local/apache/htdocs-beta/style ;;
    *)    usage ;;
esac
container="kent-$name"

# CGIs: top-level executables and symlinks only (no recursion into data
# subdirs), excluding the container's own hg.conf and the UCSC-only RNAplot.
( cd "$cgiSrc" && find . -maxdepth 1 \( -type f -o -type l \) \
        ! -name hg.conf ! -name hg.conf.local ! -name RNAplot -print0 \
    | tar --create --file=- --null --no-recursion --files-from=- ) \
    | docker exec -i "$container" tar -C /usr/local/apache/cgi-bin -xf -

# JS: keep the browser's JavaScript matched to the CGI version just copied.
( cd "$jsSrc" && tar --ignore-failed-read --create --file=- . ) \
    | docker exec -i "$container" tar -C /usr/local/apache/htdocs/js -xf -

# CSS: keep the browser's stylesheets matched to the CGI version just copied.
( cd "$styleSrc" && tar --ignore-failed-read --create --file=- . ) \
    | docker exec -i "$container" tar -C /usr/local/apache/htdocs/style -xf -
