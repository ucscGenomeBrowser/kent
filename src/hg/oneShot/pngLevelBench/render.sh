#!/bin/bash
# render each corpus URL against a parked hgTracks and keep the png.
#
#   render.sh urlFile outDir port [jobs]
#
# urlFile is the output of pickUrls.py, count and query string per line.  The
# pngs land in outDir as 0001.png and so on, and outDir/renderLog.tsv records
# the http code, the mime type, the size and the query for every one of them.
# A response that is not a png is deleted; an error page is not a track image.
#
# hgRenderTracks returns the image itself rather than a page.  The explicit
# pix= matters: without it the first hit returns addPixAndReloadPage() and no
# track is ever drawn.
set -u
urlFile="$1"; outDir="$2"; port="$3"; jobs="${4:-1}"
mkdir -p "$outDir"
log="$outDir/renderLog.tsv"
: > "$log"

render() {
    i="$1"; count="$2"; query="$3"
    out=$(printf "%s/%04d.png" "$outDir" "$i")
    code=$(curl -s --max-time 90 -o "$out" -w "%{http_code}" \
        "http://127.0.0.1:$port/cgi-bin/hgRenderTracks?${query}&pix=1100")
    type=$(file -b --mime-type "$out" 2>/dev/null)
    size=$(stat -c %s "$out" 2>/dev/null)
    printf "%d\t%s\t%s\t%s\t%s\t%s\n" "$i" "$count" "$code" "$type" "$size" "$query" >> "$log"
    [ "$type" = "image/png" ] || rm -f "$out"
}
export -f render
export outDir port log

nl -ba -w1 -s$'\t' "$urlFile" \
  | xargs -d'\n' -n1 -P "$jobs" bash -c 'IFS=$'"'"'\t'"'"' read -r i count query <<< "$0"; render "$i" "$count" "$query"'
echo "$(grep -c . "$log") renders, $(ls "$outDir"/*.png 2>/dev/null | wc -l) pngs kept"
