// Javascript for use in hgTracks CGI
// $Header: /projects/compbio/cvsroot/kent/src/hg/js/hgTracks.js,v 1.30 2009/07/10 19:49:58 tdreszer Exp $

var debug = false;
var originalPosition;
var originalSize;
var clickClipHeight;
var startDragZoom = null;
//var mapHtml;
var newWinWidth;
var imageV2 = false;
var imgBoxPortal = false;
var blockUseMap = false;

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
    startDragZoom = now.getTime();
    blockUseMap = true;
    // vvvvvvv Should be obsolete since maps items are ignored when startDragZoom is set
//    if(imageV2 == false) {
//        jQuery.each(jQuery.browser, function(i, val) {
//            if(i=="msie" && val) {
//                // Very hacky way to solve following probem specif to IE:
//                // If the user ends selection with the mouse in a map box item, the map item
//                // is choosen instead of the selection; to fix this, we remove map box items
//                // during the mouse selection process.
//                mapHtml = $('#map').html();
//                $('#map').empty();
//            }
//        });
//    }
    // ^^^^^^^^ Should be obsolete since maps items are ignored when startDragZoom is set
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
        if(newStart < 1) {
            newStart = 1;
            newEnd = newWinWidth;
        } else {
            // hgTracks gracefully handles overflow past the end of the chrom, so don't worry about that.
            newEnd = (winStart + 1) + (startDelta + Math.floor(newWinWidth / 2));
        }
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
       doIt = startDragZoom != null || selection.y1 <= clickClipHeight;
    }
    if(doIt) {
        // startDragZoom is null if mouse has never been moved
	if(updatePosition(img, selection, (selection.x2 == selection.x1) || startDragZoom == null || (now.getTime() - startDragZoom) < 100)) {
	    document.TrackHeaderForm.submit();
	}
    } else {
        setPosition(originalPosition, originalSize);
        originalPosition = originalSize = null;
//        if(mapHtml) {
//            $('#map').append(mapHtml);
//        }
    }
//    mapHtml = null;
    startDragZoom = null;
    setTimeout('blockUseMap=false;',50); // Necessary incase the selectEnd was over a map item. select takes precedence.
    return true;
}

$(window).load(function () {
        // jQuery load function with stuff to support drag selection in track img
	var rulerEle = document.getElementById("hgt.rulerClickHeight");
	var dragSelectionEle = document.getElementById("hgt.dragSelection");
	// disable if ruler is not visible.
	if((dragSelectionEle != null) && (dragSelectionEle.value == '1') && (rulerEle != null)) {
        var imgHeight = 0;
        var imgWidth  = 0;
        var img = $('#img_data_ruler');
        if(img==undefined || img.length == 0) {  // Revert to old imageV1
            img = $('#trackMap');
            imgHeight = jQuery(img).height();
            imgWidth  = jQuery(img).width();
        } else {
            imageV2   = true;
            imgHeight = $('#imgTbl').height();
            imgWidth  =  $('#td_data_ruler').width();
        }
        clickClipHeight = parseInt(rulerEle.value);
                newWinWidth = parseInt(document.getElementById("hgt.newWinWidth").value);

        img.imgAreaSelect({ selectionColor: 'blue', outerColor: '',
            minHeight: imgHeight, maxHeight: imgHeight,
            onSelectStart: selectStart, onSelectChange: selectChange, onSelectEnd: selectEnd,
            autoHide: true, movable: false,
            clickClipHeight: clickClipHeight});
    }
});

function toggleTrackGroupVisibility(button, prefix)
{
// toggle visibility of a track group; prefix is the prefix of all the id's of tr's in the
// relevant group. This code also modifies the corresponding hidden fields and the gif of the +/- img tag.
    var retval = true;
    var hidden = $("input[name='hgtgroup_"+prefix+"_close']")
    var newVal=1; // we're going - => +
    if($(button) != undefined && $(hidden) != undefined && $(hidden).length > 0) {
        var oldSrc = $(button).attr("src");
        if(arguments.length > 2)
            newVal = arguments[2] ? 0 : 1;
        else
            newVal = oldSrc.indexOf("/remove") > 0 ? 1 : 0;

        var newSrc;
        if(newVal == 1) {
            newSrc = oldSrc.replace("/remove", "/add");
            $("tr[id^='"+prefix+"-']").hide();
        } else {
            newSrc = oldSrc.replace("/add", "/remove");
            $("tr[id^='"+prefix+"-']").show();
        }
        $(button).attr("src",newSrc);
        $(hidden).val(newVal);
        retval = false;
    }
    return retval;
}

function setAllTrackGroupVisibility(newState)
{
// Set visibility of all track groups to newState (true means expanded).
// This code also modifies the corresponding hidden fields and the gif's of the +/- img tag.
    $("img[id$='_button']").each( function (i) {
        if(this.src.indexOf("/remove") > 0 || this.src.indexOf("/add") > 0)
            toggleTrackGroupVisibility(this,this.id.substring(0,this.id.length - 7),newState); // clip '_button' suffix
    });
    return false;
}

function imgTblSetOrder(table)
{
// Sets the 'order' value for the image table after a drag reorder
    $("input[name$='_imgOrd']").each(function (i) {
        var tr = $(this).parents('tr');
        if($(this).val() != $(tr).attr('rowIndex')) {
            //alert('Reordered '+$(this).val() + " to "+$(tr).attr('rowIndex'));
            $(this).val($(tr).attr('rowIndex'));
        }
    });
}

