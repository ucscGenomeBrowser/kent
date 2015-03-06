// support stuff for auto-complete using jQuery UI's autocomplete widget
// Based on Genome Browser code by Larry Meyers.  Adapted by Jim Kent
//
// requires ajax.js
// requires utils.js

var cdwSuggest = {
    ajaxGet: function ajaxGet(fieldName) {
        // Returns autocomplete source function
	// FieldName is a field in the cdwFileTags table
        var cache = {}; // cache is is used as a hash to cache responses from the server.
        return function(request, callback) {
            var key = request.term;
            if (!cache[key]) {
                $.ajax({
                    url: "../cgi-bin/cdwSuggest",
                    data: "field=" + fieldName + "&prefix=" + encodeURIComponent(key),
                    // dataType: "json",  // XXXX this doesn't work under IE, so we retrieve as text and do an eval to force to an object.
                    trueSuccess: function(response, status) {
                        // We get a lot of duplicate requests (especially the first letters of words),
                        // so we keep a cache of the suggestions lists we've retreived.
                        cache[this.key] = response;
                        this.cont(JSON.parse(response));
                    },
                    success: catchErrorOrDispatch,
                    error: function(request, status, errorThrown) {
                        // tolerate errors (i.e. don't report them) to avoid spamming people on flaky network connections
                        // with tons of error messages (#8816).
                    },
                    key: key,
                    cont: callback
                });
            } else {
                callback(JSON.parse(cache[key]));
            }
            // warn(request.term);
        };
    },

    // init function,  parameters are
    // fieldName - the name of the field we are working on
    // widget - Recommend using a "#id" string here.  Id of text input control
    init: function(fieldName, textInput) {
        var lastSelected = null; // this is the last value entered by the user via a suggestion (used to distinguish manual entry in the same field)
        // $(textInput).val("");
	$(textInput).autocomplete({
	    delay: 500,
	    minLength: 0,
	    source: this.ajaxGet(fieldName),
	    open: function(event, ui) {
		var pos = $(this).offset().top + $(this).height();
		if (!isNaN(pos)) {
		    var maxHeight = $(window).height() - pos - 30; // take off a little more because IE needs it
		    var auto = $('.ui-autocomplete');
		    var curHeight = $(auto).children().length * 21;
		    if (curHeight > maxHeight) $(auto).css({
			maxHeight: maxHeight + 'px',
			overflow: 'scroll',
			zIndex: 12
		    });
		    else $(auto).css({
			maxHeight: 'none',
			overflow: 'hidden',
			zIndex: 12
		    });
		}
	    },
	    select: function(event, ui) {
		lastSelected = ui.item.value;
	    }
	});

        // I want to set focus to the suggest element, but unforunately that prevents PgUp/PgDn from
        // working, which is a major annoyance.
        // $(textInput).focus();
        $(textInput).change(function(event) {
            if (!lastSelected || lastSelected !== $(textInput).val()) {
		$.submit();
                // This handles case where user typed or edited something rather than choosing from a suggest list;
//                var val = $(textInput).val();
//		alert("cdwSuggest.js changed to " + val);
                // Needed??? $(hiddenWidget).val(val);
            }
        });
    }
};
