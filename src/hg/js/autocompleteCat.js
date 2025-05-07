// autocompleteCat: customized JQuery autocomplete plugin that includes watermark and
// can display results broken down by category (for example, genomes from various
// assembly hubs and native genomes).

// Copyright (C) 2018 The Regents of the University of California

///////////////////////////// Module: autocompleteCat /////////////////////////////

/* jshint esnext: true */

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
                   ul.append("<div class='autoCompleteInfo' style='color:grey'>Unable to find a genome? <a target=_blank href='../assemblySearch.html'>Send us a request</a></div>");
               },
               _renderItem: function(ul, item) {
                 // In order to use HTML markup in the autocomplete, one has to overwrite
                 // autocomplete's _renderItem method using .html instead of .text.
                 // http://forum.jquery.com/topic/using-html-in-autocomplete
                   let clockIcon = '';
                   if ($("#positionInput").val().length < 2) {
                       clockIcon = '<svg xmlns="http://www.w3.org/2000/svg" height=".75em" width=".75em" viewBox="0 0 512 512"><!--!Font Awesome Free 6.5.1 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license/free Copyright 2024 Fonticons, Inc.--><path d="M75 75L41 41C25.9 25.9 0 36.6 0 57.9V168c0 13.3 10.7 24 24 24H134.1c21.4 0 32.1-25.9 17-41l-30.8-30.8C155 85.5 203 64 256 64c106 0 192 86 192 192s-86 192-192 192c-40.8 0-78.6-12.7-109.7-34.4c-14.5-10.1-34.4-6.6-44.6 7.9s-6.6 34.4 7.9 44.6C151.2 495 201.7 512 256 512c141.4 0 256-114.6 256-256S397.4 0 256 0C185.3 0 121.3 28.7 75 75zm181 53c-13.3 0-24 10.7-24 24V256c0 6.4 2.5 12.5 7 17l72 72c9.4 9.4 24.6 9.4 33.9 0s9.4-24.6 0-33.9l-65-65V152c0-13.3-10.7-24-24-24z"/></svg>&nbsp';
                   }
                   // Hits to assembly hub top level (not individial db names) have no item label,
                   // so use the value instead
                   return $("<li></li>")
                       .data("ui-autocomplete-item", item)
                       .append($("<a></a>").html(clockIcon + (item.label !== null ? item.label : item.value)))
                       .appendTo(ul);
               }
             });

    function toggleSpinner(add, options) {
        if (options.baseUrl.startsWith("hgGateway")) {
            // change the species select loading image
            if (add)
                $("#speciesSearch").after("<i id='speciesSpinner' class='fa fa-spin fa-spinner'></i>");
            else
                $("#speciesSpinner").remove();
        } else if (options.baseUrl.startsWith("hgSuggest")) {
            // change the position input loading spinner
            if (add)
                $("#positionInput").after("<i id='suggestSpinner' class='fa fa-spin fa-spinner'></i>");
            else
                $("#suggestSpinner").remove();
        }
    }

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
            var url = options.baseUrl + encodeURIComponent(term);
            if (!options.baseUrl.startsWith("hubApi")) {
                // hubApi doesn't tolerate extra arguments
                url += '&_=' + timestamp;
            }
            // put up a loading icon so users know something is happening
            toggleSpinner(true, options);
            $.getJSON(url)
               .done(function(results) {
                if (_.isFunction(options.onServerReply)) {
                    results = options.onServerReply(results, term);
                }
                // remove the loading icon
                toggleSpinner(false, options);
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
            if (this.element[0].id === "positionInput" && request.term.length < 2) {
                let searchStack = window.localStorage.getItem("searchStack");
                if (request.term.length === 0 && searchStack) {
                    let searchObj = JSON.parse(searchStack);
                    let currDb = getDb();
                    if (currDb in searchObj) {
                        // sort the results list according to the stack order:
                        let entries = Object.entries(searchObj[currDb].results);
                        let stack = searchObj[currDb].stack;
                        let callbackData = [];
                        for (let s of stack) {
                            callbackData.push(searchObj[currDb].results[s]);
                        }
                        acCallback(callbackData);
                    }
                    return;
                }
            } else if (request.term.length >=2) {
                let results = cache[request.term];
                if (results) {
                    acCallback(results);
                } else if (options.baseUrl) {
                    doSearch(request.term, acCallback);
                }
            }
        };

        var autoCompleteSelect = function(event, ui) {
            // This is a callback for autocomplete to let us know that the user selected
            // a term from the list.  See http://api.jqueryui.com/autocomplete/#event-select
            // since we are in an autocomplete don't bother saving the
            // prefix the user typed in, just keep the geneSymbol itself
            if (this.id === "positionInput") {
                addRecentSearch(getDb(), ui.item.geneSymbol, ui.item);
            }
            options.onSelect(ui.item);
            $input.blur();
        };

        // Provide default values where necessary:
        options.onSelect = options.onSelect || console.log;
        options.enterSelectsIdentical = options.enterSelectsIdentical || false;

        $input.autocompleteCat({
            delay: 500,
            minLength: 0,
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
