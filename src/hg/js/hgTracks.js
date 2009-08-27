// Javascript for use in hgTracks CGI
// $Header: /projects/compbio/cvsroot/kent/src/hg/js/hgTracks.js,v 1.38 2009/08/27 00:15:30 tdreszer Exp $

var debug = false;
var originalPosition;
var originalSize;
var clickClipHeight;
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

function updatePosition(img, selection, singleClick)
{
    // singleClick is true when the mouse hasn't moved (or has only moved a small amount).
    var insideX = parseInt(document.getElementById("hgt.insideX").value);
    var revCmplDisp = parseInt(document.getElementById("hgt.revCmplDisp").value) == 0 ? false : true;
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
    if(autoHideSetting && (selection.event.pageX >= (imgOfs.left - slop)) && (selection.event.pageX < (imgOfs.left + imgWidth + slop))
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
            $(".trDraggable,.nodrop").each( function(t) {
                                                $(this).mousemove(
                                                    function (e) {
                                                        mapEvent(e);
                                                    }
                                                );
                                                $(this).mousedown(
                                                    function (e) {
                                                        mapMouseDown(e);
                                                    }
                                                );
                                            });
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
        }

        clickClipHeight = parseInt(rulerEle.value);
        newWinWidth = parseInt(document.getElementById("hgt.newWinWidth").value);

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
                    if(center.length > 0)
                        titlePx = $(center).parent().height();
                    var side = $("#img_side_"+imgId[2]);
                    if( side.length > 0) {
                        $(side).parent().height( span.bottom - span.top + titlePx);
                        top += titlePx;
                        $(side).css( {'top': top.toString() + "px" });
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
});

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
            // console.log("Found match by objects comparison");
            retval = i;
            break;
        } else {
            // XXXX !imageV2???
            if(mapItems[i].r.contains(x, y)) {
                retval = i;
                break;
            }
        }
    }
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
    setTimeout(function() { contextMenuHitFinish(menuItemClicked, menuObject, cmd); }, 10);
}

function contextMenuHitFinish(menuItemClicked, menuObject, cmd)
{
// dispatcher for context menu hits
    if(menuObject.shown) {
        // XXXX This doesn't work; i.e. I still occassionally get a menu that doesn't get hidden.
        // console.log("Spinning: menu is still shown");
        setTimeout(function() { contextMenuHitFinish(menuItemClicked, menuObject, cmd); }, 50);
    }
    if(cmd == 'selectWholeGene') {
            // bring whole gene into view
            var href = mapItems[selectedMapItem].href;
            var chrom, chromStart, chromEnd;
            var a = /hgg_chrom=(\w+)&/.exec(href);
            if(a) {
                chrom = a[1];
                a = /hgg_start=(\d+)/.exec(href);
                // XXXX does chromStart have to be incremented by 1?
                chromStart = a[1];
                a = /hgg_end=(\d+)/.exec(href);
                chromEnd = a[1];
            } else {
                // a = /hgc.*\W+c=(\w+)/.exec(href);
                a = /hgc.*\W+c=(\w+)/.exec(href);
                chrom = a[1];
                a = /o=(\d+)/.exec(href);
                chromStart = parseInt(a[1]) + 1;
                a = /t=(\d+)/.exec(href);
                chromEnd = parseInt(a[1]);
            }
            var newPosition = setPositionByCoordinates(chrom, chromStart, chromEnd);
            if(imageV2) {
                // XXXX I don't know how to collapse down to just this portion of the display (is that possible?)
                document.TrackHeaderForm.submit();
            } else {
                jQuery('body').css('cursor', 'wait');
                $.ajax({
                           type: "GET",
                           url: "../cgi-bin/hgTracks",
                           data: "hgt.trackImgOnly=1&hgt.ideogramToo=1&position=" + newPosition + "&hgsid=" + getHgsid(),
                           dataType: "html",
                           trueSuccess: handleUpdateTrackMap,
                           success: catchErrorOrDispatch,
                           cache: false
                       });
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
        window.open(mapItems[selectedMapItem].href);
    } else {
        $("select[name=" + mapItems[selectedMapItem].id + "]").each(function(t) {
            $(this).val(cmd);
                });
        if(imageV2 && cmd == 'hide')
        {
            $('#tr_' + mapItems[selectedMapItem].id).remove();
        } else {
            if(imageV2) {
	        document.TrackForm.submit();
            } else {
                jQuery('body').css('cursor', 'wait');
                $.ajax({
                           type: "GET",
                           url: "../cgi-bin/hgTracks",
                           data: "hgt.trackImgOnly=1&" + mapItems[selectedMapItem].id + "=" + cmd + "&hgsid=" + getHgsid(),
                           dataType: "html",
                           trueSuccess: handleUpdateTrackMap,
                           success: catchErrorOrDispatch,
                           cache: false
                       });
            }
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
                // XXXX what if select is not available (b/c trackControlsOnMain is off)?
                // Move functionality to a hidden variable?
                var select = $("select[name=" + mapItems[selectedMapItem].id + "]");
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
            },
            hideCallback: function() {
                // this doesn't work
                alert("hideCallback");
            }
        });
    return;
}

function parseMap(map, reset)
{
// Parse the jQuery map object into returned mapItems array (map needn't be the element attached to current document).
        if(reset || !mapItems) {
            mapItems = new Array();
        }
        var i = mapItems.length;
        map.children().each(function() {
                                      mapItems[i++] = {
                                          r : new Rectangle(this.coords),
                                          href : this.href,
                                          title : this.title,
                                          id : this.id,
                                          obj : this
                                      };
                                  });
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
           }
    } else {
        showWarning("Couldn't parse out new image");
    }
    // now pull out and parse the map.
    a = /<MAP id='map' Name=map>([\s\S]+)<\/MAP>/.exec(response);
    if(a[1]) {
        var $map = $('<map>' + a[1] + '</map>');
        parseMap($map, true);
    } else {
        showWarning("Couldn't parse out map");
    }
    jQuery('body').css('cursor', '');
}
