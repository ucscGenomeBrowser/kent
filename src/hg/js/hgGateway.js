var newJQuery = false;       // temporary #define for use while testing jQuery 1.5/jQuery UI 1.8 in dev trees

$(document).ready(function()
{
    if(document.getElementById("hgt.newJQuery") != null) {
        newJQuery = true;
    }
    if(newJQuery) {
        $('input#suggest').autocomplete({
                                            delay: 500,
                                            minLength: 2,
                                            // XXXX necessary still? autowidth: true,
                                            source: ajaxGet(function () {return document.orgForm.db.value;}, new Object, true),
                                            open: function(event, ui) {
                                                var pos = $(this).offset().top + $(this).height();
                                                if (!isNaN(pos)) {
                                                    var maxHeight = $(window).height() - pos - 30;  // take off a little more because IE needs it
                                                    var auto = $('.ui-autocomplete');
                                                    var curHeight = $(auto).children().length * 21;
                                                    if (curHeight > maxHeight)
                                                        $(auto).css({maxHeight: maxHeight+'px',overflow:'scroll'});
                                                    else
                                                        $(auto).css({maxHeight: 'none',overflow:'hidden'});
                                                }
                                            },
                                            select: function (event, ui) {
                                                document.mainForm.position.value = ui.item.id;
                                                makeSureSuggestTrackIsVisible();
                                            }
                                        });
    } else {
        $('input#suggest').autocomplete({
                                            delay: 500,
                                            minchars: 2,
                                            autowidth: true,
                                            ajax_get: ajaxGet(function () {return document.orgForm.db.value;}, new Object, false),
                                                callback: function (obj) {
                                                document.mainForm.position.value = obj.id;
                                                makeSureSuggestTrackIsVisible();
                                            }
                                        });
    }
    // default the image width to current browser window width
    var ele = $('input[name=pix]');
    if(ele.length && (!ele.val() || ele.val().length == 0)) {
        ele.val(calculateHgTracksWidth());
    }
});

function submitButtonOnClick()
{
// onClick handler for the "submit" button.
// Handles situation where user types a gene name into the gene box and immediately hits the submit button,
// expecting the browser to jump to that gene.
    var gene = $('#suggest').val();
    var db = $('input[name=db]').val();
    if(gene && gene.length > 0) {
        var pos = lookupGene(db, gene);
        if(!pos) {
            // turn this into a full text search.
            pos = gene;
        }
        $('input[name=position]').val(pos);
    }
    return true;
}

function makeSureSuggestTrackIsVisible()
{
// make sure to show knownGene/refGene track in pack mode (redmine #3484).
    var track = $("#suggestTrack").val();
    if(track) {
        $("<input type='hidden' name='" + track + "'value='pack'>").appendTo($(document.mainForm));
    }
}
