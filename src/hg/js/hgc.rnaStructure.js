// hgc.rnaStructure.js - ES6 module for drawing an RNA secondary structure on hgc details pages.
// Loaded via dynamic import() from the detailsScript trackDb mechanism.
//
// bedDetails.scripts.rnaStructure is an array of one entry per detailsScript setting:
//   {field, value, title, colors, labels, exportFields, fieldValues}
// value is the structure in dot-bracket notation. fieldValues carries the bigBed
// fields named in exportFields:
//   sequence      the RNA sequence, same length as the structure
//   armPositions  optional, comma-separated "start-end:type" ranges, 1-based in the
//                 sequence, e.g. "31-52:5p*,67-89:3p". They are colored by type.
// colors maps a type to a color ("#D55E00"), labels maps a type to its legend text.
// A type without a color is not highlighted.
//
// The structure is drawn with fornac (ViennaRNA, Apache-2.0), which needs d3 v3 as a
// global. Both are vendored in js/external and installed next to this module, and are
// loaded with plain script tags the first time a structure is drawn.

const D3_JS = "d3-3.5.17.min.js";
const FORNAC_JS = "fornac-1.2.0.min.js";
const FORNAC_CSS = "fornac-1.2.0.css";
const HIGHLIGHT_ALPHA = 0.55;   // letters stay readable on the colored circles
const VIEW_W = 480, VIEW_H = 420;

export function rnaStructure(bedDetails) {
    if (!bedDetails || !bedDetails.scripts || !bedDetails.scripts.rnaStructure)
        return;

    for (let entry of bedDetails.scripts.rnaStructure) {
        let fieldRow = document.getElementById("bfld_" + entry.field);
        if (!fieldRow || !fieldRow.cells[1])
            continue;
        let labelCell = fieldRow.cells[0];
        let valCell = fieldRow.cells[1];
        if (labelCell)
            labelCell.textContent = entry.title || "Secondary structure";

        let fv = entry.fieldValues || {};
        let dotBracket = entry.value || "";
        let sequence = fv.sequence || "";
        if (!dotBracket || dotBracket.length !== sequence.length) {
            valCell.textContent = dotBracket ? dotBracket : "No structure available";
            continue;
        }
        let arms = parseArms(fv.armPositions || "", sequence.length);
        drawStructure(valCell, dotBracket, sequence, arms, entry.colors || {},
                      entry.labels || {});
    }
}

function parseArms(str, seqLen) {
    // "31-52:5p*,67-89:3p" -> [{start:31, end:52, type:"5p*"}, ...]; drops malformed ranges
    let arms = [];
    for (let part of str.split(",")) {
        let m = /^(\d+)-(\d+):(.+)$/.exec(part.trim());
        if (!m)
            continue;
        let start = parseInt(m[1]), end = parseInt(m[2]);
        if (start >= 1 && start <= end && end <= seqLen)
            arms.push({start: start, end: end, type: m[3]});
    }
    return arms;
}

function siblingUrl(file) {
    // the vendored libraries are installed in the same directory as this module
    return new URL(file, import.meta.url).href;
}

let libsLoaded = null;

function loadLibs() {
    // Load the fornac stylesheet, d3 and fornac once per page, in that order.
    if (libsLoaded)
        return libsLoaded;
    let css = document.createElement("link");
    css.rel = "stylesheet";
    css.href = siblingUrl(FORNAC_CSS);
    document.head.appendChild(css);
    libsLoaded = loadScript(siblingUrl(D3_JS)).then(() => loadScript(siblingUrl(FORNAC_JS)));
    return libsLoaded;
}

function loadScript(url) {
    return new Promise((resolve, reject) => {
        let s = document.createElement("script");
        s.src = url;
        s.onload = resolve;
        s.onerror = () => reject(new Error("could not load " + url));
        document.head.appendChild(s);
    });
}

function hexToRgba(color, alpha) {
    let m = /^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i.exec(color);
    if (!m)
        return color;
    return "rgba(" + parseInt(m[1], 16) + "," + parseInt(m[2], 16) + "," +
        parseInt(m[3], 16) + "," + alpha + ")";
}

function drawStructure(cell, dotBracket, sequence, arms, colors, labels) {
    cell.textContent = "";
    let box = document.createElement("div");
    box.style.width = VIEW_W + "px";
    box.style.height = VIEW_H + "px";
    box.style.border = "1px solid #ccc";
    cell.appendChild(box);

    // legend: one swatch per colored arm type, 5' to 3'. Not in the order of the
    // colors setting: hgc passes the config through a hash, which loses key order.
    let types = [];
    for (let a of arms.slice().sort((x, y) => x.start - y.start)) {
        if (colors[a.type] && !types.includes(a.type))
            types.push(a.type);
    }
    let legend = document.createElement("div");
    for (let type of types) {
        let item = document.createElement("span");
        item.style.marginRight = "1.5em";
        let swatch = document.createElement("span");
        swatch.style.display = "inline-block";
        swatch.style.width = "1em";
        swatch.style.height = "1em";
        swatch.style.verticalAlign = "middle";
        swatch.style.marginRight = "0.4em";
        swatch.style.backgroundColor = colors[type];
        item.appendChild(swatch);
        item.appendChild(document.createTextNode(labels[type] || type));
        legend.appendChild(item);
    }
    cell.appendChild(legend);

    let note = document.createElement("div");
    note.style.fontSize = "smaller";
    note.textContent = "Drag to move, scroll to zoom. Dot-bracket: " + dotBracket;
    note.style.wordBreak = "break-all";
    note.style.maxWidth = VIEW_W + "px";
    cell.appendChild(note);

    loadLibs().then(() => {
        /* global fornac */
        let container = new fornac.FornaContainer(box, {animation: true, zoomable: true,
                                                        initialSize: [VIEW_W, VIEW_H]});
        container.addRNA(dotBracket, {sequence: sequence, centerView: true});
        let parts = [];
        for (let a of arms) {
            if (colors[a.type])
                parts.push(a.start + "-" + a.end + ":" + hexToRgba(colors[a.type], HIGHLIGHT_ALPHA));
        }
        if (parts.length > 0)
            container.addCustomColorsText(parts.join(" "));
    }).catch(err => {
        box.textContent = "The structure viewer could not be loaded (" + err.message + ").";
    });
}
