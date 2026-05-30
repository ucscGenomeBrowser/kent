// hgPhyloPlace.js - Javascript for use in hgPhyloPlace CGI

// Copyright (C) 2026 The Regents of the University of California

// Don't complain about line break before '||' etc:
/* jshint -W014 */
/* jshint esversion: 8 */
/* globals $, window */


  /////////////////////////////////////////////////////////////////////////////////////////////
 //// recombinantGraph: draw a RIVET-like SVG image of a recombinant and its parent nodes ////
/////////////////////////////////////////////////////////////////////////////////////////////

var recombinantGraph = (function () {
    "use strict";

    // Parameters/configuration: most will be dynamically computed from font size
    var cfg = { matchesAcceptor: '#40C0E0',
                matchesDonor: '#E86030',
                uninformative: '#D0D0D0'
              };

    const baseColorsInformative = { 'A': '#B02818',
                                    'C': '#601C68',
                                    'G': '#BC8040',
                                    'T': '#488040',
                                    'ref': '#000000' };
    const baseColorsNonInformative = { 'A': '#BE7A72',
                                       'C': '#96749A',
                                       'G': '#C4A686',
                                       'T': '#8AA686',
                                       'ref': '#CCCCCC' };

    function configLayout(fontSize) {
        // Given font size as integer, compute all layout parameters
        if (! fontSize || ! Number.isInteger(fontSize)) {
            fontSize = 8;
        }
        cfg.fontSize = fontSize;
        cfg.titleFontSize = cfg.fontSize + 2;
        cfg.titleLineHeight = cfg.fontSize * 1.25;
        cfg.titleAreaHeight = cfg.titleLineHeight * 7;
        cfg.minBaseAreaWidth = cfg.titleFontSize * 75;
        cfg.leftLabelAreaWidth = cfg.fontSize * 20;
        cfg.posLabelAreaHeight = cfg.fontSize * 5;
        cfg.connectorAreaHeight = cfg.fontSize * 12;
        cfg.genomeGraphHeight = cfg.fontSize * 2;
        cfg.geneGraphHeight = cfg.fontSize * 2;
        cfg.tickHeight = cfg.fontSize / 2;
        cfg.rightPad = cfg.fontSize / 2;
        cfg.topPad = cfg.fontSize / 2;
        cfg.baseTextPad = cfg.fontSize / 4;
        cfg.baseWidth = cfg.fontSize * 1.25;
        cfg.interbaseWidth = cfg.fontSize / 4;
        cfg.basePitch = cfg.baseWidth + cfg.interbaseWidth;
        cfg.baseArrayHeight = 4 * cfg.basePitch - cfg.interbaseWidth;
        cfg.genomeScaleHeight = cfg.topPad * 3 + cfg.baseWidth;
    }

    function makeSvg(width, height) {
        // Create and return an SVG of the given size.
        var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
        svg.setAttribute("width", width);
        svg.setAttribute("height", height);
        svg.setAttribute("viewBox", "0 0 " + width + " " + height);
        return svg;
    }

    function svgCreateEl(type, config) {
        // Helper function for creating a new SVG element and initializing its
        // properties and attributes.  Type is something like 'rect', 'text', 'g', etc;
        // config is an object like { id: 'newThingie', x: 0, y: 10, title: 'blah blah' }.
        // Copied from hgGateway.js.
        var svgns = 'http://www.w3.org/2000/svg';
        var xlinkns = 'http://www.w3.org/1999/xlink';
        var el = document.createElementNS(svgns, type);
        var title, titleEl;
        if (el) {
            for (const [setting, value] of Object.entries(config)) {
                if (setting === 'textContent') {
                    // Text content (the text in a text element or title element) is a property:
                    el.textContent = value;
                } else if (setting === 'href') {
                    // href comes from a different namespace so must use setAttributeNS:
                    el.setAttributeNS(xlinkns, 'href', value);
                } else if (setting === 'title') {
                    title = value;
                } else if (setting === 'className') {
                    el.setAttribute('class', value);
                } else {
                    // Most of the time we're just setting an attribute:
                    el.setAttribute(setting, value);
                }
            }
        }
        // Mouseover title actually requires creating a child element.
        // Strangely, if I did this in the above loop, the child element was lost if
        // props/attributes were set afterwards!!  So save title for last.
        if (title) {
            titleEl = document.createElementNS(svgns, 'title');
            titleEl.textContent = title;
            el.appendChild(titleEl);
        }
        return el;
    }

    function addRectFill(svg, x, y, width, height, color, config={}) {
        // Add filled rectangle to svg
        config = Object.assign({ x: x, y: y, width: width, height: height,
                                 style: 'fill:' + color + '; stroke:' + color },
                               config);
        var rect = svgCreateEl('rect', config);
        svg.appendChild(rect);
    }

    function parseMutation(mutStr) {
        // Parse mutStr into reference base, position and alternate base and return object {pos, ref, alt}.
        var match = mutStr.match(/^([A-Za-z])(\d+)([A-Za-z])$/);
        if (!match) {
            console.error("parseMutation: invalid mutation string: " + mutStr);
            return null;
        }
        return {
            pos: parseInt(match[2], 10),
            ref: match[1],
            alt: match[3]
        };
    }

    function parseMutations(mutStr) {
        // Return a sorted list of mutation objects
        return mutStr.split(",").map(parseMutation).sort(function(a, b) { return a.pos - b.pos; });
    }


    function minPos3(mutA, mutB, mutC) {
        // Given 3 inputs, each of which might be undefined or null or object with pos, return the minimum pos.
        // Assume that at least one of the inputs is an object with pos.
        if (mutA && mutB && mutC) {
            let minAB = mutA.pos < mutB.pos ? mutA.pos : mutB.pos;
            return minAB < mutC.pos ? minAB : mutC.pos;
        } else if (mutA) {
            if (mutB) {
                return mutA.pos < mutB.pos ? mutA.pos : mutB.pos;
            } else if (mutC) {
                return mutA.pos < mutC.pos ? mutA.pos : mutC.pos;
            } else {
                return mutA.pos;
            }
        } else if (mutB) {
            if (mutC) {
                return mutB.pos < mutC.pos ? mutB.pos : mutC.pos;
            } else {
                return mutB.pos;
            }
        } else if (mutC) {
            return mutC.pos;
        } else {
            console.error("minPos given all empties!");
            return null;
        }
    }

    function makeCombinedMuts(recombMuts, donorMuts, acceptorMuts) {
        // Return an array of objects encoding combined mutations (or alleles) from the recombinant and parents:
        // { pos, ref, dAl, rAl, aAl }
        var combinedMuts = [];
        var rMuts = parseMutations(recombMuts);
        var dMuts = parseMutations(donorMuts);
        var aMuts = parseMutations(acceptorMuts);
        var dMut = dMuts.shift();
        var rMut = rMuts.shift();
        var aMut = aMuts.shift();
        while (dMut !== undefined || rMut !== undefined || aMut !== undefined) {
            let minPos = minPos3(dMut, rMut, aMut);
            if (minPos === null) {
                break;
            }
            let combo = { pos: minPos };
            let ref;
            if (dMut && dMut.pos === minPos) {
                ref = dMut.ref;
                combo.dAl = dMut.alt;
                dMut = dMuts.shift();
            }
            if (rMut && rMut.pos === minPos) {
                if (ref && ref != rMut.ref) {
                    console.error("Differing ref base for", minPos, ":", ref, "vs", rMut.ref);
                }
                ref = rMut.ref;
                combo.rAl = rMut.alt;
                rMut = rMuts.shift();
            }
            if (aMut && aMut.pos === minPos) {
                if (ref && ref != aMut.ref) {
                    console.error("Differing ref base for", minPos, ":", ref, "vs", aMut.ref);
                }
                ref = aMut.ref;
                combo.aAl = aMut.alt;
                aMut = aMuts.shift();
            }
            combo.ref = ref;
            if (! combo.dAl) {
                combo.dAl = ref;
            }
            if (! combo.rAl) {
                combo.rAl = ref;
            }
            if (! combo.aAl) {
                combo.aAl = ref;
            }
            combinedMuts.push(combo);
        }
        return combinedMuts;
    }

    function filterMuts(combinedMuts) {
        // Filter combinedMuts to keep only those that are informative to whether the base comes from acceptor or donor.
        let filteredMuts = [];
        for (let column of combinedMuts) {
            let matchesAcceptor = column.aAl === column.rAl;
            let matchesDonor = column.dAl === column.rAl;
            if (matchesAcceptor !== matchesDonor) {
                filteredMuts.push(column);
            }
        }
        return filteredMuts;
    }

    function addText(svg, text, x, y, config) {
        if (! config) {
            config = {};
        }
        config.x = x;
        config.y = y;
        config.textContent = text;
        if (! config['font-size']) {
            config['font-size'] = cfg.fontSize + 'px';
        }
        var el = svgCreateEl('text', config);
        svg.appendChild(el);
    }

    function addTitle(svg, recombAttrs) {
        // Add some info about the potential recombinant above the graphic
        let xRight = cfg.leftLabelAreaWidth - cfg.rightPad;
        let x = cfg.leftLabelAreaWidth + cfg.rightPad;
        let y = cfg.topPad + cfg.titleFontSize;
        addText(svg, "Recombinant node:", xRight, y,
                { 'font-weight': 'bold', 'text-anchor': 'end', 'font-size': cfg.titleFontSize });
        addText(svg, recombAttrs.rNode, x, y, { 'font-size': cfg.titleFontSize });
        y += cfg.titleLineHeight;
        addText(svg, "Acceptor node:", xRight, y,
                { 'font-weight': 'bold', 'text-anchor': 'end', 'font-size': cfg.titleFontSize });
        addText(svg, recombAttrs.aNode + ' (' + recombAttrs.aLin + ')', x, y, { 'font-size': cfg.titleFontSize });
        y += cfg.titleLineHeight;
        addText(svg, "Donor node:", xRight, y,
                { 'font-weight': 'bold', 'text-anchor': 'end', 'font-size': cfg.titleFontSize });
        addText(svg, recombAttrs.dNode + ' (' + recombAttrs.dLin + ')', x, y, { 'font-size': cfg.titleFontSize });
        y += cfg.titleLineHeight;
        addText(svg, "Parsimony improvement:", xRight, y,
                { 'font-weight': 'bold', 'text-anchor': 'end', 'font-size': cfg.titleFontSize });
        addText(svg, recombAttrs.improvement, x, y, { 'font-size': cfg.titleFontSize });
        y += cfg.titleLineHeight;
        addText(svg, "Breakpoint range 1:", xRight, y,
                { 'font-weight': 'bold', 'text-anchor': 'end', 'font-size': cfg.titleFontSize });
        addText(svg, recombAttrs.bp1, x, y, { 'font-size': cfg.titleFontSize });
        y += cfg.titleLineHeight;
        addText(svg, "Breakpoint range 2:", xRight, y,
                { 'font-weight': 'bold', 'text-anchor': 'end', 'font-size': cfg.titleFontSize });
        addText(svg, recombAttrs.bp2, x, y, { 'font-size': cfg.titleFontSize });
    }

    function addLeftLabels(svg, x, xRight, y, recombAttrs) {
        // Base value / node rows
        y += cfg.posLabelAreaHeight + cfg.baseWidth - cfg.interbaseWidth;
        addText(svg, 'Acceptor (' + recombAttrs.aLin + ')', xRight, y,
                { 'font-weight': 'bold', 'text-anchor': 'end', 'fill': cfg.matchesAcceptor });
        y += cfg.basePitch;
        addText(svg, 'Recombinant', xRight, y, { 'font-weight': 'bold', 'text-anchor': 'end' });
        y += cfg.basePitch;
        addText(svg, 'Donor (' + recombAttrs.dLin + ')', xRight, y,
                { 'font-weight': 'bold', 'text-anchor': 'end', 'fill': cfg.matchesDonor });
        y += cfg.basePitch;
        addText(svg, 'Reference', xRight, y, { 'font-weight': 'bold', 'text-anchor': 'end' });
        // Connector legend
        y += cfg.basePitch * 3;
        const legendTextX = x + cfg.basePitch;
        let legendTextY = y + cfg.baseWidth - cfg.baseTextPad;
        addRectFill(svg, x, y, cfg.baseWidth, cfg.baseWidth, cfg.matchesAcceptor);
        addText(svg, 'Recombinant matches acceptor', legendTextX, legendTextY, { 'fill': cfg.matchesAcceptor });
        y += cfg.basePitch;
        legendTextY = y + cfg.baseWidth - cfg.baseTextPad;
        addRectFill(svg, x, y, cfg.baseWidth, cfg.baseWidth, cfg.matchesDonor);
        addText(svg, 'Recombinant matches donor', legendTextX, legendTextY, { 'fill': cfg.matchesDonor });
        y += cfg.basePitch;
        legendTextY = y + cfg.baseWidth - cfg.baseTextPad;
        addRectFill(svg, x, y, cfg.baseWidth, cfg.baseWidth, cfg.uninformative);
        addText(svg, 'Not informative', legendTextX, legendTextY, { 'fill': cfg.uninformative });
        // Genome & gene graph
        y += cfg.connectorAreaHeight - cfg.basePitch * 4 + cfg.interbaseWidth;
        addText(svg, 'Genomic Coordinate', xRight, y, { 'font-weight': 'bold', 'text-anchor': 'end' });
        y += cfg.genomeGraphHeight + cfg.topPad * 5;
        addText(svg, 'Gene Annotations', xRight, y, { 'font-weight': 'bold', 'text-anchor': 'end' });
    }

    function addRotation(el, angle, x, y) {
        // Add a rotate transform to el
        const rotateStr = 'rotate(' + angle + ', ' + x + ', ' + y + ')';
        // Preserve any transform already set in config (e.g. a translate)
        const existingTransform = el.getAttribute('transform');
        el.setAttribute('transform', existingTransform ? existingTransform + ' ' + rotateStr : rotateStr);
    }

    function matchColor(column) {
        // Select a color based on whether column alleles indicate that the recombinant matches one or the other
        // of acceptor and donor.  If it matches both or neither, it's not informative.
        let color = cfg.uninformative;
        let matchesAcceptor = column.aAl === column.rAl;
        let matchesDonor = column.dAl === column.rAl;
        if (matchesAcceptor !== matchesDonor) {
            color = matchesAcceptor ? cfg.matchesAcceptor : cfg.matchesDonor;
        }
        return color;
    }

    function addPositionLabels(svg, x, y, combinedMuts) {
        // Add a row of slanted labels above the base array, with each position (1-based)
        // Tweak coordinates for SVG default text anchoring
        x += cfg.fontSize;
        y += cfg.posLabelAreaHeight - cfg.interbaseWidth;
        for (let column of combinedMuts) {
            const color = matchColor(column);
            var text = svgCreateEl('text', { x: x,
                                             y: y,
                                             textContent: column.pos,
                                             'font-size': cfg.fontSize + 'px',
                                             fill: color });
            addRotation(text, -75, x, y);
            svg.appendChild(text);
            x += cfg.basePitch;
        }
    }

    function baseColor(base, isRef, isInformative) {
        // Choose a color by base value and informativeness
        const baseColors = isInformative ? baseColorsInformative : baseColorsNonInformative;
        let color = isRef ? baseColors.ref : baseColors[base];
        if (! color) {
            console.error("baseColor: Invalid base '" + base + "'");
            color = '#FFFF00';
        }
        return color;
    }

    function addBase(svg, base, x, y, isRef, isInformative) {
        // Draw a base as a rectangle colored by value and informativeness with the base value in white
        addRectFill(svg, x, y, cfg.baseWidth, cfg.baseWidth, baseColor(base, isRef, isInformative));
        addText(svg, base, x + cfg.baseTextPad, y + cfg.baseWidth - cfg.baseTextPad, { 'fill': 'white' });
    }

    function addBaseArray(svg, x, y, combinedMuts) {
        // Add base value at each position for acceptor, recombinant, donor and reference
        for (let column of combinedMuts) {
            const isInformative = (column.aAl !== column.dAl && (column.rAl === column.aAl || column.rAl == column.dAl));
            let yBase = y;
            let isRef = (column.aAl === column.ref);
            addBase(svg, column.aAl, x, yBase, isRef, isInformative && !isRef);
            yBase += cfg.basePitch;
            isRef = (column.rAl === column.ref);
            addBase(svg, column.rAl, x, yBase, isRef, isInformative && !isRef);
            yBase += cfg.basePitch;
            isRef = (column.dAl === column.ref);
            addBase(svg, column.dAl, x, yBase, isRef, isInformative && !isRef);
            yBase += cfg.basePitch;
            addBase(svg, column.ref, x, yBase, true, true);
            x += cfg.basePitch;
        }
    }

    function addConnectors(svg, x, y, genomeY, genomeScale, combinedMuts) {
        // Add triangles connecting the bottom of each base column to the corresponding genomic coordinate,
        // colored by whether the recombinant matches acceptor only, donor only, or both/neither (uninformative)
        let cx = 0;
        for (let column of combinedMuts) {
            const color = matchColor(column);
            // Start at bottom left of base column
            let baseX = x + cx * cfg.basePitch;
            let path = "M " + baseX + ' ' + y + ' ';
            // Line to bottom right of base column
            path += "L " + (baseX + cfg.baseWidth) + ' ' + y + ' ';
            // Line to genomic coordinate point
            let genomeX = x + column.pos * genomeScale;
            path += "L " + genomeX + ' ' + genomeY + ' ';
            // Close the path
            path += 'Z';
            svg.appendChild(svgCreateEl('path', { d: path, fill: color, stroke: color,
                                                  title: column.ref + column.pos + column.rAl }));
            cx++;
        }
    }

    function addGenomeGraph(svg, x, y, width, height, genomeScale, combinedMuts) {
        // Add a gray rectangle showing the extent of the genome and a line per mutation
        // colored by whether the recombinant matches acceptor only, donor only, or both/neither (uninformative)
        addRectFill(svg, x, y, width, height, '#EEEEEE');
        for (let column of combinedMuts) {
            let xMut = x + column.pos * genomeScale;
            var color = matchColor(column);
            var el = svgCreateEl('line', { x1: xMut, y1: y, x2: xMut, y2: y + height, stroke: color });
            svg.appendChild(el);
        }
    }

    function addTick(svg, x, y, height) {
        // Add a short vertical line
        var el = svgCreateEl('line', { x1: x, y1: y, x2: x, y2: y + height, stroke: 'black' });
        svg.appendChild(el);
    }

    function commafy(number) {
        // Return a string that is the number with commas delineating thousands, millions etc.
        const string = String(number);
        const digitsStart = string.length % 3;
        var out = digitsStart ? string.substring(0, digitsStart) : '';
        for (let i = digitsStart;  i < string.length;  i += 3) {
            if (i > 0) {
                out += ',';
            }
            out += string.substring(i, i+3);
        }
        return out;
    }

    function estimateTextWidth(text) {
        // SVG text won't reveal its size until it's rendered.  Return a guess based on an assumption of font size
        // relative to baseWidth, and font width relative to font height.
        return text.length * cfg.baseWidth * 0.5;
    }

    function addGenomeScale(svg, x, y, width, genomeSize, genomeScale) {
        // Add tick marks and centered text labels underneath the genome graph
        const genomeSizeString = commafy(genomeSize);
        const maxLabelWidth = estimateTextWidth(genomeSizeString);
        const maxBins = Math.floor(width / (maxLabelWidth * 1.5));
        const powerOf10 = Math.ceil(Math.log10(genomeSize));
        const tickTextY = y + 3 * cfg.topPad;
        for (let binSize of [100, 200, 500, 1000, 200, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000]) {
            let binCount = Math.ceil(genomeSize / binSize);
            if (binCount < maxBins) {
                for (let gx = 0;  gx < genomeSize;  gx += binSize) {
                    const xOff = gx * genomeScale;
                    const tickX = x + xOff;
                    addTick(svg, tickX, y, cfg.tickHeight);
                    // Don't write label if it might overlap with the label at the end of the genome
                    if (width - xOff > maxLabelWidth) {
                        addText(svg, commafy(gx), tickX, tickTextY, { fill: 'black', 'text-anchor': 'middle' });
                    }
                }
                break;
            }
        }
        // Add one more for the end of the genome
        addTick(svg, x + width, y, cfg.tickHeight);
        addText(svg, genomeSizeString, x + width, tickTextY, { fill: 'black', 'text-anchor': 'middle' });
    }

    function addGeneGraph(svg, x, y, width, height, genomeScale, genes) {
        // Add a gray rectangle showing the extent of the genome and a colorful rectangle per gene,
        // with a gene name label if the rectangle is large enough
        addRectFill(svg, x, y, width, height, '#EEEEEE');
        const geneTextY = y + 5 * height / 8;
        for (let gene of genes) {
            let geneX = x + gene.start * genomeScale;
            let geneWidth = (gene.end - gene.start) * genomeScale;
            addRectFill(svg, geneX, y, geneWidth, height, gene.color, { title: gene.name });
            let geneXMid = geneX + geneWidth / 2;
            if (estimateTextWidth(gene.name) < geneWidth) {
                addText(svg, gene.name, geneXMid, geneTextY,
                        { fill: 'white', 'text-anchor': 'middle', title: gene.name });
            }
        }
    }

    function draw(recombAttrs, genomeSize, genes, showInformativeOnly) {
        // Return an SVG diagram showing the mutations in acceptor, recombinant and donor to visualize the evidence for
        // recombination.
        configLayout(10);
        let combinedMuts = makeCombinedMuts(recombAttrs.recombMuts, recombAttrs.donorMuts, recombAttrs.acceptorMuts);
        if (showInformativeOnly) {
            combinedMuts = filterMuts(combinedMuts);
        }
        let baseAreaWidth = combinedMuts.length * cfg.basePitch - cfg.interbaseWidth;
        if (baseAreaWidth < cfg.minBaseAreaWidth) {
            baseAreaWidth = cfg.minBaseAreaWidth;
        }
        let svgWidth = cfg.leftLabelAreaWidth + baseAreaWidth + 2 * cfg.baseWidth;
        let svgHeight = cfg.titleAreaHeight + cfg.posLabelAreaHeight + cfg.baseArrayHeight + cfg.connectorAreaHeight + cfg.genomeGraphHeight +
                        cfg.genomeScaleHeight + cfg.geneGraphHeight;
        var svg = makeSvg(svgWidth, svgHeight);

        // Title/info area
        addTitle(svg, recombAttrs);

        // Left labels
        let x = cfg.rightPad;
        let xRight = cfg.leftLabelAreaWidth - cfg.rightPad;
        let y = cfg.titleAreaHeight;
        addLeftLabels(svg, x, xRight, y, recombAttrs);

        // Base position labels
        x = cfg.leftLabelAreaWidth;
        addPositionLabels(svg, x, y, combinedMuts);

        // Base values for acceptor, recombinant, donor and reference
        y += cfg.posLabelAreaHeight;
        addBaseArray(svg, x, y, combinedMuts);

        // Connectors between base values and genomic coordinates
        y += cfg.baseArrayHeight;
        const genomeScale = baseAreaWidth / genomeSize;
        const genomeY = y + cfg.connectorAreaHeight;
        addConnectors(svg, x, y, genomeY, genomeScale, combinedMuts);

        // Genomic coordinate graph with mut lines
        y += cfg.connectorAreaHeight;
        addGenomeGraph(svg, x, y, baseAreaWidth, cfg.genomeGraphHeight, genomeScale, combinedMuts);

        // Genome coordinate scale ticks and labels
        y += cfg.genomeGraphHeight;
        addGenomeScale(svg, x, y, baseAreaWidth, genomeSize, genomeScale, genes);

        // Gene graph with embedded labels
        y += cfg.genomeScaleHeight;
        addGeneGraph(svg, x, y, baseAreaWidth, cfg.geneGraphHeight, genomeScale, genes);
        return svg;
    }

    return { draw: draw };

}()); // recombinantGraph



  ////////////////////////////////////////////////////////////
 //// popupRecombinant: pop up a recombinantGraph dialog ////
