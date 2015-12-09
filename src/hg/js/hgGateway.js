$(document).ready(function() {
    suggestBox.init(document.orgForm.db.value, 
        $('#suggestTrack').length > 0, 
	function(item) {
	    $('#positionDisplay').text(item.id);
	    $('#position').val(item.id);
	}, 
	function(position) {
	    $('#positionDisplay').text(position);
	    $('#position').val(position);
	});

    // Default the image width to current browser window width (#2633).
    var ele = $('input[name=pix]');
    if (ele.length && (!ele.val() || ele.val().length === 0)) {
        ele.val(calculateHgTracksWidth());
    }

    if ($("#suggestTrack").length) {
        // Make sure suggestTrack is visible when user chooses something via gene select (#3484).
        $(document.mainForm).submit(function(event) {
            if ($('#hgFindMatches').length) {
                var track = $("#suggestTrack").val();
                if (track) {
                    $("<input type='hidden' name='" + track + "'value='pack'>").appendTo($(event.currentTarget));
                }
            }
        });
    }
});
