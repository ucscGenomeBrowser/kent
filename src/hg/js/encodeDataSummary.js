/* encodeDataSummary.js - pull experiment table and metadata from server 
      and display in summary tables

 Formatted: jsbeautify.py -j -k
 Syntax checked: jslint indent:4, plusplus: true, continue: true, unparam: true, sloppy: true, browser: true
*/
/*global $, encodeProject */

$(function () {
    var dataTypeLabelHash = {}, targetHash = {};
    var server, organism, assembly, header;
    var spinner;
    var requests = [
            // requests to server API
            encodeProject.serverRequests.experiment,
            encodeProject.serverRequests.dataType,
            encodeProject.serverRequests.antibody,
            encodeProject.serverRequests.expId
            ];

    function tableOut(table, types, exps, isChipSeq) {
        // Helper function to output tables to document
        var total = 0, row = 0;

        $.each(exps, function (key, value) {
            types.push(key);
            total += parseInt(value, 10);
        });
        types.sort(encodeProject.cmpNoCase);

        // lay out table
        $.each(types, function (i, value) {
            description = '';
            if (isChipSeq) {
                if (targetHash[value] !== undefined)
                    description = targetHash[value].description;
            } else {
                if (dataTypeLabelHash[value] !== undefined) {
                    description = dataTypeLabelHash[value].description;
                }
            }
            // quote the end tags so HTML validator doesn't whine
            $(table).append("<tr class='" + (row % 2 === 0 ? "even" : "odd") + "'><td title='" + description + "'>" + value + "<\/td><td id='" + value + "' class='dataItem' title='Click to search for " + value + " data'>" + exps[value] + "<\/td><\/tr>");
            row++;
        });

        $(".dataItem").addClass("selectable");
        $(".dataItem").click(function () {
            // TODO: base on preview ?
            var url = encodeProject.getSearchUrl(assembly);
            if (isChipSeq) {
                target = $(this).attr("id");
                url += '&hgt_mdbVar1=antibody';
                $.each(targetHash[target].antibodies, function (i, antibody) {
                    url += '&hgt_mdbVal1=' + antibody;
                });
            } else {
                dataType = dataTypeLabelHash[$(this).attr("id")].term;
                url += '&hgt_mdbVar1=dataType&hgt_mdbVal1=' + dataType;
            }
            url += '&hgt_mdbVar2=view&hgt_mdbVal2=Any';
            // TODO: open search window 
            //window.open(url, "searchWindow");
            window.location = url;
        });

        $(table).append("<tr><td class='totals'>Total: " + types.length + "<\/td><td class='totals'>" + total + "<\/td><\/tr>");
        if (total === 0) {
            $(table).remove();
        }
    }

    function handleServerData(responses) {
        // Main actions, called when loading data from server is complete
        var experiments = responses[0], dataTypes = responses[1], 
                        antibodies = responses[2], expIds = responses[3];
        var antibodyHash = {}, dataTypeHash = {}, 
                cellAssayExps = {}, tfbsExps = {},  refGenomeExps = {};
        var refGenomeTypes = [], elementTypes = [], tfbsTypes = [];
        var dataType, antibody, target;


        hideLoadingImage(spinner);
        $('.summaryTable').show();
        $('#searchTypePanel').show();

        $("#pageHeader").text(header);
        document.title = 'ENCODE ' + header;

        $.each(antibodies, function (i, antibody) {
            antibodyHash[antibody.term] = antibody;
            target = antibody.target;
            if (targetHash[target] === undefined) {
                targetHash[target] = {
                    count: 0,   // experiments
                    description: antibody.targetDescription,
                    antibodies: []
                };
            }
            targetHash[target].antibodies.push(antibody.term)
        });
        antibodyGroups = encodeProject.getAntibodyGroups(antibodies);

        $.each(dataTypes, function (i, item) {
            dataTypeHash[item.term] = item;
            dataTypeLabelHash[item.label] = item;
        });

        // use to filter out experiments not in this assembly
        expIdHash = encodeProject.getExpIdHash(expIds);

        $.each(experiments, function (i, exp) {
            // todo: filter out with arg to hgApi
            if (exp.organism !== organism) {
                return true;
            }
            // experiment not in this assembly
            if (expIdHash[exp.ix] === undefined) {
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
                if (!tfbsExps[target]) {
                    tfbsExps[target] = 0;
                }
                tfbsExps[target]++;
            } else {
                dataType = dataTypeHash[exp.dataType].label;
                if (!cellAssayExps[dataType]) {
                    cellAssayExps[dataType] = 0;
                }
                cellAssayExps[dataType]++;
            }
        });

        // fill in tables and activate buttons
        tableOut("#refGenomeTable", refGenomeTypes, refGenomeExps, false);
        tableOut("#elementTable", elementTypes, cellAssayExps, false);
        $("#buttonDataMatrix").click(function () {
            window.location = "encodeDataMatrixHuman.html";
        });
        // TODO: enable selectable items in antibody table
        tableOut("#tfbsTable", tfbsTypes, tfbsExps, true);
        $("#buttonChipMatrix").click(function () {
            window.location = "encodeChipMatrixHuman.html";
        });
    }
    // initialize

    // get server from calling web page (intended for genome-preview)
    if ('encodeDataMatrix_server' in window) {
        server = encodeDataMatrix_server;
    } else {
        server = document.location.hostname;
        // or document.domain ?
    }

    // variables from calling page
    organism = encodeDataSummary_organism;
    assembly = encodeDataSummary_assembly;
    $("#assemblyLabel").text(assembly);
    header = encodeDataSummary_pageHeader;
    $("#pageHeader").text(header);
    document.title = 'ENCODE ' + header;

    encodeProject.setup({
        server: server,
        assembly: assembly
    });

    // add radio buttons for search type to specified div on page
    encodeProject.addSearchPanel('#searchTypePanel');

    // show only spinner until data is retrieved
    $('#searchTypePanel').hide();
    $('.summaryTable').hide();
    spinner = showLoadingImage("spinner");

    // load data from server
    encodeProject.loadAllFromServer(requests, handleServerData);
});
