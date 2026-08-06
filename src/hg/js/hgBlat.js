// hgBlat.js - client-side rendering of the hgBlat "Table" output mode.
//
// hgBlat.c emits an inline object  var hgBlatData = { config, hits }  and an empty
// <div id="blatResults">.  This script builds the whole results UI from that data:
//   - a card with a summary strip (query / length / assembly / hit count + actions)
//   - a sortable, filterable DataTable whose cells are rendered here (identity bar,
//     query-coverage bar, linked loci, action links, comma-formatted position)
//   - a docked "selected hit" detail panel updated on row click
// Header tooltips reuse the Genome Browser's own mechanism (title + convertTitleTagsToMouseovers).

/* jshint esnext: true */
/* global $, hgBlatData, convertTitleTagsToMouseovers, htmlEncode */

var blatSelectedRank = null;   // rank of the row shown in the detail panel

function blatFmt(n) {
    // 12345 -> "12,345"
    return Number(n).toLocaleString('en-US');
}

function blatIdColor(id) {
    // UCSC identity semantic colors
    if (id >= 98) { return '#1f7a34'; }
    if (id >= 95) { return '#4d7c0f'; }
    if (id >= 90) { return '#b45309'; }
    return '#b1301f';
}

function blatInjectStyle() {
    if (document.getElementById('blatStyle')) { return; }
    // UCSC house style (per the project's UCSC UI Style Guide): steel-blue section header, tan/white
    // content, navy links with maroon hover, #dbe4ee table header, zebra rows, navy selection bar.
    var css = `
    /* Plain white page background, matching the track-settings model (gb.css).  The framework body
     * is cream (#FFF9D2) and the hgInside content table carries a BGCOLOR=#FFFEE8 attribute; override
     * both to white so the results sit on one uniform white background. */
    body.cgi { background:#fff; }
    table.hgInside { background:#fff; }
    /* Main page header, styled like gb.css .gbTrackTitleBanner (hgGtexTrackSettings model): black
     * title on the house gold.  The framework's #sectTtl already holds "<assembly> BLAT Results". */
    .subheadingBar { background:#eaca92; padding:9px 16px; margin:0; border:0; box-sizing:border-box; }
    .subheadingBar #sectTtl { color:#000; font-weight:700; font-size:18px;
        display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:12px; }
    #blatResults { --accent:#003a72; --accentHover:#8b1a1a; --navy:#0a2b6b; --ink:#1e2833;
        --muted:#5b6572; --faint:#93a0ad; --line:#d0d0d0; --lineSoft:#ececec; --card:#ffffff;
        --panel:#ffffff; --section:#4c759c; --headrow:#ededed; --headrowLine:#cccccc;
        --sel:#cfe0f5; --hover:#f0f0f0; --stripe:#f7f7f7;
        --title:#eaca92; --titleLine:#d9bd82; --titleSub:#5a4a24;
        --btn:#ffffff; --btnLine:#999999; --btnText:#003a72; --btnHover:#eef2f7;
        --btnDark:#0a2b6b; --btnDarkHover:#0a2350;
        font-family:'Helvetica Neue',Helvetica,Arial,sans-serif; color:var(--ink); font-size:14px; }
    .blatCard { background:var(--card); border:1px solid var(--line); border-radius:0; overflow:hidden;
        box-shadow:0 1px 2px rgba(20,40,70,.10); margin:12px 0 24px; }
    /* Page actions sit in the gold main header (#sectTtl); no separate toolbar. */
    .blatHeadActions { display:flex; gap:8px; align-items:center; flex-wrap:wrap; }
    .blatStrip { display:flex; align-items:center; gap:24px; flex-wrap:wrap;
        padding:11px 18px; border-bottom:1px solid var(--line); background:var(--panel); }
    .blatStat { display:flex; flex-direction:column; gap:1px; }
    .blatStat .k { font-size:12px; color:var(--muted); font-weight:700; }
    .blatStat .v { font-size:14px; font-weight:700; color:var(--ink); }
    .blatDiv { width:1px; height:28px; background:var(--line); }
    .blatStripActions { margin-left:auto; display:flex; gap:8px; align-items:center; flex-wrap:wrap; }
    /* Buttons live both inside the card (#blatResults) and in the gold header (#sectTtl); scope to
     * both ids so the framework's link styles (which would add an underline) can't beat them. */
    #blatResults .blatPill, #sectTtl .blatPill, #blatResults .blatStripActions input[type=submit],
    #blatResults .blatStripActions input[type=button] {
        font-family:inherit; font-size:13px; font-weight:700; color:var(--btnText); background:var(--btn);
        border:1px solid var(--btnLine); border-radius:0; padding:5px 12px; cursor:pointer;
        text-decoration:none; display:inline-block; line-height:1.5; }
    #blatResults .blatPill:hover, #sectTtl .blatPill:hover,
    #blatResults .blatStripActions input[type=submit]:hover,
    #blatResults .blatStripActions input[type=button]:hover { background:var(--btnHover); text-decoration:none; }
    #blatResults .blatPill.primary, #sectTtl .blatPill.primary { background:var(--btnDark); color:#fff; border-color:var(--btnDark); }
    #blatResults .blatPill.primary:hover, #sectTtl .blatPill.primary:hover { background:var(--btnDarkHover); }
    /* In the gold header (#sectTtl) the framework's link styles would otherwise beat .blatPill, so
     * re-assert the button look with id-level specificity. */
    #sectTtl .blatPill { color:#003a72; background:#fff; border:1px solid #999; text-decoration:none; }
    #sectTtl .blatPill:hover { background:#eef2f7; text-decoration:none; }
    #sectTtl .blatPill.primary { background:#0a2b6b; color:#fff; border-color:#0a2b6b; }
    #sectTtl .blatPill.primary:hover { background:#0a2350; }
    .blatShareIcon { vertical-align:-2px; margin-right:6px; }
    .blatShareBox { display:flex; align-items:center; gap:10px; flex-wrap:wrap;
        padding:11px 18px; border-bottom:1px solid var(--line); background:var(--panel); }
    .blatShareMsg { font-size:14px; color:var(--muted); }
    .blatShareInput { flex:1; min-width:260px; font-size:13px; padding:5px 8px;
        border:1px solid var(--btnLine); border-radius:0; background:#fff; }
    .blatBanner { background:#fbf3e2; border:1px solid var(--titleLine); padding:10px 14px;
        margin:12px 0 0; font-size:14px; color:var(--ink); }
    .blatBanner a { color:var(--accent); }
    .blatBanner a:hover { color:var(--accentHover); }
    .blatSeqBar { flex:1 1 100%; display:flex; gap:8px; align-items:center; flex-wrap:wrap; }
    .blatSeqText { flex:1 1 100%; min-height:150px; margin-top:10px; padding:8px; white-space:pre;
        font-family:'Roboto Mono','Courier New',monospace; font-size:13px; color:var(--ink);
        border:1px solid var(--btnLine); background:#fff; overflow:auto; }
    #blatTable { font-size:13px; width:100%; border-collapse:collapse; }
    /* Let the long text columns wrap so the table shrinks to fit smaller screens, while the fixed
     * bar columns keep a min-width so they never compress into an overlap; when even that won't fit,
     * #blatTable_wrapper scrolls horizontally. */
    #blatTable td.blatPos, #blatTable td.queryCol { white-space:normal; word-break:break-word; }
    #blatTable td.scoreCol { min-width:118px; }
    #blatTable td.covCol { min-width:120px; }
    #blatTable thead th { background:var(--headrow); border-bottom:1px solid var(--headrowLine);
        font-size:12px; color:var(--navy); font-weight:700; padding:8px 12px; white-space:nowrap; }
    #blatTable tbody td { padding:8px 12px; border-bottom:1px solid var(--lineSoft); white-space:nowrap;
        vertical-align:middle; }
    #blatTable tbody tr { cursor:pointer; }
    #blatTable tbody tr:nth-child(even) { background:var(--stripe); }
    #blatTable tbody tr:hover { background:var(--hover); }
    #blatTable tbody tr.blatSel { background:var(--sel); box-shadow:inset 3px 0 0 var(--navy); }
    #blatTable td.num, #blatTable th.num { text-align:right; font-variant-numeric:tabular-nums; }
    #blatTable td.rankCol { color:var(--faint); }
    #blatTable td.strandCol { text-align:center; color:var(--muted); }
    #blatTable td.actionsCol a { color:var(--accent); font-weight:700; }
    #blatTable td.actionsCol .blatActSep { color:var(--line); margin:0 10px; }
    /* "New tab" shown as the usual box-with-arrow icon (via a CSS background so it isn't repeated in
     * every row's markup), sitting right after the position link with just a space between them. */
    #blatTable a.blatNewTab { display:inline-block; width:11px; height:11px;
        margin-left:4px; vertical-align:-1px; background-repeat:no-repeat; background-position:center;
        background-size:contain; background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 512 512'%3E%3Cpath fill='%23003a72' d='M320 0c-17.7 0-32 14.3-32 32s14.3 32 32 32h82.7L201.4 265.4c-12.5 12.5-12.5 32.8 0 45.3s32.8 12.5 45.3 0L448 109.3V192c0 17.7 14.3 32 32 32s32-14.3 32-32V32c0-17.7-14.3-32-32-32H320zM80 32C35.8 32 0 67.8 0 112V432c0 44.2 35.8 80 80 80H400c44.2 0 80-35.8 80-80V320c0-17.7-14.3-32-32-32s-32 14.3-32 32V432c0 8.8-7.2 16-16 16H80c-8.8 0-16-7.2-16-16V112c0-8.8 7.2-16 16-16H192c17.7 0 32-14.3 32-32s-14.3-32-32-32H80z'/%3E%3C/svg%3E"); }
    #blatTable a.blatNewTab:hover { opacity:.65; }
    #blatTable a { color:var(--accent); text-decoration:none; }
    #blatTable a:hover { color:var(--accentHover); text-decoration:underline; }
    .blatRowHint { padding:11px 18px 2px; font-size:13px; color:var(--muted); }
    .blatLocus { max-width:280px; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    .chrNote { color:var(--faint); text-decoration:none; margin-left:4px; }
    .chrNote:hover { color:var(--accent); }
    .blatScoreWrap { display:flex; align-items:center; gap:9px; justify-content:flex-end; }
    .blatScoreVal { font-size:13px; font-weight:700; text-align:right; font-variant-numeric:tabular-nums; }
    .blatScoreBar { flex:0 0 54px; height:8px; background:var(--headrow); border:1px solid var(--headrowLine); overflow:hidden; }
    .blatScoreBar > i { display:block; height:100%; background:var(--accent); }
    .blatIdPct { font-size:13px; font-weight:700; font-variant-numeric:tabular-nums; }
    .blatCov { position:relative; display:block; width:150px; max-width:100%; height:10px; background:var(--headrow); border:1px solid var(--headrowLine); }
    .blatCov > i { position:absolute; top:0; bottom:0; background:var(--accent); }
    #blatTable_filter { float:left; margin:0 0 10px; }
    #blatTable_filter input { width:300px; max-width:55vw; border:1px solid var(--btnLine);
        border-radius:0; padding:5px 9px; font-size:14px; }
    #blatTable_wrapper { padding:6px 0 4px; overflow-x:auto; }
    .blatDetail { border-bottom:1px solid var(--line); background:var(--panel); padding:14px 18px 16px; }
    .blatDetail .dhead { display:flex; align-items:baseline; gap:10px; margin-bottom:12px; flex-wrap:wrap; }
    .blatDetail .dhead .lab { font-size:13px; color:var(--muted); }
    .blatSelectHint { font-size:14px; color:var(--ink); }
    .blatSelectHint a { color:var(--accent); }
    .blatSelectHint a:hover { color:var(--accentHover); }
    .blatDetail .dhead .loc { font-size:14px; font-weight:700; color:var(--ink); }
    .blatDetailCard { display:flex; gap:26px; flex-wrap:wrap; padding:14px 16px; background:var(--card);
        border:1px solid var(--line); border-radius:0; }
    .blatTiles { display:grid; grid-template-columns:repeat(4,auto); gap:14px 26px; }
    .blatTile .k { font-size:13px; color:var(--muted); cursor:help; }
    .blatTile .v { font-size:14px; font-weight:700; color:var(--ink); }
    .blatDetailActions { display:flex; gap:8px; margin-top:14px; flex-wrap:wrap; }
    /* Rename modal.  On this page the old C-emitted inline rename form (#renameFormItem / #renameForm)
     * is replaced by the JS button + modal below, so keep the C markup hidden even though hgBlat.c
     * flips its display to block after buildBigPsl (our !important beats that inline style). */
    #renameFormItem, #renameForm { display:none !important; }
    .blatModalBg { position:fixed; top:0; right:0; bottom:0; left:0; z-index:1000;
        background:rgba(0,0,0,.4); display:flex; align-items:center; justify-content:center; }
    .blatModal { background:#fff; border:1px solid var(--line); border-radius:0;
        box-shadow:0 4px 18px rgba(0,0,0,.28); padding:18px 20px; min-width:340px; max-width:92vw; }
    .blatModalTitle { font-size:16px; font-weight:700; color:var(--ink); margin-bottom:12px; }
    .blatModalText { font-size:13px; color:var(--ink); line-height:1.5; max-width:430px; margin-bottom:14px; }
    .blatModalText a { color:var(--accent); }
    .blatModalText a:hover { color:var(--accentHover); }
    .blatModalLabel { display:block; font-size:12px; font-weight:700; color:var(--muted);
        margin:10px 0 3px; }
    .blatModalInput { width:100%; box-sizing:border-box; border:1px solid var(--btnLine);
        border-radius:0; padding:6px 8px; font-size:14px; font-family:inherit; background:#fff; }
    .blatModalBtns { display:flex; gap:8px; justify-content:flex-end; margin-top:18px; }
    `;
    var st = document.createElement('style');
    st.id = 'blatStyle';
    st.textContent = css;
    document.head.appendChild(st);
}

