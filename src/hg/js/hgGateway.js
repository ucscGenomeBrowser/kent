
$(document).ready(function()
{
    $('input#suggest').autocomplete({
                                        delay: 500,
                                        minchars: 1,
                                        autowidth: true,
                                        ajax_get: ajaxGet(function () {return document.orgForm.db.value;}, new Object),
                                        callback: function (obj) {
                                            document.mainForm.position.value = obj.id;
                                        }
                                       });
});
