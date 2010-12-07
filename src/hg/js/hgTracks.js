// Javascript for use in hgTracks CGI
// $Header: /projects/compbio/cvsroot/kent/src/hg/js/hgTracks.js,v 1.69 2010/06/11 18:14:23 larrym Exp $

var debug = false;
var originalPosition;
var originalSize;
var originalCursor;
var originalMouseOffset = {x:0, y:0};
var clickClipHeight;
var revCmplDisp;
var insideX;
var startDragZoom = null;
var newWinWidth;
var imageV2 = false;
var imgBoxPortal = false;
var blockUseMap = false;
var mapItems;
var trackImg;               // jQuery element for the track image
var trackImgTbl;            // jQuery element used for image table under imageV2
var imgAreaSelect;          // jQuery element used for imgAreaSelect
var originalImgTitle;
var autoHideSetting = true; // Current state of imgAreaSelect autoHide setting
var selectedMenuItem;       // currently choosen context menu item (via context menu).
var browser;                // browser ("msie", "safari" etc.)
var mapIsUpdateable = true;
var currentMapItem;
var visibilityStrsOrder = new Array("hide", "dense", "full", "pack", "squish");     // map browser numeric visibility codes to strings

function initVars(img)
{
// There are various entry points, so we call initVars in several places to make sure this variables get updated.
    if(!originalPosition) {
        // remember initial position and size so we can restore it if user cancels
        originalPosition = getOriginalPosition();
        originalSize = $('#size').text();
        originalCursor = jQuery('body').css('cursor');
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

function setPositionByCoordinates(chrom, start, end)
{
    var newPosition = chrom + ":" + commify(start) + "-" + commify(end);
    setPosition(newPosition, commify(end - start + 1));
    return newPosition;
}

function getPositionElement()
{
// Return position box object
    var tags = document.getElementsByName("position");
    // There are multiple tags with name == "position" (the visible position text input
    // and a hidden with id='positionHidden'); we return value of visible element.
    for (var i = 0; i < tags.length; i++) {
	    var ele = tags[i];
            if(ele.id != "positionHidden") {
	        return ele;
            }
    }
    return null;
}

function getPosition()
{
// Return current value of position box
    var ele = getPositionElement();
    if(ele != null) {
	return ele.value;
    }
    return null;
}

function getOriginalPosition()
{
    return originalPosition || getPosition();
}

function setPosition(position, size)
{
// Set value of position and size (in hiddens and input elements).
// We assume size has already been commified.
// Either position or size may be null.
    if(position) {
        // There are multiple tags with name == "position" (one in TrackHeaderForm and another in TrackForm).
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

function checkPosition(img, selection)
{
// return true if user's selection is still w/n the img (including some slop).
    var imgWidth = jQuery(img).width();
    var imgHeight = jQuery(img).height();
    var imgOfs = jQuery(img).offset();
    var slop = 10;

    // We ignore clicks in the gray tab and track title column (we really should suppress all drag activity there,
    // but I don't know how to do that with imgAreaSelect).
    var leftX = revCmplDisp ? imgOfs.left - slop : imgOfs.left + insideX - slop;
    var rightX = revCmplDisp ? imgOfs.left + imgWidth - insideX + slop : imgOfs.left + imgWidth + slop;

    return (selection.event.pageX >= leftX) && (selection.event.pageX < rightX)
        && (selection.event.pageY >= (imgOfs.top - slop)) && (selection.event.pageY < (imgOfs.top + imgHeight + slop));
}

function pixelsToBases(img, selStart, selEnd, winStart, winEnd)
// Convert image coordinates to chromosome coordinates
{
var insideX = parseInt(document.getElementById("hgt.insideX").value);
var imgWidth = jQuery(img).width() - insideX;
var width = winEnd - winStart;
var mult = width / imgWidth;   // mult is bp/pixel multiplier
var startDelta;                // startDelta is how many bp's to the right/left
if(revCmplDisp) {
    var x1 = Math.min(imgWidth, selStart);
    startDelta = Math.floor(mult * (imgWidth - x1));
} else {
    var x1 = Math.max(insideX, selStart);
    startDelta = Math.floor(mult * (x1 - insideX));
}
var endDelta;
if(revCmplDisp) {
    endDelta = startDelta;
    var x2 = Math.min(imgWidth, selEnd);
    startDelta = Math.floor(mult * (imgWidth - x2));
} else {
    var x2 = Math.max(insideX, selEnd);
    endDelta = Math.floor(mult * (x2 - insideX));
}
var newStart = winStart + startDelta;
var newEnd = winStart + 1 + endDelta;
if(newEnd > winEnd) {
    newEnd = winEnd;
}
return {chromStart : newStart, chromEnd : newEnd};
}

function selectionPixelsToBases(img, selection)
// Convert selection x1/x2 coordinates to chromStart/chromEnd.
{
var winStart = parseInt(document.getElementById("hgt.winStart").value);
var winEnd = parseInt(document.getElementById("hgt.winEnd").value);
return pixelsToBases(img, selection.x1, selection.x2, winStart, winEnd);
}

function updatePosition(img, selection, singleClick)
{
var chromName = document.getElementById("hgt.chromName").value;
var winStart = parseInt(document.getElementById("hgt.winStart").value);
var winEnd = parseInt(document.getElementById("hgt.winEnd").value);
var pos = pixelsToBases(img, selection.x1, selection.x2, winStart, winEnd);
if(typeof imgBoxPortalStart != "undefined" && imgBoxPortalStart) {
    winStart = imgBoxPortalStart;
    winEnd = imgBoxPortalEnd;
}
// singleClick is true when the mouse hasn't moved (or has only moved a small amount).
if(singleClick) {
    var center = (pos.chromStart + pos.chromEnd)/2;
    pos.chromStart = Math.floor(center - newWinWidth/2);
    pos.chromEnd = pos.chromStart + newWinWidth;
}
setPositionByCoordinates(chromName, pos.chromStart+1, pos.chromEnd);
return true;
}

function selectChange(img, selection)
{
    initVars();
    if(selection.x1 != selection.x2) { // TODO: Larry could you examine this?
        if(checkPosition(img, selection)) {
            updatePosition(img, selection, false);
            jQuery('body').css('cursor', originalCursor);
        } else {
            jQuery('body').css('cursor', 'not-allowed');
        }
    }
    return true;
}

function selectEnd(img, selection)
{
    var now = new Date();
    var doIt = false;
    if(originalCursor != null)
        jQuery('body').css('cursor', originalCursor);
    // ignore releases outside of the image rectangle (allowing a 10 pixel slop)
    if(autoHideSetting && checkPosition(img, selection)) {
       // ignore single clicks that aren't in the top of the image (this happens b/c the clickClipHeight test in selectStart
       // doesn't occur when the user single clicks).
       doIt = startDragZoom != null || selection.y1 <= clickClipHeight;
    }
    if(doIt) {
        // startDragZoom is null if mouse has never been moved
	if(updatePosition(img, selection, (selection.x2 == selection.x1) || startDragZoom == null || (now.getTime() - startDragZoom) < 100)) {
            jQuery('body').css('cursor', 'wait');
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
    jQuery.each(jQuery.browser, function(i, val) {
        if(val) {
            browser = i;
        }
        });
    // jQuery load function with stuff to support drag selection in track img
    if(browser == "safari" && navigator.userAgent.indexOf("Chrome") != -1) {
        // Handle the fact that (as of 1.3.1), jQuery.browser reports "safari" when the browser is in fact Chrome.
        browser = "chrome";
    }

    // Safari has the following bug: if we update the hgTracks map dynamically, the browser ignores the changes (even
    // though if you look in the DOM the changes are there); so we have to do a full form submission when the
    // user changes visibility settings or track configuration.
    //
    // Chrome used to have this problem too, but this  problem seems to have gone away as of
    // Chrome 5.0.335.1 (or possibly earlier).
    mapIsUpdateable = browser != "safari";
    loadImgAreaSelect(true);
    if($('#hgTrackUiDialog'))
        $('#hgTrackUiDialog').hide();

    // Don't load contextMenu if jquery.contextmenu.js hasn't been loaded
    if(trackImg && jQuery.fn.contextMenu) {
        $('#hgTrackUiDialog').hide();
        if(imageV2) {
            $("map[name!=ideoMap]").each( function(t) { parseMap($(this,false));});
        } else {
            // XXXX still under debate whether we have to remove the map
            parseMap($('#map'),true);
            $('#map').empty();
        }

        originalImgTitle = trackImg.attr("title");
        if(imageV2) {
            loadContextMenu(trackImgTbl);
            //$(".trDraggable,.nodrop").each( function(t) { loadContextMenu($(this)); });
            // FIXME: why isn't rightClick for sideLabel working??? Probably because there is no link!
        } else {
            loadContextMenu(trackImg);
            trackImg.mousemove(
                function (e) {
                    mapEvent(e);
                }
            );
            trackImg.mousedown(
                function (e) {
                    mapMouseDown(e);
                }
            );
        }
    }
});

function loadImgAreaSelect(firstTime)
{
    var rulerEle = document.getElementById("hgt.rulerClickHeight");
    var dragSelectionEle = document.getElementById("hgt.dragSelection");

    // disable if ruler is not visible.
    if((dragSelectionEle != null) && (dragSelectionEle.value == '1') && (rulerEle != null)) {
        var imgHeight = 0;
        trackImg = $('#img_data_ruler');

        if(trackImg == undefined || trackImg.length == 0) {  // Revert to old imageV1
            trackImg = $('#trackMap');
            imgHeight = jQuery(trackImg).height();
        } else {
            imageV2   = true;
            trackImgTbl = $('#imgTbl');
            imgHeight = trackImg.height();
            // XXXX Tim, I think we should get height from trackImgTbl, b/c it automatically adjusts as we add/delete items.
            imgHeight = trackImgTbl.height();
        }

        clickClipHeight = parseInt(rulerEle.value);
        newWinWidth = parseInt(document.getElementById("hgt.newWinWidth").value);
        revCmplDisp = parseInt(document.getElementById("hgt.revCmplDisp").value) == 0 ? false : true;
        insideX = parseInt(document.getElementById("hgt.insideX").value);

        imgAreaSelect = jQuery((trackImgTbl || trackImg).imgAreaSelect({ selectionColor: 'blue', outerColor: '',
            minHeight: imgHeight, maxHeight: imgHeight,
            onSelectStart: selectStart, onSelectChange: selectChange, onSelectEnd: selectEnd,
            autoHide: autoHideSetting, movable: false,
            clickClipHeight: clickClipHeight}));
    }
}

function makeItemsEnd(img, selection)
{
var image = $(img);
var imageId = image.attr('id');
var trackName = imageId.substring('img_data_'.length);
var chrom = document.getElementById("hgt.chromName").value;
var pos = selectionPixelsToBases(image, selection);
var command = document.getElementById('hgt_doJsCommand');
command.value = "makeItems " + trackName + " " + chrom + " " + pos.chromStart + " " + pos.chromEnd;
document.TrackHeaderForm.submit();
return true;
}

function setUpMakeItemsDrag(trackName)
{
// Set up so that they can drag out to define a new item on a makeItems track.
var img = $("#img_data_" + trackName);
if(img != undefined && img.length != 0) {
    var trackImgTbl = $('#imgTbl');
    var imgHeight = trackImgTbl.height();
    jQuery(img.imgAreaSelect( { selectionColor: 'green', outerColor: '',
	minHeight: imgHeight, maxHeight: imgHeight, onSelectEnd: makeItemsEnd,
	autoHide: true, movable: false}));
    }
}

function toggleTrackGroupVisibility(button, prefix)
{
// toggle visibility of a track group; prefix is the prefix of all the id's of tr's in the
// relevant group. This code also modifies the corresponding hidden fields and the gif of the +/- img tag.
    var retval = true;
    var hidden = $("input[name='hgtgroup_"+prefix+"_close']");
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
            $(button).attr('title','Expand this group');
            $("tr[id^='"+prefix+"-']").hide();
        } else {
            newSrc = oldSrc.replace("/add", "/remove");
            $(button).attr('title','Collapse this group');
            $("tr[id^='"+prefix+"-']").show();
        }
        $(button).attr("src",newSrc);
        $(hidden).val(newVal);
        // setCartVar("hgtgroup_" + prefix + "_close", newVal);
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

/////////////////////////////////////////////////////
// Chrom Drag/Zoom/Expand code
jQuery.fn.chromDrag = function(){
this.each(function(){
    // Plan:
    // mouseDown: determine where in map: convert to img location: pxDown
    // mouseMove: flag drag
    // mouseUp: if no drag, then create href centered on bpDown loc with current span
    //          if drag, then create href from bpDown to bpUp
    //          if ctrlKey then expand selection to containing cytoBand(s)
    var img = { top: -1, scrolledTop: -1, height: -1, left: -1, scrolledLeft: -1, width: -1 };  // Image dimensions all in pix
    var chr = { name: "", reverse: false, beg: -1, end: -1, size: -1, top: -1, bottom: -1, left: -1, right: -1, width: -1 };   // chrom Dimenaions beg,end,size in bases, rest in pix
    var pxDown = 0;     // pix X location of mouseDown
    var chrImg = $(this);
    var mouseIsDown   = false;
    var mouseHasMoved = false;
    var hilite = jQuery('<div></div>');

    initialize();

    function initialize(){

        findDimensions();

        if(chr.top == -1)
            warn("chromIdeo(): failed to register "+this.id);
        else {
            hiliteSetup();

            $('.cytoBand').mousedown( function(e)
            {   // mousedown on chrom portion of image only (map items)
                updateImgOffsets();
                pxDown = e.clientX - img.scrolledLeft;
                var pxY = e.clientY - img.scrolledTop;
                if(mouseIsDown == false
                && isWithin(chr.left,pxDown,chr.right) && isWithin(chr.top,pxY,chr.bottom)) {
                    mouseIsDown = true;
                    mouseHasMoved = false;

                    $(document).bind('mousemove',chromMove);
                    $(document).bind( 'mouseup', chromUp);
                    hiliteShow(pxDown,pxDown);
                    return false;
                }
            });
        }
    }

    function chromMove(e)
    {   // If mouse was down, determine if dragged, then show hilite
        if ( mouseIsDown ) {
            var pxX = e.clientX - img.scrolledLeft;
            var relativeX = (pxX - pxDown);
            if(mouseHasMoved || (mouseHasMoved == false && Math.abs(relativeX) > 2)) {
                mouseHasMoved = true;
                if(isWithin(chr.left,pxX,chr.right))
                    hiliteShow(pxDown,pxX);
                else if(pxX < chr.left)
                    hiliteShow(pxDown,chr.left);
                else
                    hiliteShow(pxDown,chr.right);
            }
        }
    }
    function chromUp(e)
    {   // If mouse was down, handle final selection
        $(document).unbind('mousemove',chromMove);
        $(document).unbind('mouseup',chromUp);
        chromMove(e); // Just in case
        if(mouseIsDown) {
            updateImgOffsets();
            var bands;
            var pxUp = e.clientX - img.scrolledLeft;
            var pxY  = e.clientY - img.scrolledTop;
            //warn("chromIdeo("+chr.name+") selected range (pix):"+pxDown+"-"+pxUp+" chrom range (pix):"+chr.left+"-"+chr.right+" chrom range (bp):"+chr.name+":"+chr.beg+"-"+chr.end);
            if(isWithin(0,pxY,img.height)) {  // within vertical range or else cancel
                var selRange = { beg: -1, end: -1, width: -1 };
                var dontAsk = true;

                if(e.ctrlKey) {
                    bands = findCytoBand(pxDown,pxUp);
                    if(bands.end > -1) {
                        pxDown = bands.left;
                        pxUp   = bands.right;
                        mouseHasMoved = true;
                        dontAsk = false;
                        selRange.beg = bands.beg;
                        selRange.end = bands.end;
                        hiliteShow(pxDown,pxUp);
                    }
                }
                else if(mouseHasMoved) {
                    if( isWithin(-20,pxUp,chr.left) ) // bounded by chrom dimensions: but must remain within image!
                        pxUp = chr.left;
                    if( isWithin(chr.right,pxUp,img.width + 20) )
                        pxUp = chr.right;

                    if( isWithin(chr.left,pxUp,chr.right+1) ) {

                        selRange.beg = convertToBases(pxDown);
                        selRange.end = convertToBases(pxUp);

                        if(Math.abs(selRange.end - selRange.beg) < 20)
                            mouseHasMoved = false; // Drag so small: treat as simple click
                        else
                            dontAsk = false;
                    }
                    //else warn("chromIdeo("+chr.name+") NOT WITHIN HORIZONTAL RANGE\n selected range (pix):"+pxDown+"-"+pxUp+" chrom range (pix):"+chr.left+"-"+chr.right);
                }
                if(mouseHasMoved == false) { // Not else because small drag turns this off

                    hiliteShow(pxUp,pxUp);
                    var curBeg = parseInt($("#hgt\\.winStart").val());  // Note the escaped '.'
                    var curEnd = parseInt($("#hgt\\.winEnd").val());
                    var curWidth = curEnd - curBeg;
                    selRange.beg = convertToBases(pxUp) - Math.round(curWidth/2); // Notice that beg is based upon up position
                    selRange.end  = selRange.beg + curWidth;
                }
                if(selRange.end > -1) {
                    // prompt, then submit for new position
                    selRange = rangeNormalizeToChrom(selRange,chr);
                    if(mouseHasMoved == false) { // Update highlight by converting bp back to pix
                        pxDown = convertFromBases(selRange.beg)
                        pxUp = convertFromBases(selRange.end)
                        hiliteShow(pxDown,pxUp);
                    }
                    //if((selRange.end - selRange.beg) < 50000)
                    //    dontAsk = true;
                    if(dontAsk || confirm("Jump to new position:\n\n"+chr.name+":"+commify(selRange.beg)+"-"+commify(selRange.end)+" size:"+commify(selRange.width)) ) {
                        setPositionByCoordinates(chr.name, selRange.beg, selRange.end)
                        $('.cytoBand').mousedown( function(e) { return false; }); // Stop the presses :0)
                        document.TrackHeaderForm.submit();
                        return true; // Make sure the setTimeout below is not called.
                    }
                }
            }
            //else warn("chromIdeo("+chr.name+") NOT WITHIN VERTICAL RANGE\n selected range (pix):"+pxDown+"-"+pxUp+" chrom range (pix):"+chr.left+"-"+chr.right+"\n cytoTop-Bottom:"+chr.top +"-"+chr.bottom);
            hiliteCancel();
            setTimeout('blockUseMap=false;',50);
        }
        mouseIsDown = false;
        mouseHasMoved = false;
    }

    function isWithin(beg,here,end)
    {   // Simple utility
        return ( beg <= here && here < end );
    }
    function convertToBases(pxX)
    {   // Simple utility to convert pix to bases
        var offset = (pxX - chr.left)/chr.width;
        if(chr.reverse)
            offset = 1 - offset;
        return Math.round(offset * chr.size);
    }
    function convertFromBases(bases)
    {   // Simple utility to convert bases to pix
        var offset = bases/chr.size;
        if(chr.reverse)
            offset = 1 - offset;
        return Math.round(offset * chr.width) + chr.left;
    }

    function findDimensions()
    {   // Called at init: determine the dimensions of chrom from 'cytoband' map items
        var lastX = -1;
        $('.cytoBand').each(function(ix) {
            var loc = this.coords.split(",");
            if(loc.length == 4) {
                var myLeft  = parseInt(loc[0]);
                var myRight = parseInt(loc[2]);
                if( chr.top == -1) {
                    chr.left   = myLeft;
                    chr.right  = myRight;
                    chr.top    = parseInt(loc[1]);
                    chr.bottom = parseInt(loc[3]);
                } else {
                    if( chr.left  > myLeft)
                        chr.left  = myLeft;
                    if( chr.right < parseInt(loc[2]))
                        chr.right = parseInt(loc[2]);
                }

                var range = this.title.substr(this.title.lastIndexOf(':')+1)
                var pos = range.split('-');
                if(pos.length == 2) {
                    if( chr.name.length == 0) {
                        chr.beg = parseInt(pos[0]);
                        //chr.end = parseInt(pos[1]);
                        chr.name = this.title.substring(this.title.lastIndexOf(' ')+1,this.title.lastIndexOf(':'))
                    } else {
                        if( chr.beg > parseInt(pos[0]))
                            chr.beg = parseInt(pos[0]);
                    }
                    if( chr.end < parseInt(pos[1])) {
                        chr.end = parseInt(pos[1]);
                        if(lastX == -1)
                            lastX = myRight;
                        else if(lastX > myRight)
                            chr.reverse = true;  // end is advancing, but X is not, so reverse
                    } else if(lastX != -1 && lastX < myRight)
                        chr.reverse = true;      // end is not advancing, but X is, so reverse

                }
            $(this).css( 'cursor', 'text');
            $(this).attr("href","");
            }
        });
        chr.size  = (chr.end   - chr.beg );
        chr.width = (chr.right - chr.left);
    }

    function findCytoBand(pxDown,pxUp)
    {   // Called when mouseup and ctrl: Find the bounding cytoband dimensions, both in pix and bases
        var cyto = { left: -1, right: -1, beg: -1, end: -1 };
        $('.cytoBand').each(function(ix) {
            var loc = this.coords.split(",");
            if(loc.length == 4) {
                var myLeft  = parseInt(loc[0]);
                var myRight = parseInt(loc[2]);
                if(cyto.left == -1 || cyto.left > myLeft) {
                    if( isWithin(myLeft,pxDown,myRight) || isWithin(myLeft,pxUp,myRight) ) {
                        cyto.left  = myLeft;
                        var range = this.title.substr(this.title.lastIndexOf(':')+1)
                        var pos = range.split('-');
                        if(pos.length == 2) {
                            cyto.beg  = (chr.reverse ? parseInt(pos[1]) : parseInt(pos[0]));
                        }
                    }
                }
                if(cyto.right == -1 || cyto.right < myRight) {
                    if( isWithin(myLeft,pxDown,myRight) || isWithin(myLeft,pxUp,myRight) ) {
                        cyto.right = myRight;
                        var range = this.title.substr(this.title.lastIndexOf(':')+1)
                        var pos = range.split('-');
                        if(pos.length == 2) {
                            cyto.end  = (chr.reverse ? parseInt(pos[0]) : parseInt(pos[1]));
                        }
                    }
                }
            }
        });
        return cyto;
    }
    function rangeNormalizeToChrom(selection,chrom)
    {   // Called before presenting or using base range: make sure chrom selection is within chrom range
        if(selection.end < selection.beg) {
            var tmp = selection.end;
            selection.end = selection.beg;
            selection.beg = tmp;
        }
        selection.width = (selection.end - selection.beg);
        selection.beg += 1;
        if( selection.beg < chrom.beg) {
            selection.beg = chrom.beg;
            selection.end = chrom.beg + selection.width;
        }
        if( selection.end > chrom.end) {
            selection.end = chrom.end;
            selection.beg = chrom.end - selection.width;
            if( selection.beg < chrom.beg) { // spans whole chrom
                selection.width = (selection.end - chrom.beg);
                selection.beg = chrom.beg + 1;
            }
        }
        return selection;
    }

    function hiliteShow(down,cur)
    {   // Called on mousemove, mouseup: set drag hilite dimensions
        var topY = img.top;
        var high = img.height;
        var begX = -1;
        var wide = -1;
        if(cur < down) {
            begX = cur + img.left;
            wide = (down - cur);
        } else {
            begX = down + img.left;
            wide = (cur - down);
        }
        $(hilite).css({ left: begX + 'px', width: wide + 'px', top: topY + 'px', height: high + 'px', display:'' });
        $(hilite).show();
    }
    function hiliteCancel(left,width,top,height)
    {   // Called on mouseup: Make green drag hilite disappear when no longer wanted
        $(hilite).hide();
        $(hilite).css({ left: '0px', width: '0px', top: '0px', height: '0px' });
    }

    function hiliteSetup()
    {   // Called on init: setup of drag region hilite (but don't show yet)
        $(hilite).css({ backgroundColor: 'green', opacity: 0.4, borderStyle: 'solid', borderWidth: '1px', bordercolor: '#0000FF' });
        $p = $(chrImg);

        $(hilite).css({ display: 'none', position: 'absolute', overflow: 'hidden', zIndex: 1 });
        jQuery($(chrImg).parents('body')).append($(hilite));
        return hilite;
    }

    function updateImgOffsets()
    {   // Called on mousedown: Gets the current offsets
        var offs = $(chrImg).offset();
        img.top  = Math.round(offs.top );
        img.left = Math.round(offs.left);
        img.scrolledTop  = img.top  - $("body").scrollTop();
        img.scrolledLeft = img.left - $("body").scrollLeft();
        if($.browser.msie) {
            img.height = $(chrImg).outerHeight();
            img.width  = $(chrImg).outerWidth();
        } else {
            img.height = $(chrImg).height();
            img.width  = $(chrImg).width();
        }
        return img;
    }
});
}
/////////////////////////////////////////////////////
// Drag Reorder Code
function imgTblSetOrder(table)
{
// Sets the 'order' value for the image table after a drag reorder
    var names = [];
    var values = [];
    $("tr.imgOrd").each(function (i) {
        if ($(this).attr('abbr') != $(this).attr('rowIndex').toString()) {
            $(this).attr('abbr',$(this).attr('rowIndex').toString());
            var name = this.id.substring('tr_'.length) + '_imgOrd';
            names.push(name);
            values.push($(this).attr('abbr'));
        }
    });
    if(names.length > 0) {
        setCartVars(names,values);
    }
}

function imgTblTrackShowCenterLabel(tr, show)
{   // Will show or hide centerlabel as requested
    // adjust button, sideLabel height, sideLabelOffset and centerlabel display

    if(!$(tr).hasClass('clOpt'))
        return;
    var center = $(tr).find(".sliceDiv.cntrLab");
    if($(center) == undefined)
        return;
    seen = ($(center).css('display') != 'none');
    if(show == seen)
        return;

    var centerHeight = $(center).height();

    var btn = $(tr).find("p.btn");
    var side = $(tr).find(".sliceDiv.sideLab");
    if($(btn) != undefined && $(side) != undefined) {
        var sideImg = $(side).find("img");
        if($(sideImg) != undefined) {
            var top = parseInt($(sideImg).css('top'));
            if(show) {
                $(btn).css('height',$(btn).height() + centerHeight);
                $(side).css('height',$(side).height() + centerHeight);
                top += centerHeight; // top is a negative number
                $(sideImg).css( {'top': top.toString() + "px" });
                $( center ).show();
            } else if(!show) {
                $(btn).css('height',$(btn).height() - centerHeight);
                $(side).css('height',$(side).height() - centerHeight);
                top -= centerHeight; // top is a negative number
                $(sideImg).css( {'top': top.toString() + "px" });
                $( center ).hide();
            }
        }
    }
}

function imgTblZipButtons(table)
{
// Goes through the image and binds composite track buttons when adjacent
    var rows = $(table).find('tr');
    var lastClass="";
    var lastBtn;
    var lastMatchesLast=false;
    var lastBlue=true;
    var altColors=false;
    var count=0;
    var countN=0;
    for(var ix=0;ix<rows.length;ix++) {    // Need to have buttons in order
        var btn = $( rows[ix] ).find("p.btn");
        if( btn.length == 0)
            continue;
        var classList = $( btn ).attr("class").split(" ");
        var curMatchesLast=(classList[0] == lastClass);

        // centerLabels may be conditionally seen
        if($( rows[ix] ).hasClass('clOpt')) {
            if(curMatchesLast && $( rows[ix - 1] ).hasClass('clOpt'))
                imgTblTrackShowCenterLabel(rows[ix],false);  // if same composite and previous is also centerLabel optional then hide center label
            else
                imgTblTrackShowCenterLabel(rows[ix],true);
        }

        // On with buttons
        if(lastBtn != undefined) {
            $( lastBtn ).removeClass('btnN btnU btnL btnD');
            if(curMatchesLast && lastMatchLast) {
                $( lastBtn ).addClass('btnL');
            } else if(lastMatchLast) {
                $( lastBtn ).addClass('btnU');
            } else if(curMatchesLast) {
                $( lastBtn ).addClass('btnD');
            } else {
                $( lastBtn ).addClass('btnN');
                countN++;
            }
            count++;
            if(altColors) {
                lastBlue = (lastMatchLast == lastBlue); // lastMatch and lastBlue or not lastMatch and notLastBlue
                if(lastBlue)    // Too  smart by 1/3rd
                    $( lastBtn ).addClass(    'btnBlue' );
                else
                    $( lastBtn ).removeClass( 'btnBlue' );
            }
        }
        lastMatchLast = curMatchesLast;
        lastClass = classList[0];
        lastBtn = btn;
    }
    if(lastBtn != undefined) {
        $( lastBtn ).removeClass('btnN btnU btnL btnD');
        if(lastMatchLast) {
            $( btn).addClass('btnU');
        } else {
            $( lastBtn ).addClass('btnN');
            countN++;
        }
        if(altColors) {
                lastBlue = (lastMatchLast == lastBlue); // lastMatch and lastBlue or not lastMatch and notLastBlue
                if(lastBlue)    // Too  smart by 1/3rd
                    $( lastBtn ).addClass(    'btnBlue' );
                else
                    $( lastBtn ).removeClass( 'btnBlue' );
        }
        count++;
    }
    //warn("Zipped "+count+" buttons "+countN+" are independent.");
}

function initImgTblButtons()
{
// Make side buttons visible (must also be called when updating rows in the imgTbl).
    var btns = $("p.btn");
    if(btns.length > 0) {
        imgTblZipButtons($('#imgTbl'));
        $(btns).mouseenter( imgTblButtonMouseOver );
        $(btns).mouseleave( imgTblButtonMouseOut  );
        $(btns).show();
    }
var handle = $("td.dragHandle");
    if(handle.length > 0) {
        $(handle).mouseenter( imgTblDragHandleMouseOver );
        $(handle).mouseleave( imgTblDragHandleMouseOut  );
    }
}

function imgTblDragHandleMouseOver()
{
// Highlights a single row when mouse over a dragHandle column (sideLabel and buttons)
    if(jQuery.tableDnD == undefined) {
        //var handle = $("td.dragHandle");
        //$(handle)
        //    .unbind('mouseenter')//, jQuery.tableDnD.mousemove);
        //    .unbind('mouseleave');//, jQuery.tableDnD.mouseup);
        return;
    }
    if (jQuery.tableDnD.dragObject == null) {
        $( this ).parents("tr").addClass("trDrag");
    }
}

function imgTblDragHandleMouseOut()
{
// Ends row highlighting by mouse over
    $( this ).parents("tr").removeClass("trDrag");
}

function imgTblButtonMouseOver()
{
// Highlights a composite set of buttons, regarless of whether tracks are adjacent
    if(jQuery.tableDnD == undefined || jQuery.tableDnD.dragObject == null) {
        var classList = $( this ).attr("class").split(" ");
        var btns = $( "p." + classList[0] );
        $( btns ).removeClass('btnGrey');
        $( btns ).addClass('btnBlue');
    }
}

function imgTblButtonMouseOut()
{
// Ends composite highlighting by mouse over
    var classList = $( this ).attr("class").split(" ");
    var btns = $( "p." + classList[0] );
    $( btns ).removeClass('btnBlue');
    $( btns ).addClass('btnGrey');
}


/////////////////////////////////////////////////////
// Drag Scroll code
jQuery.fn.panImages = function(imgOffset,imgBoxLeftOffset){
    // globals across all panImages
    var leftLimit   = imgBoxLeftOffset*-1;
    var rightLimit  = (imgBoxPortalWidth - imgBoxWidth + leftLimit);
    var prevX       = (imgOffset + imgBoxLeftOffset)*-1;
    var portalWidth = 0;
    panAdjustHeight(prevX);

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

    // globals across all panImages
    portalWidth     = $(pan).width();
    // globals to one panImage
    var newX        = 0;
    var mouseDownX  = 0;
    var mouseIsDown = false;
    var beyondImage = false;
    var atEdge      = false;

    initialize();

    function initialize(){

        pan.css( 'cursor', 'w-resize');

        pan.mousedown(function(e){
            if(mouseIsDown == false) {
                mouseIsDown = true;

                mouseDownX = e.clientX;
                atEdge = (!beyondImage && (prevX >= leftLimit || prevX <= rightLimit));
                $(document).bind('mousemove',panner);
                $(document).bind( 'mouseup', panMouseUp);  // Will exec only once
                return false;
            }
        });
    }

    function panner(e) {
        //if(!e) e = window.event;
        if ( mouseIsDown ) {
            var relativeX = (e.clientX - mouseDownX);

            if(relativeX != 0) {
                blockUseMap = true;
                var decelerator = 1;
                var wingSize    = 1000; // 0 stops the scroll at the edges.
                // Remeber that offsetX (prevX) is negative
                newX = prevX + relativeX;
                if ( newX >= leftLimit ) { // scrolled all the way to the left
                    if(atEdge) {  // Do not drag straight off edge.  Force second drag
                        beyondImage = true;
                        newX = leftLimit + (newX - leftLimit)/decelerator;// slower
                        if( newX >= leftLimit + wingSize) // Don't go too far over the edge!
                        newX =  leftLimit + wingSize;
                    } else
                        newX = leftLimit;

                } else if ( newX < rightLimit ) { // scrolled all the way to the right
                    if(atEdge) {  // Do not drag straight off edge.  Force second drag
                        beyondImage = true;
                        newX = rightLimit - (rightLimit - newX)/decelerator;// slower
                        if( newX < rightLimit - wingSize) // Don't go too far over the edge!
                            newX = rightLimit - wingSize;
                    } else
                        newX = rightLimit;

                } else if(newX >= rightLimit && newX < leftLimit)
                    beyondImage = false; // could have scrolled back without mouse up

                var nowPos = newX.toString() + "px";
                $(".panImg").css( {'left': nowPos });
                $('.tdData').css( {'backgroundPosition': nowPos } );
                panUpdatePosition(newX,false);
            }
        }
    }
    function panMouseUp(e) {  // Must be a separate function instead of pan.mouseup event.
        //if(!e) e = window.event;
        if(mouseIsDown) {

            $(document).unbind('mousemove',panner);
            $(document).unbind('mouseup',panMouseUp);
            mouseIsDown = false;

            // Talk to tim about this.
            if(beyondImage) {
                // FIXME:
                // 1) When dragging beyondImage, side label is seen. Only solution may be 2 images!
                //    NOTE: tried clip:rect() but this only works with position:absolute!
                // 2) Would be nice to support drag within AND beyond image:
                //    a) Within stops at border, and then second drag needed to go beyond
                //    b) Within needs support for next/prev arrows
                // 3) AJAX does not work on full image.  Larry's callback routine needs extension beyond songle track
                //$.ajax({type: "GET", url: "../cgi-bin/hgTracks",
                //    data: "hgt.trackImgOnly=1&hgsid=" + getHgsid(), dataType: "html",
                //    trueSuccess: handleUpdateTrackMap, success: catchErrorOrDispatch,
                //    cmd: "wholeImage",  cache: false });
                document.TrackHeaderForm.submit();
                return true; // Make sure the setTimeout below is not called.
            }
            if(prevX != newX) {
                panAdjustHeight(newX);
                prevX = newX;
            }
            setTimeout('blockUseMap=false;',50); // Necessary incase the selectEnd was over a map item. select takes precedence.
        }
    }
});

    function panUpdatePosition(newOffsetX,bounded)
    {
        // Updates the 'position/search" display with change due to panning
        var portalWidthBases = imgBoxPortalEnd - imgBoxPortalStart;
        var portalScrolledX  = (imgBoxPortalOffsetX+imgBoxLeftLabel) + newOffsetX;

        var newPortalStart = imgBoxPortalStart - Math.round(portalScrolledX*imgBoxBasesPerPixel); // As offset goes down, bases seen goes up!
        if( newPortalStart < imgBoxChromStart && bounded)     // Stay within bounds
            newPortalStart = imgBoxChromStart;
        var newPortalEnd = newPortalStart + portalWidthBases;
        if( newPortalEnd > imgBoxChromEnd && bounded) {
            newPortalEnd = imgBoxChromEnd;
            newPortalStart = newPortalEnd - portalWidthBases;
        }
        if(newPortalStart > 0) {
            // XXXX ? imgBoxPortalStart = newPortalStart;
            // XXXX ? imgBoxPortalEnd = newPortalEnd;
            var newPos = document.getElementById("hgt.chromName").value + ":" + commify(newPortalStart) + "-" + commify(newPortalEnd);
            setPosition(newPos, (newPortalEnd - newPortalStart + 1));
        }
        return true;
    }
    function mapTopAndBottom(mapName,left,right)
    {
    // Find the top and bottom px given left and right boundaries
        var span = { top: -10, bottom: -10 };
        var items = $("map[name='"+mapName+"']").children();
        if($(items).length>0) {
            $(items).each(function(t) {
                var loc = this.coords.split(",");
                var aleft   = parseInt(loc[0]);
                var aright  = parseInt(loc[2]);
                if(aleft < right && aright >= left) {
                    var atop    = parseInt(loc[1]);
                    var abottom = parseInt(loc[3]);
                    if( span.top    < 0 ) {
                        span.top    = atop;
                        span.bottom = abottom;
                    } else if(span.top > atop) {
                            span.top = atop;
                    } else if(span.bottom < abottom) {
                            span.bottom = abottom;
                    }
                }
            });
        }
        return span;
    }
    function panAdjustHeight(newOffsetX) {
        // Adjust the height of the track data images so that bed items scrolled off screen
        // do not waste vertical real estate
        var left = newOffsetX * -1;
        var right = left + portalWidth;
        $(".panImg").each(function(t) {
            var mapid  = "map_" + this.id.substring(4);
            var hDiv   = $(this).parent();
            var top    = parseInt($(this).css("top")) * -1;
            var bottom = top + $(hDiv).height();

            var span = mapTopAndBottom(mapid,left,right);
            if(span.top > 0) {
                var topdif = Math.abs(span.top - top);
                var botdif = Math.abs(span.bottom - bottom);
                if(topdif > 2 || botdif > 2) {
                    $(hDiv).height( span.bottom - span.top );
                    top = span.top * -1;
                    $(this).css( {'top': top.toString() + "px" });

                    // Need to adjust side label height as well!
                    var imgId = this.id.split("_");
                    var titlePx = 0;
                    var center = $("#img_center_"+imgId[2]);
                    if(center.length > 0) {
                        titlePx = $(center).parent().height();
                        top += titlePx;
                    }
                    var side = $("#img_side_"+imgId[2]);
                    if( side.length > 0) {
                        $(side).parent().height( span.bottom - span.top + titlePx);
                        $(side).css( {'top': top.toString() + "px" });
                    }
                    var btn = $("#p_btn_"+imgId[2]);
                    if( btn.length > 0) {
                        $(btn).height( span.bottom - span.top + titlePx);
                    } else {
                        btn = $("#img_btn_"+imgId[2]);
                        if( btn.length > 0) {
                            $(btn).parent().height( span.bottom - span.top + titlePx);
                            $(btn).css( {'top': top.toString() + "px" });
                        }
                    }
                }
            }
        });
    }

};

/////////////////////////////////////////////////////

function saveMouseOffset(ev)
{   // Save the mouse offset associated with this event
    originalMouseOffset = {x: ev.clientX, y: ev.clientY};
}

function mouseHasMoved(ev)
{   // return true if mouse has moved a significant amount
    var minPixels = 10;
    var movedX = ev.clientX - originalMouseOffset.x;
    var movedY = ev.clientY - originalMouseOffset.y;
    if ( arguments.length == 2) {
        var num = Number(arguments[1]);
        if(isNaN(num)) {
            if ( arguments[1].toLowerCase() == "x" )
                return (movedX > minPixels || movedX < (minPixels * -1));
            if ( arguments[1].toLowerCase() == "y" )
                return (movedY > minPixels || movedY < (minPixels * -1));
        }
        else
            minPixels = num;
    }
    return (   movedX > minPixels || movedX < (minPixels * -1)
            || movedY > minPixels || movedY < (minPixels * -1));
}

function blockTheMap(e)
{
    blockUseMap=true;
}

function blockTheMapOnMouseMove(ev)
{
    if (!blockUseMap && mouseHasMoved(ev)) {
        blockUseMap=true;
    }
}

// wait for jStore to prepare the storage engine (this token reload code is experimental and currently dead).
jQuery.jStore && jQuery.jStore.ready(function(engine) {
    // alert(engine.jri);
    // wait for the storage engine to be ready.
    engine.ready(function(){
        var engine = this;
        var newToken = document.getElementById("hgt.token").value;
        if(newToken) {
            var oldToken = engine.get("token");
            if(oldToken && oldToken == newToken) {
                // user has hit the back button.
                jQuery('body').css('cursor', 'wait');
                window.location = "../cgi-bin/hgTracks?hgsid=" + getHgsid();
            }
        }
        engine.set("token", newToken);
    });
});

function mapClk(obj)
{
    return postToSaveSettings(obj);
}

function postToSaveSettings(obj)
{
    if(blockUseMap==true) {
        return false;
    }
    if(obj.href == undefined) // called directly with obj and from callback without obj
        obj = this;
    if( obj.href.match('#') || obj.target.length > 0) {
        //alert("Matched # ["+obj.href+"] or has target:"+obj.target);
        return true;
    }
    var thisForm=$(obj).parents('form');
    if(thisForm == undefined || $(thisForm).length == 0)
        thisForm=$("FORM");
    if($(thisForm).length > 1 )
        thisForm=$(thisForm)[0];
    if(thisForm != undefined && $(thisForm).length == 1) {
        //alert("posting form:"+$(thisForm).attr('name'));
        return postTheForm($(thisForm).attr('name'),obj.href);
    }
    return true;
}

$(document).ready(function()
{
    var db = getDb();
    if(jQuery.fn.autocomplete && $('input#suggest') && db) {
        $('input#suggest').autocomplete({
                                            delay: 500,
                                            minchars: 2,
                                            ajax_get: ajaxGet(function () {return db;}, new Object),
                                            callback: function (obj) {
                                                setPosition(obj.id, commify(getSizeFromCoordinates(obj.id)));
                                                // jQuery('body').css('cursor', 'wait');
                                                // document.TrackHeaderForm.submit();
                                            }
                                        });

        // I want to set focus to the suggest element, but unforunately that prevents PgUp/PgDn from
        // working, which is a major annoyance.
        // $('input#suggest').focus();
    }
    initVars();

    if(jQuery.jStore) {
        // Experimental (currently dead) code to handle "user hits back button" problem.
        if(false) {
            jQuery.extend(jQuery.jStore.defaults, {
                              project: 'hgTracks',
                              engine: 'flash',
                              flash: '/jStore.Flash.html'
                          });
        }
        jQuery.jStore.load();
    }

    // Convert map AREA gets to post the form, ensuring that cart variables are kept up to date (but turn this off for search form).
    if($("FORM").length > 0 && $('#trackSearch').length == 0) {
        var allLinks = $('a');
        $( allLinks ).unbind('click');
        $( allLinks ).click( postToSaveSettings );
    }
    if($('#pdfLink').length == 1) {
        $('#pdfLink').click(function(i) {
            var thisForm=$('#TrackForm');
            if(thisForm != undefined && $(thisForm).length == 1) {
                //alert("posting form:"+$(thisForm).attr('name'));
                updateOrMakeNamedVariable($(thisForm),'hgt.psOutput','on');
                return postTheForm($(thisForm).attr('name'),this.href);
            }
            return true;
        });
    }
    if($('#imgTbl').length == 1) {
        imageV2   = true;
        initImgTblButtons();
        // Make imgTbl allow draw reorder of imgTrack rows
        var imgTable = $(".tableWithDragAndDrop");
        if($(imgTable).length > 0) {
            $(imgTable).tableDnD({
                onDragClass: "trDrag",
                dragHandle: "dragHandle",
                scrollAmount: 40,
                onDragStart: function(ev, table, row) {
                    saveMouseOffset(ev);
                    $(document).bind('mousemove',blockTheMapOnMouseMove);
                },
                onDrop: function(table, row, dragStartIndex) {
                    if($(row).attr('rowIndex') != dragStartIndex) {
                        if(imgTblSetOrder) {
                            imgTblSetOrder(table);
                        }
                        imgTblZipButtons( table );
                    }
                    $(document).unbind('mousemove',blockTheMapOnMouseMove);
                    setTimeout('blockUseMap=false;',50); // Necessary incase the onDrop was over a map item. onDrop takes precedence.
                }
            });
        }
        if(imgBoxPortal) {
            //warn("imgBox("+imgBoxChromStart+"-"+imgBoxChromEnd+","+imgBoxWidth+") bases/pix:"+imgBoxBasesPerPixel+"\nportal("+imgBoxPortalStart+"-"+imgBoxPortalEnd+","+imgBoxPortalWidth+") offset:"+imgBoxPortalOffsetX);

            // Turn on drag scrolling.
            $("div.scroller").panImages(imgBoxPortalOffsetX,imgBoxLeftLabel);
        }
        //$("#zoomSlider").slider({ min: -4, max: 3, step: 1 });//, handle: '.ui-slider-handle' });

        // Temporary warning while new imageV2 code is being worked through
        if($('#map').children("AREA").length > 0) {
            warn('Using imageV2, but old map is not empty!');
        }

        // Retrieve tracks via AJAX that may take too long to draw initialliy (i.e. a remote bigWig)
        var retrievables = $('#imgTbl').find("tr.mustRetrieve")
        if($(retrievables).length > 0) {
            $(retrievables).each( function (i) {
                var trackName = $(this).attr('id').substring(3);
                updateTrackImg(trackName,"","");
            });
        }
    }
    if($('img#chrom').length == 1) {
        if($('.cytoBand').length > 1) {
            $('img#chrom').chromDrag();
        }
    }

    if($("#tabs").length > 0) {
        // Search page specific code

        var val = $('#currentTab').val();
        $("#tabs").tabs({
                            show: function(event, ui) {
                                $('#currentTab').val(ui.panel.id);
                            },
                            select: function(event, ui) {
                                if( ui.panel.id == 'simpleTab' && $('div#found').length < 1) {
                                    setTimeout("$('input#simpleSearch').focus();",20); // delay necessary, since select event not afterSelect event
                                }
                            }
                        });
        $('#tabs').show();
        $("#tabs").tabs('option', 'selected', '#' + val);
        if(val =='simpleTab' && $('div#found').length < 1) {
            $('input#simpleSearch').focus();
        }
        $("#tabs").css('font-family', jQuery('body').css('font-family'));
        $("#tabs").css('font-size', jQuery('body').css('font-size'));
        $('.submitOnEnter').keydown(searchKeydown);
        findTracksNormalize();
        updateMetaDataHelpLinks(0);
    }

    if(typeof(trackDbJson) != "undefined" && trackDbJson != null) {
        for (var id in trackDbJson) {
            var rec = trackDbJson[id];
            if(rec.type == "remote") {
                if($("#img_data_" + id).length > 0) {
                    // load the remote track renderer via jsonp
                    var script = document.createElement('script');
                    // XXXX add current image width
                    var pos = parsePosition(getPosition());
                    script.setAttribute('src', rec.url + "?track=" + id + "&jsonp=remoteTrackCallback&c=" + pos.chrom +
                                        "&s=" + pos.start + "&e=" + pos.end);
                    document.getElementsByTagName('head')[0].appendChild(script);
                }
            }
        }
    }
});

function rulerModeToggle (ele)
{
    autoHideSetting = !ele.checked;
    var obj = imgAreaSelect.data('imgAreaSelect');
    obj.setOptions({autoHide : autoHideSetting});
}

function makeMapItem(id)
{
    // Create a dummy mapItem on the fly (for objects that don't have corresponding entry in the map).
    if(typeof(trackDbJson) != "undefined" && trackDbJson != null) {
        var title;
        var rec = trackDbJson[id];
        if(rec) {
            title = rec.shortLabel;
        } else {
            title = id;
        }
        return {id: id, title: "configure " + title};
    } else {
        return null;
    }
}

function mapItemMouseOver(obj)
{
    // Record data for current map area item
    currentMapItem = makeMapItem(obj.id);
    if(currentMapItem != null) {
        currentMapItem.href = obj.href;
        currentMapItem.title = obj.title;
    }
}

function mapItemMouseOut(obj)
{
    currentMapItem = null;
}

function findMapItem(e)
{
// Find mapItem for given event; returns item object or null if none found.

    if(currentMapItem) {
        return currentMapItem;
    }

    // rightClick for non-map items that can be resolved to their parent tr and then trackName (e.g. items in gray bar)
    if(e.target.tagName.toUpperCase() != "AREA") {
        var tr = $( e.target ).parents('tr.imgOrd');
        if( $(tr).length == 1 ) {
            a = /tr_(.*)/.exec($(tr).attr('id'));  // voodoo
            if(a && a[1]) {
                var id = a[1];
                return makeMapItem(id);
            }
        }
    }

    // FIXME: do we really need to worry about non-imageV2 ?
    // Yeah, I think the rest of this is (hopefully) dead code

    var x,y;
    if(imageV2) {
        // It IS appropriate to use coordinates relative to the img WHEN we have a hit in the right-hand side, but NOT
        // when we have a hit in the left hand elements (which do not have relative coordinates).
        // XXXX still trying to figure this out.
        var pos = $(e.target).position();
        if(e.target.tagName == "IMG") {
            // msie
            // warn("img: x: " + x + ", y:" + y);
            // warn("pageX: " + e.pageX + "; offsetLeft: " + pos.left);
            x = e.pageX - pos.left;
            y = e.pageY - pos.top;
            // warn("x: " + x + "; y: " + y);
        } else {
            x = e.pageX - trackImg.attr("offsetLeft");
            y = e.pageY - trackImg.attr("offsetTop");
        }
        // console.log(trackImg.attr("offsetLeft"), trackImg.attr("offsetTop"));
        // console.log("findMapItem:", x, y);
        // console.dir(mapItems);
    } else {
        x = e.pageX - e.target.offsetLeft;
        y = e.pageY - e.target.offsetTop;
    }

    var retval = -1;
    // console.log(e.target.tagName + "; " + e.target.id);
    for(var i=0;i<mapItems.length;i++)
    {
        if(mapItems[i].obj && e.target === mapItems[i].obj) {
            // e.target is AREA tag under FF and Safari;
            // This never occurs under IE
            // console.log("Found match by objects comparison");
            retval = i;
            break;
        } else if (!imageV2 || browser == "msie") {
            // Under IE, target is the IMG tag for the map's img and y is relative to the top of that,
            // so we must use the target's src to make sure we are looking at the right map.
            // We start falling through to here under safari under imageV2 once something has been modified
            // (that's a bug I still haven't figured out how to fix).
            if(mapItems[i].r.contains(x, y) && mapItems[i].src == $(e.target).attr('src')) {
                retval = i;
                break;
            }
        }
    }
    // showWarning(x + " " + y + " " + retval + " " + e.target.tagName + " " + $(e.target).attr('src'));
    // console.log("findMapItem:", e.clientX, e.clientY, x, y, pos.left, pos.top, retval, mapItems.length, e.target.tagName);
    // console.log(e.clientX, pos);
    if(retval >= 0) {
        return mapItems[retval];
    } else {
        return null;
    }
}

function mapEvent(e)
{
    var o = findMapItem(e);
    if(o) {
        e.target.title = o.title;
    } else {
        // XXXX this doesn't work.
        // $('#myMenu').html("<ul id='myMenu' class='contextMenu'><li class='edit'><a href='#img'>Get Image</a></li></ul>");
        e.target.title = originalImgTitle;
    }
}

function mapMouseDown(e)
{
    // XXXX Is rightclick logic necessary?
    var rightclick = e.which ? (e.which == 3) : (e.button == 2);
    if(rightclick)
    {
        return false;
    } else {
        var o = findMapItem(e);
        if(o) {
            // XXXX Why does href get changed to "about://" on IE?
            window.location = o.href;
        }
        return true;
    }
}

function contextMenuHit(menuItemClicked, menuObject, cmd)
{
    setTimeout(function() { contextMenuHitFinish(menuItemClicked, menuObject, cmd); }, 1);
}

function contextMenuHitFinish(menuItemClicked, menuObject, cmd)
{
// dispatcher for context menu hits
    if(menuObject.shown) {
        // showWarning("Spinning: menu is still shown");
        setTimeout(function() { contextMenuHitFinish(menuItemClicked, menuObject, cmd); }, 10);
        return;
    }
    if(cmd == 'selectWholeGene' || cmd == 'getDna') {
            // bring whole gene into view or redirect to DNA screen.
            var href = selectedMenuItem.href;
            var chromStart, chromEnd;
            var a = /hgg_chrom=(\w+)&/.exec(href);
            // Many links leave out the chrom (b/c it's in the server side cart as "c")
            var chrom = document.getElementById("hgt.chromName").value;
            if(a) {
                if(a && a[1])
                    chrom = a[1];
                a = /hgg_start=(\d+)/.exec(href);
                if(a && a[1])
                    // XXXX does chromStart have to be incremented by 1?
                    chromStart = a[1];
                a = /hgg_end=(\d+)/.exec(href);
                if(a && a[1])
                    chromEnd = a[1];
            } else {
                // a = /hgc.*\W+c=(\w+)/.exec(href);
                a = /hgc.*\W+c=(\w+)/.exec(href);
                if(a && a[1])
                    chrom = a[1];
                a = /o=(\d+)/.exec(href);
                if(a && a[1])
                    chromStart = parseInt(a[1]) + 1;
                a = /t=(\d+)/.exec(href);
                if(a && a[1])
                    chromEnd = parseInt(a[1]);
            }
            if(chrom == null || chromStart == null || chromEnd == null) {
                showWarning("couldn't parse out genomic coordinates");
            } else {
                if(cmd == 'getDna')
                {
                    if(window.open("../cgi-bin/hgc?hgsid=" + getHgsid() + "&g=getDna&i=mixed&c=" + chrom + "&l=" + (chromStart - 1) + "&r=" + chromEnd) == null) {
                        windowOpenFailedMsg();
                    }
                } else {
                    var newPosition = setPositionByCoordinates(chrom, chromStart, chromEnd);
                    if(browser == "safari" || imageV2) {
                        // We need to parse out more stuff to support resetting the position under imageV2 via ajax, but it's probably possible.
                        // See comments below on safari problems.
                        jQuery('body').css('cursor', 'wait');
                        if(document.TrackForm)
                            document.TrackForm.submit();
                        else
                            document.TrackHeaderForm.submit();
                    } else {
                        // XXXX This attempt to "update whole track image in place" didn't work for a variety of reasons, so this is dead code, but
                        // I'm leaving it in case we try to implement this functionality in the future.
                        jQuery('body').css('cursor', '');
                        $.ajax({
                                   type: "GET",
                                   url: "../cgi-bin/hgTracks",
                                   data: "hgt.trackImgOnly=1&hgt.ideogramToo=1&position=" + newPosition + "&hgsid=" + getHgsid(),
                                   dataType: "html",
                                   trueSuccess: handleUpdateTrackMap,
                                   success: catchErrorOrDispatch,
                                   error: errorHandler,
                                   cmd: cmd,
                                   cache: false
                               });
                    }
                }
            }
    } else if (cmd == 'hgTrackUi_popup') {

        hgTrackUiPopUp( selectedMenuItem.id, false );  // Launches the popup but shields the ajax with a waitOnFunction

    } else if (cmd == 'hgTrackUi_follow') {

        var link = $( 'td#td_btn_'+ selectedMenuItem.id ).children('a'); // The button already has the ref
        if( $(link) != undefined) {
            location.assign($(link).attr('href'));
        } else {
            var url = "hgTrackUi?hgsid=" + getHgsid() + "&g=";
            var id = selectedMenuItem.id;
            var rec = trackDbJson[id];
            if (tdbHasParent(rec) && tdbIsLeaf(rec))
                url += rec.parentTrack
            else
                url = selectedMenuItem.id;
            location.assign(url);
        }

    } else if (cmd == 'dragZoomMode') {
        autoHideSetting = true;
        var obj = imgAreaSelect.data('imgAreaSelect');
        obj.setOptions({autoHide : true, movable: false});
    } else if (cmd == 'hilightMode') {
        autoHideSetting = false;
        var obj = imgAreaSelect.data('imgAreaSelect');
        obj.setOptions({autoHide : false, movable: true});
    } else if (cmd == 'viewImg') {
        // Fetch a new copy of track img and show it to the user in another window. This code assume we have updated
        // remote cart with all relevant chages (e.g. drag-reorder).
        var data = "hgt.imageV1=1&hgt.trackImgOnly=1&hgsid=" + getHgsid();
        jQuery('body').css('cursor', 'wait');
        $.ajax({
                   type: "GET",
                   url: "../cgi-bin/hgTracks",
                   data: data,
                   dataType: "html",
                   trueSuccess: handleViewImg,
                   success: catchErrorOrDispatch,
                   error: errorHandler,
                   cmd: cmd,
                   cache: false
               });
    } else if (cmd == 'openLink') {
        // Remove hgsid to force a new session (see redmine ticket 1333).
        var href = removeHgsid(selectedMenuItem.href);
        var chrom = $("input[name=chromName]").val();
        if(chrom && href.indexOf("c=" + chrom) == -1) {
            // make sure the link contains chrom info (necessary b/c we are stripping hgsid)
            href = href + "&c=" + chrom;
        }
        if(window.open(href) == null) {
            windowOpenFailedMsg();
        }
    } else if (cmd == 'followLink') {
        // XXXX This is blocked by Safari's popup blocker (without any warning message).
        location.assign(selectedMenuItem.href);
    } else {   // if( cmd in 'hide','dense','squish','pack','full','show' )
        // Change visibility settings:
        //
        // First change the select on our form:
        var id = selectedMenuItem.id;
        var rec = trackDbJson[id];
        var selectUpdated = updateVisibility(id, cmd);

        // Now change the track image
        if(imageV2 && cmd == 'hide')
        {
            // Hide local display of this track and update server side cart.
            // Subtracks controlled by 2 settings so del vis and set sel=0.  Others, just set vis hide.
            if(rec.parentTrack != undefined)
                setCartVars( [ id, id+"_sel" ], [ "[]", 0 ] ); // Don't set '_sel" to [] because default gets used, but we are explicitly hiding this!
            else
                setCartVar(id, 'hide' );
            $('#tr_' + id).remove();
            initImgTblButtons();
            loadImgAreaSelect(false);
        } else if (!mapIsUpdateable) {
            jQuery('body').css('cursor', 'wait');
            if(selectUpdated) {
                // assert(document.TrackForm);
                document.TrackForm.submit();
            } else {
                    // add a hidden with new visibility value
                    var form = $(document.TrackHeaderForm);
                    $("<input type='hidden' name='" + id + "'value='" + cmd + "'>").appendTo(form);
                    document.TrackHeaderForm.submit();
            }
        } else {
            var data = "hgt.trackImgOnly=1&" + id + "=" + cmd + "&hgsid=" + getHgsid();  // this will update vis in remote cart
            if(imageV2) {
	        data += "&hgt.trackNameFilter=" + id;
            }
            //var center = $("#img_data_" + id);
            //center.attr('src', "../images/loading.gif")
            //center.attr('style', "text-align: center; display: block;");
            //warn("hgTracks?"+data); // Uesful to cut and paste the url
            var loadingId = showLoadingImage("tr_" + id);
            $.ajax({
                       type: "GET",
                       url: "../cgi-bin/hgTracks",
                       data: data,
                       dataType: "html",
                       trueSuccess: handleUpdateTrackMap,
                       success: catchErrorOrDispatch,
                       error: errorHandler,
                       cmd: cmd,
                       newVisibility: cmd,
                       id: id,
                       loadingId: loadingId,
                       cache: false
                   });
        }
    }
}

function makeContextMenuHitCallback(title)
{
// stub to avoid problem with a function closure w/n a loop
    return function(menuItemClicked, menuObject) {
        contextMenuHit(menuItemClicked, menuObject, title); return true;
    };
}

function makeImgTag(img)
{
// Return img tag with explicit dimensions for img (dimensions are currently hardwired).
// This fixes the "weird shadow problem when first loading the right-click menu" seen in FireFox 3.X,
// which occurred b/c FF doesn't actually fetch the image until the menu is being shown.
    return "<img width='16px' height='16px' src='../images/" + img + "' />";
}

function loadContextMenu(img)
{
    var menu = img.contextMenu(
        function() {
            var menu = [];
            var selectedImg = makeImgTag("greenChecksm.png");
            var blankImg    = makeImgTag("invisible16.png");
            var done = false;
            if(selectedMenuItem && selectedMenuItem.id != null) {
                var href = selectedMenuItem.href;
                var isHgc, isGene;
                if(href) {
                    isGene = href.match("hgGene");
                    isHgc = href.match("hgc");
                }
                var id = selectedMenuItem.id;
                var rec = trackDbJson[id];
                // XXXX what if select is not available (b/c trackControlsOnMain is off)?
                // Move functionality to a hidden variable?
                var select = $("select[name=" + id + "]");
                var cur = select.val();
                if(cur) {
                    select.children().each(function(index, o) {
                                               var title = $(this).val();
                                               var str = blankImg + " " + title;
                                               if(title == cur)
                                                   str = selectedImg + " " + title;
                                               var o = new Object();
                                               o[str] = {onclick: function (menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, title); return true;}};
                                               menu.push(o);
                                           });
                    done = true;
                } else {
                    if(rec) {
                        // XXXX check current state from a hidden variable.
                        var visibilityStrs = new Array("hide", "dense", "squish", "pack", "full");
                        for (i in visibilityStrs) {
                            // XXXX use maxVisibility and change hgTracks so it can hide subtracks
                            var o = new Object();
                            var str = blankImg + " " + visibilityStrs[i];
                            if(rec.canPack || (visibilityStrs[i] != "pack" && visibilityStrs[i] != "squish")) {
                                if(rec.localVisibility) {
                                    if(visibilityStrs[i] == rec.localVisibility) {
                                        str = selectedImg + " " + visibilityStrs[i];
                                    }
                                } else if(visibilityStrs[i] == visibilityStrsOrder[rec.visibility]) {
                                    str = selectedImg + " " + visibilityStrs[i];
                                }
                                o[str] = {onclick: makeContextMenuHitCallback(visibilityStrs[i])};
                                menu.push(o);
                            }
                        }
                        done = true;
                    }
                }
                if(done) {
                    var o = new Object();
                    var any = false;
                    if(isGene || isHgc) {
                        var title = selectedMenuItem.title || "feature";
                        o[makeImgTag("magnify.png") + " Zoom to " +  title] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "selectWholeGene"); return true; }};
                        o[makeImgTag("dnaIcon.png") + " Get DNA for " +  title] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "getDna"); return true; }};
                        o[makeImgTag("bookOut.png") + " Open details page in new window..."] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "openLink"); return true; }};
                        any = true;
                    }
                    if(selectedMenuItem.title != undefined && selectedMenuItem.title.length > 0
                    && selectedMenuItem.href  != undefined && selectedMenuItem.href.length  > 0) {
                        var str = selectedMenuItem.title;
                        if(str.indexOf("Click to alter ") == 0) {
                            ; // suppress the "Click to alter..." items
                        } else if(selectedMenuItem.href.indexOf("cgi-bin/hgTracks") != -1) {
                            ; // suppress menu items for hgTracks links (e.g. Next/Prev map items).
                        } else {
                            if(str.indexOf("display density") != -1)
                                str = makeImgTag("toggle.png") + str;
                            else
                                str = makeImgTag("book.png") + " Show details for " + str + "...";
                            o[str] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "followLink"); return true; }};
                            any = true;
                        }
                    }
                    if(any) {
                        menu.push($.contextMenu.separator);
                        menu.push(o);
                    }
                }
            }
            if(!done) {
                if(false) {
                    // Currently toggling b/n drag-and-zoom mode and hilite mode is disabled b/c we don't know how to keep hilite mode from disabling the
                    // context menus.
                    var o = new Object();
                    var str = "drag-and-zoom mode";
                    if(autoHideSetting) {
                        str += selectedImg;
                        // menu[str].className = 'context-menu-checked-item';
                    }
                    o[str] = { onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "dragZoomMode"); return true; }};
                    menu.push(o);
                    o = new Object();
                    // console.dir(ele);
                    str = "hilight mode";
                    if(!autoHideSetting) {
                        str += selectedImg;
                    }
                    o[str] = { onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "hilightMode"); return true; }};
                    menu.push(o);
                }
                //menu.push({"view image": {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "viewImg"); return true; }}});
            }

            if(selectedMenuItem && rec && rec["configureBy"] != 'none') {
                // Add cfg options at just shy of end...
                var o = new Object();
                if(tdbIsLeaf(rec)) {
                    o[makeImgTag("wrench.png") + " Configure " + rec.shortLabel] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "hgTrackUi_popup"); return true; }};
                    if(rec.parentTrack != undefined)
                        o[makeImgTag("folderWrench.png") + " Configure " + rec.parentLabel + " track set..."] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "hgTrackUi_follow"); return true; }};
                } else
                    o[makeImgTag("folderWrench.png") + " Configure " + rec.shortLabel + " track set..."] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "hgTrackUi_follow"); return true; }};
                menu.push($.contextMenu.separator);
                menu.push(o);
            }

            // Add view image at end
            var o = new Object();
            o[makeImgTag("eye.png") + " View image"] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "viewImg"); return true; }};
            menu.push($.contextMenu.separator);
            menu.push(o);

            return menu;
        },
        {
            beforeShow: function(e) {
                // console.log(mapItems[selectedMenuItem]);
                selectedMenuItem = findMapItem(e);
                // XXXX? blockUseMap = true;
            },
            hideCallback: function() {
                // this doesn't work
                warn("hideCallback");
            }
        });
    return;
}