// ---- cell renderers ------------------------------------------------------

function blatPositionCell(hit) {
    // For alt/fix/random/chrUn sequences show an info icon linking to the FAQ ("What is chr_alt &
    // chr_fix?"), with the short explanation as its tooltip.  (Sits after the position link, not
    // nested inside it.)
    var note = hit.chromNote ?
        ` <a class="chrNote" target="_blank" href="../FAQ/FAQblat.html#blat1c" ` +
        `title="${htmlEncode(hit.chromNote)} Click to learn more in the BLAT FAQ.">&#9432;</a>` : '';
    // The position links to the Genome Browser at this match; the new-tab icon right after it opens
    // the same in a new tab (whitespace between them, no divider).
    // URLs are htmlEncode'd before going into href="": they can carry the user's query name, so an
    // unescaped double-quote would otherwise break out of the attribute (XSS).
    return `<a class="blatPos" title="Open the Genome Browser at this location" ` +
        `href="${htmlEncode(hit.browserUrl)}">${htmlEncode(hit.chrom)}:` +
        `${blatFmt(hit.tStart)}-${blatFmt(hit.tEnd)}</a>` +
        ` <a class="blatNewTab" target="_blank" href="${htmlEncode(hit.newTabUrl)}" ` +
        `title="Open match in a new tab" aria-label="Open match in a new tab"></a>${note}`;
}

