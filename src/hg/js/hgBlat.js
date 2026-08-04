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
/* global $, hgBlatData, convertTitleTagsToMouseovers */

var blatSelectedRank = null;   // rank of the row shown in the detail panel

function blatFmt(n) {
    // 12345 -> "12,345"
    return Number(n).toLocaleString('en-US');
}

function blatEsc(s) {
    // HTML-escape a value for safe insertion as text
    return $('<div>').text(s === null || s === undefined ? '' : String(s)).html();
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
    table.hgInside { background:#eef1f4; }
    #blatResults { --accent:#0a3a7a; --accentHover:#8b1a1a; --navy:#0a2b6b; --ink:#1e2833;
        --muted:#5b6572; --faint:#93a0ad; --line:#c4cdd6; --lineSoft:#e4e9ef; --card:#ffffff;
        --panel:#f4f7fb; --section:#4c7093; --headrow:#dbe4ee; --headrowLine:#a9bcd1;
        --sel:#cfe0f5; --hover:#eef3fb; --stripe:#f4f7fb;
        --title:#e9cf9a; --titleLine:#d9bd82; --titleSub:#5a4a24;
        --btn:#ffffff; --btnLine:#999999; --btnText:#0a2b6b; --btnHover:#eef2f7;
        --btnDark:#0a2b6b; --btnDarkHover:#0a2350;
        font-family:'Helvetica Neue',Helvetica,Arial,sans-serif; color:var(--ink); font-size:14px; }
    .blatCard { background:var(--card); border:1px solid var(--line); border-radius:3px; overflow:hidden;
        box-shadow:0 1px 2px rgba(20,40,70,.10); margin:12px 0 24px; }
    .blatHead { display:flex; align-items:center; justify-content:space-between; gap:12px; flex-wrap:wrap;
        padding:9px 18px; background:var(--title); border-bottom:1px solid var(--titleLine); }
    .blatHead .t { font-weight:700; font-size:16px; color:var(--navy); }
    .blatHead .t .sub { color:var(--titleSub); font-weight:400; }
    .blatHeadActions { display:flex; gap:8px; align-items:center; flex-wrap:wrap; }
    .blatStrip { display:flex; align-items:center; gap:24px; flex-wrap:wrap;
        padding:11px 18px; border-bottom:1px solid var(--line); background:var(--panel); }
    .blatStat { display:flex; flex-direction:column; gap:1px; }
    .blatStat .k { font-size:12px; color:var(--muted); font-weight:700; }
    .blatStat .v { font-size:14px; font-weight:700; color:var(--ink); }
    .blatDiv { width:1px; height:28px; background:var(--line); }
    .blatStripActions { margin-left:auto; display:flex; gap:8px; align-items:center; flex-wrap:wrap; }
    #blatResults .blatPill, #blatResults .blatStripActions input[type=submit],
    #blatResults .blatStripActions input[type=button] {
        font-family:inherit; font-size:13px; font-weight:700; color:var(--btnText); background:var(--btn);
        border:1px solid var(--btnLine); border-radius:3px; padding:5px 12px; cursor:pointer;
        text-decoration:none; display:inline-block; line-height:1.5; }
    #blatResults .blatPill:hover, #blatResults .blatStripActions input[type=submit]:hover,
    #blatResults .blatStripActions input[type=button]:hover { background:var(--btnHover); text-decoration:none; }
    #blatResults .blatPill.primary { background:var(--btnDark); color:#fff; border-color:var(--btnDark); }
    #blatResults .blatPill.primary:hover { background:var(--btnDarkHover); }
    .blatShareBox { display:flex; align-items:center; gap:10px; flex-wrap:wrap;
        padding:11px 18px; border-bottom:1px solid var(--line); background:var(--panel); }
    .blatShareMsg { font-size:14px; color:var(--muted); }
    .blatShareInput { flex:1; min-width:260px; font-size:13px; padding:5px 8px;
        border:1px solid var(--btnLine); border-radius:3px; background:#fff; }
    #blatTable { font-size:13px; width:100%; border-collapse:collapse; }
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
    #blatTable a { color:var(--accent); text-decoration:none; }
    #blatTable a:hover { color:var(--accentHover); text-decoration:underline; }
    .blatRowHint { padding:11px 18px 2px; font-size:13px; color:var(--muted); }
    .blatLocus { max-width:230px; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
    .chrNote { color:var(--faint); cursor:help; margin-left:4px; }
    .blatIdWrap { display:flex; align-items:center; gap:9px; justify-content:flex-end; }
    .blatIdBar { flex:0 0 54px; height:8px; background:var(--headrow); border:1px solid var(--headrowLine); overflow:hidden; }
    .blatIdBar > i { display:block; height:100%; }
    .blatIdPct { font-size:13px; font-weight:700; width:48px; text-align:right; font-variant-numeric:tabular-nums; }
    .blatCov { position:relative; display:block; width:150px; height:10px; background:var(--headrow); border:1px solid var(--headrowLine); }
    .blatCov > i { position:absolute; top:0; bottom:0; background:var(--accent); }
    #blatTable_filter { float:left; margin:0 0 10px; }
    #blatTable_filter input { width:300px; max-width:55vw; border:1px solid var(--btnLine);
        border-radius:3px; padding:5px 9px; font-size:14px; }
    #blatTable_wrapper { padding:6px 18px 4px; }
    .blatDetail { border-bottom:1px solid var(--line); background:var(--panel); padding:14px 18px 16px; }
    .blatDetail .dhead { display:flex; align-items:baseline; gap:10px; margin-bottom:12px; flex-wrap:wrap; }
    .blatDetail .dhead .lab { font-size:13px; color:var(--muted); }
    .blatDetail .dhead .loc { font-size:14px; font-weight:700; color:var(--ink); }
    .blatDetailCard { display:flex; gap:26px; flex-wrap:wrap; padding:14px 16px; background:var(--card);
        border:1px solid var(--line); border-radius:3px; }
    .blatTiles { display:grid; grid-template-columns:repeat(4,auto); gap:14px 26px; }
    .blatTile .k { font-size:13px; color:var(--muted); cursor:help; }
    .blatTile .v { font-size:14px; font-weight:700; color:var(--ink); }
    .blatDetailActions { display:flex; gap:8px; margin-top:14px; flex-wrap:wrap; }
    `;
    var st = document.createElement('style');
    st.id = 'blatStyle';
    st.textContent = css;
    document.head.appendChild(st);
}

// ---- cell renderers ------------------------------------------------------

function blatPositionCell(hit) {
    var note = hit.chromNote ?
        ` <span class="chrNote" title="${blatEsc(hit.chromNote)}">&#9432;</span>` : '';
    return `<a class="blatPos" href="${hit.browserUrl}">${blatEsc(hit.chrom)}:` +
        `${blatFmt(hit.tStart)}-${blatFmt(hit.tEnd)}</a>${note}`;
}

