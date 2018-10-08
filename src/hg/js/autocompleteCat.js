// autocompleteCat: customized JQuery autocomplete plugin that includes watermark and
// can display results broken down by category (for example, genomes from various
// assembly hubs and native genomes).

// Copyright (C) 2018 The Regents of the University of California

///////////////////////////// Module: autocompleteCat /////////////////////////////

var autocompleteCat = (function() {
    // Customize jQuery UI autocomplete to show item categories and support html markup in labels.
    // Adapted from https://jqueryui.com/autocomplete/#categories and
    // http://forum.jquery.com/topic/using-html-in-autocomplete
    // Also adds watermark to input.
    $.widget("custom.autocompleteCat",
             $.ui.autocomplete,
             {
               _renderMenu: function(ul, items) {
                   var that = this;
                   var currentCategory = "";
                   // There's no this._super as shown in the doc, so I can't override
                   // _create as shown in the doc -- just do this every time we render...
                   this.widget().menu("option", "items", "> :not(.ui-autocomplete-category)");
                   $.each(items,
                          function(index, item) {
                              // Add a heading each time we see a new category:
                              if (item.category && item.category !== currentCategory) {
                                  ul.append("<li class='ui-autocomplete-category'>" +
                                            item.category + "</li>" );
                                  currentCategory = item.category;
                              }
                              that._renderItem( ul, item );
                          });
               },
               _renderItem: function(ul, item) {
                 // In order to use HTML markup in the autocomplete, one has to overwrite
                 // autocomplete's _renderItem method using .html instead of .text.
                 // http://forum.jquery.com/topic/using-html-in-autocomplete
                   return $("<li></li>")
                       .data("item.autocomplete", item)
                       .append($("<a></a>").html(item.label))
                       .appendTo(ul);
               }
             });

    function init($input, options) {
        // Set up an autocomplete and watermark for $input, with a callback options.onSelect
        // for when the user chooses a result.
        // If options.baseUrl is null, the autocomplete will not do anything, but we (re)initialize
        // it anyway in case the same input had a previous db's autocomplete in effect.
        // options.onServerReply (if given) is a function (Array, term) -> Array that
        // post-processes the list of items returned by the server before the list is
        // passed back to autocomplete for rendering.
        // The following two options apply only when using our locally modified jquery-ui:
        // If options.enterSelectsIdentical is true, then if the user hits Enter in the text input
        // and their term has an exact match in the autocomplete results, that result is selected.
        // options.onEnterTerm (if provided) is a callback function (jqEvent, jqUi) invoked
        // when the user hits Enter, after handling enterSelectsIdentical.

        // The function closure allows us to keep a private cache of past searches.
        var cache = {};

        var doSearch = function(term, acCallback) {
            // Look up term in searchObj and by sending an ajax request
            var timestamp = new Date().getTime();
            var url = options.baseUrl + encodeURIComponent(term) + '&_=' + timestamp;
            $.getJSON(url)
               .done(function(results) {
                if (_.isFunction(options.onServerReply)) {
                    results = options.onServerReply(results, term);
                }
                cache[term] = results;
                acCallback(results);
            });
            // ignore errors to avoid spamming people on flaky network connections
            // with tons of error messages (#8816).
        };

        var autoCompleteSource = function(request, acCallback) {
            // This is a callback for jqueryui.autocomplete: when the user types
            // a character, this is called with the input value as request.term and an acCallback
            // for this to return the result to autocomplete.
            // See http://api.jqueryui.com/autocomplete/#option-source
            var results = cache[request.term];
            if (results) {
                acCallback(results);
            } else if (options.baseUrl) {
                doSearch(request.term, acCallback);
            }
        };

        var autoCompleteSelect = function(event, ui) {
            // This is a callback for autocomplete to let us know that the user selected
            // a term from the list.  See http://api.jqueryui.com/autocomplete/#event-select
            options.onSelect(ui.item);
            $input.blur();
        };

        // Provide default values where necessary:
        options.onSelect = options.onSelect || console.log;
        options.enterSelectsIdentical = options.enterSelectsIdentical || false;

        $input.autocompleteCat({
            delay: 500,
            minLength: 2,
            source: autoCompleteSource,
            select: autoCompleteSelect,
            enterSelectsIdentical: options.enterSelectsIdentical,
            enterTerm: options.onEnterTerm
        });

        if (options.watermark) {
            $input.css('color', 'black');
            $input.Watermark(options.watermark, '#686868');
        }
    }

    return { init: init };
}()); // autocompleteCat