function blatActionsCell(hit) {
    // The "Open" column now holds just the base-by-base alignment link (Browser moved to the Position
    // column).  detailsUrl is htcUserAli on a fresh search, htcBlatAlign on a shared-link reopen; guard
    // in case a future caller omits it.
    if (!hit.detailsUrl) { return ''; }
    // htmlEncode the URL: detailsUrl embeds the user's query name, so an unescaped quote could break
    // out of the href attribute (XSS).
    return `<a title="Show the base-by-base alignment of your sequence to the genome" ` +
        `href="${htmlEncode(hit.detailsUrl)}">Alignment</a>`;
}

function blatLocusCell(hit) {
    // Locus is plain text (not a link): the gene names are shown for context only.  The cell grows with
    // its content up to a max-width, then a very long locus (many overlapping genes) is clipped with a
    // CSS ellipsis; the full string is always available on mouseover (title).
    if (!hit.locusText) { return ''; }
    return `<div class="blatLocus" title="${htmlEncode(hit.locusText)}">${htmlEncode(hit.locusText)}</div>`;
}

function blatScoreCell(hit, maxScore) {
    // Score with a little bar chart after it, scaled to the highest score in this result set.
    var pct = maxScore > 0 ? (hit.score / maxScore * 100) : 0;
    return `<span class="blatScoreWrap"><span class="blatScoreVal">${blatFmt(hit.score)}</span>` +
        `<span class="blatScoreBar"><i style="width:${pct.toFixed(1)}%"></i></span></span>`;
}

