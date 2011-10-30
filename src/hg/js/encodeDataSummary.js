/* encodeDataSummary.js - pull experiment table and metadata from server 
      and display in summary tables

 Formatted: jsbeautify.py -j -k
 Syntax checked: jslint indent:4, plusplus: true, continue: true, unparam: true, sloppy: true, browser: true
*/
/*global $, encodeProject */

$(function () {
    var selectedDataType = null,
        dataTypeLabelHash = {},
        server, requests = [
            // Requests to server API
                    encodeProject.serverRequests.experiment,
            encodeProject.serverRequests.dataType,
            encodeProject.serverRequests.antibody];

    function tableOut(table, types, exps, selectableData) {
        // Helper function to output tables to document
        var total = 0,
            row = 0,
            assembly = encodeDataSummary_assembly;

        $.each(exps, function (key, value) {
            types.push(key);
            total += parseInt(value, 10);
        });
        types.sort();

        // lay out table
        $.each(types, function (i, value) {
            if (dataTypeLabelHash[value]) {
                description = dataTypeLabelHash[value].description;
            } else {
                description = '';
            }
            // quote the end tags so HTML validator doesn't whine
            $(table).append("<tr class='" + (row % 2 === 0 ? "even" : "odd") + "'><td title='" + description + "'>" + value + "<\/td><td id='" + value + "' class='dataItem' title='Click to search for " + value + " data'>" + exps[value] + "<\/td><\/tr>");
            row++;
        });

        if (selectableData) {
            $(".dataItem").addClass("selectable");
            $(".dataItem").click(function () {
                // TODO: base on preview ?
                var term = dataTypeLabelHash[$(this).attr("id")].term;
                var url = encodeProject.getSearchUrl(assembly);
                url +=
                   ('&hgt_mdbVar1=dataType&hgt_mdbVal1=' + term +
                   '&hgt_mdbVar2=view&hgt_mdbVal2=Any');
                // TODO: open search window 
                //window.open(url, "searchWindow");
                window.location = url;
                // TODO:  if antibody table, add mdbVar2 and mdbVal2
                // TODO: same for histones
            });
        }

        $(table).append("<tr><td class='totals'>Total: " + types.length + "<\/td><td class='totals'>" + total + "<\/td><\/tr>");
        if (total === 0) {
            $(table).remove();
        }
    }

    function handleServerData(responses) {
        // Main actions, called when loading data from server is complete
        var experiments = responses[0],
            dataTypes = responses[1],
            antibodies = responses[2],
            antibodyHash = {},
            dataTypeHash = {},
            refGenomeExps = {},
            cellAssayExps = {},
            tfbsExps = {},
            antibody, target, dataType, total, refGenomeTypes = [],
            elementTypes = [],
            tfbsTypes = [],
            organism, assembly, header;

        // variables passed in hidden fields
        organism = encodeDataSummary_organism;
        assembly = encodeDataSummary_assembly;
        header = encodeDataSummary_pageHeader;

        hideLoadingImage(spinner);
        $('.summaryTable').show();
        $('#searchTypePanel').show();

        $("#pageHeader").text(header);
        document.title = 'ENCODE ' + header;

        $.each(antibodies, function (i, item) {
            antibodyHash[item.term] = item;
        });
        $.each(dataTypes, function (i, item) {
            dataTypeHash[item.term] = item;
            dataTypeLabelHash[item.label] = item;
        });

        $.each(experiments, function (i, exp) {
            // todo: filter out with arg to hgApi
            if (exp.organism !== organism) {
                return true;
            }
            antibody = encodeProject.antibodyFromExp(exp);
            if (antibody) {
                target = encodeProject.targetFromAntibody(antibody, antibodyHash);
            }
            // add experiments into the appropriate table object
            if (exp.cellType === 'None') {
                dataType = dataTypeHash[exp.dataType].label;
                if (!refGenomeExps[dataType]) {
                    refGenomeExps[dataType] = 0;
                }
                refGenomeExps[dataType]++;
            } else if (exp.dataType === 'ChipSeq') {
                if (!target) {
                    return true;
                }
                if (target.match(/^H[234]/)) {
                    // histone mark 
                    dataType = 'Histone ' + target;
                    if (!cellAssayExps[dataType]) {
                        cellAssayExps[dataType] = 0;
                    }
                    cellAssayExps[dataType]++;
                } else {
                    if (!tfbsExps[target]) {
                        tfbsExps[target] = 0;
                    }
                    tfbsExps[target]++;
                }
            } else {
                dataType = dataTypeHash[exp.dataType].label;
                if (!cellAssayExps[dataType]) {
                    cellAssayExps[dataType] = 0;
                }
                cellAssayExps[dataType]++;
            }
        });

        // fill in tables and activate buttons
        tableOut("#refGenomeTable", refGenomeTypes, refGenomeExps, true);
        tableOut("#elementTable", elementTypes, cellAssayExps, true);
        $("#buttonDataMatrix").click(function () {
            window.location = "encodeDataMatrixHuman.html";
        });
        // TODO: enable selectable items in antibody table
        tableOut("#tfbsTable", tfbsTypes, tfbsExps, false);
        $("#buttonChipMatrix").click(function () {
            window.location = "encodeChipMatrixHuman.html";
        });
    }
    // get server from calling web page (intended for genome-preview)
    if ('encodeDataMatrix_server' in window) {
        server = encodeDataMatrix_server;
    } else {
        server = document.location.hostname;
        // or document.domain ?
    }
    // initialize
    encodeProject.setup({
        server: server
    });

    // show only spinner until data is retrieved
    spinner = showLoadingImage("spinner");

    // add radio buttons for search type to specified div on page
    encodeProject.addSearchPanel('#searchTypePanel');
    $('#searchTypePanel').hide();
    $('.summaryTable').hide();

    // load data from server
    encodeProject.loadAllFromServer(requests, handleServerData);
});