function parseMap(ele, reset)
{
// Parse the jQuery <map> object into returned mapItems array (ele needn't be the element attached to current document).
        if(reset || !mapItems) {
            mapItems = new Array();
        }
        if(ele) {
                var i = mapItems.length;
                // src is necessary under msie
                var src = ele.next().attr('src');
                ele.children().each(function() {
                                      mapItems[i++] = {
                                          r : new Rectangle(this.coords),
                                          href : this.href,
                                          title : this.title,
                                          id : this.id,
                                          src : src,
                                          obj : this
                                      };
                                  });
    }
    return mapItems;
}

function updateTrackImg(trackName,extraData,loadingId)
{
    var data = "hgt.trackImgOnly=1&hgsid=" + getHgsid() + "&hgt.trackNameFilter=" + trackName;
    if(extraData != undefined && extraData != "")
        data += "&" + extraData;
    if(loadingId == undefined || loadingId == "")
        loadingId = showLoadingImage("tr_" + trackName);
    $.ajax({
                type: "GET",
                url: "../cgi-bin/hgTracks",
                data: data,
                dataType: "html",
                trueSuccess: handleUpdateTrackMap,
                success: catchErrorOrDispatch,
                error: errorHandler,
                cmd: 'refresh',
                loadingId: loadingId,
                id: trackName,
                cache: false
            });
}

