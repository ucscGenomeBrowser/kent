// support stuff for auto-complete using jQuery UI's autocomplete widget
//
// requires ajax.js
// requires utils.js

var suggestCache;

function ajaxGet(db, cache)
{
// Returns jquery.autocomplete.js ajax_get function object
// db is the relevant assembly (e.g. "hg18")
// cache is an optional object used as a hash to cache responses from the server.
    suggestCache = cache;
    return function (request, callback) {
        var key = request.term;
        if(suggestCache == null || suggestCache[key] == null)
        {
            $.ajax({
                       url: "../cgi-bin/hgSuggest",
                       data: "db=" + db + "&prefix=" + key,
                       // dataType: "json",  // XXXX this doesn't work under IE, so we retrieve as text and do an eval to force to an object.
                       trueSuccess: handleSuggest,
                       success: catchErrorOrDispatch,
                       error: function (request, status, errorThrown) {
                           if (typeof console != "undefined") {
                               console.dir(request);
                               console.log(status);
                           }
                           var msg = "ajax call failed";
                           if(status != "error")
                               msg = msg + "; error: " + status;
                           warn(msg + "; statusText: " + request.statusText + "; responseText: " + request.responseText);
                       },
                       key: key,
                       cont: callback
                   });
        } else {
            callback(eval(suggestCache[key]));
        }
        // warn(request.term);
    }
}

function handleSuggest(response, status)
{
    // We seem to get a lot of duplicate requests (especially the first letters of words),
    // so we keep a cache of the suggestions lists we've retreived.
    if(suggestCache != null)
        suggestCache[this.key] = response;
    this.cont(eval(response));
}

function lookupGene(db, gene)
{
// returns coordinates for gene (requires an exact match).
// Warning: this function does a synchronous ajax call.
    // first look in our local cache.
    if(suggestCache && suggestCache[gene]) {
        var list = eval(suggestCache[gene]);
        for(var i=0;i<list.length;i++) {
            if(list[i].value == gene) {
                return list[i].id;
            }
        }
    }
    // synchronously get match from the server
    var str = $.ajax({
                     url: "../cgi-bin/hgSuggest",
                     data: "exact=1&db=" + db + "&prefix=" + gene,
                     async: false
                 }).responseText;
    if(str) {
        var obj = eval(str);
        if(obj.length == 1) {
            return obj[0].id;
        }
    }
    return null;
}

/* suggest (aka gene search)
   Requires three elements on page: positionDisplay (static display), positionInput (input textbox) and position (hidden).
*/

var suggestBox = {
    init: function (db, assemblySupportsGeneSuggest, selectCallback, clickCallback)
    {
    // selectCallback: called when the user selects a new genomic position from the list
    // clickCallback: called when the user clicks on positionDisplay
        var lastEntered = null;    // this is the last value entered by the user via a suggestion (used to distinguish manual entry in the same field)
        var str;
        if(assemblySupportsGeneSuggest) {
            str = "enter new position, gene symbol or annotation search terms";
        } else {
            str = "enter new position or annotation search terms";
        }
        $('#positionInput').Watermark(str, '#686868');
        if(assemblySupportsGeneSuggest) {
            $('#positionInput').autocomplete({
                delay: 500,
                minLength: 2,
                source: ajaxGet(db, new Object),
                open: function(event, ui) {
                    var pos = $(this).offset().top + $(this).height();
                    if (!isNaN(pos)) {
                        var maxHeight = $(window).height() - pos - 30;  // take off a little more because IE needs it
                        var auto = $('.ui-autocomplete');
                        var curHeight = $(auto).children().length * 21;
                        if (curHeight > maxHeight)
                            $(auto).css({maxHeight: maxHeight+'px', overflow:'scroll'});
                        else
                            $(auto).css({maxHeight: 'none', overflow:'hidden'});
                    }
                },
                select: function (event, ui) {
                        selectCallback(ui.item.id);
                        lastEntered = ui.item.value;
                        // jQuery('body').css('cursor', 'wait');
                        // document.TrackHeaderForm.submit();
                    }
            });
        }

        // I want to set focus to the suggest element, but unforunately that prevents PgUp/PgDn from
        // working, which is a major annoyance.
        // $('#positionInput').focus();

        $("#positionInput").change(function(event) {
                                       if(!lastEntered || lastEntered != $('#positionInput').val()) {
                                           // This handles case where user typed or edited something rather than choosing from a suggest list;
                                           // in this case, we only change the position hidden; we do NOT update the displayed coordinates.
                                           $('#position').val($('#positionInput').val());
                                       }
                                   });
        $("#positionDisplay").click(function(event) {
                                        // this let's the user click on the genomic position (e.g. if they want to edit it)
                                        clickCallback($(this).text());
                                        $('#positionInput').val($(this).text());
                                    });
    }
}
