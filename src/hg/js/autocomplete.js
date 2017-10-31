// support stuff for auto-complete using jQuery UI's autocomplete widget
//
// requires ajax.js
// requires utils.js
/* suggest (aka gene search)
   Requires three elements on page: positionDisplay (static display), positionInput (input textbox) and position (hidden).
*/

var hgTracks = hgTracks || {};

var suggestBox = {
    ajaxGet: function ajaxGet(db) {
        // Returns autocomplete source function
        // db is the relevant assembly (e.g. "hg18")
        var cache = {}; // cache is is used as a hash to cache responses from the server.
        return function(request, callback) {
            var key = request.term;
            if (!cache[key]) {
                $.ajax({
                    url: "../cgi-bin/hgSuggest",
                    data: "db=" + db + "&prefix=" + encodeURIComponent(key),
                    // dataType: "json",  // XXXX this doesn't work under IE, so we retrieve as text and do an eval to force to an object.
                    trueSuccess: function(response, status) {
                        // We get a lot of duplicate requests (especially the first letters of words),
                        // so we keep a cache of the suggestions lists we've retreived.
                        cache[this.key] = response;
                        this.cont(JSON.parse(response));
                    },
                    success: catchErrorOrDispatch,
                    error: function(request, status, errorThrown) {
                        // tolerate errors (i.e. don't report them) to avoid spamming people on flaky network connections
                        // with tons of error messages (#8816).
                    },
                    key: key,
                    cont: callback
                });
            } else {
                callback(JSON.parse(cache[key]));
            }
            // warn(request.term);
        };
    },

    clearFindMatches: function() {
        // clear any hgFind.matches set by a previous user selection (e.g. when user directly edits the search box)
        if ($('#hgFindMatches').length) $('#hgFindMatches').remove();
    },

    updateFindMatches: function(val) {
        // highlight genes choosen from suggest list (#6330)
        if ($('#hgFindMatches').length) $('#hgFindMatches').val(val);
        else $('#positionInput').parents('form').append("<input type='hidden' id='hgFindMatches' name='hgFind.matches' " + "value='" + val + "'>");
    },

    initialized: false,

    init: function(db, assemblySupportsGeneSuggest, selectCallback, clickCallback) {
        // selectCallback(item): called when the user selects a new genomic position from the list
        // clickCallback(position): called when the user clicks on positionDisplay
        this.initialized = true;
        var lastSelected = null; // this is the last value entered by the user via a suggestion (used to distinguish manual entry in the same field)
        var $posInput = $('#positionInput');
        var waterMark;
        if (assemblySupportsGeneSuggest) {
            if (db.match(/^hg[0-9]+/)) {
                // Mention HGVS variant notation for human assemblies only, although it should work
                // with any assembly that has refGene or ncbiRefSeq.
                waterMark = "enter position, gene symbol, HGVS or search terms";
            } else {
                waterMark = "enter position, gene symbol or search terms";
            }
        } else {
            waterMark = "enter position or search terms";
        }
        if ($posInput[0] != document.activeElement) {
            // Reset value before adding watermark -- only if user is not already typing here
            $posInput.val("");
        }
        $posInput.Watermark(waterMark, '#686868');
        if (assemblySupportsGeneSuggest) {
            $('#positionInput').autocomplete({
                delay: 500,
                minLength: 2,
                source: this.ajaxGet(db),
                open: function(event, ui) {
                    var pos = $(this).offset().top + $(this).height();
                    if (!isNaN(pos)) {
                        var maxHeight = $(window).height() - pos - 30; // take off a little more because IE needs it
                        var auto = $('.ui-autocomplete');
                        var curHeight = $(auto).children().length * 21;
                        if (curHeight > maxHeight) $(auto).css({
                            maxHeight: maxHeight + 'px',
                            overflow: 'scroll',
                            zIndex: 12
                        });
                        else $(auto).css({
                            maxHeight: 'none',
                            overflow: 'hidden',
                            zIndex: 12
                        });
                    }
                },
                select: function(event, ui) {
                    selectCallback(ui.item);
                    lastSelected = ui.item.value;
                    suggestBox.updateFindMatches(ui.item.internalId);
                    // jQuery('body').css('cursor', 'wait');
                    // document.TrackHeaderForm.submit();
                }
            });
        }

        // I want to set focus to the suggest element, but unforunately that prevents PgUp/PgDn from
        // working, which is a major annoyance.
        // $('#positionInput').focus();
        $("#positionInput").change(function(event) {
            if (!lastSelected || lastSelected !== $('#positionInput').val()) {
                // This handles case where user typed or edited something rather than choosing from a suggest list;
                // in this case, we only change the position hidden; we do NOT update the displayed coordinates.
                var val = $('#positionInput').val();
                // handles case where users zeroes out positionInput; in that case we revert to currently displayed position
                if (!val || val.length === 0 || val === waterMark)
                    val = $('#positionDisplay').text();
                $('#position').val(val);
                suggestBox.clearFindMatches();
            }
        });
        $("#positionDisplay").click(function(event) {
            // this let's the user click on the genomic position (e.g. if they want to edit it)
            clickCallback($(this).text());
            $('#positionInput').val($(this).text());
            suggestBox.clearFindMatches();
            if (hgTracks.windows)
                {
                genomePos.positionDisplayDialog();
                }
        });
    }
};