////////////////////////////////////////////////////////////

var popUpRecombinant = (function () {
    "use strict";

    function cleanup() {
        // Clean out contents when done
        if ($('#recombinantDialog').html().length > 0) {
            $('#recombinantDialog').html('');
        }
    }

    function saveSvg(svgElement, name="image.svg") {
        // Serialize the SVG to a XML string
        const serializer = new XMLSerializer();
        let source = serializer.serializeToString(svgElement);

        // Add namespaces if they are missing
        if (!source.match(/^<svg[^>]+xmlns="http:\/\/www\.w3\.org\/2000\/svg"/)) {
            source = source.replace(/^<svg/, '<svg xmlns="http://www.w3.org/2000/svg"');
        }
        if (!source.match(/^<svg[^>]+"http:\/\/www\.w3\.org\/1999\/xlink"/)) {
            source = source.replace(/^<svg/, '<svg xmlns:xlink="http://www.w3.org/1999/xlink"');
        }

        // Add XML declaration and create a Blob
        const svgData = '<?xml version="1.0" standalone="no"?>\r\n' + source;
        const svgBlob = new Blob([svgData], {type: "image/svg+xml;charset=utf-8"});
        const svgUrl = URL.createObjectURL(svgBlob);

        // Create a temporary download link, click it, remove it
        const downloadLink = document.createElement("a");
        downloadLink.href = svgUrl;
        downloadLink.download = name;
        document.body.appendChild(downloadLink);
        downloadLink.click();
        document.body.removeChild(downloadLink);
    }

    function display(recombinantData, recombinantIndex, showInformativeOnly) {
        // Searching for some semblance of size suitability
        var popMaxHeight = (window.innerHeight * 0.85); // make 15% of the bottom of the screen still visible
        var popMaxWidth  =  (window.innerWidth * 0.9);  // take up 90% of the window
        var recombAttrs = recombinantData.recombinants[recombinantIndex];
        var genomeSize = recombinantData.genomeSize;
        var genes = recombinantData.genes;

        $('#recombinantDialog').html("<div id='popContents' style='font-size:1.1em;'></div>");
        var $div = $('#popContents');
        $div.html("<p><button id='prevRecomb'>previous</button>&nbsp;&nbsp;" +
                  "<input type='checkbox' id='hgpp_informativeOnly'>&nbsp;Show only informative mutations&nbsp;&nbsp;" +
                  "<button id='saveSvg'>save image</button>&nbsp;&nbsp;" +
                  "<button id='nextRecomb'>next</button></p>\n");

        var svg = recombinantGraph.draw(recombAttrs, genomeSize, genes, showInformativeOnly);
        $div.append(svg);

        // Clicking on the checkbox toggles showInformativeOnly behavior
        let $cb = $('#hgpp_informativeOnly');
        $cb.prop('checked', showInformativeOnly);
        $cb.on('click', function() {
            // Redraw image, update cart var and hidden input that tracks the checkbox across popups
            showInformativeOnly = $cb.prop('checked');
            setCartVar('hgpp_informativeOnly', showInformativeOnly ? '1' : '0', null, true);
            $('#hidden_showInformative').val(showInformativeOnly ? '1' : '0');
            $div.children("svg").remove();
            $div.append(recombinantGraph.draw(recombAttrs, genomeSize, genes, showInformativeOnly));
        });
        // Disable prev/next button if at beginning/end
        let $prevBtn = $('#prevRecomb');
        let $nextBtn = $('#nextRecomb');
        $prevBtn.prop('disabled', recombinantIndex < 1);
        $nextBtn.prop('disabled', recombinantIndex >= recombinantData.recombinants.length - 1);
        $prevBtn.on('click', function() {
            if (recombinantIndex > 0) {
                recombinantIndex--;
                $prevBtn.prop('disabled', recombinantIndex < 1);
                $nextBtn.prop('disabled', recombinantIndex >= recombinantData.recombinants.length - 1);
                recombAttrs = recombinantData.recombinants[recombinantIndex];
                $div.children("svg").remove();
                $div.append(recombinantGraph.draw(recombAttrs, genomeSize, genes, showInformativeOnly));
            }
        });
        $nextBtn.on('click', function() {
            if (recombinantIndex < recombinantData.recombinants.length - 1) {
                recombinantIndex++;
                $prevBtn.prop('disabled', recombinantIndex < 1);
                $nextBtn.prop('disabled', recombinantIndex >= recombinantData.recombinants.length - 1);
                recombAttrs = recombinantData.recombinants[recombinantIndex];
                $div.children("svg").remove();
                $div.append(recombinantGraph.draw(recombAttrs, genomeSize, genes, showInformativeOnly));
            }
        });
        // Save image button
        let $saveImgBtn = $('#saveSvg');
        $saveImgBtn.on('click', function() {
            saveSvg(svg, "recombinant.svg");
        });

        var uiDialogButtons = {
            Close: function() {
                $(this).dialog("close");
            }
        };
        $('#recombinantDialog').dialog({
            resizable: false,
            height: popMaxHeight,
            width: popMaxWidth,
            minHeight: 200,
            minWidth: 400,
            modal: true,
            closeOnEscape: true,
            autoOpen: false,
            buttons: uiDialogButtons,
            title: "Potential recombinant",

            open: function (event) {
                // fix popup to a location -- near the top and somewhat centered on the browser image
                $(event.target).parent().css('position', 'fixed');
                $(event.target).parent().css('top', '10%');
            },

            close: function() {
                popUpRecombinant.cleanup();
            }
        });
        $('#recombinantDialog').dialog('open');
    } // display

    return { cleanup: cleanup,
             display: display };
}());


  //////////////////////
 //// hgPhyloPlace ////
//////////////////////

var hgPhyloPlace = (function() {
    "use strict";

    function onClickRecombinant(recombinantData, recombinantIndex, showInformativeOnly) {
        // When user clicks on "Show mutations" button for a potential recombinant, make a pop-up with a diagram showing
        // mutations in the recombinant and its parents, highlighting where they agree.
        popUpRecombinant.display(recombinantData, recombinantIndex, showInformativeOnly);
    }

    return { onClickRecombinant: onClickRecombinant };

}()); // hgPhyloPlace
