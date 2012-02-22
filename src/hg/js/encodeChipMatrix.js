// encodeChipMatrix.js - pull experiment table and metadata from server 
//      and display ChIP antibodies vs. cell types in a matrix
// Formatted: jsbeautify.py -j
// Syntax checked: jslint indent:4, plusplus: true, continue: true, unparam: true, sloppy: true, browser: true */
/*global $, encodeProject */

$(function () {
    var dataType, server, requests = [
        // requests to server API
        encodeProject.serverRequests.experiment,
        encodeProject.serverRequests.cellType,
        encodeProject.serverRequests.antibody,
        encodeProject.serverRequests.expId
        ];

    var cellTypeHash = {}, antibodyHash = {}, targetHash = {};
    var cellType, antibody, target;
    var organism, assembly, server, header;
    var karyotype;
    var spinner;

    function tableOut(matrix, cellTiers, antibodyGroups) {
        // Create table with rows for each cell type and columns for each antibody target
        var table, tableHeader, row, td;

        // fill in column headers from antibody targets returned by server
        tableHeader = $('#columnHeaders');
        $.each(antibodyGroups, function (i, group) {
            tableHeader.append('<th class="groupType"><div class="verticalText">' + 
                                group.label + '</div></th>');
            $.each(group.targets, function (i, target) {
                // prune out targets with no experiments 
                if (targetHash[target] === undefined) {
                    return true;
                }
                if (targetHash[target].count !== undefined) {
                    tableHeader.append('<th class="elementType" title="' +
                                        targetHash[target].description +
                                        '"><div class="verticalText">' + 
                                        target + '</div></th>');
                }
            });
        });
        // fill in matrix:
        // add rows with cell type labels (column 1) and cells for experiments
        table = $('#matrixTable');

        // add sections for each Tier of cells
        $.each(cellTiers, function (i, tier) {
            //skip bogus 4th tier (not my property ?)
            if (tier === undefined) {
                return true;
            }
            table.append($('<tr class="matrix"><th class="groupType">' + "Tier " + 
                                tier.term + '</th></td></tr>'));

            $.each(tier.cellTypes, function (i, cellType) {
                if (!cellType) {
                    return true;
                }
                if (!matrix[cellType]) {
                    return true;
                }
                karyotype = cellTypeHash[cellType].karyotype;
                if (karyotype !== 'cancer' && karyotype !== 'normal') {
                    karyotype = 'unknown';
                }
                row = $('<tr><th class="elementType" title="' +
                        cellTypeHash[cellType].description +
                        '"><a href="/cgi-bin/hgEncodeVocab?ra=encode/cv.ra&term=' + cellType 
                        + '">' + cellType + '</a><span title="karyotype: ' + karyotype +
                        '" class="' + karyotype + '">&bull;</span></th>');

                $.each(antibodyGroups, function (i, group) {
                    // skip group header
                    row.append('<td></td>');
                    $.each(group.targets, function (i, target) {
                        // prune out targets with no experiments
                        if (targetHash[target] === undefined) {
                            return true;
                        }
                        if (targetHash[target].count === undefined) {
                            return true;
                        }
                        td = $('<td></td>');
                        td.addClass('matrixCell');
                        if (matrix[cellType][target]) {
                            td.addClass('experiment');
                            td.text(matrix[cellType][target]);
                            td.data({
                                'target' : target,
                                'cellType' : cellType
                            });
                            td.mouseover(function() {
                                $(this).attr('title', 'Click to select: ' +
                                                $(this).data().target + ' ' + ' in ' +
                                                $(this).data().cellType +' cells');
                            });
                            td.click(function() {
                               // TODO: base on preview ?
                                var url = encodeProject.getSearchUrl(assembly);

                                // TODO: encapsulate var names
                                // TODO: search on antibody
                                url +=
                                   '&hgt_mdbVar1=dataType&hgt_mdbVal1=' + 'ChipSeq' +
                                   '&hgt_mdbVar2=cell&hgt_mdbVal2=' + cellType +
                                   '&hgt_mdbVar3=antibody';
                                // TODO: html encode ?
                                $.each(targetHash[target].antibodies, function (i, antibody) {
                                    url += '&hgt_mdbVal3=' + antibody;
                                });
                                url += '&hgt_mdbVar4=view&hgt_mdbVal4=Any';
                                // TODO: open search window 
                                //window.open(url, "searchWindow");
                                window.location = url;
                            });
                        }
                        row.append(td);
                    });
                    table.append(row);
                });
                table.append(row);
            });
        });
        $("body").append(table);

        // use floating-table-header plugin
        table.floatHeader({
            cbFadeIn: function (header) {
                // hide axis labels -- a bit tricky to do so
                // as special handling needed for X axis label
                $(".floatHeader #headerLabelRow").remove();
                $(".floatHeader #searchTypePanel").remove();
                $(".floatHeader #cellHeaderLabel").html('');

                // Note: user-defined callback requires 
                // default actions from floatHeader plugin
                // implementation (stop+fadeIn)
                header.stop(true, true);
                header.fadeIn(100);
            }
        });
    }

    function handleServerData(responses) {
        // main actions, called when loading data from server is complete
        var experiments = responses[0], cellTypes = responses[1], 
                antibodies = responses[2], expIds = responses[3];
        var antibodyGroups, cellTiers, expIdHash;
        var matrix = {};

        hideLoadingImage(spinner);
        $('#matrixTable').show();

        // set up structures for antibodies and their groups
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
            // todo: filter out with arg to hgApi ?
            if (exp.dataType !== 'ChipSeq') {
                return true;
            }
            // count experiments per target so we can prune those having none
            // (the matrix[cellType] indicates this for cell types 
            // so don't need hash for those
            antibody = encodeProject.antibodyFromExp(exp);
            target = encodeProject.targetFromAntibody(antibody, antibodyHash);
            if (targetHash[target] !== undefined) {
                targetHash[target].count++;
            }

            cellType = exp.cellType;
            if (!matrix[cellType]) {
                matrix[cellType] = {};
            }
            if (!matrix[cellType][target]) {
                matrix[cellType][target] = 0;
            }
            matrix[cellType][target]++;
        });

        // fill in table
        tableOut(matrix, cellTiers, antibodyGroups);
    }

    // initialize

    // get server from calling web page (intended for genome-preview)
    if ('encodeDataMatrix_server' in window) {
        server = encodeDataMatrix_server;
    } else {
        server = document.location.hostname;
    }

    // variables passed from calling page
    organism = encodeChipMatrix_organism;
    assembly = encodeChipMatrix_assembly;
    $("#assemblyLabel").text(assembly);
    header = encodeChipMatrix_pageHeader;
    $("#pageHeader").text(header);
    document.title = 'ENCODE ' + header;

    encodeProject.setup({
        server: server,
        assembly: assembly
    });

    // show only spinner until data is retrieved
    $('#matrixTable').hide();
    spinner = showLoadingImage("spinner", true);

    // add radio buttons for search type to specified div on page
    encodeProject.addSearchPanel('#searchTypePanel');

    encodeProject.loadAllFromServer(requests, handleServerData);
});
