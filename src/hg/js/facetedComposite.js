// SPDX-License-Identifier: MIT; (c) 2025 Andrew D Smith (author)
/* jshint esversion: 11 */
$(function() {
    /* ADS: Uncomment below to force confirm on unload/reload */
    // window.addEventListener("beforeunload", function (e) {
    //     e.preventDefault(); e.returnValue = ""; });
    const DEFAULT_MAX_CHECKBOXES = 20;  // ADS: without default, can get crazy

    // Hover help for the sort note above the table.  addMouseover() in utils.js
    // renders this with innerHTML, so simple tags are fine.
    const SORT_ORDER_HELP =
        "The row order of this table sets the order the subtracks appear in the " +
        "Genome Browser image.<br><br>" +
        "Click a column heading to sort by that column; click it again to reverse " +
        "the direction.<br><br>" +
        "To sort on more than one column, click the first heading, then " +
        "shift-click each additional heading, in the order you want them " +
        "applied.<br><br>" +
        "Rows can also be dragged by the handle in the Reorder column, which " +
        "appears on the \"shown in the browser\" tab.";

    // Hover help for the two selection tabs.
    const SHOWN_TAB_HELP =
        "The first option shows all samples. Check a sample row to make this " +
        "sample visible, i.e. show all its tracks in the Genome Browser.<br>" +
        "The second button lists only the samples with visible tracks. Drag to " +
        "reorder these tracks or uncheck the sample to hide all its tracks.";

    // Hover help for the grouping tabs.
    const GROUP_BY_HELP =
        "How the tracks are arranged in the Genome Browser image when a sample " +
        "has more than one kind of data.<br><br>" +
        "Group by sample keeps one sample's tracks together, which suits " +
        "comparing different kinds of data in the same sample.<br><br>" +
        "Group by data type puts the same kind of data for every sample " +
        "together, which suits comparing one measurement across samples.";

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

    const showLoading = () => {  // spinner shown during fetch + table build
        if (document.getElementById("faceted-loading")) return;
        const el = document.createElement("div");
        el.id = "faceted-loading";
        el.innerHTML =
            `<div class="faceted-spinner"></div><div>Loading metadata…</div>`;
        document.getElementById("metadata-placeholder").appendChild(el);
    };
    const hideLoading = () => {
        const el = document.getElementById("faceted-loading");
        if (el) el.remove();
    };

    const toTitleStyle = str =>
            str.replace(/_+/g, " ");

    const escapeRegex = str => str.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

    // For primaryKey values that use the 'id|label' form, return just the id.
    // The label is for display only; the cart and rowToIdx need the bare id.
    const primaryKeyId = v => {
        if (v == null) return v;
        const s = String(v);
        const bar = s.indexOf("|");
        return bar >= 0 ? s.slice(0, bar) : s;
    };

    // Split a TSV row on tabs, respecting double- or single-quoted fields.
    function parseTsvRow(str) {
        const fields = [];
        let i = 0, n = str.length, start = 0, inQuote = false, q = '';
        while (i < n) {
            if (inQuote) {
                if (str[i] === q) {
                    if (i + 1 < n && str[i + 1] === q) { i += 2; continue; }  // escaped quote
                    inQuote = false;
                }
                i++;
            } else if (str[i] === '"' || str[i] === "'") {
                q = str[i]; inQuote = true; i++;
            } else if (str[i] === '\t') {
                fields.push(str.slice(start, i)); i++; start = i;
            } else {
                i++;
            }
        }
        fields.push(str.slice(start));
        return fields;
    }

    // Split a cell value on commas, respecting double- or single-quoted substrings.
    // Returns the trimmed, non-empty tokens.
    function parseCsvValues(str) {
        if (str === null || str === undefined || str === "") return [];
        // Callers mostly pass metadata strings, but a synthetic numeric field
        // reaches here too, and a number has no .slice().
        if (typeof str !== "string") str = String(str);
        const tokens = [];
        let i = 0, n = str.length, start = 0, inQuote = false, q = '';
        while (i < n) {
            if (inQuote) {
                if (str[i] === q) {
                    if (i + 1 < n && str[i + 1] === q) { i += 2; continue; }
                    inQuote = false;
                }
                i++;
            } else if (str[i] === '"' || str[i] === "'") {
                q = str[i]; inQuote = true; i++;
            } else if (str[i] === ',') {
                tokens.push(str.slice(start, i).trim()); i++; start = i;
            } else {
                i++;
            }
        }
        tokens.push(str.slice(start).trim());
        return tokens.filter(Boolean);
    }

    // Parse one CSV token into {id, label}.
    // Format: \w+(\|label)? where label may be a quoted string.
    // label is null when no | is present; id is used for display in that case.
    function parseValue(token) {
        const bar = token.indexOf('|');
        if (bar < 0) return { id: token.trim(), label: null };
        const id = token.slice(0, bar).trim();
        let label = token.slice(bar + 1);
        if (label.length >= 2) {
            const f = label[0], l = label[label.length - 1];
            if ((f === '"' && l === '"') || (f === "'" && l === "'"))
                label = label.slice(1, -1);
        }
        return { id, label };
    }

    // Return the lowercased ids parsed from a cell value string.
    function parseCellIds(val) {
        return parseCsvValues(String(val ?? "")).map(tok => parseValue(tok).id.toLowerCase());
    }

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

    // Set by initTable(), which owns the Display Mode dropdown.  Called from the
    // data type and facet handlers, which live in other functions.  A no-op
    // until the table has loaded, which is before the user can click anything.
    let showTracks = () => {};

    // Set by initTable(), which owns the two selection tabs.  Called from the
    // facet handlers in initFilters() to drop back to the full list.
    let showAllRows = () => {};

    // Set by initTable() once the "Group by" tabs exist, read by initSubmit().
    // Returns null for a composite without data types, where there is nothing
    // to group and no control is drawn.
    let getGroupBy = () => null;

    // How this picker was last left: which facet boxes were ticked, what was
    // typed in each column's search box, which tab was showing, how many rows
    // per page, and a hand-dragged row order if there is one.  None of it
    // changes what the Genome Browser draws, so it stays out of the cart:
    // putting it there would grow every session, and a manual order over a
    // table the size of Methbase's 6500 rows would be a large value to carry
    // around for a display preference.  The sort column is the exception and
    // does live in the cart, as facetSortOrder, because it also sets the track
    // order in the image.
    const uiStateKey = `facetedComposite.${embeddedData.mdid}`;

    function loadUiState() {
        // A private window throws on access rather than returning null, and a
        // half-written value from an older build should not break the page.
        try {
            const raw = localStorage.getItem(uiStateKey);
            const state = raw ? JSON.parse(raw) : null;
            return (state && typeof state === "object") ? state : {};
        } catch (e) {
            return {};
        }
    }

    // A page length the user picked wins.  Otherwise paginating a table that
    // would nearly fit anyway just hides rows behind a menu, so show everything
    // up to the first menu step past 25.  -1 is what DataTables reads as "all".
    function savedPageLength(saved, rowCount) {
        if (typeof saved === "number" && saved !== 0)
            return saved;
        return rowCount < 50 ? -1 : 25;
    }

    function saveUiState(patch) {
        try {
            localStorage.setItem(uiStateKey,
                                 JSON.stringify(Object.assign(loadUiState(), patch)));
        } catch (e) {
            /* private window, or the quota is full; the page works without it */
        }
    }

    function generateHTML() {
        const container = document.createElement("div");
        container.id = "myTag";
        container.innerHTML = `
        <div id="dataTypeSelector"></div>
        <div id="container">
            <div id="filters"></div>
            <div id="tableColumn">
                <div id="sortNote" class="smallText"></div>
                <table id="theMetaDataTable">
                    <thead></thead>
                    <tfoot></tfoot>
                </table>
            </div>
        </div>
        `;
        // Instead of appending to body, append into the placeholder div
        document.getElementById("metadata-placeholder").appendChild(container);

        // The table's row order drives the order tracks are drawn in the browser
        // image.  That's easy to miss (the classic composite UI never says so
        // either), so state it in one line and put the details in the hover.
        // The icon is appended as a node because createInfoIcon() returns an
        // element that already has its mouseover listeners attached.
        const note = document.getElementById("sortNote");
        note.appendChild(document.createTextNode(
            "Tracks appear in the Genome Browser in the same order as the table " +
            "below - click a column heading to re-sort or drag individual rows " +
            "to change the order"));
        note.appendChild(createInfoIcon(SORT_ORDER_HELP));
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
            innerHTML: "<b>Data types shown in the browser:</b>",
        }));

        selector.appendChild(createInfoIcon(
            "Each sample has several data type tracks in the Genome Browser. " +
            "Check the boxes of the types of tracks you wish to show when a " +
            "sample row is selected below."));

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

        // Turning a data type on is a request to see it, so take the container
        // out of hide the same way selecting a sample does.  These boxes were
        // otherwise only read at submit time.
        document.querySelectorAll("input.cbgroup").forEach(cb => {
            cb.addEventListener("change", () => {
                if (cb.checked) showTracks();
            });
        });

        // Capture initial data type state
        initialState.dataTypes = new Set(selectedDataTypes);
    }

    function initTable(allData) {
        const { metadata, rowToIdx, colNames } = allData;
        const colDescriptions = allData.colDescriptions || {};
        const primaryKey = embeddedData.primaryKey;

        // Match subtrackUrls trackDb keys against metadata column names
        // ignoring leading underscores on either side, so authors can toggle
        // facet visibility by adding/removing a '_' prefix in the metadata
        // file without having to re-edit trackDb.
        const stripUnderscores = s => s.replace(/^_+/, "");
        const subtrackUrls = Object.fromEntries(
            Object.entries(embeddedData.subtrackUrls || {})
                  .map(([k, v]) => [stripUnderscores(k), v])
        );

        const ordinaryColumns = colNames.map(key => {
            const col = {
                data: key,
                title: toTitleStyle(key.replace(/^_+/, "")),
            };
            const urlTemplate = subtrackUrls[stripUnderscores(key)];
            if (urlTemplate) {
                // Mirrors hgc/hgc.c:printIdOrLinks(): split cell on ',', each
                // token may be 'id|label' (id substitutes $$, label is shown).
                // urlTemplate is html-encoded server-side (htmlEncode in
                // hgTrackUi.c), so it's safe to interpolate into an href.
                col.render = (data, type) => {
                    if (type !== "display") return data;
                    if (data == null || data === "") return "";
                    const parts = parseCsvValues(String(data));
                    if (!parts.length) return String(data);
                    return parts.map(tok => {
                        const { id, label } = parseValue(tok);
                        const displayLabel = label !== null ? label : id;
                        const encode = label === null && !/^https?:/i.test(displayLabel);
                        const sub = encode ? encodeURIComponent(id) : id;
                        const href = urlTemplate.replace(/\$\$/g, sub);
                        return `<a href="${href}" target="_blank">${displayLabel}</a>`;
                    }).join(", ");
                };
            } else {
                col.render = (data, type) => {
                    if (type !== "display") return data;
                    if (data == null || data === "") return data;
                    return parseCsvValues(String(data))
                        .map(tok => { const {id, label} = parseValue(tok); return label ?? id; })
                        .join(", ");
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
        // Capitalized, for the two filter tabs.  With data types a row is a
        // sample rather than a track, since each row stands for as many tracks
        // as there are active data types.
        const itemLabelCap = itemLabel.charAt(0).toUpperCase() + itemLabel.slice(1);

        // Drag handle for manual row ordering, and the field RowReorder swaps
        // when a row is dropped.  It sits second, right after the checkboxes,
        // where drag handles are normally looked for and where it cannot scroll
        // off the right edge of a narrow window.  Everything that maps a
        // DataTables column index onto colNames therefore skips two leading
        // columns rather than one; DATA_COL_OFFSET below is that count, and the
        // sortSpec the submit builds excludes this column by name so a manual
        // order is never written to the cart as a nonexistent sort field.
        const savedState = loadUiState();
        const ORDER_FIELD = "__rowOrder";
        metadata.forEach((row, i) => { row[ORDER_FIELD] = i; });
        // A hand-dragged order from a previous visit, as primary key values.
        // Rows the metadata no longer has are ignored, and rows the saved order
        // does not mention keep their file order after the ones it does, so an
        // edited metadata file degrades instead of throwing.
        if (Array.isArray(savedState.rowOrder) && savedState.rowOrder.length) {
            const rank = new Map(savedState.rowOrder.map((id, i) => [String(id), i]));
            const big = savedState.rowOrder.length;
            metadata.forEach((row, i) => {
                const seen = rank.get(String(primaryKeyId(row[primaryKey])));
                row[ORDER_FIELD] = (seen === undefined) ? big + i : seen;
            });
        }
        // Six dots in two columns, the conventional drag-handle shape.  Drawn
        // here rather than taken from Font Awesome: the deployed version is
        // 4.5.0, which predates fa-grip-vertical, and this page does not load
        // Font Awesome at all.  Inline SVG follows createInfoIcon() in utils.js.
        const GRIP_SVG =
            "<svg class='dt-grip' viewBox='0 0 10 16' aria-hidden='true'>" +
            "<circle cx='3' cy='3' r='1.4'/><circle cx='7' cy='3' r='1.4'/>" +
            "<circle cx='3' cy='8' r='1.4'/><circle cx='7' cy='8' r='1.4'/>" +
            "<circle cx='3' cy='13' r='1.4'/><circle cx='7' cy='13' r='1.4'/>" +
            "</svg>";
        const reorderColumn = {
            data: ORDER_FIELD,
            className: "dt-reorder",
            orderable: true,
            // One word, so the column stays narrow; "Drag to reorder" would sit
            // on one line and take about twice the width.
            title: "Reorder",
            visible: false,       // only shown on the Active tab
            render: () => GRIP_SVG,
        };

        const columns = [checkboxColumn, reorderColumn, ...ordinaryColumns];
        const reorderColIdx = 1;
        // How many non-metadata columns precede the data columns: the select
        // checkboxes and the drag handle.
        const DATA_COL_OFFSET = 2;

        // Map a metadata field name to its DataTables column index, matching
        // case-insensitively and ignoring leading underscores.  Returns -1 when
        // the name isn't one of the metadata columns.
        const colIdxForName = name => {
            const target = name.replace(/^_+/, "").toLowerCase();
            const idx = colNames.findIndex(
                c => c.replace(/^_+/, "").toLowerCase() === target);
            return idx >= 0 ? idx + DATA_COL_OFFSET : -1;
        };

        // Determine which column to sort by: use defaultSortField if it matches
        // a metadata column, otherwise fall back to the first data column.
        let defaultSortCol = 1;  // column 0 is checkboxes, 1 is first data col
        if (embeddedData.defaultSortField) {
            const idx = colIdxForName(embeddedData.defaultSortField);
            if (idx > 0)
                defaultSortCol = idx;
        }

        // A sort the user established on an earlier visit wins over
        // defaultSortField.  facetSortOrder mirrors the classic composite
        // '<track>.sortOrder' cart value: 'field=+ field2=-', in sort precedence
        // order.  Fields no longer present in the metadata are dropped, so a
        // changed metadata file degrades to a partial (or default) sort rather
        // than an error.
        let initialOrder = [[defaultSortCol, "asc"]];
        if (embeddedData.facetSortOrder) {
            const savedOrder = embeddedData.facetSortOrder.trim().split(/\s+/)
                .map(token => {
                    const eq = token.lastIndexOf("=");
                    if (eq < 1) return null;
                    const idx = colIdxForName(token.slice(0, eq));
                    if (idx < 0) return null;
                    return [idx, token.slice(eq + 1) === "-" ? "desc" : "asc"];
                })
                .filter(Boolean);
            if (savedOrder.length > 0)
                initialOrder = savedOrder;
        }

        const table = $("#theMetaDataTable").DataTable({
            data: metadata,
            deferRender: true,    // seems faster
            columns: columns,
            columnDefs: [ { targets:0, render: DataTable.render.select() } ],
            // 'responsive' (collapsing overflow columns) is intentionally off:
            // a wide table instead gets an internal horizontal scrollbar via
            // the .table-xscroll wrapper added at the end of initTable.
            responsive: false,
            layout: {
                topStart: 'pageLength',
                topEnd: null,        // omit global search
                bottomStart: 'info',
                bottomEnd: 'paging'
            },
            order: initialOrder,
            // Paginating a table that would nearly fit anyway just hides rows
            // behind a menu, so show everything up to the first menu step that
            // exceeds 25.  -1 is what DataTables reads as "all", the same value
            // behind the "All" entry in the menu below.
            // A page length the user picked wins; otherwise paginating a
            // table that would nearly fit anyway just hides rows behind a
            // menu, so show everything up to the first menu step past 25.
            // -1 is what DataTables reads as "all".
            pageLength: savedPageLength(savedState.pageLength, metadata.length),
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
            // Dragging is only offered on the Active tab, where the row order
            // is the track order, so it starts disabled.  The handle cell is
            // the only drag target: with the whole row draggable a click meant
            // to select a sample would start a drag instead.
            rowReorder: {
                dataSrc: ORDER_FIELD,
                selector: "td.dt-reorder",
                enable: false,
                snapX: true,
            },
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

        // The mode to come back to when the container needs to be shown.
        // Pack rather than full: a faceted composite usually mixes signal and
        // feature tracks, and pack is the mode that suits both.  An explicit
        // choice by the user replaces it, below.
        let preferredVis = "pack";
        if (visDropdown && visDropdown.value !== "hide") {
            preferredVis = visDropdown.value;
        }

        // Anything the user does on this page that means "I want to see this"
        // takes the container out of hide.  Without it, picking samples on a
        // container whose visibility is hide silently produces no image.
        function showTracksNow() {
            if (visDropdown && visDropdown.value === "hide")
                visDropdown.value = preferredVis;
        }
        showTracks = showTracksNow;

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

        // Create "All / Selected" segmented tabs in the toolbar. These are a
        // re-skin of a simple on/off selection filter: a hidden checkbox holds
        // the filter state so the search-filter plug-in and selection handlers
        // below can stay unchanged; the tabs just drive that checkbox.
        const lengthDiv = document.querySelector(
            "#theMetaDataTable_wrapper .dt-length");
        const toggleWrapper = document.createElement("div");
        toggleWrapper.id = "selected-filter";
        const toggleCheckbox = document.createElement("input");
        toggleCheckbox.type = "checkbox";
        toggleCheckbox.dataset.selectFilter = "true";
        toggleCheckbox.style.display = "none";
        toggleWrapper.appendChild(toggleCheckbox);
        const allTab = Object.assign(document.createElement("button"),
            {type: "button", className: "filter-tab"});
        const selectedTab = Object.assign(document.createElement("button"),
            {type: "button", className: "filter-tab"});
        toggleWrapper.appendChild(allTab);
        toggleWrapper.appendChild(selectedTab);
        // Outside the buttons, so clicking the icon does not switch tabs.
        toggleWrapper.appendChild(createInfoIcon(SHOWN_TAB_HELP));
        lengthDiv.appendChild(toggleWrapper);

        // With data types, each row stands for several tracks, so there are two
        // ways to lay them out in the image: keep a sample's data types
        // together, or keep the same data type for every sample together.  A
        // second pair of tabs picks between them.  Without data types a row is
        // one track and there is nothing to group, so the control is omitted.
        let groupBy = embeddedData.groupBy === "dataType" ? "dataType" : "sample";
        if (hasDataTypes) {
            const groupWrapper = document.createElement("div");
            groupWrapper.id = "group-by";
            const groupLabel = document.createElement("span");
            groupLabel.textContent = "Group tracks in the browser by:";
            groupWrapper.appendChild(groupLabel);
            groupWrapper.appendChild(createInfoIcon(GROUP_BY_HELP));
            const bySampleTab = Object.assign(document.createElement("button"),
                {type: "button", className: "filter-tab",
                 textContent: singularLabel === "sample" ? "Sample" : "Row",
                 title: "Keep each sample's data types together in the image"});
            const byTypeTab = Object.assign(document.createElement("button"),
                {type: "button", className: "filter-tab", textContent: "Data type",
                 title: "Keep the same data type for every sample together in the image"});
            groupWrapper.appendChild(bySampleTab);
            groupWrapper.appendChild(byTypeTab);
            lengthDiv.appendChild(groupWrapper);

            const paintGroupBy = () => {
                bySampleTab.classList.toggle("active", groupBy === "sample");
                byTypeTab.classList.toggle("active", groupBy === "dataType");
            };
            // Ordering happens server side, on submit, so a click only records
            // the choice; nothing about the table itself changes.
            bySampleTab.addEventListener("click", () => {
                groupBy = "sample"; paintGroupBy(); showTracksNow();
            });
            byTypeTab.addEventListener("click", () => {
                groupBy = "dataType"; paintGroupBy(); showTracksNow();
            });
            paintGroupBy();
        }
        // Read by the submit handler, which lives in a different function.
        getGroupBy = () => (hasDataTypes ? groupBy : null);

        // Refresh the tab labels and the active-tab highlight. Counts are grand
        // totals (default search:'none'), independent of the facet/search
        // filters, so "Selected" never misleadingly reads 0 when tracks are
        // selected but currently hidden by a facet. How many rows are actually
        // visible is reported by DataTables' bottom info line.
        function updateSelectedText() {
            const selCount = table.rows({selected: true}).count();
            const totalCount = table.rows().count();
            allTab.textContent = `All ${itemLabelCap} (${totalCount})`;
            // "All" means every row in the table, drawn or not, so it keeps the
            // plain noun; the other tab is the subset that reaches the image.
            selectedTab.textContent =
                `${itemLabelCap} shown in the browser (${selCount})`;
            const showSelected = toggleCheckbox.checked;
            allTab.classList.toggle("active", !showSelected);
            selectedTab.classList.toggle("active", showSelected);
        }
        updateSelectedText();

        table.on("length", (e, settings, len) => saveUiState({pageLength: len}));

        // Clicking a tab switches the selection filter and redraws. "Selected"
        // is always clickable; with nothing selected it just shows an empty list.
        function setFilterMode(showSelected) {
            toggleCheckbox.checked = showSelected;
            // Rows can only be dragged on the Active tab, and dragging only
            // means something while the table is sorted by the drag column, so
            // entering the tab renumbers that column from whatever order is on
            // screen and sorts by it.  Leaving restores nothing: the column
            // sort the user had is still in the header, one click away.
            table.column(reorderColIdx).visible(showSelected, false);
            syncReorderSearchCell(showSelected);
            table.rowReorder[showSelected ? "enable" : "disable"]();
            if (showSelected) {
                let n = 0;
                table.rows({order: "current", search: "none"}).every(function () {
                    const d = this.data();
                    d[ORDER_FIELD] = n++;
                    this.data(d);
                });
                table.order([reorderColIdx, "asc"]);
            }
            table.draw();
            updateSelectedText();
            saveUiState({tab: showSelected ? "active" : "all"});
        }
        allTab.addEventListener("click", () => setFilterMode(false));
        selectedTab.addEventListener("click", () => {
            setFilterMode(true);
            // Asking to see just the samples that reach the image is a request
            // to see the image.  Hooked on the click rather than inside
            // setFilterMode, which also runs when the tab is restored on page
            // load, where nothing the user did should move the dropdown.
            showTracksNow();
        });
        showAllRows = () => setFilterMode(false);

        // Remember a hand-dragged order by primary key, so it survives a
        // metadata file whose row order or contents have changed since.
        table.on("row-reorder", () => {
            // The event fires before RowReorder has applied the swap, so read
            // the new order on the next tick.
            setTimeout(() => {
                const ids = table.rows({order: "current", search: "none"})
                    .data().toArray()
                    .map(r => primaryKeyId(r[primaryKey]));
                saveUiState({rowOrder: ids});
            }, 0);
        });

        // Unified handler for selection changes
        function onSelectionChanged() {
            const selCount = table.rows({selected: true}).count();

            // Keep the "Selected" view in sync as rows are (de)selected; no need
            // to redraw while showing all rows.
            if (toggleCheckbox.checked) {
                table.draw();
            }

            // Selecting anything shows the container; clearing the whole
            // selection hides it again, since there would be nothing to draw.
            if (visDropdown) {
                if (selCount === 0) {
                    if (prevSelCount > 0)
                        visDropdown.value = "hide";
                } else {
                    showTracksNow();
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

        // define inputs for search functionality for each column in the table.
        // DataTables does not manage this row, so hiding a column does not
        // remove its cell here: the row has to be kept the same length as the
        // header by hand, or every search box shifts one column to the left and
        // a stray empty cell appears past the last heading.  searchCells keeps
        // the cell for each column so setFilterMode() can match the drag
        // column's visibility.  The cells stay in the DOM either way, so the
        // parent().index() arithmetic further down is unaffected.
        const row = document.querySelector("#theMetaDataTable thead").insertRow();
        const searchCells = [];
        columns.forEach((col) => {
            const cell = row.insertCell();
            searchCells.push(cell);
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
        // Match the drag column's starting state, which is hidden.
        const syncReorderSearchCell = shown => {
            if (searchCells[reorderColIdx])
                searchCells[reorderColIdx].style.display = shown ? "" : "none";
        };
        syncReorderSearchCell(false);

        // behaviors for the column-based search functionality
        $("#theMetaDataTable thead input[type='text']")
            .on("keyup change", function () {
                const dtColIdx = $(this).parent().index();
                const colName = colNames[dtColIdx - DATA_COL_OFFSET];
                if (this.value) {
                    textFilters.set(colName, this.value.toLowerCase());
                } else {
                    textFilters.delete(colName);
                }
                const searches = Object.assign({}, loadUiState().searches);
                if (this.value)
                    searches[colName] = this.value;
                else
                    delete searches[colName];
                saveUiState({searches: searches});
                table.column(dtColIdx).search(this.value).draw();
            });

        // Put back whatever was typed into each search box last time.  Applied
        // before the first draw below, so the row counts are right from the
        // start rather than flickering.
        const savedSearches = loadUiState().searches;
        if (savedSearches && typeof savedSearches === "object") {
            $("#theMetaDataTable thead input[type='text']").each(function () {
                const dtColIdx = $(this).parent().index();
                const colName = colNames[dtColIdx - DATA_COL_OFFSET];
                const val = savedSearches[colName];
                if (typeof val === "string" && val !== "") {
                    this.value = val;
                    textFilters.set(colName, val.toLowerCase());
                    table.column(dtColIdx).search(val);
                }
            });
        }
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

        // Wrap the table in a horizontally-scrolling box. When the metadata has
        // many fields the table is wider than the viewport; this gives it its
        // own internal X scrollbar instead of letting it spill off the right
        // edge of the screen (which also dragged the "Show N" / paging controls
        // off-screen). The toolbar rows stay outside this box, so they remain
        // visible at the wrapper's width regardless of how wide the table gets.
        // A column whose header carried a '|description' gets an info icon in
        // its heading.  Added as an element after the table exists, because
        // createInfoIcon() returns a node with its hover listeners already
        // attached and putting the markup in the column title would lose them.
        // The header is also the sort control, so the icon swallows its own
        // clicks rather than re-sorting the table.
        colNames.forEach((name, i) => {
            const desc = colDescriptions[name];
            if (!desc) return;
            const th = table.column(i + DATA_COL_OFFSET).header();
            if (!th) return;
            const icon = createInfoIcon(desc);
            icon.addEventListener("click", e => e.stopPropagation());
            th.appendChild(icon);
        });

        const scrollBox = document.createElement("div");
        scrollBox.className = "table-xscroll";
        tableEl.parentNode.insertBefore(scrollBox, tableEl);
        scrollBox.appendChild(tableEl);

        // Come back on whichever tab was showing, through setFilterMode so the
        // drag column and the row numbering are set up exactly as a click would
        // leave them.  This has to run last: the search plug-in that hides
        // unselected rows is registered further down this function, and a draw
        // before that point leaves the tab highlighted while the table still
        // shows every row.  Only restored when something is actually selected,
        // since this tab on an empty selection is a blank table.
        if (savedState.tab === "active" && table.rows({selected: true}).count())
            setFilterMode(true);

        return table;
    }  // end initTable


    // Map of colName -> Map of lowercaseId -> spanElement, for dynamic counts
    const countSpans = new Map();
    // Filter state for cross-facet count computation
    const checkboxFilters = new Map();  // colName -> Set<string> (lowercase ids)
    const textFilters = new Map();      // colName -> lowercase string

    function updateFacetCounts(metadata) {
        // For each facet, count values among rows that pass all OTHER filters
        // (excluding this facet's own checkbox filter). This way, unchecked
        // values show how many rows would be added if you checked them.
        for (const [facetCol, valMap] of countSpans) {
            const counts = new Map();  // lowercased id -> count
            for (const row of metadata) {
                let passes = true;
                for (const [col, idSet] of checkboxFilters) {
                    if (col === facetCol) continue;
                    if (!parseCellIds(row[col]).some(id => idSet.has(id))) {
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
                    for (const id of parseCellIds(row[facetCol])) {
                        counts.set(id, (counts.get(id) ?? 0) + 1);
                    }
                }
            }
            for (const [id, span] of valMap) {
                span.textContent = `(${counts.get(id) ?? 0})`;
            }
        }
    }

    function initFilters(table, allData) {
        const { metadata, colorMap, colNames } = allData;
        const colDescriptions = allData.colDescriptions || {};

        // iterate once over entire data not separately per attribute
        // Keyed by lowercase id; first-seen label (or id) is the canonical display.
        const possibleValues = {};  // key -> Map<lowerId, {id, display, count}>
        for (const entry of metadata) {
            for (const [key, val] of Object.entries(entry)) {
                // The drag-order field is ours, not metadata; it never becomes
                // a facet and its values are row numbers.
                if (key === "__rowOrder") continue;
                if (!possibleValues[key]) possibleValues[key] = new Map();
                const map = possibleValues[key];
                for (const tok of parseCsvValues(val)) {
                    const { id, label } = parseValue(tok);
                    const idLower = id.toLowerCase();
                    const existing = map.get(idLower);
                    if (existing) {
                        existing.count++;
                    } else {
                        map.set(idLower, { id, display: label ?? id, count: 1 });
                    }
                }
            }
        }

        let { maxCheckboxes, primaryKey } = embeddedData;
        if (maxCheckboxes === null || maxCheckboxes === undefined) {
            maxCheckboxes = DEFAULT_MAX_CHECKBOXES;
        }
        const excludeCheckboxes = [primaryKey];

        const filtersDiv = document.getElementById("filters");
        colNames.forEach((key) => {
            // skip attributes if they should be excluded from checkbox sets
            if (excludeCheckboxes.includes(key) || key.startsWith("_")) {
                return;
            }

            // possibleValues[key] is Map<lowerId, {id, display, count}>
            const sortedPossibleVals = Array.from(possibleValues[key].values());
            sortedPossibleVals.sort((a, b) => b.count - a.count);

            // Use 'maxCheckboxes' most frequent items (if they appear > 1 time)
            let topToShow = sortedPossibleVals
                .filter(({id, count}) =>
                    id.trim().toUpperCase() !== "NA" && count > 1)
                .slice(0, maxCheckboxes);

            // Any "other/Other/OTHER" entry will be put at the end
            let otherEntry = null;
            topToShow = topToShow.filter(entry => {
                if (entry.id.toLowerCase() === "other") { otherEntry = entry; return false; }
                return true;
            });
            if (otherEntry !== null) topToShow.push(otherEntry);

            if (topToShow.length <= 1) {  // no point if there's only one group
                excludeCheckboxes.push(key);
                return;
            }

            // --- Build the facet group with collapsible structure ---
            const facetDiv = document.createElement("div");
            facetDiv.classList.add("facet-group");

            // Clickable heading that toggles collapse
            const heading = Object.assign(document.createElement("strong"), {
                textContent: toTitleStyle(key),
                className: "facet-heading",
            });
            // Same description as the column heading carries, for whichever of
            // the two the reader happens to be looking at.  The heading toggles
            // the group open and shut, so the icon keeps its clicks to itself.
            if (colDescriptions[key]) {
                const icon = createInfoIcon(colDescriptions[key]);
                icon.addEventListener("click", e => e.stopPropagation());
                heading.appendChild(icon);
            }
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

            // Push this facet's current checkbox state into the filter, remember
            // it, and redraw.  narrowing is true when the user just added a
            // value rather than removed one.
            const applyFacetChange = narrowing => {
                const checkedIds = new Set(
                    cboxes.filter(c => c.checked)
                          .map(c => c.dataset.valueId.toLowerCase())
                );
                if (checkedIds.size)
                    checkboxFilters.set(key, checkedIds);
                else
                    checkboxFilters.delete(key);
                const facets = Object.assign({}, loadUiState().facets);
                if (checkedIds.size)
                    facets[key] = [...checkedIds];
                else
                    delete facets[key];
                saveUiState({facets: facets});
                if (narrowing) {
                    showTracks();
                    // Narrowing by a facet is about finding samples in the full
                    // list, so a facet applied while only the selected rows are
                    // showing would filter a handful of rows the user had
                    // already picked.  Drop back to all of them.
                    showAllRows();
                }
                table.draw();
                updateActiveFilters();
            };
            if (!countSpans.has(key)) countSpans.set(key, new Map());
            const colSpans = countSpans.get(key);
            topToShow.forEach(({id, display, count}) => {
                const label = document.createElement("label");
                const checkbox = document.createElement("input");
                checkbox.type = "checkbox";
                checkbox.dataset.valueId = id;
                label.appendChild(checkbox);
                if (colorMap && key in colorMap) {
                    const colorBox = document.createElement("span");
                    colorBox.classList.add("color-box");
                    if (id in colorMap[key]) {
                        colorBox.style.backgroundColor = colorMap[key][id];
                    }
                    label.appendChild(colorBox);
                }
                label.appendChild(document.createTextNode(`${display} `));
                const countSpan = document.createElement("span");
                countSpan.textContent = `(${count})`;
                label.appendChild(countSpan);
                colSpans.set(id.toLowerCase(), countSpan);

                // "only" narrows this facet to this one value.  Hidden until
                // the row is hovered, so a long list stays quiet to read.  It
                // sits inside the label, which would otherwise toggle the
                // checkbox when the link is clicked, hence preventDefault as
                // well as stopPropagation.
                const onlyLink = document.createElement("a");
                onlyLink.className = "facet-only";
                onlyLink.href = "#";
                onlyLink.textContent = "only";
                onlyLink.title = `Show only ${display}`;
                onlyLink.addEventListener("click", e => {
                    e.preventDefault();
                    e.stopPropagation();
                    cboxes.forEach(c => { c.checked = (c === checkbox); });
                    applyFacetChange(true);
                });
                label.appendChild(onlyLink);

                facetBody.appendChild(label);
                cboxes.push(checkbox);
            });

            facetDiv.appendChild(facetBody);
            filtersDiv.appendChild(facetDiv);

            // --- Wire up collapse toggle ---
            heading.addEventListener("click", () => {
                const isCollapsed = facetBody.classList.toggle("collapsed");
                heading.classList.toggle("collapsed", isCollapsed);
            });

            // --- Wire up checkbox filtering ---
            // Filtering is handled by the custom search extension below, which
            // parses each cell's ids and checks them against checkboxFilters.
            // Restore the boxes this column was left with.  Values that are no
            // longer in the metadata are simply not found and stay unticked.
            const savedFacet = (loadUiState().facets || {})[key];
            if (Array.isArray(savedFacet) && savedFacet.length) {
                const want = new Set(savedFacet.map(v => String(v).toLowerCase()));
                cboxes.forEach(cb => {
                    if (want.has(cb.dataset.valueId.toLowerCase()))
                        cb.checked = true;
                });
                const checkedIds = new Set(
                    cboxes.filter(c => c.checked)
                          .map(c => c.dataset.valueId.toLowerCase())
                );
                if (checkedIds.size)
                    checkboxFilters.set(key, checkedIds);
            }

            cboxes.forEach(cb => {
                cb.addEventListener("change", () => applyFacetChange(cb.checked));
            });

            // --- Wire up Clear button ---
            clearBtn.addEventListener("click", () => {
                cboxes.forEach(cb => cb.checked = false);
                applyFacetChange(false);
            });
        });  // done creating collapsible checkbox filters for each column

        // With every column either the primary key, underscored, or holding
        // values too unique to be worth a checkbox, no facet group gets built.
        // The sidebar is a fixed-width column that does not shrink, so an empty
        // one would sit beside the table as 300px of nothing.  Checked on the
        // element rather than with :empty, which a stray newline in the markup
        // template would quietly defeat.
        filtersDiv.classList.toggle("no-facets", filtersDiv.children.length === 0);

        // Any facet boxes put back from the last visit were ticked while the
        // groups were still being built, so draw the chip bar now that every
        // column has been through the loop.
        if (checkboxFilters.size || textFilters.size) {
            table.draw();
            updateActiveFilters();
        }

        // Custom search extension: filter rows by parsed cell ids vs checkboxFilters.
        // Replaces the old per-column regex search so that id-based collapsing works
        // (e.g. "CD8+T" and "CD8+T|CD8+ T Cells" both match the same facet entry).
        $.fn.dataTable.ext.search.push(function(_, __, dataIndex) {
            const rowData = table.row(dataIndex).data();
            for (const [col, idSet] of checkboxFilters) {
                if (!parseCellIds(rowData[col]).some(id => idSet.has(id))) return false;
            }
            return true;
        });

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
            // Get the display text from the label (strip the count suffix).
            // The label also carries an "only" link, which is a control rather
            // than part of the value's name, so leave it out.
            const label = cb.parentElement;
            const labelText = [...label.childNodes]
                .filter(n => !(n.nodeType === Node.ELEMENT_NODE &&
                               n.classList.contains("facet-only")))
                .map(n => n.textContent)
                .join("")
                .trim();
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

    function initSubmit(table, allData) {  // logic for the submit event
        const { colNames } = allData;
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

            // Get current data element selections, in the order they are
            // currently sorted/displayed in the table. The server uses this
            // order to assign each shown subtrack a '.priority' cart value so
            // the tracks appear in hgTracks in the same order as here. 'search'
            // is 'none' so selected-but-facet-filtered rows still get a sensible
            // position rather than being dropped from the ordering.
            const currentDataElements =
                table.rows({selected: true, order: "current", search: "none"})
                    .data().toArray()
                    .map(obj => primaryKeyId(obj[primaryKey]));

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

            // Preserve the current sort so the table comes back the same way on the
            // next visit.  Column names rather than DataTables column indexes, so
            // this survives a change in metadata column order - the same reason
            // defaultSortField is matched by name.  Format mirrors the classic
            // composite '<track>.sortOrder' cart value: 'field=+ field2=-'.
            // Columns 0 and 1 are the select checkboxes and the drag handle,
            // neither of which is a metadata field, so the data columns start
            // at 2.  Skipping the handle also keeps a hand-dragged order out of
            // the cart, where it would name a field the metadata does not have.
            const DATA_COL_OFFSET = 2;
            const sortSpec = table.order()
                // an entry with neither direction is a column left unsorted
                .filter(o => Array.isArray(o) && o[0] >= DATA_COL_OFFSET &&
                             o[0] < colNames.length + DATA_COL_OFFSET &&
                             (o[1] === "asc" || o[1] === "desc"))
                .map(o => colNames[o[0] - DATA_COL_OFFSET] +
                          (o[1] === "asc" ? "=+" : "=-"))
                // whitespace in a field name would break the space-separated format
                .filter(token => !/\s/.test(token));
            // Sent even when empty, so the server clears any stale value
            uriForUpdate.append(`${mdid}.facetSortOrder`, sortSpec.join(" "));

            // Which dimension stays contiguous in the image.  Sent even when
            // empty, for the same reason as the sort order above.
            uriForUpdate.append(`${mdid}.groupBy`, getGroupBy() || "");

            updateVisibilities(uriForUpdate, submitBtnEvent);
        });
    }  // end initSubmit

    function initAll(dataForTable) {
        initDataTypeSelector();
        const table = initTable(dataForTable);
        initFilters(table, dataForTable);
        initSubmit(table, dataForTable);
        hideLoading();  // table is built and drawn; remove the spinner
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
                    // A header cell may carry an optional longer description
                    // after a '|', e.g. "Sample_class|HPRC = ...".  Only the
                    // name part is the column name, because it is also the key
                    // every row object is looked up by.
                    const rawColNames = parseTsvRow(rows[0]);
                    const colNames = [];
                    const colDescriptions = {};
                    rawColNames.forEach(raw => {
                        const bar = raw.indexOf("|");
                        const name = (bar < 0 ? raw : raw.slice(0, bar)).trim();
                        colNames.push(name);
                        if (bar >= 0) {
                            const desc = raw.slice(bar + 1).trim();
                            if (desc) colDescriptions[name] = desc;
                        }
                    });
                    if (!primaryKey)
                        throw new Error("trackDb setting 'primaryKey' is missing");
                    if (!colNames.includes(primaryKey))
                        throw new Error(`primaryKey '${primaryKey}' not found in metadata columns`);
                    const metadata = rows.slice(1).map(row => {
                        const values = parseTsvRow(row);
                        const obj = {};
                        colNames.forEach((attrib, i) => { obj[attrib] = values[i] ?? ""; });
                        return obj;
                    });
                    // Each primaryKey cell must map to exactly one subtrack.
                    const badPk = metadata.find(row =>
                        parseCsvValues(String(row[primaryKey] ?? "")).length > 1);
                    if (badPk)
                        throw new Error(
                            `primaryKey column '${primaryKey}' has multiple values in ` +
                            `'${badPk[primaryKey]}'; only one value is allowed per primaryKey cell`);
                    const rowToIdx = Object.fromEntries(
                        metadata.map((row, i) => [primaryKeyId(row[primaryKey]), i])
                    );
                    colorMap = isValidColorMap(colorMap) ? colorMap : null;
                    const freshData = { metadata, rowToIdx, colNames, colorMap,
                                        colDescriptions };

                    initAll(freshData);
                });
            })
            .catch(err => {
                hideLoading();  // stop the spinner before showing the error
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
    showLoading();  // show spinner immediately, before the metadata fetch
    loadDataAndInit();

});
