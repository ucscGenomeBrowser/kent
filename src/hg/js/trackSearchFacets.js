/* trackSearchFacets.js - Faceted search UI for track discovery */
/* jshint esversion: 11 */
var trackSearchFacets = (function() {
    "use strict";

    const DEFAULT_PAGE_LENGTH = 100;
    const MIN_FACET_VALUES = 2;       // skip facets with fewer unique values
    const FACET_TOP_VALUES = 8;       // values shown before the "more" link
    const MIN_METADATA_TRACKS = 5;    // only show metadata keys present in this many tracks
    const SEARCH_DEBOUNCE_MS = 200;   // wait this long after the last keystroke to search

    // Assemblies the finder can also search, by the assembly being viewed
    const BROWSE_DBS = {
        hs1: ["hg38", "hg19"],
        hg38: ["hg19"],
        hg19: ["hg38"],
        mm39: ["mm10"],
        mm10: ["mm39"]
    };

    // Rows from every assembly are in one table.  The assembly being viewed
    // is the cart's, and its rows can be turned on.  Rows from the other
    // assemblies, the browsing ones, can only be looked at.
    let viewingDb = "";
    let browseDbs = [];
    let dbErrors = {};
    let multiDb = false;
    let allTracks = [];
    let allGroups = [];
    let allRecTrackSets = [];
    let allRecentTracks = [];
    let allRelatedTracks = [];
    let allFacetDefs = [];
    let tagColors = {};
    // Checked values of facets whose rows hold a list, by row field
    let listFilters = {};
    let serverPosition = "";
    let dataTable = null;
    let focusedDisplayIdx = -1;
    // Supertrack children are folded under their supertrack row while no
    // search or facet filter is active.
    let grouped = true;
    let expandedSupers = new Set();
    // Rows and tracks are keyed by assembly and track name, see rowKey()
    let rowByKey = {};
    let groupFilterAdded = false;
    let selectedTracks = new Set();
    let dataPromises = {};
    let trackByKey = {};
    let facetList = [];
    let tableBuilt = false;
    let lastPageVis = {};
    let lastDrawn = new Set();
    // Display modes and track order picked on the Visible Tracks tab, kept
    // until OK.  Both are reset whenever the dialog opens.
    let pendingVis = {};
    let tabOrder = null;
    // Session id for links, read when the dialog opens
    let sessionId = "";
    // The first load of the dialog, while it runs
    let loadingPromise = null;

    const rowKey = (db, track) => db + " " + track;

    // Key of a track on the assembly being viewed
    const viewingKey = track => rowKey(viewingDb, track);

    const rowIsSelected = row => selectedTracks.has(row.key);

    const escapeRegex = str => str.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

    const toTitleCase = str =>
        str.toLowerCase()
        .split(/[_\s-]+/)
        .map(word => word.charAt(0).toUpperCase() + word.slice(1)).join(" ");

    // DataTables puts display values into cells as HTML, and labels and
    // metadata can come from track hubs
    const textRender = (data, type) => type === "display" ? htmlEncode(data) : data;

    function fetchDbData(db) {
        // Fetch one assembly's track data once per page.  The prefetch hooks
        // and the dialog share the same promise, so a second call never
        // refetches.  The page's visibilities are noted when the viewed
        // assembly's fetch starts, so the first open picks up changes made
        // after it.
        if (dataPromises[db])
            return dataPromises[db];
        if (db === getDb()) {
            lastPageVis = pageVisibilities();
            lastDrawn = drawnTracks();
        }
        var cmd = JSON.stringify({getTrackFinderData: {db: db}});
        var url = "../cgi-bin/hgTracks?hgsid=" + encodeURIComponent(getHgsid()) +
            "&cjCmd=" + encodeURIComponent(cmd);
        dataPromises[db] = fetch(url)
            .then(function(response) {
                if (!response.ok)
                    throw new Error("HTTP " + response.status);
                return response.json();
            })
            .then(function(resp) {
                if (resp.error)
                    throw new Error(resp.error);
                return resp.trackFinderData;
            })
            .catch(function(err) {
                delete dataPromises[db];  // let the next open try again
                throw err;
            });
        return dataPromises[db];
    }

    function pageOnlyTracks(serverTracks) {
        // Tracks hgTracks makes itself, such as duplicates and BLAT results,
        // are not in the server's list.  Take the top-level ones drawn on the
        // page from hgTracks.trackDb.
        var visNames = ["hide", "dense", "full", "pack", "squish"];
        var serverNames = new Set(serverTracks.map(function(t) { return t.track; }));
        var tracks = [];
        Object.keys(hgTracks.trackDb || {}).forEach(function(name) {
            var rec = hgTracks.trackDb[name];
            if (name === "ruler" || serverNames.has(name) || tdbHasParent(rec))
                return;
            var vis = visNames[rec.visibility] || "hide";
            tracks.push({
                track: name,
                shortLabel: rec.shortLabel,
                longLabel: rec.longLabel,
                group: "other",
                groupLabel: "Other",
                type: (rec.type || "").split(" ")[0],
                isComposite: false,
                isSuperTrack: false,
                subtrackCount: 0,
                visibility: vis,
                ownVisibility: vis,
                tags: [],
                facets: {},
                metadata: {}
            });
        });
        return tracks;
    }

    function mergeData(loaded) {
        // Put the tracks of every loaded assembly into allTracks.  loaded
        // starts with the viewed assembly.  Facet definitions and tag colors
        // are the same for every assembly, and the suggestions only exist for
        // the viewed one.
        var viewing = loaded[0];
        allTracks = [];
        allGroups = [];
        var groupNames = new Set();
        loaded.forEach(function(data) {
            var tracks = data.tracks || [];
            if (data === viewing)
                tracks = tracks.concat(pageOnlyTracks(tracks));
            tracks.forEach(function(t) {
                t.db = data.db;
                t.key = rowKey(data.db, t.track);
                t.viewing = (data === viewing);
                allTracks.push(t);
            });
            (data.groups || []).forEach(function(g) {
                if (!groupNames.has(g.name)) {
                    groupNames.add(g.name);
                    allGroups.push(g);
                }
            });
        });
        allRecTrackSets = viewing.recTrackSets || [];
        allRecentTracks = viewing.recentTracks || [];
        allRelatedTracks = viewing.relatedTracks || [];
        allFacetDefs = viewing.facetDefs || [];
        tagColors = viewing.tagColors || {};
        serverPosition = viewing.position || "";
        trackByKey = {};
        allTracks.forEach(function(t) { trackByKey[t.key] = t; });
        multiDb = loaded.length > 1;
    }

    function loadTracks() {
        // Load the viewed assembly and the checked browsing ones and merge
        // them.  A browsing assembly that fails is left out and its error
        // kept in dbErrors.  The viewed assembly failing fails the load.
        var dbs = [viewingDb].concat(browseDbs);
        return Promise.all(dbs.map(function(db) {
            return fetchDbData(db).then(
                function(data) { return {db: db, data: data}; },
                function(err) { return {db: db, error: err}; });
        })).then(function(results) {
            if (results[0].error)
                throw results[0].error;
            dbErrors = {};
            results.forEach(function(r) {
                if (r.error)
                    dbErrors[r.db] = r.error.message;
            });
            mergeData(results.filter(function(r) { return !r.error; })
                .map(function(r) { return r.data; }));
        });
    }

    function prefetch() {
        // Start loading before the user opens the dialog.  Errors are shown
        // when the dialog opens, so ignore them here.
        fetchDbData(getDb()).catch(function() {});
    }

    function buildFacetDimensions() {
        // Built-in facets: group and type
        // Plus any metadata keys that appear in enough tracks.  The counts
        // have no prototype, since hub values like "__proto__" are keys.
        var dbCounts = Object.create(null);
        var groupCounts = Object.create(null);
        var typeCounts = Object.create(null);
        var tagCounts = Object.create(null);
        var facetCounts = Object.create(null);    // trackFacets.ra name -> { value -> count }
        var metaKeyCounts = Object.create(null);  // key -> { value -> count }

        allTracks.forEach(function(track) {
            dbCounts[track.db] = (dbCounts[track.db] || 0) + 1;
            // Group
            var g = track.groupLabel || track.group;
            groupCounts[g] = (groupCounts[g] || 0) + 1;
            // Type
            var t = track.type || "unknown";
            typeCounts[t] = (typeCounts[t] || 0) + 1;
            // Tags, a track counts once for each of its tags
            (track.tags || []).forEach(function(tag) {
                tagCounts[tag] = (tagCounts[tag] || 0) + 1;
            });
            // trackFacets.ra facets, a track counts once for each value
            Object.keys(track.facets || {}).forEach(function(name) {
                if (!facetCounts[name])
                    facetCounts[name] = Object.create(null);
                track.facets[name].forEach(function(val) {
                    facetCounts[name][val] = (facetCounts[name][val] || 0) + 1;
                });
            });
            // Metadata keys
            if (track.metadata) {
                Object.keys(track.metadata).forEach(function(key) {
                    if (!metaKeyCounts[key])
                        metaKeyCounts[key] = Object.create(null);
                    var val = track.metadata[key];
                    metaKeyCounts[key][val] = (metaKeyCounts[key][val] || 0) + 1;
                });
            }
        });

        var facets = [];

        // Assembly facet, once more than one assembly is loaded
        if (multiDb) {
            facets.push({
                name: "Assembly",
                field: "db",
                values: sortByCount(dbCounts)
            });
        }

        // Group facet
        facets.push({
            name: "Group",
            field: "groupLabel",
            values: sortByCount(groupCounts)
        });

        // trackFacets.ra facets in its order.  Each shows as soon as one
        // track has a value.
        allFacetDefs.forEach(function(def) {
            facets.push({
                name: def.label,
                description: def.description,
                field: "facet_" + def.name,
                isList: true,
                minValues: 1,
                values: sortByCount(facetCounts[def.name] || {})
            });
        });

        // Tags facet.  A track can have several tags.
        facets.push({
            name: "Tags",
            field: "tags",
            isList: true,
            minValues: 1,
            values: sortByCount(tagCounts)
        });

        // Type facet
        facets.push({
            name: "Type",
            field: "type",
            values: sortByCount(typeCounts),
            collapsed: true
        });

        // Metadata facets, sorted by coverage
        var metaKeys = Object.keys(metaKeyCounts);
        metaKeys.sort(function(a, b) {
            var countA = Object.values(metaKeyCounts[a]).reduce(function(s, v) { return s + v; }, 0);
            var countB = Object.values(metaKeyCounts[b]).reduce(function(s, v) { return s + v; }, 0);
            return countB - countA;
        });
        metaKeys.forEach(function(key) {
            var totalTracks = Object.values(metaKeyCounts[key]).reduce(function(s, v) { return s + v; }, 0);
            if (totalTracks < MIN_METADATA_TRACKS)
                return;
            var vals = sortByCount(metaKeyCounts[key]);
            if (vals.length >= MIN_FACET_VALUES) {
                facets.push({
                    name: toTitleCase(key),
                    field: "meta_" + key,
                    metaKey: key,
                    values: vals
                });
            }
        });

        return facets;
    }

    function sortByCount(countObj) {
        var entries = [];
        Object.keys(countObj).forEach(function(key) {
            entries.push([key, countObj[key]]);
        });
        entries.sort(function(a, b) {
            if (a[1] !== b[1]) return b[1] - a[1];
            return a[0].localeCompare(b[0]);
        });
        return entries;
    }

    function buildTableData() {
        // Build flat array of row objects for DataTables
        var superKeys = new Set(allTracks.filter(function(t) { return t.isSuperTrack; })
            .map(function(t) { return t.key; }));
        var rows = allTracks.map(function(track) {
            var row = {
                db: track.db,
                key: track.key,
                viewing: track.viewing,
                track: track.track,
                shortLabel: track.shortLabel,
                longLabel: track.longLabel,
                groupLabel: track.groupLabel || track.group,
                type: track.type || "",
                isComposite: track.isComposite,
                isSuperTrack: track.isSuperTrack,
                parentTrack: track.parentTrack || null,
                subtrackCount: track.subtrackCount || 0,
                visibility: track.visibility || "hide",
                ownVisibility: track.ownVisibility || "hide",
                tags: track.tags || [],
                superParent: null,
                superChildren: []
            };
            Object.keys(track.facets || {}).forEach(function(name) {
                row["facet_" + name] = track.facets[name];
            });
            // superParent is the key of the supertrack, in the same assembly
            if (track.parentTrack && superKeys.has(rowKey(track.db, track.parentTrack)))
                row.superParent = rowKey(track.db, track.parentTrack);
            // Flatten metadata into meta_ prefixed keys
            if (track.metadata) {
                Object.keys(track.metadata).forEach(function(key) {
                    row["meta_" + key] = track.metadata[key];
                });
            }
            return row;
        });
        rowByKey = {};
        rows.forEach(function(row) { rowByKey[row.key] = row; });
        rows.forEach(function(row) {
            if (row.superParent)
                rowByKey[row.superParent].superChildren.push(row);
        });
        return rows;
    }

    function superParentRow(row) {
        // Return the supertrack row a row is folded under, or null
        return row.superParent ? rowByKey[row.superParent] : null;
    }

    function refreshGrouping() {
        // Fold supertrack children only when no text search or facet filter is
        // set.  Call after changing a search and before drawing.
        var nowGrouped = !dataTable.search() &&
            !dataTable.columns().search().toArray().some(Boolean) &&
            !Object.values(listFilters).some(function(s) { return s.size > 0; });
        if (nowGrouped !== grouped) {
            grouped = nowGrouped;
            dataTable.rows().invalidate("data");
        }
    }

    function toggleSuper(key) {
        if (expandedSupers.has(key))
            expandedSupers.delete(key);
        else
            expandedSupers.add(key);
        dataTable.rows(function(idx, data) { return data.key === key; })
            .invalidate("data");
        dataTable.draw(false);
    }

    function addGroupFilter() {
        // Hide the children of collapsed supertracks.  DataTables keeps search
        // functions globally, so add this only once.
        if (groupFilterAdded) return;
        groupFilterAdded = true;
        DataTable.ext.search.push(function(settings, searchData, dataIndex, rowData) {
            if (settings.nTable.id !== "theMetaDataTable" || !grouped)
                return true;
            if (!superParentRow(rowData))
                return true;
            return expandedSupers.has(rowData.superParent);
        });
        // Facets whose rows hold a list: keep a row if any of its values
        // is checked
        DataTable.ext.search.push(function(settings, searchData, dataIndex, rowData) {
            if (settings.nTable.id !== "theMetaDataTable")
                return true;
            return Object.keys(listFilters).every(function(field) {
                var checked = listFilters[field];
                if (checked.size === 0)
                    return true;
                return (rowData[field] || []).some(function(v) { return checked.has(v); });
            });
        });
    }

    function groupSortRender(field) {
        // While grouped, a supertrack child sorts on its parent's value so it
        // stays next to the parent in every column's sort order.
        return function(data, type, row) {
            var parent = superParentRow(row);
            if (type === "sort" && grouped && parent)
                return parent[field];
            return textRender(data, type);
        };
    }

    function selectTrackInTable(trackName) {
        // Select a track of the viewed assembly and show its page
        if (!dataTable) return;
        var key = viewingKey(trackName);
        var found = false;
        var targetRow = null;
        dataTable.rows().every(function(idx) {
            if (this.data().key === key) {
                this.select();
                targetRow = idx;
                found = true;
            }
        });
        if (!found) {
            alert("Track '" + trackName + "' is not in the track list.");
            return;
        }
        // Unfold the supertrack so the row can be shown
        var row = rowByKey[key];
        if (grouped && row && row.superParent && !expandedSupers.has(row.superParent))
            toggleSuper(row.superParent);
        // Jump to the page containing this track in display order
        var displayOrder = dataTable.rows({order: 'applied', search: 'applied'})
            .indexes().toArray();
        var displayIdx = displayOrder.indexOf(targetRow);
        if (displayIdx < 0) {
            alert("Track '" + getTrackLabel(trackName) + "' is selected, but the current " +
                "search or filters hide it.");
            return;
        }
        var pageLen = dataTable.page.len();
        if (pageLen > 0)
            dataTable.page(Math.floor(displayIdx / pageLen)).draw('page');
    }

    function getTrackLabel(trackName) {
        var t = trackByKey[viewingKey(trackName)];
        return t ? t.shortLabel : trackName;
    }

    function renderSuggestions(container) {
        var hasRts = allRecTrackSets.length > 0;
        var hasRecent = allRecentTracks.length > 0;
        var hasRelated = allRelatedTracks.length > 0;
        if (!hasRts && !hasRecent && !hasRelated) return;

        var wrapper = document.createElement("div");
        wrapper.id = "facetSuggestions";
        wrapper.style.marginBottom = "8px";
        wrapper.style.padding = "4px 8px";
        wrapper.style.border = "1px solid #ddd";
        wrapper.style.borderRadius = "4px";
        wrapper.style.backgroundColor = "#fafafa";
        wrapper.style.fontSize = "0.9em";

        // Recommended Track Sets
        if (hasRts) {
            var rtsDiv = document.createElement("div");
            rtsDiv.style.marginBottom = "4px";
            var rtsLabel = document.createElement("strong");
            rtsLabel.textContent = "Recommended track sets: ";
            rtsLabel.title = "Clicking a set replaces your current tracks with that set";
            rtsDiv.appendChild(rtsLabel);

            allRecTrackSets.forEach(function(rts) {
                var chip = document.createElement("a");
                chip.className = "rts-chip";
                chip.textContent = rts.label;
                chip.title = rts.description;
                // sessionName goes in as written in the track sets file, the
                // way printRecTrackSets() in hgTracks writes it
                chip.href = "../cgi-bin/hgTracks?hgS_otherUserName=" +
                    encodeURIComponent(rts.userName) +
                    "&rtsLoad=" + rts.sessionName +
                    "&hgsid=" + encodeURIComponent(sessionId) +
                    "&position=" + encodeURIComponent(serverPosition);
                chip.style.display = "inline-block";
                chip.style.margin = "1px 3px";
                chip.style.padding = "1px 8px";
                chip.style.backgroundColor = "#e0e8f0";
                chip.style.borderRadius = "12px";
                chip.style.textDecoration = "none";
                chip.style.color = "#333";
                rtsDiv.appendChild(chip);
            });
            var rtsNote = document.createElement("span");
            rtsNote.textContent = " (loads the set in place of your current tracks)";
            rtsNote.style.color = "#777";
            rtsDiv.appendChild(rtsNote);
            wrapper.appendChild(rtsDiv);
        }

        // Recently Used Tracks
        if (hasRecent) {
            var recentDiv = document.createElement("div");
            recentDiv.style.marginBottom = "4px";
            var recentLabel = document.createElement("strong");
            recentLabel.textContent = "Recently used: ";
            recentDiv.appendChild(recentLabel);

            allRecentTracks.forEach(function(trackName, idx) {
                if (idx > 0) {
                    recentDiv.appendChild(document.createTextNode(" \u00b7 "));
                }
                var link = document.createElement("a");
                link.href = "#";
                link.textContent = getTrackLabel(trackName);
                link.title = "Select " + trackName + " in results";
                link.addEventListener("click", function(e) {
                    e.preventDefault();
                    selectTrackInTable(trackName);
                });
                recentDiv.appendChild(link);
            });
            wrapper.appendChild(recentDiv);
        }

        // Related Tracks, folded away until asked for
        if (hasRelated) {
            var relDiv = document.createElement("details");
            var relLabel = document.createElement("summary");
            relLabel.style.cursor = "pointer";
            var relLabelText = document.createElement("strong");
            relLabelText.textContent = "Related to your visible tracks (" +
                allRelatedTracks.length + ")";
            relLabel.appendChild(relLabelText);
            relDiv.appendChild(relLabel);

            var relList = document.createElement("ul");
            relList.style.margin = "4px 0 0 16px";
            relList.style.padding = "0";
            relList.style.maxHeight = "120px";
            relList.style.overflowY = "auto";
            allRelatedTracks.forEach(function(rel) {
                var li = document.createElement("li");
                var link = document.createElement("a");
                link.href = "#";
                link.textContent = rel.shortLabel;
                link.addEventListener("click", function(e) {
                    e.preventDefault();
                    selectTrackInTable(rel.track);
                });
                li.appendChild(link);
                var reason = document.createElement("span");
                reason.style.color = "#777";
                reason.textContent = " related to " + getTrackLabel(rel.relatedTo) +
                    ": " + rel.why;
                li.appendChild(reason);
                relList.appendChild(li);
            });
            relDiv.appendChild(relList);
            wrapper.appendChild(relDiv);
        }

        container.appendChild(wrapper);
    }

    function dbStatusText() {
        // Errors from browsing assemblies that did not load
        return Object.keys(dbErrors).map(function(db) {
            return "Could not load " + db + ": " + dbErrors[db];
        }).join("; ");
    }

    function renderDbPicker(container) {
        // Checkboxes for the other assemblies this one can search
        var choices = BROWSE_DBS[viewingDb] || [];
        if (choices.length === 0) return;
        var div = document.createElement("div");
        div.id = "finderDbs";
        var label = document.createElement("strong");
        label.textContent = "Also search: ";
        div.appendChild(label);
        choices.forEach(function(db) {
            var dbLabel = document.createElement("label");
            var cb = document.createElement("input");
            cb.type = "checkbox";
            cb.value = db;
            cb.checked = browseDbs.includes(db);
            cb.addEventListener("change", changeBrowseDbs);
            dbLabel.appendChild(cb);
            dbLabel.appendChild(document.createTextNode(" " + db));
            div.appendChild(dbLabel);
        });
        var status = document.createElement("span");
        status.id = "finderDbStatus";
        status.textContent = dbStatusText();
        div.appendChild(status);
        container.appendChild(div);
    }

    function changeBrowseDbs() {
        // Reload the table with the checked assemblies.  Tracks picked on the
        // viewed assembly and the search text are kept, facet filters are not.
        var boxes = Array.from(document.querySelectorAll("#finderDbs input"));
        browseDbs = boxes.filter(function(cb) { return cb.checked; })
            .map(function(cb) { return cb.value; });
        boxes.forEach(function(cb) { cb.disabled = true; });
        var loading = browseDbs.filter(function(db) { return !dataPromises[db]; });
        if (loading.length > 0)
            $("#finderDbStatus").text("Loading " + loading.join(", ") + "...");
        var keep = new Set(selectedTracks);
        var searchText = $("#facetTextSearch").val() || "";
        loadTracks()
            .then(function() {
                dataTable.destroy();
                dataTable = null;
                renderUI(keep, searchText);
                renderActiveTracks();
                dataTable.columns.adjust();
            })
            .catch(function(err) {
                boxes.forEach(function(cb) { cb.disabled = false; });
                $("#finderDbStatus").text("Error loading track data: " + err.message);
            });
    }

    function renderUI(keepSelected, searchText) {
        // Build the dialog contents.  keepSelected is the set of row keys to
        // select, or null to select the tracks that are on.
        var container = document.getElementById("facetSearchContainer");
        if (!container) return;

        container.innerHTML = "";
        listFilters = {};

        renderDbPicker(container);

        // Text search bar
        var searchDiv = document.createElement("div");
        searchDiv.style.marginBottom = "10px";
        var searchInput = document.createElement("input");
        searchInput.type = "text";
        searchInput.id = "facetTextSearch";
        searchInput.placeholder = "Search tracks by name...";
        searchInput.style.width = "100%";
        searchInput.style.padding = "6px";
        searchInput.style.boxSizing = "border-box";
        searchDiv.appendChild(searchInput);
        container.appendChild(searchDiv);

        // Suggestions section (RTS, recently used, related tracks)
        renderSuggestions(container);

        // Main layout: filters sidebar + table
        var layoutDiv = document.createElement("div");
        layoutDiv.id = "container";

        var filtersDiv = document.createElement("div");
        filtersDiv.id = "filters";
        layoutDiv.appendChild(filtersDiv);

        var tableEl = document.createElement("table");
        tableEl.id = "theMetaDataTable";
        tableEl.className = "display";
        var thead = document.createElement("thead");
        tableEl.appendChild(thead);
        layoutDiv.appendChild(tableEl);

        container.appendChild(layoutDiv);

        // Build facets and table.  Facets never change after this, so the
        // text search reuses facetList instead of rebuilding it.
        facetList = buildFacetDimensions();
        var tableData = buildTableData();

        initTable(tableData, facetList, keepSelected);
        initFacetFilters(facetList);
        initTextSearch();
        initKeyboardNav();
        if (searchText) {
            searchInput.value = searchText;
            dataTable.search(searchText);
            refreshGrouping();
            dataTable.draw();
            updateFacetCounts(facetList);
        }
    }

    function viewOnHref(row) {
        // Link to the row's assembly with the track turned on.  A supertrack
        // child needs its supertrack shown too.  hgTracks goes to the last
        // position used on that assembly, or to its default position.
        var params = ["hgsid=" + encodeURIComponent(sessionId),
                      "db=" + encodeURIComponent(row.db),
                      "position=lastDbPos",
                      encodeURIComponent(row.track) + "=" + (row.isSuperTrack ? "show" : "pack")];
        var parent = superParentRow(row);
        if (parent)
            params.push(encodeURIComponent(parent.track) + "=show");
        return "../cgi-bin/hgTracks?" + params.join("&");
    }

    function initTable(tableData, facets, keepSelected) {
        // Only rows of the viewed assembly get a checkbox
        var selectRender = DataTable.render.select();
        var columns = [
            {
                data: null,
                orderable: false,
                width: "30px",
                render: function(data, type, row, meta) {
                    if (!row.viewing)
                        return "";
                    return selectRender(data, type, row, meta);
                }
            },
            {
                data: "shortLabel",
                title: "Track",
                render: function(data, type, row) {
                    if (type === "sort")
                        return groupSortRender("shortLabel")(data, type, row);
                    if (type !== "display") return data;
                    var label = htmlEncode(data);
                    if (row.isComposite && row.subtrackCount > 0)
                        label += " (" + row.subtrackCount + " subtracks)";
                    var toggle = "";
                    if (row.superChildren.length > 0) {
                        var numOn = row.superChildren.filter(function(c) {
                            return rowIsSelected(c);
                        }).length;
                        label += " (" + numOn + " of " + row.superChildren.length + " on)";
                        if (grouped) {
                            var arrow = expandedSupers.has(row.key) ? "▾" : "▸";
                            toggle = '<span class="superToggle" data-key="' +
                                htmlEncode(row.key) +
                                '" title="Show or hide the tracks in this collection"' +
                                ' style="cursor:pointer; padding-right:4px;">' + arrow + '</span>';
                        }
                    }
                    if (grouped && row.superParent)
                        toggle = '<span style="padding-left:1.5em;"></span>';
                    // Settings pages are only for the viewed assembly: hgTrackUi
                    // would switch the session to the row's assembly
                    var html = toggle + label;
                    if (row.viewing) {
                        var href = "../cgi-bin/hgTrackUi?hgsid=" + encodeURIComponent(sessionId) +
                            "&db=" + encodeURIComponent(row.db) +
                            "&g=" + encodeURIComponent(row.track);
                        html = toggle + '<a href="' + href + '" title="Configure track">' +
                            label + '</a>';
                    }
                    if (multiDb)
                        html += '<span class="trackDbLabel">' + htmlEncode(row.db) + '</span>';
                    if (!row.viewing)
                        html += ' <a href="' + htmlEncode(viewOnHref(row)) + '">View on ' +
                            htmlEncode(row.db) + '</a>';
                    return html;
                }
            },
            {
                data: "longLabel",
                title: "Description",
                render: function(data, type, row) {
                    // Tags show as pills after the description
                    if (type === "sort")
                        return groupSortRender("longLabel")(data, type, row);
                    if (type !== "display")
                        return data;
                    var pills = row.tags.map(function(tag) {
                        // Colors come from trackFacets.ra as #rrggbb codes
                        var style = "";
                        if (Object.prototype.hasOwnProperty.call(tagColors, tag))
                            style = ' style="background-color:' + tagColors[tag] + '"';
                        return '<span class="trackTag"' + style + '>' + htmlEncode(tag) + '</span>';
                    }).join("");
                    return htmlEncode(data) + pills;
                }
            },
            { data: "groupLabel", title: "Group", render: groupSortRender("groupLabel") },
            // Only the Type facet filters on this, so keep it out of view
            { data: "type", title: "Type", visible: false, render: groupSortRender("type") },
            // Hidden tiebreak columns that keep a supertrack's children
            // directly under it, parent first, while grouped.
            {
                data: null, visible: false, searchable: false,
                render: function(data, type, row) {
                    var top = (grouped && superParentRow(row)) || row;
                    return textRender(top.shortLabel + "\u0000" + top.key, type);
                }
            },
            {
                data: null, visible: false, searchable: false,
                render: function(data, type, row) {
                    return (grouped && superParentRow(row)) ? 1 : 0;
                }
            },
            { data: "shortLabel", visible: false, searchable: false, render: textRender }
        ];

        // Add metadata columns (hidden, for filtering)
        var metaFacets = facets.filter(function(f) { return f.metaKey; });
        metaFacets.forEach(function(f) {
            columns.push({
                data: f.field,
                title: htmlEncode(f.name),
                visible: false,
                defaultContent: "",
                render: textRender
            });
        });

        // Hidden columns so the text search finds tags and trackFacets.ra
        // values.  Their facets filter on the row lists, not these columns.
        var listFields = ["tags"].concat(allFacetDefs.map(function(def) {
            return "facet_" + def.name;
        }));
        listFields.forEach(function(field) {
            columns.push({
                data: field,
                visible: false,
                defaultContent: "",
                render: function(data, type) { return textRender((data || []).join(" "), type); }
            });
        });

        // The Assembly facet filters on this
        columns.push({ data: "db", visible: false, render: textRender });

        grouped = true;
        expandedSupers = new Set();
        selectedTracks = new Set();
        addGroupFilter();
        var tiebreakIdx = 5;  // first hidden tiebreak column
        dataTable = $("#theMetaDataTable").DataTable({
            data: tableData,
            deferRender: true,
            columns: columns,
            order: [[1, "asc"]],
            orderFixed: { post: [[tiebreakIdx, "asc"], [tiebreakIdx + 1, "asc"],
                                 [tiebreakIdx + 2, "asc"]] },
            pageLength: DEFAULT_PAGE_LENGTH,
            lengthMenu: [[10, 25, 50, 100, -1], [10, 25, 50, 100, "All"]],
            dom: 'lrtip',
            language: { lengthMenu: "Show _MENU_ tracks" },
            select: {
                items: 'row',
                selector: 'td',
                style: 'multi+shift'
            }
        });

        // Keep the supertrack "on" counts current.  A shift-click range can
        // take in rows of other assemblies, so those are deselected again.
        dataTable.on('select deselect', function(e, dt, type, indexes) {
            if (type === 'row') {
                var parents = new Set();
                var browsing = [];
                dataTable.rows(indexes).every(function(idx) {
                    var d = this.data();
                    if (!d.viewing) {
                        browsing.push(idx);
                        return;
                    }
                    if (e.type === 'select')
                        selectedTracks.add(d.key);
                    else
                        selectedTracks.delete(d.key);
                    if (d.superParent)
                        parents.add(d.superParent);
                });
                if (parents.size > 0)
                    dataTable.rows(function(idx, d) { return parents.has(d.key); })
                        .invalidate("data");
                if (e.type === 'select' && browsing.length > 0)
                    dataTable.rows(browsing).deselect();
            }
        });

        // Clicking a supertrack's arrow folds or unfolds it without selecting
        // the row, and rows of other assemblies cannot be selected
        dataTable.on('user-select', function(e, dt, type, cell, originalEvent) {
            if ($(originalEvent.target).closest('.superToggle').length)
                e.preventDefault();
            else if (type === 'row' && !dt.row(cell.index().row).data().viewing)
                e.preventDefault();
        });
        $('#theMetaDataTable tbody').on('click', 'span.superToggle', function() {
            toggleSuper(this.dataset.key);
        });

        // Pre-select the kept tracks, or else the tracks that are on
        dataTable.rows(function(idx, d) {
            if (!d.viewing)
                return false;
            if (keepSelected)
                return keepSelected.has(d.key);
            return d.visibility && d.visibility !== "hide";
        }).select();

        // Reset keyboard focus on page/search changes
        dataTable.on('page.dt search.dt', function() {
            focusedDisplayIdx = -1;
            updateRowFocus();
        });
    }

    function initFacetFilters(facets) {
        var filtersDiv = document.getElementById("filters");
        if (!filtersDiv) return;

        facets.forEach(function(facet, facetIdx) {
            if (facet.values.length < (facet.minValues || MIN_FACET_VALUES)) return;

            var sectionDiv = document.createElement("div");

            // Clickable heading that collapses the body
            var heading = document.createElement("strong");
            heading.textContent = facet.name;
            heading.className = "facet-heading";
            sectionDiv.appendChild(heading);
            // Optional info icon, outside the heading so a click does not collapse
            if (facet.description)
                sectionDiv.appendChild(createInfoIcon(facet.description));

            // Collapsible body: holds Clear button and checkboxes
            var facetBody = document.createElement("div");
            facetBody.className = "facet-body";
            sectionDiv.appendChild(facetBody);
            if (facet.collapsed) {
                heading.classList.add("collapsed");
                facetBody.classList.add("collapsed");
            }
            heading.addEventListener("click", function() {
                var isCollapsed = facetBody.classList.toggle("collapsed");
                heading.classList.toggle("collapsed", isCollapsed);
            });

            var clearBtn = document.createElement("button");
            clearBtn.textContent = "Clear";
            clearBtn.type = "button";
            clearBtn.style.fontSize = "0.8em";
            clearBtn.style.alignSelf = "flex-start";
            facetBody.appendChild(clearBtn);

            // Checkboxes.  Keep each value's count text node for updateFacetCounts.
            var checkboxes = [];
            facet.countNodes = [];
            facet.values.forEach(function(entry) {
                var val = entry[0];
                var count = entry[1];
                var label = document.createElement("label");
                var cb = document.createElement("input");
                cb.type = "checkbox";
                cb.value = val;
                cb.dataset.facetIdx = facetIdx;
                label.appendChild(cb);
                var textNode = document.createTextNode(" " + val + " (" + count + ")");
                label.appendChild(textNode);
                facetBody.appendChild(label);
                checkboxes.push(cb);
                facet.countNodes.push({val: val, textNode: textNode});
            });

            // Show the top values only, plus any checked ones, until "more"
            // is clicked
            if (checkboxes.length > FACET_TOP_VALUES) {
                var expanded = false;
                var moreLink = document.createElement("a");
                moreLink.href = "#";
                moreLink.className = "facet-more";
                facetBody.appendChild(moreLink);
                var showValues = function() {
                    var numHidden = 0;
                    checkboxes.forEach(function(c, i) {
                        var show = expanded || i < FACET_TOP_VALUES || c.checked;
                        c.parentNode.style.display = show ? "" : "none";
                        if (!show)
                            numHidden++;
                    });
                    if (expanded)
                        moreLink.textContent = "Show fewer";
                    else
                        moreLink.textContent = "Show " + numHidden + " more";
                };
                moreLink.addEventListener("click", function(e) {
                    e.preventDefault();
                    expanded = !expanded;
                    showValues();
                });
                showValues();
            }

            // A list facet filters through listFilters, the others through a
            // regex on their DataTable column
            var colIdx = facet.isList ? -1 : getColumnIndexForFacet(facet);
            if (facet.isList)
                listFilters[facet.field] = new Set();

            function applyFilter() {
                var checked = checkboxes.filter(function(c) { return c.checked; })
                    .map(function(c) { return c.value; });
                if (facet.isList) {
                    listFilters[facet.field] = new Set(checked);
                } else {
                    var query = "";
                    if (checked.length)
                        query = "^(" + checked.map(escapeRegex).join("|") + ")$";
                    dataTable.column(colIdx).search(query, true, false);
                }
                refreshGrouping();
                dataTable.draw();
                updateFacetCounts(facets);
            }

            checkboxes.forEach(function(cb) {
                cb.addEventListener("change", applyFilter);
            });

            clearBtn.addEventListener("click", function() {
                checkboxes.forEach(function(cb) { cb.checked = false; });
                if (showValues)
                    showValues();
                applyFilter();
            });

            filtersDiv.appendChild(sectionDiv);
        });
    }

    function getColumnIndexForFacet(facet) {
        // Columns: [checkbox, shortLabel, longLabel, groupLabel, type, ...metaCols]
        if (facet.field === "groupLabel") return 3;
        if (facet.field === "type") return 4;
        // For metadata facets, find the column index
        var cols = dataTable.settings()[0].aoColumns;
        for (var i = 0; i < cols.length; i++) {
            if (cols[i].data === facet.field) return i;
        }
        return -1;
    }

    function updateFacetCounts(facets) {
        // Recount facet values from currently visible rows.  While grouped no
        // filter is set, so count every row, folded children included.
        var visibleData = dataTable.rows(grouped ? {} : { search: "applied" }).data().toArray();

        facets.forEach(function(facet) {
            if (!facet.countNodes) return;
            var counts = Object.create(null);
            visibleData.forEach(function(row) {
                if (facet.isList) {
                    (row[facet.field] || []).forEach(function(val) {
                        counts[val] = (counts[val] || 0) + 1;
                    });
                    return;
                }
                var val = row[facet.field] || "";
                counts[val] = (counts[val] || 0) + 1;
            });
            facet.countNodes.forEach(function(n) {
                n.textNode.textContent = " " + n.val + " (" + (counts[n.val] || 0) + ")";
            });
        });
    }

    function initTextSearch() {
        var input = document.getElementById("facetTextSearch");
        if (!input || !dataTable) return;
        // Search once typing pauses, not on every keystroke
        var timer = null;
        input.addEventListener("input", function() {
            clearTimeout(timer);
            timer = setTimeout(function() {
                dataTable.search(input.value);
                refreshGrouping();
                dataTable.draw();
                updateFacetCounts(facetList);
            }, SEARCH_DEBOUNCE_MS);
        });
    }

    function pendingVisFor(row) {
        // The display mode a track will get on OK if it is on
        if (pendingVis[row.track])
            return pendingVis[row.track];
        if (row.visibility && row.visibility !== "hide")
            return row.visibility;
        return "pack";
    }

    function collectChanges() {
        // Return object of track visibility changes {trackName: newVis} for
        // the viewed assembly
        if (!dataTable) return {};
        var changes = {};
        dataTable.rows().every(function() {
            var row = this.data();
            if (!row.viewing)
                return;
            var isSelected = this.selected();
            var wasVisible = row.visibility && row.visibility !== "hide";
            if (isSelected && !wasVisible) {
                // A supertrack is only ever shown or hidden
                changes[row.track] = row.isSuperTrack ? "show" : pendingVisFor(row);
            } else if (isSelected && pendingVis[row.track] &&
                       pendingVis[row.track] !== row.visibility) {
                changes[row.track] = pendingVis[row.track];
            } else if (!isSelected && wasVisible) {
                changes[row.track] = "hide";
            }
        });
        return changes;
    }

    function checkParentChildWarnings(changes) {
        // Check if any newly-enabled supertracks have no visible children,
        // and warn the user.  Composite subtracks are not in the track list,
        // and the server already shows a composite with none on as hidden.
        // Build a map of parentTrack -> [child row data] from all tracks
        var childMap = {};
        if (!allTracks) return [];
        allTracks.forEach(function(row) {
            if (row.viewing && row.parentTrack) {
                if (!childMap[row.parentTrack])
                    childMap[row.parentTrack] = [];
                childMap[row.parentTrack].push(row);
            }
        });

        var warnings = [];
        for (var trackName in changes) {
            if (changes[trackName] === "hide") continue;
            var trackRow = trackByKey[viewingKey(trackName)];
            if (!trackRow) continue;
            if (!trackRow.isSuperTrack) continue;
            var children = childMap[trackName];
            if (!children || children.length === 0) continue;
            // Check if any child is visible or being turned on
            var anyChildOn = false;
            for (var i = 0; i < children.length; i++) {
                var c = children[i];
                if (changes[c.track] && changes[c.track] !== "hide") {
                    anyChildOn = true;
                    break;
                }
                if (c.ownVisibility && c.ownVisibility !== "hide") {
                    anyChildOn = true;
                    break;
                }
            }
            if (!anyChildOn) {
                warnings.push(trackRow.shortLabel + " (" + trackRow.track +
                    ") is enabled but has no visible tracks." +
                    " Configure it to select which tracks to display.");
            }
        }
        return warnings;
    }

    function applyChanges() {
        // Update cart via AJAX and refresh track image
        var changes = collectChanges();
        var moved = applyOrder();
        pendingVis = {};
        tabOrder = null;
        if (Object.keys(changes).length === 0) {
            // Only the order changed, and the rows have already moved
            if (moved)
                cart.setVarsObj({});
            return;
        }

        // A track inside a hidden supertrack is not drawn, so turn the
        // supertrack on too and tick it in the table.  Its other children
        // that are on by default are hidden, so only the ticked ones appear.
        Object.keys(changes).forEach(function(t) {
            var parent = superParentRow(rowByKey[viewingKey(t)]);
            if (changes[t] === "hide" || !parent || parent.visibility !== "hide")
                return;
            changes[parent.track] = "show";
            dataTable.rows(function(idx, d) { return d.key === parent.key; }).select();
            parent.superChildren.forEach(function(child) {
                if (!selectedTracks.has(child.key) && child.ownVisibility !== "hide") {
                    changes[child.track] = "hide";
                    child.ownVisibility = "hide";
                }
            });
        });

        // Warn about parent tracks with no visible children
        var warnings = checkParentChildWarnings(changes);
        if (warnings.length > 0)
            alert("Note:\n\n" + warnings.join("\n\n"));

        // Hide drawn tracks the same way right-click hide does, so a container
        // that loses its last drawn child is hidden too.
        var cartVars = [], cartVals = [];
        var hideList = Object.keys(changes).filter(function(t) { return changes[t] === "hide"; });
        var drawnList = hideList.filter(function(t) { return hgTracks.trackDb[t]; });
        var family = tdbFindChildless(hgTracks.trackDb, drawnList);
        family.loneParents.forEach(function(lp) {
            cartVars.push(lp[0]);
            cartVals.push("hide");
            changes[lp[0]] = "hide";
        });
        family.others.forEach(function(t) { cartHideAnyTrack(t, cartVars, cartVals); });
        Object.keys(changes).forEach(function(t) {
            if (changes[t] !== "hide") {
                cartVars.push(t);
                cartVals.push(changes[t]);
            } else if (!hgTracks.trackDb[t]) {
                cartVars.push(t);
                cartVals.push("hide");
            }
        });

        // Update the track list below the image.  A supertrack child is listed
        // under its supertrack there, so add the supertrack.  vis.update runs
        // after the add because a copied select does not keep its value.  The
        // visible tracks group is laid out again once, after all removals.
        var removed = false;
        Object.keys(changes).forEach(function(t) {
            if (changes[t] === "hide") {
                vis.update(t, "hide");
                removed = removeTrackFromVisibleGroup(t, true) || removed;
            } else {
                var row = rowByKey[viewingKey(t)];
                var parent = row && superParentRow(row);
                addTrackToVisibleGroup(parent ? parent.track : t);
                vis.update(t, changes[t]);
                // The image will draw it, so a later hide from the image is noticed
                lastDrawn.add(t);
                if (parent)
                    lastDrawn.add(parent.track);
            }
        });
        if (removed)
            reflowVisibleGroup();
        // The selects now match the dialog, so later page changes are noticed
        lastPageVis = pageVisibilities();

        Object.keys(changes).forEach(function(t) { setTrackVisibility(t, changes[t]); });

        // Queue cart updates, then navigateInPlace sends them with the image request
        cart.addVarsToQueue(cartVars, cartVals);
        imageV2.navigateInPlace("db=" + getDb(), null, false);

        // Refresh visible tracks tab
        renderActiveTracks();
    }

    function initKeyboardNav() {
        // Inject focus style once
        if (!document.getElementById('dtRowFocusStyle')) {
            var style = document.createElement('style');
            style.id = 'dtRowFocusStyle';
            style.textContent =
                '#theMetaDataTable tbody tr.dt-row-focus { outline: 2px solid #4a90d9; }';
            document.head.appendChild(style);
        }

        var tbody = document.querySelector('#theMetaDataTable tbody');
        if (!tbody) return;
        tbody.setAttribute('tabindex', '0');
        tbody.style.outline = 'none';

        var searchInput = document.getElementById('facetTextSearch');

        // Tab or Down Arrow from search bar moves focus into the table
        if (searchInput) {
            searchInput.addEventListener('keydown', function(e) {
                if ((e.key === 'Tab' && !e.shiftKey) || e.key === 'ArrowDown') {
                    e.preventDefault();
                    tbody.focus();
                    focusedDisplayIdx = 0;
                    updateRowFocus();
                }
            });
        }

        // Keyboard navigation within the table
        tbody.addEventListener('keydown', function(e) {
            var pageRowCount = dataTable.rows({page: 'current'}).indexes().length;

            if (e.key === 'ArrowDown') {
                e.preventDefault();
                if (focusedDisplayIdx < pageRowCount - 1)
                    focusedDisplayIdx++;
                else if (focusedDisplayIdx === -1)
                    focusedDisplayIdx = 0;
                updateRowFocus();
            } else if (e.key === 'ArrowUp') {
                e.preventDefault();
                if (focusedDisplayIdx > 0) {
                    focusedDisplayIdx--;
                    updateRowFocus();
                } else if (focusedDisplayIdx === 0) {
                    // Up from first row goes back to search bar
                    focusedDisplayIdx = -1;
                    updateRowFocus();
                    if (searchInput) searchInput.focus();
                }
            } else if (e.key === ' ' || (e.key === 'Enter' && focusedDisplayIdx >= 0)) {
                e.preventDefault();
                toggleFocusedRow();
            } else if (e.key === 'Tab' && !e.shiftKey) {
                e.preventDefault();
                focusedDisplayIdx = -1;
                updateRowFocus();
                $('#trackDiscoveryDialog').closest('.ui-dialog')
                    .find('.ui-dialog-buttonpane button').first().focus();
            } else if (e.key === 'Tab' && e.shiftKey) {
                e.preventDefault();
                focusedDisplayIdx = -1;
                updateRowFocus();
                if (searchInput) searchInput.focus();
            }
        });
    }

    function updateRowFocus() {
        $('#theMetaDataTable tbody tr.dt-row-focus').removeClass('dt-row-focus');
        if (focusedDisplayIdx < 0) return;
        var rows = $('#theMetaDataTable tbody tr');
        if (focusedDisplayIdx < rows.length) {
            $(rows[focusedDisplayIdx]).addClass('dt-row-focus');
            rows[focusedDisplayIdx].scrollIntoView({block: 'nearest'});
        }
    }

    function toggleFocusedRow() {
        if (focusedDisplayIdx < 0 || !dataTable) return;
        var pageIndexes = dataTable.rows({page: 'current'}).indexes().toArray();
        if (focusedDisplayIdx < pageIndexes.length) {
            var row = dataTable.row(pageIndexes[focusedDisplayIdx]);
            if (!row.data().viewing)
                return;
            if (row.selected())
                row.deselect();
            else
                row.select();
        }
    }

    function imageRows(tabTracks) {
        // Match the image rows to the tracks on the Visible Tracks tab.  A
        // composite draws one row per subtrack, so a row belongs to the tab
        // track named by the row itself, its composite, or its top parent.
        // Returns the rows in image order, the owner of each, the rows of each
        // tab track, and the tab tracks in the order their first row appears.
        var result = { rows: [], owner: new Map(), rowsOf: {}, order: [] };
        document.querySelectorAll("#imgTbl tr.imgOrd").forEach(function(tr) {
            result.rows.push(tr);
            var name = tr.id.substring("tr_".length);
            var rec = (hgTracks.trackDb && hgTracks.trackDb[name]) || {};
            var owner = [name, rec.parentTrack, rec.topParent].find(function(c) {
                return c && tabTracks.has(c);
            });
            if (!owner) return;
            result.owner.set(tr, owner);
            if (!result.rowsOf[owner]) {
                result.rowsOf[owner] = [];
                result.order.push(owner);
            }
            result.rowsOf[owner].push(tr);
        });
        return result;
    }

    function pendingVisibleTracks() {
        // Tracks that will be on after OK.  "drawn" have rows in the image and
        // are listed in tabOrder, which starts as the image order after an
        // open or an apply.  "other" are not in the image: the ideogram, and
        // tracks turned on in this dialog.  Supertracks are left out because
        // they have no row of their own.
        var names = allTracks.filter(function(t) {
            return t.viewing && selectedTracks.has(t.key) && !t.isSuperTrack;
        }).map(function(t) { return t.track; });
        var drawnOrder = imageRows(new Set(names)).order;
        if (!tabOrder) {
            tabOrder = drawnOrder;
        } else {
            var drawn = new Set(drawnOrder);
            var old = new Set(tabOrder);
            tabOrder = tabOrder.filter(function(t) { return drawn.has(t); })
                .concat(drawnOrder.filter(function(t) { return !old.has(t); }));
        }
        var inTab = new Set(tabOrder);
        return {
            drawn: tabOrder,
            other: names.filter(function(t) { return !inTab.has(t); })
        };
    }

    function orderChanged() {
        if (!tabOrder) return false;
        var imageOrder = imageRows(new Set(tabOrder)).order;
        return tabOrder.some(function(t, i) { return t !== imageOrder[i]; });
    }

    function applyOrder() {
        // Move the image rows to match the Visible Tracks tab and queue the
        // new positions the way dragReorder.setOrder() saves them.  A track's
        // rows move together, and rows not on the tab keep their places.
        // Return true if anything moved.
        if (!orderChanged()) return false;
        var image = imageRows(new Set(tabOrder));
        var wanted = [];
        tabOrder.forEach(function(t) { wanted = wanted.concat(image.rowsOf[t]); });
        var parent = image.rows[0].parentNode;
        image.rows.forEach(function(tr) {
            parent.appendChild(image.owner.has(tr) ? wanted.shift() : tr);
        });
        var varsToUpdate = {};
        $("tr.imgOrd").each(function() {
            $(this).attr('abbr', $(this).prop('rowIndex').toString());
            varsToUpdate[this.id.substring('tr_'.length) + '_imgOrd'] = $(this).attr('abbr');
        });
        cart.queueVarsObj(varsToUpdate);
        dragReorder.zipButtons($("#imgTbl"));
        imageV2.markAsDirtyPage();
        return true;
    }

    function renderActiveTracks() {
        // Changes made on this tab only take effect on OK: hiding deselects the
        // track in the search table, and display modes and order are kept in
        // pendingVis and tabOrder.
        var container = document.getElementById("activeTracksContainer");
        if (!container) return;
        container.innerHTML = "";

        var pending = pendingVisibleTracks();
        if (pending.drawn.length === 0 && pending.other.length === 0) {
            container.innerHTML = "<p>No tracks are set to be visible.</p>";
            return;
        }

        var topDiv = document.createElement("div");
        topDiv.style.marginBottom = "8px";
        var hideAllBtn = document.createElement("input");
        hideAllBtn.type = "button";
        hideAllBtn.value = "Hide all";
        hideAllBtn.style.fontSize = "0.8em";
        hideAllBtn.addEventListener("click", function() {
            dataTable.rows().deselect();
            renderActiveTracks();
        });
        topDiv.appendChild(hideAllBtn);
        var note = document.createElement("span");
        note.textContent = " Drag the arrows to change the track order. " +
            "Changes take effect when you click OK.";
        note.style.color = "#777";
        topDiv.appendChild(note);
        container.appendChild(topDiv);

        // One table so both lists share columns.  Tracks in the image, in
        // image order, are in the first tbody and can be dragged.
        var table = document.createElement("table");
        table.style.borderCollapse = "collapse";
        table.style.fontSize = "0.9em";
        var tbody = document.createElement("tbody");
        table.appendChild(tbody);
        pending.drawn.forEach(function(trackName) {
            tbody.appendChild(makeTrackRow(trackName, true));
        });
        makeRowsDraggable(tbody);

        // Tracks not in the image cannot be ordered here
        if (pending.other.length > 0) {
            var otherBody = document.createElement("tbody");
            var headRow = document.createElement("tr");
            var headCell = document.createElement("td");
            headCell.colSpan = 4;
            headCell.style.padding = "12px 8px 4px 8px";
            headCell.style.color = "#777";
            headCell.textContent = "Not in the image: the chromosome ideogram, " +
                "tracks with nothing to draw in this region, and tracks turned on " +
                "here, which are added at the bottom of the image.";
            headRow.appendChild(headCell);
            otherBody.appendChild(headRow);
            pending.other.forEach(function(trackName) {
                otherBody.appendChild(makeTrackRow(trackName, false));
            });
            table.appendChild(otherBody);
        }
        container.appendChild(table);
    }

    function makeRowsDraggable(tbody) {
        // Rows move with native drag and drop, started only from the arrow
        // cell.  The new order is saved to tabOrder when a drag ends.
        var dragRow = null;
        tbody.addEventListener("dragover", function(e) {
            e.preventDefault();
            var over = e.target.closest("tr");
            if (!dragRow || !over || over === dragRow) return;
            var rect = over.getBoundingClientRect();
            if (e.clientY < rect.top + rect.height / 2)
                over.before(dragRow);
            else
                over.after(dragRow);
        });
        tbody.addEventListener("drop", function(e) { e.preventDefault(); });
        Array.from(tbody.rows).forEach(function(tr) {
            var handle = tr.querySelector("td.dragArrow");
            handle.addEventListener("mousedown", function() { tr.draggable = true; });
            handle.addEventListener("mouseup", function() { tr.draggable = false; });
            tr.addEventListener("dragstart", function(e) {
                dragRow = tr;
                e.dataTransfer.effectAllowed = "move";
                e.dataTransfer.setData("text/plain", tr.dataset.track);
            });
            tr.addEventListener("dragend", function() {
                tr.draggable = false;
                dragRow = null;
                tabOrder = Array.from(tbody.rows).map(function(r) { return r.dataset.track; });
            });
        });
    }

    function makeTrackRow(trackName, draggable) {
        // One Visible Tracks row: drag arrow (or an empty cell), label linked
        // to the track settings, display mode menu and Hide button
        var track = trackByKey[viewingKey(trackName)];
        var tr = document.createElement("tr");
        tr.dataset.track = trackName;
        tr.style.borderBottom = "1px solid #eee";

        var tdDrag = document.createElement("td");
        tdDrag.style.padding = "4px 8px";
        tdDrag.style.width = "1em";
        if (draggable) {
            tdDrag.className = "dragArrow";
            tdDrag.style.cursor = "grab";
            tdDrag.textContent = "↕";
            tdDrag.title = "Drag to change the track order";
        }
        tr.appendChild(tdDrag);

        // Track name
        var tdName = document.createElement("td");
        tdName.style.padding = "4px 8px";
        var link = document.createElement("a");
        link.href = "../cgi-bin/hgTrackUi?hgsid=" + encodeURIComponent(sessionId) +
            "&db=" + encodeURIComponent(viewingDb) + "&g=" + encodeURIComponent(track.track);
        link.textContent = track.shortLabel;
        link.title = "Configure " + track.shortLabel;
        tdName.appendChild(link);
        tr.appendChild(tdName);

        // Visibility dropdown
        var tdVis = document.createElement("td");
        tdVis.style.padding = "4px 8px";
        var select = document.createElement("select");
        select.style.fontSize = "0.9em";
        var visOptions = ["dense", "squish", "pack", "full"];
        var curVis = pendingVisFor(rowByKey[viewingKey(trackName)]);
        visOptions.forEach(function(v) {
            var opt = document.createElement("option");
            opt.value = v;
            opt.textContent = v;
            if (v === curVis)
                opt.selected = true;
            select.appendChild(opt);
        });
        select.addEventListener("change", function() {
            pendingVis[trackName] = this.value;
        });
        tdVis.appendChild(select);
        tr.appendChild(tdVis);

        // Hide button
        var tdHide = document.createElement("td");
        tdHide.style.padding = "4px 8px";
        var hideBtn = document.createElement("input");
        hideBtn.type = "button";
        hideBtn.value = "Hide";
        hideBtn.style.fontSize = "0.8em";
        hideBtn.addEventListener("click", function() {
            var key = viewingKey(trackName);
            dataTable.rows(function(idx, d) { return d.key === key; }).deselect();
            renderActiveTracks();
        });
        tdHide.appendChild(hideBtn);
        tr.appendChild(tdHide);

        return tr;
    }

    function pageVisibilities() {
        // Return {track: vis} from the visibility selects in the track list
        // below the image.  This can run before the table is built, so it
        // keeps every select; setTrackVisibility skips tracks not listed.
        var result = Object.create(null);
        $('select.normalText,select.hiddenText').each(function() {
            if (!(this.name in result))
                result[this.name] = this.value;
        });
        return result;
    }

    function drawnTracks() {
        // Return the set of tracks in the image, counting a container as drawn
        // while any of its tracks is
        var drawn = new Set();
        Object.keys(hgTracks.trackDb || {}).forEach(function(t) {
            var rec = hgTracks.trackDb[t];
            drawn.add(t);
            if (rec.parentTrack)
                drawn.add(rec.parentTrack);
            if (rec.topParent)
                drawn.add(rec.topParent);
        });
        return drawn;
    }

    function setTrackVisibility(trackName, newVis) {
        // Record a new visibility in both copies of the track data
        var key = viewingKey(trackName);
        var row = rowByKey[key];
        if (row)
            row.visibility = newVis;
        if (trackByKey[key])
            trackByKey[key].visibility = newVis;
    }

    function syncFromPage() {
        // Pick up visibility changes made on the page since the dialog last
        // looked, then reset the selection to what is actually on.  Only
        // tracks whose select changed are taken from the page, since the
        // server's view of a container can differ from its select.  Hiding a
        // track from the image does not always change its select, so a track
        // that has left the image also counts as hidden.  Changes left from a
        // Cancel are dropped.
        pendingVis = {};
        tabOrder = null;
        var nowVis = pageVisibilities();
        Object.keys(nowVis).forEach(function(t) {
            if (nowVis[t] !== lastPageVis[t])
                setTrackVisibility(t, nowVis[t]);
        });
        lastPageVis = nowVis;
        var nowDrawn = drawnTracks();
        lastDrawn.forEach(function(t) {
            if (!nowDrawn.has(t))
                setTrackVisibility(t, "hide");
        });
        lastDrawn = nowDrawn;
        dataTable.rows(function(idx, d) { return d.viewing && d.visibility === "hide"; })
            .deselect();
        dataTable.rows(function(idx, d) { return d.viewing && d.visibility !== "hide"; })
            .select();
    }

    function positionDialog() {
        // Fixed positioning must be set after dialog() creates the wrapper,
        // and the position after that, to avoid a document-relative offset
        var wrap = $('#trackDiscoveryDialog').closest('.ui-dialog');
        wrap.css({
            'position': 'fixed',
            'top': '20px',
            'left': Math.max(0, ($(window).width() - wrap.outerWidth()) / 2) + 'px'
        });
    }

    function advancedTabActive() {
        // True when the Advanced Search tab is the one showing
        return $('#discoveryAdvancedTab').is(':visible');
    }

    function loadAdvancedSearch() {
        // The Advanced Search tab shows the hgTracks track search page in a
        // frame.  It is loaded the first time the tab is shown.
        var panel = document.getElementById('discoveryAdvancedTab');
        if (!panel || document.getElementById('advancedSearchFrame'))
            return;
        var frame = document.createElement('iframe');
        frame.id = 'advancedSearchFrame';
        frame.title = 'Advanced track search';
        frame.src = "../cgi-bin/hgTracks?hgt_tSearch=track+search&hgt_tsEmbed=1&hgsid=" +
                    encodeURIComponent(sessionId);
        panel.appendChild(frame);
    }

    function submitAdvancedSearch() {
        // Send the tracks picked on the Advanced Search tab to the browser.
        // The results form targets the top window.  Returns false when the
        // tab has no search results.
        var frame = document.getElementById('advancedSearchFrame');
        if (!frame || !frame.contentDocument)
            return false;
        var btn = frame.contentDocument.querySelector('form#searchResults input.viewBtn');
        if (!btn)
            return false;
        btn.click();
        return true;
    }

    function showTab(tab) {
        // Show the named tab, if the dialog has it
        if (tab !== 'advanced')
            return;
        var panels = $('#discoveryTabs').children('div');
        var idx = panels.index($('#discoveryAdvancedTab'));
        if (idx >= 0)
            $('#discoveryTabs').tabs('option', 'active', idx);
    }

    function openDialog(tab) {
        // Called from hgTracks page "Find Tracks" button.  tab is 'advanced'
        // to open on the Advanced Search tab.
        var dlg = $('#trackDiscoveryDialog');
        if (dlg.length === 0) return;
        sessionId = getHgsid();

        // If already open, reposition and focus it
        if (dlg.hasClass('ui-dialog-content') && dlg.dialog('isOpen')) {
            positionDialog();
            dlg.dialog('moveToTop');
            showTab(tab);
            return;
        }

        // Open dialog first so container is visible (DataTables needs this)
        if (dlg.hasClass('ui-dialog-content')) {
            dlg.dialog('open');
        } else {
            // The Visible Tracks tab shows selections made on the Search tab
            $('#discoveryTabs').tabs({
                activate: function(event, ui) {
                    if (ui.newPanel.attr('id') === 'discoveryAdvancedTab')
                        loadAdvancedSearch();
                    else if (tableBuilt)
                        renderActiveTracks();
                }
            });
            dlg.dialog({
                title: 'Find Tracks',
                width: $(window).width() - 40,
                height: $(window).height() - 40,
                modal: true,
                resizable: true,
                closeOnEscape: true,
                buttons: {
                    // On the Advanced Search tab, OK sends that tab's picks
                    // and reloads the page
                    "OK": function() {
                        if (advancedTabActive() && submitAdvancedSearch())
                            return;
                        applyChanges();
                        $(this).dialog('close');
                    },
                    // Reopening resets the selection to what is on the page
                    "Cancel": function() {
                        $(this).dialog('close');
                    }
                }
            });
        }
        positionDialog();
        showTab(tab);

        // The table is built once per page and kept between opens
        if (tableBuilt) {
            syncFromPage();
            renderActiveTracks();
            dataTable.columns.adjust();
            $('#facetTextSearch').focus();
            return;
        }

        // A second open while the first load runs waits for that load
        if (loadingPromise)
            return;
        var container = document.getElementById("facetSearchContainer");
        container.textContent = "Loading tracks...";
        viewingDb = getDb();
        loadingPromise = loadTracks()
            .then(function() {
                renderUI(null, "");
                tableBuilt = true;
                // Pick up page changes made since the fetch started
                syncFromPage();
                renderActiveTracks();
                dataTable.columns.adjust();
                $('#facetTextSearch').focus();
            })
            .catch(function(err) {
                container.textContent = "Error loading track data: " + err.message;
            })
            .finally(function() {
                loadingPromise = null;
            });
    }

    function initPrefetch() {
        // Load the track data when the user looks likely to open the dialog:
        // pointing at or tabbing to the Find Tracks button, or pressing "f",
        // the first key of the "f t" shortcut
        if (!document.getElementById('trackDiscoveryDialog')) return;
        var btn = document.getElementById('hgt.findTracks');
        if (btn) {
            btn.addEventListener('mouseenter', prefetch, {once: true});
            btn.addEventListener('focus', prefetch, {once: true});
        }
        var onKey = function(e) {
            if (e.key !== 'f' || e.target.closest('input, textarea, select, [contenteditable]'))
                return;
            prefetch();
            document.removeEventListener('keydown', onKey);
        };
        document.addEventListener('keydown', onKey);
    }

    $(initPrefetch);

    return { openDialog: openDialog };
})();
