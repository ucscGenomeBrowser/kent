// Javascript for use in hgTracks CGI

var debug = false;
var originalPosition;
var originalSize;
var clickClipHeight;
var start;
var mapHtml;
var newWinWidth;

function commify (str) {
    if(typeof(str) == "number")
	str = str + "";
    var n = str.length;
    if (n <= 3) {
	return str;
    } else {
	var pre = str.substring(0, n-3);
	var post = str.substring(n-3);
	var pre = commify(pre);
	return pre + "," + post;
    }
}

function initVars(img)
{
// There are various entry points, so we call initVars in several places to make sure this variables get updated.
    if(!originalPosition) {
        // remember initial position and size so we can restore it if user cancels
        originalPosition = $('#positionHidden').val();
        originalSize = $('#size').text();
    }
}

function selectStart(img, selection)
{
    initVars();
    var now = new Date();
    start = now.getTime();
    jQuery.each(jQuery.browser, function(i, val) {
        if(i=="msie" && val) {
            // Very hacky way to solve following probem specif to IE:
            // If the user ends selection with the mouse in a map box item, the map item
            // is choosen instead of the selection; to fix this, we remove map box items
            // during the mouse selection process.
            mapHtml = $('#map').html();
            $('#map').empty();
        }
    });
}

function setPosition(position, size)
{
// Set value of position and size (in hiddens and input elements).
// We assume size has already been commified.
// Either position or size may be null.
    if(position) {
        // XXXX There are multiple tags with name == "position":^(
	var tags = document.getElementsByName("position");
	for (var i = 0; i < tags.length; i++) {
	    var ele = tags[i];
	    ele.value = position;
	}
    }
    if(size) {
        $('#size').text(size);
    }
}

function updatePosition(img, selection, singleClick)
{
    // singleClick is true when the mouse hasn't moved (or has only moved a small amount).
    var insideX = parseInt(document.getElementById("hgt.insideX").value);
    var revCmplDisp = parseInt(document.getElementById("hgt.revCmplDisp").value) == 0 ? false : true;
    var chromName = document.getElementById("hgt.chromName").value;
    var winStart = parseInt(document.getElementById("hgt.winStart").value);
    var winEnd = parseInt(document.getElementById("hgt.winEnd").value);
    var imgWidth = jQuery(img).width() - insideX;

    var width = winEnd - winStart;
    var newPos = null;
    var newSize = null;
    var mult = width / imgWidth;			    // mult is bp/pixel multiplier
    var startDelta;                                     // startDelta is how many bp's to the right/left
    if(revCmplDisp) {
        var x1 = Math.min(imgWidth, selection.x1);
        startDelta = Math.floor(mult * (imgWidth - x1));
    } else {
        var x1 = Math.max(insideX, selection.x1);
        startDelta = Math.floor(mult * (x1 - insideX));
    }
    if(singleClick) {
	var newStart = (winStart + 1) + (startDelta - Math.floor(newWinWidth / 2));
	var newEnd = (winStart + 1) + (startDelta + Math.floor(newWinWidth / 2));
	newPos = chromName + ":" + newStart + "-" + newEnd;
	newSize = newEnd - newStart + 1;
    } else {
        var endDelta;
        if(revCmplDisp) {
            endDelta = startDelta;
            var x2 = Math.min(imgWidth, selection.x2);
	    startDelta = Math.floor(mult * (imgWidth - x2));
        } else {
            var x2 = Math.max(insideX, selection.x2);
	    endDelta = Math.floor(mult * (x2 - insideX));
        }
        var newStart = winStart + 1 + startDelta;
        var newEnd = winStart + 1 + endDelta;
        if(newEnd > winEnd) {
            newEnd = winEnd;
        }
        newPos = chromName + ":" + newStart + "-" + newEnd;
        newSize = newEnd - newStart + 1;
    }

    if(newPos != null) {
        setPosition(newPos, commify(newSize));
	return true;
    }
}

function selectChange(img, selection)
{
    initVars();
    updatePosition(img, selection, false);
    return true;
}