var popUpTrackName = "";
var popUpTrackDescriptionOnly = false;
var popSaveAllVars = null;
function _hgTrackUiPopUp(trackName,descriptionOnly)
{ // popup cfg dialog
    popUpTrackName = trackName;
    var myLink = "../cgi-bin/hgTrackUi?g=" + trackName + "&hgsid=" + getHgsid() + "&db=" + getDb();
    popUpTrackDescriptionOnly = descriptionOnly;
    if(popUpTrackDescriptionOnly)
        myLink += "&descriptionOnly=1";

    var rec = trackDbJson[trackName];
    if(!descriptionOnly && rec != null && rec["configureBy"] != null) {
        if (rec["configureBy"] == 'none')
            return;
        else if (rec["configureBy"] == 'clickThrough') {
            jQuery('body').css('cursor', 'wait');
            window.location = myLink;
            return;
        }  // default falls through to configureBy popup
    }
    myLink += "&ajax=1";
    $.ajax({
                type: "GET",
                url: myLink,
                dataType: "html",
                trueSuccess: handleTrackUi,
                success: catchErrorOrDispatch,
                error: errorHandler,
                cmd: selectedMenuItem,
                cache: false
            });
}

function hgTrackUiPopUp(trackName,descriptionOnly)
{
    waitOnFunction( _hgTrackUiPopUp, trackName, descriptionOnly );  // Launches the popup but shields the ajax with a waitOnFunction
}