function blatIdentityCell(hit) {
    // Just the percentage now (the bar chart moved to the Score column), kept in its semantic color.
    var c = blatIdColor(hit.identity);
    return `<span class="blatIdPct" style="color:${c}">${hit.identity.toFixed(1)}%</span>`;
}

function blatCoverageCell(hit) {
    var left = (hit.qStart - 1) / hit.qSize * 100;
    var width = (hit.qEnd - hit.qStart + 1) / hit.qSize * 100;
    var tip = `Query matches the genome at ${blatFmt(hit.qStart)}-${blatFmt(hit.qEnd)}bp out of ${blatFmt(hit.qSize)}bp`;
    return `<span class="blatCov" title="${tip}"><i style="left:${left.toFixed(1)}%;` +
        `width:${width.toFixed(1)}%"></i></span>`;
}

// ---- summary strip + detail panel ---------------------------------------

function blatSummaryStrip(cfg, queryCount) {
    var stat = (k, v) => `<div class="blatStat"><span class="k">${k}</span>` +
        `<span class="v">${v}</span></div>`;
    var div = '<span class="blatDiv"></span>';
    var assembly = stat('Assembly', htmlEncode(cfg.organism) + ' / ' + htmlEncode(cfg.db)) + div +
        stat('Matches', blatFmt(cfg.hitCount));
    var stats;
    if (cfg.multiQuery) {
        // With more than one query sequence a single query name/length would be wrong, so show the
        // number of distinct queries; each hit's own query is in the table's Query column.
        stats = stat('Queries', blatFmt(queryCount)) + div + assembly;
    } else {
        stats = stat('Query', htmlEncode(cfg.queryName)) + div +
            stat('Length', blatFmt(cfg.querySize) + ' bp') + div + assembly;
    }
    var actions = '';
    // "View all in browser" is the primary action, so it comes first.
    if (cfg.viewAllUrl) {
        actions += `<a class="blatPill" title="Open the Genome Browser with all these BLAT hits shown together as one custom track" href="${htmlEncode(cfg.viewAllUrl)}">View all in browser</a>`;
    }
    // "Show Query Sequence" opens the query FASTA in a panel (with Download / Copy). Only on a fresh
    // search, where the uploaded sequence is available (cfg.querySeqs emitted by hgBlat.c).
    if (cfg.querySeqs && cfg.querySeqs.length) {
        actions += '<button type="button" class="blatPill" id="blatSeqBtn" ' +
            'title="Show the sequence you searched with, in FASTA format">Show Query Sequence</button>';
    }
    // "Share a link" just reveals the page's stable URL (cfg.shareUrl, a trash-backed reopen link).
    // cfg.canShare covers old session-based links (?u=&s=), where the current URL is already shareable.
    if (cfg.shareUrl || cfg.canShare) {
        // A small share-nodes icon precedes the label so users learn to associate it with sharing.
        var shareIcon = '<svg class="blatShareIcon" viewBox="0 0 24 24" width="13" height="13" ' +
            'fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" ' +
            'stroke-linejoin="round" aria-hidden="true"><circle cx="18" cy="5" r="3"></circle>' +
            '<circle cx="6" cy="12" r="3"></circle><circle cx="18" cy="19" r="3"></circle>' +
            '<line x1="8.6" y1="10.5" x2="15.4" y2="6.5"></line>' +
            '<line x1="8.6" y1="13.5" x2="15.4" y2="17.5"></line></svg>';
        actions += '<button type="button" class="blatPill" id="blatShareBtn" ' +
            'title="Show a link that reopens these results (works for a limited time)">' +
            shareIcon + 'Share a link</button>';
    }
    // "Rename BLAT Track" opens a modal to rename the results custom track. This is a JS-native
    // button (renders immediately with the strip) that replaces the old C-emitted inline form, which
    // only appeared after the buildBigPsl AJAX finished and reflowed the page when clicked.
    if (cfg.canRename) {
        actions += '<button type="button" class="blatPill" id="blatRenameBtn" ' +
            'title="Rename this BLAT results custom track and its description">Rename BLAT Track</button>';
    }
    return `<div class="blatStrip">${stats}<span class="blatStripActions">${actions}</span></div>`;
}

