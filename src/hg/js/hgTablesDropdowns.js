// hgTablesDropdowns.js - searchable group, track and table dropdowns on the Table Browser pages.
//
// Turned on by the hg.conf setting hgta.searchableDropdowns.  hgTables marks each select with the
// class tbSearchable.  Most are searched in the browser.  A table dropdown with data-ajax holds only
// the selected table, because the full list can be tens of thousands of tables (big hub
// collections).  Its options come from hgTables?hgta_doTableList as the user types.
//
// The widget is Selectize (jQuery).  The selects keep their own change handlers (they submit the
// page), and Selectize triggers change on the select, so nothing else on the page has to know
// about the widget.

/* jshint esversion: 6 */
/* global getHgsid, $ */

var tbDropdowns = [];         // the Selectize objects on this page
var tbMoreValue = '\u0000more';  // value of the "N more" note at the end of an ajax list

function tbRegionIsGenome()
/* Return true if the region radio button is set to genome. */
{
    var el = document.querySelector('input[name="hgta_regionType"]:checked');
    return el !== null && el.value === 'genome';
}

function tbTypeToSearch(config)
/* Add focus and blur handlers to the Selectize config, so that typing starts a search.
 * A single-choice Selectize ignores typing while an item is selected; the user would have to
 * press Backspace first.  So on focus the item is taken out quietly and shown as placeholder,
 * and on blur it is put back if nothing else was chosen. */
{
    var placeholder = config.placeholder || '';
    // Tab moves on to the next field, it does not pick the highlighted option (that submits)
    config.selectOnTab = false;
    config.dropdownParent = 'body';
    config.onFocus = function() {
        // Selectize 0.15 only closes on a mousedown elsewhere in the page, and the mousedown on
        // another Selectize never gets there; tabbing to it does not either.  So close the others.
        var self = this;
        tbDropdowns.forEach(function(other) {
            if (other !== self && other.isFocused)
                other.blur();
        });
        var value = this.getValue();
        var opt = this.options[value];
        if (value === '' || !opt)
            return;
        // an ajax dropdown drops the options of earlier searches, so keep the option's data
        this.tbSavedOption = Object.assign({}, opt);
        this.settings.placeholder = opt[this.settings.labelField];
        this.clear(true);
        this.updatePlaceholder();
    };
    config.onBlur = function() {
        var saved = this.tbSavedOption;
        if (this.getValue() === '' && saved !== undefined) {
            var value = saved[this.settings.valueField];
            if (!this.options[value])
                this.addOption(saved);
            this.setValue(value, true);
        }
        this.tbSavedOption = undefined;
        this.settings.placeholder = placeholder;
        this.updatePlaceholder();
    };
    return config;
}

function tbTableLoader(sel)
/* Return the Selectize load function for the ajax table dropdown sel. */
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
                // Only show this answer.  Selectize's clearOptions() would also unselect
                // the selected table, so drop the other options by hand.
                Object.keys(self.options).forEach(function(value) {
                    if (self.items.indexOf(value) < 0)
                        delete self.options[value];
                });
                self.sifter.items = self.options;
                self.renderCache = {};
                // Selectize would not ask again for a query it has loaded once.
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
    $(sel).selectize(tbTypeToSearch({
        placeholder: 'Type to search ' + Number(sel.dataset.count).toLocaleString() + ' tables',
        valueField: 'value',
        labelField: 'text',
        searchField: ['text'],
        maxOptions: 1000,
        preload: 'focus',
        loadThrottle: 250,
        load: tbTableLoader(sel),
        // The server has already matched every word of the query, show all it sent.
        score: function() { return function() { return 1; }; },
        render: {
            option: function(item, escape) {
                return '<div class="option">' + escape(item.text) + '</div>';
            }
        }
    }));
    return sel.selectize;
}

function tbSetupLocal(sel)
/* Make the dropdown sel searchable in the browser. */
{
    // Selectize keeps only the selected option in the select: remember which were noGenome.
    var noGenome = [];
    sel.querySelectorAll('option.hgtaNoGenome').forEach(function(opt) {
        noGenome.push(opt.value);
    });
    $(sel).selectize(tbTypeToSearch({
        maxOptions: 100000,
        // match anywhere in a word, not only at the start of one
        respect_word_boundaries: false
    }));
    var ss = sel.selectize;
    noGenome.forEach(function(value) {
        if (ss.options[value])
            ss.options[value].noGenome = true;
    });
    return ss;
}

function tbDropdownsSync(regionIsGenome)
/* Called by maybeDisableNoGenome() when the noGenome options were disabled or enabled. */
{
    tbDropdowns.forEach(function(ss) {
        Object.keys(ss.options).forEach(function(value) {
            var opt = ss.options[value];
            if (opt.noGenome === true && Boolean(opt.disabled) !== regionIsGenome)
                ss.updateOption(value, Object.assign({}, opt, {disabled: regionIsGenome}));
        });
    });
}

function tbDropdownsInit()
/* Turn every select marked tbSearchable into a searchable dropdown. */
{
    if (typeof $ === 'undefined' || typeof $.fn.selectize === 'undefined')
        return;
    document.querySelectorAll('select.tbSearchable').forEach(function(sel) {
        tbDropdowns.push(sel.dataset.ajax ? tbSetupAjax(sel) : tbSetupLocal(sel));
    });
}

if (document.readyState === 'loading')
    document.addEventListener('DOMContentLoaded', tbDropdownsInit);
else
    tbDropdownsInit();
