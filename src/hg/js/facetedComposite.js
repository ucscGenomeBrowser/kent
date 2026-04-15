// SPDX-License-Identifier: MIT; (c) 2025 Andrew D Smith (author)
/* jshint esversion: 11 */
$(function() {
    /* ADS: Uncomment below to force confirm on unload/reload */
    // window.addEventListener("beforeunload", function (e) {
    //     e.preventDefault(); e.returnValue = ""; });
    const DEFAULT_MAX_CHECKBOXES = 20;  // ADS: without default, can get crazy

    const isValidColorMap = obj =>  // check the whole thing and ignore if invalid
          typeof obj === "object" && obj !== null && !Array.isArray(obj) &&
          Object.values(obj).every(x =>
              typeof x === "object" && x !== null && !Array.isArray(x) &&
                  Object.values(x).every(value => typeof value === "string"));

    // fetch file dynamically
    const loadOptional = (url, hgsid, track) =>  { // load if possible otherwise carry on
        if (!url) return Promise.resolve(null);
        let fetchBody = `fileUrl=${url}&track=${track}`;
        if (hgsid !== null) {
            fetchBody = fetchBody + `&hgsid=${hgsid}`;
        }
        const fetchUrl = `/cgi-bin/hgTrackUi?${fetchBody}`;
        const req = (fetchUrl.length > 2048 || embeddedData.udcTimeout) ?
            fetch("/cgi-bin/hgTrackUi", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: fetchBody,
            })
            : fetch(fetchUrl, {
                method: "GET",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
            });
        return req.then(r => r.ok ? r.json() : null).catch(() => null);
    };

    const toTitleCase = str =>
          str.toLowerCase()
          .split(/[_\s-]+/)  // Split on underscore, space, or hyphen
          .map(word => word.charAt(0).toUpperCase() + word.slice(1)).join(" ");

    const escapeRegex = str => str.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

    const embeddedData = (() => {
        // get data that was embedded in the HTML here to use them as globals
        const dataTag = document.getElementById("app-data");
        return dataTag ? JSON.parse(dataTag.innerText) : "";
    })();

    // Store initial checkbox states for delta computation on server
    const initialState = {
        dataElements: new Set(),
        dataTypes: new Set()
    };

    function generateHTML() {
        const container = document.createElement("div");
        container.id = "myTag";
        container.innerHTML = `
        <div id="dataTypeSelector"></div>
        <div id="container">
            <div id="filters"></div>
            <table id="theMetaDataTable">
                <thead></thead>
                <tfoot></tfoot>
            </table>
        </div>
        `;
        // Instead of appending to body, append into the placeholder div
        document.getElementById("metadata-placeholder").appendChild(container);
    }

    function updateVisibilities(uriForUpdate, submitBtnEvent) {
        // get query params from URL
        const paramsFromUrl = new URLSearchParams(window.location.search);
        const db = paramsFromUrl.get("db");
        const hgsid = paramsFromUrl.get("hgsid");
        let body = `${uriForUpdate}`;
        if (db !== null) {
            body = body + `&db=${db}`;
        }
        if (hgsid !== null) {
            body = body + `&hgsid=${hgsid}`;
        }
        fetch("/cgi-bin/cartDump", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: body,
        }).then(() => {
            // 'disable' any CSS named elements here to them keep out of cart
            const dtLength = submitBtnEvent.
                  target.form.querySelector("select[name$='_length']");
            if (dtLength) {
                dtLength.disabled = true;
            }
            submitBtnEvent.target.form.submit();  // release submit event
        });
    }

    function initDataTypeSelector() {
        // Skip if no dataTypes defined or empty object
        if (!embeddedData.dataTypes || Object.keys(embeddedData.dataTypes).length === 0) {
            return;
        }

        const selector = document.getElementById("dataTypeSelector");
        selector.appendChild(Object.assign(document.createElement("label"), {
            innerHTML: "<b>Subtrack types enabled:</b>",
        }));
        Object.keys(embeddedData.dataTypes).forEach(name => {
            const label = document.createElement("label");
            const dataType = embeddedData.dataTypes[name];
            label.innerHTML = `
                <input type="checkbox" class="cbgroup" value="${name}">${dataType.title}`;
            selector.appendChild(label);
        });
        const selectedDataTypes = new Set(  // get dataTypes selected initially
            Object.entries(embeddedData.dataTypes).filter(([_, val]) => val.active === 1)
                .map(([key]) => key)
        );
        // initialize data type checkboxes (using class instead of 'name')
        document.querySelectorAll("input.cbgroup")
            .forEach(cb => { cb.checked = selectedDataTypes.has(cb.value); });

        // Capture initial data type state
        initialState.dataTypes = new Set(selectedDataTypes);
    }

    function initTable(allData) {
        const { metadata, rowToIdx, colNames } = allData;

        const ordinaryColumns = colNames.map(key => {
            const col = {
                data: key,
                title: toTitleCase(key.replace(/^_/, "")),
            };
            if (key === embeddedData.primaryKey && embeddedData.subtrackUrl) {
                col.render = (data, type) => {
                    if (type !== "display") return data;
                    const url = embeddedData.subtrackUrl.replace("$$", encodeURIComponent(data));
                    return `<a href="${url}" target="_blank">${data}</a>`;
                };
            }
            return col;
        });

        const checkboxColumn = {
            data: null,
            orderable: false,
            defaultContent: "",
            title: `
            <label title="Select all visible rows">
            <input type="checkbox" id="select-all"/></label>`,
            // no render function needed
        };

        const hasDataTypes = embeddedData.dataTypes &&
                             Object.keys(embeddedData.dataTypes).length > 0;
        const itemLabel = hasDataTypes ? "samples" : "tracks";
        const singularLabel = itemLabel.slice(0, -1);

        const columns = [checkboxColumn, ...ordinaryColumns];

        // Determine which column to sort by: use defaultSortField if it matches
        // a metadata column (case-insensitive, ignoring leading underscores),
        // otherwise fall back to the first data column.
        let defaultSortCol = 1;  // column 0 is checkboxes, 1 is first data col
        if (embeddedData.defaultSortField) {
            const target = embeddedData.defaultSortField.replace(/^_+/, "").toLowerCase();
            const idx = colNames.findIndex(
                c => c.replace(/^_+/, "").toLowerCase() === target);
            if (idx >= 0)
                defaultSortCol = idx + 1;  // +1 for the checkbox column
        }

        const table = $("#theMetaDataTable").DataTable({
            data: metadata,
            deferRender: true,    // seems faster
            columns: columns,
            columnDefs: [ { targets:0, render: DataTable.render.select() } ],
            responsive: true,
            layout: {
                topStart: 'pageLength',
                topEnd: null,        // omit global search
                bottomStart: 'info',
                bottomEnd: 'paging'
            },
            order: [[defaultSortCol, "asc"]],
            pageLength: 25,       // show 25 rows per page by default
            lengthMenu: [[10, 25, 50, 100, -1], [10, 25, 50, 100, "All"]],
            language: {
                lengthMenu: `Show _MENU_ ${itemLabel}`,
                select: {
                    rows: {
                        0: "",
                        1: `1 ${singularLabel} selected`,
                        _: `%d ${itemLabel} selected`
                    }
                },
                info: `Showing _START_ to _END_ of _TOTAL_ ${itemLabel}`,
                infoFiltered: `(filtered from _MAX_ total ${itemLabel})`,
            },
            select: { style: "multi", selector: "td:not(:has(a))" },
            initComplete: function() {  // Check appropriate boxes
                const api = this.api();
                embeddedData.dataElements.forEach(rowName => {
                    const rowIndex = rowToIdx[rowName];
                    if (rowIndex !== undefined) {
                        api.row(rowIndex).select();
                    }
                });
                // Capture initial data element state
                initialState.dataElements = new Set(embeddedData.dataElements);
            },
            drawCallback: function() {
                updateSelectAllCheckbox(this.api());
            },
        });

        function updateSelectAllCheckbox(api) {
            const filteredCount = api.rows({ search: "applied" }).count();
            const selectedCount = api.rows({ search: "applied", selected: true }).count();
            $("#select-all")
                .prop("checked", filteredCount > 0 && selectedCount === filteredCount)
                .prop("indeterminate", selectedCount > 0 && selectedCount < filteredCount);
        }
        // Find the Display Mode dropdown rendered by C code
        const visDropdown = document.querySelector(
            'select[name="' + embeddedData.track + '"]');

        // Track preferred non-hide visibility for auto-restore
        let preferredVis = "full";
        if (visDropdown && visDropdown.value !== "hide") {
            preferredVis = visDropdown.value;
        }

        // Track previous selection count for detecting 0<->nonzero transitions
        let prevSelCount = table.rows({selected: true}).count();

        // Update preferredVis when user manually changes the dropdown
        if (visDropdown) {
            visDropdown.addEventListener("change", function() {
                if (this.value !== "hide") {
                    preferredVis = this.value;
                }
            });
        }

        updateSelectAllCheckbox(table);  // set initial state after pre-selections

        // Create "show only selected" toggle in the toolbar
        const lengthDiv = document.querySelector(
            "#theMetaDataTable_wrapper .dt-length");
        const toggleWrapper = document.createElement("div");
        toggleWrapper.id = "selected-filter";
        const toggleLabel = document.createElement("label");
        toggleLabel.classList.add("toggle-switch");
        const toggleCheckbox = document.createElement("input");
        toggleCheckbox.type = "checkbox";
        toggleCheckbox.dataset.selectFilter = "true";
        toggleLabel.appendChild(toggleCheckbox);
        toggleLabel.appendChild(Object.assign(
            document.createElement("span"), {className: "toggle-slider"}));
        toggleWrapper.appendChild(toggleLabel);
        const toggleText = Object.assign(
            document.createElement("span"), {id: "selected-filter-text"});
        toggleWrapper.appendChild(toggleText);
        lengthDiv.appendChild(toggleWrapper);

        // Disable toggle initially if no rows are selected
        toggleCheckbox.disabled = (prevSelCount === 0);

        function updateSelectedText() {
            const selCount = table.rows({selected: true}).count();
            const totalCount = table.rows().count();
            toggleText.textContent =
                `Show only selected ${itemLabel} (${selCount} of ${totalCount} selected)`;
        }
        updateSelectedText();

        // Unified handler for selection changes
        function onSelectionChanged() {
            const selCount = table.rows({selected: true}).count();

            // Disable toggle when nothing is selected; auto-uncheck if count hits 0
            toggleCheckbox.disabled = (selCount === 0);
            if (selCount === 0 && toggleCheckbox.checked) {
                toggleCheckbox.checked = false;
                table.draw();
            }

            // Auto-switch Display Mode on 0<->nonzero transitions
            if (visDropdown) {
                if (selCount === 0 && prevSelCount > 0) {
                    visDropdown.value = "hide";
                } else if (selCount > 0 && prevSelCount === 0) {
                    visDropdown.value = preferredVis;
                }
            }

            updateSelectAllCheckbox(table);
            updateSelectedText();
            prevSelCount = selCount;
        }
        table.on("select deselect", onSelectionChanged);

        // Create active-filters chip bar (hidden when empty)
        const activeFiltersDiv = document.createElement("div");
        activeFiltersDiv.id = "active-filters";
        activeFiltersDiv.style.display = "none";
        const tableEl = document.getElementById("theMetaDataTable");
        tableEl.parentNode.insertBefore(activeFiltersDiv, tableEl);

        // define inputs for search functionality for each column in the table
        const row = document.querySelector("#theMetaDataTable thead").insertRow();
        columns.forEach((col) => {
            const cell = row.insertCell();
            if (col.data === null) {
                // left empty; toggle is now in the toolbar
            } else if (col.data && col.data.startsWith("__")) {
                // no search box for double-underscore columns
            } else {
                const input = document.createElement("input");
                input.type = "text";
                input.placeholder = "Search...";
                input.style.width = "100%";
                cell.appendChild(input);
            }
        });

        // behaviors for the column-based search functionality
        $("#theMetaDataTable thead input[type='text']")
            .on("keyup change", function () {
                const dtColIdx = $(this).parent().index();
                const colName = colNames[dtColIdx - 1];  // offset for checkbox col
                if (this.value) {
                    textFilters.set(colName, this.value.toLowerCase());
                } else {
                    textFilters.delete(colName);
                }
                table.column(dtColIdx).search(this.value).draw();
            });
        $.fn.dataTable.ext.search.push(function (_, data, dataIndex) {
            const filterInput =
                  document.querySelector("input[data-select-filter]");
            if (!filterInput?.checked) {  // If checkbox not checked, show all rows
                return true;
            }
            // Otherwise, only show selected rows
            const row = table.row(dataIndex);
            return row.select && row.selected();
        });
        $("#selected-filter input[data-select-filter]")
            .on("change", function () { table.draw(); });

        // implement the 'select all' at the top of the checkbox column
        $("#select-all").closest("label").attr(
            "title", `Select all filtered ${itemLabel}`);
        $("#theMetaDataTable thead").on("click", "#select-all", function () {
            const rowIsChecked = this.checked;
            if (rowIsChecked) {
                table.rows({ search: "applied" }).select();
            } else {
                table.rows({ search: "applied" }).deselect();
            }
        });
        return table;
    }  // end initTable


    // Map of colName -> Map of unescapedValue -> spanElement, for dynamic counts
    const countSpans = new Map();
    // Filter state for cross-facet count computation
    const checkboxFilters = new Map();  // colName -> Set<string> (raw values)
    const textFilters = new Map();      // colName -> lowercase string

    function updateFacetCounts(metadata) {
        // For each facet, count values among rows that pass all OTHER filters
        // (excluding this facet's own checkbox filter). This way, unchecked
        // values show how many rows would be added if you checked them.
        for (const [facetCol, valMap] of countSpans) {
            const counts = new Map();  // lowercased value -> count
            for (const row of metadata) {
                let passes = true;
                for (const [col, valueSet] of checkboxFilters) {
                    if (col === facetCol) continue;
                    if (!valueSet.has(row[col]?.toLowerCase())) {
                        passes = false; break;
                    }
                }
                if (passes) {
                    for (const [col, text] of textFilters) {
                        if (!row[col]?.toLowerCase().includes(text)) {
                            passes = false; break;
                        }
                    }
                }
                if (passes) {
                    const val = row[facetCol]?.toLowerCase();
                    counts.set(val, (counts.get(val) ?? 0) + 1);
                }
            }
            for (const [val, span] of valMap) {
                span.textContent = `(${counts.get(val.toLowerCase()) ?? 0})`;
            }
        }
    }

    function initFilters(table, allData) {
        const { metadata, colorMap, colNames } = allData;

        // iterate once over entire data not separately per attribute
        // Case-insensitive: merge variants, keep first-seen casing as display form
        const possibleValues = {};  // key -> Map<lowerVal, [displayVal, count]>
        for (const entry of metadata) {
            for (const [key, val] of Object.entries(entry)) {
                if (!possibleValues[key]) {
                    possibleValues[key] = new Map();
                }
                const map = possibleValues[key];
                const lower = val.toLowerCase();
                const existing = map.get(lower);
                if (existing) {
                    existing[1]++;
                } else {
                    map.set(lower, [val, 1]);
                }
            }
        }

        let { maxCheckboxes, primaryKey } = embeddedData;
        if (maxCheckboxes === null || maxCheckboxes === undefined) {
            maxCheckboxes = DEFAULT_MAX_CHECKBOXES;
        }
        const excludeCheckboxes = [primaryKey];

        const filtersDiv = document.getElementById("filters");
        colNames.forEach((key, colIdx) => {
            // skip attributes if they should be excluded from checkbox sets
            if (excludeCheckboxes.includes(key) || key.startsWith("_")) {
                return;
            }

            // possibleValues[key] is Map<lower, [displayVal, count]>; extract [displayVal, count]
            const sortedPossibleVals = Array.from(possibleValues[key].values());
            sortedPossibleVals.sort((a, b) => b[1] - a[1]);  // sort by count descending

            // Use 'maxCheckboxes' most frequent items (if they appear > 1 time)
            let topToShow = sortedPossibleVals
                .filter(([val, count]) =>
                    val.trim().toUpperCase() !== "NA" && count > 1)
                .slice(0, maxCheckboxes);

            // Any "other/Other/OTHER" entry will be put at the end
            let otherKey = null, otherValue = null;
            topToShow = topToShow.filter(([val, value]) => {
                if (val.toLowerCase() === "other") {
                    otherKey = val;
                    otherValue = value;
                    return false;
                }
                return true;
            });
            if (otherValue !== null) {
                topToShow.push([otherKey, otherValue]);
            }
            if (topToShow.length <= 1) {  // no point if there's only one group
                excludeCheckboxes.push(key);
                return;
            }

            // --- Build the facet group with collapsible structure ---
            const facetDiv = document.createElement("div");
            facetDiv.classList.add("facet-group");

            // Clickable heading that toggles collapse
            const heading = Object.assign(document.createElement("strong"), {
                textContent: toTitleCase(key),
                className: "facet-heading",
            });
            facetDiv.appendChild(heading);

            // Collapsible body: holds Clear button + all checkboxes
            const facetBody = document.createElement("div");
            facetBody.classList.add("facet-body");

            // Clear button — built here so it lives inside the collapsible body
            const clearBtn = document.createElement("button");
            clearBtn.textContent = "Clear";
            clearBtn.type = "button";
            facetBody.appendChild(clearBtn);

            // Build checkbox labels
            const cboxes = [];
            const rawValues = [];  // parallel to cboxes: unescaped values
            if (!countSpans.has(key)) countSpans.set(key, new Map());
            const colSpans = countSpans.get(key);
            topToShow.forEach(([val, count]) => {
                const label = document.createElement("label");
                const checkbox = document.createElement("input");
                checkbox.type = "checkbox";
                checkbox.value = escapeRegex(val);
                label.appendChild(checkbox);
                if (colorMap && key in colorMap) {
                    const colorBox = document.createElement("span");
                    colorBox.classList.add("color-box");
                    if (val in colorMap[key]) {
                        colorBox.style.backgroundColor = colorMap[key][val];
                    }
                    label.appendChild(colorBox);
                }
                label.appendChild(document.createTextNode(`${val} `));
                const countSpan = document.createElement("span");
                countSpan.textContent = `(${count})`;
                label.appendChild(countSpan);
                colSpans.set(val, countSpan);
                facetBody.appendChild(label);
                cboxes.push(checkbox);
                rawValues.push(val);
            });

            facetDiv.appendChild(facetBody);
            filtersDiv.appendChild(facetDiv);

            // --- Wire up collapse toggle ---
            heading.addEventListener("click", () => {
                const isCollapsed = facetBody.classList.toggle("collapsed");
                heading.classList.toggle("collapsed", isCollapsed);
            });

            // --- Wire up checkbox filtering (same logic as before) ---
            // colIdx is the 0-based index into colNames; DataTable column is
            // colIdx + 1 because column 0 is the select-checkbox column.
            const dtColIdx = colIdx + 1;
            cboxes.forEach(cb => {
                cb.addEventListener("change", () => {
                    const checked = cboxes.filter(c => c.checked).map(c => c.value);
                    const query = checked.length ? "^(" + checked.join("|") + ")$" : "";
                    // Track lowercased values for cross-facet counting
                    const checkedRaw = new Set();
                    cboxes.forEach((c, i) => {
                        if (c.checked) checkedRaw.add(rawValues[i].toLowerCase());
                    });
                    if (checkedRaw.size) {
                        checkboxFilters.set(key, checkedRaw);
                    } else {
                        checkboxFilters.delete(key);
                    }
                    table.column(dtColIdx).search(query, true, false).draw();
                    updateActiveFilters();
                });
            });

            // --- Wire up Clear button ---
            clearBtn.addEventListener("click", () => {
                cboxes.forEach(cb => cb.checked = false);
                checkboxFilters.delete(key);
                table.column(dtColIdx).search("", true, false).draw();
                updateActiveFilters();
            });
        });  // done creating collapsible checkbox filters for each column

        // Update facet counts whenever the table is redrawn (filtering, search, etc.)
        table.on("draw", () => updateFacetCounts(metadata));

        return table;  // to chain calls
    }  // end initFilters

    function updateActiveFilters() {
        const container = document.getElementById("active-filters");
        if (!container) return;
        container.innerHTML = "";

        const checked = document.querySelectorAll(
            "#filters input[type='checkbox']:checked");
        if (checked.length === 0) {
            container.style.display = "none";
            return;
        }

        // Group by facet name
        const groups = new Map();
        checked.forEach(cb => {
            const facetGroup = cb.closest(".facet-group");
            if (!facetGroup) return;
            const heading = facetGroup.querySelector(".facet-heading");
            if (!heading) return;
            const facetName = heading.textContent.trim();
            // Get the display text from the label (strip the count suffix)
            const label = cb.parentElement;
            const labelText = label.textContent.trim();
            if (!groups.has(facetName)) groups.set(facetName, []);
            groups.get(facetName).push({ labelText, checkbox: cb });
        });

        groups.forEach((chips, facetName) => {
            const groupLabel = document.createElement("span");
            groupLabel.className = "filter-chip-group-label";
            groupLabel.textContent = facetName + ":";
            container.appendChild(groupLabel);

            chips.forEach(({ labelText, checkbox }) => {
                const chip = document.createElement("span");
                chip.className = "filter-chip";
                chip.appendChild(document.createTextNode(labelText + " "));
                const removeBtn = document.createElement("button");
                removeBtn.className = "remove-chip";
                removeBtn.type = "button";
                removeBtn.textContent = "\u00d7";
                removeBtn.addEventListener("click", () => {
                    checkbox.checked = false;
                    checkbox.dispatchEvent(new Event("change"));
                });
                chip.appendChild(removeBtn);
                container.appendChild(chip);
            });
        });

        container.style.display = "flex";
    }

    function initSubmit(table) {  // logic for the submit event
        const { mdid, primaryKey } = embeddedData;  // mdid: metadata identifier
        const hasDataTypes = embeddedData.dataTypes &&
                             Object.keys(embeddedData.dataTypes).length > 0;
        document.getElementById("Submit").addEventListener("click", (submitBtnEvent) => {
            submitBtnEvent.preventDefault();  // hold the submit button event

            const currentDataTypes = [];
            if (hasDataTypes) {
                // Get current data type selections
                document.querySelectorAll("input.cbgroup").forEach(cb => {
                    if (cb.checked) {
                        currentDataTypes.push(cb.value);
                    }
                });
                // Require at least one data type when the selector exists
                if (currentDataTypes.length === 0) {
                    alert("Please select at least one data type.");
                    return;  // abort submission
                }
            }

            // Get current data element selections
            const currentDataElements = table.rows({selected: true}).data().toArray()
                .map(obj => obj[primaryKey]);

            // Enforce an upper bound on the number of tracks on at the same time.
            // This is imperfect when data types are present - some combinations might
            // have been manually hidden by the user.  But it should be a good ballpark.
            const trackLimit = 1000;
            if (hasDataTypes) {
                if (currentDataTypes.length * currentDataElements.length > trackLimit) {
                    alert("You have turned on too many subtracks (over 1000) - please uncheck some.");
                    return;  // abort submission
                }
            } else {
                if (currentDataElements.length > trackLimit) {
                    alert("You have turned on too many subtracks (over 1000) - please uncheck some.");
                    return;  // abort submission
                }
            }

            // Build the parameters for the cart update
            const uriForUpdate = new URLSearchParams({
                "cartDump.metaDataId": mdid,
                "noDisplay": 1
            });

            // Data elements: was and now
            if (initialState.dataElements.size > 0) {
                initialState.dataElements.forEach(de =>
                    uriForUpdate.append(`${mdid}.de_was`, de));
            } else {
                uriForUpdate.append(`${mdid}.de_was`, "");
            }
            if (currentDataElements.length > 0) {
                currentDataElements.forEach(de =>
                    uriForUpdate.append(`${mdid}.de_now`, de));
            } else {
                uriForUpdate.append(`${mdid}.de_now`, "");
            }

            if (hasDataTypes) {
            // Data types: was and now
                if (initialState.dataTypes.size > 0) {
                    initialState.dataTypes.forEach(dt => {
                        uriForUpdate.append(`${mdid}.dt_was`, dt);});
                } else {
                    uriForUpdate.append(`${mdid}.dt_was`, "");
                }
                if (currentDataTypes.length > 0) {
                    currentDataTypes.forEach(dt => {
                        uriForUpdate.append(`${mdid}.dt_now`, dt);});
                } else {
                    uriForUpdate.append(`${mdid}.dt_now`, "");
                }
            }
            // No ${mdid}.dt* variables indicates that the composite doesn't use data types

            updateVisibilities(uriForUpdate, submitBtnEvent);
        });
    }  // end initSubmit

    function initAll(dataForTable) {
        initDataTypeSelector();
        const table = initTable(dataForTable);
        initFilters(table, dataForTable);
        initSubmit(table);
    }

    function loadDataAndInit() {  // load data and call init functions
        const { mdid, primaryKey, metadataUrl, colorSettingsUrl, track } = embeddedData;

        const paramsFromUrl = new URLSearchParams(window.location.search);
        const hgsid = paramsFromUrl.get("hgsid");
        let fetchBody = `fileUrl=${metadataUrl}&track=${track}`;
        if (hgsid !== null) {
            fetchBody = fetchBody + `&hgsid=${hgsid}`;
        }

        // fetch file dynamically
        const fetchUrl = "/cgi-bin/hgTrackUi?" + fetchBody;
        const req = (fetchUrl.length > 2048 || embeddedData.udcTimeout) ?
            fetch("/cgi-bin/hgTrackUi", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: fetchBody,
            })
            : fetch(fetchUrl, {
                method: "GET",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
            });
        req.then(response => {
            if (!response.ok) {  // a 404 will look like plain text
                throw new Error(`HTTP Status: ${response.status}`);
            }
            return response.text();
            })
            .then(tsvText => {  // metadata table is a TSV file to parse
                loadOptional(colorSettingsUrl, hgsid, track).then(colorMap => {
                    const rows = tsvText.trim().split("\n");
                    const colNames = rows[0].split("\t");
                    const metadata = rows.slice(1).map(row => {
                        const values = row.split("\t");
                        const obj = {};
                        colNames.forEach((attrib, i) => { obj[attrib] = values[i]; });
                        return obj;
                    });
                    const rowToIdx = Object.fromEntries(
                        metadata.map((row, i) => [row[primaryKey], i])
                    );
                    colorMap = isValidColorMap(colorMap) ? colorMap : null;
                    const freshData = { metadata, rowToIdx, colNames, colorMap };

                    initAll(freshData);
                });
            })
            .catch(err => {
                const table = document.getElementById("theMetaDataTable");
                if (table) {
                    table.innerHTML =
                        `<tr><td style="padding:20px;color:#a00;">` +
                        `Error loading metadata: ${err.message}</td></tr>`;
                }
            });
    }  // end loadDataAndInit

    document.addEventListener("keydown", e => {  // block accidental submit
        if (e.key === "Enter") { e.preventDefault(); e.stopPropagation(); }
    }, true);

    generateHTML();
    loadDataAndInit();

});
