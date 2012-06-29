$(document).ready(function()
{
    suggestBox.init(document.orgForm.db.value, $('#suggestTrack').length > 0,
                      function(item) {
                          $('#positionDisplay').text(item.id);
                          $('#position').val(item.id);
                      },
                      function(position) {
                          $('#positionDisplay').text(position);
                          $('#position').val(position);
                      }
                   );
    // default the image width to current browser window width
    var ele = $('input[name=pix]');
    if(ele.length && (!ele.val() || ele.val().length == 0)) {
        ele.val(calculateHgTracksWidth());
    }

    // Make sure suggestTrack is visible if user has choosen soemting via gene select.
    $(document.mainForm).submit(function(event)
                                {
                                    if($('#hgFindMatches').length) {
                                        var track = $("#suggestTrack").val();
                                        if(track) {
                                            $("<input type='hidden' name='" + track + "'value='pack'>").appendTo($(event.currentTarget));
                                        }
                                    }
                                });
});