function selectEnd(img, selection)
{
    var imgWidth = jQuery(img).width();
    var imgHeight = jQuery(img).height();
    var imgOfs = jQuery(img).offset();
    // ignore releases outside of the image rectangle (allowing a 10 pixel slop)
    var slop = 10;
    var now = new Date();
    var doIt = false;
    if((selection.event.pageX >= (imgOfs.left - slop)) && (selection.event.pageX < (imgOfs.left + imgWidth + slop))
       && (selection.event.pageY >= (imgOfs.top - slop)) && (selection.event.pageY < (imgOfs.top + imgHeight + slop))) {
       // ignore single clicks that aren't in the top of the image (this happens b/c the clickClipHeight test in selectStart
       // doesn't occur when the user single clicks).
       doIt = start != null || selection.y1 <= clickClipHeight;
    }
    if(doIt) {
        // start is null if mouse has never been moved
	if(updatePosition(img, selection, (selection.x2 == selection.x1) || start == null || (now.getTime() - start) < 100)) {
	    document.TrackHeaderForm.submit();
	}
    } else {
        setPosition(originalPosition, originalSize);
        originalPosition = originalSize = null;
        if(mapHtml) {
            $('#map').append(mapHtml);
        }
    }
    mapHtml = start = null;
    return true;
}

$(window).load(function () {
        // jQuery load function with stuff to support drag selection in track img
	var rulerEle = document.getElementById("hgt.rulerClickHeight");
	var dragSelectionEle = document.getElementById("hgt.dragSelection");
	// disable if ruler is not visible.
	if((dragSelectionEle != null) && (dragSelectionEle.value == '1') && (rulerEle != null)) {
		var img = $('#trackMap');
		var imgHeight = jQuery(img).height();
		var imgWidth = jQuery(img).width();
		var imgOfs = jQuery(img).offset();
		clickClipHeight = parseInt(rulerEle.value);
                newWinWidth = parseInt(document.getElementById("hgt.newWinWidth").value);

		img.imgAreaSelect({ selectionColor: 'blue', outerColor: '',
			minHeight: imgHeight, maxHeight: imgHeight,
			onSelectStart: selectStart, onSelectChange: selectChange, onSelectEnd: selectEnd,
			autoHide: true, movable: false,
			clickClipHeight: clickClipHeight});
	}
});

function toggleTrackGroupVisibility(obj, prefix)
{
// toggle visibility of a track group; prefix is the prefix of all the id's of tr's in the
// relevant group. This code also modifies the corresponding hidden fields and the gif of the +/- img tag.
    var retval = true;
    if (document.getElementsByTagName){
	// Tested in FireFox 1.5 & 2, IE 6 & 7, and Safari
	var button = document.getElementById(prefix + "_button");
	var hidden1 = document.getElementById("hgtgroup_" + prefix + "_close_1");
	var hidden2 = document.getElementById("hgtgroup_" + prefix + "_close_2");

	// Hidding tr's works diff b/n IE and FF: http://snook.ca/archives/html_and_css/dynamically_sho/
	// This browser sniffing is very kludgy; s/d find a better way to do determine IE vs. other browsers.
	// Currently we are going to see if just '' will work in all browsers (which means we can
	// avoid this browser sniffing).
	// var canSee = (navigator.userAgent.indexOf("MSIE") > -1) ? 'block' : 'table-row';
	var canSee = '';

	var src = button.src;
	if(button && hidden1 && hidden2 && src) {
	    var newSrc = src.replace("/remove", "/add");
	    var newVal;
	    if(newSrc == src) {
		// we're going + => -
		newVal = '0';
		hidden2.value = '0';
		newSrc = src.replace("/add", "/remove");
	    } else {
		newVal = '1';
	    }
	    hidden1.value = newVal;
	    hidden2.value = newVal;
	    button.src = newSrc;
	}

	// Send a message to hgTracks to record every user choice, so
	// they get stored into their cart, even if the user then navigates with a link.
	// XXXX Currently dead code
	// setCartVar("hgtgroup_" + prefix + "_close", newVal);

	var list = document.getElementsByTagName('tr');
	for (var i=0;i<list.length;i++) {
	    var ele = list[i];
	    // arbitrary numbers are used to make id's unique (e.g. "map-1"), so look for prefix + "-"
	    if(ele.id.indexOf(prefix + "-") == 0) {
		var styleObj = ele.style;
		if(styleObj.display == 'none') {
		    styleObj.display = canSee;
		} else {
		    styleObj.display = 'none';
		}
	    }
	}
	retval = false;
    } else if (document.all){
	// IE 4.X/5.0 - I don't have access to a browser to get this to work
	if(debug)
	    alert("toggleTrackGroupVisibility is unimplemented in this browser");
	if(0) {
	    // core of code that could perhaps work.
	    // Loop looking for the appropriate style object
	    var styleObj = getStyle("#" + id);
	    if(styleObj.display == 'none'){
		styleObj.display = '';
		obj.innerText = "hide answer";
	    } else {
		styleObj.display = 'none';
		obj.innerText = "show answer";
	    }
	}
    } else {
	// NS 4.x - I gave up trying to get this to work.
	if(debug)
	    alert("toggleTrackGroupVisibility is unimplemented in this browser");
    }
    return retval;
}

