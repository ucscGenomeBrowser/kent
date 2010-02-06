// support stuff for auto-complete using jquery.autocomplete.js
//
// requires ajax.js
// requires utils.js

var suggestCache;

function ajaxGet(getDb, cache)
{
// Returns jquery.autocomplete.js ajax_get function object
// getDb should be a function which returns the relevant assembly (e.g. "hg18)
// cache is an optional object used as a hash to cache responses from the server.
    suggestCache = cache;
    return function (key, cont) {
        if(suggestCache == null || suggestCache[key] == null)
        {
            $.ajax({
                       url: "../cgi-bin/suggest",
                       data: "db=" + getDb() + "&prefix=" + key,
                       // dataType: "json",  // XXXX this doesn't work under IE.
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
                       cont: cont
                   });
        } else {
            cont(eval(suggestCache[key]));
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