/////////////////////////////////////////////////////
jQuery.fn.panImages = function(imgOffset,imgBoxLeftOffset){
this.each(function(){

    var pic;
    var pan;

    if ( $(this).is("img") ) {
        pan = $(this).parent("div");
        pic = $(this);
    }
    else if ( $(this).is("div.scroller")  ) {
        pan = $(this);
        pic = $(this).children("img#panImg"); // Get the real pic
    }

    if(pan == undefined || pic == undefined) {
        throw "Not a div with a child image! 'panImages' can only be used with divs contain images.";
    }

    var leftLimit   = imgBoxLeftOffset*-1; // This hides the leftLabel if the image contains it
    var prevX       = imgOffset*-1;
    var newX        = 0;
    var mouseDownX  = 0;
    var mouseIsDown = false;

    initialize();

    function initialize(){

        pan.css( 'cursor', 'w-resize');//'move');

        pan.mousedown(function(e){
            mouseIsDown = true;

            mouseDownX = e.clientX;
            $(document).bind( 'mouseup',  panMouseUp);  // Will exec only once
            $(document).bind('mousemove',panner)
            return false;
        });
    }

    function panner(e) {
        if ( mouseIsDown ) {
            var relativeX = (e.clientX - mouseDownX);

            if(relativeX != 0) {
                blockUseMap = true;
                // Remeber that offsetX (prevX) is negative
                if ( (prevX + relativeX) >= leftLimit ) { // scrolled all the way to the left
                    newX = leftLimit;
                } else if ( (prevX + relativeX) < (imgBoxPortalWidth - imgBoxWidth + leftLimit) ) { // scrolled all the way to the left
                    newX = (imgBoxPortalWidth - imgBoxWidth + leftLimit);
                } else
                    newX = prevX + relativeX;

                $(".panImg").css( {'left': newX.toString() + "px" });
                panUpdatePosition(newX);
            }
        }
    }
    function panMouseUp(e) {  // Must be a separate function instead of pan.mouseup event.
        if(mouseIsDown) {
            panner(e);

            prevX = newX;
            $(document).unbind('mousemove',panner);
            $(document).unbind('mouseup',panMouseUp);
            mouseIsDown = false;
            setTimeout('blockUseMap=false;',50); // Necessary incase the selectEnd was over a map item. select takes precedence.
        }
    }
    function panUpdatePosition(newOffsetX)
    {
        // Updates the 'position/search" display with change due to panning
        var portalWidthBases = imgBoxPortalEnd - imgBoxPortalStart;
        var portalScrolledX  = (imgBoxPortalOffsetX+imgBoxLeftLabel) + newOffsetX;

        var newPortalStart = imgBoxPortalStart - Math.round(portalScrolledX*imgBoxBasesPerPixel); // As offset goes down, bases seen goes up!
        if( newPortalStart < imgBoxChromStart)     // Stay within bounds
            newPortalStart = imgBoxChromStart;
        var newPortalEnd = newPortalStart + portalWidthBases;
        if( newPortalEnd > imgBoxChromEnd) {
            newPortalEnd = imgBoxChromEnd;
            newPortalStart = newPortalEnd - portalWidthBases;
        }
        if(newPortalStart > 0) {
            var newPos = document.getElementById("hgt.chromName").value + ":" + commify(newPortalStart) + "-" + commify(newPortalEnd);
            setPosition(newPos, (newPortalEnd - newPortalStart + 1));
        }
        return true;
    }
});

};

/////////////////////////////////////////////////////

function blockTheMap(e)
{
    blockUseMap=true;
}

$(document).ready(function()
{
    // Convert map AREA gets to post the form, ensuring that cart variables are kept up to date
    $('a,area').not("[href*='#']").filter("[target='']").click(function(i) {
        if(blockUseMap==true) {
            //alert("blockUseMap");
            return false;
        }
        var thisForm=$(this).parents('form');
        if(thisForm == undefined || $(thisForm).length == 0)
            thisForm=$("FORM");
        if($(thisForm).length > 1)
            thisForm=$(thisForm)[0];
        if(thisForm != undefined && $(thisForm).length == 1) {
            //alert("posting form:"+$(thisForm).attr('name'));
            return postTheForm($(thisForm).attr('name'),this.href);
        }

        return true;
    });
    if($('#imgTbl').length == 1) {
        imageV2   = true;
        // Make imgTbl allow draw reorder of imgTrack rows
        if($(".tableWithDragAndDrop").length > 0) {
            $(".tableWithDragAndDrop").tableDnD({
                onDragClass: "trDrag",
                dragHandle: "dragHandle",
                onDragStart: function(table, row) {
                    $(document).bind('mousemove',blockTheMap);
                },
                onDrop: function(table, row) {
                        if(imgTblSetOrder) { imgTblSetOrder(table); }
                        $(document).unbind('mousemove',blockTheMap);
                        setTimeout('blockUseMap=false;',50); // Necessary incase the selectEnd was over a map item. select takes precedence.
                    }
                });
        }
        if(imgBoxPortal) {
            //alert("imgBox("+imgBoxChromStart+"-"+imgBoxChromEnd+","+imgBoxWidth+") bases/pix:"+imgBoxBasesPerPixel+"\nportal("+imgBoxPortalStart+"-"+imgBoxPortalEnd+","+imgBoxPortalWidth+") offset:"+imgBoxPortalOffsetX);

            // Turn on drag scrolling.
            $("div.scroller").panImages(imgBoxPortalOffsetX,imgBoxLeftLabel);
        }
        // Temporary warning while new imageV2 code is being worked through
        if($('#map').children("AREA").length > 0) {
            alert('Using imageV2, but old map is not empty!');
        }
    }
});
