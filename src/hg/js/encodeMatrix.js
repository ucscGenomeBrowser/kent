/* encodeMatrix.js - shared code for ENCODE data matrix apps

 Formatted: jsbeautify.py -j
 Syntax checked: jslint indent:4, plusplus: true, continue: true, unparam: true, sloppy: true, browser: true
*/
/*global $, encodeProject */

var encodeMatrix = (function () {

    // spinning image displayed during AJAX activity
    var spinner;

    // UI panel for search: select tracks or files

    function addSearchPanel($div) {
        // Create panel of radio buttons for user to select search type
        // Add to passed in div ID; e.g. #searchTypePanel
        $div.append('<span id="searchPanelInstructions">Search for:&nbsp;</span><input type="radio" name="searchType" id="searchTracks" value="tracks" checked="checked">tracks<input type="radio" name="searchType" id="searchFiles" value="files">files');
    }

    return {

        // UI panel for search: select tracks or files

        getSearchUrl: function (assembly) {
            // Return URL for search of type requested in search panel

            var prog, cartVar, url;
            if ($('input:radio[name=searchType]:checked').val() === "tracks") {
                prog = 'hgTracks';
                cartVar = 'hgt_tSearch';
            } else {
                prog = "hgFileSearch";
                cartVar = "hgfs_Search";
            }
             url = '/cgi-bin/' + prog + '?db=' + assembly + '&' + cartVar + '=search' +
                    '&tsCurTab=advancedTab&hgt_tsPage=';
            return (url);
        },

        getSearchType: function () {
            return $('input:radio[name=searchType]:checked').val();
        },

        // General purpose functions

        start: function ($el) {
            // Initialize application. Caller passes in jQuery selector object to hide/show

            // get server from calling web page (e.g. can sub in genome-preview)
            if ('encodeMatrix_server' in window) {
                server = encodeMatrix_server;
            } else {
                server = document.location.hostname;
                // or document.domain ?
            }
            // variables from calling page
            organism = encodeMatrix_organism;
            assembly = encodeMatrix_assembly;
            $('#assemblyLabel').text(assembly);
            header = encodeMatrix_pageHeader;
            $('#pageHeader').text(header);
            document.title = 'ENCODE ' + header;

            encodeProject.setup({
                server: server,
                assembly: assembly
            });

            // show only spinner until data is retrieved
            $el.hide();
            spinner = showLoadingImage('spinner', true);

            // add radio buttons for search type to specified div on page
            addSearchPanel($('#searchTypePanel'));
        },

    show: function ($el) {
        // Call after data loads to show display
        hideLoadingImage(spinner);
        $el.show();
        },

    // Table rendering special effects

    addTableFloatingHeader: function ($table) {
        // add callback for floating table header feature

       // NOTE: it may be possible to revive floating header functionality in IE 
       // using this plug-in, but I've timed out 
       // (not able to make it work in simple HTML either).

          $table.floatHeader({
            cbFadeIn: function (header) {
                // hide axis labels -- a bit tricky to do so
                // as special handling needed for X axis label
                $('.floatHeader #headerLabelRow').remove();
                $('.floatHeader #cellHeaderLabel').html('');
                $('.floatHeader #searchTypePanel').remove();

                // Note: user-defined callback requires 
                // default actions from floatHeader plugin
                // implementation (stop+fadeIn)
                header.stop(true, true);
                header.fadeIn(100);
            }
        });
    },

    hoverTableCrossHair: function ($table) {
        // Add Cross-hair effect (column and row hover) to table
        // thanks to Chris Coyier, css-tricks.com
        // NOTE:  acts on colgroups declared at start of table
        // NOTE: uses many hard-coded CSS classes, and adds 'crossHair' class (relies on styling)

        var $col;
        var tableId, tableSelector;

        // NOTE: too slow on IE, so skip 
        if ($.browser.msie) {
            return;
        }
        tableId = $table.attr('id');
        // add event handling to both main table and its clone
        // NOTE: second table name is generated from floatheader plugin
        tables = '#' + tableId + ',' + '#' + tableId + 'FloatHeaderClone';

        $(tables).delegate('.matrixCell, .elementType','mouseover mouseleave', function(ev) {
            if (!$(this).hasClass('experiment') && !$(this).hasClass('todoExperiment') &&
                !$(this).hasClass('elementType') && !$(this).hasClass('groupType')) {
                return;
            }
            if (ev.type == 'mouseover') {
                // refrain from highlighting header row
                if (!$(this).parent().is('#columnHeaders')) {
                    $(this).parent().addClass('crossHair');
                }
                $col = $('colGroup').eq($(this).index());
                if ($col.hasClass('experimentCol')) {
                    $col.addClass('crossHair');
                }
            } else {
                $(this).parent().removeClass('crossHair');
                $('colGroup').eq($(this).index()).removeClass('crossHair');
            }
        });
    },

    rotateTableCells: function ($table) {
       // plugin from David Votrubec, handles IE rotate
       // TODO: restrict to IE
       $table.rotateTableCellContent({className: 'verticalText'});
       $(this).attr('disabled', 'disabled');
    }

    };
}());
