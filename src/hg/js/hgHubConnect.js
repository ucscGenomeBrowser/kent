// hover effect to highlight table rows
$(function(){
	$(".hubList tr").hover(
   		function(){ $(this).addClass("hoverRow");},
   		function(){ $(this).removeClass("hoverRow"); }
   	)
});


// initializes the tabs - with cookie option
// cookie option requires jquery.cookie.js
$(function() {
	$( "#tabs" ).tabs({ cookie: { name: 'hubTab_cookie', expires: 30 } });
});


// creates keyup event; listening for return key press
$(document).ready(function() {
    $('#hubUrl').bind('keypress', function(e) {  // binds listener to url field
		if (e.which == 13) {  // listens for return key
		        e.preventDefault();   // prevents return from also submitting whole form
                        if(validateUrl($('#hubUrl').val()))
		            $('input[name="hubAddButton"]').focus().click();  // clicks the AddHub button
		}
    })
});




