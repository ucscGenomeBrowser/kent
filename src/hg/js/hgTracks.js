// Javascript for use in hgTracks CGI
// $Header: /projects/compbio/cvsroot/kent/src/hg/js/hgTracks.js,v 1.45 2009/11/12 23:38:41 tdreszer Exp $

var debug = false;
var originalPosition;
var originalSize;
var originalCursor;
var clickClipHeight;
var revCmplDisp;
var insideX;
var startDragZoom = null;
var newWinWidth;
var imageV2 = false;
var imgBoxPortal = false;
var blockUseMap = false;
var mapHtml;
var mapItems;
var trackImg;               // jQuery element for the track image
var trackImgTbl;            // jQuery element used for image table under imageV2
var imgAreaSelect;          // jQuery element used for imgAreaSelect
var originalImgTitle;
var autoHideSetting = true; // Current state of imgAreaSelect autoHide setting
var selectedMapItem;        // index of currently choosen map item (via context menu).
var browser;                // browser ("msie", "safari" etc.)

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

function updatePosition(img, selection, singleClick)
{
    // singleClick is true when the mouse hasn't moved (or has only moved a small amount).
    var chromName = document.getElementById("hgt.chromName").value;
    var winStart = parseInt(document.getElementById("hgt.winStart").value);
    var winEnd = parseInt(document.getElementById("hgt.winEnd").value);
    if(typeof imgBoxPortalStart != "undefined" && imgBoxPortalStart) {
        winStart = imgBoxPortalStart;
        winEnd = imgBoxPortalEnd;
    }
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
	newPos = chromName + ":" + commify(newStart) + "-" + commify(newEnd);
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
        newPos = chromName + ":" + commify(newStart) + "-" + commify(newEnd);
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
    if(checkPosition(img, selection)) {
        updatePosition(img, selection, false);
        jQuery('body').css('cursor', originalCursor);
    } else {
        jQuery('body').css('cursor', 'not-allowed');
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
    loadImgAreaSelect(true);

    // Don't load contextMenu if jquery.contextmenu.js hasn't been loaded
    if(trackImg && jQuery.fn.contextMenu) {
        if(imageV2) {
            $("map[name!=ideoMap]").each( function(t) { parseMap($(this,false));});
        } else {
            // XXXX still under debate whether we have to remove the map
            parseMap($('#map'),true);
            mapHtml = $('#map').html();
            $('#map').empty();
        }

        originalImgTitle = trackImg.attr("title");
        if(imageV2) {
            loadContextMenu(trackImgTbl);
            $(".trDraggable,.nodrop").each( function(t) { loadContextMenu($(this)); });
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
            $("tr[id^='"+prefix+"-']").hide();
        } else {
            newSrc = oldSrc.replace("/add", "/remove");
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
            alert("chromIdeo(): failed to register "+this.id);
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
            //alert("chromIdeo("+chr.name+") selected range (pix):"+pxDown+"-"+pxUp+" chrom range (pix):"+chr.left+"-"+chr.right+" chrom range (bp):"+chr.name+":"+chr.beg+"-"+chr.end);
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
                    //else alert("chromIdeo("+chr.name+") NOT WITHIN HORIZONTAL RANGE\n selected range (pix):"+pxDown+"-"+pxUp+" chrom range (pix):"+chr.left+"-"+chr.right);
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
                    if(dontAsk || confirm("Jump to new position:\n\n"+chr.name+":"+commify(selRange.beg)+"-"+commify(selRange.end)+" size:"+commify(selRange.width)) ) {
                        setPositionByCoordinates(chr.name, selRange.beg, selRange.end)
                        $('.cytoBand').mousedown( function(e) { return false; }); // Stop the presses :0)
                        document.TrackHeaderForm.submit();
                        return true; // Make sure the setTimeout below is not called.
                    }
                }
            }
            //else alert("chromIdeo("+chr.name+") NOT WITHIN VERTICAL RANGE\n selected range (pix):"+pxDown+"-"+pxUp+" chrom range (pix):"+chr.left+"-"+chr.right+"\n cytoTop-Bottom:"+chr.top +"-"+chr.bottom);
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
    var portalWidth = $(pan).width();
    //var ie=( $.browser.msie == true );

    initialize();

    function initialize(){

        pan.css( 'cursor', 'w-resize');//'move');

        panAdjustHeight(prevX);

        pan.mousedown(function(e){
            if(mouseIsDown == false) {
                mouseIsDown = true;

                mouseDownX = e.clientX;
                //if(ie) {  // Doesn't work (yet?)
                //    pic.ondrag='panner';
                //    pic.ondragend='panMouseUp';
                //} else {
                    $(document).bind('mousemove',panner);
                    $(document).bind( 'mouseup', panMouseUp);  // Will exec only once
                //}
                return false;
            }
        });
    }

    function panner(e) {
        //if(!e) e = window.event;
        if ( mouseIsDown ) {
            var relativeX = (e.clientX - mouseDownX);

            // Would be good to:
            // 1) Slow scroll near edge
            // 2) Allow scrolling past edge
            // 3) ajax to new image when scrolled off edge
            if(relativeX != 0) {
                blockUseMap = true;
                // Remeber that offsetX (prevX) is negative
                if ( (prevX + relativeX) >= leftLimit ) { // scrolled all the way to the left
                    newX = leftLimit;
                    // get new image?
                } else if ( (prevX + relativeX) < (imgBoxPortalWidth - imgBoxWidth + leftLimit) ) { // scrolled all the way to the right
                    newX = (imgBoxPortalWidth - imgBoxWidth + leftLimit);
                    // get new image?
                } else
                    newX = prevX + relativeX;

                $(".panImg").css( {'left': newX.toString() + "px" });
                // Now is the time to get left-right, then march through data images to trim horizontal
                panUpdatePosition(newX);
            }
        }
    }
    function panMouseUp(e) {  // Must be a separate function instead of pan.mouseup event.
        //if(!e) e = window.event;
        if(mouseIsDown) {

            if(prevX != newX)
                panAdjustHeight(newX);
            //if(prevX != newX) {
                prevX = newX;
                //if(ie) {
                //    pic.ondrag=''; pic.ondragend='';
                //} else {
                    $(document).unbind('mousemove',panner);
                    $(document).unbind('mouseup',panMouseUp);
                //}
                mouseIsDown = false;
                setTimeout('blockUseMap=false;',50); // Necessary incase the selectEnd was over a map item. select takes precedence.
            //}
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
                    var btn = $("#img_btn_"+imgId[2]);
                    if( btn.length > 0) {
                        $(btn).parent().height( span.bottom - span.top + titlePx);
                        $(btn).css( {'top': top.toString() + "px" });
                    }

                }
            }
        });
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
    if($("FORM").length > 0) {
        $('a,area').not("[href*='#']").not("[target]").click(function(i) {
            if(blockUseMap==true) {
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
    }
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
    if($('img#chrom').length == 1) {
        if($('.cytoBand').length > 1) {
            $('img#chrom').chromDrag();
        }
    }
});

////////// Attempts to get back button to work with us
//window.onload = function () {
//    // Requires "Signed Scripts" and "UniversalBrowserRead" http://www.mozilla.org/projects/security/components/signed-scripts.html
//    if(window.history.next != undefined && window.history.next.length > 10) {
//	alert("Not at the end of time.");
//    } else
//	alert("At the end of time.");
//}

function rulerModeToggle (ele)
{
    autoHideSetting = !ele.checked;
    var obj = imgAreaSelect.data('imgAreaSelect');
    obj.setOptions({autoHide : autoHideSetting});
}

function findMapItem(e)
{
// Find mapItem for given event
    var x,y;
    if(imageV2) {
        // It IS appropriate to use coordinates relative to the img WHEN we have a hit in the right-hand side, but NOT
        // when we have a hit in the left hand elements (which do not have relative coordinates).
        // XXXX still trying to figure this out.
        var pos = $(e.target).position();
        if(e.target.tagName == "IMG") {
            // alert("img: x: " + x + ", y:" + y);
            // alert("pageX: " + e.pageX + "; offsetLeft: " + pos.left);
            x = e.pageX - pos.left;
            y = e.pageY - pos.top;
            // alert("x: " + x + "; y: " + y);
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
    for(var i=0;i<mapItems.length;i++)
    {
        if(mapItems[i].obj && e.target === mapItems[i].obj) {
            // This never occurs under IE
            // console.log("Found match by objects comparison");
            retval = i;
            break;
        } else if (!imageV2 || browser == "msie") {
            //
            // We start falling through to here under safari under imageV2 once something has been modified
            if(mapItems[i].r.contains(x, y)) {
                retval = i;
                break;
            }
        }
    }
    // showWarning(x + " " + y + " " + retval + " " + $(e.target).attr('src'));
    // console.log("findMapItem:", e.clientX, e.clientY, x, y, pos.left, pos.top, retval, mapItems.length, e.target.tagName);
    // console.log(e.clientX, pos);
    return retval;
}

function mapEvent(e)
{
    var i = findMapItem(e);
    if(i >= 0)
    {
        e.target.title = mapItems[i].title;
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
        var i = findMapItem(e);
        if(i >= 0)
        {
            // XXXX Why does href get changed to "about://" on IE?
            window.location = mapItems[i].href;
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
    if(cmd == 'selectWholeGene') {
            // bring whole gene into view
            var href = mapItems[selectedMapItem].href;
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
                var newPosition = setPositionByCoordinates(chrom, chromStart, chromEnd);
                if(browser == "safari" || imageV2) {
                    // See comments below on safari.
                    // We need to parse out more stuff to support imageV2 via ajax, but it's probably possible.
                    jQuery('body').css('cursor', 'wait');
                    document.TrackHeaderForm.submit();
                } else {
                    jQuery('body').css('cursor', '');
                    $.ajax({
                               type: "GET",
                               url: "../cgi-bin/hgTracks",
                               data: "hgt.trackImgOnly=1&hgt.ideogramToo=1&position=" + newPosition + "&hgsid=" + getHgsid(),
                               dataType: "html",
                               trueSuccess: handleUpdateTrackMap,
                               success: catchErrorOrDispatch,
                               cmd: cmd,
                               cache: false
                           });
                }
            }
    } else if (cmd == 'hgTrackUi') {
        // data: ?
        jQuery('body').css('cursor', 'wait');
        $.ajax({
                   type: "POST",
                   url: "../cgi-bin/hgTrackUi?ajax=1&g=" + mapItems[selectedMapItem].id + "&hgsid=" + getHgsid(),
                   dataType: "html",
                   trueSuccess: handleTrackUi,
                   success: catchErrorOrDispatch,
                   cache: true
               });
    } else if (cmd == 'dragZoomMode') {
        autoHideSetting = true;
        var obj = imgAreaSelect.data('imgAreaSelect');
        obj.setOptions({autoHide : true, movable: false});
    } else if (cmd == 'hilightMode') {
        autoHideSetting = false;
        var obj = imgAreaSelect.data('imgAreaSelect');
        obj.setOptions({autoHide : false, movable: true});
    } else if (cmd == 'viewImg') {
        window.open(trackImg.attr('src'));
    } else if (cmd == 'openLink') {
        // XXXX This is blocked by Safari's popup blocker (without any warning message).
        window.open(mapItems[selectedMapItem].href);
    } else {
        var id = mapItems[selectedMapItem].id;
        var rec = trackDbJson[id];
        if(rec && rec.parentTrack) {
            // currently we fall back to the parentTrack
            id = rec.parentTrack;
        }
        $("select[name=" + id + "]").each(function(t) {
            $(this).val(cmd);
                });
        if(imageV2 && cmd == 'hide')
        {
            // Tell remote cart what happened (to keep them in sync with us).
            setCartVar(id, cmd);
            $('#tr_' + id).remove();
            loadImgAreaSelect(false);
        } else if (browser == "safari") {
            // Safari has the following bug: if we update the local map dynamically, the changes don't get registered (even
            // though if you look in the DOM the changes are there); so we have to do a full form submission when the
            // user changes visibility settings.
            jQuery('body').css('cursor', 'wait');
            document.TrackForm.submit();
        } else {
            var data = "hgt.trackImgOnly=1&" + id + "=" + cmd + "&hgsid=" + getHgsid();
            if(imageV2) {
	        data += "&hgt.trackNameFilter=" + id;
            }
            jQuery('body').css('cursor', 'wait');
            $.ajax({
                       type: "GET",
                       url: "../cgi-bin/hgTracks",
                       data: data,
                       dataType: "html",
                       trueSuccess: handleUpdateTrackMap,
                       success: catchErrorOrDispatch,
                       cmd: cmd,
                       cache: false
                   });
        }
    }
}

function loadContextMenu(img)
{
    var menu = img.contextMenu(
        function() {
            var menu = [];
            var selectedImg = " <img src='../images/Green_check.png' height='10' width='10' />";
            var done = false;
            if(selectedMapItem >= 0)
            {
                var href = mapItems[selectedMapItem].href;
                var isGene = href.match("hgGene");
                var isHgc = href.match("hgc");
                var rec = trackDbJson[id];
                var id = mapItems[selectedMapItem].id;
                var rec = trackDbJson[id];
                if(rec && rec.parentTrack) {
                    // currently we fall back to the parentTrack
                    id = rec.parentTrack;
                }
                // XXXX what if select is not available (b/c trackControlsOnMain is off)?
                // Move functionality to a hidden variable?
                var select = $("select[name=" + id + "]");
                var cur = select.val();
                if(cur) {
                    select.children().each(function(index, o) {
                                               var title = $(this).val();
                                               var str = title;
                                               if(title == cur) {
                                                   str += selectedImg;
                                               }
                                               var o = new Object();
                                               o[str] = {onclick: function (menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, title); return true;}};
                                               menu.push(o);
                                           });
                    menu.push($.contextMenu.separator);
                    var o = new Object();
                    if(isGene || isHgc) {
                        var title = mapItems[selectedMapItem].title || "feature";
                        o["Zoom to " +  title] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "selectWholeGene"); return true; }};
                        o["Open Link in New Window"] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "openLink"); return true; }};
                    } else {
                        o[mapItems[selectedMapItem].title] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "hgTrackUi"); return true; }};
                    }
                    menu.push(o);
                    done = true;
                } else {
                    // XXXX currently dead code
                    if(rec) {
                        // XXXX check current state from a hidden variable.
                        var visibilityStrsOrder = new Array("hide", "dense", "full", "pack", "squish");
                        var visibilityStrs = new Array("hide", "dense", "squish", "pack", "full");
                        for (i in visibilityStrs) {
                            // XXXX use maxVisibility and change hgTracks so it can hide subtracks
                            var o = new Object();
                            var str = visibilityStrs[i];
                            if(rec.canPack || (str != "pack" && str != "squish")) {
                                if(str == visibilityStrsOrder[rec.visibility]) {
                                    str += selectedImg;
                                }
                                o[str] = {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, visibilityStrs[i]); return true; }};
                                menu.push(o);
                            }
                        }
                        done = true;
                    }
                }
            }
            if(!done) {
                var str = "drag-and-zoom mode";
                var o = new Object();
                if(autoHideSetting) {
                    str += selectedImg;
                    // menu[str].className = 'context-menu-checked-item';
                }
                o[str] = { onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "dragZoomMode"); return true; }};
                menu.push(o);
                o = new Object();
                str = "hilight mode";
                if(!autoHideSetting) {
                    str += selectedImg;
                }
                o[str] = { onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "hilightMode"); return true; }};
                menu.push(o);
                menu.push({"view image": {onclick: function(menuItemClicked, menuObject) { contextMenuHit(menuItemClicked, menuObject, "viewImg"); return true; }}});
            }
            return menu;
        },
        {
            beforeShow: function(e) {
                // console.log(mapItems[selectedMapItem]);
                selectedMapItem = findMapItem(e);
                // XXXX? blockUseMap = true;
            },
            hideCallback: function() {
                // this doesn't work
                alert("hideCallback");
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
                ele.children().each(function() {
                                      mapItems[i++] = {
                                          r : new Rectangle(this.coords),
                                          href : this.href,
                                          title : this.title,
                                          id : this.id,
                                          obj : this
                                      };
                                  });
    }
    return mapItems;
}

