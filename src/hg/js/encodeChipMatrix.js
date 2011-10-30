// encodeChipMatrix.js - pull experiment table and metadata from server 
//      and display ChIP antibodies vs. cell types in a matrix
// Formatted: jsbeautify.py -j
// Syntax checked: jslint indent:4, plusplus: true, continue: true, unparam: true, sloppy: true, browser: true */
/*global $, encodeProject */

$(function () {
    var dataType, server, requests = [
        // Requests to server API
            encodeProject.serverRequests.experiment,
        encodeProject.serverRequests.cellType,
        encodeProject.serverRequests.antibody];

    function tableOut(matrix, cellTiers, cellTypeHash, antibodyGroups, antibodyHash, targetHash) {
        // Create table where rows = cell types and columns are datatypes
        // create table and first row 2 rows (column title and column headers)
        var table, tableHeader, row, td;
        var antibodyHeaderLabel = "Antibody Targets";
        var cellHeaderLabel = "Cell Types";

        table = $('<table id="matrixTable"><thead><tr id="headerLabelRow"><td></td><td id="antibodyHeaderLabel" class="axisType" colspan=6 title="Click to view information about all element types">' + antibodyHeaderLabel + '</td></tr>' + '<tr id="columnHeaders"><td id="cellHeaderLabel" class="axisType" title="Click to view information about all cell types"><a href="/cgi-bin/hgEncodeVocab?ra=encode/cv.ra&type=Cell+Line&organism=' + organism + '">' + cellHeaderLabel + '</td></tr></thead><tbody>');
        tableHeader = $('#columnHeaders', table);
        $.each(antibodyGroups, function (i, group) {
            tableHeader.append('<th class="groupType"><div class="verticalText">' + group.label + '</div></th>');
            $.each(group.targets, function (i, target) {
                if (targetHash[target] === undefined) {
                    return true;
                }
                // prune out targets with no experiments
                if (targetHash[target].count !== undefined) {
                    tableHeader.append('<th class="elementType"><div class="verticalText">' + target + '</div></th>');
                }
            });
        });
        // add rows with cell type labels and matrix elements for indicating if
        //  there's an experiment 
        $.each(cellTiers, function (i, tier) {
            //skip bogus 4th tier (not my property ?)
            if (tier === undefined) {
                return true;
            }
            // td or th here ?
            table.append($('<tr class="matrix"><th class="groupType">' + "Tier " + tier.term + '</th></td></tr>'));


            $.each(tier.cellTypes, function (i, cellType) {
                if (!cellType) {
                    return true;
                }
                if (!matrix[cellType]) {
                    return true;
                }
                row = $('<tr><th class="elementType">' + cellType + '</th>');

                $.each(antibodyGroups, function (i, group) {
                    // skip group header
                    row.append('<td></td>');
                    $.each(group.targets, function (i, target) {
                        // TODO: change this class to matrixElementType
                        //$(".cellType").click(matrixClickHandler);
                        //"searchWindow");
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
                        }
                        td += '">';
                        if (matrix[cellType][target]) {
                            td += '<a target="searchWindow" href="http://genome-preview.ucsc.edu/cgi-bin/hgTracks?db=hg19&tsCurTab=advancedTab&hgt_tsPage=&hgt_tSearch=search&hgt_mdbVar1=cell&hgt_mdbVar2=target&hgt_mdbVar3=view&hgt_mdbVal2=Any&hgt_mdbVal1=';
                            td += cellType;
                            td += '&hgt_mdbVal2=';
                            // TODO: needs to be join of all antibodies for this target
                            td += target;
                            //td += '"><font color=#00994D>';
                            td += '"><font>';
                            td += matrix[cellType][target];
                            //td += ".....";
                            td += '</font></a>';
                            td += '</td>';
                        }
                        row.append(td);
                    });
                    table.append(row);
                });
                table.append(row);
            });
        });
        table.append('</tbody>');
        $("body").append(table);

        // use floating-table-header plugin
        table.floatHeader({
            cbFadeIn: function (header) {
                // hide axis labels -- a bit tricky to do so
                // as special handling needed for X axis label
                $(".floatHeader #headerLabelRow").remove();
                $(".floatHeader #cellHeaderLabel").html('');

                // Note: user-defined callback requires 
                // default actions from floatHeader plugin
                // implementation (stop+fadeIn)
                header.stop(true, true);
                header.fadeIn(100);
                //header.fadeIn(100);

            }/*,
            cbFadeOut: function (header) {
                // show elements with class axisType
                header.stop(true, true)
                header.fadeOut(100);
                $("#cellHeaderLabel").html(cellHeaderLabel);
                $("#antibodyHeaderLabel").html(antibodyHeaderLabel);
            }
            */
        });
    }

    function handleServerData(responses) {
        // Main actions, called when loading data from server is complete
        var experiments = responses[0],
            cellTypes = responses[1],
            antibodies = responses[2],
            antibodyGroups, antibodyHash = {},
            targetHash = {},
            cellTypeHash = {},
            antibody, target, cellTiers, cellType, matrix = {},
            organism, assembly, header;

        // variables passed in hidden fields
        organism = encodeChipMatrix_organism;
        assembly = encodeChipMatrix_assembly;
        header = encodeChipMatrix_pageHeader;

        $("#pageHeader").text(header);
        document.title = 'ENCODE ' + header;

        // set up structures for antibodies and their groups
        $.each(antibodies, function (i, item) {
            antibodyHash[item.term] = item;
        });
        antibodyGroups = encodeProject.getAntibodyGroups(antibodies);

        // set up structures for cell types and their tiers
        $.each(cellTypes, function (i, item) {
            cellTypeHash[item.term] = item;
        });
        cellTiers = encodeProject.getCellTiers(cellTypes);

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
            // todo: filter out with arg to hgApi ?
            if (exp.dataType !== 'ChipSeq') {
                return true;
            }
            // count experiments per target so we can prune those having none
            // (the matrix[cellType] indicates this for cell types 
            // so don't need hash for those
            antibody = encodeProject.antibodyFromExp(exp);
            target = encodeProject.targetFromAntibody(antibody, antibodyHash);
            if (!targetHash[target]) {
                targetHash[target] = {
                    count: 0,
                    antibodies: {}
                };
            }
            if (!targetHash[target].antibodies[antibody]) {
                targetHash[target].antibodies[antibody] = antibody;
            }
            targetHash[target].count++;

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
        tableOut(matrix, cellTiers, cellTypeHash, antibodyGroups, antibodyHash, targetHash);
    }

    // get server from calling web page (intended for genome-preview)
    if ('encodeDataMatrix_server' in window) {
        server = encodeDataMatrix_server;
    } else {
        server = document.location.hostname;
    }

    // initialize
    encodeProject.setup({
        server: server
    });
    encodeProject.loadAllFromServer(requests, handleServerData);
});
