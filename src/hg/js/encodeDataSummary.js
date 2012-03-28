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

    function addDataType(dataTypeName, expList, isChip) {
        // Helper function to fill datatype lists that are used to make tables

        var dataType, dataTypeLabel;

        if (!isChip) {
            // get data type label
            dataType = encodeProject.getDataType(dataTypeName);
            if (dataType !== undefined) {
                dataTypeLabel = dataType.label;
            }
        }
        if (dataTypeLabel === undefined) {
            // if there is a mismatch between experiment table and CV we might not
            // find dataType for the experiment
            dataTypeLabel = dataTypeName;
        }
        if (!expList[dataTypeLabel]) {
            expList[dataTypeLabel] = 0;
        }
        expList[dataTypeLabel]++;
    }

    function handleServerData(responses) {
        // Main actions, called when loading data from server is complete
        var experiments = responses[0], 
            dataTypes = responses[1], 
            antibodies = responses[2], 
            expIds = responses[3];

        var cellAssayExps = {}, tfbsExps = {},  refGenomeExps = {};
        var refGenomeTypes = [], elementTypes = [], tfbsTypes = [];
        var antibody, dataType;

        encodeMatrix.show($summaryTables);

        antibodyGroups = encodeProject.getAntibodyGroups(antibodies);
        encodeProject.getDataGroups(dataTypes);

        // use to filter out experiments not in this assembly
        expIdHash = encodeProject.getExpIdHash(expIds);

        $.each(experiments, function (i, exp) {
            // exlude experiment not in this assembly
            if (expIdHash[exp.ix] === undefined) {
                return true;
            }
            if (exp.dataType === undefined) {
                return true;
            }
            // add experiment into the appropriate list(s)
            if (exp.cellType === 'None') {
                addDataType(exp.dataType, refGenomeExps, false);
            } else {
                addDataType(exp.dataType, cellAssayExps, false);
            }
            if (exp.dataType === 'ChipSeq') {
                antibody = encodeProject.antibodyFromExp(exp);
                if (!antibody) {
                    return true;
                }
                dataType = encodeProject.targetFromAntibody(antibody);
                if (!dataType) {
                    // this excludes controls
                    return true;
                }
                addDataType(dataType, tfbsExps, true);
            }
        });
        // work-around for some supplementary files being accessioned as experiments (5C)
        // they show up in both reference genome and cell assay lists incorrectly
        // remove them from refGenome list of they are in cellAssayExps
        for (dataType in refGenomeExps) {
            if (cellAssayExps[dataType] !== undefined) {
                delete refGenomeExps[dataType];
            }
        }
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
        var description, term;

        $.each(exps, function (key, value) {
            types.push(key);
            total += parseInt(value, 10);
        });
        types.sort(encodeProject.cmpNoCase);

        // lay out table
        $.each(types, function (i, value) {
            description = '';
            term = '';
            if (isChipSeq) {
                antibodyTarget = encodeProject.getAntibodyTarget(value);
                if (antibodyTarget !== undefined) {
                    description = antibodyTarget.description;
                    term = value;
                }
            } else {
                dataType = encodeProject.getDataTypeByLabel(value);
                if (dataType !== undefined) {
                    description = dataType.description;
                    term = dataType.term;
                }
            }
            // quote the end tags so HTML validator doesn't whine
            $(table).append("<tr class='" + (row % 2 === 0 ? "even" : "odd") + "'><td title='" + description + "'>" + value + "<\/td><td id='" + term + "' class='dataItem' title='Click to search for " + value + " data'>" + exps[value] + "<\/td><\/tr>");
            row++;
        });

        $(".even, .odd").click(function () {
            var dataType, target, url, antibodyTarget;
            // NOTE: generating full search URL should be generalized & encapsulated
            url = encodeMatrix.getSearchUrl(encodeProject.getAssembly());
            if ($(this).parents('table').attr('id') === 'tfbsTable') {
                target = $(this).children('.dataItem').attr('id');
                url += '&hgt_mdbVar1=antibody';
                antibodyTarget = encodeProject.getAntibodyTarget(target);
                $.each(antibodyTarget.antibodies, function (i, antibody) {
                    url += '&hgt_mdbVal1=' + antibody;
                });
            } else {
                dataType = $(this).children('.dataItem').attr('id');
                url += '&hgt_mdbVar1=dataType&hgt_mdbVal1=' + dataType;
            }
            url += '&hgt_mdbVar2=view&hgt_mdbVal2=Any';
            // remove extra rows
            url += '&hgt_mdbVar3=[]';
            url += '&hgt_mdbVar4=[]';
            url += '&hgt_mdbVar5=[]';
            url += '&hgt_mdbVar6=[]';
            window.open(url, "searchWindow");
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

