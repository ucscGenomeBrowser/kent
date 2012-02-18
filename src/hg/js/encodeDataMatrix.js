/* encodeDataMatrix.js - pull experiment table and metadata from server 
      and display in data matrix

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

    var dataTypeLabelHash = {}, dataTypeTermHash = {}, cellTypeHash = {};
    var dataType, cellType;
    var organism, assembly, server, header;
    var karyotype;
    var spinner;

    function rowAddCells(row, dataGroups, matrix, cellType) {
        // populate a row in the matrix with cells for data groups and data types

        $.each(dataGroups, function (i, group) {
            // skip group header
            td = $('<td></td>');
            td.addClass('matrixCell');
            row.append(td);
            $.each(group.dataTypes, function (i, dataTypeLabel) {
                dataType = dataTypeLabelHash[dataTypeLabel].term;
                // prune out datatypes with no experiments
                if (dataTypeLabelHash[dataTypeLabel].count === undefined) {
                    return true;
                }
                td = $('<td></td>');
                td.addClass('matrixCell');
                row.append(td);
                if (cellType === null) {
                    return true;
                }
                if (!matrix[cellType][dataType]) {
                    td.addClass('todoExperiment');
                    return true;
                }
                // fill in count, mouseover and selection by click
                td.addClass('experiment');
                td.text(matrix[cellType][dataType]);
                td.data({
                    'dataType' : dataType,
                    'cellType' : cellType
                });
                td.mouseover(function() {
                    $(this).attr('title', 'Click to select: ' + 
                                    dataTypeTermHash[$(this).data().dataType].label +
                                    ' ' + ' in ' + 
                                    $(this).data().cellType +' cells');
                });
                td.click(function() {
                   // TODO: base on preview ?
                    var url = encodeProject.getSearchUrl(assembly);
                    // TODO: encapsulate var names
                    url +=
                       ('&hgt_mdbVar1=dataType&hgt_mdbVal1=' + $(this).data().dataType +
                       '&hgt_mdbVar2=cell&hgt_mdbVal2=' + $(this).data().cellType +
                       '&hgt_mdbVar3=view&hgt_mdbVal3=Any');
                    window.open(url, "searchWindow");
                });
            });
        });
    }

    function addFloatingHeader(table) {
        // add callback for floating table header feature

        table.floatHeader({
            cbFadeIn: function (header) {
                // hide axis labels -- a bit tricky to do so
                // as special handling needed for X axis label
                $(".floatHeader #headerLabelRow").remove();
                $(".floatHeader #cellHeaderLabel").html('');
                $(".floatHeader #searchTypePanel").remove();

                // Note: user-defined callback requires 
                // default actions from floatHeader plugin
                // implementation (stop+fadeIn)
                header.stop(true, true);
                header.fadeIn(100);
            }
        });
    }

    function tableOut(matrix, cellTiers, dataGroups) {
        // create table with rows for each cell types and columns for each data type
        var table, thead, tableHeader, row, td;

        // fill in column headers from dataTypes returned by server
        tableHeader = $('#columnHeaders');
        table = $('#matrixTable');
        thead = $('thead');
        // 1st column is row headers
        thead.before('<colgroup></colgroup>');
        $.each(dataGroups, function (i, group) {
            tableHeader.append('<th class="groupType"><div class="verticalText">' + 
                                group.label + '</div></th>');
            // add colgroup element to support cross-hair hover effect
            thead.before('<colgroup></colgroup>');
            $.each(group.dataTypes, function (i, dataTypeLabel) {
                dataType = dataTypeLabelHash[dataTypeLabel].term;
                if (dataTypeLabelHash[dataTypeLabel].count !== undefined) {
                    // prune out datatypes with no experiments
                    tableHeader.append('<th class="elementType" title="' + 
                                        dataTypeLabelHash[dataTypeLabel].description + 
                                        '"><div class="verticalText">' + dataTypeLabel + 
                                        '</div></th>');
                    // add colgroup element to support cross-hair hover effect
                    thead.before('<colgroup class="dataTypeCol"></colgroup>');
                }
            });
        });
        // fill in matrix --
        // add rows with cell type labels (column 1) and cells for experiments
        // add sections for each Tier of cell type
        $.each(cellTiers, function (i, tier) {
            //skip bogus 4th tier (not my property ?)
            if (tier === undefined) {
                return true;
            }
            row = $('<tr class="matrix"><th class="groupType">' + 
                                "Tier " + tier.term + '</th></td></tr>');
            rowAddCells(row, dataGroups, matrix, null);
            table.append(row);

            $.each(tier.cellTypes, function (i, cellType) {
                if (!cellType) {
                    return true;
                }
                if (!matrix[cellType]) {
                    return true;
                }
                karyotype = cellTypeHash[cellType].karyotype;
                // TODO: recognize cancer*
                // NOTE: coupled to CSS
                if (karyotype !== 'cancer' && karyotype !== 'normal') {
                    karyotype = 'unknown';
                }
                row = $('<tr><th class="elementType" title="' + 
                        cellTypeHash[cellType].description + 
                        '"><a href="/cgi-bin/hgEncodeVocab?ra=encode/cv.ra&term=' + cellType + 
                        '">' + cellType + '</a><span title="karyotype: ' + karyotype + 
                        '" class="' + karyotype + '">&bull;</span></th>');

                rowAddCells(row, dataGroups, matrix, cellType);

                table.append(row);
            });
        });
        $("body").append(table);

        addFloatingHeader(table);

        // column and row hover (cross-hair effect)
        // thanks to Chris Coyier, css-tricks.com
        // NOTE:  acts on colgroups declared at start of table
        $("#matrixTable, #matrixTableFloatHeaderClone").delegate('.matrixCell, .elementType','mouseover mouseleave', function(e) {
            if (!$(this).hasClass('experiment') && !$(this).hasClass('todoExperiment') &&
                !$(this).hasClass('elementType') && !$(this).hasClass('groupType')) {
                return;
            }
            if (e.type == 'mouseover') {
                // refrain from highlighting header row
                if (!$(this).parent().is("#columnHeaders")) {
                    $(this).parent().addClass("crossHair");
                }
                col = $("colGroup").eq($(this).index());
                if (col.hasClass("dataTypeCol")) {
                    col.addClass("crossHair");
                }
            } else {
                $(this).parent().removeClass("crossHair");
                $("colGroup").eq($(this).index()).removeClass("crossHair");
            }
        });
    }

    function handleServerData(responses) {
        // main actions, called when loading data from server is complete
        var experiments = responses[0], dataTypes = responses[1], 
                        cellTypes = responses[2], expIds = responses[3];
        var dataGroups, cellTiers, expIdHash, header;
        var matrix = {};

        hideLoadingImage(spinner);
        $('#matrixTable').show();

        // set up structures for data types and their groups
        $.each(dataTypes, function (i, dataType) {
            dataTypeTermHash[dataType.term] = dataType;
            dataTypeLabelHash[dataType.label] = dataType;
        });
        // data type labels tucked into their tiers
        dataGroups = encodeProject.getDataGroups(dataTypes);

        // set up structures for cell types and their tiers
        $.each(cellTypes, function (i, cellType) {
            cellTypeHash[cellType.term] = cellType;
        });
        cellTiers = encodeProject.getCellTiers(cellTypes);

        // use to filter out experiments not in this assembly
        expIdHash = encodeProject.getExpIdHash(expIds);

        // gather experiments into matrix
        $.each(experiments, function (i, exp) {
            // todo: filter out with arg to hgApi
            if (exp.organism !== organism) {
                return true;
            }
            // exclude ref genome annotations
            if (exp.cellType === 'None') {
                return true;
            }
            if (expIdHash[exp.ix] === undefined) {
                return true;
            }
            // count experiments per dataType so we can prune those having none
            // (the matrix[cellType] indicates this for cell types 
            // so don't need hash for those
            dataType = exp.dataType;
            if (!dataTypeTermHash[dataType].count) {
                dataTypeTermHash[dataType].count = 0;
            }
            dataTypeTermHash[dataType].count++;

            cellType = exp.cellType;
            if (!matrix[cellType]) {
                matrix[cellType] = {};
            }
            if (!matrix[cellType][dataType]) {
                matrix[cellType][dataType] = 0;
            }
            matrix[cellType][dataType]++;
        });

        // fill in table
        tableOut(matrix, cellTiers, dataGroups);
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
    organism = encodeDataMatrix_organism;
    assembly = encodeDataMatrix_assembly;
    $("#assemblyLabel").text(assembly);
    header = encodeDataMatrix_pageHeader;
    $("#pageHeader").text(header);
    document.title = 'ENCODE ' + header;

    encodeProject.setup({
        server: server,
        assembly: assembly
    });

    // show only spinner until data is retrieved
    $('#matrixTable').hide();
    spinner = showLoadingImage("spinner");

    // add radio buttons for search type to specified div on page
    encodeProject.addSearchPanel('#searchTypePanel');

    // load data from server and do callback
    encodeProject.loadAllFromServer(requests, handleServerData);
});
