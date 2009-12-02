
var tipup = -1;

function removetitles()
{
    $(".hastip").each(function(i)
    {
        $(this).data('title', $(this).attr('title'));
        $(this).removeAttr("title");
    });
}
function addtitles()
{
    $(".hastip").each(function(i)
    {
        $(this).attr('title', $(this).data('title'));
    });
}
function changetitles()
{
    if($("input[name=showtooltips]").attr('checked'))
    {
        removetitles();
    }
    else
    {
        addtitles();
    }
}


function setwidth()
{
   var windwidth = $(window).width() + $(window).scrollLeft() - 20;
   $("#map").prev().before('<input name="pix" type="hidden" value="'+windwidth+'"/>');
    
}
function basictracks()
{
   var i;
   $(".normalText").attr("value","hide");
   var packtracks= ["refSeq","gbHits","pfam","gbRNAs","cddInfo"];
   var fulltracks = ["ruler","gc20Base"];
   $("select[name^='multiz']").attr("value","pack");
   for(i in fulltracks)
   {
       $("select[name="+fulltracks[i]+"]").attr("value","full");
   }
   for(i in packtracks)
   {
       $("select[name="+packtracks[i]+"]").attr("value","pack");
   }
   //$("#TrackForm").submit();
    
}


function rnatracks()
{
   //$(".normaltext option:contains('hide')").text("GOAWAY");
   var i;
   $(".normalText").attr("value","hide");
   var packtracks= ["refSeq","CRISPRs","Rfam","tRNAs","snoRNAs","gbRNAs","alignInfo"];
   var fulltracks = ["ruler","gc20Base"];
   $("select[name^='multiz']").attr("value","pack");
   for(i in fulltracks)
   {
       //alert($("select[name="+RNAtracks[i]+"]").size());
       $("select[name="+fulltracks[i]+"]").attr("value","full");
   }
   for(i in packtracks)
   {
       //alert($("select[name="+RNAtracks[i]+"]").size());
       $("select[name="+packtracks[i]+"]").attr("value","pack");
   }
   //$("#TrackForm").submit();
    
}
function genepredtracks()
{
   //$(".normaltext option:contains('hide')").text("GOAWAY");
   var i;
   $(".normalText").attr("value","hide");
   var packtracks=["refSeq","allpredictions","gbHits","tigrCmrORFs","cddInfo","alignInfo"];
   $("select[name^='BlastP']").attr("value","dense");
   $("select[name^='BlastX']").attr("value","dense");
   $("select[name^='multiz']").attr("value","pack");
   var fulltracks = ["ruler","gc20Base"];
   for(i in fulltracks)
   {
       //alert($("select[name="+RNAtracks[i]+"]").size());
       $("select[name="+fulltracks[i]+"]").attr("value","full");
   }
   for(i in packtracks)
   {
       //alert($("select[name="+RNAtracks[i]+"]").size());
       $("select[name="+packtracks[i]+"]").attr("value","pack");
   }
   //$("#TrackForm").submit();
    
}
function regulationtracks()
{
   //$(".normaltext option:contains('hide')").text("GOAWAY");
   var i;
   $(".normalText").attr("value","hide");
   var packtracks=["refSeq","lowelabPromoter","tfBindSitePal"];
   
   var fulltracks = ["ruler","gc20Base","codonBias","promoterScanPos","promoterScanNeg","shineDGPos","shineDGNeg"];
   for(i in fulltracks)
   {
       //alert($("select[name="+RNAtracks[i]+"]").size());
       $("select[name="+fulltracks[i]+"]").attr("value","full");
   }
   for(i in packtracks)
   {
       //alert($("select[name="+RNAtracks[i]+"]").size());
       $("select[name="+packtracks[i]+"]").attr("value","pack");
   }
//   $("#TrackForm").submit();
    
}
function compgenotracks()
{
   //$(".normaltext option:contains('hide')").text("GOAWAY");
   var i;
   $(".normalText").attr("value","hide");
   var packtracks=["refSeq","alignInfo","insertionRegions"];
   $("select[name=ultraConserved]").attr("value","dense");
   $("select[name^='BlastP']").attr("value","dense");
   $("select[name^='BlastX']").attr("value","dense");
   $("select[name^='multiz']").attr("value","pack");
   $("select[name=blastzSelf").attr("value","dense");
   $("select[name=alloperons]").attr("value","dense");
   var fulltracks = ["ruler","gc20Base"];
   for(i in fulltracks)
   {
       //alert($("select[name="+RNAtracks[i]+"]").size());
       $("select[name="+fulltracks[i]+"]").attr("value","full");
   }
   for(i in packtracks)
   {
       //alert($("select[name="+RNAtracks[i]+"]").size());
       $("select[name="+packtracks[i]+"]").attr("value","pack");
   }
   //$("#TrackForm").submit();
    
}
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
    if(tipup == -1)
    {
        resettip(event.target);
    }
    //console.log(tipup);
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
    
    if($(event.target).data('num') === tipup)
    {
        $(".tooltip").css('left',0).css('top',0);
        
        $(".tooltip").html(output);
        
        update(event);
        $(".tooltip").css('width',$("#tooltip").width()+5);
        
        $(".tooltip").show();
        
        $(event.target).bind('mousemove', update);
    }    
    //$(this).unbind('mousemove')    
}
function ajaxError(XMLHttpRequest, textStatus, errorThrown)
{
    XMLHttpRequest.abort();
    console.log(textStatus);
    $("#tooltip").text(textStatus + "\n");
    //console.log(errorThrown);
}



