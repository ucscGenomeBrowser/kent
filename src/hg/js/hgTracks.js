// Javascript for use in hgTracks CGI
// $Header: /projects/compbio/cvsroot/kent/src/hg/js/hgTracks.js,v 1.32 2009/08/13 06:47:10 larrym Exp $

var debug = false;
var originalPosition;
var originalSize;
var clickClipHeight;
var startDragZoom = null;
var mapHtml;
var mapItems;
var newWinWidth;
var img;
var imgTitle;
var imageV2 = false;
var imgBoxPortal = false;
var blockUseMap = false;
var autoHideSetting = true;
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
    setTimeout('blockUseMap=false; loadContextMenu(img);',50); // Necessary incase the selectEnd was over a map item. select takes precedence.
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
        img = $('#img_data_ruler');
        mapItems = new Array();
        var i = 0;
        $('#map').children().each(function() {
                                      mapItems[i++] = {
                                          coords : this.coords,
                                          href : this.href,
                                          title : this.title,
                                          id : this.id
                                      };
                                  });
        mapHtml = $('#map').html();
        $('#map').empty();
                        
        if(img==undefined || img.length == 0) {  // Revert to old imageV1
            img = $('#trackMap');
            imgHeight = jQuery(img).height();
            imgWidth  = jQuery(img).width();
        } else {
            imageV2   = true;
            imgHeight = $('#imgTbl').height();
            imgWidth  =  $('#td_data_ruler').width();
        }
        img.mousemove(
                function (e) {
                    mapEvent(e);
                }
            );
        img.mousedown(
                function (e) {
                    mapMouseDown(e);
                }
            );
        imgTitle = img.attr("title");
        loadContextMenu(img);
        // http://www.trendskitchens.co.nz/jquery/contextmenu/
        clickClipHeight = parseInt(rulerEle.value);
                newWinWidth = parseInt(document.getElementById("hgt.newWinWidth").value);

        img.imgAreaSelect({ selectionColor: 'blue', outerColor: '',
            minHeight: imgHeight, maxHeight: imgHeight,
            onSelectStart: selectStart, onSelectChange: selectChange, onSelectEnd: selectEnd,
            autoHide: autoHideSetting, movable: false,
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
    //var ie=( $.browser.msie == true );

    initialize();

    function initialize(){

        pan.css( 'cursor', 'w-resize');//'move');

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
        //if(!e) e = window.event;
        if(mouseIsDown) {

            prevX = newX;
            //if(ie) {
            //    pic.ondrag='';
            //    pic.ondragend='';
            //} else {
                $(document).unbind('mousemove',panner);
                $(document).unbind('mouseup',panMouseUp);
            //}
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
    if($("FORM").length > 0) {
        $('a,area').not("[href*='#']").filter("[target='foo']").click(function(i) {
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
    var obj = jQuery(img).data('imgAreaSelect');
    obj.setOptions({autoHide : autoHideSetting});
}

function findMapItem(e)
{
    var x = e.pageX - e.target.offsetLeft;
    var y = e.pageY - e.target.offsetTop;
    var retval = -1;
    for(var i=0;i<mapItems.length;i++)
    {
        var coords = mapItems[i].coords.split(",");
        if(x >= coords[0] && x <= coords[2] && y >= coords[1] && y <= coords[3]) {
            retval = i;
        }
    }
    return retval;
}

function mapEvent(e)
{
    var now = new Date();
    var i = findMapItem(e);
    if(i >= 0)
    {
        e.target.title = mapItems[i].title;
    } else {
        // XXXX this doesn't work.
        // $('#myMenu').html("<ul id='myMenu' class='contextMenu'><li class='edit'><a href='#img'>Get Image</a></li></ul>");
        e.target.title = imgTitle;
    }
    lastSet = now;
}

function mapMouseDown(e)
{
    // XXXX The rightclick logic works (but I don't know why).
    var rightclick = e.which ? (e.which == 3) : (e.button == 2);
    if(rightclick)
    {
        return false;
    } else {
        var i = findMapItem(e);
        if(i >= 0)
        {
            window.location = mapItems[i].href;
        }
        return true;
    }
}

function contextMenuHit(ele, cmd)
{
    $("select[name=" + mapItems[selectedMapItem].id + "]").each(function(t) {
        $(this).val(cmd);
    });
    document.TrackForm.submit();
}

function loadContextMenu(img)
{
        img.contextMenu("myMenu", {
                            menuStyle: {
                                width: '100%'
                            },
                            bindings: {
                                'hide': function(t) {
                                    contextMenuHit(t, 'hide');
                                },
                                'dense': function(t) {
                                    contextMenuHit(t, 'dense');
                                },
                                'pack': function(t) {
                                    contextMenuHit(t, 'pack');
                                },
                                'squish': function(t) {
                                    contextMenuHit(t, 'squish');
                                },
                                'full': function(t) {
                                    contextMenuHit(t, 'full');
                                },
                                'dragZoomMode': function(t) {
                                    autoHideSetting = true;
                                    var obj = jQuery(img).data('imgAreaSelect');
                                    obj.setOptions({autoHide : true, movable: false});
                                },
                                'hilightMode': function(t) {
                                    autoHideSetting = false;
                                    var obj = jQuery(img).data('imgAreaSelect');
                                    obj.setOptions({autoHide : false, movable: true});
                                },
                                'getImg': function(t) {
                                    window.open(img.attr('src'));
                                },
                                'selectWholeGene': function(t) {
                                    // bring whole gene into view
                                    var href = mapItems[selectedMapItem].href;
                                    var chrom, chromStart, chromEnd;
                                    var a = /hgg_chrom=(\w+)&/(href);
                                    chrom = a[1];
                                    a = /hgg_start=(\d+)&/(href);
                                    chromStart = a[1];
                                    a = /hgg_end=(\d+)&/(href);
                                    chromEnd = a[1];
                                    setPosition(chrom + ":" + chromStart + "-" + chromEnd, null);
                                    document.TrackHeaderForm.submit();
                                },
                                'hgTrackUi': function(t) {
                                    // data: ?
                                    $.ajax({
                                               type: "GET",
                                               url: "../cgi-bin/hgTrackUi?ajax=1&g=" + mapItems[selectedMapItem].id + "&hgsid=" + getHgsid(),
                                               dataType: "html",
                                               trueSuccess: handleTrackUi,
                                               success: catchErrorOrDispatch,
                                               cache: true
                                           });
                                }
                            },
                            onContextMenu: function(e) {
                                var i = findMapItem(e);
                                var html = "<div id='myMenu' class='contextMenu'><ul>";
                                if(i >= 0)
                                {
                                    selectedMapItem = i;
                                    var href = mapItems[i].href;
                                    var isGene = /hgGene/(href);
                                    
                                    var select = $("select[name=" + mapItems[selectedMapItem].id + "]");
                                    var cur = select.val();
                                    if(cur) {
                                        select.children().each(function(index, o) {
                                            var title = $(this).val();
                                            html += "<li id='" + title + "'>" + title;
                                            if(title == cur) {
                                                html += " <img src='../images/Green_check.png' height='20' width='20' />";
                                            }
                                            html += "</li>";
                                            });
                                            if(isGene) {
                                                html += "<li id='selectWholeGene'>Show whole gene</li>";
                                            } else {
                                                html += "<li id='hgTrackUi'>" + mapItems[i].title + "</li>";
                                            }
                                    } else {
                                        return false;
                                    }
                                } else {
                                    html += "<li id='dragZoomMode'>drag-and-zoom mode";
                                    if(autoHideSetting) {
                                        html += " <img src='../images/Green_check.png' height='20' width='20' />";
                                    }
                                    html += "</li>";
                                    html += "<li id='hilightMode'>hilight mode";
                                    if(!autoHideSetting) {
                                        html += " <img src='../images/Green_check.png' height='20' width='20' />";
                                    }
                                    html += "</li>";
                                    html += "<li id='getImg'>Download image</li>";
                                }
                                html += "</ul></div>";
                                $('#myMenu').html(html);
                                return true;
                            }
                        });
}

function catchErrorOrDispatch(obj,status)
{
    if(obj.err)
    {
        $("#warningText").text(obj.err);
        $("#warning").show();
    }
    else
        this.trueSuccess(obj,status);
}

function handleTrackUi(response, status)
{
    // $('#hgTrackUiDialog').html("<p>Hello world</p><INPUT TYPE=HIDDEN NAME='boolshad.knownGene.label.kgId' VALUE='0'>TEST");
    $('#hgTrackUiDialog').html(response);
    $('#hgTrackUiDialog').dialog({
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

    $('#hgTrackUiDialog').dialog('open');
}