function blatActionsCell(hit) {
    // detailsUrl is the base-by-base alignment page (htcUserAli on a fresh search, htcBlatAlign on a
    // shared-link reopen); it is always present now, but guard in case a future caller omits it.
    var parts = [`<a href="${hit.browserUrl}">Browser</a>`,
        `<a target="_blank" href="${hit.newTabUrl}">New&nbsp;tab</a>`];
    if (hit.detailsUrl) { parts.push(`<a href="${hit.detailsUrl}">Alignment</a>`); }
    // a thin divider between the links makes the grouping clearer
    return parts.join('<span class="blatActSep">|</span>');
}

function blatLocusCell(hit, cfg) {
    if (!hit.locusText) { return ''; }
    var inner;
    if (hit.locusType && hit.locusGenes && hit.locusGenes.length) {
        var links = hit.locusGenes.map(function(g) {
            return `<a href="${cfg.geneUrlBase}${encodeURIComponent(g)}">${blatEsc(g)}</a>`;
        }).join('-');
        inner = `${blatEsc(hit.locusType)} ${links}`;
    } else {
        inner = blatEsc(hit.locusText);
    }
    return `<div class="blatLocus" title="${blatEsc(hit.locusText)}">${inner}</div>`;
}

function blatIdentityCell(hit) {
    var c = blatIdColor(hit.identity);
    return `<span class="blatIdWrap"><span class="blatIdBar">` +
        `<i style="width:${hit.identity}%;background:${c}"></i></span>` +
        `<span class="blatIdPct" style="color:${c}">${hit.identity.toFixed(1)}%</span></span>`;
}

function blatCoverageCell(hit) {
    var left = (hit.qStart - 1) / hit.qSize * 100;
    var width = (hit.qEnd - hit.qStart + 1) / hit.qSize * 100;
    var tip = `query ${blatFmt(hit.qStart)}–${blatFmt(hit.qEnd)} of ${blatFmt(hit.qSize)} bp`;
    return `<span class="blatCov" title="${tip}"><i style="left:${left.toFixed(1)}%;` +
        `width:${width.toFixed(1)}%"></i></span>`;
}

// ---- summary strip + detail panel ---------------------------------------

