
var tipup;
var chrom;
var dbname;
var timed;
var tracks;
var tooldiv;
var currtip;
var timeout;

/*
function setwidth()
{
   var windwidth = $(window).width()  - 20;
   
   if($("#imgTbl").length == 0)
   {
       $("#TrackForm").append('<input type="hidden" name="pix" value="'+windwidth+'"/>');
       //$("#TrackForm").submit();
   }
   else
   {
       $("input[name=pix]").val(windwidth);
   }
    
}
*/

function makevisible(element)
{
    if($(element).css('display') == "none") 
    { 
        $(element).show(); 
    }
    return $(element);
}
function update(event)
{
    if(!tipup)
    {
        resettip(event.target);
    }

    var toolx = event.pageX+15;
    var tooly = event.pageY+15;
    var windwidth = $(window).width() + $(window).scrollLeft() - 10;
    var windheight = $(window).height() + $(window).scrollTop() - 10;
    if((event.pageX+15 + $("#tooltip").width() > windwidth) && event.pageY+15 + $("#tooltip").height() > windheight)
    {
        toolx = event.pageX- ($("#tooltip").width())-15;
        tooly = windheight - ($("#tooltip").height());
    }
    else if(event.pageX+15 + $("#tooltip").width() > windwidth)
    {
        toolx = windwidth - ($("#tooltip").width());
    }
    else if(event.pageY+15 + $("#tooltip").height() > windheight)
    {
        tooly = windheight - ($("#tooltip").height());
    }

    $("#tooltip").css('left',toolx).css('top',tooly);
}
function showInfo(event,output)
{

        tipup = true;
        currtip = event.target;
	$("#tooltip").html(output);
	$("#tooltip").css('left',0).css('top',0);
	$("#tooltip").css('width',$("#tooltip").width()+5);
	update(event);


	$("#tooltip").show();
	//update(event);

	$(event.target).bind('mousemove', update);
}
function ajaxtooltipError(XMLHttpRequest, textStatus, errorThrown,event)
{
    showInfo(event, "<B>Name:</B> "+event.target.data);
}



function resettip(hastip)
{
    //console.log("Tip gone"+tipup);
    tipup = false;
    $(hastip).unbind('mousemove', update);
    tooldiv.hide();
    tooldiv.text('');
    
    tooldiv.css('width','auto');
    
    
}
function hidetip(event)
{
    event.stopPropagation();
    if(! $(event.target).is("area[href^='../cgi-bin/hgc']"))
    {
    return false;
    }
    if(timeout)
    {
        clearTimeout(timeout);
        timeout = 0;
    }
    
    if(currrequest != null)
    {
    	currrequest.abort();
    }
    if(tipup && $(event.target).is("div.sliceDiv area[href^='../cgi-bin/hgc']"))
    {
    resettip(event.target);
    }
    //setTimeout("tracks.live('mouseout', hidetip)", 100);
    return false;
}


function showtip(event)
{
    event.stopPropagation();
    if( ! $(event.target).is("area[href^='../cgi-bin/hgc']"))
    {
    return false;
    }
    var ajaxtip = function() {getajaxtooltip(event);};
    timeout = setTimeout(ajaxtip, 300);
    return false;
}
function getajaxtooltip(event)
{
    currrequest = $.ajax({
            type: "GET",
            url: "../cgi-bin/tooltip?c="+chrom+"&db="+dbname+"&"+($(event.target).attr('href').split("?"))[1],
            dataType: "html",
            success: function(output){showInfo(event,output);},
            error: function(XMLHttpRequest, textStatus, errorThrown){ajaxtooltipError(XMLHttpRequest, textStatus, errorThrown,event);},
            cache: true
    });
    //tracks.die('mouseover', showtip);
    //setTimeout("tracks.live('mouseover', showtip)", 100);
    return false;
    
}
function removetitles(index, Element){
        Element.data = Element.title;
    	Element.title = "";
    	//Element.className = "hastip";
}
//try not enabling mouseout event
function enabletips()
{
    timeout = 0;
    $("#imgTbl").bind('mouseover', showtip);
    $("#imgTbl").bind('mouseout', hidetip);
    //tracks.live('mouseover', showtip);
    //tracks.live('mouseout', hidetip);
}
function disabletips()
{
    $("#imgTbl").unbind('mouseover', showtip);
    $("#imgTbl").unbind('mouseout', hidetip);
    //tracks.die('mouseover', showtip);
    //tracks.die('mouseout', hidetip);
}

$(document).ready(function() 
{
    if($("#imgTbl").length != 0)
    {
	    timed = 0;
	    currrequest = null;
	    tipup = false;
	    $("body").append("<div class='tooltip' id='tooltip'></div>");
	    tooldiv = $("#tooltip");
	    $("#tooltip").css({
	    "background": "white",
	    "border": "1px black solid",
	    "display": "none",
	    "position": "absolute",
	    "font-size": "small",
	    "opacity" : ".9",
	    "filter": "alpha(opacity = 90)",
	    "zoom": "1"
	    });
	    $("#trackMap").removeAttr("title");
	    chrom = $("input[name=chromName]").val();
	    dbname = $("input[name=db]").val();
	    tracks = $("area[href^='../cgi-bin/hgc']");
	    //tracks.addClass('hastip');
	    //tracks.removeAttr("title");
	    tracks.each(removetitles);
	    enabletips();
	    //$(window).scroll(tempdisable);
    }
    
    
    //    $("input[name=hgt.toggleRevCmplDisp]").after('<input type="submit" class="setwidth" value="auto-set width" onclick="setwidth()"/>');

    
    
});