function setAllTrackGroupVisibility(newState)
{
// Set visibility of all track groups to newState (true means expanded).
// This code also modifies the corresponding hidden fields and the gif's of the +/- img tag.
// See toggleTrackGroupVisibility for very similar code.
    var retval = true;
    if (document.getElementsByTagName){
	// Tested in FireFox 1.5 & 2, IE 6 & 7, and Safari
	var inputList = document.getElementsByTagName('input');
        var trList = document.getElementsByTagName('tr');
        var sharedPrefix = "hgtgroup_";
	for (var i=0;i<inputList.length;i++) {
	    var ele = inputList[i];
            var start = ele.id.indexOf(sharedPrefix);
            var end = ele.id.indexOf("_close");
	    if(start == 0 && end > 0) {
                var prefix = ele.id.substring(start + sharedPrefix.length, end);
                var newVal = newState ? 0 : 1;
	        var button = document.getElementById(prefix + "_button");
	        var hidden1 = document.getElementById("hgtgroup_" + prefix + "_close_1");
	        var hidden2 = document.getElementById("hgtgroup_" + prefix + "_close_2");
	        var canSee = '';

	        var src = button.src;
	        if(button && hidden1 && hidden2 && src) {
                    var newSrc;
                    if(newState) {
	                newSrc = src.replace("/add", "/remove");
                    } else {
	                newSrc = src.replace("/remove", "/add");
                    }
	            hidden1.value = newVal;
	            hidden2.value = newVal;
	            button.src = newSrc;
	        }

	        // Send a message to hgTracks to record every user choice, so
	        // they get stored into their cart, even if the user then navigates with a link.
	        // XXXX Currently dead code
	        // setCartVar("hgtgroup_" + prefix + "_close", newVal);

	        for (var j=0;j<trList.length;j++) {
	            var ele = trList[j];
	            // arbitrary numbers are used to make id's unique (e.g. "map-1"), so look for prefix + "-"
	            if(ele.id.indexOf(prefix + "-") == 0) {
		        var styleObj = ele.style;
                        styleObj.display = newState ? canSee : 'none';
	            }
	        }
            }
        }
	retval = false;
    } else {
	// IE 4.X/5.0, NS 4.x - I gave up trying to get this to work.
	if(debug)
	    alert("toggleTrackGroupVisibility is unimplemented in this browser");
    }
    return retval;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * http://www.gnu.org/licenses/gpl.txt
 *
 *
 * Creates a image like div with the image as background with ability to
 * pan it with mouse.
 *
 * @name        panFullSize
 * @author      Esa-Matti Suuronen
 * @contact     esa-matti [a] suuronen [dot] org
 * @license     GPL
 * @version     1.02
 *
 *
 *
 * Takes only img-elements (and custom pan-divs). Otherwise exception is thrown.
 *
 * Example: $("img#mypic").panFullSize(300,200);
 *
 *
 *
 */
jQuery.fn.panFullSize = function(x, y){
this.each(function(){

    var pic;
    var pic_real_width;
    var pic_real_height;
    var picX_start = 0;
    var picY_start = 0;
    var prevX = 0;
    var prevY = 0;
    var newX = 0;
    var newY = 0;
    var mousedown = false;

    //alert("panFullSize: "+ $(this).attr("id"));

    if ( $(this).is("img") ) {
        pic = $(this);
    }
    else if ( $(this).is("div.panFullSize")  ) { // from custom pan-div
        pic = $(this).prev("img"); // Get the real pic
    }
    else {
        throw "Not an image! panFullSize can only be used with images.";
    }

    var pan = getPan();
    //var exists = pan.length > 0 ;
    var exists = pan.is("*");

    if (exists) {
        if (!x) x = pan.width()
        if (!y) y = pan.height();
    }
    else {
        // Defaults from img-element
        if (!x) x = pic.width()
        if (!y) y = pic.height();
    }

    var box_width = x;
    var box_height = y;

    if ( !exists ) {
        // new pan-div
        // The space in div is required
        pic.after('<div id="pan' + pic.attr("id") +'" class="panFullSize"> </div>');
        pan = getPan().hide();
        // On creating pan-div, we need to wait for the image to load before we can initialize pan-div.
        // Otherwise getting the real image width&heigth fails...
        pic.load(initialize);
    }
    else {
        initialize();
    }
    center();
    show();

    function initialize(){

        pic.hide();

        pan.css( 'width', box_width + "px").css( 'height', box_height + "px" );
        pan.css( 'display', 'inline-block');  // Make div to act like img. inline-block does not work in FF2?
        // It's only needed to add size-attributes if we already have pan-div
        if ( exists ) return;

        //pan.css( 'background-color', "red" ); // For debugging. Should not be seen ever.
        pan.css( 'background-image', 'url("' + pic.attr("src") + '")' );
        pan.css( 'background-repeat', "no-repeat" );

        // Get the real size of the image
        var pic_orig_width = pic.width();
        var pic_orig_height = pic.height();
        pic.removeAttr("width");
        pic.removeAttr("height");
        pic_real_width = pic.width();
        pic_real_height = pic.height();
        pic.width(pic_orig_width).height(pic_orig_height);
        //pan.css( 'overflow', "scroll" );
        //pan.css( 'height', pic.height() + 10 );
        // Doesn't do any good here because the img is background

        pan.mousedown(function(e){
            //alert("Mousedown");
            mousedown = true;

            box_width = pan.width();
            box_height = pan.height();

            picX_start = e.clientX;
            picY_start = e.clientY;
        });

        $(document).mousemove(onpan);

        $(document).mouseup(function(e){
            onpan(e);
            mousedown = false;

            prevX = newX;
            prevY = newY;

        });

    }



    /**
     * BUG: Gets stuck in corners?
     */
    function onpan(e){


        var diffX = e.clientX - picX_start;
        var diffY = e.clientY - picY_start;

         if ( mousedown ){

          var in_areaX = true;
          var in_areaY = true;

          if ( prevX + diffX >= 0 )
              in_areaX = false;
          if ( -(prevX + diffX) > pic_real_width - box_width )
              in_areaX = false;
          if (in_areaX)  newX = prevX + diffX;


          if ( prevY + diffY >= 0 )
              in_areaY = false;
          if ( -(prevY + diffY) > pic_real_height - box_height )
              in_areaY = false;
          if (in_areaY)  newY = prevY + diffY;


            pan.css( {backgroundPosition:  newX.toString() +"px " + newY.toString() + "px"} )


         }

     }

     function getPan(){
         return pic.next("div.panFullSize");
     }

     function center(){
        //alert("pan.center(img:"+$(this).attr("id")+") pic.width()"+pic.width()+ " box_width:"+box_width);
        if(pic.width() > box_width)
            newX = (box_width - pic.width())/2;
        if(pic.height() > box_height)
            newY = (box_height - pic.height())/2;
        if(newX!=prevX || newY!=prevY){
            //alert("pan.center(img:"+$(this).attr("id")+") pic.width()"+pic.width()+ " box_width:"+box_width+" newX:"+newX);
            pan.css( {backgroundPosition:  newX.toString() +"px " + newY.toString() + "px"} );
            prevX = newX;
            prevY = newY;
        }
    }

    function hide(){
        pan.css( {visibility: "hidden"} );
    }
    function show(){
        pan.css( {visibility: "visible"} );
        pic.css( {visibility: "visible"} );
    }
});

// Lets return the new pan-divs
return $(this).next("div.panFullSize");


};


/**
 * Restores normal image view
 */
jQuery.fn.normalView = function(){
 this.each(function(){
    if ( $(this).is("div.panFullSize") ){
        $(this).hide();
        $(this).prev("img").show();

    }
    else if ( $(this).is("img") && $(this).next("div.panFullSize").is("*") ) {
        $(this).show();
        $(this).next("div.panFullSize").hide();

    }

 });


if ( $(this).is("div.panFullSize")  )
    return $(this).prev("img");
return $(this);

};

jQuery.fn.isPan = function(){
    return $(this).is("div.panFullSize");
};


$(document).ready( function() {
    //$("img#trackMap").panFullSize($("img#trackMap").width()/2, $("img#trackMap").height());
    $("img#chrom").panFullSize($("img#chrom").width()/2, $("img#chrom").height());

});
