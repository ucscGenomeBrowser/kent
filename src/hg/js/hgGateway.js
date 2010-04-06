
$(document).ready(function()
{
    $('input#suggest').autocomplete({
                                        delay: 500,
                                        minchars: 2,
                                        autowidth: true,
                                        ajax_get: ajaxGet(function () {return document.orgForm.db.value;}, new Object),
                                        callback: function (obj) {
                                            document.mainForm.position.value = obj.id;
                                        }
                                       });
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
