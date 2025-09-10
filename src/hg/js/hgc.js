// "use strict";
/* jshint esnext: true */

// insert a node after the reference node
function insertAfter(newNode, referenceNode) {
    referenceNode.parentNode.insertBefore(newNode, referenceNode.nextSibling);
}

// a generic table with no headers or sortable rows:
function makeGenericTable(data) {
    var table = document.createElement("table");
    table.classList.add("jsonTable");
    var val, i, j;
    var keys = Object.keys(data);
    for (i = 0; i < keys.length; i++) {
        key = keys[i];
        val = data[key];
        var row = table.insertRow();
        if (Array.isArray(val)) {
            for (j = 0; j < val.length; j++) {
                let cell = row.insertCell();
                if (j < val.length) {
                    cell.appendChild(document.createTextNode(val[j]));
                } else {
                    cell.appendChild(document.createTextNode(""));
                }
            }
        } else {
            var rowLabel = document.createTextNode(key);
            var labelCell = row.insertCell();
            var cell;
            labelCell.appendChild(rowLabel);

            if (typeof(val) == "object" && !Array.isArray(val)) {
                cell = row.insertCell();
                cell.appendChild(makeGenericTable(val));
            } else {
                if (val === null) {
                    cell = row.insertCell();
                    cell.appendChild(document.createTextNode(""));
                } else {
                    cell = row.insertCell();
                    cell.appendChild(document.createTextNode(val));
                }
            }
        }
    }
    return table;
}

// make the vep table, which is limited to 2x2 for the vep fields
function makeVepTable(data) {
    var table = document.createElement("table");
    table.classList.add("jsonTable");
    var gene;
    for (gene in data) {
        var geneRow = table.insertRow();
        var newLabel = document.createTextNode(gene);
        var labelCell = geneRow.insertCell();
        labelCell.appendChild(newLabel);

        // a 2x2 sub table for the vep info:
        var subTable = document.createElement("table");
        subTable.classList.add("jsonTable");
        var annot;
        for (annot in data[gene]) {
            var annotRow = subTable.insertRow();
            var annotLabel = document.createTextNode(annot);
            var annotCell = annotRow.insertCell();
            annotCell.appendChild(annotLabel);
            annotCell = annotRow.insertCell();
            var dataLabel = document.createTextNode(data[gene][annot].join(", "));
            annotCell.appendChild(dataLabel);
        }
        labelCell = geneRow.insertCell();
        labelCell.appendChild(subTable);
    }
    return table;
}

// make the pop frequencies or haplotype frequencies table
function makePopTable(data) {
    var table = document.createElement("table");
    table.classList.add("jsonTable");
    var pop;
    var thead, tfoot, tbody;
    for (pop in data) {
        if (pop === "Populations" || pop === "Haplogroup") {
            thead = table.createTHead();
            theadRow = thead.insertRow();
            th = theadRow.insertCell();
            th.appendChild(document.createTextNode(pop));
            var header;
            for (header in data[pop]) {
                th = theadRow.insertCell();
                th.appendChild(document.createTextNode(data[pop][header]));
            }
        } else if (pop === "Total") {
            tfoot = table.createTFoot();
            tfootRow = tfoot.insertRow();
            th = tfootRow.insertCell();
            th.appendChild(document.createTextNode(pop));
            var footer;
            for (footer in data[pop]) {
                th = tfootRow.insertCell();
                th.appendChild(document.createTextNode(data[pop][footer]));
            }
        } else {
            if (table.tBodies.length === 0) {
                tbody = table.createTBody();
            }
            var popRow = tbody.insertRow();
            var newLabel = document.createTextNode(pop);
            var labelCell = popRow.insertCell();
            labelCell.appendChild(newLabel);

            // a 5x5 sub table for the population freqs or 6x6 for haplotypes:
            var vals;
            for (vals in data[pop]) {
                var valsLabel = document.createTextNode(data[pop][vals]);
                var valsCell = popRow.insertCell();
                valsCell.appendChild(valsLabel);
            }
        }
    }
    return table;
}

// turn a json object into an html table
function dataToTable(label, data) {
    var subTable = document.createElement("table");
    subTable.classList.add("jsonTable");
    var subRow = subTable.insertRow();
    var newTableNode;
    if (label === "Variant Effect Predictor")
        newTableNode  = makeVepTable(data);
    else if (label === "Population Frequencies" || label === "Haplotype Frequencies")
        newTableNode  = makePopTable(data);
    else
        newTableNode  = makeGenericTable(data);
    return newTableNode ;
}
 
