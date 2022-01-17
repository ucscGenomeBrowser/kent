// "use strict";

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
                cell = row.insertCell();
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

// on page load initialize VEP, Population Frequency and Haplotype Tables
// for gnomAD v3.1.1 track
$(document).ready(function() {
    if (_jsonHgcLabels !== null) {
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
});