function blatSummaryStrip(cfg, queryCount) {
    var stat = (k, v) => `<div class="blatStat"><span class="k">${k}</span>` +
        `<span class="v">${v}</span></div>`;
    var div = '<span class="blatDiv"></span>';
    var assembly = stat('Assembly', blatEsc(cfg.organism) + ' / ' + blatEsc(cfg.db)) + div +
        stat('Hits', blatFmt(cfg.hitCount));
    var stats;
    if (cfg.multiQuery) {
        // With more than one query sequence a single query name/length would be wrong, so show the
        // number of distinct queries; each hit's own query is in the table's Query column.
        stats = stat('Queries', blatFmt(queryCount)) + div + assembly;
    } else {
        stats = stat('Query', blatEsc(cfg.queryName)) + div +
            stat('Length', blatFmt(cfg.querySize) + ' bp') + div + assembly;
    }
    var actions = '';
    // "Share a link" saves a durable anonymous session that reopens these results; it only works
    // when a stable custom track was made from them (cfg.canShare, i.e. autoBigPsl is on).
    if (cfg.canShare) {
        actions += '<button type="button" class="blatPill" id="blatShareBtn">Share a link</button>';
    }
    if (cfg.viewAllUrl) {
        actions += `<a class="blatPill" href="${cfg.viewAllUrl}">View all in browser</a>`;
    }
    // custom-track Rename/Delete buttons (emitted by hgBlat.c) get relocated here after render
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
    return `<div class="blatTile"><div class="k" title="${blatEsc(tip)}">${label}</div>` +
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
        `<a class="blatPill" id="dvBrowser" href="#">Open in browser</a>` +
        `<a class="blatPill" id="dvNewTab" target="_blank" href="#">Open in new tab</a></div></div>` +
        `<div id="dvAlignBox" style="flex:1;min-width:320px;border-left:1px solid var(--line);padding-left:24px;` +
        `display:flex;flex-direction:column;justify-content:center">` +
        `<div class="blatTile"><div class="k">Alignment</div></div>` +
        `<div id="dvAlign" style="font-size:14px;color:var(--muted);line-height:1.55;` +
        `margin:8px 0 14px;max-width:360px"></div>` +
        `<a class="blatPill" id="dvViewAlign" style="align-self:flex-start" href="#">` +
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
    var locus = hit.locusText ? blatEsc(hit.locusText) + ' · ' : '';
    var q = hgBlatData.config.multiQuery ? blatEsc(hit.qName) + ' · ' : '';
    blatSet('dvLoc', 'html',
        `#${hit.rank} · ${q}${locus}${blatEsc(hit.chrom)}:${blatFmt(hit.tStart)}-${blatFmt(hit.tEnd)}`);
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
    'Position': 'Genomic location of the match (1-based). Click to open the Genome Browser.',
    'Actions': 'Open the match in the browser, see the alignment details, or open in a new tab',
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
    // Save the current cart as an anonymous shared session (hgSession API), then build a link back
    // to this page that restores it.  The restored cart carries the durable bigPsl custom track
    // made from these results, which hgBlat rebuilds this table from (no BLAT re-run) on open.
    var cfg = hgBlatData.config;
    var box = document.getElementById('blatShareBox');
    box.style.display = 'flex';
    box.innerHTML = '<span class="blatShareMsg">Creating shareable link…</span>';
    // Save the cart as an anonymous shared session; same endpoint/params as topLinks.js "Share a link".
    $.ajax({
        type: 'POST',
        url: '../cgi-bin/hgSession',
        data: { hgsid: cfg.hgsid, 'hgS_doSaveSessionJson': 1, 'hgS_shareAnon': 1 },
        dataType: 'json',
        success: function(data) {
            if (!data || !data.name) {
                box.innerHTML = '<span class="blatShareMsg">Could not create link: ' +
                    blatEsc(data && data.error ? data.error : 'unknown error') + '</span>';
                return;
            }
            // anonymous sessions are saved under the reserved user "l"; hgBlat maps the short
            // u=/s= params to the session load and rebuilds this table from the durable bigPsl
            // custom track (no BLAT re-run)
            var link = window.location.origin + window.location.pathname +
                '?u=l&s=' + encodeURIComponent(data.name);
            box.innerHTML =
                '<span class="blatShareMsg">Shareable link (opens these results for anyone):</span>' +
                '<input id="blatShareInput" class="blatShareInput" type="text" readonly>' +
                '<button type="button" class="blatPill" id="blatShareCopy">Copy</button>';
            var inp = document.getElementById('blatShareInput');
            inp.value = link;
            inp.focus();
            inp.select();
            $('#blatShareCopy').on('click', function() {
                inp.select();
                if (navigator.clipboard) { navigator.clipboard.writeText(link); }
                else { document.execCommand('copy'); }
                document.getElementById('blatShareCopy').textContent = 'Copied';
            });
        },
        error: function() {
            box.innerHTML = '<span class="blatShareMsg">Could not reach the server. Please try again.</span>';
        }
    });
}