var BLAT_TILE_TIPS = {
    'Score': 'BLAT score: matches minus mismatches and gap penalties. Higher is better.',
    'Identity': 'Percent identity of the aligned bases.',
    'Matches': 'Query bases that match the genome.',
    'Mismatch': 'Bases that differ between query and genome.',
    'Gaps': 'Number of gaps (insertions or deletions) in the alignment.',
    'Blocks': 'Number of ungapped aligned blocks.',
    'Strand': 'Genome strand the query matched (+ or -).',
    'Q span': 'Range of the query sequence that aligned (1-based).'
};

function blatTileSkeleton(label, id, color) {
    var style = color ? ` style="color:${color}"` : '';
    var tip = BLAT_TILE_TIPS[label] || '';
    return `<div class="blatTile"><div class="k" title="${htmlEncode(tip)}">${label}</div>` +
        `<div class="v" id="${id}"${style}></div></div>`;
}

function blatDetailSkeleton() {
    // Built once; blatRenderDetail() only updates values, so the tile-label tooltips
    // are wired a single time by convertTitleTagsToMouseovers.
    var tiles =
        blatTileSkeleton('Score', 'dvScore') +
        blatTileSkeleton('Identity', 'dvIdentity') +
        blatTileSkeleton('Matches', 'dvMatches') +
        blatTileSkeleton('Mismatch', 'dvMismatch') +
        blatTileSkeleton('Gaps', 'dvGaps') +
        blatTileSkeleton('Blocks', 'dvBlocks') +
        blatTileSkeleton('Strand', 'dvStrand') +
        blatTileSkeleton('Q span', 'dvQspan');
    document.getElementById('blatDetail').innerHTML =
        `<div class="dhead"><span class="lab">Selected hit</span>` +
        `<span class="loc" id="dvLoc"></span></div>` +
        `<div class="blatDetailCard"><div style="display:flex;flex-direction:column;gap:16px;min-width:250px">` +
        `<div class="blatTiles">${tiles}</div>` +
        `<div class="blatDetailActions">` +
        `<a class="blatPill" id="dvBrowser" title="Open this hit in the Genome Browser" href="#">Open in browser</a>` +
        `<a class="blatPill" id="dvNewTab" target="_blank" title="Open this hit in the Genome Browser in a new tab" href="#">Open in new tab</a></div></div>` +
        `<div id="dvAlignBox" style="flex:1;min-width:320px;border-left:1px solid var(--line);padding-left:24px;` +
        `display:flex;flex-direction:column;justify-content:center">` +
        `<div class="blatTile"><div class="k">Alignment</div></div>` +
        `<div id="dvAlign" style="font-size:14px;color:var(--muted);line-height:1.55;` +
        `margin:8px 0 14px;max-width:360px"></div>` +
        `<a class="blatPill" id="dvViewAlign" style="align-self:flex-start" ` +
        `title="See the base-by-base alignment of your query against this hit" href="#">` +
        `View alignment</a></div></div>`;
    if (typeof convertTitleTagsToMouseovers === 'function') { convertTitleTagsToMouseovers(); }
}

function blatSet(id, prop, val) {
    var e = document.getElementById(id);
    if (!e) { return; }
    if (prop === 'text') { e.textContent = val; }
    else if (prop === 'html') { e.innerHTML = val; }
    else if (prop === 'href') { e.setAttribute('href', val); }
    else if (prop === 'color') { e.style.color = val; }
}

function blatRenderDetail(hit) {
    if (!hit || !document.getElementById('blatDetail')) { return; }
    if (!document.getElementById('dvScore')) { blatDetailSkeleton(); }
    var idc = blatIdColor(hit.identity);
    var locus = hit.locusText ? htmlEncode(hit.locusText) + ' · ' : '';
    var q = hgBlatData.config.multiQuery ? htmlEncode(hit.qName) + ' · ' : '';
    blatSet('dvLoc', 'html',
        `#${hit.rank} · ${q}${locus}${htmlEncode(hit.chrom)}:${blatFmt(hit.tStart)}-${blatFmt(hit.tEnd)}`);
    blatSet('dvScore', 'text', blatFmt(hit.score));
    blatSet('dvIdentity', 'text', hit.identity.toFixed(1) + '%');
    blatSet('dvIdentity', 'color', idc);
    blatSet('dvMatches', 'text', blatFmt(hit.matches));
    blatSet('dvMismatch', 'text', blatFmt(hit.misMatch));
    blatSet('dvGaps', 'text', blatFmt(hit.gaps));
    blatSet('dvBlocks', 'text', blatFmt(hit.blocks));
    blatSet('dvStrand', 'text', hit.strand);
    blatSet('dvQspan', 'text', blatFmt(hit.qStart) + '–' + blatFmt(hit.qEnd));
    blatSet('dvBrowser', 'href', hit.browserUrl);
    blatSet('dvNewTab', 'href', hit.newTabUrl);
    // Show the Alignment box whenever a base-by-base alignment page is available (htcUserAli on a
    // fresh search, htcBlatAlign on a shared-link reopen); hide it only if detailsUrl is missing.
    var alignBox = document.getElementById('dvAlignBox');
    if (alignBox) { alignBox.style.display = hit.detailsUrl ? '' : 'none'; }
    if (hit.detailsUrl) {
        blatSet('dvViewAlign', 'href', hit.detailsUrl);
        blatSet('dvAlign', 'text',
            'See the base-by-base alignment of your query against ' + hit.chrom +
            ': matches, mismatches and gaps across the whole span.');
    }
}

