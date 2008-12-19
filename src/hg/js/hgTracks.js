// Javascript for use in hgTracks CGI

var debug = false;
var originalPosition;
var originalSize;

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

function selectStart(img, selection)
{
    // remember initial position and size so we can restore it if user cancels
    originalPosition = $('#positionHidden').val();
    originalSize = $('#size').text();
}

function updatePosition(img, selection, singleClick)
{
    // singleClick is true when the mouse hasn't moved (or has only moved a small amount).
    var insideX = parseInt(document.getElementById("hgt.insideX").value);
    var newWinWidth = parseInt(document.getElementById("hgt.newWinWidth").value);
    var chromName = document.getElementById("hgt.chromName").value;
    var winStart = parseInt(document.getElementById("hgt.winStart").value);
    var winEnd = parseInt(document.getElementById("hgt.winEnd").value);

    var update = false;
    if(selection.x1 < insideX) {
	selection.x1 = insideX;
	update = true;
    }
    if(selection.x2 < insideX) {
	selection.x2 = insideX;
	update = true;
    }
    if(update) {
	// Force update of selection box
	jQuery(img).imgAreaSelect({x1: selection.x1,x2: selection.x2,y1: selection.y1,y2: selection.y2});
    }

    var width = winEnd - winStart;
    var imgWidth = jQuery(img).width() - insideX;
    var newPos = null;
    var newSize = null;

    var mult = width / imgWidth;			    // mult is bp/pixel multiplier
    var x1 = Math.max(insideX, selection.x1);
    var startDelta = parseInt(mult * (x1 - insideX));       // startDelta is how many bp's to the right
    if(singleClick) {
	var newStart = winStart + startDelta - Math.floor(newWinWidth / 2);
	var newEnd = winStart + startDelta + Math.floor(newWinWidth / 2);
	newPos = chromName + ":" + newStart + "-" + newEnd;
	newSize = newEnd - newStart + 1;
    } else {
	var x2 = Math.max(insideX, selection.x2);
	var endDelta = parseInt(mult * (x2 - insideX));
	newPos = chromName + ":" + (winStart + startDelta) + "-" + (winStart + endDelta);
	newSize = endDelta - startDelta + 1;
    }

    if(newPos != null) {
	// XXXX There are multiple tags with name == "position":^(
	// The jQuery syntax for this is noticeably slow (don't know why).
	// 	$("input[name='position']").value = newPos;
	var tags = document.getElementsByTagName("input");
	for (var i = 0; i < tags.length; i++) { 
	    var ele = tags[i];
	    if(ele.name == "position") {
		ele.value = newPos;
	    }
	}
	$('#size').text(commify(newSize));
	return true;
    }
}

function selectChange(img, selection)
{
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
    if((selection.event.pageX >= (imgOfs.left - slop)) && (selection.event.pageX < (imgOfs.left + imgWidth + slop)) 
       && (selection.event.pageY >= (imgOfs.top - slop)) && (selection.event.pageY < (imgOfs.top + imgHeight + slop))) {
	if(updatePosition(img, selection, selection.x2 <= (selection.x1 + 10))) {
	    document.TrackHeaderForm.submit();
	}
    } else {
        if(originalPosition) {
	    var tags = document.getElementsByTagName("input");
	    for (var i = 0; i < tags.length; i++) { 
	        var ele = tags[i];
	        if(ele.name == "position") {
		    ele.value = originalPosition;
	        }
	    }
        }
        if(originalSize) {
            $('#size').text(originalSize);
        }
    }
    return true;
}

$(window).load(function () {
        // jscript load function with stuff to support drag selection in track img
	var rulerEle = document.getElementById("hgt.rulerClickHeight");
	var dragSelectionEle = document.getElementById("hgt.dragSelection");
	// disable if ruler is not visible.
	if((dragSelectionEle != null) && dragSelectionEle.value && (rulerEle != null)) {
		var img = $('#trackMap');
		var imgHeight = jQuery(img).height();
		var imgWidth = jQuery(img).width();
		var imgOfs = jQuery(img).offset();
		var clickClipHeight = parseInt(rulerEle.value);

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