function hgTrackUiPopCfgOk(popObj, trackName)
{ // When hgTrackUi Cfg popup closes with ok, then update cart and refresh parts of page
    var rec = trackDbJson[trackName];
    var subtrack = tdbIsSubtrack(rec) ? trackName :undefined;  // If subtrack then vis rules differ
    var allVars = getAllVars($('#pop'), subtrack );
    var changedVars = varHashChanges(allVars,popSaveAllVars);
    //warn("cfgVars:"+varHashToQueryString(changedVars));
    var newVis = changedVars[trackName];
    var hide = (newVis != null && (newVis == 'hide' || newVis == '[]'));  // subtracks do not have "hide", thus '[]'
    if($('#imgTbl') == undefined) { // On findTracks or config page
        setVarsFromHash(changedVars);
        //if(hide) // TODO: When findTracks or config page has cfg popup, then vis change needs to be handled in page here
    }
    else {  // On image page
        if(hide) {
            setVarsFromHash(changedVars);
            $('#tr_' + trackName).remove();
            initImgTblButtons();
            loadImgAreaSelect(false);
        } else {
            // Keep local state in sync if user changed visibility
            if(newVis != null) {
                updateVisibility(trackName, newVis);
            }
            var urlData = varHashToQueryString(changedVars);
            if(mapIsUpdateable) {
                updateTrackImg(trackName,urlData,"");
            } else {
                window.location = "../cgi-bin/hgTracks?" + urlData + "&hgsid=" + getHgsid();
            }
        }
    }
}

