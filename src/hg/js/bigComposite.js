// SPDX-License-Identifier: MIT; (c) 2025 Andrew D Smith (author)
$(function() {
    /* ADS: Uncomment below to force confirm on unload/reload */
    // window.addEventListener("beforeunload", function (e) {
    //     e.preventDefault(); e.returnValue = ""; });

    // ADS: need "matching" versions for the libraries/plugins
    const JQUERY_URL = "https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js";
    const DATATABLES_URL = "https://cdn.datatables.net/1.13.6/js/jquery.dataTables.min.js";
    const DATATABLES_SELECT_URL = "https://cdn.datatables.net/select/1.7.0/js/dataTables.select.min.js";
    const CSS_URLS = [
        "https://cdn.datatables.net/1.13.6/css/jquery.dataTables.min.css",  // dataTables CSS
        "https://cdn.datatables.net/select/1.7.0/css/select.dataTables.min.css",  // dataTables Select CSS
        "/style/bigComposite.css",  // Local metadata table CSS
    ];

    const toTitleCase = str =>
          str.toLowerCase()
          .split(/[_\s-]+/)  // Split on underscore, space, or hyphen
          .map(word => word.charAt(0).toUpperCase() + word.slice(1)).join(" ");

    const escapeRegex = str => str.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

    CSS_URLS.map(href =>  // load all the CSS
        document.head.appendChild(Object.assign(
            document.createElement("link"), { rel: "stylesheet", href })));

    const pageEmbeddedData = (() => {  // get data embedded in html
        const dataTag = document.getElementById("app-data");
        return dataTag ? JSON.parse(dataTag.innerText) : "";
    })();

    // get query params from URL
    const paramsFromUrl = new URLSearchParams(window.location.search);
    const db = paramsFromUrl.get("db");
    const hgsid = paramsFromUrl.get("hgsid");

    function updateVisibilities(sessionDbContents, submitBtnEvent) {
        fetch("/cgi-bin/bigCompositeUpdate", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: `hgsid=${hgsid}&db=${db}&${sessionDbContents}`,
        }).then(() => {
            const dtLength =
                  submitBtnEvent.target.form.querySelector("select[name$='_length']");
            if (dtLength) {  // ADS: disable this named element to prevent leak
                dtLength.disabled = true;
            }
            submitBtnEvent.target.form.submit();  // release submit event
        });
    }

    // block accidental submit
    document.addEventListener("keydown", e => {
        if (e.key === "Enter") { e.preventDefault(); e.stopPropagation(); }
    }, true);

    function generateHTML() {
        const container = document.createElement("div");
        container.id = "myTag";  // ADS: need to have a named scope here
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

    function initTableAndFilters(allData, sessionDbContents) {
        const { metadata, colorMap, accToRowId, settings } = allData;
        if (!metadata.length) {
            return;
        }

        // identifier for database (e.g., mb2 for MethBase2) and data types
        const { mdid, dataTypes } = settings;

        // Make data type selector checkboxes
        const selector = document.getElementById("dataTypeSelector");
        selector.appendChild(Object.assign(document.createElement("label"), {
            innerHTML: "<b>Data type</b>",
        }));
        dataTypes.forEach(type => {
            const label = document.createElement("label");
            label.innerHTML = `
                <input type="checkbox" class="group1" value="${type}">
                ${type}
            `;
            selector.appendChild(label);
        });

        const selectedAccessions = new Set(sessionDbContents.data_elements);
        const selectedDataTypes = new Set(sessionDbContents.data_types);

        // initialize the data type checkboxes without using 'name'
        document.querySelectorAll("input.group1")
            .forEach(cb => { cb.checked = selectedDataTypes.has(cb.value); });

        const dynamicColumns = settings.orderedColumnNames.map(key => ({
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
            <input type="checkbox" id="select-all"/></label>
            `,
            // no render function needed
        };

        const columns = [checkboxColumn, ...dynamicColumns];
        const table = $("#theMetaDataTable").DataTable({
            data: metadata,
            deferRender: true,    // seems faster
            columns: columns,
            responsive: true,
            // autoWidth: true,   // might help columns shrink to fit content
            order: [[1, "asc"]],  // sort by the first data column, not checkbox
            pageLength: 50,       // show 50 rows per page by default
            lengthMenu: [[10, 25, 50, 100, -1], [10, 25, 50, 100, "All"]],
            select: { style: "multi", selector: "td:first-child" },
            initComplete: function() {  // Check appropriate boxes
                const api = this.api();
                selectedAccessions.forEach(accession => {
                    const rowIndex = accToRowId[accession];
                    if (rowIndex !== undefined) {
                        api.row(rowIndex).select();
                    }
                });
            },
            drawCallback: function() {  // Reset header "select all" checkbox
                $("#select-all")
                    .prop("checked", false)
                    .prop("indeterminate", false);
            },
        });

        const row = document.querySelector("#theMetaDataTable thead").insertRow();

        columns.forEach((col) => {
            const cell = row.insertCell();
            if (col.className === "select-checkbox") {
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

        // search functionality
        $("#theMetaDataTable thead input[type='text']")
            .on("keyup change", function () {
                table.column($(this).parent().index()).search(this.value).draw();
            });

        $.fn.dataTable.ext.search.push(function (_, data, dataIndex) {
            const filterInput =
                  document.querySelector("input[data-select-filter]");
            const onlySelected = filterInput?.checked;
            // If checkbox is not checked, show all rows
            if (!onlySelected) {
                return true;
            }
            // Otherwise, only show selected rows
            const row = table.row(dataIndex);
            return row.select && row.selected();
        });

        $("#theMetaDataTable thead input[data-select-filter]")
            .on("change", function () { table.draw(); });

        $("#theMetaDataTable thead").on("click", "#select-all", function () {
            const rowIsChecked = this.checked;
            if (rowIsChecked) {
                table.rows({ page: "current" }).select();
            } else {
                table.rows({ page: "current" }).deselect();
            }
        });

        // iterate once over entire data not separately per attribute
        const possibleValues = {};
        for (const entry of metadata) {
            for (const [key, val] of Object.entries(entry)) {
                const map = possibleValues[key] ??= new Map();
                map.set(val, (map.get(val) ?? 0) + 1);
            }
        }

        const filtersDiv = document.getElementById("filters");
        settings.orderedColumnNames.forEach((key, colIdx) => {
            // skip attributes to exclude from checkbox sets
            if (settings.excludedFromCheckboxes.includes(key)) {
                return;
            }

            const groupDiv = document.createElement("div");
            groupDiv.dataset.colidx = colIdx;  // store the column index here

            // Add the heading and the filtered checkboxes
            // (only top MAX_CHECKBOX_ELEMENTS)
            const heading = document.createElement("strong");
            heading.textContent = toTitleCase(key);
            groupDiv.appendChild(heading);

            const sortedPossibleValues = Array.from(possibleValues[key].entries());
            sortedPossibleValues.sort((a, b) => b[1] - a[1]);  // neg: less-than

            // Keep only the top MAX_CHECKBOX_ELEMENTS most frequent entries
            let topToShow = sortedPossibleValues
                .filter(([val, _]) => val.trim().toUpperCase() !== "NA")
                .slice(0, settings.MAX_CHECKBOX_ELEMENTS);

            // If there is an "other" entry, put it at the end
            let otherKey = null;
            let otherValue = null;  // might be title case or not
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

            topToShow.forEach(([val, count]) => {
                const label = document.createElement("label");
                const checkbox = document.createElement("input");
                checkbox.type = "checkbox";
                checkbox.value = escapeRegex(val);
                label.appendChild(checkbox);
                if (key === settings.color_attribute) {
                    const colorBox = document.createElement("span");
                    colorBox.classList.add("color-box");
                    colorBox.style.backgroundColor = colorMap[val];  // color
                    label.appendChild(colorBox);
                }
                label.appendChild(document.createTextNode(`${val} (${count})`));
                groupDiv.appendChild(label);
            });

            filtersDiv.appendChild(groupDiv);
        });

        const checkboxAttributeIndexes =
              settings.orderedColumnNames.reduce((acc, key, colIdx) => {
                  if (!settings.excludedFromCheckboxes.includes(key)) {
                      acc.push(colIdx);
                  }
                  return acc;
              }, []);

        checkboxAttributeIndexes.forEach((colIdx, idx) => {
            const attributeDiv = filtersDiv.children[idx];
            const cboxes = [  // need array for 'filter'
                ...attributeDiv.querySelectorAll("input[type=checkbox]")
            ];
            // Add a set of checkboxes for this attribute
            cboxes.forEach(cb => {
                cb.addEventListener("change", () => {
                    const chk = cboxes.filter(c => c.checked).map(c => c.value);
                    const query = chk.length ? "^(" + chk.join("|") + ")$" : "";
                    table.column(colIdx + 1).search(query, true, false).draw();
                });
            });
            // Make a "clear" button
            const clearBtn = document.createElement("button");
            clearBtn.textContent = "Clear";
            clearBtn.type = "button"; // prevent form submission if inside a form
            clearBtn.addEventListener("click", () => {
                cboxes.forEach(cb => cb.checked = false);  // Uncheck all
                // Recalculate the (now empty) string search term and update table
                table.column(colIdx + 1).search("", true, false).draw();
            });
            // Prepend the "clear" button
            attributeDiv.insertBefore(clearBtn, attributeDiv.children[1] || null);
        });

        document.getElementById("Submit").addEventListener("click", (submitBtnEvent) => {
            submitBtnEvent.preventDefault();  // hold the submit event

            const selectedData = table.rows({ selected: true }).data().toArray();
            const selectedDataTypes = [];

            document.querySelectorAll("input.group1").forEach(cb => {
                if (cb.checked) {
                    selectedDataTypes.push(cb.value);
                }
            });
            const sessionDbUri = new URLSearchParams();
            sessionDbUri.set("mdid", mdid);
            selectedData.forEach(obj =>
                sessionDbUri.append(`${mdid}_de`, obj.accession));
            selectedDataTypes.forEach(
                dat => sessionDbUri.append(`${mdid}_dt`, dat));
            updateVisibilities(sessionDbUri, submitBtnEvent);
        });
    }  // end initTableAndFilters

    function loadDataAndInit() {
        const CACHE_KEY = "MethBase2MetaData";
        const CACHE_TIMESTAMP_KEY = "MethBase2MetaDataTimestamp";
        const CACHE_EXPIRY_MS = 24 * 60 * 60 * 1000; // 24 hours

        const now = Date.now();
        const cachedTime = parseInt(localStorage.getItem(CACHE_TIMESTAMP_KEY), 10);

        let cachedData = null;
        let useCache = false;

        if (cachedTime && (now - cachedTime < CACHE_EXPIRY_MS)) {
            const cachedStr = localStorage.getItem(CACHE_KEY);
            cachedData = cachedStr ? JSON.parse(cachedStr) : null;
            useCache = !!cachedData;
        }

        if (useCache) {
            initTableAndFilters(cachedData, pageEmbeddedData);
        } else {
            fetch(pageEmbeddedData.metadata_url)
                .then(res => res.json())
                .then(freshData => {
                    freshData["accToRowId"] = Object.fromEntries(
                        freshData["metadata"].map((row, idx) => [row.accession, idx])
                    );
                    // cache the data
                    localStorage.setItem(CACHE_KEY, JSON.stringify(freshData));
                    localStorage.setItem(CACHE_TIMESTAMP_KEY, now.toString());

                    initTableAndFilters(freshData, pageEmbeddedData);
                });
        }
    }

    function loadIfMissing(condition, url, callback) {
        if (condition) {
            const s = document.createElement("script");
            s.src = url;
            s.onload = callback;
            document.head.appendChild(s);
        } else {
            callback();
        }
    }

    function whenReady(callback) {
        if (document.readyState === "loading") {
            document.addEventListener("DOMContentLoaded", callback);
        } else {
            callback();
        }
    }

    // ADS: in case js deps aren't already loaded by outer scope
    loadIfMissing(!window.jQuery, JQUERY_URL, () => {
        loadIfMissing(!$.fn.DataTable, DATATABLES_URL, () => {
            loadIfMissing(!$.fn.dataTable.select, DATATABLES_SELECT_URL, () => {
                whenReady(() => {
                    generateHTML();
                    loadDataAndInit();
                });
            });
        });
    });
});