function showWarning(str)
{
    $("#warningText").text(str);
    $("#warning").show();
}

function catchErrorOrDispatch(obj,status)
{
    if(obj.err)
    {
        showWarning(obj.err);
        jQuery('body').css('cursor', '');
    }
    else
        this.trueSuccess(obj,status);
}

function handleTrackUi(response, status)
{
// Take html from hgTrackUi and put it up as a modal dialog.
    $('#hgTrackUiDialog').html(response);
    $('#hgTrackUiDialog').dialog({
                               ajaxOptions: {
                                   // This doesn't work
                                   cache: true
                               },
                               resizable: true,
                               bgiframe: true,
                               height: 450,
                               width: 600,
                               modal: true,
                               closeOnEscape: true,
                               autoOpen: false,
                               title: "Track Settings",
                               close: function(){
                                   // clear out html after close to prevent problems caused by duplicate html elements
                                   $('#hgTrackUiDialog').html("");
                               }
                           });
    jQuery('body').css('cursor', '');
    $('#hgTrackUiDialog').dialog('open');
}

function handleUpdateTrackMap(response, status)
{
// Handle ajax response with an updated trackMap image (gif or png) and map.
//
// this.cmd can be used to figure out which menu item triggered this.
//
//    var a= /(<IMG SRC\s*=\s*([^"]+)"[^>]+id='trackMap'\s*>/.exec(response);
// <IMG SRC = "../trash/hgtIdeo/hgtIdeo_hgwdev_larrym_61d1_8b4a80.gif" BORDER=1 WIDTH=1039 HEIGHT=21 USEMAP=#ideoMap id='chrom'>
    // Parse out new ideoGram url (if available)
    var a = /<IMG([^>]+SRC[^>]+id='chrom'[^>]*)>/.exec(response);
    if(a && a[1]) {
        b = /SRC\s*=\s*"([^")]+)"/.exec(a[1]);
        if(b[1]) {
            $('#chrom').attr('src', b[1]);
        }
    }
    if(imageV2 && this.cmd && this.cmd != 'selectWholeGene') {
          // Extract <TR id='tr_ID' class='trDraggable'>...</TR> and update appropriate row in imgTbl;
          // this updates src in img_left_ID, img_center_ID and img_data_ID and map in map_data_ID
          var id = mapItems[selectedMapItem].id;
          var rec = trackDbJson[id];
          if(rec && rec.parentTrack) {
              // currently we fall back to the parentTrack
              id = rec.parentTrack;
          }
          var str = "<TR id='tr_" + id + "' class='trDraggable'>([\\s\\S]+?)</TR>";
          var reg = new RegExp(str);
          a = reg.exec(response);
          if(a && a[1]) {
//               $('#tr_' + id).html();
//               $('#tr_' + id).empty();
               $('#tr_' + id).html(a[1]);
               // XXXX move following to a shared method
               parseMap(null, true);
               $("map[name!=ideoMap]").each( function(t) { parseMap($(this, false));});
               loadImgAreaSelect(false);
               // Do NOT reload context menu (otherwise we get the "context menu sticks" problem).
               // loadContextMenu($('#tr_' + id));
          } else {
               showWarning("Couldn't parse out new image");
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
                       // XXX this doesn't work (i.e. does't make the re-sized row draggable).
                       jQuery.tableDnD.updateTables();
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
