// support stuff for auto-complete using jQuery UI's autocomplete widget
//
// requires ajax.js
// requires utils.js
/* suggest (aka gene search)
   Requires three elements on page: positionDisplay (static display), positionInput (input textbox) and position (hidden).
*/
/* jshint esnext: true */

var hgTracks = hgTracks || {};

var suggestBox = {
    ajaxGet: function ajaxGet(db) {
        // Returns autocomplete source function
        // db is the relevant assembly (e.g. "hg18")
        var cache = {}; // cache is is used as a hash to cache responses from the server.
        return function(request, callback) {
            var key = request.term;
            if (key.length < 2) {
                // show the most recent searches
                let searchStack = window.localStorage.getItem("searchStack");
                if (key.length === 0 && searchStack) {
                    let searchObj = JSON.parse(searchStack);
                    let currDb = getDb();
                    if (currDb in searchObj) {
                        // sort the results list according to the stack order:
                        let entries = Object.entries(searchObj[currDb].results);
                        let stack = searchObj[currDb].stack;
                        let callbackData = [];
                        for (let s of stack) {
                            callbackData.push(searchObj[currDb].results[s]);
                        }
                        callback(callbackData);
                    }
                    return;
                } else {
                    return;
                }
            }
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

    lastMouseDown : null,

    init: function(db, assemblySupportsGeneSuggest, selectCallback, clickCallback) {
        // selectCallback(item): called when the user selects a new genomic position from the list
        // clickCallback(position): called when the user clicks on positionDisplay
        this.initialized = true;
        var lastSelected = null; // this is the last value entered by the user via a suggestion (used to distinguish manual entry in the same field)
        var $posInput = $('#positionInput');
        if ($posInput[0] !== document.activeElement) {
            // Reset value before adding watermark -- only if user is not already typing here
            $posInput.val("");
        }
        var waterMark = suggestBox.restoreWatermark(db, assemblySupportsGeneSuggest);
        if (assemblySupportsGeneSuggest) {
            $.widget("custom.autocompletePosInput",
                $.ui.autocomplete,
                {
                _renderMenu: function(ul, items) {
                    var that = this;
                    jQuery.each(items, function(index, item) {
                        that._renderItemData(ul, item);
                    });
                    if ($(this)[0].term === "") {
                        ul.append("<div class='autoCompleteInfo'>Showing 5 most recent searches. Enter 2 or more characters to start auto-complete search.</div>");
                    } else {
                        ul.append("<div class='autoCompleteInfo'>Click 'Search' or press Enter to search all tracks, hubs and our web pages. See 'examples' link.</div>");
                    }
                },
                _renderItem: function(ul, item) {
                    // In order to use HTML markup in the autocomplete, one has to overwrite
                    // autocomplete's _renderItem method using .html instead of .text.
                    // http://forum.jquery.com/topic/using-html-in-autocomplete
                    let clockIcon = '';
                    if ($("#positionInput").val().length < 2) {
                        clockIcon = '<svg xmlns="http://www.w3.org/2000/svg" height=".75em" width=".75em" viewBox="0 0 512 512"><!--!Font Awesome Free 6.5.1 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license/free Copyright 2024 Fonticons, Inc.--><path d="M75 75L41 41C25.9 25.9 0 36.6 0 57.9V168c0 13.3 10.7 24 24 24H134.1c21.4 0 32.1-25.9 17-41l-30.8-30.8C155 85.5 203 64 256 64c106 0 192 86 192 192s-86 192-192 192c-40.8 0-78.6-12.7-109.7-34.4c-14.5-10.1-34.4-6.6-44.6 7.9s-6.6 34.4 7.9 44.6C151.2 495 201.7 512 256 512c141.4 0 256-114.6 256-256S397.4 0 256 0C185.3 0 121.3 28.7 75 75zm181 53c-13.3 0-24 10.7-24 24V256c0 6.4 2.5 12.5 7 17l72 72c9.4 9.4 24.6 9.4 33.9 0s9.4-24.6 0-33.9l-65-65V152c0-13.3-10.7-24-24-24z"/></svg>&nbsp';
                    }
                    return $("<li></li>")
                        .data("ui-autocomplete-item", item)
                        .append($("<a></a>").html(clockIcon + item.label))
                        .appendTo(ul);
                }
            });
            $('#positionInput').autocompletePosInput({
                delay: 500,
                minLength: 0,
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
                        // we need to remove the ui-menu-item class from the informational div
                        // because it is not selectable/focuseable
                        document.querySelectorAll('.autoCompleteInfo').forEach(function(i) {
                            i.classList.remove("ui-menu-item");
                        });
                    }
                },
                select: function(event, ui) {
                    lastSelected = ui.item.value;
                    suggestBox.updateFindMatches(ui.item.internalId);
                    addRecentSearch(db, ui.item.geneSymbol, ui.item);
                    selectCallback(ui.item);
                    // jQuery('body').css('cursor', 'wait');
                    // document.TrackHeaderForm.submit();
                }
            });
        }

        // I want to set focus to the suggest element, but unforunately that prevents PgUp/PgDn from
        // working, which is a major annoyance.
        $('#positionInput').on("focus", function() {$(this).autocompletePosInput("search", "");});
        $("#positionInput").on("change", function(event) {
            if (!lastSelected || lastSelected !== $('#positionInput').val()) {
                // This handles case where user typed or edited something rather than choosing from a suggest list;
                // in this case, we only change the position hidden; we do NOT update the displayed coordinates.
                var val = $('#positionInput').val();
                // handles case where users zeroes out positionInput; in that case we revert to currently displayed position
                if (!val || val.length === 0 || val === waterMark)
                    val = $('#positionDisplay').text();
                else
                    val = val.replace(/\u2013|\u2014/g, "-");  // replace en-dash and em-dash with hyphen
                $('#position').val(val);
                suggestBox.clearFindMatches();
            }
        });

        $("#positionDisplay").on("mousedown", function(event) {
            // this let's the user click on the genomic position (e.g. if they want to edit it)
            lastMouseDown = event.offsetX;
        });

        $("#positionDisplay").on("mouseup", function(event) {
            if (event.offsetX !== lastMouseDown)
                return; 
            // this let's the user click on the genomic position (e.g. if they want to edit it)
            clickCallback($(this).text());
            $('#positionInput').val($(this).text());
            suggestBox.clearFindMatches();
            if (hgTracks.windows)
                {
                genomePos.positionDisplayDialog();
                }
        });
    },

    restoreWatermark: function(db, assemblySupportsGeneSuggest) {
        var waterMark;
        var $posInput = $('#positionInput');
        if (assemblySupportsGeneSuggest) {
                waterMark = "gene, chromosome range, search terms, help pages, see examples";
        } else {
            waterMark = "chromosome range, search terms, help pages, see examples";
        }
            //$('input[name="hgt.positionInput"]').val("");
        $posInput.val("");
        $posInput.Watermark(waterMark, '#686868');

        return waterMark;
    }, 

};