function handleTrackUi(response, status)
{
// Take html from hgTrackUi and put it up as a modal dialog.

    // make sure all links (e.g. help links) open up in a new window
    response = response.replace(/<a /ig, "<a target='_blank' ");

    $('#hgTrackUiDialog').html("<div id='pop'>" + response + "</div>");
    $('#hgTrackUiDialog').dialog({
                               ajaxOptions: {
                                   // This doesn't work
                                   cache: true
                               },
                               resizable: popUpTrackDescriptionOnly,
                               height: 'auto',
                               width: 'auto',
                               minHeight: 200,
                               minWidth: 700,
                               modal: true,
                               closeOnEscape: true,
                               autoOpen: false,
                               buttons: { "OK": function() {
                                    if( ! popUpTrackDescriptionOnly )
                                        hgTrackUiPopCfgOk($('#pop'), popUpTrackName);
                                    $(this).dialog("close");
                               }},
                               open: function() {
                                    var subtrack = tdbIsSubtrack(trackDbJson[popUpTrackName]) ? popUpTrackName :"";  // If subtrack then vis rules differ
                                    popSaveAllVars = getAllVars( $('#pop'), subtrack );
                               },
                               close: function() {
                                   $('#hgTrackUiDialog').html("");  // clear out html after close to prevent problems caused by duplicate html elements
                                popUpTrackName = ""; //set to defaults
                                popUpTrackDescriptionOnly = false;
                                popSaveAllVars = null;
                               }
                           });
    if(popUpTrackDescriptionOnly) {
        var myWidth =  $(window).width() - 300;
        if(myWidth > 900)
            myWidth = 900;
        $('#hgTrackUiDialog').dialog("option", "maxWidth", myWidth);
        $('#hgTrackUiDialog').dialog("option", "width", myWidth);
        $('#hgTrackUiDialog').dialog('option' , 'title' , trackDbJson[popUpTrackName].shortLabel + " Track Description");
        $('#hgTrackUiDialog').dialog('open');
        var buttOk = $('button.ui-state-default');
        if($(buttOk).length == 1)
            $(buttOk).focus();
    } else {
        $('#hgTrackUiDialog').dialog('option' , 'title' , trackDbJson[popUpTrackName].shortLabel + " Track Settings");
        $('#hgTrackUiDialog').dialog('open');
    }
}

