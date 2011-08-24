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



