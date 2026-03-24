// detailsHistogram.js - Draw SVG histograms on hgc details pages from bigBed field data.
// Called automatically via the detailsJs trackDb mechanism.
// Expects bedDetails object with: track, chrom, start, end, fields, args
// args.histograms is an array of {binsField, valuesField, title}

function detailsHistogram(bedDetails) {
    if (!bedDetails || !bedDetails.args || !bedDetails.args.histograms)
        return;
    if (!bedDetails.fields)
        return;

    var histograms = bedDetails.args.histograms;
    for (var ci = 0; ci < histograms.length; ci++) {
        var hist = histograms[ci];
        var binsStr = bedDetails.fields[hist.binsField];
        var valuesStr = bedDetails.fields[hist.valuesField];
        if (!binsStr || !valuesStr)
            continue;

        var bins = binsStr.split(",");
        var values = valuesStr.split(",").map(Number);
        if (bins.length !== values.length || bins.length === 0)
            continue;

        var svg = buildHistogramSvg(bins, values, hist.title);

        // Find the bins row by id attribute set by hgc and replace its value cell
        var binsRow = document.getElementById("bfld_" + hist.binsField);
        var valuesRow = document.getElementById("bfld_" + hist.valuesField);
        if (binsRow) {
            var valCell = binsRow.cells[1];
            if (valCell)
                valCell.innerHTML = svg;
            // Update the label to the histogram title
            var labelCell = binsRow.cells[0];
            if (labelCell)
                labelCell.innerHTML = hist.title || "Distribution";
        }
        // Remove the values row since data is now visualized
        if (valuesRow)
            valuesRow.remove();
    }
}

function buildHistogramSvg(bins, values, title) {
    var maxVal = Math.max.apply(null, values);
    if (maxVal === 0)
        return "<em>No data</em>";

    // Chart dimensions
    var barWidth = Math.max(14, Math.min(36, Math.floor(600 / bins.length)));
    var gap = Math.max(1, Math.floor(barWidth / 8));
    var chartHeight = 160;
    var labelHeight = 30;
    var topPad = 5;
    var leftPad = 50; // room for y-axis labels
    var svgWidth = leftPad + bins.length * barWidth + 10;
    var svgHeight = chartHeight + labelHeight + topPad;

    var lines = [];
    lines.push('<svg width="' + svgWidth + '" height="' + svgHeight + '" ' +
        'style="font-family: sans-serif; font-size: 11px;">');

    // Y-axis: a few tick marks
    var ticks = calcHistTicks(maxVal);
    for (var ti = 0; ti < ticks.length; ti++) {
        var tickVal = ticks[ti];
        var y = topPad + chartHeight - (tickVal / maxVal) * chartHeight;
        lines.push('<line x1="' + (leftPad - 4) + '" y1="' + y + '" x2="' + (leftPad) +
            '" y2="' + y + '" stroke="#666"/>');
        lines.push('<text x="' + (leftPad - 6) + '" y="' + (y + 4) +
            '" text-anchor="end" fill="#333" font-size="10">' + formatHistNumber(tickVal) + '</text>');
        // Subtle grid line
        lines.push('<line x1="' + leftPad + '" y1="' + y + '" x2="' + (svgWidth - 10) +
            '" y2="' + y + '" stroke="#eee"/>');
    }

    // Bars
    for (var i = 0; i < bins.length; i++) {
        var barHeight = (values[i] / maxVal) * chartHeight;
        var bx = leftPad + i * barWidth + gap;
        var by = topPad + chartHeight - barHeight;
        var bw = barWidth - 2 * gap;

        lines.push('<rect x="' + bx + '" y="' + by + '" width="' + bw +
            '" height="' + barHeight + '" fill="#4682B4">');
        lines.push('<title>' + bins[i] + ': ' + values[i] + '</title>');
        lines.push('</rect>');

        // X-axis label (skip some if too crowded)
        var labelEvery = Math.ceil(bins.length / (svgWidth / 30));
        if (i % labelEvery === 0 || bins.length <= 30) {
            lines.push('<text x="' + (bx + bw / 2) + '" y="' + (topPad + chartHeight + 14) +
                '" text-anchor="middle" fill="#333" font-size="10">' + bins[i] + '</text>');
        }
    }

    // Axis lines
    lines.push('<line x1="' + leftPad + '" y1="' + topPad + '" x2="' + leftPad +
        '" y2="' + (topPad + chartHeight) + '" stroke="#333"/>');
    lines.push('<line x1="' + leftPad + '" y1="' + (topPad + chartHeight) + '" x2="' +
        (svgWidth - 10) + '" y2="' + (topPad + chartHeight) + '" stroke="#333"/>');

    // X-axis label
    lines.push('<text x="' + (leftPad + (svgWidth - leftPad) / 2) + '" y="' +
        (svgHeight - 2) + '" text-anchor="middle" fill="#333" font-size="11">Allele size (repeat copies)</text>');

    lines.push('</svg>');
    return lines.join("\n");
}

function calcHistTicks(maxVal) {
    // Return 3-5 nice tick values for the y-axis
    if (maxVal <= 5)
        return [1, 2, 3, 4, 5].filter(function(v) { return v <= maxVal; });
    var magnitude = Math.pow(10, Math.floor(Math.log10(maxVal)));
    var step = magnitude;
    if (maxVal / step < 3)
        step = magnitude / 2;
    else if (maxVal / step > 6)
        step = magnitude * 2;
    var ticks = [];
    for (var v = step; v <= maxVal; v += step)
        ticks.push(Math.round(v));
    return ticks;
}

function formatHistNumber(n) {
    if (n >= 1000000) return (n / 1000000).toFixed(1) + "M";
    if (n >= 1000) return (n / 1000).toFixed(1) + "K";
    return "" + n;
}