function handleUpdateTrackMap(response, status)
{
// Handle ajax response with an updated trackMap image (gif or png) and map.
//
// this.cmd can be used to figure out which menu item triggered this.
//
// e.g.: <IMG SRC = "../trash/hgtIdeo/hgtIdeo_hgwdev_larrym_61d1_8b4a80.gif" BORDER=1 WIDTH=1039 HEIGHT=21 USEMAP=#ideoMap id='chrom'>
    // Parse out new ideoGram url (if available)
    var a = /<IMG([^>]+SRC[^>]+id='chrom'[^>]*)>/.exec(response);
    if(a && a[1]) {
        b = /SRC\s*=\s*"([^")]+)"/.exec(a[1]);
        if(b[1]) {
            $('#chrom').attr('src', b[1]);
        }
    }
    var re = /<\!-- trackDbJson -->\n<script>var trackDbJson = ([\S\s]+)<\/script>\n<\!-- trackDbJson -->/m;
    a = re.exec(response);
    if(a && a[1]) {
        var json = eval("(" + a[1] + ")");
        if(json && json[this.id]) {
            var visibility = visibilityStrsOrder[json[this.id].visibility];
            var limitedVis;
            if(json[this.id].limitedVis)
                limitedVis = visibilityStrsOrder[json[this.id].limitedVis];
            if(this.newVisibility && limitedVis && this.newVisibility != limitedVis)
                alert("There are too many items to display the track in " + this.newVisibility + " mode.");
            updateVisibility(this.id, visibility);
        } else {
            showWarning("Invalid trackDbJson received from the server");
        }
    } else {
        showWarning("trackDbJson is missing from the response");
    }
    if(imageV2 && this.id && this.cmd && this.cmd != 'wholeImage' && this.cmd != 'selectWholeGene') {
          // Extract <TR id='tr_ID'>...</TR> and update appropriate row in imgTbl;
          // this updates src in img_left_ID, img_center_ID and img_data_ID and map in map_data_ID
          var id = this.id;
          if(this.loadingId) {
	          hideLoadingImage(this.loadingId);
          }
          var rec = trackDbJson[id];
          var str = "<TR id='tr_" + id + "'[^>]*>([\\s\\S]+?)</TR>";
          var reg = new RegExp(str);
          a = reg.exec(response);
          if(a && a[1]) {
//               $('#tr_' + id).html();
//               $('#tr_' + id).empty();
               $('#tr_' + id).html(a[1]);
               // XXXX move following to a shared method
               parseMap(null, true);
               $("map[name!=ideoMap]").each( function(t) { parseMap($(this, false));});
               initImgTblButtons();
               loadImgAreaSelect(false);
               // Do NOT reload context menu (otherwise we get the "context menu sticks" problem).
               // loadContextMenu($('#tr_' + id));
               if(trackImgTbl.tableDnDUpdate)
                   trackImgTbl.tableDnDUpdate();
          } else {
               showWarning("Couldn't parse out new image for id: " + id);
               //alert("Couldn't parse out new image for id: " + id+"BR"+response);  // Very helpful
          }
    } else {
        if(imageV2) {
            a= /<TABLE id=\'imgTbl\'[^>]*>([\S\s]+?)<\/TABLE>/.exec(response);
            if(a[1]) {
                // This doesn't work (much weirdness ensues).
                $('#imgTbl').html(a[1]);
            } else {
                showWarning("Couldn't parse out new image");
            }
        } else {
            a= /<IMG([^>]+SRC[^>]+id='trackMap[^>]*)>/.exec(response);
            // Deal with a is null
            if(a[1]) {
                    var b = /WIDTH\s*=\s*['"]?(\d+)['"]?/.exec(a[1]);
                    var width = b[1];
                    b = /HEIGHT\s*=\s*['"]?(\d+)['"]?/.exec(a[1]);
                    var height = b[1];
                    b = /SRC\s*=\s*"([^")]+)"/.exec(a[1]);
                    var src = b[1];
                    $('#trackMap').attr('src', src);
                    var obj = imgAreaSelect.data('imgAreaSelect');
                    if(width) {
                        trackImg.attr('width', width);
                    }
                    if(height) {
                        trackImg.attr('height', height);

                        // obj.setOptions({minHeight : height, maxHeight: height});
                        // obj.getOptions().minHeight = height;
        		// obj.getOptions().maxHeight = height;
                        // XXX doesn't work obj.options.maxHeight = height;
                        // This doesn't work: obj.windowResize();
                        // This works, but causes weird error IF we also change minHeight and maxHeight.
                        // jQuery(window).triggerHandler("resize");

                       // After much debugging, I found the best way to have imgAreaSelect continue to work
                       // was to reload it:
                       loadImgAreaSelect(false);
                       // XXX this doesn't work (i.e. does't make the re-sized row draggable under safari).
                       trackImgTbl.tableDnD();
                   }
            } else {
                showWarning("Couldn't parse out new image");
            }
        }
        // now pull out and parse the map.
        a = /<MAP id='map' Name=map>([\s\S]+)<\/MAP>/.exec(response);
        if(a[1]) {
            var $map = $('<map>' + a[1] + '</map>');
            parseMap($map, true);
        } else {
            showWarning("Couldn't parse out map");
        }
    }
    jQuery('body').css('cursor', '');
}

