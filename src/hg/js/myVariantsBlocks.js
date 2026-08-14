// myVariantsBlocks.js - shared row-based BED12 block editor.
// Mounted by the hgTracks "Add Annotation" dialog and by the hgc edit page.
// Authoritative validation is server-side via loadAndValidateBed; this widget
// gives quick UX feedback and keeps three hidden inputs in sync with the rows.

/* jshint esnext: true */
/* jshint -W014 */
/* jshint -W087 */
/* global document */

var myVariantsBlocks = (function () {

    function makeRow(offsetVal, sizeVal, onChange, onRemove) {
        var row = document.createElement("div");
        row.className = "blockRow";
        row.style.cssText = "display:flex; align-items:center; gap:6px; margin-bottom:4px;";

        var offsetInput = document.createElement("input");
        offsetInput.type = "number";
        offsetInput.min = "0";
        offsetInput.className = "blockStart";
        offsetInput.style.width = "90px";
        offsetInput.placeholder = "offset";
        if (offsetVal !== undefined && offsetVal !== null) {
            offsetInput.value = offsetVal;
        }

        var sizeInput = document.createElement("input");
        sizeInput.type = "number";
        sizeInput.min = "1";
        sizeInput.className = "blockSize";
        sizeInput.style.width = "90px";
        sizeInput.placeholder = "size";
        if (sizeVal !== undefined && sizeVal !== null) {
            sizeInput.value = sizeVal;
        }

        var removeBtn = document.createElement("button");
        removeBtn.type = "button";
        removeBtn.textContent = "x";
        removeBtn.title = "Remove this block";
        removeBtn.style.cssText = "border:none; background:none; color:#c00; font-size:18px; cursor:pointer; padding:0 4px; line-height:1;";

        offsetInput.addEventListener("input", onChange);
        sizeInput.addEventListener("input", onChange);
        removeBtn.addEventListener("click", function (e) {
            // Stop the click before the dialog's outside-click handler sees
            // the target detached and decides we clicked outside.
            e.stopPropagation();
            onRemove();
        });

        row.appendChild(document.createTextNode("offset "));
        row.appendChild(offsetInput);
        row.appendChild(document.createTextNode(" size "));
        row.appendChild(sizeInput);
        row.appendChild(removeBtn);
        return row;
    }

    function parseRows(listEl) {
        var rows = listEl.querySelectorAll(".blockRow");
        var starts = [];
        var sizes = [];
        for (var i = 0; i < rows.length; i++) {
            var s = rows[i].querySelector(".blockStart");
            var z = rows[i].querySelector(".blockSize");
            var sv = parseInt(s.value, 10);
            var zv = parseInt(z.value, 10);
            starts.push(isNaN(sv) ? null : sv);
            sizes.push(isNaN(zv) ? null : zv);
        }
        return {starts: starts, sizes: sizes, rows: rows};
    }

    // Light row-level sanity check while the user is mid-edit. Only flags
    // values the user has actually typed; never complains about empty fields,
    // first-offset-is-0, or last-block-reaches-end. Those structural rules
    // belong in strictValidate (called explicitly at submit time).
    function liveRowCheck(starts, sizes) {
        for (var i = 0; i < starts.length; i++) {
            if (starts[i] !== null && starts[i] < 0) {
                return {ok: false, row: i, msg: "Offset must be >= 0"};
            }
            if (sizes[i] !== null && sizes[i] <= 0) {
                return {ok: false, row: i, msg: "Size must be > 0"};
            }
        }
        return {ok: true};
    }

    // Full BED12 invariants. Caller invokes this just before submit; the
    // server's loadAndValidateBed is the authoritative re-check.
    // Returns noBlocks:true when no row has a size filled in -- caller
    // treats that as "no blocks" so the server synthesizes a single
    // full-span block, matching the user's "I changed my mind" intent.
    function strictValidate(starts, sizes, getStart, getEnd) {
        if (starts.length === 0) {
            return {ok: true, noBlocks: true};
        }
        var hasAnySize = sizes.some(function (s) { return s !== null; });
        if (!hasAnySize) {
            return {ok: true, noBlocks: true};
        }
        var i;
        for (i = 0; i < starts.length; i++) {
            if (starts[i] === null || sizes[i] === null) {
                return {ok: false, row: i, msg: "Both offset and size are required"};
            }
            if (starts[i] < 0 || sizes[i] <= 0) {
                return {ok: false, row: i, msg: "Offset must be >= 0 and size > 0"};
            }
        }
        var span = getEnd() - getStart();
        if (span <= 0) {
            return {ok: false, row: 0, msg: "End must be greater than Start"};
        }
        if (starts[0] !== 0) {
            return {ok: false, row: 0, msg: "First offset must be 0"};
        }
        for (i = 1; i < starts.length; i++) {
            if (starts[i] < starts[i-1] + sizes[i-1]) {
                return {ok: false, row: i, msg: "Blocks must be ordered and non-overlapping"};
            }
        }
        var last = starts.length - 1;
        if (starts[last] + sizes[last] > span) {
            return {ok: false, row: last, msg: "Last block runs past End"};
        }
        if (starts[last] + sizes[last] !== span) {
            return {ok: false, row: last,
                    msg: "Last block must reach End (offset + size must equal End - Start = " + span + ")"};
        }
        return {ok: true};
    }

    function commaList(arr) {
        return arr.map(function (n) { return n === null ? "" : String(n); }).join(",");
    }

    function mount(containerOrId, opts) {
        // containerOrId may be either an element or a DOM id string.
        var container = (typeof containerOrId === "string")
            ? document.getElementById(containerOrId)
            : containerOrId;
        if (!container) {
            return null;
        }
        container.textContent = "";

        var listDiv = document.createElement("div");
        listDiv.className = "blockListDiv";

        var addBtn = document.createElement("button");
        addBtn.type = "button";
        addBtn.textContent = "+ Add block";
        addBtn.style.cssText = "margin-top:4px; padding:3px 10px; font-size:12px; cursor:pointer;";

        var countLabel = document.createElement("div");
        countLabel.style.cssText = "font-size:12px; color:#666; margin-top:4px;";

        var errLabel = document.createElement("div");
        errLabel.style.cssText = "font-size:12px; color:#c00; margin-top:4px;";

        function syncHiddens(data) {
            if (opts.hiddenCountInput) {
                opts.hiddenCountInput.value = data.rows.length;
            }
            if (opts.hiddenSizesInput) {
                opts.hiddenSizesInput.value = commaList(data.sizes);
            }
            if (opts.hiddenStartsInput) {
                opts.hiddenStartsInput.value = commaList(data.starts);
            }
        }

        // Live updates: sync hidden inputs and only show row-level garbage
        // (negative offset, non-positive size).  Structural rules wait for
        // an explicit validate() call from the caller's submit handler.
        function refresh() {
            var data = parseRows(listDiv);
            countLabel.textContent = "blockCount: " + data.rows.length +
                (data.rows.length === 0 ? " (single full-span block will be stored)" : "");

            for (var i = 0; i < data.rows.length; i++) {
                data.rows[i].style.outline = "";
            }
            var live = liveRowCheck(data.starts, data.sizes);
            if (!live.ok && data.rows[live.row]) {
                data.rows[live.row].style.outline = "1px solid #c00";
            }
            errLabel.textContent = live.ok ? "" : live.msg;
            syncHiddens(data);
        }

        // Strict pre-submit validation. Returns {ok, msg}; highlights the
        // offending row and shows the message. Server is still the
        // authoritative validator.
        function validate() {
            var data = parseRows(listDiv);
            for (var i = 0; i < data.rows.length; i++) {
                data.rows[i].style.outline = "";
            }
            var result = strictValidate(data.starts, data.sizes,
                                        opts.getStart, opts.getEnd);
            if (!result.ok && data.rows[result.row]) {
                data.rows[result.row].style.outline = "1px solid #c00";
            }
            errLabel.textContent = result.ok ? "" : result.msg;
            syncHiddens(data);
            return result;
        }

        function addRow(offsetVal, sizeVal) {
            // First row's offset is always 0 per BED12 spec; pre-fill it so
            // the user doesn't have to think about that constraint.
            if (offsetVal === undefined &&
                listDiv.querySelectorAll(".blockRow").length === 0) {
                offsetVal = 0;
            }
            var row = makeRow(offsetVal, sizeVal, refresh, function () {
                listDiv.removeChild(row);
                refresh();
            });
            listDiv.appendChild(row);
            refresh();
        }

        addBtn.addEventListener("click", function () { addRow(); });

        container.appendChild(listDiv);
        container.appendChild(addBtn);
        container.appendChild(countLabel);
        container.appendChild(errLabel);

        if (opts.initialSizes && opts.initialStarts &&
            opts.initialSizes.length === opts.initialStarts.length) {
            for (var i = 0; i < opts.initialSizes.length; i++) {
                addRow(opts.initialStarts[i], opts.initialSizes[i]);
            }
        }
        refresh();

        function clear() {
            while (listDiv.firstChild) {
                listDiv.removeChild(listDiv.firstChild);
            }
            refresh();
        }

        return {
            addRow: addRow,
            refresh: refresh,
            validate: validate,
            clear: clear,
            getRowCount: function () {
                return listDiv.querySelectorAll(".blockRow").length;
            }
        };
    }

    return { mount: mount };
})();
