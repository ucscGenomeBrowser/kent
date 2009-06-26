// Javascript for use in hgTracks CGI
// $Header: /projects/compbio/cvsroot/kent/src/hg/js/hgTracks.js,v 1.27 2009/06/26 20:53:18 tdreszer Exp $

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

$(document).ready(function()
{
    // Convert map AREA gets to post the form, ensuring that cart variables are kept up to date
    $('a,area').not("[href*='#']").filter("[target='']").click(function(i) {
        var thisForm=$(this).parents('form');
        if(thisForm != undefined && thisForm.length == 1)
            return postTheForm($(thisForm).attr('name'),this.href);

        return true;
    });
    if($('table#imgTbl').length == 1) {
        // Make imgTbl allow draw reorder of imgTrack rows
        if($(".tableWithDragAndDrop").length > 0) {
            $(".tableWithDragAndDrop").tableDnD({
                onDragClass: "trDrag",
                dragHandle: "dragHandle"
                });
        }

        // Turn on drag scrolling.
        //$(".panDivScroller").panImages($(".panDivScroller").width(),0,0);

        // Temporary warning while new imageV2 code is being worked through
        if($('map#map').children().length > 0) {
            alert('Using imageV2, but old map is not empty!');
        }
    }
});
