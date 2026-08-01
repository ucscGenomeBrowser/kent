#!/bin/bash
# Render README.md (the Docent language reference) to a standalone HTML page.
#
#     ./mkref.sh                       # -> ~/public_html/docent.html
#     ./mkref.sh /some/where/ref.html
#
# The published page is generated, never hand-edited: edit README.md and re-run.
# (The previous hand-written page went stale the moment the language was renamed.)

set -e
out="${1:-$HOME/public_html/docent.html}"
here="$(cd "$(dirname "$0")" && pwd)"
css="$(mktemp)"
trap 'rm -f "$css"' EXIT

cat > "$css" <<'EOF'
<style>
  /* Overrides pandoc's built-in template CSS (Georgia/20px), which precedes this. */
  html { font-size: 16px; font-family: inherit; }
  body { font: 15px/1.6 -apple-system, Segoe UI, Roboto, Helvetica, Arial, sans-serif;
         max-width: 62rem; margin: 2rem auto; padding: 0 1.2rem; color: #1a1a1a; }
  h1 { font-size: 1.6rem; margin-bottom: .3rem; }
  h2 { font-size: 1.15rem; margin-top: 2.2rem; border-bottom: 1px solid #ddd;
       padding-bottom: .2rem; }
  table { border-collapse: collapse; width: 100%; margin: .6rem 0 1.2rem; }
  th, td { border: 1px solid #ddd; padding: .45rem .6rem; text-align: left;
           vertical-align: top; }
  th { background: #f5f5f5; }
  td:first-child { white-space: nowrap; }
  code { background: #f2f2f2; padding: .05rem .3rem; border-radius: 3px;
         font: 13px/1.45 SFMono-Regular, Menlo, Consolas, monospace; }
  pre { background: #f7f7f7; border: 1px solid #e2e2e2; border-radius: 4px;
        padding: .7rem .9rem; overflow-x: auto; }
  pre code { background: none; padding: 0; }
  blockquote { border-left: 3px solid #ddd; margin-left: 0; padding-left: 1rem;
               color: #555; }
  .src { color: #666; font-size: .9rem; border-top: 1px solid #ddd;
         margin-top: 2.5rem; padding-top: .8rem; }
</style>
EOF

pandoc "$here/README.md" \
    --from=gfm --to=html5 --standalone \
    --metadata title="Docent — a language for authoring Genome Browser tours" \
    --include-in-header="$css" \
    -o "$out"

# Pandoc repeats the metadata title as an <h1>; the README supplies its own.
perl -0pi -e 's{<header id="title-block-header">.*?</header>\n}{}s' "$out"

cat >> "$out" <<EOF
<p class="src">Generated from <code>kent/src/hg/utils/docent/README.md</code> by
<code>mkref.sh</code> — edit the README, not this page.</p>
</body></html>
EOF
# The appended footer follows pandoc's own </body></html>; strip the first pair.
perl -0pi -e 's{</body>\n</html>\n(?=<p class="src")}{}s' "$out"

chmod 644 "$out"
echo "wrote $out"