function makeVisInput(parentEl, name, trackName="", defaultVis="Hide") {
    ["Hide","Dense","Squish","Pack","Full"].forEach(function(vis) {
        let label = document.createElement("label");
        //label.classList.add(name);
        let ctrl = document.createElement("input");
        ctrl.classList.add(name);
        ctrl.type = "radio";
        ctrl.name = "chainHprc" + trackName;
        ctrl.value = vis.toLowerCase();
        if (defaultVis.toLowerCase() === vis.toLowerCase()) {
            ctrl.checked = true;
        }       
        ctrl.setAttribute("data-default", ctrl.checked);
        if (trackName.length > 0) {
            ctrl.setAttribute("data-trackName", trackName);
        }           
        label.appendChild(ctrl);
        label.append(vis);
        parentEl.append(label);
    }); 
}

function makeSetAllDiv(parentEl, text, classPre) {
    let textDiv = document.createElement("div");
    textDiv.append(text);
    textDiv.classList.add(classPre + "Text");
    textDiv.classList.add("gridItem");
    let ctrlDiv = document.createElement("div");
    makeVisInput(ctrlDiv, classPre + "Vis");
    ctrlDiv.classList.add(classPre + "Ctrl");
    ctrlDiv.classList.add("gridItem");
    parentEl.append(textDiv);
    parentEl.append(ctrlDiv);
}   

// assume the first row, second column of the first bedExtraTbl is a list of assemblies
// with corresponding chain tracks
function makeHPRCTable() {
    let tbl = $(".bedExtraTbl");
    if (tbl.length > 0) {
        tbl = tbl[0];
        let td = tbl.firstChild.firstChild.children[1];
        
        // get the list of assemblies
        asms = td.innerText;
        
        // clear the old text
        td.replaceChildren();
        
        let newForm = document.createElement("form");
        td.append(newForm);
        newForm.name = "chainBreak";
        newForm.action = "../cgi-bin/hgTracks";
        newForm.method = "POST";
        
        // the hidden hgsid for hgTracks
        let hgsidInput = document.createElement("input");
        hgsidInput.type = "hidden";
        hgsidInput.name = "hgsid";
        hgsidInput.value = common.hgsid;
        newForm.append(hgsidInput);
        
        newForm.innerHTML += "View tracks";
        let submitBtn = document.createElement("input");
        submitBtn.type = "submit";
        newForm.append(submitBtn);
        
        let newTblDiv = document.createElement("div");
        newTblDiv.classList.add("chainBreak");
        newForm.append(newTblDiv);
        setAllText = "Change display mode of all assembly chain tracks";
        makeSetAllDiv(newTblDiv, setAllText, "topSetAll");
        
        // go through and make each link
        asms.split(",").forEach(function(asm) {
            asmSafe = asm.replaceAll(".","v");
            let trackTextDiv = document.createElement("div");
            trackTextDiv.append(asmSafe + " display mode:");
            newTblDiv.append(trackTextDiv);
            let trackCtrlDiv = document.createElement("div");
            let defaultVis = "Hide";
            if (typeof chainVis !== "undefined" && asm in chainVis) {defaultVis = chainVis[asm];}
            makeVisInput(trackCtrlDiv, asmSafe+"SetVis", trackName=asmSafe, defaultVis=defaultVis);
            newTblDiv.append(trackCtrlDiv);
            trackTextDiv.classList.add("gridItem");
            trackCtrlDiv.classList.add("gridItem");
            // TODO: allow makeVisInput to take a default vis
            $("."+asmSafe+"SetVis").each(function(i, clickedElem) {
                clickedElem.addEventListener("click", function(e) {
                   $("[class$=SetAllVis]").each(function(i, radioElem) {
                        if (radioElem.checked) {
                            radioElem.checked = false;
                        }
                    });
                });
            });
        });
        if (asms.split(",").length > 25 ) {
            makeSetAllDiv(newTblDiv, setAllText, "bottomSetAll");
        }   
        $("[class$=SetAllVis]").each(function(i, elem) {
            elem.addEventListener("click", function(e) {
                // change vis of each track
                $("[class$=Vis][value="+e.target.value).each(function(i, e) {
                    e.checked = true;
                });
            });
        });
        newForm.addEventListener("submit", function(e) {
            inputs  = e.target.elements;
            for (let i = 0; i < inputs.length; i++) {
                input = inputs[i];
                if (!input.checked) {
                    // pass hgsid and other variables on through
                    continue;
                }
                if (input.name.endsWith("SetAllVis") || (input.getAttribute("data-default") === input.checked.toString())) {
                    input.disabled = true;
                } else {
                    input.value = input.value.toLowerCase();
                }
            }
        });
    }
}

