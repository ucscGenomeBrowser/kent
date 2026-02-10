// SPDX-License-Identifier: MIT; (c) 2025 Andrew D Smith (author)
/* jshint esversion: 11 */
$(function() {
    /* ADS: Uncomment below to force confirm on unload/reload */
    // window.addEventListener("beforeunload", function (e) {
    //     e.preventDefault(); e.returnValue = ""; });
    const DEFAULT_MAX_CHECKBOXES = 20;  // ADS: without default, can get crazy

    // ADS: need "matching" versions for the plugins
    const DATATABLES_URL = "../js/dataTables-1.13.6.min.js";
    const DATATABLES_SELECT_URL = "../js/dataTables.select-1.7.0.min.js";
    const CSS_URLS = [
        "../style/dataTables-1.13.6.min.css",  // dataTables CSS
        "../style/dataTables.select-1.7.0.min.css",  // dataTables Select CSS
        "../style/facetedComposite.css",  // Local metadata table CSS
    ];

    const isValidColorMap = obj =>  // check the whole thing and ignore if invalid
          typeof obj === "object" && obj !== null && !Array.isArray(obj) &&
          Object.values(obj).every(x =>
              typeof x === "object" && x !== null && !Array.isArray(x) &&
                  Object.values(x).every(value => typeof value === "string"));

    const loadOptional = url =>  // load if possible otherwise carry on
          url ?
          fetch(url).then(r => r.ok ? r.json() : null).catch(() => null)
          : Promise.resolve(null);

    const loadIfMissing = (condition, url, callback) =>  // for missing plugins
          condition ?
          document.head.appendChild(Object.assign(
              document.createElement("script"), { src: url, onload: callback }))
          : callback();

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
        fetch("/cgi-bin/cartDump", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: `hgsid=${hgsid}&db=${db}&${uriForUpdate}`,
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

        const ordinaryColumns = colNames.map(key => ({  // all but checkboxes
            data: key,
            title: toTitleCase(key),
        }));

        const checkboxColumn = {
            data: null,
            orderable: false,
            className: "select-checkbox",
            defaultContent: "",
            title: `
            <label title="Select all visible rows">
            <input type="checkbox" id="select-all"/></label>`,
            // no render function needed
        };

        const columns = [checkboxColumn, ...ordinaryColumns];
        const hasDataTypes = embeddedData.dataTypes && 
                             Object.keys(embeddedData.dataTypes).length > 0;
        const itemLabel = hasDataTypes ? "samples" : "tracks";
        const table = $("#theMetaDataTable").DataTable({
            data: metadata,
            deferRender: true,    // seems faster
            columns: columns,
            responsive: true,
            // autoWidth: true,   // might help columns shrink to fit content
            order: [[1, "asc"]],  // sort by the first data column, not checkbox
            pageLength: 50,       // show 50 rows per page by default
            lengthMenu: [[10, 25, 50, 100, -1], [10, 25, 50, 100, "All"]],
            language: { lengthMenu: `Show _MENU_ ${itemLabel}`, },
            select: { style: "multi", selector: "td:first-child" },
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
            drawCallback: function() {  // Reset header "select all" checkbox
                $("#select-all")
                    .prop("checked", false)
                    .prop("indeterminate", false);
            },
        });

        // define inputs for search functionality for each column in the table
        const row = document.querySelector("#theMetaDataTable thead").insertRow();
        columns.forEach((col) => {
            const cell = row.insertCell();
            if (col.className === "select-checkbox") {  // show selected items
                const label = document.createElement("label");
                label.title = "Show only selected rows";

                const checkbox = document.createElement("input");
                checkbox.type = "checkbox";
                checkbox.dataset.selectFilter = "true";

                label.appendChild(checkbox);
                cell.appendChild(label);
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
                table.column($(this).parent().index()).search(this.value).draw();
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
        $("#theMetaDataTable thead input[data-select-filter]")
            .on("change", function () { table.draw(); });

        // implement the 'select all' at the top of the checkbox column
        $("#theMetaDataTable thead").on("click", "#select-all", function () {
            const rowIsChecked = this.checked;
            if (rowIsChecked) {
                table.rows({ page: "current" }).select();
            } else {
                table.rows({ page: "current" }).deselect();
            }
        });
        return table;
    }  // end initTable

    function initFilters(table, allData) {
        const { metadata, colorMap, colNames } = allData;

        // iterate once over entire data not separately per attribute
        const possibleValues = {};
        for (const entry of metadata) {
            for (const [key, val] of Object.entries(entry)) {
                if (possibleValues[key] === null || possibleValues[key] === undefined) {
                    possibleValues[key] = new Map();
                }
                const map = possibleValues[key];
                map.set(val, (map.get(val) ?? 0) + 1);
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
            if (excludeCheckboxes.includes(key)) {
                return;
            }

            const sortedPossibleVals = Array.from(possibleValues[key].entries());
            sortedPossibleVals.sort((a, b) => // neg: less-than
                 a[1] !== b[1] ? b[1] - a[1] : a[0].localeCompare(b[0]));

            // Use 'maxCheckboxes' most frequent items (if they appear > 1 time)
            let topToShow = sortedPossibleVals
                .filter(([val, count]) =>
                    val.trim().toUpperCase() !== "NA")  // why only > 1?
                    //val.trim().toUpperCase() !== "NA" && count > 1)
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

            // Add headings and filter checkboxes (only top maxCheckboxes)
            const cbSetsDiv = document.createElement("div");
            cbSetsDiv.appendChild(Object.assign(document.createElement("strong"), {
                textContent: toTitleCase(key)
            }));
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
                label.appendChild(document.createTextNode(`${val} (${count})`));
                cbSetsDiv.appendChild(label);
            });
            filtersDiv.appendChild(cbSetsDiv);
        });  // done creating checkbox filters for each column

        const checkboxAttributeIndexes =  // checkbox sets => cols in data table
              colNames.reduce((acc, key, colIdx) => {
                  if (!excludeCheckboxes.includes(key)) { acc.push(colIdx); }
                  return acc;
              }, []);
        // Now do logic to implement checkboxes-based rows display
        checkboxAttributeIndexes.forEach((colIdx, idx) => {
            const attrDiv = filtersDiv.children[idx];
            const cboxes = [  // need this to be 'array' to use 'filter'
                ...attrDiv.querySelectorAll("input[type=checkbox]")
            ];
            cboxes.forEach(cb => {  // Add set of checkboxes for this attribute
                cb.addEventListener("change", () => {
                    const chk = cboxes.filter(c => c.checked).map(c => c.value);
                    const query = chk.length ? "^(" + chk.join("|") + ")$" : "";
                    table.column(colIdx + 1).search(query, true, false).draw();
                });
            });
            // Make a "clear" button
            const clearBtn = document.createElement("button");
            clearBtn.textContent = "Clear";
            clearBtn.type = "button";  // prevent form submit if inside a form
            clearBtn.addEventListener("click", () => {
                cboxes.forEach(cb => cb.checked = false);  // Uncheck all
                // Recalculate the (now cleared) search term and update table
                table.column(colIdx + 1).search("", true, false).draw();
            });
            // Prepend the "clear" button
            attrDiv.insertBefore(clearBtn, attrDiv.children[1] || null);
        });
        return table;  // to chain calls
    }  // end initFilters

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
        const { mdid, primaryKey, metadataUrl, colorSettingsUrl } = embeddedData;

        const CACHE_KEY = mdid;
        const CACHE_TIMESTAMP = `${CACHE_KEY}_time_stamp`;
        const CACHE_EXPIRY_MS = 24 * 60 * 60 * 1000; // 24 hours

        const now = Date.now();
        const cachedTime = parseInt(localStorage.getItem(CACHE_TIMESTAMP), 10);

        let cachedData = null;
        let useCache = false;

        if (cachedTime && (now - cachedTime < CACHE_EXPIRY_MS)) {
            const cachedStr = localStorage.getItem(CACHE_KEY);
            cachedData = cachedStr ? JSON.parse(cachedStr) : null;
            useCache = !!cachedData;
        }

        if (useCache) {
            initAll(cachedData);
            return;
        }

        fetch(metadataUrl)
            .then(response => {
                if (!response.ok) {  // a 404 will look like plain text
                    throw new Error(`HTTP Status: ${response.status}`);
                }
                return response.text();
            })
            .then(tsvText => {  // metadata table is a TSV file to parse
                loadOptional(colorSettingsUrl).then(colorMap => {
                    const rows = tsvText.trim().split("\n");
                    const colNames = rows[0].split("\t");
                    const metadata = rows.slice(1).map(row => {
                        const values = row.split("\t");
                        const obj = {};
                        colNames.forEach((attrib, i) => { obj[attrib] = values[i]; });
                        return obj;
                    });
                    if (!metadata.length || !colNames.length) {
                        localStorage.removeItem(CACHE_KEY);
                        localStorage.removeItem(CACHE_TIMESTAMP);
                        return;
                    }
                    const rowToIdx = Object.fromEntries(
                        metadata.map((row, i) => [row[primaryKey], i])
                    );
                    colorMap = isValidColorMap(colorMap) ? colorMap : null;
                    const freshData = { metadata, rowToIdx, colNames, colorMap };
                    // cache the data
                    localStorage.setItem(CACHE_KEY, JSON.stringify(freshData));
                    localStorage.setItem(CACHE_TIMESTAMP, now.toString());

                    initAll(freshData);
                });
            });
    }  // end loadDataAndInit

    CSS_URLS.map(href =>  // load all the CSS
        document.head.appendChild(Object.assign(
            document.createElement("link"), { rel: "stylesheet", href })));

    document.addEventListener("keydown", e => {  // block accidental submit
        if (e.key === "Enter") { e.preventDefault(); e.stopPropagation(); }
    }, true);

    // ADS: only load plugins if they are not already loaded
    loadIfMissing(!$.fn.DataTable, DATATABLES_URL, () => {
        loadIfMissing(!$.fn.dataTable.select, DATATABLES_SELECT_URL, () => {
            generateHTML();
            loadDataAndInit();
        });
    });
});
