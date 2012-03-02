/* encodeDataMatrix.js - ENCODE Data Matrix application

      Pulls experiment table and metadata from server 
      and displays in matrix of assay vs. cell type

 NOTE: $variables are jQuery objects

 Formatted: jsbeautify.py -j
 Syntax checked: jslint indent:4, plusplus: true, continue: true, unparam: true, sloppy: true, browser: true
*/
/*global $, encodeProject */

$(function () {
    var requests = [
    // requests to server API
        encodeProject.serverRequests.experiment, 
        encodeProject.serverRequests.dataType, 
        encodeProject.serverRequests.cellType,
        encodeProject.serverRequests.expId
        ];

    var $matrixTable = $('#matrixTable');

    function handleServerData(responses) {
        // Main actions, called when loading data from server is complete

        // NOTE: ordering of responses is based on request order
        var experiments = responses[0], 
                          dataTypes = responses[1], 
                          cellTypes = responses[2], 
                          expIds = responses[3];

        var dataGroups, cellTiers, expIdHash;
        var dataType, cellType;
        var matrix, dataTypeExps = {};

        // hide spinner and show table
        encodeMatrix.show($matrixTable);

        // set up structures for data types and their groups
        // data type labels tucked into their tiers
        dataGroups = encodeProject.getDataGroups(dataTypes);

        // set up structures for cell types and their tiers
        cellTiers = encodeProject.getCellTiers(cellTypes);

        // use to filter out experiments not in this assembly
        expIdHash = encodeProject.getExpIdHash(expIds);

        // gather experiments into matrix
        // NOTE: dataTypeExps is populated here
        matrix = makeExperimentMatrix(experiments, expIdHash, dataTypeExps);

        // fill in table using matrix
        encodeMatrix.tableOut($matrixTable, matrix, cellTiers, 
                    dataGroups, dataTypeExps, tableHeaderOut, rowAddCells);
    }

    function makeExperimentMatrix(experiments, expIdHash, dataTypeExps) {
        // Populate dataType vs. cellType array with counts of experiments

        var dataType, cellType;
        var matrix = {};

        $.each(experiments, function (i, exp) {
            // exclude ref genome annotations
            if (exp.cellType === 'None') {
                return true;
            }
            // exclude experiments lacking an expID (not in this assembly)
            if (expIdHash[exp.ix] === undefined) {
                return true;
            }
            // count experiments per dataType so we can prune those having none
            // (the matrix[cellType] indicates this for cell types 
            // so don't need hash for those
            dataType = exp.dataType;
            if (dataTypeExps[dataType] === undefined) {
                dataTypeExps[dataType] = 0;
            }
            dataTypeExps[dataType]++;

            cellType = exp.cellType;
            if (!matrix[cellType]) {
                matrix[cellType] = {};
            }
            if (!matrix[cellType][dataType]) {
                matrix[cellType][dataType] = 0;
            }
            matrix[cellType][dataType]++;
        });
    return matrix;
    }

    function tableHeaderOut($table, dataGroups, dataTypeExps) {
        // Generate table header and add to document
        // NOTE: relies on hard-coded classes and ids

        var $tableHeader, $thead;
        var maxLen, dataType;

        // fill in column headers from dataTypes returned by server
        $tableHeader = $('#columnHeaders');
        $thead = $('thead');

        // 1st column is row headers
        // colgroups are needed to support cross-hair hover effect
        $thead.before('<colgroup></colgroup>');

        $.each(dataGroups, function (i, group) {
            $tableHeader.append('<th class="groupType"><div class="verticalText">' + 
                                group.label + '</div></th>');
            maxLen = Math.max(maxLen, group.label.length);
            $thead.before('<colgroup></colgroup>');
            $.each(group.dataTypes, function (i, label) {
                dataType = encodeProject.getDataTypeByLabel(label);

                // prune out datatypes with no experiments
                if (dataTypeExps[dataType.term] !== undefined) {
                    $tableHeader.append('<th class="elementType" title="' + 
                                        dataType.description + 
                                        '"><div class="verticalText">' + dataType.label + 
                                        '</div></th>');
                    // add colgroup element to support cross-hair hover effect
                    $thead.before('<colgroup class="experimentCol"></colgroup>');
                    maxLen = Math.max(maxLen, dataType.label.length);
                }
            });
        });
        // adjust size of headers based on longest label length
        // empirically len/2 em's is right
        $('#columnHeaders th').css('height', (String((maxLen/2 + 2)).concat('em')));
        $('#columnHeaders th').css('width', '1em');
    }

    function rowAddCells($row, dataGroups, dataTypeExps, matrix, cellType) {
        // populate a row in the matrix with cells for data groups and data types
        // null cellType indicates this is a row for a cell group (tier)

        var $td;

        $.each(dataGroups, function (i, group) {
            // skip group header
            $td = $('<td></td>');
            $td.addClass('matrixCell');
            $row.append($td);

            $.each(group.dataTypes, function (i, dataTypeLabel) {
                dataType = encodeProject.getDataTypeByLabel(dataTypeLabel).term;
                // prune out datatypes with no experiments
                if (dataTypeExps[dataType] === undefined) {
                    return true;
                }
                $td = $('<td></td>');
                $td.addClass('matrixCell');
                $row.append($td);
                if (cellType === null) {
                    return true;
                }
                if (!matrix[cellType][dataType]) {
                    $td.addClass('todoExperiment');
                    return true;
                }
                // this cell represents experiments that
                // fill in count, mouseover and selection by click
                $td.addClass('experiment');
                $td.text(matrix[cellType][dataType]);
                $td.data({
                    'dataType' : dataType,
                    'cellType' : cellType
                });
                $td.mouseover(function() {
                    $(this).attr('title', 'Click to select: ' + 
                        encodeProject.getDataType($(this).data().dataType).label +
                            ' ' + ' in ' + $(this).data().cellType +' cells');
                });
                $td.click(function() {
                    var url = encodeMatrix.getSearchUrl(encodeProject.getAssembly());
                    // TODO: encapsulate var names
                    url +=
                       ('&hgt_mdbVar1=dataType&hgt_mdbVal1=' + $(this).data().dataType +
                       '&hgt_mdbVar2=cell&hgt_mdbVal2=' + $(this).data().cellType +
                       '&hgt_mdbVar3=view&hgt_mdbVal3=Any');
                    // specifying window name limits open window glut
                    window.open(url, "searchWindow");
                });
            });
        });
    }

    // initialize application
    encodeMatrix.start($matrixTable);

    // load data from server and do callback
    encodeProject.loadAllFromServer(requests, handleServerData);
});