function blatSelect(dt, rank) {
    blatSelectedRank = rank;
    $('#blatTable tbody tr').each(function() {
        var d = dt.row(this).data();
        $(this).toggleClass('blatSel', !!d && d.rank === rank);
    });
    var hit = hgBlatData.hits.find(h => h.rank === rank);
    blatRenderDetail(hit);
}

// ---- header tooltips (reuse the browser's title -> mouseover system) -----

var BLAT_HEADER_TIPS = {
    '#': 'Rank by the chosen sort order',
    'Query': 'The query sequence this hit came from',
    'Open in Genome Browser': 'Genomic location of the match (1-based). Click the position to ' +
        'open the Genome Browser there, or the icon to open it in a new tab.',
    'Show': 'Show the base-by-base alignment of your sequence to the genome',
    'Locus': 'Nearest gene(s), and whether the hit falls in an exon, intron, or intergenic region',
    'Score': 'BLAT score: matches minus mismatches and gap penalties. Higher is better.',
    'Identity': 'Percent identity of the aligned bases',
    'Strand': 'Genome strand the query matched (+ or -)',
    'Query coverage': 'Which part of the query aligned (blue) across its full length',
    'Span': 'Length of the match on the genome (bp). Larger than the query length means ' +
        'the alignment crosses introns or deletions.'
};

function blatApplyTooltips() {
    $('#blatTable thead th').each(function() {
        var tip = BLAT_HEADER_TIPS[$(this).text().trim()];
        if (tip) { $(this).attr('title', tip); }
    });
    if (typeof convertTitleTagsToMouseovers === 'function') {
        convertTitleTagsToMouseovers();
    }
}

// ---- share a link --------------------------------------------------------

function blatShareLink() {
    // No session, no AJAX: the results page already has a stable, shareable URL (hgBlat.c emits it as
    // cfg.shareUrl and blatBuild() pins it into the address bar with history.replaceState), so this
    // just shows/copies window.location.  The link reopens straight from the trash .pslx/.fa, so it
    // works until those trash files are cleaned - hence the retention note.
    var box = document.getElementById('blatShareBox');
    if (!box) { return; }
    if (box.style.display === 'flex') { box.style.display = 'none'; return; }   // toggle off
    var url = window.location.href;
    box.style.display = 'flex';
    box.innerHTML =
        '<span class="blatShareMsg" style="flex:1 1 100%">Shareable link — anyone with it can reopen ' +
        'these results. The results are stored temporarily, so the link works for at least 48 hours ' +
        'after they were last viewed.</span>' +
        '<input id="blatShareInput" class="blatShareInput" type="text" readonly>' +
        '<button type="button" class="blatPill" id="blatShareCopy" title="Copy the link to the clipboard">Copy</button>';
    var inp = document.getElementById('blatShareInput');
    inp.value = url;
    inp.focus();
    inp.select();
    $('#blatShareCopy').on('click', function() {
        inp.select();
        if (navigator.clipboard) { navigator.clipboard.writeText(url); }
        else { document.execCommand('copy'); }
        this.textContent = 'Copied';
    });
}

// ---- Rename BLAT track (modal) -------------------------------------------
// The results custom track is built (and renamed) by hgBlat.c's inline code, which exposes a small
// window.blatRenameCt(name, description) helper (it POSTs to hgc's buildBigPsl and rebuilds the
// track).  We reuse that helper (no new endpoint), just swapping its old inline toggle-form UI for a
// proper modal dialog.  The current name/description come from cfg (hgBlat.c), not a global, so this
// does not depend on any generic page-global.

function blatRenameModalHtml(cfg) {
    // hgSession link is relative (same /cgi-bin/), carrying db + hgsid so the session page opens in
    // this assembly and cart.
    var sessionUrl = `hgSession?db=${encodeURIComponent(cfg.db)}&hgsid=${encodeURIComponent(cfg.hgsid)}`;
    return '<div id="blatModalBg" class="blatModalBg" style="display:none">' +
        '<div class="blatModal" role="dialog" aria-modal="true" aria-labelledby="blatModalTitle">' +
        '<div class="blatModalTitle" id="blatModalTitle">Rename BLAT Track</div>' +
        '<div class="blatModalText">Every BLAT result is stored in its own track in the Genome ' +
        'Browser. You can rename the track here. Results will disappear after 2–3 days, unless ' +
        `they are saved into a <a href="${sessionUrl}">Session link</a>.</div>` +
        '<label class="blatModalLabel" for="blatRenameName">Track name</label>' +
        '<input id="blatRenameName" class="blatModalInput" type="text" maxlength="80">' +
        '<label class="blatModalLabel" for="blatRenameDesc">Description</label>' +
        '<input id="blatRenameDesc" class="blatModalInput" type="text" maxlength="120">' +
        '<div class="blatModalBtns">' +
        '<button type="button" class="blatPill" id="blatRenameCancel">Cancel</button>' +
        '<button type="button" class="blatPill primary" id="blatRenameOk">OK</button>' +
        '</div></div></div>';
}

function blatCloseRename() {
    var bg = document.getElementById('blatModalBg');
    if (bg) { bg.style.display = 'none'; }
}

function blatOpenRename() {
    var bg = document.getElementById('blatModalBg');
    if (!bg) { return; }
    // Pre-fill with the track's current name/description (emitted by hgBlat.c in cfg).
    var cfg = hgBlatData.config;
    document.getElementById('blatRenameName').value = cfg.trackName || '';
    document.getElementById('blatRenameDesc').value = cfg.trackDescription || '';
    bg.style.display = 'flex';
    document.getElementById('blatRenameName').focus();
    document.getElementById('blatRenameName').select();
}

