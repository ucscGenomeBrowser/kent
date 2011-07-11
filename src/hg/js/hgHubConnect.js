
//<script type="text/javascript" >
    // hover effect to highlight table rows
    $(function(){
    	$(".hubList tr").hover(
    		function(){ $(this).addClass("hoverRow");},
    		function(){ $(this).removeClass("hoverRow"); }
    		)
    	});
    $(function() {
	    $( "#tabs" ).tabs();
	});
//</script>