function resettip(hastip)
{
    //console.log("Tip gone"+tipup);
    tipup = -1;
    $(hastip).unbind('mousemove', update);
    $("#tooltip").hide();
    $("#tooltip").text('');
    $(".tooltip").css('width','auto');
    
    
}

function hidetip(event)
{
    resettip(event.target);
}


function showtip(event)
{
    /*if(tipup != -1)
    {
        
    }*/
    resettip($(".hastip").get(tipup));
    tipup = $(event.target).data('num');
    $(event.target).bind("mouseleave",hidetip );
    
    if($("input[name=showtooltips]").attr('checked') === false)
    {
        return;
    }
    //$("#tooltip").load("../cgi-bin/tooltip?"+($(event.target).attr('href').split("?"))[1],"",makevisible);
    
    
    $.ajax({
            type: "GET",
            url: "../cgi-bin/tooltip?"+($(event.target).attr('href').split("?"))[1],
            dataType: "html",
            success: function(output){showInfo(event,output);},
            error: ajaxError,
            cache: true
    });
    
    
    
}
$(document).ready(function() 
{
    //NEWVERSION
    $("area[href^='../cgi-bin/hgc']").addClass('hastip');
    
    $(".hastip").each(function(i)
    {
        $(this).data('title', $(this).attr('title'));
        $(this).data('num',i);
        $(this).removeAttr("title");
    });
    $("body").append("<div class='tooltip' id='tooltip'></div>");
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
    $("#tooltiptext").css({
    "font-size": "small",
    "opacity" : "1"});
    $("#trackMap").removeAttr("title");
    $(".hastip").bind("mouseenter", showtip);
    
    $("input[name=hgt.toggleRevCmplDisp]").after('  <input type="submit" class="setwidth" value="auto-set width" onclick="setwidth()"/>');
    
    
    /*$("#map").prev().before('<input type="checkbox" name="showtooltips" onclick="changetitles()">Show tooltips</input>'+
    '<br>'+
    '<input type="submit" class="presetbutton" value="basic tracks" onclick="basictracks()"/>'+
    '<input type="submit" class="presetbutton" value="RNA tracks" onclick="rnatracks()"/>'+
    '<input type="submit" class="presetbutton" value="coding gene tracks" onclick="genepredtracks()"/>'+
    '<input type="submit" class="presetbutton" value="comparative genomics tracks" onclick="compgenotracks()"/>'+
    '<input type="submit" class="presetbutton" value="gene regulation tracks" onclick="regulationtracks()"/>');*/
    $('input[name=showtooltips]').attr('checked', true);
    
    
});