function handleViewImg(response, status)
{ // handles view image response, which must get new image without imageV2 gimmickery
    jQuery('body').css('cursor', '');
    var str = "<IMG[^>]*SRC='([^']+)'";
    var reg = new RegExp(str);
    a = reg.exec(response);
    if(a && a[1]) {
        if(window.open(a[1]) == null) {
            windowOpenFailedMsg();
        }
        return;
    }
    showWarning("Couldn't parse out img src");
}

function jumpButtonOnClick()
{
// onClick handler for the "jump" button.
// Handles situation where user types a gene name into the gene box and immediately hits the jump button,
// expecting the browser to jump to that gene.
    var gene = $('#suggest').val();
    var db = getDb();
    if(gene && db && gene.length > 0 && (getOriginalPosition() == getPosition() || getPosition().length == 0)) {
        pos = lookupGene(db, gene);
        if(pos) {
            setPosition(pos, null);
        } else {
            // turn this into a full text search.
            setPosition(gene, null);
        }
    }
    return true;
}

function remoteTrackCallback(rec)
// jsonp callback to load a remote track.
{
    if(rec.error) {
        alert("retrieval from remote site failed with error: " + rec.error)
    } else {
        var track = rec.track;
        $('#img_data_' + track).attr('style', '');
        $('#img_data_' + track).attr('height', rec.height);
        $('#img_data_' + track).attr('width', rec.width);
        $('#img_data_' + track).attr('src', rec.img);
        $('#td_data_' + track + ' > div').each(function(index) {
                                                   if(index == 1) {
                                                       var style = $(this).attr('style');
                                                       style = style.replace(/height:\s*\d+/i, "height:" + rec.height);
                                                       $(this).attr('style', style);
                                                   }
                                               });
        var style = $('#p_btn_' + track).attr('style');
        style = style.replace(/height:\s*\d+/i, "height:" + rec.height);
        $('#p_btn_' + track).attr('style', style);
    }
}

/////////////////////////////////////////////////////
// findTracks functions

function findTracksMdbVarChanged(obj)
{ // Ajax call to repopulate a metadata vals select when mdb var changes
    var newVar = $(obj).val();
    var a = /hgt_mdbVar(\d+)/.exec(obj.name); // NOTE must match METADATA_NAME_PREFIX in hg/hgTracks/searchTracks.c
    if(newVar != undefined && a && a[1]) {
        var num = a[1];
        $.ajax({
                   type: "GET",
                   url: "../cgi-bin/hgApi",
                   data: "db=" + getDb() +  "&cmd=metaDb&var=" + newVar,
                   trueSuccess: findTracksHandleNewMdbVals,
                   success: catchErrorOrDispatch,
                   error: errorHandler,
                   cache: true,
                   cmd: "hgt_mdbVal" + num, // NOTE must match METADATA_VALUE_PREFIX in hg/hgTracks/searchTracks.c
                   num: num
               });
    }
    //findTracksSearchButtonsEnable(true);
}

function findTracksHandleNewMdbVals(response, status)
// Handle ajax response (repopulate a metadata val select)
{   // TODO Support for returning whole control, not list of values.
    var list = eval(response);
    var ele = $('select[name=' + this.cmd + ']');
    ele.empty();
    ele.append("<option VALUE='Any'>Any</option>");
    for (var i = 0; i < list.length; i++) {
        var pair = list[i];
        ele.append("<option VALUE='" + pair[1] + "'>" + pair[0] + "</option>");
    }
    updateMetaDataHelpLinks(this.num);
}

// TODO: Replace findTracksMdbVarChanged() and findTracksHandleNewMdbVals()
function findTracksMdbVarChanged2(obj)
{ // Ajax call to repopulate a metadata vals select when mdb var changes
    var newVar = $(obj).val();
    var a = /hgt_mdbVar(\d+)/.exec(obj.name); // NOTE must match METADATA_NAME_PREFIX in hg/hgTracks/searchTracks.c
    if(newVar != undefined && a && a[1]) {
        var num = a[1];
        $.ajax({
                   type: "GET",
                   url: "../cgi-bin/hgApi",
                   data: "db=" + getDb() +  "&cmd=hgt_mdbVal" + num + "&var=" + newVar,
                   trueSuccess: findTracksHandleNewMdbVals2,
                   success: catchErrorOrDispatch,
                   error: errorHandler,
                   cache: true,
                   cmd: "hgt_mdbVal" + num, // NOTE must match METADATA_VALUE_PREFIX in hg/hgTracks/searchTracks.c
                   num: num
               });
    }
    //findTracksSearchButtonsEnable(true);
}