function blatWireRename() {
    $('#blatRenameBtn').on('click', blatOpenRename);
    $('#blatRenameCancel').on('click', blatCloseRename);
    // Click on the dark backdrop (but not the dialog itself) closes.
    $('#blatModalBg').on('click', function(ev) {
        if (ev.target === this) { blatCloseRename(); }
    });
    $(document).on('keydown.blatRename', function(ev) {
        var bg = document.getElementById('blatModalBg');
        if (bg && bg.style.display !== 'none' && ev.key === 'Escape') { blatCloseRename(); }
    });
    $('#blatRenameOk').on('click', function() {
        var name = document.getElementById('blatRenameName').value.trim();
        var desc = document.getElementById('blatRenameDesc').value.trim();
        if (!name) { document.getElementById('blatRenameName').focus(); return; }
        // Reuse hgBlat.c's window.blatRenameCt(name, description): rebuilds the custom track under the
        // new name via the existing hgc buildBigPsl call.  Keep cfg in sync so a re-open of the modal
        // shows the new values.
        if (typeof window.blatRenameCt === 'function') {
            hgBlatData.config.trackName = name;
            hgBlatData.config.trackDescription = desc;
            window.blatRenameCt(name, desc);
        }
        blatCloseRename();
    });
}

// ---- FASTA viewer (generic) ----------------------------------------------

function blatToFasta(seqs) {
    // seqs: [{name, seq}, ...] -> FASTA text, sequence wrapped at 60 chars per line.
    return seqs.map(function(s) {
        var body = String(s.seq || '').toUpperCase().replace(/(.{60})/g, '$1\n').replace(/\n$/, '');
        return '>' + s.name + '\n' + body;
    }).join('\n');
}

function blatShowFasta(box, seqs, fileName) {
    // Render seqs as FASTA inside `box`, with Copy-to-clipboard and Download buttons. Generic — takes
    // any [{name, seq}] list so it can be reused for other sequences later.
    var fasta = blatToFasta(seqs);
    box.style.display = 'flex';
    box.innerHTML =
        '<div class="blatSeqBar">' +
        '<span class="blatShareMsg">Query sequence (FASTA):</span>' +
        '<button type="button" class="blatPill" id="blatSeqCopy" title="Copy the FASTA to the clipboard">Copy to Clipboard</button>' +
        '<button type="button" class="blatPill" id="blatSeqDownload" title="Download the FASTA as a .fa file">Download</button>' +
        '<button type="button" class="blatPill" id="blatSeqClose" title="Hide the query sequence">Close</button>' +
        '</div><textarea id="blatSeqText" class="blatSeqText" readonly></textarea>';
    var ta = document.getElementById('blatSeqText');
    ta.value = fasta;
    document.getElementById('blatSeqCopy').addEventListener('click', function() {
        ta.select();
        if (navigator.clipboard) { navigator.clipboard.writeText(fasta); }
        else { document.execCommand('copy'); }
        this.textContent = 'Copied';
    });
    document.getElementById('blatSeqDownload').addEventListener('click', function() {
        var a = document.createElement('a');
        a.href = URL.createObjectURL(new Blob([fasta], { type: 'text/plain' }));
        a.download = fileName || 'query.fa';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        setTimeout(function() { URL.revokeObjectURL(a.href); }, 0);
    });
    document.getElementById('blatSeqClose').addEventListener('click', function() {
        box.style.display = 'none';
    });
}

function blatShowQuerySeq() {
    var box = document.getElementById('blatSeqBox');
    if (box.style.display === 'flex') { box.style.display = 'none'; return; }   // toggle off
    blatShowFasta(box, hgBlatData.config.querySeqs, 'blatQuery.fa');
}

// ---- build ---------------------------------------------------------------