// ---- build ---------------------------------------------------------------

function blatBuild() {
    var cfg = hgBlatData.config;
    var hits = hgBlatData.hits;
    blatInjectStyle();

    var back = cfg.backUrl ?
        `<a class="blatPill" title="${blatEsc(cfg.backPos)}" href="${cfg.backUrl}">Back to browser</a>` : '';
    // "Old BLAT result page" re-renders the classic list from this session's fresh trash files and
    // clears the blatNewPage preference (blatNewPage=0), so future searches use the classic page
    // until the user opts back in.  Only offered on a fresh search (cfg.canOldPage), not a reopen.
    var oldPage = cfg.canOldPage ?
        `<a class="blatPill" title="Switch back to the classic BLAT results page" ` +
        `href="hgBlat?blatNewPage=0&blatReopen=1&hgsid=${encodeURIComponent(cfg.hgsid)}">Old BLAT result page</a>` : '';
    var headHtml =
        `<div class="blatHead"><span class="t">BLAT <span class="sub">results</span></span>` +
        `<span class="blatHeadActions">${oldPage}${back}` +
        `<a class="blatPill primary" href="${cfg.newSearchUrl}">New BLAT search</a></span></div>`;

    var queryCount = new Set(hits.map(h => h.qName)).size;

    var th = [];
    th.push('<th class="num">#</th>');
    if (cfg.multiQuery) { th.push('<th>Query</th>'); }
    th.push('<th>Position</th>');
    th.push('<th>Actions</th>');
    if (cfg.hasLocus) { th.push('<th>Locus</th>'); }
    th.push('<th class="num">Score</th>');
    th.push('<th class="num">Identity</th>');
    th.push('<th>Strand</th>');
    th.push('<th>Query coverage</th>');
    th.push('<th class="num">Span</th>');

    // detail dock sits above the table: with long hit lists a bottom dock scrolls out of view
    document.getElementById('blatResults').innerHTML =
        `<div class="blatCard">${headHtml}${blatSummaryStrip(cfg, queryCount)}` +
        `<div id="blatShareBox" class="blatShareBox" style="display:none"></div>` +
        `<div id="blatDetail" class="blatDetail"></div>` +
        `<div class="blatRowHint">Click any row to inspect it in the panel above.</div>` +
        `<table id="blatTable" class="display" style="width:100%"><thead><tr>${th.join('')}</tr></thead></table></div>`;

    // relocate the C-emitted custom-track buttons into the summary strip
    var actionBar = document.querySelector('.blatStripActions');
    ['renameFormItem', 'deleteCtForm'].forEach(function(id) {
        var el = document.getElementById(id);
        if (el && actionBar) { actionBar.appendChild(el); }
    });
    $('#blatShareBtn').on('click', blatShareLink);

    var columns = [];
    columns.push({ data: 'rank', className: 'num rankCol' });
    if (cfg.multiQuery) { columns.push({ data: 'qName', className: 'queryCol' }); }
    columns.push({ data: null, orderable: false, className: 'blatPos',
        render: (d, type, row) => (type === 'display' ? blatPositionCell(row) : row.chrom + ':' + row.tStart) });
    columns.push({ data: null, orderable: false, className: 'actionsCol',
        render: (d, type, row) => (type === 'display' ? blatActionsCell(row) : '') });
    if (cfg.hasLocus) {
        columns.push({ data: 'locusText',
            render: (d, type, row) => (type === 'display' ? blatLocusCell(row, cfg) : (d || '')) });
    }
    columns.push({ data: 'score', className: 'num' });
    columns.push({ data: 'identity', className: 'num',
        render: (d, type, row) => (type === 'display' ? blatIdentityCell(row) : d) });
    columns.push({ data: 'strand', className: 'strandCol' });
    columns.push({ data: null, className: 'covCol', orderable: false,
        render: (d, type, row) => (type === 'display' ? blatCoverageCell(row) :
            (row.qEnd - row.qStart + 1)) });
    columns.push({ data: 'span', className: 'num' });

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
        `<div class="dhead"><span class="lab">Select a hit below to see its alignment details.</span></div>`;
    blatApplyTooltips();
}

$(document).ready(function() {
    if (typeof hgBlatData !== 'undefined' && document.getElementById('blatResults')) {
        blatBuild();
    }
});
