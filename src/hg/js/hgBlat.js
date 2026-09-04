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
/* global $, hgBlatData, convertTitleTagsToMouseovers, htmlEncode, commify, gbShowTimingDialog */

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

// ---- cell renderers ------------------------------------------------------

function blatPositionCell(hit) {
    // For alt/fix/random/chrUn sequences show an info icon linking to the FAQ ("What is chr_alt &
    // chr_fix?"), with the short explanation as its tooltip.  (Sits after the position link, not
    // nested inside it.)
    // Drawn as the browser's own info-icon SVG rather than the &#9432; glyph it used to be: the
    // glyph is missing from some system fonts (it renders as a tofu box), and an SVG can take the
    // red that makes it stand out in the row (Lou, #38086 note-37).  currentColor lets .chrNote in
    // hgBlat.css own both the resting and the hover colour.
    var note = hit.chromNote ?
        ` <a class="chrNote" target="_blank" href="../FAQ/FAQblat.html#blat1c" ` +
        `title="${htmlEncode(hit.chromNote)} Click to learn more in the BLAT FAQ.">` +
        `${blatInfoSvg('currentColor')}</a>` : '';
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

function blatUnit() {
    // A protein query is measured in amino acids, everything else in bases.
    return hgBlatData.config.isProt ? 'aa' : 'bp';
}

function blatCoverageCell(hit) {
    var left = (hit.qStart - 1) / hit.qSize * 100;
    var width = (hit.qEnd - hit.qStart + 1) / hit.qSize * 100;
    var u = blatUnit();
    var tip = `Query matches the genome at ${blatFmt(hit.qStart)}-${blatFmt(hit.qEnd)}${u} out of ${blatFmt(hit.qSize)}${u}`;
    return `<span class="blatCov" title="${tip}"><i style="left:${left.toFixed(1)}%;` +
        `width:${width.toFixed(1)}%"></i></span>`;
}

// ---- summary strip + detail panel ---------------------------------------

function blatSummaryStrip(cfg, queryCount) {
    var stat = (k, v) => `<div class="gbStat"><span class="k">${k}</span>` +
        `<span class="v">${v}</span></div>`;
    var div = '<span class="gbDiv"></span>';
    var assembly = stat('Assembly', htmlEncode(cfg.organism) + ' / ' + htmlEncode(cfg.db)) + div +
        stat('Matches', blatFmt(cfg.hitCount));
    var stats;
    if (cfg.multiQuery) {
        // With more than one query sequence a single query name/length would be wrong, so show the
        // number of distinct queries; each hit's own query is in the table's Query column.
        stats = stat('Queries', blatFmt(queryCount)) + div + assembly;
    } else {
        stats = stat('Query', htmlEncode(cfg.queryName)) + div +
            stat('Length', blatFmt(cfg.querySize) + ' ' + blatUnit()) + div + assembly;
    }
    var actions = '';
    // "View all in browser" is the primary action, so it comes first.
    if (cfg.viewAllUrl) {
        actions += `<a class="gbPill" title="Open the Genome Browser with all these BLAT hits shown together as one custom track" href="${htmlEncode(cfg.viewAllUrl)}">View all in browser</a>`;
    }
    // "Show Query Sequence" opens the query FASTA in a panel (with Download / Copy). Only on a fresh
    // search, where the uploaded sequence is available (cfg.querySeqs emitted by hgBlat.c).
    if (cfg.querySeqs && cfg.querySeqs.length) {
        actions += '<button type="button" class="gbPill" id="blatSeqBtn" ' +
            'title="Show the sequence you searched with, in FASTA format">Show Query Sequence</button>';
    }
    // "Share a link" creates a durable, minimal snapshot session (db + results bigPsl only) and shows
    // its ?u=&s= reopen link (see blatShareLink).  Only offered when a durable bigPsl backs the
    // results (cfg.canShare = autoBigPsl); without it there is nothing for the shared link to reopen.
    if (cfg.canShare) {
        // A small share-nodes icon precedes the label so users learn to associate it with sharing.
        var shareIcon = '<svg class="blatShareIcon" viewBox="0 0 24 24" width="13" height="13" ' +
            'fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" ' +
            'stroke-linejoin="round" aria-hidden="true"><circle cx="18" cy="5" r="3"></circle>' +
            '<circle cx="6" cy="12" r="3"></circle><circle cx="18" cy="19" r="3"></circle>' +
            '<line x1="8.6" y1="10.5" x2="15.4" y2="6.5"></line>' +
            '<line x1="8.6" y1="13.5" x2="15.4" y2="17.5"></line></svg>';
        actions += '<button type="button" class="gbPill" id="blatShareBtn" ' +
            'title="Create a durable link that reopens these BLAT results">' +
            shareIcon + 'Share a link</button>';
    }
    // "Rename BLAT Track" opens a modal to rename the results custom track. This is a JS-native
    // button (renders immediately with the strip) that replaces the old C-emitted inline form, which
    // only appeared after the buildBigPsl AJAX finished and reflowed the page when clicked.
    if (cfg.canRename) {
        actions += '<button type="button" class="gbPill" id="blatRenameBtn" ' +
            'title="Rename this BLAT results custom track and its description">Rename BLAT Track</button>';
    }
    return `<div class="gbStrip">${stats}<span class="gbStripActions">${actions}</span></div>`;
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
        `<div class="blatDetailCard"><div class="blatDetailCol">` +
        `<div class="blatTiles">${tiles}</div>` +
        `<div class="blatDetailActions">` +
        `<a class="gbPill" id="dvBrowser" title="Open this hit in the Genome Browser" href="#">Open in browser</a>` +
        `<a class="gbPill" id="dvNewTab" target="_blank" title="Open this hit in the Genome Browser in a new tab" href="#">Open in new tab</a></div></div>` +
        `<div id="dvAlignBox" class="blatAlignBox">` +
        `<div class="blatTile"><div class="k">Alignment</div></div>` +
        `<div id="dvAlign" class="blatAlignText"></div>` +
        `<a class="gbPill" id="dvViewAlign" ` +
        `title="See the base-by-base alignment of your query against this hit" href="#">` +
        `View alignment</a></div></div>`;
    if (typeof convertTitleTagsToMouseovers === 'function') { convertTitleTagsToMouseovers(); }
}

function blatSet(id, prop, val) {
    var e = document.getElementById(id);
    if (!e) { return; }
    if (prop === 'text') { e.textContent = val; }
    else if (prop === 'href') { e.setAttribute('href', val); }
    else if (prop === 'color') { e.style.color = val; }
}

function blatRenderDetail(hit) {
    if (!hit || !document.getElementById('blatDetail')) { return; }
    if (!document.getElementById('dvScore')) { blatDetailSkeleton(); }
    var idc = blatIdColor(hit.identity);
    // Location line is plain text, so set it via textContent (blatSet 'text') - no HTML, nothing to
    // escape.  q and locus stay raw here for that reason.
    var locus = hit.locusText ? hit.locusText + ' · ' : '';
    var q = hgBlatData.config.multiQuery ? hit.qName + ' · ' : '';
    blatSet('dvLoc', 'text',
        `#${hit.rank} · ${q}${locus}${hit.chrom}:${blatFmt(hit.tStart)}-${blatFmt(hit.tEnd)}`);
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

// The snapshot link we created for this page view, cached so re-opening the box doesn't make another.
var blatShareCachedUrl = null;

// Render the share box.  url set -> show the link + Copy; url null -> "Creating link…"; msg (url null)
// -> show an error.
function blatShowShareBox(box, url, msg) {
    box.style.display = 'flex';
    if (msg) {
        box.innerHTML = '<span class="gbShareMsg gbShareFull" style="color:#a00">' +
            htmlEncode(msg) + '</span>';
        return;
    }
    if (!url) {
        box.innerHTML = '<span class="gbShareMsg gbShareFull">Creating link…</span>';
        return;
    }
    box.innerHTML =
        '<span class="gbShareMsg gbShareFull">Shareable link — anyone with it can reopen these ' +
        'BLAT results. It stores only the results (not your other tracks or settings) and stays ' +
        'active as long as it is used.</span>' +
        '<input id="gbShareInput" class="gbShareInput" type="text" readonly>' +
        '<button type="button" class="gbPill" id="blatShareCopy" title="Copy the link to the clipboard">Copy</button>';
    var inp = document.getElementById('gbShareInput');
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

function blatShareLink() {
    // Create (or reveal) a durable share link.  It is backed by a lightweight "snapshot" session that
    // stores only db + the results bigPsl - not the whole cart - under a server-generated unique name
    // (see lib/snapshotSession.c).  hgBlat's ?u=&s= reopen (doShareReopen) rebuilds the results table
    // from that bigPsl.  The token generation, uniqueness and cleanup are shared with hgc and the
    // top-right "Share a link".
    var box = document.getElementById('gbShareBox');
    if (!box) { return; }
    if (box.style.display === 'flex') { box.style.display = 'none'; return; }   // toggle off

    // Already viewing a shared session link: the current URL is itself the shareable link.
    if (/[?&]s=/.test(window.location.search)) { blatShowShareBox(box, window.location.href); return; }
    // Already created one this page view: reuse it rather than creating another session.
    if (blatShareCachedUrl) { blatShowShareBox(box, blatShareCachedUrl); return; }

    var cfg = hgBlatData.config;
    blatShowShareBox(box, null);   // "Creating link…"
    var body = 'hgsid=' + encodeURIComponent(cfg.hgsid || '') +
        '&hgS_doSaveSessionJson=1&hgS_shareAnon=1&hgS_snapshotType=blat';
    fetch('../cgi-bin/hgSession', {method: 'POST', credentials: 'same-origin',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: body})
        .then(function(r) { return r.json(); })
        .then(function(data) {
            if (!data || !data.name) {
                blatShowShareBox(box, null, (data && data.error) || 'Could not create the link.');
                return;
            }
            blatShareCachedUrl = window.location.origin + '/cgi-bin/hgBlat?u=l&s=' +
                encodeURIComponent(data.name);
            blatShowShareBox(box, blatShareCachedUrl);
        })
        .catch(function() {
            blatShowShareBox(box, null, 'Could not reach the server. Please try again.');
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
    return '<div id="gbModalBg" class="gbModalBg" style="display:none">' +
        '<div class="gbModal" role="dialog" aria-modal="true" aria-labelledby="gbModalTitle">' +
        '<div class="gbModalTitle" id="gbModalTitle">Rename BLAT Track</div>' +
        '<div class="gbModalText">Every BLAT result is stored in its own track in the Genome ' +
        'Browser. You can rename the track here. Results will disappear after 2–3 days, unless ' +
        `they are saved into a <a href="${sessionUrl}">Session link</a>.</div>` +
        '<label class="gbModalLabel" for="blatRenameName">Track name</label>' +
        '<input id="blatRenameName" class="gbModalInput" type="text" maxlength="80">' +
        '<label class="gbModalLabel" for="blatRenameDesc">Description</label>' +
        '<input id="blatRenameDesc" class="gbModalInput" type="text" maxlength="120">' +
        '<div class="gbModalBtns">' +
        '<button type="button" class="gbPill" id="blatRenameCancel">Cancel</button>' +
        '<button type="button" class="gbPill primary" id="blatRenameOk">OK</button>' +
        '</div></div></div>';
}

function blatCloseRename() {
    var bg = document.getElementById('gbModalBg');
    if (bg) { bg.style.display = 'none'; }
}

function blatOpenRename() {
    var bg = document.getElementById('gbModalBg');
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
    $('#gbModalBg').on('click', function(ev) {
        if (ev.target === this) { blatCloseRename(); }
    });
    $(document).on('keydown.blatRename', function(ev) {
        var bg = document.getElementById('gbModalBg');
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
        '<span class="gbShareMsg">Query sequence (FASTA):</span>' +
        '<button type="button" class="gbPill" id="blatSeqCopy" title="Copy the FASTA to the clipboard">Copy to Clipboard</button>' +
        '<button type="button" class="gbPill" id="blatSeqDownload" title="Download the FASTA as a .fa file">Download</button>' +
        '<button type="button" class="gbPill" id="blatSeqClose" title="Hide the query sequence">Close</button>' +
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
    // When loaded with &measureTiming=1 the C side attaches hgBlatData.timing; time the client
    // render too so the dialog shows the full server+client picture.
    var tBuildStart = (hgBlatData.timing && window.performance) ? performance.now() : 0;

    // Pin a stable, shareable URL into the address bar (no server redirect) so refresh, bookmark and
    // "Share a link" all use the trash-backed reopen link instead of the transient POST/search URL.
    if (cfg.shareUrl) {
        try { history.replaceState(null, '', cfg.shareUrl); } catch (e) { /* older browsers: ignore */ }
    }

    var back = cfg.backUrl ?
        `<a class="gbPill" title="Return to the Genome Browser at your previous location (${htmlEncode(cfg.backPos)})" ` +
        `href="${htmlEncode(cfg.backUrl)}">Back to Genome Browser</a>` : '';
    // The page actions live in the gold main-header bar (framework #sectTtl), next to the title -
    // so there is no separate toolbar (.blatHead is gone).  Injected into #sectTtl below.
    var headActions =
        `${back}<a class="gbPill primary" title="Start a new BLAT search" href="${htmlEncode(cfg.newSearchUrl)}">New BLAT search</a>`;

    // Top banner: note this is the new page, link back to the classic page (fresh searches only,
    // where the trash files still exist), and invite feedback.  The old page also clears the
    // blatNewPage preference so later searches use the classic page until the user opts back in.
    var origPage = cfg.canOldPage ?
        ` You can go back to <a title="Show these results on the classic BLAT results page" ` +
        `href="hgBlat?blatNewPage=0&blatReopen=1&hgsid=${encodeURIComponent(cfg.hgsid)}">the original page</a> anytime.` : '';
    var bannerHtml =
        `<div class="gbBanner">We are testing a new BLAT output page.${origPage} ` +
        `If you have feedback on this new page, do not hesitate to let us know via ` +
        `<a href="mailto:genome@soe.ucsc.edu">genome@soe.ucsc.edu</a>.</div>`;

    var queryCount = new Set(hits.map(h => h.qName)).size;

    var th = [];
    th.push('<th>#</th>');
    if (cfg.multiQuery) { th.push('<th>Query</th>'); }
    th.push('<th>Open in Genome Browser</th>');
    th.push('<th>Show</th>');
    th.push('<th>Query coverage</th>');
    if (cfg.hasLocus) { th.push('<th>Locus</th>'); }
    th.push('<th>Score</th>');
    th.push('<th>Identity</th>');
    th.push('<th>Strand</th>');
    th.push('<th>Span</th>');

    // detail dock sits above the table: with long hit lists a bottom dock scrolls out of view
    document.getElementById('blatResults').innerHTML =
        bannerHtml +
        `<div class="gbCard">${blatSummaryStrip(cfg, queryCount)}` +
        `<div id="gbShareBox" class="gbShareBox" style="display:none"></div>` +
        `<div id="blatSeqBox" class="gbShareBox" style="display:none"></div>` +
        `<div id="blatDetail" class="blatDetail"></div>` +
        `<table id="blatTable" class="display"><thead><tr>${th.join('')}</tr></thead></table></div>` +
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
    columns.push({ data: 'rank', className: 'rankCol' });
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
    columns.push({ data: 'score', className: 'scoreCol',
        render: (d, type, row) => (type === 'display' ? blatScoreCell(row, maxScore) : d) });
    columns.push({ data: 'identity', className: 'identCol',
        render: (d, type, row) => (type === 'display' ? blatIdentityCell(row) : d) });
    columns.push({ data: 'strand', className: 'strandCol' });
    columns.push({ data: 'span', className: 'spanCol',
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

    // Timing report (only when loaded with &measureTiming=1): a pill in the summary strip that opens
    // the shared dialog with the server phases plus the client render time.
    if (hgBlatData.timing) {
        var clientRows = [{ label: 'build page (JS)',
                            ms: Math.round(performance.now() - tBuildStart) }];
        var pill = document.createElement('button');
        pill.type = 'button';
        pill.className = 'gbPill';
        pill.id = 'blatTimingBtn';
        pill.innerHTML = '&#9201; Timing';
        pill.title = 'Show where this page spent its time (server and browser)';
        pill.addEventListener('click', function() {
            gbShowTimingDialog(hgBlatData.timing, clientRows);
        });
        var strip = document.querySelector('#blatResults .gbStripActions') ||
                    document.querySelector('#blatResults .gbStrip');
        if (strip) { strip.appendChild(pill); }
        // measureTiming=1 on the URL is an explicit request to see the numbers, so open the dialog
        // right away; the pill stays for reopening it after Close.
        gbShowTimingDialog(hgBlatData.timing, clientRows);
    }

    blatApplyTooltips();
}

// ==== search form (the input page) ========================================
// hgBlat.c emits  var hgBlatFormData = {...}  together with a real <form name="mainForm"> that
// contains an empty <div id="blatFormBox"> and the C-generated genome search bar.  We build the
// controls as real form fields *inside that form*, so the browser serializes them natively -
// including the file input - and Submit / I'm feeling lucky / Clear stay plain submit buttons
// handled by the existing C code.  There is no shadow form and no copying of values on submit.
// Styling comes from hgBlat.css (loaded by webIncludeResourceFile in hgBlat.c), shared with the results page.

// The Genome Browser's standard info icon, copied from printInfoIconSvg() in hg/lib/hui.c so the
// page's icons are pixel-identical to the C-rendered ones elsewhere in the browser.  The stroke
// colour is a parameter because the results table wants a red one (see blatPositionCell); pass
// 'currentColor' to let CSS drive it.
function blatInfoSvg(stroke) {
    return "<svg style='height:1.1em; vertical-align:top' viewBox='0 0 24 24' fill='none' " +
        "xmlns='http://www.w3.org/2000/svg'>" +
        "<circle cx='12' cy='12' r='10' stroke='" + stroke + "' stroke-width='1.5'/>" +
        "<path d='M12 17V11' stroke='" + stroke + "' stroke-width='1.5' stroke-linecap='round'/>" +
        "<circle cx='1' cy='1' r='1' transform='matrix(1 0 0 -1 11 9)' fill='" + stroke +
        "'/></svg>";
}
var BLAT_INFO_SVG = blatInfoSvg('#1C274C');

// The assembly-search syntax help.  setupGenomeSelector hides the info icon that
// printGenomeSearchBar (hg/lib/web.c) normally puts next to the box, so the new form loses that
// explanation of +word/-word/word*/"phrase"; we re-attach it to an icon after the label instead.
// Kept word-for-word in sync with searchHelpText in web.c so both pickers explain the box the same
// way.  This is HTML (a bullet list), rendered as such by the mouseover, so it is NOT htmlEncode'd -
// like the C printInfoIcon, it relies on the string containing no double quotes to sit in a title=.
var BLAT_GENOME_SEARCH_HELP =
    "All genome searches are case-insensitive.  Single-word searches default to prefix " +
    "matching if an exact match is not found. " +
    "<ul id='searchTipList' class='noBullets'>" +
    "<li> Force inclusion: Use a + sign before <b>+word</b> to ensure it appears in result.</li>" +
    "<li> Exclude words: Use a - sign before <b>-word</b> to exclude it from the search result.</li>" +
    "<li> Wildcard search: Add an * (asterisk) at end of <b>word*</b> to search for all terms starting with that prefix.</li>" +
    "<li> Phrase search: Enclose 'words in quotes' to search for the exact phrase.</li>" +
    "</ul>";

// Cross-session memory of the "Keep results" checkbox.  A plain '1'/'0' string under one key;
// wrapped in try/catch because localStorage throws in private-mode / disabled-storage browsers, in
// which case we simply fall back to the cart-supplied default and skip persistence.
var BLAT_KEEP_RESULTS_KEY = 'blatKeepResults';
var BLAT_ONLY_LATEST_KEY = 'blatOnlyLatest';

function blatGetKeepResultsPref() {
    // Returns true/false for a stored preference, or null if the user has never set one here.
    try {
        var v = localStorage.getItem(BLAT_KEEP_RESULTS_KEY);
        return v === null ? null : (v === '1');
    } catch (e) { return null; }
}

function blatSetKeepResultsPref(on) {
    try { localStorage.setItem(BLAT_KEEP_RESULTS_KEY, on ? '1' : '0'); } catch (e) { /* ignore */ }
}

function blatGetOnlyLatestPref() {
    // Returns true/false for a stored preference, or null if the user has never set one here.
    try {
        var v = localStorage.getItem(BLAT_ONLY_LATEST_KEY);
        return v === null ? null : (v === '1');
    } catch (e) { return null; }
}

function blatSetOnlyLatestPref(on) {
    try { localStorage.setItem(BLAT_ONLY_LATEST_KEY, on ? '1' : '0'); } catch (e) { /* ignore */ }
}

function blatOpts(list, cur) {
    return list.map(function(v) {
        return `<option value="${htmlEncode(v)}"${v === cur ? ' selected' : ''}>${htmlEncode(v)}</option>`;
    }).join('');
}

function blatFormCount() {
    // Live character count under the textarea.  Only these two nodes are touched on input - the
    // textarea itself is never re-rendered, so the caret stays where the user put it.
    var ta = document.getElementById('blatUserSeq');
    var out = document.getElementById('blatCountText');
    if (!ta || !out) { return; }
    var n = ta.value.replace(/[^A-Za-z*]/g, '').length;
    out.textContent = blatFmt(n) + ' of 25,000 characters';
    $('#blatLimitLink').toggleClass('over', n > 25000);
}

function blatFormTab(showUpload) {
    $('#blatTabPaste').toggleClass('on', !showUpload);
    $('#blatTabUpload').toggleClass('on', showUpload);
    $('#blatPanePaste').toggle(!showUpload);
    $('#blatPaneUpload').toggle(showUpload);
}

function blatFormLimitsModal() {
    var row = (k, v) => `<div class="blatLimitRow"><span>${k}</span><strong>${v}</strong></div>`;
    return '<div id="blatLimitsBg" class="gbModalBg" style="display:none">' +
        '<div class="gbModal" role="dialog" aria-modal="true" aria-labelledby="blatLimitsTitle">' +
        '<div class="gbModalTitle" id="blatLimitsTitle">Input limits</div>' +
        row('DNA per sequence', '25,000 bases') +
        row('Protein / translated', '10,000 letters') +
        row('Sequences per run', '25') +
        row('Total per submission', '50,000 bases') +
        '<div class="gbModalText gbModalNote">Queries above these limits are rejected ' +
        'before alignment. For larger jobs, run BLAT from the ' +
        '<a target="_blank" href="https://hgdownload.soe.ucsc.edu/downloads.html#utilities_downloads">' +
        'command line</a> on your own server.</div>' +
        '<div class="gbModalBtns"><button type="button" class="gbPill" id="blatLimitsClose">Close</button></div>' +
        '</div></div>';
}

function blatFormBusyMarkup() {
    // Spinner overlay shown between Submit and the arrival of the results page.  Built with the
    // form (hidden) rather than on demand, so nothing has to be parsed or fetched at the moment the
    // browser is already busy navigating away.
    return '<div id="blatBusyBg" class="gbBusyBg" role="status" aria-live="polite">' +
        '<div class="gbBusyCard"><div class="gbSpinner"></div>' +
        '<div><div class="gbBusyTitle" id="blatBusyTitle">Running BLAT&hellip;</div>' +
        '<div class="gbBusyText" id="blatBusyText"></div></div></div></div>';
}

function blatFormBusyWire(cfg) {
    // The search is a plain form POST that navigates the page, so between the click on Submit and
    // the arrival of the results nothing on screen changes at all - and a BLAT run is anywhere from
    // under a second to well over half a minute, the long end being an assembly served by a dynamic
    // BLAT server, which has to load its index from disk before it can answer.  Cover the form with
    // a spinner for that interval.  Nothing here hides it again: the only thing that ends the wait
    // is the results page replacing this one.  The exception is a Back navigation, where the browser
    // may restore this page from its cache with the overlay still up - see the pageshow handler.
    var form = document.mainForm;
    if (!form) { return; }
    var showTimer = null;
    var slowTimer = null;

    // Which button was used.  SubmitEvent.submitter is missing in older Safari, so also remember
    // the last submit button the user activated and fall back to that.
    var lastBtn = null;
    $('#blatFormBox input[type=submit]').on('click', function() { lastBtn = this; });

    $(form).on('submit', function(ev) {
        var btn = (ev.originalEvent && ev.originalEvent.submitter) || lastBtn;
        // Clear just empties the textarea and comes straight back; no spinner for that.
        if (btn && btn.name === 'Clear') { return; }
        // Nothing to align - hgBlat returns its "please paste a sequence" page immediately, so a
        // spinner would only flash.
        var ta = document.getElementById('blatUserSeq');
        var file = document.getElementById('blatSeqFile');
        if (!(ta && ta.value.trim()) && !(file && file.files && file.files.length)) { return; }

        var allGenomes = $('#blat_allGenomes').prop('checked');
        // The genome can be changed without reloading the page, so take the label from the search
        // bar (which setupGenomeSearchBar keeps current) rather than from the page-load config.
        var genomeInput = document.getElementById('genomeSearch');
        var genome = (genomeInput && genomeInput.value) || cfg.dbLabel || 'the selected assembly';
        document.getElementById('blatBusyTitle').innerHTML =
            allGenomes ? 'Searching all genomes&hellip;' : 'Running BLAT&hellip;';
        document.getElementById('blatBusyText').textContent = allGenomes ?
            'Aligning your sequence against every assembly that has its own BLAT server.' :
            'Aligning your sequence against ' + genome + '.';
        // Held back a moment: a hg38 DNA search of this size comes back in well under a second, and
        // a scrim that appears and vanishes again inside that time reads as a glitch rather than as
        // progress.  Timers keep running on a page whose navigation is in flight, so the overlay
        // still appears for every search slow enough to be worth reporting.
        showTimer = setTimeout(function() { $('#blatBusyBg').addClass('on'); }, 350);

        // A search that is still going after this long is almost always one where the assembly's
        // BLAT index is being loaded on demand.  Say so, rather than leaving the user guessing
        // whether anything is still happening.
        slowTimer = setTimeout(function() {
            document.getElementById('blatBusyText').textContent =
                'Still working. Assemblies whose BLAT index is loaded on demand are slow to ' +
                'answer the first search.';
        }, 8000);
    });

    // Back button: browsers that restore this page from the back/forward cache restore it exactly as
    // it was left, spinner and all.  Take it down and cancel the pending "still working" message.
    $(window).on('pageshow', function() {
        $('#blatBusyBg').removeClass('on');
        if (showTimer) { clearTimeout(showTimer); showTimer = null; }
        if (slowTimer) { clearTimeout(slowTimer); slowTimer = null; }
    });
}

function blatFormSetDb(db) {
    // Called by hgBlat.c's setupGenomeSearchBar onSelect.  Picking a genome does not reload the
    // page, so everything on it that depends on db is updated here instead: the hidden field that
    // the search is submitted with, and the sidebar links that carry a db= parameter.  The current
    // assembly label is updated by setupGenomeSearchBar itself.
    document.mainForm.db.value = db;
    $('#blatFormBox a[data-urltpl]').each(function() {
        this.href = this.getAttribute('data-urltpl').replace('$DB$', encodeURIComponent(db));
    });
}

function blatFormSidebar(cfg) {
    // Same links the classic page offered.  hgBlat.c supplies them as templates holding $DB$ (see
    // blatFormSetDb); the template is kept in data-urltpl so the link can be retargeted later.
    var tools = '';
    var tplLink = (tpl, label) => {
        var href = tpl.replace('$DB$', encodeURIComponent(hgBlatFormData.db));
        return `<a data-urltpl="${htmlEncode(tpl)}" href="${htmlEncode(href)}">${label}</a>`;
    };
    if (cfg.pcrUrlTpl) {
        tools += `<div>${tplLink(cfg.pcrUrlTpl, 'In-Silico PCR')} — better than BLAT for ` +
            'locating PCR primers.</div>';
    }
    if (cfg.oligoMatchUrlTpl) {
        tools += `<div>${tplLink(cfg.oligoMatchUrlTpl, 'Short Sequence Match')} — for ` +
            'sequences under 20 bp, within the region shown in the Genome Browser.</div>';
    }
    tools += '<div><a target="_blank" href="https://hgdownload.soe.ucsc.edu/downloads.html#utilities_downloads">' +
        'findMotifs</a> — command-line search across a whole genome.</div>';
    return '<div>' +
        (tools ? `<div class="gbCard"><h3>Similar tools</h3>${tools}</div>` : '') +
        '<div class="gbCard"><h3>Help</h3>' +
        '<div><a href="../FAQ/FAQblat.html">BLAT FAQ</a></div>' +
        '<div><a href="../goldenPath/help/hgTracksHelp.html#BLATAlign">BLAT documentation</a></div>' +
        // No "Search all genomes FAQ" here: that link now lives in the "Search many genomes"
        // tooltip, next to the checkbox it actually explains.
        '<div><a href="../FAQ/FAQblat.html#blat14">Programmatic / batch BLAT</a></div>' +
        '</div>' +
        '<div class="gbCard"><h3>About BLAT</h3>' +
        '<div>DNA BLAT quickly finds sequences of 95% and greater similarity that are at least 25 bases ' +
        'long; it finds perfect matches down to 20 bases, and may miss shorter or more divergent ' +
        'alignments. Protein BLAT finds sequences of 80% and greater similarity at least 20 amino acids ' +
        'long.</div>' +
        '<div>Kent WJ. <a target="_blank" href="https://genome.cshlp.org/content/12/4/656.abstract">' +
        'BLAT — the BLAST-like alignment tool</a>. Genome Res. 2002 Apr;12(4):656-64.</div>' +
        '</div></div>';
}

function blatFormBuild() {
    var cfg = hgBlatFormData;

    var banner = '';
    if (cfg.classicUrl) {
        banner = '<div class="gbBanner">We are testing a new BLAT search page. You can go back to ' +
            `<a href="${htmlEncode(cfg.classicUrl)}">the original page</a> anytime. If you have feedback ` +
            'on this new page, do not hesitate to let us know via ' +
            '<a href="mailto:genome@soe.ucsc.edu">genome@soe.ucsc.edu</a>.</div>';
    }

    // Checkbox plus the browser's standard info icon.  Same SVG and same title +
    // convertTitleTagsToMouseovers mechanism as printInfoIcon()/printInfoIconSvg() in hg/lib/hui.c,
    // so these read identically to the info icons on every other Genome Browser page.
    var check = (name, on, label, tip) =>
        `<span class="blatCheck"><label><input type="checkbox" name="${name}" ` +
        `id="blat_${name}"${on ? ' checked' : ''}>${label}</label>` +
        `<span class="blatInfo" title="${htmlEncode(tip)}">${BLAT_INFO_SVG}</span></span>`;

    // "Keep results" starting state.  The cart (cfg.keepResults) only remembers the choice within a
    // session; localStorage carries it across sessions so a user who wants their BLAT results to
    // accumulate does not have to re-tick the box on every visit.  localStorage wins when set (it is
    // the more durable record of the user's own preference); the cart is the fallback for a browser
    // that has never stored one.  Only consulted where the box is actually shown (blatOldTracks=
    // delete); elsewhere the choice has no effect, so there is nothing worth persisting.
    var keepResultsInit = cfg.keepResults;
    if (cfg.showKeepResults) {
        var storedKeep = blatGetKeepResultsPref();
        if (storedKeep !== null) { keepResultsInit = storedKeep; }
    }
    var onlyLatestInit = cfg.onlyLatest;
    if (cfg.showOnlyLatest) {
        var storedLatest = blatGetOnlyLatestPref();
        if (storedLatest !== null) { onlyLatestInit = storedLatest; }
    }

    document.getElementById('blatFormBox').innerHTML =
        banner +
        '<div class="blatFormGrid"><div>' +

        '<div class="gbSection">Search &ndash; type keywords to find the target assembly</div>' +
        '<div class="blatRow">' +
            '<div class="blatField blatGenomeSlot"><span>Genome or assembly ' +
                `<span class="blatInfo" title="${BLAT_GENOME_SEARCH_HELP}">${BLAT_INFO_SVG}</span>` +
                '</span>' +
                '<div id="blatGenomeSlot"></div></div>' +
            `<label class="blatField"><span>Query type</span><select name="type">${blatOpts(cfg.types, cfg.type)}</select></label>` +
            // Sort and output are submitted but not offered: sorting by anything other than score
            // is rarely useful, and this page always wants the hyperlink (results table) output.
            // Kept as hidden fields so the request hgBlat receives is unchanged.
            `<input type="hidden" name="sort" value="${htmlEncode(cfg.sort)}">` +
            `<input type="hidden" name="output" value="${htmlEncode(cfg.output)}">` +
        '</div>' +

        '<div class="blatChecks">' +
            // The mouseover popup keeps itself open while the pointer is inside it (see the
            // mouseoverContainer mouseenter handler in utils.js), and renders its text as HTML, so
            // a link in the tip is genuinely clickable.  htmlEncode keeps the title attribute
            // well-formed; the browser decodes it back to markup before it is injected.
            // Only offered where hg.conf blatOldTracks=delete, i.e. where there is something to opt
            // out of.  Unlike the three below (which keep the classic form's plain-checkbox
            // behaviour), this one is submitted through an explicit hidden field: a checkbox sends
            // nothing when unticked, so cartUsualBoolean would never see it go back to false and
            // "Keep results" could not be switched off again once used.
            (cfg.showKeepResults ?
                '<span class="blatCheck">' +
                `<input type="hidden" name="blatKeepResults" id="blatKeepResultsVal" value="${keepResultsInit ? 1 : 0}">` +
                `<label><input type="checkbox" id="blat_keepResults"${keepResultsInit ? ' checked' : ''}>` +
                'Keep results</label>' +
                `<span class="blatInfo" title="${htmlEncode(
                    'A new BLAT search always overrides your previous BLAT results: each search ' +
                    'replaces the result track of the one before it in the Genome Browser. Check ' +
                    'this box to keep earlier results instead, so every search adds its own track ' +
                    'and results accumulate. Your choice is remembered for next time.')}">` +
                `${BLAT_INFO_SVG}</span></span>` : '') +
            check('autoRearr', cfg.autoRearr, 'Show rearrangements',
                'Shows duplications of the query sequence using multiple lines with connecting lines ' +
                'between fragments, and displays inversions better (the "snakes" display). Can also ' +
                'be switched on or off from the BLAT track configuration page.') +
            check('allResults', cfg.allResults, 'No min. score',
                'Turns off minimum-match filtering so every alignment is returned. A human DNA search ' +
                'normally requires 20 matching bases, based on the genome size, to filter out ' +
                'lower-quality results; useful for short queries and the tiny genomes of ' +
                'microorganisms.') +
            check('allGenomes', cfg.allGenomes, 'Search many genomes',
                'Runs the same query against every default assembly and attached hub that has a ' +
                'dedicated BLAT server. Dynamic BLAT servers are skipped and listed as such in the ' +
                "output. See our <a target='_blank' href='../FAQ/FAQblat.html#blat9'>BLAT All FAQ</a> " +
                'for more information.') +
            // "Keep only last search" (RM #38086): submitted through a hidden field for the same
            // reason as "Keep results" above: an unchecked box sends nothing, so the cart could
            // never see it switched back off.
            (cfg.showOnlyLatest ?
                '<span class="blatCheck">' +
                `<input type="hidden" name="blatOnlyLatest" id="blatOnlyLatestVal" value="${onlyLatestInit ? 1 : 0}">` +
                `<label><input type="checkbox" id="blat_onlyLatest"${onlyLatestInit ? ' checked' : ''}>` +
                'Keep only last search</label>' +
                `<span class="blatInfo" title="${htmlEncode(
                    'Each new BLAT search removes your earlier BLAT result tracks, so only the ' +
                    'newest search shows in the Genome Browser. Leave unchecked to keep all ' +
                    'results. Your choice is remembered for next time.')}">` +
                `${BLAT_INFO_SVG}</span></span>` : '') +
        '</div>' +

        '<div class="gbSection">Query sequence</div>' +
        '<div class="blatTabs">' +
            '<button type="button" class="blatTab on" id="blatTabPaste">Paste sequence</button>' +
            '<button type="button" class="blatTab" id="blatTabUpload">Upload file</button>' +
        '</div>' +

        '<div id="blatPanePaste">' +
            '<div class="blatPaneHint"><span>Separate multiple sequences with a &gt;name line. ' +
            'Up to 25 sequences.</span>' +
            `<a href="#" id="blatExample">${htmlEncode(cfg.exampleLabel)}</a></div>` +
            '<textarea class="blatSeq" name="userSeq" id="blatUserSeq" spellcheck="false" ' +
            'aria-label="Paste in a query sequence"></textarea>' +
            '<div class="blatCount"><span id="blatCountText"></span>' +
            '<a href="#" id="blatLimitLink">Show input limits</a></div>' +
        '</div>' +

        '<div id="blatPaneUpload" style="display:none">' +
            '<div class="blatDrop" id="blatDrop">' +
                '<div class="blatDropTitle">Drop a sequence file here</div>' +
                '<div class="blatDropSub">Plain text or FASTA, up to 50,000 bases total</div>' +
                '<input type="file" name="seqFile" id="blatSeqFile">' +
                '<div class="blatFileName" id="blatFileName"></div>' +
            '</div>' +
        '</div>' +

        '<div class="blatActions">' +
            '<input type="submit" class="gbPill primary" name="Submit" value="Submit" ' +
            `title="${htmlEncode('Align the sequence and show all matches')}">` +
            '<input type="submit" class="gbPill" name="Lucky" value="I&#39;m feeling lucky" ' +
            `title="${htmlEncode('Skip the list of matches and open the best-scoring one straight ' +
                'away in the Genome Browser. Ignored when "Search many genomes" is ticked.')}">` +
            '<input type="submit" class="gbPill" name="Clear" value="Clear" ' +
            `title="${htmlEncode('Empty the query sequence box')}">` +
        '</div>' +

        '</div>' + blatFormSidebar(cfg) + '</div>' +
        blatFormLimitsModal() +
        blatFormBusyMarkup();

    // Move the C-generated genome search bar (real autocomplete over every assembly, already wired
    // by setupGenomeSearchBar) into its slot, rather than reimplementing it with a hardcoded list.
    var holder = document.getElementById('blatGenomeHolder');
    if (holder) { document.getElementById('blatGenomeSlot').appendChild(holder); }


    // Show the current assembly in the search bar itself instead of in a separate "Current genome:"
    // line - the bar is wide enough for the whole description.  setupGenomeSearchBar writes the new
    // one in on each pick, and focusing the bar selects all of it, so it reads as a filled-in search
    // box rather than as a value the user has to clear by hand.
    var genomeInput = document.getElementById('genomeSearch');
    if (genomeInput && cfg.dbLabel) { genomeInput.value = cfg.dbLabel; }

    // Restore the sequence from the cart without going through innerHTML (avoids re-escaping).
    document.getElementById('blatUserSeq').value = cfg.userSeq || '';
    blatFormCount();

    $('#blatUserSeq').on('input', blatFormCount);
    // Mirror the "Keep results" checkbox into its hidden field so an unticked box submits an
    // explicit 0 rather than nothing at all, and remember the choice in localStorage so it comes
    // back pre-set on the user's next visit (see keepResultsInit above).
    $('#blat_keepResults').on('change', function() {
        document.getElementById('blatKeepResultsVal').value = this.checked ? '1' : '0';
        blatSetKeepResultsPref(this.checked);
    });
    $('#blat_onlyLatest').on('change', function() {
        document.getElementById('blatOnlyLatestVal').value = this.checked ? '1' : '0';
        blatSetOnlyLatestPref(this.checked);
    });
    $('#blatTabPaste').on('click', function() { blatFormTab(false); });
    $('#blatTabUpload').on('click', function() { blatFormTab(true); });
    // The example sequence is a real 2.5 kb query, fetched on demand so it is not carried in every
    // page load.  The link doubles as its own status indicator while the request is in flight.
    $('#blatExample').on('click', function(ev) {
        ev.preventDefault();
        var link = this;
        var label = cfg.exampleLabel;
        link.textContent = 'Loading example…';
        fetch(cfg.exampleUrl)
            .then(function(resp) {
                if (!resp.ok) { throw new Error('HTTP ' + resp.status); }
                return resp.text();
            })
            .then(function(fa) {
                var ta = document.getElementById('blatUserSeq');
                ta.value = fa.trim();
                blatFormCount();
                ta.focus();
                ta.setSelectionRange(0, 0);
                ta.scrollTop = 0;
                link.textContent = label;
                blatFormTab(false);   // in case the user was on the upload tab
            })
            .catch(function(err) {
                link.textContent = 'Could not load example';
                // Leave the message up briefly, then let the user try again.
                setTimeout(function() { link.textContent = label; }, 4000);
                console.error('hgBlat: example fetch failed:', err);
            });
    });
    $('#blatLimitLink').on('click', function(ev) {
        ev.preventDefault();
        $('#blatLimitsBg').css('display', 'flex');
    });
    $('#blatLimitsClose').on('click', function() { $('#blatLimitsBg').hide(); });
    $('#blatLimitsBg').on('click', function(ev) { if (ev.target === this) { $(this).hide(); } });
    $(document).on('keydown.blatLimits', function(ev) {
        if (ev.key === 'Escape') { $('#blatLimitsBg').hide(); }
    });

    var fileInput = document.getElementById('blatSeqFile');
    var drop = document.getElementById('blatDrop');
    $(fileInput).on('change', function() {
        document.getElementById('blatFileName').textContent =
            this.files && this.files.length ? this.files[0].name : '';
    });
    ['dragenter', 'dragover'].forEach(function(e) {
        drop.addEventListener(e, function(ev) { ev.preventDefault(); drop.classList.add('hot'); });
    });
    ['dragleave', 'drop'].forEach(function(e) {
        drop.addEventListener(e, function(ev) { ev.preventDefault(); drop.classList.remove('hot'); });
    });
    drop.addEventListener('drop', function(ev) {
        if (ev.dataTransfer.files.length) {
            fileInput.files = ev.dataTransfer.files;
            $(fileInput).trigger('change');
        }
    });

    blatFormBusyWire(cfg);

    if (typeof convertTitleTagsToMouseovers === 'function') { convertTitleTagsToMouseovers(); }
}

$(document).ready(function() {
    if (typeof hgBlatData !== 'undefined' && document.getElementById('blatResults')) {
        blatBuild();
    }
    if (typeof hgBlatFormData !== 'undefined' && document.getElementById('blatFormBox')) {
        blatFormBuild();
    }
});
