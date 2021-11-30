var searchExample = (function() {

    // this object contains the categories we can facet/filter on and the
    // search results
    var uiState = {};
    var debugCartJson = true;

    var digitTest = /^\d+$/,
            keyBreaker = /([^\[\]]+)|(\[\])/g,
            plus = /\+/g,
            paramTest = /([^?#]*)(#.*)?$/;

    function deparam(params) {
        /* https://github.com/jupiterjs/jquerymx/blob/master/lang/string/deparam/deparam.js */
        if(! params || ! paramTest.test(params) ) {
            return {};
        }

        var data = {},
            pairs = params.split('&'),
            current;

        for (var i=0; i < pairs.length; i++){
            current = data;
            var pair = pairs[i].split('=');

            // if we find foo=1+1=2
            if(pair.length !== 2) {
                pair = [pair[0], pair.slice(1).join("=")];
            }

            var key = decodeURIComponent(pair[0].replace(plus, " ")),
            value = decodeURIComponent(pair[1].replace(plus, " ")),
            parts = key.match(keyBreaker);

            for ( var j = 0; j < parts.length - 1; j++ ) {
                var part = parts[j];
                if (!current[part] ) {
                    //if what we are pointing to looks like an array
                    current[part] = digitTest.test(parts[j+1]) || parts[j+1] === "[]" ? [] : {};
                }
                current = current[part];
                }
            var lastPart = parts[parts.length - 1];
            if (lastPart === "[]"){
                current.push(value);
            } else{
                current[lastPart] = value;
            }
        }
        return data;
    }

    function changeUrl(vars, oldVars) {
        // Save the users search string to the url so web browser can easily
        // cache search results into the browser history
        // vars: object of new key: val pairs like CGI arguments
        // oldVars: arguments we want to keep between calls
        var myUrl = window.location.href;
        myUrl = myUrl.replace('#','');
        var urlParts = myUrl.split("?");
        var baseUrl;
        if (urlParts.length > 1)
            baseUrl = urlParts[0];
        else
            baseUrl = myUrl;

        var urlVars;
        if (oldVars === undefined) {
            var queryStr = urlParts[1];
            urlVars = deparam(queryStr);
        } else {
            urlVars = oldVars;
        }

        for (var key in vars) {
            var val = vars[key];
            if (val === null || val === "") {
                if (key in urlVars) {
                    delete urlVars[key];
                }
            } else {
                urlVars[key] = val;
            }
        }

        var argStr = jQuery.param(urlVars);
        argStr = argStr.replace(/%20/g, "+");

        return {"baseUrl": baseUrl, "args": argStr, "urlVars": urlVars};
    }

    function saveHistory(obj, urlParts,replace) {
        if (replace) {
            history.replaceState(obj, "", urlParts.baseUrl + (urlParts.args.length !== 0 ? "?" + urlParts.args : ""));
        } else {
            history.pushState(obj, "", urlParts.baseUrl + (urlParts.args.length !== 0 ? "?" + urlParts.args : ""));
        }
    }

    // comparator function for sorting tracks, lowest priority wins,
    // followed by short label
    function compareTrack(trackA, trackB) {
        priorityA = trackA.priority;
        priorityB = trackB.priority;
        // if both priorities are undefined or equal to each other, sort
        // on shortlabel alphabetically
        if (priorityA === priorityB) {
            if (trackA.name < trackB.name) {
                return -1;
            } else if (trackA.name > trackB.name) {
                return 1;
            } else {
                return 0;
            }
        } else {
            if (priorityA === undefined) {
                return 1;
            } else if (priorityB === undefined) {
                return -1;
            } else if (priorityA < priorityB) {
                return -1;
            } else if (priorityA > priorityA) {
                return 1;
            } else {
                return 0;
            }
        }
    }

    function compareGroups(a, b) {
        return uiState.trackGroups[a.name].priority - uiState.trackGroups[b.name].priority;
    }

    function sortByTrackGroups(groupList) {
        return groupList.sort(compareGroups);
    }

    // Sort the nested track list structure such that within each group
    // the leaves of the tree are sorted by priority
    function sortTrackCategories(trackList) {
        if (trackList.children !== undefined) {
            trackList.children.sort(compareTrack);
            for (var i = 0; i < trackList.children.length; i++) {
                trackList.children[i] = sortTrackCategories(trackList.children[i]);
            }
        }
        return trackList;
    }

    function categoryComp(category) {
        if (category.priority !== undefined)
            return category.priority;
        return 1000.0;
    }

    function sortCategories(categList) {
        return _.sortBy(categList, categoryComp);
    }

    function filtersToJstree(categs) {
        // Create a tree of filters, where on click of a leaf or group, show hide
        // results from list
        var filterData = [];
        _.each(categs, function(categ) {
            var trackGroups = uiState.trackGroups;
            var groups = {};
            var parentsHash = {};
            var newCateg = {};
            _.assign(newCateg, categ);
            newCateg.text = categ.longLabel;
            if (categ.id !== undefined && categ.id === "trackData") {
                newCateg.text = categ.label;
                _.each(categ.tracks, function(track) {
                    var group = track.group;
                    if (uiState && uiState.positionMatches) {
                        inResults = _.find(uiState.positionMatches, function(match) {
                            return match.trackName === track.id;
                        }) !== undefined;
                        track.state = {selected: inResults};
                    }
                    track.text = track.label;
                    track.li_attr = {title: track.description};
                    if (track.visibility > 0) {
                        if (groups.visible)
                            groups.visible.children.push(track);
                        else {
                            groups.visible = {};
                            groups.visible.id = "Visible Tracks";
                            groups.visible.name = "Visible Tracks";
                            groups.visible.text = "Visible Tracks";
                            groups.visible.li_attr = {title: "Search for track items in all the currently visible searchable tracks"};
                            groups.visible.children = [track];
                            groups.visible.state = {opened: true, loaded: true};
                            groups.visible.priority = 0.0;
                        }
                    } else {
                        var last = track;
                        var doNewComp = true;
                        if (track.parents) {
                            var tracksAndLabels = track.parents.split(',');
                            var l = tracksAndLabels.length;
                            for (var i = 0; i < l; i+=2) {
                                var parentTrack= tracksAndLabels[i];
                                var parentLabel = tracksAndLabels[i+1];
                                if (!(parentTrack in parentsHash)) {
                                    parent = {};
                                    parent.id = parentTrack;
                                    parent.text = parentLabel;
                                    parent.children = [last];
                                    parent.li_attr = {title: "Search for track items in all of the searchable subtracks of the " + parentLabel + " track"};
                                    parentsHash[parentTrack] = parent;
                                    last = parent;
                                    doNewComp = true;
                                } else {
                                    doNewComp = false;
                                    parentsHash[parentTrack].children.push(last);
                                }
                            }
                        }
                        if (groups[group] !== undefined && doNewComp) {
                            groups[group].children.push(last);
                        } else if (doNewComp) {
                            groups[group] = {};
                            groups[group].id = group;
                            groups[group].name = group;
                            groups[group].text = group;
                            groups[group].children = [last];
                            if (trackGroups !== undefined && group in trackGroups) {
                                groups[group].priority = trackGroups[group].priority;
                                groups[group].text= trackGroups[group].label;
                            } else {
                                if (trackGroups === undefined) {
                                    uiState.trackGroups = {};
                                    trackGroups = uiState.trackGroups;
                                }
                                trackGroups[group] = groups[group];
                            }
                        }
                    }
                });
                newCateg.children = [];
                if ("visible" in groups) {
                    groups.visible.children = sortTrackCategories(groups.visible.children);
                    newCateg.children.push(groups.visible);
                }
                hiddenTrackChildren = [];
                newCateg.children.push({id: "Currently Hidden Tracks", name: "Currently Hidden Tracks", text: "Currently Hidden Tracks", children: [], state: {opened: true, loaded: true}, li_attr: {title: "Search for track items in currently hidden tracks"}});
                _.each(groups, function(group) {
                    if (group.id !== "Visible Tracks") {
                        group.li_attr = {title: "Search for track items in the " + group.text + " set of tracks"};
                        group.children = sortTrackCategories(group.children);
                        hiddenTrackChildren.push(group);
                    }
                });
                hiddenTrackChildren = sortByTrackGroups(hiddenTrackChildren);
                _.each(hiddenTrackChildren, function(group) {
                    newCateg.children.at(-1).children.push(group);
                });
            filterData.push(newCateg);
            }
        });
        return filterData;
    }

    function categsToJstree(categs) {
        // We need to convert the categories to a jstree acceptable object
        // which is an array of nodes, where each elem of the array
        // is either a string, or an object with certain properties.
        // Since categs is already an array, we can just add any necessary
        // jstree attributes to the each categ
        var categData = [];
        _.each(categs, function(categ) {
            categ.text = categ.longLabel;
            if (categ.id !== undefined && categ.id === "trackData") {
                categ.text = categ.label;
                categ.state = {opened: true, loaded: true};
                categ.li_attr = {title: categ.description};
            } else {
                categ.text = categ.label;
                categ.state = {selected: categ.visibility > 0};
                categ.li_attr = {title: categ.description};
            }
            categData.push(categ);
        });
        uiState.categs = categData = sortCategories(categData);
        return categData;

        // save the possible categories so we can load the page quickly upon back
        // button or refresh
        //localStorage.setItem('ucscGBSearchCategories', JSON.stringify(categs));
    }

    function makeCategoryTree(data) {
        var parentDiv = $("#searchCategories");
        $.jstree.defaults.core.themes.icons = false;
        $.jstree.defaults.core.themes.dots = true;
        $.jstree.defaults.contextmenu.show_at_node = false;
        parentDiv.jstree({
            'plugins' : ['contextmenu', 'checkbox', 'state'],
            'core': {
                'data': data,
                'check_callback': true
            },
        });
        parentDiv.css('height', "auto");
    }

    function showOrHideResults(event, node) {
        if (node.state.checked) {
            // show results
            resultLi = $('#' + node.id + "Results")[0];
            if (resultLi === undefined)
                alert(node);
            resultLi.style = "display";
        } else {
            // hide results
            resultLi = $('#' + node.id + "Results")[0];
            resultLi.style = "display: none";
        }
    }

    function makeFiltersTree(data) {
        var parentDiv = $("#searchFilters");
        $.jstree.defaults.core.themes.icons = false;
        $.jstree.defaults.core.themes.dots = true;
        $.jstree.defaults.contextmenu.show_at_node = false;
        parentDiv.jstree({
            'plugins' : ['contextmenu', 'checkbox', 'state'],
            'core': {
                'data': data,
                'check_callback': true
            },
            'checkbox': {
                'tie_selection': false
            }
        });
        parentDiv.css('height', "auto");
        parentDiv.on('check_node.jstree uncheck_node.jstree', function(e, data) {
            if ($("#searchResults")[0].children.length > 0) {
                showOrHideResults(e,data.node);
            }
        });
    }

    function updateFilters(uiState) {
        if (uiState.categs !== undefined) {
            var categData = categsToJstree(uiState.categs);
            var filtersData = filtersToJstree(uiState.categs);
            makeCategoryTree(categData);
            makeFiltersTree(filtersData);
        }
    }

    // Update the 'facet' counts with the number of search results returned per category
    // Optionally, if a searchTime is present, update the time it took in milliseconds
    // for this track/group to be searched
    function updateCategoryCounts(categName, count, isParent, searchTime) {
        var oldNode, newtext;
        var oldCount = 0, oldSearchTime = 0;
        theTree = $("#searchFilters").jstree();
        if (!categName.startsWith("trackDb"))
            oldNode = theTree.get_node(categName);
        else
            oldNode = theTree.get_node("[id^=" + categName + "]");
        // If the user entered an hgvs term, we may have got a match to LRG or whatever track
        // that isn't open yet, so open it now and check it so it's easier to find.
        if (!isParent) {
            theTree._open_to(oldNode);
            theTree.check_node(oldNode); //.state.checked  = true;
        }
        var oldCountSpan = $("[id='" + oldNode.id + "count'");
        var oldTimeSpan = $("[id='" + oldNode.id + "searchTime'");
        if (oldCountSpan.length) {
            if (isParent) {
                oldCount = parseInt(oldCountSpan[0].innerText.split(' ')[0].slice(1));
            }
        }
        if (oldTimeSpan.length) {
            if (isParent) {
                if (typeof searchTime !== 'undefined') {
                    oldSearchTime = parseInt(oldTimeSpan[0].innerText.split(' ')[1].slice(0,-2));
                }
            }
        }
        newtext = "<span id='" + oldNode.id + "extraInfo'>";
        newtext += " <span id='" + oldNode.id + "count'>(<b>" + (count + oldCount) + " results</b>";
        if (typeof searchTime !== 'undefined') {
            newtext += "</span><span id='" + oldNode.id + "searchTime'>, <b>" + (searchTime + oldSearchTime) + "ms searchTime </b>";
        }
        newtext += ")</span>"; // count or timing span
        newtext += "</span>"; // container span
        if (typeof newtext !== 'undefined')
            $("[id='" + oldNode.id + "extraInfo']").remove();
            $("[id='"+oldNode.id+"_anchor']").append(newtext);

        // bubble the counts up to any parents:
        if (oldNode && oldNode.parent !== "#")
            updateCategoryCounts(oldNode.parent, count, true, searchTime);
    }

    function clearOldFacetCounts() {
        $("[id*='extraInfo']").remove();
    }

    function printMatches(list, matches, title) {
        var printCount = 0;
        _.each(matches, function(match, printCount) {
            var position = match.position.split(':');
            var url, matchTitle;
            if (title === "helpDocs") {
                url = position[0];
                matchTitle = position[1].replace(/_/g, " ");
            } else if (title === "publicHubs") {
                var hubUrl = position[0] + ":" + position[1];
                var dbName = position[2];
                var track = position[3];
                var hubShortLabel = position[4];
                var hubLongLabel = position[5];
                url = "hgTrackUi?hubUrl=" + hubUrl + "&g=" + track + "&db=" + dbName;
                matchTitle = hubShortLabel;
            } else if (title === "trackDb") {
                var trackName = position[0];
                var shortLabel = position[1];
                var longLabel = position[2];
                url = "hgTrackUi?g=" + trackName;
                matchTitle = shortLabel + " - " + longLabel;
            } else {
                url = "hgTracks?" + title + "=pack&position=" + match.position + "&hgFind.matches=" + match.hgFindMatches;
                if (match.extraSel)
                    url += "&" + match.extraSel;
                matchTitle = match.posName;
                if (match.canonical === true)
                    matchTitle = "<b>" + matchTitle + "</b>";
            }
            var newListObj;
            if (printCount < 500) {
                if (printCount + 1 > 10) {
                    newListObj = "<li class='" + title + "_hidden' style='display: none'><a href=\"" + url + "\">" + matchTitle + "</a> - ";
                } else {
                    newListObj = "<li><a href=\"" + url + "\">" + matchTitle + "</a> - ";
                }
                printedPos = false;
                if (!(["helpDocs", "publicHubs", "trackDb"].includes(title))) {
                    newListObj += match.position;
                    printedPos = true;
                }
                if (match.description) {
                    if (printedPos) {newListObj += " - ";}
                    newListObj += match.description;
                }
                newListObj += "</li>";
                list.innerHTML += newListObj;
                printCount += 1;
            }
        });
    }

    function showMoreResults() {
        var trackName = this.id.replace(/Results_.*/, "");
        var isHidden = $("." + trackName + "_hidden")[0].style.display === "none";
        _.each($("." + trackName + "_hidden"), function(hiddenLi) {
            if (isHidden) {
                hiddenLi.style = "display:";
            } else {
                hiddenLi.style = "display: none";
            }
        });
        if (isHidden) {
            newText = this.nextSibling.innerHTML.replace(/Show/,"Hide");
            this.nextSibling.innerHTML = newText;
            this.src = "../images/remove_sm.gif";
        } else {
            newText = this.nextSibling.innerHTML.replace(/Hide/,"Show");
            this.nextSibling.innerHTML = newText;
            this.src = "../images/add_sm.gif";
        }
    }

    function collapseNode() {
        var toCollapse = this.parentNode.childNodes[3];
        var isHidden  = toCollapse.style.display === "none";
        if (isHidden)
            {
            toCollapse.style = 'display:';
            this.src = "../images/remove_sm.gif";
            }
        else
            {
            toCollapse.style = 'display: none';
            this.src = "../images/add_sm.gif";
            }
    }

    function updateSearchResults(uiState) {
        var parentDiv = $("#searchResults");
        if (uiState && uiState.search !== undefined) {
            $("#searchBarSearchString").val(uiState.search);
        } else {
            // back button all the way to the beginning
            $("#searchBarSearchString").val("");
        }
        if (uiState && uiState.positionMatches && uiState.positionMatches.length > 0) {
            // clear the old search results if there were any:
            parentDiv.empty();
            var newList = document.createElement("ul");
            var noUlStyle = document.createAttribute("class");
            noUlStyle.value = "ulNoStyle";
            newList.setAttributeNode(noUlStyle);
            parentDiv.append(newList);
            clearOldFacetCounts();
            var categoryCount = 0;
            // Loop through categories of match (public hubs, help docs, a single track, ...
            _.each(uiState.positionMatches, function(categ) {
                var title = categ.name;
                var searchDesc = categ.description;
                var matches = categ.matches;
                var numMatches = matches.length;
                updateCategoryCounts(title, numMatches, false, categ.searchTime);
                var newListObj = document.createElement("li");
                var idAttr = document.createAttribute("id");
                idAttr.value = title + 'Results';
                newListObj.setAttributeNode(idAttr);
                var noLiStyle = document.createAttribute("class");
                noLiStyle.value = "liNoStyle";
                newListObj.setAttributeNode(noLiStyle);
                newListObj.innerHTML += "<input type='hidden' id='" + idAttr.value + categoryCount + "' value='0'>";
                newListObj.innerHTML += "<img height='18' width='18' id='" + idAttr.value + categoryCount + "_button' src='../images/remove_sm.gif'>";
                newListObj.innerHTML += "&nbsp;Matches to " + searchDesc + ":";
                //printOneFullMatch(newList, matches[0], title, searchDesc);
                // Now loop through each actual hit on this table and unpack onto list
                var subList = document.createElement("ul");
                printMatches(subList, matches, title);
                if (matches.length > 10) {
                    subList.innerHTML += "<li class='liNoStyle'>";
                    subList.innerHTML += "<input type='hidden' id='" + idAttr.value + "_" + categoryCount +  "showMore' value='0'>";
                    subList.innerHTML += "<img height='18' width='18' id='" + idAttr.value + "_" + categoryCount + "_showMoreButton' src='../images/add_sm.gif'>";
                    if (matches.length > 500)
                        subList.innerHTML += "<div class='showMoreDiv' id='" + idAttr.value+"_"+categoryCount+"_showMoreDiv'>Show 490 (out of " + (matches.length) + " total) more matches for " + searchDesc + "</div></li>";
                    else
                        subList.innerHTML += "<div class='showMoreDiv' id='" + idAttr.value+"_"+categoryCount+"_showMoreDiv'>Show " + (matches.length - 10) + " more matches for " + searchDesc + "</div></li>";
                }
                newListObj.append(subList);
                newList.append(newListObj);

                // make result list collapsible:
                $('#'+idAttr.value+categoryCount+"_button").click(collapseNode);
                $('#'+idAttr.value+"_" +categoryCount+"_showMoreButton").click(showMoreResults);
                categoryCount += 1;
            });
        } else if (uiState) {
            // No results from match
            var msg = "<p>No results for: <b>" + uiState.search + "<b></p>";
            parentDiv.empty();
            parentDiv.html(msg);
            clearOldFacetCounts();
        } else {
            parentDiv.empty();
        }
    }

    function checkJsonData(jsonData, callerName) {
        // Return true if jsonData isn't empty and doesn't contain an error;
        // otherwise complain on behalf of caller.
        if (! jsonData) {
            alert(callerName + ': empty response from server');
        } else if (jsonData.error) {
            console.error(jsonData.error);
            alert(callerName + ': error from server: ' + jsonData.error);
        } else {
            if (debugCartJson) {
                console.log('from server:\n', jsonData);
            }
            return true;
        }
        return false;
    }

    function updateStateAndPage(jsonData) {
        // Update uiState with new values and update the page.
        _.assign(uiState, jsonData);
        updateFilters(uiState);
        if (uiState.positionMatches !== undefined) {
            updateSearchResults(uiState);
            urlVars = {"search": uiState.search, "showSearchResults": ""};
            // changing the url allows the history to be associated to a specific url
            var urlParts = changeUrl(urlVars);
            saveHistory(uiState, urlParts);
        }
    }

    function handleRefreshState(jsonData) {
        if (checkJsonData(jsonData, 'handleRefreshState')) {
            updateStateAndPage(jsonData);
        }
        $("#spinner").remove();
    }

    function handleErrorState(jqXHR, textStatus) {
        cart.defaultErrorCallback(jqXHR, textStatus);
        $("#spinner").remove();
    }

    /* Recursive function to find the highest selected parent in the tree */
    function getParentCheckedNode(tree, node) {
        if (node.parent === "#") {
            if (node.state.selected)
                return node;
        } else if (node.parent === "Currently Hidden Tracks") {
            hiddenTracksNode =  tree.get_node(node.parent);
            if (hiddenTracksNode.state.selected) {
                topNode = tree.get_node(hiddenTracksNode.parent);
                if (!topNode.state.selected) {
                    return node;
                } else {
                    return getParentCheckedNode(tree, topNode);
                }
            } else {
                return node;
            }
        } else {
            var parent = tree.get_node(node.parent);
            if ( parent.state.selected) {
                return getParentCheckedNode(tree, parent);
            } else {
                return node;
            }
        }
    }

    function getCheckedCategories() {
        var ret = [];
        var parents = {};
        theTree = $("#searchCategories").jstree();
        _.each($("#searchCategories").jstree().get_checked(full=true), function(categ) {
            var parent = getParentCheckedNode(theTree, categ);
            if (categ === parent && (parent.id.startsWith("Visible") || parent.id.startsWith("Currently Hidden"))) {
                // goes to the next iteration of _.each():
                return;
            } else {
                if (parent !== categ && (parent.id.startsWith("Visible") || parent.id.startsWith("Currently Hidden"))) {
                    ret.push(categ.id);
                } else if (!(parents[parent.id])) {
                    parents[parent.id] = true;
                    if (parent.id === "trackData")
                        ret.push("allTracks");
                    else
                        ret.push(parent.id);
                }
            }
        });
        return ret;
    }

    function sendUserSearch() {
        // User has clicked the search button, if they also entered a search
        // term, fire off a search
        cart.debug(debugCartJson);
        var searchTerm = $("#searchBarSearchString").val();
        if (searchTerm !== undefined) {
            // put up a loading image
            $("#searchBarSearchButton").parent().append("<i id='spinner' class='fa fa-spinner fa-spin'></i>");

            // if the user entered a plain position string like chr1:blah-blah, just go to the old cgi/hgTracks
            var canonMatch = searchTerm.match(canonicalRangeExp);
            var gbrowserMatch = searchTerm.match(gbrowserRangeExp);
            var lengthMatch = searchTerm.match(lengthRangeExp);
            var bedMatch = searchTerm.match(bedRangeExp);
            var sqlMatch = searchTerm.match(sqlRangeExp);
            var singleMatch = searchTerm.match(singleBaseExp);
            var positionMatch = canonMatch || gbrowserMatch || lengthMatch || bedMatch || sqlMatch || singleMatch;
            if (positionMatch !== null) {
                var prevCgi = uiState.prevCgi !== undefined ? uiState.prevCgi : "hgTracks";
                window.location.replace("../cgi-bin/" + prevCgi + "?position=" + searchTerm);
                return;
            }

            // change the url so the web browser history can function:
            _.assign(uiState, {"search": searchTerm});
            var wantedFilters = getCheckedCategories();
            // save the selected categories to the users cart so if we navigate away and come back, the cgi
            // can restore the old search results on the server side, we already save the results client
            // side when staying on the page itself
            //cart.send({ saveCategoriesToCart: {categs: wantedFilters}});
            //cart.flush();
            cart.send({ getSearchResults: {searchString: searchTerm, categs: wantedFilters}}, handleRefreshState, handleErrorState);
            // always update the results when a search has happened
            cart.flush();
        }
    }

    function init() {
        cart.setCgi('searchExample');
        cart.debug(debugCartJson);
        // If a user clicks search before the page has finished loading
        // start processing it now:
        $("#searchBarSearchButton").click(sendUserSearch);
        if (typeof cartJson !== "undefined") {
            var urlParts = {};
            if (debugCartJson) {
                console.log('from server:\n', cartJson);
            }
            if (typeof cartJson.search !== "undefined") {
                urlParts = changeUrl({"search": cartJson.search, "showSearchResults": ""});
            } else {
                urlParts = changeUrl({"showSearchResults": ""});
                cartJson.search = urlParts.urlVars.search;
            }
            _.assign(uiState,cartJson);
            if (typeof cartJson.categs  !== "undefined") {
                var categData = categsToJstree(uiState.categs);
                var filterData = filtersToJstree(uiState.categs);
                makeCategoryTree(categData);
                makeFiltersTree(categData);
            } else {
                cart.send({ getUiState: {} }, handleRefreshState);
                cart.flush();
            }
            $("#searchCategories").bind('ready.jstree', function(e, data) {
                // wait for the jstree to finish loading before showing the results
                $("#searchBarSearchString").val(uiState.search);
                updateSearchResults(uiState);
                saveHistory(cartJson, urlParts, true);
            });
        } else {
            // no cartJson object means we are coming to the page for the first time:
            cart.send({ getUiState: {} }, handleRefreshState);
            cart.flush();
        }
    }

    return { init: init,
             updateSearchResults: updateSearchResults
           };

}());

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

// when a user reaches this page from the back button we can display our saved state
// instead of sending another network request
window.onpopstate = function(event) {
    event.preventDefault();
    searchExample.updateSearchResults(event.state);
};