function blatBuild() {
    var cfg = hgBlatData.config;
    var hits = hgBlatData.hits;
    blatInjectStyle();

    // Pin a stable, shareable URL into the address bar (no server redirect) so refresh, bookmark and
    // "Share a link" all use the trash-backed reopen link instead of the transient POST/search URL.
    if (cfg.shareUrl) {
        try { history.replaceState(null, '', cfg.shareUrl); } catch (e) { /* older browsers: ignore */ }
    }

    var back = cfg.backUrl ?
        `<a class="blatPill" title="Return to the Genome Browser at your previous location (${htmlEncode(cfg.backPos)})" ` +
        `href="${htmlEncode(cfg.backUrl)}">Back to Genome Browser</a>` : '';
    // The page actions live in the gold main-header bar (framework #sectTtl), next to the title -
    // so there is no separate toolbar (.blatHead is gone).  Injected into #sectTtl below.
    var headActions =
        `${back}<a class="blatPill primary" title="Start a new BLAT search" href="${htmlEncode(cfg.newSearchUrl)}">New BLAT search</a>`;

    // Top banner: note this is the new page, link back to the classic page (fresh searches only,
    // where the trash files still exist), and invite feedback.  The old page also clears the
    // blatNewPage preference so later searches use the classic page until the user opts back in.
    var origPage = cfg.canOldPage ?
        ` You can go back to <a title="Show these results on the classic BLAT results page" ` +
        `href="hgBlat?blatNewPage=0&blatReopen=1&hgsid=${encodeURIComponent(cfg.hgsid)}">the original page</a> anytime.` : '';
    var bannerHtml =
        `<div class="blatBanner">We are testing a new BLAT output page.${origPage} ` +
        `If you have feedback on this new page, do not hesitate to let us know via ` +
        `<a href="mailto:genome@soe.ucsc.edu">genome@soe.ucsc.edu</a>.</div>`;

    var queryCount = new Set(hits.map(h => h.qName)).size;

    var th = [];
    th.push('<th class="num">#</th>');
    if (cfg.multiQuery) { th.push('<th>Query</th>'); }
    th.push('<th>Open in Genome Browser</th>');
    th.push('<th>Show</th>');
    th.push('<th>Query coverage</th>');
    if (cfg.hasLocus) { th.push('<th>Locus</th>'); }
    th.push('<th class="num">Score</th>');
    th.push('<th class="num">Identity</th>');
    th.push('<th>Strand</th>');
    th.push('<th class="num">Span</th>');

    // detail dock sits above the table: with long hit lists a bottom dock scrolls out of view
    document.getElementById('blatResults').innerHTML =
        bannerHtml +
        `<div class="blatCard">${blatSummaryStrip(cfg, queryCount)}` +
        `<div id="blatShareBox" class="blatShareBox" style="display:none"></div>` +
        `<div id="blatSeqBox" class="blatShareBox" style="display:none"></div>` +
        `<div id="blatDetail" class="blatDetail"></div>` +
        `<table id="blatTable" class="display" style="width:100%"><thead><tr>${th.join('')}</tr></thead></table></div>` +
        (cfg.canRename ? blatRenameModalHtml(cfg) : '');

    // Put the page actions in the gold main-header bar, to the right of the title (framework #sectTtl).
    var sectTtl = document.getElementById('sectTtl');
    if (sectTtl) {
        var acts = document.createElement('span');
        acts.className = 'blatHeadActions';
        acts.innerHTML = headActions;
        sectTtl.appendChild(acts);
    }

    $('#blatShareBtn').on('click', blatShareLink);
    $('#blatSeqBtn').on('click', blatShowQuerySeq);
    blatWireRename();

    var columns = [];
    columns.push({ data: 'rank', className: 'num rankCol' });
    if (cfg.multiQuery) { columns.push({ data: 'qName', className: 'queryCol' }); }
    columns.push({ data: null, orderable: false, className: 'blatPos',
        render: (d, type, row) => (type === 'display' ? blatPositionCell(row) : row.chrom + ':' + row.tStart) });
    columns.push({ data: null, orderable: false, className: 'actionsCol',
        render: (d, type, row) => (type === 'display' ? blatActionsCell(row) : '') });
    columns.push({ data: null, className: 'covCol', orderable: false,
        render: (d, type, row) => (type === 'display' ? blatCoverageCell(row) :
            (row.qEnd - row.qStart + 1)) });
    if (cfg.hasLocus) {
        columns.push({ data: 'locusText',
            render: (d, type, row) => (type === 'display' ? blatLocusCell(row) : (d || '')) });
    }
    // Score carries a bar scaled to the highest score in this result set (raw score kept for sorting).
    var maxScore = hits.reduce((m, h) => Math.max(m, h.score || 0), 0);
    columns.push({ data: 'score', className: 'num scoreCol',
        render: (d, type, row) => (type === 'display' ? blatScoreCell(row, maxScore) : d) });
    columns.push({ data: 'identity', className: 'num identCol',
        render: (d, type, row) => (type === 'display' ? blatIdentityCell(row) : d) });
    columns.push({ data: 'strand', className: 'strandCol' });
    columns.push({ data: 'span', className: 'num',
        render: (d, type, row) => (type === 'display' ? blatFmt(d) : d) });

    var dt = $('#blatTable').DataTable({
        data: hits,
        columns: columns,
        paging: false,
        info: false,
        order: [],
        language: { search: '', searchPlaceholder: 'Filter hits by locus, chrom, position…' }
    });

    $('#blatTable tbody').on('click', 'tr', function(ev) {
        if ($(ev.target).closest('a').length) { return; }   // let links work normally
        var d = dt.row(this).data();
        if (d) { blatSelect(dt, d.rank); }
    });

    // Keep the selected-row highlight after sort/filter.  Header tooltips are wired once below (the
    // <thead> persists across draws); we deliberately do NOT re-run convertTitleTagsToMouseovers on
    // every draw, as it re-scans the whole document and adds global listeners on each call.
    dt.on('draw', function() {
        if (blatSelectedRank !== null) { blatSelect(dt, blatSelectedRank); }
    });

    // No hit is pre-selected: several hits are often tied on score/identity, so picking one for the
    // user is misleading.  The detail panel shows a prompt until a row is clicked.
    document.getElementById('blatDetail').innerHTML =
        `<div class="blatSelectHint">Click a hit below to see its alignment details. ` +
        `If you are missing matches that you think should be there, ` +
        `<a target="_blank" href="../FAQ/FAQblat.html#blat1b">read our BLAT FAQ</a> or ` +
        `<a href="mailto:genome@soe.ucsc.edu">contact us</a>.</div>`;
    blatApplyTooltips();
}

$(document).ready(function() {
    if (typeof hgBlatData !== 'undefined' && document.getElementById('blatResults')) {
        blatBuild();
    }
});
