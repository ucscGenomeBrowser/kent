// hgTablesDropdowns.js - searchable group, track and table dropdowns on the Table Browser pages.
//
// Turned on by the hg.conf setting hgta.searchableDropdowns.  hgTables marks each select with the
// class tbSearchable.  Most are searched in the browser.  A table dropdown with data-ajax holds only
// the selected table, because the full list can be tens of thousands of tables (big hub
// collections).  Its options come from hgTables?hgta_doTableList as the user types.
//
// The selects keep their own change handlers (they submit the page), and TomSelect fires a native
// change event on the select, so nothing else on the page has to know about the widget.

/* jshint esversion: 6 */
/* global TomSelect, getHgsid */

var tbDropdowns = [];         // the TomSelect objects on this page
var tbMoreValue = '\u0000more';  // value of the "N more" note at the end of an ajax list

function tbRegionIsGenome()
/* Return true if the region radio button is set to genome. */
{
    var el = document.querySelector('input[name="hgta_regionType"]:checked');
    return el !== null && el.value === 'genome';
}

function tbTableLoader(sel)
/* Return the TomSelect load function for the ajax table dropdown sel. */
{
    return function(query, callback) {
        var self = this;
        var url = 'hgTables?hgsid=' + encodeURIComponent(getHgsid()) +
                  '&hgta_doTableList=' + encodeURIComponent(sel.name) +
                  '&hgta_doTableListQuery=' + encodeURIComponent(query);
        fetch(url, {credentials: 'same-origin'})
            .then(function(resp) {
                if (!resp.ok)
                    throw new Error('HTTP ' + resp.status);
                return resp.json();
            })
            .then(function(data) {
                var isGenome = tbRegionIsGenome();
                var items = data.items.map(function(item) {
                    item.disabled = (item.noGenome === true && isGenome);
                    return item;
                });
                if (data.total > items.length)
                    items.push({value: tbMoreValue, disabled: true,
                                text: (data.total - items.length).toLocaleString() +
                                      ' more, type more to narrow the list'});
                // Only show this answer: clearOptions keeps the selected table.
                self.clearOptions();
                // TomSelect would not ask again for a query it has loaded once.
                self.loadedSearches = {};
                callback(items);
            })
            .catch(function() {
                callback();
            });
    };
}

function tbSetupAjax(sel)
/* Make the table dropdown sel searchable, with its options loaded from the server. */
{
    return new TomSelect(sel, {
        plugins: {dropdown_input: {}},
        placeholder: 'Type to search ' + Number(sel.dataset.count).toLocaleString() + ' tables',
        valueField: 'value',
        labelField: 'text',
        searchField: ['text'],
        maxOptions: null,
        preload: 'focus',
        loadThrottle: 250,
        dropdownParent: 'body',
        load: tbTableLoader(sel),
        // The server has already matched every word of the query, show all it sent.
        score: function() { return function() { return 1; }; },
        render: {
            loading: function() { return '<div class="loading">Searching...</div>'; },
            no_results: function() { return '<div class="no-results">No matching table</div>'; }
        }
    });
}

function tbSetupLocal(sel)
/* Make the dropdown sel searchable in the browser. */
{
    return new TomSelect(sel, {
        plugins: {dropdown_input: {}},
        placeholder: 'Type to search',
        maxOptions: null,
        dropdownParent: 'body',
        render: {
            no_results: function() { return '<div class="no-results">No match</div>'; }
        }
    });
}

function tbDropdownsSync(regionIsGenome)
/* Called by maybeDisableNoGenome() when the noGenome options were disabled or enabled. */
{
    tbDropdowns.forEach(function(ts) {
        if (ts.input.dataset.ajax) {
            Object.keys(ts.options).forEach(function(value) {
                var opt = ts.options[value];
                if (opt.noGenome === true && opt.disabled !== regionIsGenome)
                    ts.updateOption(value, Object.assign({}, opt, {disabled: regionIsGenome}));
            });
        } else {
            ts.sync();
        }
    });
}

function tbDropdownsInit()
/* Turn every select marked tbSearchable into a searchable dropdown. */
{
    if (typeof TomSelect === 'undefined')
        return;
    document.querySelectorAll('select.tbSearchable').forEach(function(sel) {
        tbDropdowns.push(sel.dataset.ajax ? tbSetupAjax(sel) : tbSetupLocal(sel));
    });
}

if (document.readyState === 'loading')
    document.addEventListener('DOMContentLoaded', tbDropdownsInit);
else
    tbDropdownsInit();
