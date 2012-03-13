/* encodeDataSummary.js - pull experiment table and metadata from server 
      and display in summary tables

 Formatted: jsbeautify.py -j -k
 Syntax checked: jslint indent:4, plusplus: true, continue: true, unparam: true, sloppy: true, browser: true
*/
/*global $, encodeProject */

$(function () {
    var requests = [
            // requests to server API
            encodeProject.serverRequests.experiment,
            encodeProject.serverRequests.dataType,
            encodeProject.serverRequests.antibody,
            encodeProject.serverRequests.expId
            ];

    var $summaryTables = $('.summaryTable');

    function handleServerData(responses) {
        // Main actions, called when loading data from server is complete
        var experiments = responses[0], 
            dataTypes = responses[1], 
            antibodies = responses[2], 
            expIds = responses[3];

        var cellAssayExps = {}, tfbsExps = {},  refGenomeExps = {};
        var refGenomeTypes = [], elementTypes = [], tfbsTypes = [];
        var dataType, antibody, target;

        encodeMatrix.show($summaryTables);

        antibodyGroups = encodeProject.getAntibodyGroups(antibodies);
        encodeProject.getDataGroups(dataTypes);

        // use to filter out experiments not in this assembly
        expIdHash = encodeProject.getExpIdHash(expIds);

        $.each(experiments, function (i, exp) {
            // experiment not in this assembly
            if (expIdHash[exp.ix] === undefined) {
                return true;
            }
            antibody = encodeProject.antibodyFromExp(exp);
            if (antibody) {
                target = encodeProject.targetFromAntibody(antibody);
            }
            // add experiments into the appropriate table object
            if (exp.cellType === 'None') {
                dataType = encodeProject.getDataType(exp.dataType);
                if (dataType !== undefined) {
                    dataType = dataType.label;
                } else {
                    dataType = exp.dataType;
                }
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
                dataType = encodeProject.getDataType(exp.dataType);
                if (dataType !== undefined) {
                    dataType = dataType.label;
                } else {
                    dataType = exp.dataType;
                }
                if (!cellAssayExps[dataType]) {
                    cellAssayExps[dataType] = 0;
                }
                cellAssayExps[dataType]++;
            }
        });
        // fill in tables and activate buttons
        tableOut('#refGenomeTable', refGenomeTypes, refGenomeExps, false);
        tableOut('#elementTable', elementTypes, cellAssayExps, false);
        $('#buttonDataMatrix').click(function () {
            window.location = 'encodeDataMatrixHuman.html';
        });
        // TODO: enable selectable items in antibody table
        tableOut('#tfbsTable', tfbsTypes, tfbsExps, true);
        $('#buttonChipMatrix').click(function () {
            window.location = 'encodeChipMatrixHuman.html';
        });

        // add row highlight
        $('.summaryTable').delegate('.even, .odd', 'mouseover mouseleave', function (ev) {
            if (ev.type == 'mouseover') {
                $(this).addClass('rowHighlight');
            } else {
                $(this).removeClass('rowHighlight');
            }
        });
    }

    function tableOut(table, types, exps, isChipSeq) {
        // Helper function to output tables to document
        var total = 0, row = 0;
        var dataType, antibodyTarget;

        $.each(exps, function (key, value) {
            types.push(key);
            total += parseInt(value, 10);
        });
        types.sort(encodeProject.cmpNoCase);

        // lay out table
        $.each(types, function (i, value) {
            description = '';
            if (isChipSeq) {
                antibodyTarget = encodeProject.getAntibodyTarget(value);
                if (antibodyTarget !== undefined) {
                    description = antibodyTarget.description;
                }
            } else {
                dataType = encodeProject.getDataTypeByLabel(value);
                if (dataType !== undefined) {
                    description = dataType.description;
                }
            }
            // quote the end tags so HTML validator doesn't whine
            $(table).append("<tr class='" + (row % 2 === 0 ? "even" : "odd") + "'><td title='" + description + "'>" + value + "<\/td><td id='" + value + "' class='dataItem' title='Click to search for " + value + " data'>" + exps[value] + "<\/td><\/tr>");
            row++;
        });

        /* $(".dataItem").addClass("selectable"); */
        $(".even, .odd").click(function () {
            // TODO: base on preview ?
            var url = encodeMatrix.getSearchUrl(encodeProject.getAssembly());
            if (isChipSeq) {
                target = $(this).children('.dataItem').attr("id");
                url += '&hgt_mdbVar1=antibody';
                antibodyTarget = encodeProject.getAntibodyTarget(target);
                $.each(antibodyTarget.antibodies, function (i, antibody) {
                    url += '&hgt_mdbVal1=' + antibody;
                });
            } else {
                dataType = $(this).children('.dataItem').attr("id");
                url += '&hgt_mdbVar1=dataType&hgt_mdbVal1=' + dataType;
            }
            url += '&hgt_mdbVar2=view&hgt_mdbVal2=Any';
            // TODO: open search window 
            window.open(url, "searchWindow");
            //window.location = url;
        });

        $(table).append("<tr><td class='totals'>Total: " + types.length + "<\/td><td class='totals'>" + total + "<\/td><\/tr>");
        if (total === 0) {
            $(table).remove();
        }
    }

    // initialize

    encodeMatrix.start($summaryTables);

    // load data from server
    encodeProject.loadAllFromServer(requests, handleServerData);
});

