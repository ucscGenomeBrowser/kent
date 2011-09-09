//encodeDataSummary.js - pull experiment table and metadata from server 
//      and display in summary tables
// Formatted: jsbeautify.py -j
// Syntax checked: jslint indent:4, plusplus: true, continue: true, unparam: true, sloppy: true, browser: true */
/*global $, encodeProject */

$(function () {
    var requests = [
    // Requests to server API
    encodeProject.serverRequests.experiment, encodeProject.serverRequests.dataType, encodeProject.serverRequests.antibody],
        selectedDataType = null,
        dataTypeLabelHash = {};

    function tableOut(table, types, exps, selectableData) {
        // Helper function to output tables to document
        var total = 0,
            row = 0,
            assembly = $("#var_assembly").val();

        // lay out table
        $.each(exps, function (key, value) {
            types.push(key);
            total += parseInt(value, 10);
        });
        types.sort();
        $.each(types, function (i, value) {
            if (dataTypeLabelHash[value]) {
                description = dataTypeLabelHash[value].description;
            } else {
                description = '';
            }
            // quote the end tags so HTML validator doesn't whine
            $(table).append("<tr class='" +
                (row % 2 === 0 ? "even" : "odd") +
                "'><td title='" + description + "'>" + value + "<\/td><td id='" + value + "' class='dataItem' title='Click to search for " + value + " data'>" + exps[value] + "<\/td><\/tr>");
            row++;
        });
        if (selectableData) {
        // TODO: suppress 'Click' title for non-selectables
        //if (!selectableData) {
            //$(".dataItem").removeAttr("title");
        //} else {
            // set up search buttons, initially disabled (must select data to enable)
            $(".searchButton").attr("disabled", "true");
            $("#buttonTrackSearch").click(function () {
                // TODO: base on preview
                window.location = "/cgi-bin/hgTracks?db=" + assembly + "&tsCurTab=advancedTab&hgt_tsPage=&hgt_tSearch=search&hgt_mdbVar1=dataType&hgt_mdbVal1=" + selectedDataType;
            });
            $("#buttonFileSearch").click(function () {
                // TODO: base on preview
                window.location = "/cgi-bin/hgFileSearch?db=" + assembly + "&tsCurTab=advancedTab&hgt_tsPage=&hgt_tSearch=search&hgt_mdbVar1=dataType&hgt_mdbVal1=" + selectedDataType;
            });

            // set up selectability on data types
            $(".dataItem").addClass("selectable");
            $(".dataItem").click(function () {
                if (selectedDataType === null) {
                    $(this).addClass("selected");
                    selectedDataType = $(this).attr("id");
                    $(".searchButton").removeAttr("disabled");
                } else {
                    if ($(this).hasClass("selected")) {
                        selectedDataType = null;
                        $(this).removeClass("selected");
                        $(".searchButton").attr("disabled", "true");
                    } else {
                        $(".selected").removeClass("selected");
                        $(this).addClass("selected");
                    }
                }
            });
        }
        $(table).append("<tr><th>Total: " + types.length + "<\/th><th>" + total + "<\/th><\/tr>");
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
        organism = $("#var_organism").val();
        assembly = $("#var_assembly").val();
        header = $("#var_pageHeader").val();

        $("#pageHeader").text(header);
        $("title").text('ENCODE ' + header);

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
        tableOut("#elementTable", elementTypes, cellAssayExps, false);
        $("#buttonDataMatrix").click(function () {
            window.location = "encodeDataMatrixHuman.html";
        });
        tableOut("#tfbsTable", tfbsTypes, tfbsExps, false);
        $("#buttonChipMatrix").click(function () {
            window.location = "encodeChipMatrixHuman.html";
        });
    }

    // initialize
    encodeProject.setup({
        // todo: add hidden page variable for server
        server: "hgwdev-kate.cse.ucsc.edu"
        //server: "genome-preview.ucsc.edu"
    });
    encodeProject.loadAllFromServer(requests, handleServerData);
});
