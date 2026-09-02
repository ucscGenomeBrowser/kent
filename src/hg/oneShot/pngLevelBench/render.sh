#!/bin/bash
# render each corpus URL against the parked hgTracks and keep the png
S="$1"; PORT=48088; i=0
: > $S/renderLog.tsv
while IFS=$'\t' read -r count query; do
    i=$((i+1))
    out=$(printf "%s/corpus/%04d.png" "$S" "$i")
    code=$(curl -s --max-time 90 -o "$out" -w "%{http_code}" \
        "http://127.0.0.1:$PORT/cgi-bin/hgRenderTracks?${query}&pix=1100")
    type=$(file -b --mime-type "$out" 2>/dev/null)
    size=$(stat -c %s "$out" 2>/dev/null)
    printf "%d\t%s\t%s\t%s\t%s\t%s\n" "$i" "$count" "$code" "$type" "$size" "$query" >> $S/renderLog.tsv
    if [ "$type" != "image/png" ]; then rm -f "$out"; fi
done < $S/corpusUrls.tsv
echo "renders done" > $S/render.done
