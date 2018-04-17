// hgVai functions: a few little dynamic page updates and a disclaimer before first submission

var hgva = // result of invoking:
(function()
{
    // Tell jslint and browser to use strict mode for this:
    "use strict";

    // Public methods/objects:
    return {
	changeRegion: function()
	{
	    var newVal = $('select[name="hgva_regionType"]').val();
	    if (newVal === 'range') {
		$('#positionContainer').show().focus();
	    } else {
		$('#positionContainer').hide();
	    }
	    setCartVar("hgva_regionType", newVal);
	},

	changeVariantSource: function()
	{
	    var newVal = $('select[name="hgva_variantTrack"]').val();
	    $('#variantPasteContainer').hide();
	    $('#hgvsPasteContainer').hide();
	    if (newVal === 'hgva_useVariantIds') {
		$('#variantPasteContainer').show().focus();
	    } else if (newVal === "hgva_useHgvs") {
                $('#hgvsPasteContainer').show().focus();
            }
	    setCartVar("hgva_variantTrack", newVal);
	},

        changeGeneSource: function()
        {
            // Every time the user changes the gene prediction track, see if any
            // HGVS or transcript status options can be shown for the new track.
            var newVal = $("select#hgva_geneTrack").val();
            var isRefSeq = (newVal === 'refGene' || newVal.match(/^ncbiRefSeq/)) ? true : false;
            // Look for a div with classes txStatus and the name of the new gene track --
            // if the new gene track is GENCODE, strip it down to only the version at end.
            var matchesGencode = newVal.match(/^wgEncodeGencode(Basic|Comp|PseudoGene)(V[A-Z]?[0-9]+)$/);
            var geneClass = matchesGencode ? matchesGencode[2] : newVal;
            var canDoHgvs = (isRefSeq ||
                             !!newVal.match(/wgEncodeGencode(Basic|Comp)(V[A-Z]?[0-9]+)$/));
            var visibleCount = 0;
            var $txStatusDivs = $("div.txStatus");
            $txStatusDivs.each(function(n, div) {
                var $div = $(div);
                var hasGeneClass = $div.hasClass(geneClass);
                // Set visibility according to whether the class list includes geneClass:
                $div.toggle(hasGeneClass);
                if (hasGeneClass) {
                    visibleCount++;
                }
            });
            $("div.noTxStatus").toggle(visibleCount === 0);
            $("div#hgvsOptions").toggle(canDoHgvs);
            $("div#noHgvs").toggle(!canDoHgvs);
        },

        goToAddCustomTrack: function()
        {
            var $ctForm = $('#customTrackForm');
            $ctForm.append('<INPUT TYPE=HIDDEN NAME="hgct_do_add" VALUE=""/>');
            $ctForm.submit();
            return false;
        },

	submitQuery: function()
	{
            // Show loading image and message -- unless downloading to a file
            if (! $('#hgva_outFile').val()) {
	        loadingImage.run();
            }
	    var startQueryHiddenInput = '<INPUT TYPE=HIDDEN NAME="hgva_startQuery" VALUE="1">';
	    $("#mainForm").append(startQueryHiddenInput).submit();
	},

	userClickedAgree: function()
	{
	    // Use async=false here so that setCartVar doesn't get terminated when we submit:
	    setCartVar("hgva_agreedToDisclaimer", "1", null, false);
	    hgva.submitQuery();
	},

	submitQueryIfDisclaimerAgreed: function()
	{
	    if (hgva.disclaimer.check()) {
		hgva.submitQuery();
	    }
	},

	disclaimer: (function()  // disclaimer object results from this invocation
	{
	    var gotAgreement = false;
	    var $dialog;

	    return {
		init: function(agreed, agreeFunction) {
		// If there is a dialog box for disclaimer notice and user has not already
		// agreed, initialize it to warn & ask for user's agreement.
		// When user clicks to agree, close dialog and run agreeFunction.
		    $dialog = $("#disclaimerDialog");
		    if (!$dialog) {
			return;
		    }
		    if (agreed) {
			gotAgreement = true;
			$dialog.hide();
		    } else {
			var closeDialog = function() { $dialog.dialog("close"); };
			var onClickAgree = function() {
			    gotAgreement = true;
			    closeDialog();
			    if (agreeFunction) {
				agreeFunction();
			    }
			};
			var dialogOpts = {
			    autoOpen: false,
			    buttons:
			    [ { text: "Cancel", click: closeDialog },
			      { text: "I understand and agree", click: onClickAgree } ],
			    dialogClass: "warnDialog",
			    width: 500
			};
			$dialog.dialog(dialogOpts);
		    }
		},

		check: function() {
		// Return true if the user has already agreed to the disclaimer.
		// If they haven't agreed, show them the disclaimer dialog box and
		// return false (wait for dialog callback agreeFunction).
		    if (!$dialog || gotAgreement) {
			return true;
		    }
		    $dialog.dialog("open");
		    return false;
		}
	    };
	}()) // end & invocation of function that makes hgva.disclaimer
    };
}()); // end & invocation of function that makes hgva


$(document).ready(function()
{
    // initialize ajax.js's loadingImage, to warn user that query might take a while.
    loadingImage.init($("#loadingImg"), $("#loadingMsg"),
		      "<p style='color: red; font-style: italic;'>" +
		      "Executing your query may take some time. " +
		      "Please leave this window open during processing.</p>");
});
