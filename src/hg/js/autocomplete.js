// support stuff for auto-complete using jquery.autocomplete.js
//
// requires ajax.js
// requires utils.js

var suggestCache;

function ajaxGet(getDb, cache, newJQuery)
{
// Returns jquery.autocomplete.js ajax_get function object
// getDb should be a function which returns the relevant assembly (e.g. "hg18")
// cache is an optional object used as a hash to cache responses from the server.
    suggestCache = cache;
    return function (request, callback) {
        var key = newJQuery ? request.term : request;
        if(suggestCache == null || suggestCache[key] == null)
        {
            $.ajax({
                       url: "../cgi-bin/hgSuggest",
                       data: "db=" + getDb() + "&prefix=" + key,
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
                           showWarning(msg + "; statusText: " + request.statusText + "; responseText: " + request.responseText);
                       },
                       key: key,
                       cont: callback
                   });
        } else {
            callback(eval(suggestCache[key]));
        }
        // showWarning(request.term);
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
