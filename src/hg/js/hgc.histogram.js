// hgc.histogram.js - ES6 module for drawing SVG histograms on hgc details pages.
// Loaded via dynamic import() from the detailsScript trackDb mechanism.
// bedDetails.scripts.histogram is an array of {field, value, title, xLabel}
// where value is logfmt: space-separated key=value pairs (e.g. "3=11 4=3809")

export function histogram(bedDetails) {
    if (!bedDetails || !bedDetails.scripts || !bedDetails.scripts.histogram)
        return;

    let entries = bedDetails.scripts.histogram;
    for (let entry of entries) {
        if (!entry.value)
            continue;

        let parsed = parseLogfmt(entry.value);
        if (parsed.keys.length === 0)
            continue;

        let svg = buildSvg(parsed.keys, parsed.values, entry.title, entry.xLabel);

        // Find the field row by id and draw into its value cell
        let fieldRow = document.getElementById("bfld_" + entry.field);
        if (fieldRow) {
            let valCell = fieldRow.cells[1];
            if (valCell)
                valCell.innerHTML = svg;
            let labelCell = fieldRow.cells[0];
            if (labelCell)
                labelCell.innerHTML = entry.title || "Distribution";
        }
    }
}

function parseLogfmt(str) {
    // Parse logfmt string into parallel arrays of keys and numeric values.
    // Supports: key=val, "quoted key"=val, key="quoted val"
    // Examples: '3=11 4=3809', '"My point"=20 other=5'
    let keys = [];
    let values = [];
    let i = 0;
    let len = str.length;
    while (i < len) {
        // skip whitespace
        while (i < len && str[i] === " ")
            i++;
        if (i >= len)
            break;
        // read key (possibly quoted)
        let key;
        if (str[i] === '"') {
            i++;
            let end = str.indexOf('"', i);
            if (end === -1)
                break;
            key = str.substring(i, end);
            i = end + 1;
        } else {
            let start = i;
            while (i < len && str[i] !== "=" && str[i] !== " ")
                i++;
            key = str.substring(start, i);
        }
        // expect '='
        if (i >= len || str[i] !== "=")
            break;
        i++;
        // read value (possibly quoted)
        let val;
        if (i < len && str[i] === '"') {
            i++;
            let vend = str.indexOf('"', i);
            if (vend === -1)
                break;
            val = str.substring(i, vend);
            i = vend + 1;
        } else {
            let vstart = i;
            while (i < len && str[i] !== " ")
                i++;
            val = str.substring(vstart, i);
        }
        keys.push(key);
        values.push(Number(val));
    }
    return {keys, values};
}

function buildSvg(bins, values, title, xLabel) {
    let maxVal = Math.max(...values);
    if (maxVal === 0)
        return "<em>No data</em>";

    // Chart dimensions
    let barWidth = Math.max(14, Math.min(36, Math.floor(600 / bins.length)));
    let gap = Math.max(1, Math.floor(barWidth / 8));
    let chartHeight = 160;
    let labelHeight = 30;
    let topPad = 5;
    let leftPad = 50;
    let svgWidth = Math.max(200, leftPad + bins.length * barWidth + 10);
    let svgHeight = chartHeight + labelHeight + topPad;

    let lines = [];
    lines.push(`<svg width="${svgWidth}" height="${svgHeight}" style="font-family: sans-serif; font-size: 11px;">`);

    // Y-axis tick marks
    let ticks = calcTicks(maxVal);
    for (let tickVal of ticks) {
        let y = topPad + chartHeight - (tickVal / maxVal) * chartHeight;
        lines.push(`<line x1="${leftPad - 4}" y1="${y}" x2="${leftPad}" y2="${y}" stroke="#666"/>`);
        lines.push(`<text x="${leftPad - 6}" y="${y + 4}" text-anchor="end" fill="#333" font-size="10">${formatNumber(tickVal)}</text>`);
        lines.push(`<line x1="${leftPad}" y1="${y}" x2="${svgWidth - 10}" y2="${y}" stroke="#eee"/>`);
    }

    // Bars
    let labelEvery = Math.ceil(bins.length / (svgWidth / 30));
    for (let i = 0; i < bins.length; i++) {
        let barHeight = (values[i] / maxVal) * chartHeight;
        let bx = leftPad + i * barWidth + gap;
        let by = topPad + chartHeight - barHeight;
        let bw = barWidth - 2 * gap;

        lines.push(`<rect x="${bx}" y="${by}" width="${bw}" height="${barHeight}" fill="#4682B4">`);
        lines.push(`<title>${bins[i]}: ${values[i]}</title>`);
        lines.push('</rect>');

        if (i % labelEvery === 0 || bins.length <= 30) {
            lines.push(`<text x="${bx + bw / 2}" y="${topPad + chartHeight + 14}" text-anchor="middle" fill="#333" font-size="10">${bins[i]}</text>`);
        }
    }

    // Axis lines
    lines.push(`<line x1="${leftPad}" y1="${topPad}" x2="${leftPad}" y2="${topPad + chartHeight}" stroke="#333"/>`);
    lines.push(`<line x1="${leftPad}" y1="${topPad + chartHeight}" x2="${svgWidth - 10}" y2="${topPad + chartHeight}" stroke="#333"/>`);

    if (xLabel) {
        lines.push(`<text x="${leftPad + (svgWidth - leftPad) / 2}" y="${svgHeight - 2}" text-anchor="middle" fill="#333" font-size="11">${xLabel}</text>`);
    }

    lines.push('</svg>');
    return lines.join("\n");
}

function calcTicks(maxVal) {
    if (maxVal <= 5)
        return [1, 2, 3, 4, 5].filter(v => v <= maxVal);
    let magnitude = Math.pow(10, Math.floor(Math.log10(maxVal)));
    let step = magnitude;
    if (maxVal / step < 3)
        step = magnitude / 2;
    else if (maxVal / step > 6)
        step = magnitude * 2;
    let ticks = [];
    for (let v = step; v <= maxVal; v += step)
        ticks.push(Math.round(v));
    return ticks;
}

function formatNumber(n) {
    if (n >= 1000000) return (n / 1000000).toFixed(1) + "M";
    if (n >= 1000) return (n / 1000).toFixed(1) + "K";
    return "" + n;
}
