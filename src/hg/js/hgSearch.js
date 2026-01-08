// Utility JavaScript

// "use strict";

// Don't complain about line break before '||' etc:
/* jshint -W014 */
/* jshint -W087 */
/* jshint esnext: true */
var db; /* A global variable for the current assembly. Needed when we may not have
           sent a cartJson request yet */
var hgSearch = (function() {

    // this object contains everything needed to build current state of the page
    var uiState = {
        db: "",              /* The assembly for which all this business belongs to */
        categs: {},          /* all possible categories for this database, this includes all possible
                              * searchable tracks */
        currentCategs: {},   /* the categories (filters) for the current search results */
        positionMatches: [], /* an array of search result objects, one for each category,
                              * created by hgPositionsJson in cartJson.c */
        search: "",          /* what is currently in the search box */
        trackGroups: {},     /* the track groups available for this assembly */
        resultHash: {},      /* positionMatches but each objects' category name is a key */
        genomes: {},         /* Hash of organism name: [{assembly 1}, {assembly 2}, ...] */
        };

    // if true, log to the console anything that comes back from the server (for debug)
    var debugCartJson = false;

    // This object is the parent for all tracks currently hidden on hgTracks
    var hiddenTrackGroup = {
        id: "Currently Hidden Tracks",
        name: "Currently Hidden Tracks",
        label: "Currently Hidden Tracks",
        text: "Currently Hidden Tracks",
        numMatches: 0,
        searchTime: -1,
        children: [],
        state: {opened: true, loaded: true},
        li_attr: {title: "Search for track items in currently hidden tracks"}
    };

    // This object that is the parent for all tracks currently visible on hgTracks
    var visibleTrackGroup = {
        id: "Visible Tracks",
        name: "Visible Tracks",
        label: "Visible Tracks",
        text: "Visible Tracks",
        numMatches: 0,
        searchTime: -1,
        li_attr: {title: "Search for track items in all the currently visible searchable tracks"},
        children: [],
        state: {opened: true, loaded: true},
        priority: 0.0
    };

    function compareTrack(trackA, trackB) {
        /* comparator function for sorting tracks, lowest priority wins,
         * followed by short label */
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
            if (typeof priorityA === "undefined") {
                return 1;
            } else if (typeof priorityB === "undefined") {
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
        /* Compare function for track group sorting */
        return uiState.trackGroups[a.name].priority - uiState.trackGroups[b.name].priority;
    }

    function sortByTrackGroups(groupList) {
        return groupList.sort(compareGroups);
    }

    function sortTrackCategories(trackList) {
        /* Sort the nested track list structure such that within each group
         * the leaves of the tree are sorted by priority */
        if (typeof trackList.children !== "undefined") {
            trackList.children.sort(compareTrack);
            for (var i = 0; i < trackList.children.length; i++) {
                trackList.children[i] = sortTrackCategories(trackList.children[i]);
            }
        }
        return trackList;
    }

    function categoryComp(category) {
        if (typeof category.priority !== "undefined")
            return category.priority;
        return 1000.0;
    }

    function sortCategories(categList) {
        return _.sortBy(categList, categoryComp);
    }

    function addCountAndTimeToLabel(categ) {
        /* Change the text label of the node */
        categ.text = categ.label + " (<span id='" + categ.id + "count'><b>" + categ.numMatches + " results</b></span>";
        if (typeof categ.searchTime !== "undefined" && categ.searchTime >= 0) {
            categ.text += ", <span id='" + categ.id + "searchTime'><b>" + categ.searchTime + "ms searchTime</b></span>";
        }
        categ.text += ")";
    }

    function tracksToTree(trackList) {
        /* Go through the list of all tracks for this assembly, filling
         * out the necessary information for jstree to be able to work.
         * Only include categories that have results.
         * The groups object will get filled out along the way. */
        trackGroups = uiState.trackGroups;
        var ret = [];
        var parentsHash = {};
        var groups = {};
        visibleTrackGroup.children = [];
        visibleTrackGroup.numMatches = 0;
        visibleTrackGroup.searchTime = -1;
        hiddenTrackGroup.children = [];
        hiddenTrackGroup.numMatches = 0;
        hiddenTrackGroup.searchTime = -1;
        _.each(trackList, function(track) {
            if (!(track.id in uiState.resultHash)) {
                return;
            }
            var newCateg = {};
            _.assign(newCateg, track);
            newCateg.text = track.longLabel;
            newCateg.text = track.label;
            var group = track.group;
            newCateg.state = {checked: true, opened: true};
            newCateg.text = track.label;
            newCateg.li_attr = {title: track.description};
            newCateg.numMatches = uiState.resultHash[newCateg.id].matches.length;
            newCateg.searchTime = uiState.resultHash[newCateg.id].searchTime;
            addCountAndTimeToLabel(newCateg);
            if (track.visibility > 0) {
                if (!groups.visible) {
                    groups.visible = visibleTrackGroup;
                }
                groups.visible.children.push(newCateg);
                if (typeof newCateg.searchTime !== "undefined") {
                    if (groups.visible.searchTime < 0)
                        groups.visible.searchTime = 0;
                    groups.visible.searchTime += newCateg.searchTime;
                }
                groups.visible.numMatches += newCateg.numMatches;
            } else {
                var last = newCateg;
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
                            parent.label = parentLabel;
                            parent.children = [last];
                            parent.li_attr = {title: "Search for track items in all of the searchable subtracks of the " + parentLabel + " track"};
                            parent.numMatches = last.numMatches;
                            parent.searchTime = last.searchTime;
                            parentsHash[parentTrack] = parent;
                            addCountAndTimeToLabel(parent);
                            last = parent;
                            doNewComp = true;
                        } else if (typeof last !== "undefined") {
                            // if we are processing the first parent, we need to add ourself (last)
                            // as a child so the subtrack list is correct, but we still need
                            // to go up through the parent list and update the summarized counts
                            if (doNewComp) {
                                parentsHash[parentTrack].children.push(last);
                            }
                            doNewComp = false;
                            parentsHash[parentTrack].numMatches += last.numMatches;
                            if (typeof last.searchTime !== "undefined") {
                                parentsHash[parentTrack].searchTime += last.searchTime;
                            }
                            addCountAndTimeToLabel(parent);
                        }
                    }
                }
                if (typeof groups[group] !== "undefined" && typeof last !== "undefined") {
                    groups[group].numMatches += last.numMatches;
                    if (typeof last.searchTime !== "undefined") {
                        groups[group].searchTime += last.searchTime;
                    }
                    addCountAndTimeToLabel(groups[group]);
                    if (doNewComp) {
                        groups[group].children.push(last);
                    }
                } else if (doNewComp) {
                    groups[group] = {};
                    groups[group].id = group;
                    groups[group].name = group;
                    groups[group].label = group;
                    addCountAndTimeToLabel(groups[group]);
                    groups[group].numMatches = last.numMatches;
                    groups[group].searchTime = last.searchTime;
                    groups[group].children = [last];
                    if (typeof trackGroups !== "undefined" && group in trackGroups) {
                        groups[group].priority = trackGroups[group].priority;
                        groups[group].label = trackGroups[group].label;
                        addCountAndTimeToLabel(groups[group]);
                    } else {
                        trackGroups[group] = groups[group];
                    }
                }
            }
        });
        if ("visible" in groups) {
            groups.visible.children = sortTrackCategories(groups.visible.children);
            addCountAndTimeToLabel(groups.visible);
            ret.push(groups.visible);
        }
        hiddenTrackChildren = [];
        _.each(groups, function(group) {
            if (group.id !== "Visible Tracks") {
                group.li_attr = {title: "Search for track items in the " + group.label+ " set of tracks"};
                group.state = {checked: true, opened: true};
                group.children = sortTrackCategories(group.children);
                hiddenTrackChildren.push(group);
            }
        });
        if (hiddenTrackChildren.length > 0) {
            hiddenTrackChildren = sortByTrackGroups(hiddenTrackChildren);
            _.each(hiddenTrackChildren, function(group) {
                hiddenTrackGroup.children.push(group);
                if (typeof group.searchTime !== "undefined") {
                    if (hiddenTrackGroup.searchTime < 0) {
                        hiddenTrackGroup.searchTime = 0;
                    }
                    hiddenTrackGroup.searchTime += group.searchTime;
                }
                hiddenTrackGroup.numMatches += group.numMatches;
            });
            addCountAndTimeToLabel(hiddenTrackGroup);
            ret.push(hiddenTrackGroup);
        }
        return ret;
    }

    function filtersToJstree() {
        /* Turns uiState.categs into uiState.currentCategs, which populates the
         * tree of filters. We only make a leaf node in the tree if that leaf
         * has a search result */
        thisCategs = {};
        _.each(uiState.categs, function(categ) {
            var newCateg = {};
            if (categ.id === "trackData") {
                // this id will never be in the results since we only get a result
                // per leaf node, so handle this case separately
                _.assign(newCateg, categ);
                newCateg.text = categ.label;
                newCateg.li_attr = {title: newCateg.description};
                newCateg.state = {opened: true, loaded: true, checked: true};
                newCateg.children = tracksToTree(categ.tracks);
                newCateg.searchTime = 0;
                newCateg.numMatches = 0;
                if (_.isEmpty(newCateg.children)) {
                    return true; // goes to next instance of _.each()
                }
                _.each(newCateg.children, function(track) {
                    newCateg.searchTime += track.searchTime;
                    newCateg.numMatches += track.numMatches;
                });
                addCountAndTimeToLabel(newCateg);
            } else if (categ.id in uiState.resultHash) {
                _.assign(newCateg, categ);
                newCateg.numMatches = uiState.resultHash[categ.id].matches.length;
                newCateg.searchTime = uiState.resultHash[categ.id].searchTime;
                addCountAndTimeToLabel(newCateg);
                newCateg.li_attr = {title: "Show/hide hits to " + newCateg.description};
                newCateg.state = {opened: true, loaded: true, checked: true};
            }

            if (!_.isEmpty(newCateg))
                thisCategs[categ.id] = newCateg;
        });

        // all of the currentCategs need to be children of the root node
        uiState.currentCategs['#'] = {
            id: '#',
            children: sortCategories(Object.keys(thisCategs).map(function(ele) {
                return thisCategs[ele];
            }))
        };
    }

    function showOrHideResults(event, node) {
        /* When a checkbox is checked/uncheck in the tree, show/hide the corresponding
         * result section in the list of results */
        var state = node.state.checked;
        if (node.children.length > 0) {
            _.each(node.children, function(n) {
                showOrHideResults(event, $("#searchCategories").jstree().get_node(n));
            });
        } else {
            resultLi = $('[id="' + node.id + 'Results"');
            if (typeof resultLi !== "undefined") // if we don't have any results for this track resultLi is undefined
                _.each(resultLi, function(li) {
                    li.style = state ? "display" : "display: none";
                });
        }
    }

    function buildTree(node, cb) {
        cb.call(this, uiState.currentCategs[node.id]);
    }

    function makeCategoryTree() {
        var parentDiv = $("#searchCategories");
        $.jstree.defaults.core.themes.icons = false;
        $.jstree.defaults.core.themes.dots = true;
        $.jstree.defaults.contextmenu.show_at_node = false;
        parentDiv.jstree({
            'plugins' : ['contextmenu', 'checkbox'],
            'core': {
                'data': buildTree,
                'check_callback': true
            },
            'checkbox': {
                'tie_selection': false
            }
        });
        parentDiv.css('height', "auto");
    }

    function updateFilters(uiState) {
        if (typeof uiState.categs !== "undefined") {
            filtersToJstree();
            makeCategoryTree();
        }
    }

    function clearOldFacetCounts() {
        $("[id*='extraInfo']").remove();
    }

    function printMatches(list, matches, title, searchDesc, doShowMore) {
        var printCount = 0;
        _.each(matches, function(match, printCount) {
            var position = match.position.split(':');
            var url, matchTitle;
            if (title === "helpDocs") {
                url = position[0];
                matchTitle = "<b>" + position[1].replace(/_/g, " ") + "</b>";
            } else if (title === "publicHubs") {
                var hubUrl = position[0] + ":" + position[1];
                var dbName = position[2];
                var track = position[3];
                var hubShortLabel = position[4];
                var hubLongLabel = position[5];
                url = "hgTrackUi?hubUrl=" + hubUrl + "&g=" + track + "&db=" + dbName;
                matchTitle = "<b>" + hubShortLabel + "</b>";
            } else if (title === "trackDb") {
                var trackName = position[0];
                var shortLabel = position[1];
                var longLabel = position[2];
                url = "hgTrackUi?g=" + trackName;
                matchTitle = "<b>" +  shortLabel + " - " + longLabel + "</b>";
            } else {
                // unaligned mrnas and ests can still be searched but all you can get
                // to is the hgc page, no hgTracks for them
                goToHgTracks = true;
                hgTracksTitle = hgcTitle = title;
                if (["all_mrna", "all_est", "xenoMrna", "xenoEst", "intronEst"].includes(title)) {
                    hgTracksTitle = title.replace(/all_/, "");
                    if (searchDesc.includes("Unaligned"))
                        goToHgTracks = false;
                }
                if (goToHgTracks) {
                    url = "hgTracks?db=" + db + "&" + hgTracksTitle + "=pack&position=" + match.position;
                    if (match.hgFindMatches) {
                        url += "&hgFind.matches=" + match.hgFindMatches;
                    }
                    if (match.extraSel) {
                        url += "&" + match.extraSel;
                    }
                    if (match.highlight) {
                        url += url[url.length-1] !== '&' ? '&' : '';
                        url += "addHighlight=" + encodeURIComponent(match.highlight);
                    }
                } else {
                    url = "hgc?db=" + db + "&g=" + hgcTitle + "&i=" + match.position + "&c=0&o=0&l=0&r=0" ;
                }
                matchTitle = match.posName;
                //if (match.canonical === true)
                matchTitle = "<b>" + matchTitle + "</b>";
            }
            if (printCount < 500) {
                let newListObj = document.createElement("li");
                let className = "searchResult ";
                if (doShowMore || printCount + 1 > 10) {
                    className += title + "_hidden";
                    if (!doShowMore) {
                        newListObj.style.display = "none";
                    }
                }
                newListObj.className = className;
                let a = document.createElement("a");
                newListObj.appendChild(a);
                a.href = url;
                a.innerHTML = matchTitle; // need the bold in the title so use innerHTML here
                let textStr = " - ";

                printedPos = false;
                if (!(["helpDocs", "publicHubs", "trackDb"].includes(title))) {
                    textStr += match.position;
                    printedPos = true;
                }
                if (match.description) {
                    if (printedPos) {textStr += " - ";}
                    textStr += match.description;
                }
                newListObj.innerHTML += textStr; // the bolded search term can appear anywhere in
                                                 // the text string so use innerHTML for now
                if (list.nodeName === "LI") {
                    list.parentNode.insertBefore(newListObj, list);
                } else {
                    list.appendChild(newListObj);
                }
                printCount += 1;
            }
        });
    }

    function showMoreResults() {
        let trackName = this.id.replace(/Results_.*/, "");
        let isHidden = true;
        let parentNode; // the li with the control input to hide/show
        if (this.nodeName === "IMG") {
            isHidden = this.nextSibling.textContent.startsWith(" Show");
            parentNode = this.parentNode;
        } else if (this.nodeName === "A") {
            isHidden = this.textContent.startsWith(" Show");
            parentNode = this.parentNode.parentNode;
        }
        let btnId = this.id.replace(/Link/, "Button");
        let alreadyMadeElems = document.querySelectorAll("." + trackName + "_hidden");
        if (alreadyMadeElems.length > 0) {
            _.each(alreadyMadeElems, function(hiddenLi) {
                if (isHidden) {
                    hiddenLi.style = "display:";
                } else {
                    hiddenLi.style = "display: none";
                }
            });
        } else {
            // insert more results before parentNode li
            printMoreResults(trackName, parentNode);
        }
        let isIconClick = this.nodeName !== "A";
        let linkEl = null;
        if (isIconClick) {linkEl = this.nextSibling.children[0];}
        if (isHidden) {
            if (isIconClick) {
                // click on the '+' icon
                newText = linkEl.textContent.replace(/Show/,"Hide");
                linkEl.textContent = newText;
                this.src = "../images/remove_sm.gif";
            } else {
                // click on the link text
                this.textContent = this.textContent.replace(/Show/,"Hide");
                let img = document.getElementById(btnId);
                img.src = "../images/remove_sm.gif";
            }
        } else {
            if (isIconClick) {
                // click on the '-' icon
                newText = linkEl.textContent.replace(/Hide/,"Show");
                linkEl.textContent = newText;
                this.src = "../images/add_sm.gif";
            } else {
                this.textContent = this.textContent.replace(/Hide/,"Show");
                let img = document.getElementById(btnId);
                img.src = "../images/add_sm.gif";
            }
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

    function printMoreResults(trackName, nodeAfter) {
        /* Print the 11-500 result before nodeAfter */
        let results = uiState.resultHash[trackName];
        let title = results.name;
        let searchDesc = results.description;
        // show the 11-500th elements
        // after this, only CSS is used to show hide them, since they are part of the
        // page already
        printMatches(nodeAfter, results.matches.slice(10), title, searchDesc, true);
    }

    function updateSearchResults(uiState) {
        var parentDiv = $("#searchResults");
        if (uiState && typeof uiState.search !== "undefined") {
            $("#searchBarSearchString").val(uiState.search);
        } else {
            // back button all the way to the beginning
            $("#searchBarSearchString").val("");
        }
        if (uiState && uiState.positionMatches && uiState.positionMatches.length > 0) {
            // clear the old search results if there were any:
            parentDiv.empty();

            // create the elements that will hold results:
            var newList = document.createElement("ul");
            var noUlStyle = document.createAttribute("class");
            noUlStyle.value = "ulNoStyle";
            newList.setAttributeNode(noUlStyle);
            parentDiv.append(newList);

            clearOldFacetCounts();
            var categoryCount = 0;
            // Loop through categories of match (public hubs, help docs, a single track, ...
            _.each(uiState.positionMatches, function(categ) {
                let title = categ.name;
                let searchDesc = categ.description;
                let matches = categ.matches;
                let numMatches = matches.length;
                let newListObj = document.createElement("li");
                let idAttr = document.createAttribute("id");
                idAttr.value = title + 'Results';
                newListObj.setAttributeNode(idAttr);
                let noLiStyle = document.createAttribute("class");
                noLiStyle.value = "liNoStyle";
                newListObj.setAttributeNode(noLiStyle);
                let inp = document.createElement("input");
                inp.type = "hidden";
                inp.id = idAttr.value + categoryCount;
                inp.value = "0";
                newListObj.appendChild(inp);
                let ctrlImg = document.createElement("img");
                ctrlImg.height = "18";
                ctrlImg.width = "18";
                ctrlImg.id = idAttr.value + categoryCount + "_button";
                ctrlImg.src = "../images/remove_sm.gif";
                newListObj.appendChild(ctrlImg);
                let descText = document.createTextNode(" " + searchDesc + ":");
                newListObj.appendChild(descText);
                // Now loop through each actual hit on this table and unpack onto list
                let subList = document.createElement("ul");
                // only print the first 10 at first
                printMatches(subList, matches.slice(0,10), title, searchDesc, false);
                if (matches.length > 10) {
                    let idStr = idAttr.value + "_" + categoryCount;
                    let showMoreLi = document.createElement("li");
                    showMoreLi.id = idStr;
                    showMoreLi.classList.add("liNoStyle","searchResult");
                    let showMoreInp = document.createElement("input");
                    showMoreInp.type = "hidden";
                    showMoreInp.value = '0';
                    showMoreInp.id = showMoreLi.id + "showMore";
                    showMoreLi.appendChild(showMoreInp);
                    let showMoreImg = document.createElement("img");
                    showMoreImg.height = "18";
                    showMoreImg.width = "18";
                    showMoreImg.id = showMoreLi.id + "_showMoreButton";
                    showMoreImg.src = "../images/add_sm.gif";
                    showMoreLi.appendChild(showMoreImg);
                    let showMoreDiv = document.createElement("div");
                    showMoreDiv.id = idStr + "_showMoreDiv";
                    showMoreDiv.classList.add("showMoreDiv");
                    let showMoreA = document.createElement("a");
                    showMoreA.id = idStr + "_showMoreLink";
                    let newText = "";
                    if (matches.length > 500) {
                        newText = " Show 490 (out of " + (matches.length) + " total) more matches for " + searchDesc;
                    } else {
                        newText = " Show " + (matches.length - 10) + " more matches for " + searchDesc;
                    }
                    showMoreA.textContent = newText;
                    showMoreDiv.appendChild(showMoreA);
                    showMoreLi.appendChild(showMoreDiv);
                    subList.appendChild(showMoreLi);
                }
                newListObj.append(subList);
                newList.append(newListObj);

                // make result list collapsible:
                $('#'+idAttr.value+categoryCount+"_button").click(collapseNode);
                $('#'+idAttr.value+"_" +categoryCount+"_showMoreButton").click(showMoreResults);
                $('#'+idAttr.value + "_" + categoryCount + "_showMoreLink").click(showMoreResults);
                categoryCount += 1;
            });
        } else if (uiState && typeof uiState.search !== "undefined") {
            // No results from match
            var msg = "<p>No results</p>";
            parentDiv.empty();
            parentDiv.html(msg);
            clearOldFacetCounts();
        } else {
            parentDiv.empty();
        }
    }

    function onGenomeSelect(item) {
        // Called when user selects a genome from autocomplete
        // item.genome is the db name from hubApi/findGenome
        // Just update the label and store the selection - don't reload the page
        db = item.genome;
        $("#currentGenome").text(item.commonName + ' - ' + item.genome);
        $("#genomeSearchInput").val('');
    }

    function initGenomeAutocomplete() {
        // Initialize the genome search autocomplete using the standard function from utils.js
        initSpeciesAutoCompleteDropdown('genomeSearchInput', onGenomeSelect);

        // Add click handler for the Change Genome button to trigger autocomplete search
        $("#genomeSearchButton").click(function(e) {
            e.preventDefault();
            $("#genomeSearchInput").autocomplete("search", $("#genomeSearchInput").val());
        });
    }

    function updateCurrentGenomeLabel() {
        // Update the label showing current genome
        var genomeInfo = null;
        // Find the current db in the genomes data
        _.each(uiState.genomes, function(genomeList) {
            _.each(genomeList, function(assembly) {
                if (assembly.name === db ||
                    (assembly.isCurated && assembly.name === trackHubSkipHubName(db))) {
                    genomeInfo = assembly;
                }
            });
        });
        if (genomeInfo) {
            $("#currentGenome").text(genomeInfo.organism + ' - ' + genomeInfo.description + ' (' + db + ')');
        } else {
            $("#currentGenome").text(db);
        }
    }

    function changeSearchResultsLabel() {
        // change the title to indicate what assembly was searched:
        $("#dbPlaceholder").empty();
        var genomeInfo = null;
        // Find the current db in the genomes data
        _.each(uiState.genomes, function(genomeList) {
            _.each(genomeList, function(assembly) {
                if (assembly.name === db ||
                    (assembly.isCurated && assembly.name === trackHubSkipHubName(db))) {
                    genomeInfo = assembly;
                }
            });
        });
        if (genomeInfo) {
            $("#dbPlaceholder").append("on " + db + " (" + genomeInfo.organism + " " + genomeInfo.description + ")");
        } else {
            $("#dbPlaceholder").append("on " + db);
        }
    }

    function checkJsonData(jsonData, callerName) {
        // Return true if jsonData isn't empty and doesn't contain an error;
        // otherwise complain on behalf of caller.
        if (! jsonData) {
            alert(callerName + ': empty response from server');
        } else if (jsonData.error && !jsonData.error.startsWith("Sorry, couldn't locate")) {
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
        db = uiState.db;
        if (typeof jsonData === "undefined" || jsonData === null) {
            // now that the show more text is a link, a popstate event gets fired because the url changes
            // we can safely return because there is no state to change
            return;
        }
        if (typeof jsonData.positionMatches !== "undefined") {
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
        updateCurrentGenomeLabel();
        urlVars = {"db": db, "search": uiState.search, "showSearchResults": ""};
        // changing the url allows the history to be associated to a specific url
        var urlParts = changeUrl(urlVars);
        $("#searchCategories").jstree(true).refresh(false,true);
        saveLinkClicks();
        if (doSaveHistory)
            saveHistory(uiState, urlParts);
        changeSearchResultsLabel();
    }

    function handleRefreshState(jsonData) {
        if (checkJsonData(jsonData, 'handleRefreshState')) {
            updateStateAndPage(jsonData, true);
        }
        $("#spinner").remove();
    }

    function handleErrorState(jqXHR, textStatus) {
        cart.defaultErrorCallback(jqXHR, textStatus);
        $("#spinner").remove();
    }

    function sendUserSearch() {
        // User has clicked the search button, if they also entered a search
        // term, fire off a search
        cart.debug(debugCartJson);
        var searchTerm = $("#searchBarSearchString").val().replaceAll("\"","");
        searchTerm = searchTerm.replace(/[\u200b-\u200d\u2060\uFEFF]/g,''); // remove 0-width chars
        if (typeof searchTerm !== 'undefined' && searchTerm.length > 0) {
            // put up a loading image
            $("#searchBarSearchButton").after("<i id='spinner' class='fa fa-spinner fa-spin'></i>");

            // redirect to hgBlat if the input looks like a DNA sequence
            // minimum length=19 so we do not accidentally redirect to hgBlat for a gene identifier
            // like ATG5
            var dnaRe = new RegExp("^(>[^\n\r ]+[\n\r ]+)?(\\s*[actgnACTGN \n\r]{19,}\\s*)$");
            if (dnaRe.test(searchTerm)) {
                var blatUrl = "hgBlat?type=BLAT%27s+guess&userSeq="+searchTerm;
                window.location.href = blatUrl;
                return false;
            }

            // if the user entered a plain position string like chr1:blah-blah, just
            // go to the old cgi/hgTracks
            var canonMatch = searchTerm.match(canonicalRangeExp);
            var gbrowserMatch = searchTerm.match(gbrowserRangeExp);
            var lengthMatch = searchTerm.match(lengthRangeExp);
            var bedMatch = searchTerm.match(bedRangeExp);
            var sqlMatch = searchTerm.match(sqlRangeExp);
            var singleMatch = searchTerm.match(singleBaseExp);
            var gnomadRangeMatch = searchTerm.match(gnomadRangeExp);
            var gnomadVarMatch = searchTerm.match(gnomadVarExp);
            var positionMatch = canonMatch || gbrowserMatch || lengthMatch || bedMatch || sqlMatch || singleMatch || gnomadVarMatch || gnomadRangeMatch;
            if (positionMatch !== null) {
                var prevCgi = uiState.prevCgi !== undefined ? uiState.prevCgi : "hgTracks";
                window.location.replace("../cgi-bin/" + prevCgi + "?db=" + db + "&position=" + encodeURIComponent(searchTerm));
                return;
            }

            _.assign(uiState, {"search": searchTerm});
            cart.send({ getSearchResults:
                        {
                        db: db,
                        search: searchTerm
                        }
                    },
                    handleRefreshState,
                    handleErrorState);
            // always update the results when a search has happened
            cart.flush();
        }
    }

    function switchAssemblies(newDb) {
        // reload the page to attach curated hub (if any)
        re = /db=[\w,\.]*/;
        window.location = window.location.href.replace(re,"db="+newDb);
    }

    function saveLinkClicks() {
        // attach the link handlers to save search history
        document.querySelectorAll(".searchResult>a").forEach(function(i,j,k) {
            i.addEventListener("click", function(e) {
                // i is the <a> element, use parent elements and the href
                // to construct a fake autocomplete option and save it
                e.preventDefault(); // stops the page from redirecting until this function finishes
                let callbackData = {};
                let trackName = i.parentNode.parentNode.parentNode.id.replace(/Results$/,"");
                let matchList = uiState.positionMatches.find((matches) => matches.name === trackName);
                let id, match, matchStr = i.childNodes[0].textContent;
                // switch lookup depending on the different search categories:
                let decoder = function(str) {
                    // helper decoder to change the html encoded entities in
                    // uiState.positionMatches.posName to what is actually rendered
                    // in matchStr
                    return $("<textarea/>").html(str).text();
                };
                let geneSymbol; // a potentially fake geneSymbol for the autoComplete
                if (trackName === "trackDb") {
                    match = matchList.matches.find((elem) => {
                        let posSplit = elem.posName.split(":");
                        geneSymbol = decoder(posSplit[1] + " - " + posSplit[2]);
                        id = "hgTrackUi?db=" + uiState.db + "&g=" + posSplit[0];
                        return geneSymbol === matchStr;
                    });
                    callbackData.label = geneSymbol;
                    callbackData.value = geneSymbol;
                    callbackData.id = id;
                    callbackData.geneSymbol = geneSymbol;
                    callbackData.internalId = "";
                } else if (trackName === "publicHubs") {
                    match = matchList.matches.find((elem) => {
                        let posSplit = elem.posName.split(":");
                        geneSymbol = decoder(posSplit[4] + " - " + trackHubSkipHubName(posSplit[3]));
                        id = "hgTrackUi?hubUrl=" + posSplit[0] + ":" + posSplit[1] + "&g=" + posSplit[3] + "&db=" + posSplit[3];
                        return decoder(posSplit[4]) === matchStr;
                    });
                    callbackData.label = geneSymbol;
                    callbackData.value = geneSymbol;
                    callbackData.id = id;
                    callbackData.geneSymbol = geneSymbol;
                    callbackData.internalId = "";
                } else if (trackName === "helpDocs") {
                    match = matchList.matches.find((elem) => {
                        let posSplit = elem.posName.split(":");
                        geneSymbol = decoder(posSplit[1].replaceAll("_", " "));
                        return geneSymbol === matchStr;
                    });
                    callbackData.label = geneSymbol;
                    callbackData.value = geneSymbol;
                    callbackData.id = match.position.split(":")[0];
                    callbackData.geneSymbol = geneSymbol;
                    callbackData.internalId = "";
                } else { // regular track item  search result click
                    match = matchList.matches.find((elem) => {
                        geneSymbol = elem.posName.replace(/ .*$/,"");
                        return decoder(elem.posName) === matchStr;
                    });
                    callbackData.label = geneSymbol;
                    callbackData.value = geneSymbol;
                    // special case the genbank searches that are supposed to go to hgc
                    // and not hgTracks
                    let parentTitle = i.parentNode.parentNode.parentNode.childNodes[2];
                    if (["all_mrna", "all_est", "xenoMrna", "xenoEst", "intronEst"].includes(trackName) && parentTitle.textContent.includes("Unaligned")) {
                        id = "hgc?db=" + db + "&g=" + trackName+ "&i=" + match.position + "&c=0&o=0&l=0&r=0" ;
                    } else {
                        id = match.position;
                    }
                    callbackData.id = id;
                    callbackData.geneSymbol = geneSymbol;
                    callbackData.internalId = match.hgFindMatches;
                }
                // the value text is the same for all the types, and needs this
                // hack to remove the bolding from the item values in the autocomplete
                if (match && typeof match.description !== "undefined") {
                    callbackData.label += " " + match.description;
                    callbackData.value = $("<div>" + match.description + "</div>").text();
                }
                // type for autocomplete select to know where to navigate to
                callbackData.type = trackName;
                addRecentSearch(db, callbackData.geneSymbol, callbackData);
                window.location = i.href;
            });
        });
    }

    function init() {
        cart.setCgi('hgSearch');
        cart.debug(debugCartJson);
        // If a user clicks search before the page has finished loading
        // start processing it now:
        $("#searchBarSearchButton").click(sendUserSearch);
        if (typeof cartJson !== "undefined") {
            if (typeof cartJson.db !== "undefined") {
                db = cartJson.db;
            } else {
                alert("Error no database from request");
            }
            if (typeof cartJson.warning !== "undefined") {
                alert("Warning: " + cartJson.warning);
            }
            checkJsonData(cartJson, "init");
            // check right away for a special redirect to hgTracks:
            if (typeof cartJson.warning === "undefined" &&
                    typeof cartJson.positionMatches !== "undefined" &&
                    cartJson.positionMatches.length == 1 &&
                    cartJson.positionMatches[0].matches[0].doRedirect === true) {
                positionMatch = cartJson.positionMatches[0];
                match = positionMatch.matches[0];
                position = match.position;
                newUrl = "../cgi-bin/hgTracks" + "?db=" + db + "&position=" + position;
                if (match.highlight) {
                    newUrl += "&addHighlight=" + encodeURIComponent(match.highlight);
                }
                if (positionMatch.name !== "chromInfo") {
                    newUrl += "&" + positionMatch.name + "=pack";
                }
                window.location.replace(newUrl);
            }
            var urlParts = {};
            if (debugCartJson) {
                console.log('from server:\n', cartJson);
            }
            if (typeof cartJson.search !== "undefined") {
                urlParts = changeUrl({"search": cartJson.search});
            } else {
                urlParts = changeUrl({"db": db});
                cartJson.search = urlParts.urlVars.search;
            }
            _.assign(uiState,cartJson);
            if (typeof cartJson.categs  !== "undefined") {
                _.each(uiState.positionMatches, function(match) {
                    uiState.resultHash[match.name] = match;
                });
                filtersToJstree();
                makeCategoryTree();
            } else {
                cart.send({ getUiState: {db: db} }, handleRefreshState);
                cart.flush();
            }
            $("#searchCategories").bind('ready.jstree', function(e, data) {
                // wait for the category jstree to finish loading before showing the results
                $("#searchBarSearchString").val(uiState.search);
                updateSearchResults(uiState);

                // when a category is checked/unchecked we show/hide that result
                // from the result list
                $("#searchCategories").on('check_node.jstree uncheck_node.jstree', function(e, data) {
                    if ($("#searchResults")[0].children.length > 0) {
                        showOrHideResults(e,data.node);
                    }
                });
                // Reattach event handlers as necessary:
                saveLinkClicks();
            });
            saveHistory(cartJson, urlParts, true);
        } else {
            // no cartJson object means we are coming to the page for the first time:
            cart.send({ getUiState: {db: db} }, handleRefreshState);
            cart.flush();
        }

        initGenomeAutocomplete(); // initialize the genome search autocomplete
        updateCurrentGenomeLabel(); // show the current genome
        changeSearchResultsLabel();
    }

    return { init: init,
             updateSearchResults: updateSearchResults,
             updateFilters: updateFilters,
             updateStateAndPage: updateStateAndPage
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
    hgSearch.updateStateAndPage(event.state, false);
};