function findTracksHandleNewMdbVals2(response, status)
// Handle ajax response (repopulate a metadata val select)
{   // TODO Support for returning whole control, not list of values.
    //var list = eval(response);
    var td = $('td#' + this.cmd );
    if (td != undefined) {
        td.empty();
        td.append(response);
        var inp = $(td).find('.mdbVal');
        var tdIsLike = $('td#isLike'+this.num);
        if (inp != undefined && tdIsLike != undefined) {
            if ($(inp).hasClass('freeText')) {
                $(tdIsLike).text('contains');
            } else {
                $(tdIsLike).text('is');
            }
        }
    }
    updateMetaDataHelpLinks(this.num);
}

function searchKeydown(event)
{
    if (event.which == 13) {
        // Required to fix problem on IE and Safari where value of hgt_tSearch is "-" (i.e. not "Search").
        $("input[name=hgt_tsPage]").val(0);  // NOTE: must match TRACK_SEARCH_PAGER in hg/inc/searchTracks.h
        $('#trackSearch').submit();
        // This doesn't work with IE or Safari.
        // $('#searchSubmit').click();
    }
}

function findTracksChangeVis(seenVis)
{ // called by onchange of vis
    var visName = $(seenVis).attr('id');
    var trackName = visName.substring(0,visName.length - "_id".length)
    var hiddenVis = $("input[name='"+trackName+"']");
    var tdb = tdbGetJsonRecord(trackName);
    if($(seenVis).val() != "hide")
        $(hiddenVis).val($(seenVis).val());
    else {
        var selCb = $("input#"+trackName+"_sel_id");
        $(selCb).attr('checked',false);  // Can't set it to [] because that means default setting is used.  However, we are explicitly hiding this!
        $(seenVis).attr('disabled',true);  // Can't set it to [] because that means default setting is used.  However, we are explicitly hiding this!
        var needSel = (tdb.parentTrack != undefined);
        if (needSel) {
            var hiddenSel = $("input[name='"+trackName+"_sel']");
            $(hiddenSel).val('0');  // Can't set it to [] because that means default setting is used.  However, we are explicitly hiding this!
            $(hiddenSel).attr('disabled',false);
        }
        if(tdbIsSubtrack(tdb))
            $(hiddenVis).val("[]");
        else
            $(hiddenVis).val("hide");
    }
    $(hiddenVis).attr('disabled',false);

    $('input.viewBtn').val('View in Browser');
    //warn("Changed "+trackName+" to "+$(hiddenVis).val())
}

function findTracksClickedOne(selCb,justClicked)
{ // called by on click of CB and findTracksCheckAll()
    var selName = $(selCb).attr('id');
    var trackName = selName.substring(0,selName.length - "_sel_id".length)
    var hiddenSel = $("input[name='"+trackName+"_sel']");
    var seenVis = $('select#' + trackName + "_id");
    var hiddenVis = $("input[name='"+trackName+"']");
    var tr = $(selCb).parents('tr.found');
    var tdb = tdbGetJsonRecord(trackName);
    var needSel = (tdb.parentTrack != undefined);
    var shouldPack = tdb.canPack && tdb.kindOfParent == 0; // If parent then not pack but full
    if (shouldPack && tdb.shouldPack != undefined && !tdb.shouldPack)
        shouldPack = false;
    var checked = $(selCb).attr('checked');
    //warn(trackName +" selName:"+selName +" justClicked:"+justClicked +" hiddenSel:"+$(hiddenSel).attr('name') +" seenVis:"+$(seenVis).attr('id') +" hiddenVis:"+$(hiddenVis).attr('name') +" needSel:"+needSel +" shouldPack:"+shouldPack);

    // First deal with seenVis control
    if(checked) {
        $(seenVis).attr('disabled', false);
        if($(seenVis).attr('selectedIndex') == 0) {
            if(shouldPack)
                $(seenVis).attr('selectedIndex',3);  // packed
            else
                $(seenVis).attr('selectedIndex',$(seenVis).attr('length') - 1);
        }
    } else {
        $(seenVis).attr('selectedIndex',0);  // hide
        $(seenVis).attr('disabled', true );
    }

    // Deal with hiddenSel and hiddenVis so that submit does the right thing
    // Setting these requires justClicked OR seen vs. hidden to be different
    var setHiddenInputs = justClicked;
    if(!justClicked) {
        if(needSel)
            setHiddenInputs = (checked != ($(hiddenSel).val() == '1'));
        else if (checked)
            setHiddenInputs = ($(seenVis).val() != $(hiddenVis).val());
        else
            setHiddenInputs = ($(hiddenVis).val() != "hide" && $(hiddenVis).val() != "[]");
    }
    if(setHiddenInputs) {
        if(checked)
            $(hiddenVis).val($(seenVis).val());
        else if(tdbIsSubtrack(tdb))
            $(hiddenVis).val("[]");
        else
            $(hiddenVis).val("hide");
        $(hiddenVis).attr('disabled',false);

        if(needSel) {
            if(checked)
                $(hiddenSel).val('1');
            else
                $(hiddenSel).val('0');  // Can't set it to [] because that means default setting is used.  However, we are explicitly hiding this!
            $(hiddenSel).attr('disabled',false);
        }
    }

    // The "view in browser" button should be enabled/disabled
    if(justClicked) {
        $('input.viewBtn').val('View in Browser');
        findTracksCounts();
    }
}


function findTracksNormalize()
{ // Normalize the page based upon current state of all found tracks
    $('div#found').show()
    var selCbs = $('input.selCb');

    // All should have their vis enabled/disabled appropriately (false means don't update cart)
    $(selCbs).each( function(i) { findTracksClickedOne(this,false); });

    findTracksCounts();
}

function findTracksNormalizeWaitOn()
{ // Put up wait mask then Normalize the page based upon current state of all found tracks
    waitOnFunction( findTracksNormalize );
}

function findTracksCheckAll(check)
{ // Checks/unchecks all found tracks.
    var selCbs = $('input.selCb');
    $(selCbs).attr('checked',check);

    // All should have their vis enabled/disabled appropriately (false means don't update cart)
    $(selCbs).each( function(i) { findTracksClickedOne(this,false); });

    $('input.viewBtn').val('View in Browser');
    findTracksCounts();
    return false;  // Pressing button does nothing more
}

function findTracksCheckAllWithWait(check)
{
    waitOnFunction( findTracksCheckAll, check);
}

function findTracksSearchButtonsEnable(enable)
{ // Displays visible and checked track count
    var searchButton = $('input[name="hgt_tSearch"]'); // NOTE: must match TRACK_SEARCH in hg/inc/searchTracks.h
    var clearButton  = $('input.clear');
    if(enable) {
        $(searchButton).attr('disabled',false);
        $(clearButton).attr('disabled',false);
    } else {
        $(searchButton).attr('disabled',true);
        $(clearButton).attr('disabled',true);
    }
}

function findTracksCounts()
{// Displays visible and checked track count
    var counter = $('.selCbCount');
    if(counter != undefined) {
        var selCbs =  $("input.selCb");
        $(counter).text("("+$(selCbs).filter(":enabled:checked").length + " of " +$(selCbs).length+ " selected)");
    }
}

function findTracksClear()
{// Clear found tracks and all input controls
    var foundTracks = $('div#found');
    if(foundTracks != undefined)
        $(foundTracks).remove();
    $('input[type="text"]').val(''); // This will always be found
    //$('select.mdbVar').attr('selectedIndex',0); // Do we want to set the first two to cell/antibody?
    $('select.mdbVal').attr('selectedIndex',0); // Should be 'Any'
    $('select.groupSearch').attr('selectedIndex',0);
    $('select.typeSearch').attr('selectedIndex',0);
    //findTracksSearchButtonsEnable(false);
    return false;
}

function findTracksSortNow(obj)
{// Called by radio button to sort tracks
    if( $('#sortIt').length == 0 )
        $('form#trackSearch').append("<input TYPE=HIDDEN id='sortIt' name='"+$(obj).attr('name')+"' value='"+$(obj).val()+"'>");
    else
        $('#sortIt').val($(obj).val());
    $('#searchSubmit').click();
    return true;
}

function findTracksPage(pageVar,startAt)
{// Called by radio button to sort tracks
    var pager = $("input[name='"+pageVar+"']");
    if( $(pager).length == 1)
        $(pager).val(startAt);

    // FIXME: Remove this code if sving settings on paging is not wanted
    // How to hold onto selected tracks?
    // There are 2 separate forms.  Scrape named inputs from searchResults form and dup them on trackSearch?
    var inp = $('form#searchResults').find('input:hidden').not(':disabled').not("[name='hgsid']");
    if($(inp).length > 0) {
        $(inp).appendTo('form#trackSearch');
        $('form#trackSearch').attr('method','POST'); // Must be post to avoid url too long  NOTE: probably needs to be post anyway
    }
    // FIXME: Remove this code if sving settings on paging is not wanted

    $('#searchSubmit').click();
    return false;
}

/////////////////////////////////////////////////////

function delSearchSelect(obj, rowNum)
{
    obj = $(obj);
    $("input[name=hgt_tsDelRow]").val(rowNum);  // NOTE: Must match TRACK_SEARCH_DEL_ROW in hg/inc/searchTracks.h
    return true;
}

function addSearchSelect(obj, rowNum)
{
    obj = $(obj);
    $("input[name=hgt_tsAddRow]").val(rowNum);  // NOTE: Must match TRACK_SEARCH_ADD_ROW in hg/inc/searchTracks.h
    return true;
}

function updateMetaDataHelpLinks(index)
{
// update the metadata help links based on currently selected values.
// If index == 0 we update all help items, otherwise we only update the one == index.
    var i;
    var db = getDb();
    var disabled = {
        'accession': 1,
        'dataType': 1,
        'dataVersion': 1,
        'geoSample': 1,
        'grant': 1,
        'lab': 1,
        'labExpId': 1,
        'labVersion': 1,
        'origAssembly': 1,
        'replicate': 1,
        'setType': 1,
        'softwareVersion': 1,
        'subId': 1,
        'view': 1
    }
    for(i=1;true;i++) {
        var span = $("#helpLink" + i);
        if(span.length > 0) {
            if(index == 0 || i == index) {
                var val = $("select[name='hgt_mdbVar" + i + "']").val();  // NOTE must match METADATA_NAME_PREFIX in hg/hgTracks/searchTracks.c
                var text = $("select[name='hgt_mdbVar" + i + "'] option:selected").text();
                var str;
                span.empty();
                if(val == 'cell') {
                    if(db.substr(0, 2) == "mm") {
                        str = "../ENCODE/cellTypesMouse.html";
                    } else {
                        str = "../ENCODE/cellTypes.html";
                    }
                } else if (val.toLowerCase() == 'antibody') {
                    str = "../ENCODE/antibodies.html";
                } else {
                    str = "../ENCODE/otherTerms.html#" + val;
                }
                if(typeof(disabled[val]) == 'undefined')
                    span.html("<a target='_blank' title='detailed descriptions of terms' href='" + str + "'>" + text + "</a>");
                else
                    span.empty();
            }
        } else {
            return;
        }
    }
}

function windowOpenFailedMsg()
{
    alert("Your web browser prevented us from opening a new window.\n\nPlease change your browser settings to allow pop-up windows from " + document.domain + ".");
}

function updateVisibility(track, visibility)
{
// Updates visibility state in trackDbJson and any visible elements on the page.
// returns true if we modify at least one select in the group list
    var rec = trackDbJson[track];
    var selectUpdated = false;
    $("select[name=" + track + "]").each(function(t) {
                                          $(this).val(visibility);
                                          selectUpdated = true;
                                      });
    if(rec) {
        rec.localVisibility = visibility;
    }
    return selectUpdated;
}