const svgNS = "http://www.w3.org/2000/svg";
function getTextWidth(text) {
    let hiddenSvg = document.createElementNS(svgNS, "svg");
    hiddenSvg.style.visibility = "hidden";
    hiddenSvg.style.position = "absolute";
    document.body.appendChild(hiddenSvg);
    let textN = document.createElementNS(svgNS, "text");
    textN.setAttribute("x", 0);
    textN.setAttribute("y", 0);
    textN.textContent = text;
    hiddenSvg.appendChild(textN);
    let ret = textN.getComputedTextLength();
    document.body.removeChild(hiddenSvg);
    return ret;
}

function drawSvgTable(svg, data) {
    // Given an id of an svg (which has already been mostly created), fill it in
    // with the data in arr. data is an object with structure:
    // {metricLabel, sampleLabel, values: [
    //     {color: "", label: "", nValue: number, barValue: number},
    //     {...},
    //     ...
    // ]
    let parentTable = svg.parentNode;
    while (parentTable && parentTable.nodeName !== "TABLE") {
        parentTable = parentTable.parentNode;
    }
    let totalWidth = Math.round(parentTable.clientWidth * 0.95);
    let padding = 5;
    let hasSampleN = svg.getAttribute("hassamplen") === "true" || svg.getAttribute("hassamplen") === "TRUE";
    let labelWidth = 0;
    let nWidth = 0;
    for (let obj of data.values) {
        labelWidth = Math.max(labelWidth, getTextWidth(obj.label));
        if (hasSampleN) {
            if (typeof obj.nValue !== "undefined") {
                nWidth = Math.max(nWidth, getTextWidth(obj.nValue));
            }
        }
    }
    if (labelWidth > (totalWidth * 0.4)) {
        labelWidth = totalWidth * 0.4; // clamp at 40% of max width
    }
    labelWidth += padding; // for padding on each
    if (hasSampleN) {
        nWidth += (2 * padding);
    }

    // these values come from barChartClick.c:
    let heightPer = 17;
    let fontSize = 16;
    let borderSize = 1;
    let headerColor = "#d9e4f8";
    let swatchWidth = 15;
    let swatchColWidth = swatchWidth + padding;
    // find the total width leftover for the bar itself
    // the barValue is specified as %5.3f in barChartClick.c, so max of 8 chars
    let barWidth = totalWidth - (swatchColWidth + labelWidth + nWidth + getTextWidth("00000.000") + padding);

    let columnWidths = [swatchColWidth, labelWidth, nWidth, barWidth];

    // now we can draw the header
    let header = document.createElementNS(svgNS, "rect");
    header.id = "svgTableHeader";
    header.setAttribute("width", totalWidth);
    header.setAttribute("height", 20);
    header.setAttribute("fill", headerColor);
    svg.appendChild(header);
    let sampleColLabel = document.createElementNS(svgNS, "text");
    sampleColLabel.textContent = data.sampleLabel;
    sampleColLabel.setAttribute("x", swatchColWidth);
    sampleColLabel.setAttribute("y", heightPer - 1);
    sampleColLabel.setAttribute("font-size", fontSize);
    svg.appendChild(sampleColLabel);

    if (hasSampleN) {
        let nColLabel = document.createElementNS(svgNS, "text");
        nColLabel.textContent = "N";
        nColLabel.setAttribute("x", swatchColWidth + labelWidth);
        nColLabel.setAttribute("y", heightPer - 1);
        nColLabel.setAttribute("font-size", fontSize);
        svg.appendChild(nColLabel);
    }

    let metricColLabel = document.createElementNS(svgNS, "text");
    metricColLabel.textContent = data.metricLabel;
    metricColLabel.setAttribute("x", swatchColWidth + labelWidth + nWidth);
    metricColLabel.setAttribute("y", heightPer - 1);
    metricColLabel.setAttribute("font-size", fontSize);
    svg.appendChild(metricColLabel);
    svg.setAttribute("width", totalWidth);
    svg.setAttribute("height", (data.values.length * (heightPer + 1) + 20));

    // finally we can draw each row of the table
    let maxVal = data.values.reduce((a,b) => Math.max(a, b.barValue), -Infinity);
    for (let i = 0; i < data.values.length; ++i) {
        let textY = 20 + (i * (heightPer + 1)) + (heightPer / 2) + (fontSize / 3);
        let swatch = document.createElementNS(svgNS, "rect");
        swatch.setAttribute("x", 0);
        swatch.setAttribute("y", 20 + ((heightPer + 1) * i));
        swatch.setAttribute("width", swatchWidth);
        swatch.setAttribute("fill", data.values[i].color);
        swatch.setAttribute("height", heightPer);

        if (i & 1) {
            let band = document.createElementNS(svgNS, "rect");
            band.setAttribute("x", swatchColWidth);
            band.setAttribute("y", 20 + ((heightPer + 1) * i));
            band.setAttribute("fill", "#ffffff");
            band.setAttribute("width", labelWidth + nWidth);
            band.setAttribute("height", heightPer);
            svg.appendChild(band);
        }

        let label = document.createElementNS(svgNS, "text");
        label.setAttribute("x", swatchColWidth);
        label.setAttribute("y", textY);
        label.setAttribute("font-size", fontSize);
        label.textContent = data.values[i].label;

        if (typeof data.values[i].nValue !== "undefined") {
            let optN = document.createElementNS(svgNS, "text");
            optN.setAttribute("x", swatchColWidth + labelWidth);
            optN.setAttribute("y", textY);
            optN.setAttribute("font-size", fontSize);
            optN.textContent = data.values[i].nValue;
            svg.appendChild(optN);
        }

        let bar = document.createElementNS(svgNS, "rect");
        bar.setAttribute("x", swatchColWidth + labelWidth + nWidth);
        bar.setAttribute("y", 20 + ((heightPer + 1) * i));
        let thisBarWidth = barWidth * data.values[i].barValue / maxVal;
        bar.setAttribute("width", thisBarWidth);
        bar.setAttribute("height", heightPer);
        bar.setAttribute("fill", data.values[i].color);

        let barVal = document.createElementNS(svgNS, "text");
        barVal.setAttribute("x", swatchColWidth + labelWidth + nWidth + thisBarWidth + padding);
        barVal.setAttribute("y", textY);
        barVal.setAttribute("font-size", fontSize);
        barVal.textContent = data.values[i].barValue;
        svg.appendChild(swatch);
        svg.appendChild(label);
        svg.appendChild(bar);
        svg.appendChild(barVal);
    }
}


