/* jshint esnext: true */
debugCartJson = false;
var hgMyData = (function() {
    let uiState = {};
    function checkJsonData(jsonData, callerName) {
        // Return true if jsonData isn't empty and doesn't contain an error;
        // otherwise complain on behalf of caller.
        if (! jsonData) {
            alert(callerName + ': empty response from server');
        } else if (jsonData.error) {
            console.error(jsonData.error);
            alert(callerName + ': error from server: ' + jsonData.error);
        } else if (jsonData.warning) {
            alert("Warning: " + jsonData.warning);
            return true;
        } else {
            if (debugCartJson) {
                console.log('from server:\n', jsonData);
            }
            return true;
        }
        return false;
    }

    function updateStateAndPage(jsonData, doSaveHistory) {
        // Update uiState with new values and update the page.
        _.assign(uiState, jsonData);
        /*
        db = uiState.db;
        if (jsonData.positionMatches !== undefined) {
            // clear the old resultHash
            uiState.resultHash = {};
            _.each(uiState.positionMatches, function(match) {
                uiState.resultHash[match.name] = match;
            });
        } else {
            // no results for this search
            uiState.resultHash = {};
            uiState.positionMatches = [];
        }
        updateFilters(uiState);
        updateSearchResults(uiState);
        buildSpeciesDropdown();
        fillOutAssemblies();
        urlVars = {"db": db, "search": uiState.search, "showSearchResults": ""};
        // changing the url allows the history to be associated to a specific url
        var urlParts = changeUrl(urlVars);
        $("#searchCategories").jstree(true).refresh(false,true);
        if (doSaveHistory)
            saveHistory(uiState, urlParts);
        changeSearchResultsLabel();
        */
    }

    function handleRefreshState(jsonData) {
        if (checkJsonData(jsonData, 'handleRefreshState')) {
            updateStateAndPage(jsonData, true);
        }
        $("#spinner").remove();
    }

    function init() {
        cart.setCgi('hgMyData');
        cart.debug(debugCartJson);
        if (typeof cartJson !== "undefined") {
            if (typeof cartJson.warning !== "undefined") {
                alert("Warning: " + cartJson.warning);
            }
            var urlParts = {};
            if (debugCartJson) {
                console.log('from server:\n', cartJson);
            }
            /*
            if (typeof cartJson.search !== "undefined") {
                urlParts = changeUrl({"search": cartJson.search});
            } else {
                urlParts = changeUrl({"db": db});
                cartJson.search = urlParts.urlVars.search;
            }
            */
            _.assign(uiState,cartJson);
            if (typeof cartJson.categs  !== "undefined") {
                _.each(uiState.positionMatches, function(match) {
                    uiState.resultHash[match.name] = match;
                });
                filtersToJstree();
                makeCategoryTree();
            } else {
                cart.send({ getUiState: {} }, handleRefreshState);
                cart.flush();
            }
            saveHistory(cartJson, urlParts, true);
        } else {
            // no cartJson object means we are coming to the page for the first time:
            cart.send({ getUiState: {} }, handleRefreshState);
            cart.flush();
        }

        // your code here
    }
    return { init: init,
           };

}());

/*
$(document).ready(function() {
    $('#searchBarSearchString').bind('keypress', function(e) {  // binds listener to search button
        if (e.which === 13) {  // listens for return key
            e.preventDefault();   // prevents return from also submitting whole form
            if ($("#searchBarSearchString").val() !== undefined) {
                $('#searchBarSearchButton').focus().click(); // clicks search button button
            }
        }
    });
});
*/

// when a user reaches this page from the back button we can display our saved state
// instead of sending another network request
window.onpopstate = function(event) {
    event.preventDefault();
    hgMyData.updateStateAndPage(event.state, false);
};