function initPage() {
    if (typeof doHPRCTable !== "undefined") {
        makeHPRCTable();
    }
    if (typeof svgTable !== "undefined") {
        // redraw the svg with appropriate widths for all columns
        // swatchWidth and columnSpacer are taken from svgBarChart() in hgc/barChartClick.c
        // they should probably be dynamically determined
        drawSvgTable(document.getElementById("svgBarChart"), barChartValues);
    }
    if (typeof _jsonHgcLabels !== "undefined") {
        var obj, o;
        for (obj in _jsonHgcLabels) {
            // build up the new table:
            var newTable = document.createElement("table");
            var newRow = newTable.insertRow();
            var newCell = newRow.insertCell();
            var label = _jsonHgcLabels[obj].label;
            var data = _jsonHgcLabels[obj].data;
            var newText = document.createTextNode(label);
            newCell.appendChild(newText);
            newCell = newRow.insertCell();
            newCell.appendChild(dataToTable(label, data));
            // find the last details table and add a new table on:
            var currTbl = $(".bedExtraTbl");
            l = currTbl.length;
            var last = currTbl[l-1];
            insertAfter(newTable, last);
            newTable.classList.add("bedExtraTbl");
            last.parentNode.insertBefore(document.createElement("br"), newTable);
        }
    }
}

// Export a way to call the document.ready() functions after ajax
var hgc = {initPage: initPage};

// on page load initialize VEP, Population Frequency and Haplotype Tables
// for gnomAD v3.1.1 track
$(document).ready(function() {
    initPage();
});
