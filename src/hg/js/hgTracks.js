// Javascript for use in hgTracks CGI

// "use strict";
// Don't complain about line break before '||' etc:
/* jshint -W014 */


var debug = false;

/* Data passed in from CGI via the hgTracks object:
 *
 * string cgiVersion      // CGI_VERSION
 * string chromName       // current chromosome
 * int winStart           // genomic start coordinate (0-based, half-open)
 * int winEnd             // genomic end coordinate
 * int newWinWidth        // new width (in bps) if user clicks on the top ruler
 * boolean revCmplDisp    // true if we are in reverse display
 * int insideX            // width of side-bar (in pixels)
 * int rulerClickHeight   // height of ruler (in pixels) - zero if ruler is hidden
 * boolean inPlaceUpdate  // true if in-place-update is turned on
 * int imgBox*            // various drag-scroll values
 * boolean measureTiming  // true if measureTiming is on
 * Object trackDb         // hash of trackDb entries for tracks which are visible on current page
 */

function initVars()
{  // There are various entry points, so we call initVars in several places to make sure all is well
    if (typeof(hgTracks) !== "undefined" && !genomePos.original) {
        // remember initial position and size so we can restore it if user cancels
        genomePos.original = genomePos.getOriginalPos();
        genomePos.originalSize = $('#size').text().replace(/,/g, ""); // strip out any commas
        dragSelect.originalCursor = jQuery('body').css('cursor');

        imageV2.imgTbl = $('#imgTbl');
        // imageV2.enabled === true unless: advancedJavascript===false, or trackSearch, or config pg
        imageV2.enabled = (imageV2.imgTbl && imageV2.imgTbl.length > 0);

        // jQuery load function with stuff to support drag selection in track img
        if (theClient.isSafari()) {
            // Safari has the following bug: if we update the hgTracks map dynamically,
            // the browser ignores the changes (even though if you look in the DOM the changes
            // are there). So we have to do a full form submission when the user changes
            // visibility settings or track configuration.
            // As of 5.0.4 (7533.20.27) this is problem still exists in safari.
            // As of 5.1 (7534.50) this problem appears to have been fixed - unfortunately,
            // logs for 7/2011 show vast majority of safari users are pre-5.1 (5.0.5 is by far
            // the most common).
            //
            // Early versions of Chrome had this problem too, but this problem went away
            // as of Chrome 5.0.335.1 (or possibly earlier).
            //
            // KRR/JAT 2/2016:
            // This Safari issue is likely resolved in all current versions.  However the test
            // for version had been failing, likely for some time now.
            // (As of 9.0.9, possibly earlier, the 3rd part of the version is included in the
            // user agent string, so must be accounted for in string match)
            // Consequences were that page refresh was used instead of img update (e.g. 
            // for drag-zoom).  And UI dialog was unable to update (e.g. via Apply button).
            imageV2.mapIsUpdateable = false;
            var reg = new RegExp("Version\/([0-9]+.[0-9]+)(.[0-9]+)? Safari");
            var a = reg.exec(navigator.userAgent);
            if (a && a[1]) {
                var version = Number(a[1]);
                if (version >= 5.1) {
                    imageV2.mapIsUpdateable = true;
                }
            }
        }
        imageV2.inPlaceUpdate = hgTracks.inPlaceUpdate && imageV2.mapIsUpdateable;
    }
}

  /////////////////////////////////////
 ////////// Genomic position /////////
/////////////////////////////////////
var genomePos = {

    original: null,
    originalSize: 0,

    linkFixup: function (pos, id, reg, endParamName)
    {   // fixup external links (e.g. ensembl)
        var ele = $(document.getElementById(id));
        if (ele.length) {
            var link = ele.attr('href');
            var a = reg.exec(link);
            if (a && a[1]) {
                ele.attr('href', a[1] + pos.start + "&" + endParamName + "=" + pos.end);
            }
        }
    },

    setByCoordinates: function (chrom, start, end)
    {
        var newPosition = chrom + ":" + start + "-" + end;
        genomePos.set(newPosition, end - start + 1);
        return newPosition;
    },

    getElement: function ()
    {
    // Return position box object
        var tags = document.getElementsByName("position");
        // There are multiple tags with name === "position" (the visible position text input
        // and a hidden with id='positionHidden'); we return value of visible element.
        for (var i = 0; i < tags.length; i++) {
                var ele = tags[i];
                if (ele.id !== "positionHidden") {
                    return ele;
                }
        }
        return null;
    },

    get: function ()
    {
    // Return current value of position box
        var ele = genomePos.getElement();
        if (ele) {
            return ele.value;
        }
        return null;
    },

    getOriginalPos: function ()
    {
        return genomePos.original || genomePos.get();
    },

    revertToOriginalPos: function ()
    {
    // undo changes to position (i.e. after user aborts a drag-and-select).
        this.set(this.original, this.originalSize);
        this.original = this.originalSize = null;         // not sure if this is necessary.
    },

    undisguisePosition: function(position) // UN-DISGUISE VMODE
    {   // find the virt position
        //  position should be real chrom span
        var pos = parsePosition(position);
        if (!pos)
            return position; // some parsing error, return original
        var start = pos.start - 1;
        var end = pos.end;
        var chromName = hgTracks.windows[0].chromName;
        if (pos.chrom !== chromName)
            return position; // return original
        var newStart = -1;
        var newEnd = -1;
        var lastW = null;
        var windows = null;
        for (j=0; j < 3; ++j) {
            if (j === 0) windows = hgTracks.windowsBefore;
            if (j === 1) windows = hgTracks.windows;
            if (j === 2) windows = hgTracks.windowsAfter;
            for (i=0,len=windows.length; i < len; ++i) {
                var w = windows[i];
                //  double check chrom is same thoughout all windows, otherwise warning, return original value
                if (w.chromName != chromName) {
                    return position; // return original
                }
                // check that the regions are ascending and non-overlapping
                if (lastW && w.winStart < lastW.winEnd) {
                    return position; // return original
                }
                // overlap with position?
                //  if intersection, 
                if (w.winEnd > start && end > w.winStart) {
                    var s = Math.max(start, w.winStart);
                    var e = Math.min(end, w.winEnd);
                    var cs = s - w.winStart + w.virtStart;
                    var ce = e - w.winStart + w.virtStart;
                    if (newStart === -1)
                        newStart = cs;
                    newEnd = ce;
                }
                lastW = w;
            }
        }
        //  return new virt undisguised position as a string
        var newPos = "virt:" + (newStart+1) + "-" + newEnd;
        return newPos;
    },

    disguiseSize: function(position) // DISGUISE VMODE
    {   // find the real size of the windows spanned
        //  position should be a real chrom span
        var pos = parsePosition(position);
        if (!pos)
            return 0;
        var start = pos.start - 1;
        var end = pos.end;
        var newSize = 0;
        var windows = null;
        for (j=0; j < 3; ++j) {
            if (j === 0) windows = hgTracks.windowsBefore;
            if (j === 1) windows = hgTracks.windows;
            if (j === 2) windows = hgTracks.windowsAfter;
            for (i=0,len=windows.length; i < len; ++i) {
                var w = windows[i];
                // overlap with position?
                //  if intersection, 
                if (w.winEnd > start && end > w.winStart) {
                    var s = Math.max(start, w.winStart);
                    var e = Math.min(end, w.winEnd);
                    newSize += (e - s);
                }
            }
        }
        //  return real size of the disguised position 
        return newSize;
    },

    disguisePosition: function(position) // DISGUISE VMODE
    {   // find the single-chrom range spanned
        //  position should be virt
        var pos = parsePosition(position);
        if (!pos)
            return position; // some parsing error, return original
        var start = pos.start - 1;
        var end = pos.end;
        var chromName = hgTracks.windows[0].chromName;
        var newStart = -1;
        var newEnd = -1;
        var lastW = null;
        var windows = null;
        for (j=0; j < 3; ++j) {
            if (j === 0) windows = hgTracks.windowsBefore;
            if (j === 1) windows = hgTracks.windows;
            if (j === 2) windows = hgTracks.windowsAfter;
            for (i=0,len=windows.length; i < len; ++i) {
                var w = windows[i];
                //  double check chrom is same thoughout all windows, otherwise warning, return original value
                if (w.chromName != chromName) {
                    return position; // return undisguised original
                }
                // check that the regions are ascending and non-overlapping
                if (lastW && w.winStart < lastW.winEnd) {
                    return position; // return undisguised original
                }
                // overlap with position?
                //  if intersection, 
                if (w.virtEnd > start && end > w.virtStart) {
                    var s = Math.max(start, w.virtStart);
                    var e = Math.min(end, w.virtEnd);
                    var cs = s - w.virtStart + w.winStart;
                    var ce = e - w.virtStart + w.winStart;
                    if (newStart === -1)
                        newStart = cs;
                    newEnd = ce;
                }
                lastW = w;
            }
        }
        //  return new non-virt disguised position as a string
        var newPos = chromName + ":" + (newStart+1) + "-" + newEnd;
        return newPos;
    },

    set: function (position, size)
    {   // Set value of position and size (in hiddens and input elements).
        // We assume size has already been commified.
        // Either position or size may be null.

        // stack dump  // DEBUG
        //console.trace();
        // NOT work on safari
        //var obj = {};
        //Error.captureStackTrace(obj);
        //warn("genomePos.set() called "+obj.stack);

        position = position.replace(/,/g, ""); // strip out any commas

        if (position) {
            // DISGUISE VMODE
            //warn("genomePos.set() called, position = "+position);
            if (hgTracks.virtualSingleChrom && (position.search("virt:")===0)) {
                var newPosition = genomePos.disguisePosition(position);
                //warn("genomePos.set() position = "+position+", newPosition = "+newPosition);
                position = newPosition;
            }
        }
        if (position) {
            // There are multiple tags with name === "position"
            // (one in TrackHeaderForm and another in TrackForm).
            var tags = document.getElementsByName("position");
            for (var i = 0; i < tags.length; i++) {
                var ele = tags[i];
                ele.value = position;
            }
        }
        var pos = parsePosition(position);
        if ($('#positionDisplay').length) {
            // add commas to positionDisplay
            var commaPosition = position;
            if (pos)
                commaPosition = pos.chrom+":"+commify(pos.start)+"-"+commify(pos.end);  
            $('#positionDisplay').text(commaPosition);
        }
        if (size) {
            if (hgTracks.virtualSingleChrom && (position.search("virt:")!==0)) {
                var newSize = genomePos.disguiseSize(position);
                //warn("genomePos.set() position = "+position+", newSize = "+newSize);
                if (newSize > 0)
                    size = newSize;
            }
            $('#size').text(commify(size)); // add commas
        }
        if (pos) {
            // fixup external static links on page'

            // Example ensembl link:
            // http://www.ensembl.org/Homo_sapiens/contigview?chr=21&start=33031934&end=33041241
            genomePos.linkFixup(pos, "ensemblLink", new RegExp("(.+start=)[0-9]+"), "end");

            // Example NCBI Map Viewer link (obsolete):
            // https://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9606&CHR=21&BEG=33031934&END=33041241
            genomePos.linkFixup(pos, "ncbiLink", new RegExp("(.+BEG=)[0-9]+"), "END");

            // Example NCBI Genome Data Viewer link
            // https://www.ncbi.nlm.nih.gov/genome/gdv/browser/?id=GCF_000001405.37&chr=4&from=45985744&to=45991655&context=genome
            genomePos.linkFixup(pos, "ncbiLink", new RegExp("(.+from=)[0-9]+"), "to");

            // Example medaka link: 
            // http://utgenome.org/medakabrowser_ens_jump.php?revision=version1.0&chr=chromosome18&start=14435198&end=14444829
            genomePos.linkFixup(pos, "medakaLink", new RegExp("(.+start=)[0-9]+"), "end");

            var link;
            var reg;
            var a;
            if ($('#wormbaseLink').length) {
                // e.g. http://www.wormbase.org/db/gb2/gbrowse/c_elegans?name=II:14646301-14667800
                link = $('#wormbaseLink').attr('href');
                reg = new RegExp("(.+:)[0-9]+");
                a = reg.exec(link);
                if (a && a[1]) {
                    $('#wormbaseLink').attr('href', a[1] + pos.start + "-" + pos.end);
                }
            }
            // Fixup DNA link; e.g.: 
            // hgc?hgsid=2999470&o=114385768&g=getDna&i=mixed&c=chr7&l=114385768&r=114651696&db=panTro2&hgsid=2999470
            if ($('#dnaLink').length) {
                link = $('#dnaLink').attr('href');
                reg = new RegExp("(.+&o=)[0-9]+.+&db=[^&]+(.*)");
                a = reg.exec(link);
                if (a && a[1]) {
                    var url = a[1] + (pos.start - 1) + "&g=getDna&i=mixed&c=" + pos.chrom;
                    url += "&l=" + (pos.start - 1) + "&r=" + pos.end + "&db=" + getDb() + a[2];
                    $('#dnaLink').attr('href', url);
                }
            }
        }
        if (!imageV2.backSupport)
            imageV2.markAsDirtyPage();
    },

    check: function (img, selection)
    {   // return true if user's selection is still w/n the img (including some slop).
        var imgWidth = jQuery(img).width();
        var imgHeight = jQuery(img).height();
        var imgOfs = jQuery(img).offset();
        var slop = 10;

        // We ignore clicks in the gray tab and track title column
        // (we really should suppress all drag activity there,
        // but I don't know how to do that with imgAreaSelect).
        var leftX = hgTracks.revCmplDisp ?  imgOfs.left - slop :
                                            imgOfs.left + hgTracks.insideX - slop;
        var rightX = hgTracks.revCmplDisp ? imgOfs.left + imgWidth - hgTracks.insideX + slop :
                                            imgOfs.left + imgWidth + slop;

        return (   (selection.event.pageX >= leftX)
                && (selection.event.pageX < rightX)
                && (selection.event.pageY >= (imgOfs.top - slop))
                && (selection.event.pageY <  (imgOfs.top + imgHeight + slop)));
    },

    pixelsToBases: function (img, selStart, selEnd, winStart, winEnd)
    {   // Convert image coordinates to chromosome coordinates
        var imgWidth = jQuery(img).width() - hgTracks.insideX;
        var width = hgTracks.winEnd - hgTracks.winStart;
        var mult = width / imgWidth;   // mult is bp/pixel multiplier
        var startDelta;   // startDelta is how many bp's to the right/left
        var x1;

        // The magic number three appear at another place in the code 
        // as LEFTADD. It was originally annotated as "borders or cgi item calc
        // ?" by Larry. It has to be used when going any time when converting 
        // between pixels and coordinates.
        selStart -= 3;
        selEnd -= 3;

        if (hgTracks.revCmplDisp) {
            x1 = Math.min(imgWidth, selStart);
            startDelta = Math.floor(mult * (imgWidth - x1));
        } else {
            x1 = Math.max(hgTracks.insideX, selStart);
            startDelta = Math.floor(mult * (x1 - hgTracks.insideX));
        }
        var endDelta;
        var x2;
        if (hgTracks.revCmplDisp) {
            endDelta = startDelta;
            x2 = Math.min(imgWidth, selEnd);
            startDelta = Math.floor(mult * (imgWidth - x2));
        } else {
            x2 = Math.max(hgTracks.insideX, selEnd);
            endDelta = Math.floor(mult * (x2 - hgTracks.insideX));
        }
        var newStart = hgTracks.winStart + startDelta;
        var newEnd = hgTracks.winStart + 1 + endDelta;
        if (newEnd > winEnd) {
            newEnd = winEnd;
        }
        return {chromStart : newStart, chromEnd : newEnd};
    },

    chromToVirtChrom: function (chrom, chromStart, chromEnd)
    {   // Convert regular chromosome position to virtual chrom coordinates using hgTracks.windows list
        // Consider the first contiguous set of overlapping regions to define the match (for now).
        // only works for regions covered by the current hgTracks.windows
        var virtStart = -1, virtEnd = -1;
        var s,e;
        var i, len;
        for (i = 0, len = hgTracks.windows.length; i < len; ++i) {
            var w = hgTracks.windows[i];
            var overlap = (chrom == w.chromName && chromEnd > w.winStart && w.winEnd > chromStart);
            if (virtStart == -1) {
                if (overlap) {
                    // when they overlap the first time
                    s = Math.max(chromStart, w.winStart);
                    e = Math.min(chromEnd, w.winEnd);
                    virtStart = w.virtStart + (s - w.winStart);
                    virtEnd   = w.virtStart + (e - w.winStart);
                } else {
                    // until they overlap
                    // do nothing
                }
            } else {
                if (overlap) {
                    // while they continue to overlap, extend
                    e = Math.min(chromEnd, w.winEnd);
                    virtEnd   = w.virtStart + (e - w.winStart);
                } else {
                    // when they do not overlap anymore, stop
                    break;
                }
            }
        }
        return {chromStart : virtStart, chromEnd : virtEnd};
    },

    selectionPixelsToBases: function (img, selection)
    {   // Convert selection x1/x2 coordinates to chromStart/chromEnd.
        return genomePos.pixelsToBases(img, selection.x1, selection.x2,
                                        hgTracks.winStart, hgTracks.winEnd);
    },

    update: function (img, selection, singleClick)
    {
        var pos = genomePos.pixelsToBases(img, selection.x1, selection.x2,
                                            hgTracks.winStart, hgTracks.winEnd);
        // singleClick is true when the mouse hasn't moved (or has only moved a small amount).
        if (singleClick) {
            var center = (pos.chromStart + pos.chromEnd)/2;
            pos.chromStart = Math.floor(center - hgTracks.newWinWidth/2);
            pos.chromEnd = pos.chromStart + hgTracks.newWinWidth;
            // clip
            if (pos.chromStart < hgTracks.chromStart)
                pos.chromStart = hgTracks.chromStart; // usually  1
            if (pos.chromEnd > hgTracks.chromEnd)
                pos.chromEnd = hgTracks.chromEnd; // usually virt chrom size

            // save current position so that that it may be restored after highlight or cancel.
            genomePos.original = genomePos.getOriginalPos();
            genomePos.originalSize = $('#size').text().replace(/,/g, ""); // strip out any commas

        }
        var newPosition = genomePos.setByCoordinates(hgTracks.chromName,
                                                            pos.chromStart+1, pos.chromEnd);
        return newPosition;
    },

    handleChange: function (response, status)
    {
        var json = JSON.parse(response);
        genomePos.set(json.pos);
    },

    changeAssemblies: function (ele)  // UNUSED?  Larry's experimental code
    {   // code to update page when user changes assembly select list.
        $.ajax({
                type: "GET",
                url: "../cgi-bin/hgApi",
                data: cart.varsToUrlData({ 'cmd': 'defaultPos', 'db': getDb() }),
                dataType: "html",
                trueSuccess: genomePos.handleChange,
                success: catchErrorOrDispatch,
                error: errorHandler,
                cache: true
            });
        return false;
    },


    convertedVirtCoords : {chromStart : -1, chromEnd : -1},

    handleConvertChromPosToVirtCoords: function (response, status)
    {
        var virtStart = -1, virtEnd = -1;
        var newJson = scrapeVariable(response, "convertChromToVirtChrom");
        if (!newJson) {
            warn("convertChromToVirtChrom object is missing from the response");
        } else {
            virtStart = newJson.virtWinStart;
            virtEnd   = newJson.virtWinEnd;
        }
        genomePos.convertedVirtCoords = {chromStart : virtStart, chromEnd : virtEnd};
    },

    convertChromPosToVirtCoords: function (chrom, chromStart, chromEnd)
    {   // code to convert chrom position to virt coords
        genomePos.convertedVirtCoords = {chromStart : -1, chromEnd : -1};  // reset
        var pos = chrom+":"+(chromStart+1)+"-"+chromEnd; // easier to pass 1 parameter than 3
        $.ajax({
                type: "GET",
                async: false, // wait for result
                url: "../cgi-bin/hgTracks",
                data: cart.varsToUrlData({ 'hgt.convertChromToVirtChrom': pos, 'hgt.trackImgOnly' : 1, 'hgsid': getHgsid() }),
                dataType: "html",
                trueSuccess: genomePos.handleConvertChromPosToVirtCoords,
                success: catchErrorOrDispatch,
                error: errorHandler,
                cache: false
            });
        return genomePos.convertedVirtCoords;
    },

    positionDisplayDialog: function ()
    // Show the virtual and real positions of the windows
    {   
        var position = genomePos.get();
        var positionDialog = $("#positionDialog")[0];
        if (!positionDialog) {
            $("body").append("<div id='positionDialog'><span id='positionDisplayPosition'></span>");
            positionDialog = $("#positionDialog")[0];
        }
        if (hgTracks.windows) {
            var i,len;
            var str = position;
            if (!(hgTracks.virtualSingleChrom && (hgTracks.windows.length === 1))) {
                str += "<br>\n";
                str += "<br>\n";
                str += "<ul style='list-style-type:none; max-height:200px; padding:0; width:80%; overflow:hidden; overflow-y:scroll;'>\n";
                for (i=0,len=hgTracks.windows.length; i < len; ++i) {
                    var w = hgTracks.windows[i];
                    str += "<li>" + w.chromName + ":" + (w.winStart+1) + "-" + w.winEnd + "</li>\n";
                }
                str += "</ul>\n";
            }
            $("#positionDisplayPosition").html(str);
        } else {
            $("#positionDisplayPosition").html(position);
        }
        $(positionDialog).dialog({
                modal: true,
                title: "Window-Positions",
                closeOnEscape: true,
                resizable: false,
                autoOpen: false,
                minWidth: 400,
                minHeight: 40,
                buttons: {  
                    "OK": function() {
                        $(this).dialog("close");
                    }
                },

                open: function () { // Make OK the focus/default action
                   $(this).parents('.ui-dialog-buttonpane button:eq(0)').focus(); 
                },

                close: function() {
                    // All exits to dialog should go through this
                    $(imageV2.imgTbl).imgAreaSelect({hide:true});
                    $(this).hide();
                    $('body').css('cursor', ''); // Occasionally wait cursor got left behind
                }
        });
        $(positionDialog).dialog('open');
    }

};

  /////////////////////////////////////
 //// Creating items by dragging /////
/////////////////////////////////////
var makeItemsByDrag = {

    end: function (img, selection)
    {
        var image = $(img);
        var imageId = image.attr('id');
        var trackName = imageId.substring('img_data_'.length);
        var pos = genomePos.selectionPixelsToBases(image, selection);
        var command = document.getElementById('hgt_doJsCommand');
        command.value  = "makeItems " + trackName + " " + hgTracks.chromName;
        command.value +=  " " + pos.chromStart + " " + pos.chromEnd;
        document.TrackHeaderForm.submit();
        return true;
    },

    init: function (trackName)
    {
    // Set up so that they can drag out to define a new item on a makeItems track.
    var img = $("#img_data_" + trackName);
    if (img && img.length !== 0) {
        var imgHeight = imageV2.imgTbl.height();
        jQuery(img.imgAreaSelect( { selectionColor: 'green', outerColor: '',
            minHeight: imgHeight, maxHeight: imgHeight, onSelectEnd: makeItemsByDrag.end,
            autoHide: true, movable: false}));
        }
    },

    load: function ()
    {
        for (var id in hgTracks.trackDb) {
            var rec = hgTracks.trackDb[id];
            if (rec && rec.type && rec.type.indexOf("makeItems") === 0) {
                this.init(id);
            }
        }
    }
};

  ///////////////////
 /////  mouse  /////
///////////////////
var mouse = {

    savedOffset: {x:0, y:0},

    saveOffset: function (ev)
    {   // Save the mouse offset associated with this event
        mouse.savedOffset = {x: ev.clientX, y: ev.clientY};
    },

    hasMoved: function (ev)
    {   // return true if mouse has moved a significant amount
        var minPixels = 10;
        var movedX = ev.clientX - mouse.savedOffset.x;
        var movedY = ev.clientY - mouse.savedOffset.y;
        if (arguments.length === 2) {
            var num = Number(arguments[1]);
            if (isNaN(num)) {
                if ( arguments[1].toLowerCase() === "x" )
                    return (movedX > minPixels || movedX < (minPixels * -1));
                if ( arguments[1].toLowerCase() === "y" )
                    return (movedY > minPixels || movedY < (minPixels * -1));
            }
            else
                minPixels = num;
        }
        return (   movedX > minPixels || movedX < (minPixels * -1)
                || movedY > minPixels || movedY < (minPixels * -1));
    }
};

  /////////////////
 //// posting ////
/////////////////
var posting = {

    blockUseMap: false,

    blockMapClicks: function () 
    // Blocks clicking on map items when in effect. Drag opperations frequently call this.
    { 
        posting.blockUseMap=true;  
    },
    
    allowMapClicks:function ()  
    // reallows map clicks.  Called after operations that compete with clicks (e.g. dragging) 
    { 
        $('body').css('cursor', ''); // Explicitly remove wait cursor.
        posting.blockUseMap=false; 
    },

    mapClicksAllowed: function () 
    // Verify that click-competing operation (e.g. dragging) isn't currently occurring.
    { 
        return (posting.blockUseMap === false); 
    },

    blockTheMapOnMouseMove: function (ev)
    {
        if (!posting.blockUseMap && mouse.hasMoved(ev)) {
            posting.blockUseMap=true;
        }
    },

    mapClk: function ()
    {
        var done = false;
        if (false && imageV2.inPlaceUpdate) {
            // XXXX experimental and only turned on in larrym's tree.
            // Use in-place update if the map item just modifies the current position (this is nice
            // because it's faster and preserves the users relative position in the track image).
            //
            // First test handles next/prev item.
            var str = "/cgi-bin/hgTracks\\?position=([^:]+):(.+)&hgsid=(\\d+)" +
                                                                    "&(hgt.(next|prev)Item=[^&]+)";
            var reg = new RegExp(str);
            var a = reg.exec(this.href);
            if (a && a[1] && a[1] === hgTracks.chromName) {
                imageV2.navigateInPlace("position=" + encodeURIComponent(a[1] + ":" + a[2]) + 
                                                                            "&" + a[4], null, true);
                done = true;
            } else {
                // handle next/prev exon
                str = "/cgi-bin/hgTracks\\?position=([^:]+):(.+)&hgsid=(\\d+)$";
                reg = new RegExp(str);
                a = reg.exec(this.href);
                if (a && a[1]) {
                    imageV2.navigateInPlace("position=" + encodeURIComponent(a[1] + ":" + a[2]), 
                                                                                        null, true);
                    done = true;
                } else {
                    // handle toggle visibility.
                    // Request may include a track set, so we cannot use requestImgUpdate.
                    str = "/cgi-bin/hgTracks\\?(position=[^:]+:.+&hgsid=\\d+&([^=]+)=([^&]+))$";
                    reg = new RegExp(str);
                    a = reg.exec(this.href);
                    if (a && a[1]) {
                        imageV2.navigateInPlace(a[1], null, true);
                        // imageV2.requestImgUpdate(a[1], a[1] + "=" + a[2], "", a[2]);
                        done = true;
                    }
                }
            }
        }
        if (done)
            return false;
        else
            return posting.saveSettings(this);
    },

    saveSettings: function (obj)
    {
        if (posting.blockUseMap === true) {
            return false;
        }
        if (!obj || !obj.href) // called directly with obj
            obj = this;                               // and from callback without obj

        if ($(obj).hasClass('noLink'))  // TITLE_BUT_NO_LINK
            return false;

        if (obj.href.match('#') || obj.target.length > 0) {
            //alert("Matched # ["+obj.href+"] or has target:"+obj.target);
            return true;
        }
        var thisForm = normed($(obj).parents('form').first());
        if (!thisForm)
            thisForm = normed($("FORM").first());
        if (thisForm) {
            //alert("posting form:"+$(thisForm).attr('name'));
            return postTheForm($(thisForm).attr('name'),cart.addUpdatesToUrl(obj.href));
        }
        return true;
    }
};

/////////////////////////
//// cart updating /////
///////////////////////
var cart = {
    // Controls queuing and ultimately updating cart variables vis ajax or submit. Queued vars
    // are held in an object with unique keys preventing duplicate updates and ensuring last update 
    // takes precedence.  WARNING: be careful creating an object with variables on the fly:
    // cart.setVarsObj({track: vis}) is invalid but cart.setVarsObj({'knownGene': vis}) is ok! 

    updateQueue: {},
    
    updatesWaiting: function ()
    {   // returns TRUE if updates are waiting.
        return objNotEmpty(cart.updateQueue);
    },
    
    addUpdatesToUrl: function (url)
    {   // adds any outstanding cart updates to the url, then clears the queue
        if (cart.updatesWaiting()) {
            //console.log('cart.addUpdatesToUrl: '+objKeyCount(cart.updateQueue)+' vars');
            var updates = cart.varsToUrlData(); // clears the queue
            if (!url || url.length === 0)
                return updates;

            if (updates.length > 0) {
                var dataOnly = (url.indexOf("cgi-bin") === -1); // all urls should be to our cgis
                if (!dataOnly && url.lastIndexOf("?") === -1)
                    url += "?" + updates;
                else
                    url += '&' + updates;
            }
        }
        return url;
    },
    
    beforeUnload: function ()
    {   // named function that can be bound and unbound to beforeunload event
        // Makes sure any outstanding queued updates are sent before leaving the page.
        //console.log('cart.beforeUnload: '+objKeyCount(cart.updateQueue)+' vars');
        cart.setVarsObj( {}, null, false ); // synchronous
    },
    
    varsToUrlData: function (varsObj)
    {   // creates a url data (var1=val1&var2=val2...) string from vars object and queue
        // The queue will be emptied by this call.
        cart.queueVarsObj(varsObj); // lay ontop of queue, to give new values precedence
        
        // Now convert to url data and clear queue
        var urlData = '';
        if (cart.updatesWaiting()) {
            //console.log('cart.varsToUrlData: '+objKeyCount(cart.updateQueue)+' vars');
            urlData = varHashToQueryString(cart.updateQueue);
            cart.updateQueue = {};
        }
        return urlData;
    },
    
    setVarsObj: function (varsObj, errFunc, async)
    {   // Set all vars in a var hash, appending any queued updates
        //console.log('cart.setVarsObj: were:'+objKeyCount(cart.updateQueue) + 
        //            ' new:'+objKeyCount(varsObj);
        cart.queueVarsObj(varsObj); // lay ontop of queue, to give new values precedence
        
        // Now ajax update all in queue and clear queue
        if (cart.updatesWaiting()) {
            setVarsFromHash(cart.updateQueue, errFunc, async);
            cart.updateQueue = {};
        }
    },
    
    setVars: function (names, values, errFunc, async)
    {   // ajax updates the cart, and includes any queued updates.
        cart.setVarsObj(arysToObj(names, values), errFunc, async);
    },

    queueVarsObj: function (varsObj)
    {   // Add object worth of cart updates to the 'to be updated' queue, so they can be sent to
        // the server later. Note: hash allows overwriting previous updates to the same variable.
        if (typeof varsObj !== 'undefined' && objNotEmpty(varsObj)) {
            //console.log('cart.queueVarsObj: were:'+objKeyCount(cart.updateQueue) + 
            //            ' new:'+objKeyCount(varsObj));
            for (var name in varsObj) {
                cart.updateQueue[name] = varsObj[name];
                
                // Could update in background, however, failing to hit "refresh" is user choice
                // first in queue, schedule background update
                if (objKeyCount(cart.updateQueue) === 1) {
                    // By unbind/bind, we assure that there is only one instance bound
                    $(window).unbind('beforeunload', cart.beforeUnload); 
                    $(window).bind(  'beforeunload', cart.beforeUnload); 
                }
            }
        }
    },
    
    addVarsToQueue: function (names,values)
    {   // creates a string of updates to save for ajax batch or a submit
        cart.queueVarsObj(arysToObj(names,values));
    }
};

  ///////////////////////////////////////////////
 //// visibility (mixed with group toggle) /////
///////////////////////////////////////////////
var vis = {

    // map cgi enum visibility codes to strings
    enumOrder: new Array("hide", "dense", "full", "pack", "squish"),  

    update: function (track, visibility)
    {   // Updates visibility state in hgTracks.trackDb and any visible elements on the page.
        // returns true if we modify at least one select in the group list
        var rec = hgTracks.trackDb[track];
        var selectUpdated = false;
        $("select[name=" + escapeJQuerySelectorChars(track) + "]").each(function(t) {
            $(this).attr('class', visibility === 'hide' ? 'hiddenText' : 'normalText');
            $(this).val(visibility);
            selectUpdated = true;
        });
        if (rec) {
            rec.localVisibility = visibility;
        }
        return selectUpdated;
    },

    get: function (track)
    {   // return current visibility for given track
        var rec = hgTracks.trackDb[track];
        if (rec) {
            if (rec.localVisibility) {
                return rec.localVisibility;
            } else {
                return vis.enumOrder[rec.visibility];
            }
        } else {
            return null;
        }
    },

    makeTrackVisible: function (track)
    {   // Sets the vis box to visible, and ques a cart update, but does not update the image
        if (track && vis.get(track) !== "full") {
            vis.update(track, 'pack');
            cart.addVarsToQueue([track], ['pack']);
        }
    },

    toggleForGroup: function (button, prefix)
    {   // toggle visibility of a track group; prefix is the prefix of all the id's of tr's in the
        // relevant group. This code also modifies the corresponding hidden fields and the gif
        // of the +/- img tag.
        imageV2.markAsDirtyPage();
        if (arguments.length > 2)
            return setTableRowVisibility(button, prefix, "hgtgroup", "group",false,arguments[2]);
        else
            return setTableRowVisibility(button, prefix, "hgtgroup", "group",false);
    },

    expandAllGroups: function (newState)
    {   // Set visibility of all track groups to newState (true means expanded).
        // This code also modifies the corresponding hidden fields and the gif's of the +/- img tag.
        imageV2.markAsDirtyPage();
        $(".toggleButton[id$='_button']").each( function (i) {  
            // works for old img type AND new BUTTONS_BY_CSS      // - 7: clip '_button' suffix
            vis.toggleForGroup(this,this.id.substring(0,this.id.length - 7),newState);
        });
        return false;
    },
    
    initForAjax: function()
    {   // To better support the back-button, it is good to eliminate extraneous form puts
        // Towards that end, we support visBoxes making ajax calls to update cart.
        var sels = $('select.normalText,select.hiddenText');
        $(sels).change(function() {
            var track = $(this).attr('name');
            if ($(this).val() === 'hide') {
                var rec = hgTracks.trackDb[track];
                if (rec)
                    rec.visibility = 0;
                // else Would be nice to hide subtracks as well but that may be overkill
                $(document.getElementById('tr_' + track)).remove();
                imageV2.highlightRegion();
                $(this).attr('class', 'hiddenText');
            } else
                $(this).attr('class', 'normalText');
            
            cart.addVarsToQueue([track], [$(this).val()]);
            imageV2.markAsDirtyPage();
            return false;
        });
        // Now we can rid the submt of the burden of all those vis boxes
        var form = $('form#TrackForm');
        $(form).submit(function () {
            $('select.normalText,select.hiddenText').attr('disabled',true);
        });
        $(form).attr('method','get');
    },

    restoreFromBackButton: function()
    // Re-enabling vis dropdowns is necessary because initForAjax() disables them on submit.
    {
        $('select.normalText,select.hiddenText').attr('disabled',false);
    }
};

  ////////////////////////////////////////////////////////////
 // dragSelect is also known as dragZoom or shift-dragZoom //
////////////////////////////////////////////////////////////
var dragSelect = {

    hlColorDefault: '#aaedff', // default highlight color, if nothing specified
    hlColor :       '#aaedff', // current highlight color
    areaSelector:    null, // formerly "imgAreaSelect". jQuery element used for imgAreaSelect
    originalCursor:  null,
    startTime:       null,
    escPressed :     false,  // flag is set when user presses Escape

    selectStart: function (img, selection)
    {
        initVars();
        dragSelect.escPressed = false;
        if (rightClick.menu) {
            rightClick.menu.hide();
        }
        var now = new Date();
        dragSelect.startTime = now.getTime();
        posting.blockMapClicks();
    },

    selectChange: function (img, selection)
    {
        if (selection.x1 !== selection.x2) {
            if (genomePos.check(img, selection)) {
                genomePos.update(img, selection, false);
                jQuery('body').css('cursor', dragSelect.originalCursor);
            } else {
                jQuery('body').css('cursor', 'not-allowed');
            }
        }
        return true;
    },

    findHighlightIdxForPos : function(findPos) {
        // return the index of the highlight string e.g. hg19.chrom1:123-345#AABBDCC that includes a chrom range findPos
        // mostly copied from highlightRegion()
        var currDb = getDb();
        if (hgTracks.highlight) {
            var hlArray = hgTracks.highlight.split("|"); // support multiple highlight items
            for (var i = 0; i < hlArray.length; i++) {
                hlString = hlArray[i];
                pos = parsePositionWithDb(hlString);
                imageV2.undisguiseHighlight(pos);

                if (!pos) 
                    continue; // ignore invalid position strings
                pos.start--;

                if (pos.chrom === hgTracks.chromName && pos.db === currDb
                &&  pos.start <= findPos.chromStart && pos.end >= findPos.chromEnd) {
                    return i;
                }
            }
        }
        return null;
    },

    highlightThisRegion: function(newPosition, doAdd, hlColor)
    // set highlighting newPosition in server-side cart and apply the highlighting in local UI.
    {
        var hlColorName = hlColor; // js convention: do not assign to argument variables
        if (hlColor==="" || hlColor===null || hlColor===undefined)
            hlColorName = dragSelect.hlColor;

        var pos = parsePosition(newPosition);
        var start = pos.start;
        var end = pos.end;
        var newHighlight = getDb() + "." + pos.chrom + ":" + start + "-" + end + hlColorName;
        newHighlight = imageV2.disguiseHighlight(newHighlight);
        var oldHighlight = hgTracks.highlight;
        if (oldHighlight===undefined || doAdd===undefined || doAdd===false || oldHighlight==="") {
            // just set/overwrite the old highlight position, this used to be the default
            hgTracks.highlight = newHighlight;
        }
        else {
            // add to the end of a |-separated list
            hgTracks.highlight = oldHighlight+"|"+newHighlight;
        }
        // we include enableHighlightingDialog because it may have been changed by the dialog
        var cartSettings = {             'highlight': hgTracks.highlight, 
                          'enableHighlightingDialog': hgTracks.enableHighlightingDialog ? 1 : 0 };

        if (hgTracks.windows && !hgTracks.virtualSingleChrom) {
            var nonVirtChrom = "";
            var nonVirtStart = -1; 
            var nonVirtEnd   = -1; 
            for (i=0,len=hgTracks.windows.length; i < len; ++i) {
                var w = hgTracks.windows[i];
                // overlap with new position?
                if (w.virtEnd > start && end > w.virtStart) {
                    var s = Math.max(start, w.virtStart);
                    var e = Math.min(end, w.virtEnd);
                    var cs = s - w.virtStart + w.winStart;
                    var ce = e - w.virtStart + w.winStart;
                    if (nonVirtChrom === "") {
                        nonVirtChrom = w.chromName;
                        nonVirtStart = cs; 
                        nonVirtEnd   = ce;
                    } else {
                        if (w.chromName === nonVirtChrom) {
                            nonVirtEnd = Math.max(ce, nonVirtEnd);
                        } else {
                            break;
                        }
                    }
                }
            }
            if (nonVirtChrom !== "")
                cartSettings.nonVirtHighlight = getDb() + '.' + nonVirtChrom + ':' + nonVirtStart + '-' + (nonVirtEnd+1) + hlColorName;
        } else if (hgTracks.windows && hgTracks.virtualSingleChrom) {
                cartSettings.nonVirtHighlight = hgTracks.highlight;
        }
        // TODO if not virt, do we need to erase cart nonVirtHighlight ?
        cart.setVarsObj(cartSettings);
        imageV2.highlightRegion();
    },

    selectionEndDialog: function (newPosition)
    // Let user choose between zoom-in and highlighting.
    {   
        // if the user hit Escape just before, do not show this dialog
        if (dragSelect.startTime===null)
            return;
        var dragSelectDialog = $("#dragSelectDialog")[0];
        if (!dragSelectDialog) {
            $("body").append("<div id='dragSelectDialog'>" + 
                             "<p><ul>"+
                             "<li>Hold <b>Shift+drag</b> to show this dialog" +
                             "<li>Hold <b>Alt+drag</b> to add a highlight" +
                             "<li>Hold <b>Ctrl+drag</b> (Windows) or <b>Cmd+drag</b> (Mac) to zoom" +
                             "<li>To cancel, press <tt>Esc</tt> anytime or drag mouse outside image" +
                             "<li>Highlight the current position with <tt>h then m</tt>" +
                             "<li>Clear all highlights with View - Clear Highlights or <tt>h then c</tt>" +
                             "</ul></p>" +
                             "<p>Highlight color: <input type='text' style='width:70px' id='hlColorInput' value='"+dragSelect.hlColor+"'>" +
                             //"<span id='hlColorBox' style='width:20px'></span>" + 
                             "&nbsp;&nbsp;<input id='hlColorPicker'>" + 
                             "&nbsp;&nbsp;<a href='#' id='hlReset'>Reset</a></p>" + 
                             "<input style='float:left' type='checkbox' id='disableDragHighlight'>" + 
                             "<span style='border:solid 1px #DDDDDD; padding:3px;display:inline-block' id='hlNotShowAgainMsg'>Don't show this again and always zoom with shift.<br>" + 
                             "Re-enable via 'View - Configure Browser' (<tt>c then f</tt>)</span></p>"+ 
                             "Selected chromosome position: <span id='dragSelectPosition'></span>");
            dragSelectDialog = $("#dragSelectDialog")[0];
            // reset value
            $('#hlReset').click(function() { 
                $('#hlColorInput').val(dragSelect.hlColorDefault);
                $("#hlColorPicker").spectrum("set", dragSelect.hlColorDefault);
            });
            // allow to click checkbox by clicking on the label
            $('#hlNotShowAgainMsg').click(function() { $('#disableDragHighlight').click();});
            // click "add highlight" when enter is pressed in color input box
            $("#hlColorInput").keyup(function(event){
                if(event.keyCode == 13){
                    $(".ui-dialog-buttonset button:nth-child(3)").click();
                }
            });
            // activate the color picker
            var opt = {
                hideAfterPaletteSelect : true,
                color : $('#hlColorInput').val(),
                showPalette: true,
                showInput: true,
                preferredFormat: "hex",
                change: function() { var color = $("#hlColorPicker").spectrum("get"); $('#hlColorInput').val(color); },
                };
            $("#hlColorPicker").spectrum(opt);
            // update the color picker if you change the input box
            $("#hlColorInput").change(function(){ $("#hlColorPicker").spectrum("set", $('#hlColorInput').val()); });
        }

        $("#hlColorPicker").spectrum("set", $('#hlColorInput').val());

        if (hgTracks.windows) {
            var i,len;
            var newerPosition = newPosition;
            if (hgTracks.virtualSingleChrom && (newPosition.search("virt:")===0)) {
                newerPosition = genomePos.disguisePosition(newPosition);
            }
            var str = newerPosition + "<br>\n";
            var str2 = "<br>\n";
            str2 += "<ul style='list-style-type:none; max-height:200px; padding:0; width:80%; overflow:hidden; overflow-y:scroll;'>\n";
            var pos = parsePosition(newPosition);
            var start = pos.start - 1;
            var end = pos.end;
            var selectedRegions = 0;
            for (i=0,len=hgTracks.windows.length; i < len; ++i) {
                var w = hgTracks.windows[i];
                // overlap with new position?
                if (w.virtEnd > start && end > w.virtStart) {
                    var s = Math.max(start, w.virtStart);
                    var e = Math.min(end, w.virtEnd);
                    var cs = s - w.virtStart + w.winStart;
                    var ce = e - w.virtStart + w.winStart;
                    str2 += "<li>" + w.chromName + ":" + (cs+1) + "-" + ce + "</li>\n";
                    selectedRegions += 1;
                }
            }
            str2 += "</ul>\n";
            if (!(hgTracks.virtualSingleChrom && (selectedRegions === 1))) {
                str += str2;
            }
            $("#dragSelectPosition").html(str);
        } else {
            $("#dragSelectPosition").html(newPosition);
        }
        $(dragSelectDialog).dialog({
                modal: true,
                title: "Drag-and-select",
                closeOnEscape: true,
                resizable: false,
                autoOpen: false,
                revertToOriginalPos: true,
                minWidth: 500,
                buttons: {  
                    "Zoom In": function() {
                        // Zoom to selection
                        $(this).dialog("option", "revertToOriginalPos", false);
                        if ($("#disableDragHighlight").attr('checked'))
                            hgTracks.enableHighlightingDialog = false;
                        if (imageV2.inPlaceUpdate) {
                            if (hgTracks.virtualSingleChrom && (newPosition.search("virt:")===0)) {
                                newPosition = genomePos.disguisePosition(newPosition); // DISGUISE
                            }
                            var params = "position=" + newPosition;
                            if (!hgTracks.enableHighlightingDialog)
                                params += "&enableHighlightingDialog=0";
                            imageV2.navigateInPlace(params, null, true);
                        } else {
                            $('body').css('cursor', 'wait');
                            if (!hgTracks.enableHighlightingDialog)
                                cart.setVarsObj({'enableHighlightingDialog': 0 },null,false); // async=false
                            document.TrackHeaderForm.submit();
                        }
                        $(this).dialog("close");
                    },
                    "Single Highlight": function() {
                        // Clear old highlight and Highlight selection
                        $(imageV2.imgTbl).imgAreaSelect({hide:true});
                        if ($("#disableDragHighlight").attr('checked'))
                            hgTracks.enableHighlightingDialog = false;
                        dragSelect.hlColor = $("#hlColorInput").val();
                        dragSelect.highlightThisRegion(newPosition, false);
                        $(this).dialog("close");
                    },
                    "Add Highlight": function() {
                        // Highlight selection
                        if ($("#disableDragHighlight").attr('checked'))
                            hgTracks.enableHighlightingDialog = false;
                        dragSelect.hlColor = $("#hlColorInput").val();
                        dragSelect.highlightThisRegion(newPosition, true);
                        $(this).dialog("close");
                    },
                    "Cancel": function() {
                        $(this).dialog("close");
                    }
                },

                open: function () { // Make zoom the focus/default action
                   $(this).parents('.ui-dialog-buttonpane button:eq(0)').focus(); 
                },

                close: function() {
                    // All exits to dialog should go through this
                    $(imageV2.imgTbl).imgAreaSelect({hide:true});
                    if ($(this).dialog("option", "revertToOriginalPos"))
                        genomePos.revertToOriginalPos();
                    if ($("#disableDragHighlight").attr('checked'))
                        $(this).remove();
                    else
                        $(this).hide();
                    $('body').css('cursor', ''); // Occasionally wait cursor got left behind
                    $("#hlColorPicker").spectrum("hide");
                }
        });
        $(dragSelectDialog).dialog('open');
        
        // put the cursor into the input field
        // we are not doing this for now - default behavior was to zoom when enter was pressed
        // so people may still expect that "enter" on the dialog will zoom.
        //var el = $("#hlColorInput")[0];
        //el.selectionStart = 0;
        //el.selectionEnd = el.value.length;
        //el.focus();

    },

    selectEnd: function (img, selection, event)
    {
        var now = new Date();
        var doIt = false;
        var rulerClicked = selection.y1 <= hgTracks.rulerClickHeight; // = drag on base position track (no shift)
        if (dragSelect.originalCursor)
            jQuery('body').css('cursor', dragSelect.originalCursor);
        if (dragSelect.escPressed)
            return false;
        // ignore releases outside of the image rectangle (allowing a 10 pixel slop)
        if (genomePos.check(img, selection)) {
            // ignore single clicks that aren't in the top of the image
            // (this happens b/c the clickClipHeight test in dragSelect.selectStart
            // doesn't occur when the user single clicks).
            doIt = (dragSelect.startTime !== null || rulerClicked);
        }

        if (doIt) {
            // dragSelect.startTime is null if mouse has never been moved
            var singleClick = (  (selection.x2 === selection.x1)
                              || dragSelect.startTime === null
                              || (now.getTime() - dragSelect.startTime) < 100);
            var newPosition = genomePos.update(img, selection, singleClick);
            if (newPosition) {
                if (event.altKey) {
                    // with the alt-key, only highlight the region, do not zoom
                    dragSelect.highlightThisRegion(newPosition, true);
                    $(imageV2.imgTbl).imgAreaSelect({hide:true});
                } else {
                    if (hgTracks.enableHighlightingDialog && !(event.metaKey || event.ctrlKey))
                        // don't show the dialog if: clicked on ruler, if dialog deactivated or meta/ctrl was pressed
                        dragSelect.selectionEndDialog(newPosition);
                    else {
                        // in every other case, show the dialog
                        $(imageV2.imgTbl).imgAreaSelect({hide:true});
                        if (imageV2.inPlaceUpdate) {
                            if (hgTracks.virtualSingleChrom && (newPosition.search("virt:")===0)) {
                                newPosition = genomePos.disguisePosition(newPosition); // DISGUISE
                            }
                            imageV2.navigateInPlace("position=" + newPosition, null, true);
                        } else {
                            jQuery('body').css('cursor', 'wait');
                            document.TrackHeaderForm.submit();
                        }
                    }
                }
            } else {
                $(imageV2.imgTbl).imgAreaSelect({hide:true});
                genomePos.revertToOriginalPos();
            }
            dragSelect.startTime = null;
            // blockMapClicks/allowMapClicks() is necessary if selectEnd was over a map item.
            setTimeout(posting.allowMapClicks,50);
            return true;
        }
    },

    load: function (firstTime)
    {
        var imgHeight = 0;
        if (imageV2.enabled)
            imgHeight = imageV2.imgTbl.innerHeight() - 1; // last little bit makes border look ok
        

        // No longer disable without ruler, because shift-drag still works
        if (typeof(hgTracks) !== "undefined") {

            if (hgTracks.rulerClickHeight === undefined || hgTracks.rulerClickHeight === null)
                hgTracks.rulerClickHeight = 0; // will be zero if no ruler track
            var heights = hgTracks.rulerClickHeight;

            dragSelect.areaSelector = jQuery((imageV2.imgTbl).imgAreaSelect({
                selectionColor:  'blue',
                outerColor:      '',
                minHeight:       imgHeight,
                maxHeight:       imgHeight,
                onSelectStart:   dragSelect.selectStart,
                onSelectChange:  dragSelect.selectChange,
                onSelectEnd:     dragSelect.selectEnd,
                autoHide:        false, // gets hidden after possible dialog
                movable:         false,
                clickClipHeight: heights
            }));

            // remove any ongoing drag-selects when the esc key is pressed anywhere for this document
            // This allows to abort zooming / highlighting
            $(document).keyup(function(e){
                if(e.keyCode === 27) {
                    $(imageV2.imgTbl).imgAreaSelect({hide:true});
                    dragSelect.escPressed = true;
                }
            });

            // hide and redraw all current highlights when the browser window is resized
            $(window).resize(function() {
                $(imageV2.imgTbl).imgAreaSelect({hide:true});
                imageV2.highlightRegion();
            });

        }
    }
};

  /////////////////////////////////////
 //// Chrom Drag/Zoom/Expand code ////
/////////////////////////////////////
jQuery.fn.chromDrag = function(){
this.each(function(){
    // Plan:
    // mouseDown: determine where in map: convert to img location: pxDown
    // mouseMove: flag drag
    // mouseUp: if no drag, then create href centered on bpDown loc with current span
    //          if drag, then create href from bpDown to bpUp
    //          if ctrlKey then expand selection to containing cytoBand(s)

    // Image dimensions all in pix
    var img = { top: -1, scrolledTop: -1, height: -1, left: -1, scrolledLeft: -1, width: -1 };  
    // chrom Dimensions beg,end,size in bases, rest in pix
    var chr = { name: "", reverse: false, beg: -1, end: -1, size: -1,
                top: -1, bottom: -1, left: -1, right: -1, width: -1 };   
    var pxDown = 0;     // pix X location of mouseDown
    var chrImg = $(this);
    var mouseIsDown   = false;
    var mouseHasMoved = false;
    var hilite = null;

    initialize();

    function initialize(){

        findDimensions();

        if (chr.top === -1)
            warn("chromIdeo(): failed to register "+this.id);
        else {
            hiliteSetup();

            $('area.cytoBand').unbind('mousedown');  // Make sure this is only bound once
            $('area.cytoBand').mousedown( function(e)
            {   // mousedown on chrom portion of image only (map items)
                updateImgOffsets();
                pxDown = e.clientX - img.scrolledLeft;
                var pxY = e.clientY - img.scrolledTop;
                if (mouseIsDown === false
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
            if (mouseHasMoved || (mouseHasMoved === false && Math.abs(relativeX) > 2)) {
                mouseHasMoved = true;
                if (isWithin(chr.left,pxX,chr.right))
                    hiliteShow(pxDown,pxX);
                else if (pxX < chr.left)
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
        if (mouseIsDown) {
            updateImgOffsets();
            var bands;
            var pxUp = e.clientX - img.scrolledLeft;
            var pxY  = e.clientY - img.scrolledTop;
            if (isWithin(0,pxY,img.height)) {  // within vertical range or else cancel
                var selRange = { beg: -1, end: -1, width: -1 };
                var dontAsk = true;

                if (e.ctrlKey) {
                    bands = findCytoBand(pxDown,pxUp);
                    if (bands.end > -1) {
                        pxDown = bands.left;
                        pxUp   = bands.right;
                        mouseHasMoved = true;
                        dontAsk = false;
                        selRange.beg = bands.beg;
                        selRange.end = bands.end;
                        hiliteShow(pxDown,pxUp);
                    }
                }
                else if (mouseHasMoved) {
                    // bounded by chrom dimensions: but must remain within image!
                    if (isWithin(-20,pxUp,chr.left)) 
                        pxUp = chr.left;
                    if (isWithin(chr.right,pxUp,img.width + 20))
                        pxUp = chr.right;

                    if ( isWithin(chr.left,pxUp,chr.right+1) ) {

                        selRange.beg = convertToBases(pxDown);
                        selRange.end = convertToBases(pxUp);

                        if (Math.abs(selRange.end - selRange.beg) < 20)
                            mouseHasMoved = false; // Drag so small: treat as simple click
                        else
                            dontAsk = false;
                    }
                }
                if (mouseHasMoved === false) { // Not else because small drag turns this off

                    hiliteShow(pxUp,pxUp);
                    var curWidth = hgTracks.winEnd - hgTracks.winStart;
                    // Notice that beg is based upon up position
                    selRange.beg = convertToBases(pxUp) - Math.round(curWidth/2); 
                    selRange.end  = selRange.beg + curWidth;
                }
                if (selRange.end > -1) {
                    // prompt, then submit for new position
                    selRange = rangeNormalizeToChrom(selRange,chr);
                    if (mouseHasMoved === false) { // Update highlight by converting bp back to pix
                        pxDown = convertFromBases(selRange.beg);
                        pxUp = convertFromBases(selRange.end);
                        hiliteShow(pxDown,pxUp);
                    }
                    //if ((selRange.end - selRange.beg) < 50000)
                    //    dontAsk = true;
                    if (dontAsk
                    || confirm("Jump to new position:\n\n"+chr.name+":"+commify(selRange.beg)+
                               "-"+commify(selRange.end)+" size:"+commify(selRange.width)) ) {
                        genomePos.setByCoordinates(chr.name, selRange.beg, selRange.end);
                        // Stop the presses :0)
                        $('area.cytoBand').mousedown( function(e) { return false; });
                        if (imageV2.backSupport) {
                            imageV2.navigateInPlace("position=" +  
                                    encodeURIComponent(genomePos.get().replace(/,/g,'')) + 
                                    "&findNearest=1",null,true);
                            hiliteCancel();
                        } else
                            document.TrackHeaderForm.submit();
                        return true; // Make sure the setTimeout below is not called.
                    }
                }
            }
            hiliteCancel();
            setTimeout(posting.allowMapClicks,50);
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
        if (chr.reverse)
            offset = 1 - offset;
        return Math.round(offset * chr.size);
    }
    function convertFromBases(bases)
    {   // Simple utility to convert bases to pix
        var offset = bases/chr.size;
        if (chr.reverse)
            offset = 1 - offset;
        return Math.round(offset * chr.width) + chr.left;
    }

    function findDimensions()
    {   // Called at init: determine the dimensions of chrom from 'cytoband' map items
        var lastX = -1;
        $('area.cytoBand').each(function(ix) {
            var loc = this.coords.split(",");
            if (loc.length === 4) {
                var myLeft  = parseInt(loc[0]);
                var myRight = parseInt(loc[2]);
                if (chr.top === -1) {
                    chr.left   = myLeft;
                    chr.right  = myRight;
                    chr.top    = parseInt(loc[1]);
                    chr.bottom = parseInt(loc[3]);
                } else {
                    if (chr.left  > myLeft)
                        chr.left  = myLeft;
                    if (chr.right < parseInt(loc[2]))
                        chr.right = parseInt(loc[2]);
                }

                var range = this.title.substr(this.title.lastIndexOf(':')+1);
                var pos = range.split('-');
                if (pos.length === 2) {
                    if (chr.name.length === 0) {
                        chr.beg = parseInt(pos[0]);
                        //chr.end = parseInt(pos[1]);
                        chr.name = this.title.substring(this.title.lastIndexOf(' ')+1,
                                                        this.title.lastIndexOf(':'));
                    } else {
                        if (chr.beg > parseInt(pos[0]))
                            chr.beg = parseInt(pos[0]);
                    }
                    if (chr.end < parseInt(pos[1])) {
                        chr.end = parseInt(pos[1]);
                        if (lastX === -1)
                            lastX = myRight;
                        else if (lastX > myRight)
                            chr.reverse = true;  // end is advancing, but X is not, so reverse
                    } else if (lastX !== -1 && lastX < myRight)
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
    {   // Called when mouseup and ctrl: Find the bounding cytoband dimensions (in pix and bases)
        var cyto = { left: -1, right: -1, beg: -1, end: -1 };
        $('area.cytoBand').each(function(ix) {
            var loc = this.coords.split(",");
            if (loc.length === 4) {
                var myLeft  = parseInt(loc[0]);
                var myRight = parseInt(loc[2]);
                var range;
                var pos;
                if (cyto.left === -1 || cyto.left > myLeft) {
                    if ( isWithin(myLeft,pxDown,myRight) || isWithin(myLeft,pxUp,myRight) ) {
                        cyto.left  = myLeft;
                        range = this.title.substr(this.title.lastIndexOf(':')+1);
                        pos = range.split('-');
                        if (pos.length === 2) {
                            cyto.beg  = (chr.reverse ? parseInt(pos[1]) : parseInt(pos[0]));
                        }
                    }
                }
                if (cyto.right === -1 || cyto.right < myRight) {
                    if ( isWithin(myLeft,pxDown,myRight) || isWithin(myLeft,pxUp,myRight) ) {
                        cyto.right = myRight;
                        range = this.title.substr(this.title.lastIndexOf(':')+1);
                        pos = range.split('-');
                        if (pos.length === 2) {
                            cyto.end  = (chr.reverse ? parseInt(pos[0]) : parseInt(pos[1]));
                        }
                    }
                }
            }
        });
        return cyto;
    }
    function rangeNormalizeToChrom(selection,chrom)
    {   // Called before presenting or using base range: make sure chrom selection
        // is within chrom range
        if (selection.end < selection.beg) {
            var tmp = selection.end;
            selection.end = selection.beg;
            selection.beg = tmp;
        }
        selection.width = (selection.end - selection.beg);
        selection.beg += 1;
        if (selection.beg < chrom.beg) {
            selection.beg = chrom.beg;
            selection.end = chrom.beg + selection.width;
        }
        if (selection.end > chrom.end) {
            selection.end = chrom.end;
            selection.beg = chrom.end - selection.width;
            if (selection.beg < chrom.beg) { // spans whole chrom
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
        if (cur < down) {
            begX = cur + img.left;
            wide = (down - cur);
        } else {
            begX = down + img.left;
            wide = (cur - down);
        }
        $(hilite).css({ left: begX + 'px', width: wide + 'px', top: topY + 'px',
                        height: high + 'px', display:'' });
        $(hilite).show();
    }
    function hiliteCancel(left,width,top,height)
    {   // Called on mouseup: Make green drag hilite disappear when no longer wanted
        $(hilite).hide();
        $(hilite).css({ left: '0px', width: '0px', top: '0px', height: '0px' });
    }

    function hiliteSetup()
    {   // Called on init: setup of drag region hilite (but don't show yet)
        if (hilite === null) {  // setup only once
            hilite = jQuery("<div id='chrHi'></div>");
            $(hilite).css({ backgroundColor: 'green', opacity: 0.4, borderStyle: 'solid',
                            borderWidth: '1px', bordercolor: '#0000FF' });
            $(hilite).css({ display: 'none', position: 'absolute', overflow: 'hidden', zIndex: 1 });
            jQuery($(chrImg).parents('body')).append($(hilite));
        }
        return hilite;
    }

    function updateImgOffsets()
    {   // Called on mousedown: Gets the current offsets
        var offs = $(chrImg).offset();
        img.top  = Math.round(offs.top );
        img.left = Math.round(offs.left);
        img.scrolledTop  = img.top  - $("body").scrollTop();
        img.scrolledLeft = img.left - $("body").scrollLeft();
        if (theClient.isIePre11()) {
            img.height = $(chrImg).outerHeight();
            img.width  = $(chrImg).outerWidth();
        } else {
            img.height = $(chrImg).height();
            img.width  = $(chrImg).width();
        }
        return img;
    }
});
};

  ///////////////////////////
 //// Drag Reorder Code ////
///////////////////////////
var dragReorder = {

    setOrder: function (table)
    {   // Sets the 'order' value for the image table after a drag reorder
        var varsToUpdate = {};
        $("tr.imgOrd").each(function (i) {
            if ($(this).attr('abbr') !== $(this).attr('rowIndex').toString()) {
                $(this).attr('abbr',$(this).attr('rowIndex').toString());
                var name = this.id.substring('tr_'.length) + '_imgOrd';
                varsToUpdate[name] = $(this).attr('abbr');
            }
        });
        if (objNotEmpty(varsToUpdate)) {
            cart.setVarsObj(varsToUpdate);
            imageV2.markAsDirtyPage();
        }
    },

    sort: function (table)
    {   // Sets the table row order to match the order of the abbr attribute.
        // This is needed for back-button, and for visBox changes combined with refresh.
        var tbody = $(table).find('tbody')[0];
        if (!tbody)
            tbody = table;
        
        // Do we need to sort?
        var trs = tbody.rows;
        var needToSort = false;
        $(trs).each(function(ix) {
            if ($(this).attr('abbr') !== $(this).attr('rowIndex').toString()) {
                needToSort = true;
                return false;  // break for each() loops
            }
        });
        if (!needToSort)
            return false;
            
        // Create array of tr holders to sort
        var ary = [];
        $(trs).each(function(ix) {  // using sortTable found in utils.js
            ary.push(new sortTable.field(parseInt($(this).attr('abbr')),false,this));
        });

        // Sort the array
        ary.sort(sortTable.fieldCmp);

        // most efficient reload of sorted rows I have found
        var sortedRows = jQuery.map(ary, function(ary, i) { return ary.row; });
        $(tbody).append( sortedRows ); // removes tr from current position and adds to end.
        return true;
    },

    showCenterLabel: function (tr, show)
    {   // Will show or hide centerlabel as requested
        // adjust button, sideLabel height, sideLabelOffset and centerlabel display

        if (!$(tr).hasClass('clOpt'))
            return;
        var center = normed($(tr).find(".sliceDiv.cntrLab"));
        if (!center)
            return;
        var seen = ($(center).css('display') !== 'none');
        if (show === seen)
            return;

        var centerHeight = $(center).height();

        var btn = normed($(tr).find("p.btn"));
        var side = normed($(tr).find(".sliceDiv.sideLab"));
        if (btn && side) {
            var sideImg = normed($(side).find("img"));
            if (sideImg) {
                var top = parseInt($(sideImg).css('top'));
                if (show) {
                    $(btn).css('height',$(btn).height() + centerHeight);
                    $(side).css('height',$(side).height() + centerHeight);
                    top += centerHeight; // top is a negative number
                    $(sideImg).css( {'top': top.toString() + "px" });
                    $( center ).show();
                } else if (!show) {
                    $(btn).css('height',$(btn).height() - centerHeight);
                    $(side).css('height',$(side).height() - centerHeight);
                    top -= centerHeight; // top is a negative number
                    $(sideImg).css( {'top': top.toString() + "px" });
                    $( center ).hide();
                }
            }
        }
    },

    getContiguousRowSet: function (row)
    {   // Returns the set of rows that are of the same class and contiguous
        if (!row)
            return null;
        var btn = $( row ).find("p.btn");
        if (btn.length === 0)
            return null;
        var classList = $( btn ).attr("class").split(" ");
        var matchClass = classList[0];
        var table = $(row).parents('table#imgTbl')[0];
        var rows = $(table).find('tr');

        // Find start index
        var startIndex = $(row).attr('rowIndex');
        var endIndex = startIndex;
        for (var ix=startIndex-1; ix >= 0; ix--) {
            btn = $( rows[ix] ).find("p.btn");
            if (btn.length === 0)
                break;
            classList = $( btn ).attr("class").split(" ");
            if (classList[0] !== matchClass)
                break;
            startIndex = ix;
        }

        // Find end index
        for (var rIx=endIndex; rIx<rows.length; rIx++) {
            btn = $( rows[rIx] ).find("p.btn");
            if (btn.length === 0)
                break;
            classList = $( btn ).attr("class").split(" ");
            if (classList[0] !== matchClass)
                break;
            endIndex = rIx;
        }
        return rows.slice(startIndex,endIndex+1); // endIndex is 1 based!
    },

    getCompositeSet: function (row)
    {   // Returns the set of rows that are of the same class and contiguous
        if (!row)
            return null;
        var rowId = $(row).attr('id').substring('tr_'.length);
        var rec = hgTracks.trackDb[rowId];
        if (tdbIsSubtrack(rec) === false)
            return null;

        var rows = $('tr.trDraggable:has(p.' + rec.parentTrack+')');
        return rows;
    },

    zipButtons: function (table)
    {   // Goes through the image and binds composite track buttons when adjacent
        var rows = $(table).find('tr');
        var lastClass="";
        var lastBtn = null;
        var lastSide = null;
        var lastMatchesLast=false;
        var lastBlue=true;
        var altColors=false;
        var count=0;
        var countN=0;
        for (var ix=0; ix<rows.length; ix++) {    // Need to have buttons in order
            var btn = $( rows[ix] ).find("p.btn");
            var side = $( rows[ix] ).find(".sliceDiv.sideLab"); // added by GALT
            if (btn.length === 0)
                continue;
            var classList = $( btn ).attr("class").split(" ");
            var curMatchesLast=(classList[0] === lastClass);

            // centerLabels may be conditionally seen
            if ($( rows[ix] ).hasClass('clOpt')) {
                // if same composite and previous also centerLabel optional then hide center label
                if (curMatchesLast && $( rows[ix - 1] ).hasClass('clOpt'))
                    dragReorder.showCenterLabel(rows[ix],false);
                else
                    dragReorder.showCenterLabel(rows[ix],true);
            }

            // On with buttons
            if (lastBtn) {
                $( lastBtn ).removeClass('btnN btnU btnL btnD');
                if (curMatchesLast && lastMatchesLast) {
                    $( lastBtn ).addClass('btnL');
                    $( lastBtn ).css('height', $( lastSide ).height() - 0);  // added by GALT
                } else if (lastMatchesLast) {
                    $( lastBtn ).addClass('btnU');
                    $( lastBtn ).css('height', $( lastSide ).height() - 1);  // added by GALT
                } else if (curMatchesLast) {
                    $( lastBtn ).addClass('btnD');
                    $( lastBtn ).css('height', $( lastSide ).height() - 2);  // added by GALT
                } else {
                    $( lastBtn ).addClass('btnN');
                    $( lastBtn ).css('height', $( lastSide ).height() - 3);  // added by GALT
                    countN++;
                }
                count++;
                if (altColors) {
                    // lastMatch and lastBlue or not lastMatch and notLastBlue
                    lastBlue = (lastMatchesLast === lastBlue);
                    if (lastBlue)    // Too  smart by 1/3rd
                        $( lastBtn ).addClass(    'btnBlue' );
                    else
                        $( lastBtn ).removeClass( 'btnBlue' );
                }
            }
            lastMatchesLast = curMatchesLast;
            lastClass = classList[0];
            lastBtn = btn;
            lastSide = side;
        }
        if (lastBtn) {
            $( lastBtn ).removeClass('btnN btnU btnL btnD');
            if (lastMatchesLast) {
                $( lastBtn ).addClass('btnU');
                $( lastBtn ).css('height', $( lastSide ).height() - 1);  // added by GALT
            } else {
                $( lastBtn ).addClass('btnN');
                $( lastBtn ).css('height', $( lastSide ).height() - 3);  // added by GALT
                countN++;
            }
            if (altColors) {
                // lastMatch and lastBlue or not lastMatch and notLastBlue
                lastBlue = (lastMatchesLast === lastBlue); 
                if (lastBlue)    // Too  smart by 1/3rd
                    $( lastBtn ).addClass(    'btnBlue' );
                else
                    $( lastBtn ).removeClass( 'btnBlue' );
            }
            count++;
        }
        //warn("Zipped "+count+" buttons "+countN+" are independent.");
    },

    dragHandleMouseOver: function ()
    {   // Highlights a single row when mouse over a dragHandle column (sideLabel and buttons)
        if ( ! jQuery.tableDnD ) {
            //var handle = $("td.dragHandle");
            //$(handle)
            //    .unbind('mouseenter')//, jQuery.tableDnD.mousemove);
            //    .unbind('mouseleave');//, jQuery.tableDnD.mouseup);
            return;
        }
        if ( ! jQuery.tableDnD.dragObject ) {
            $( this ).parents("tr.trDraggable").addClass("trDrag");
        }
    },

    dragHandleMouseOut: function ()
    {   // Ends row highlighting by mouse over
        $( this ).parents("tr.trDraggable").removeClass("trDrag");
    },

    buttonMouseOver: function ()
    {   // Highlights a composite set of buttons, regarless of whether tracks are adjacent
        if ( ! jQuery.tableDnD || ! jQuery.tableDnD.dragObject ) {
            var classList = $( this ).attr("class").split(" ");
            var btns = $( "p." + classList[0] );
            $( btns ).removeClass('btnGrey');
            $( btns ).addClass('btnBlue');
            if (jQuery.tableDnD) {
                var rows = dragReorder.getContiguousRowSet($(this).parents('tr.trDraggable')[0]);
                if (rows)
                    $( rows ).addClass("trDrag");
            }
        }
    },

    buttonMouseOut: function ()
    {   // Ends composite highlighting by mouse over
        var classList = $( this ).attr("class").split(" ");
        var btns = $( "p." + classList[0] );
        $( btns ).removeClass('btnBlue');
        $( btns ).addClass('btnGrey');
        if (jQuery.tableDnD) {
            var rows = dragReorder.getContiguousRowSet($(this).parents('tr.trDraggable')[0]);
            if (rows)
            $( rows ).removeClass("trDrag");
        }
    },

    trMouseOver: function (e)
    {   // Trying to make sure there is always a imageV2.lastTrack so that we know where we are
        var id = '';
        var a = /tr_(.*)/.exec($(this).attr('id'));  // voodoo
        if (a && a[1]) {
            id = a[1];
        }
        if (id.length > 0) {
            if ( ! imageV2.lastTrack || imageV2.lastTrack.id !== id)
                imageV2.lastTrack = rightClick.makeMapItem(id); 
                // currentMapItem gets set by mapItemMapOver.   This is just backup
        }
    },

    mapItemMouseOver: function ()
    {
        // Record data for current map area item
        var id = this.id;
        if (!id || id.length === 0) {
            id = '';
            var tr = $( this ).parents('tr.imgOrd');
            if ( $(tr).length === 1 ) {
                var a = /tr_(.*)/.exec($(tr).attr('id'));  // voodoo
                if (a && a[1]) {
                    id = a[1];
                }
            }
        }
        if (id.length > 0) {
            rightClick.currentMapItem = rightClick.makeMapItem(id);
            if (rightClick.currentMapItem) {
                rightClick.currentMapItem.href = this.href;
                rightClick.currentMapItem.title = this.title;

                // Handle linked features with separate clickmaps for each exon/intron 
                if ((this.title.indexOf('Exon ') === 0) || (this.title.indexOf('Intron ') === 0)) {
                    // if the title is Exon ... or Intron ... 
                    // then search for the sibling with the same href
                    // that has the real title item label
                    var elem = this.parentNode.firstChild;
                    while (elem) {
                        if ((elem.href === this.href)
                            && !((elem.title.indexOf('Exon ') === 0) || (elem.title.indexOf('Intron ') === 0))) {
                            rightClick.currentMapItem.title = elem.title;
                            break;
                        }
                        elem = elem.nextSibling;
                    }
                }

            }
        }
    },

    mapItemMouseOut: function ()
    {
        imageV2.lastTrack = rightClick.currentMapItem; // Just a backup
        rightClick.currentMapItem = null;
    },

    init: function ()
    {   // Make side buttons visible (must also be called when updating rows in the imgTbl).
        var btns = $("p.btn");
        if (btns.length > 0) {
            dragReorder.zipButtons($('#imgTbl'));
            $(btns).mouseenter( dragReorder.buttonMouseOver );
            $(btns).mouseleave( dragReorder.buttonMouseOut  );
            $(btns).show();
        }
        var handle = $("td.dragHandle");
        if (handle.length > 0) {
            $(handle).mouseenter( dragReorder.dragHandleMouseOver );
            $(handle).mouseleave( dragReorder.dragHandleMouseOut  );
        }

        // setup mouse callbacks for the area tags
        $(imageV2.imgTbl).find("tr").mouseover( dragReorder.trMouseOver );

        $(".area").each( function(t) {
                            this.onmouseover = dragReorder.mapItemMouseOver;
                            this.onmouseout = dragReorder.mapItemMouseOut;
                            this.onclick = posting.mapClk;
                        });
    }
};


  //////////////////////////
 //// Drag Scroll code ////
//////////////////////////
jQuery.fn.panImages = function(){
    // globals across all panImages
    genomePos.original = genomePos.getOriginalPos(); // redundant but makes certain original is set.
    var leftLimit   = hgTracks.imgBoxLeftLabel * -1;
    var rightLimit  = (hgTracks.imgBoxPortalWidth - hgTracks.imgBoxWidth + leftLimit);
    var only1xScrolling = ((hgTracks.imgBoxWidth - hgTracks.imgBoxPortalWidth) === 0);
    var prevX       = (hgTracks.imgBoxPortalOffsetX + hgTracks.imgBoxLeftLabel) * -1;
    var portalWidth = 0;
    var portalAbsoluteX = 0;
    var savedPosition;
    var highlightAreas  = null; // Used to ensure dragSelect highlight will scroll. 

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

    if (!pan || !pic) {
        throw "Not a div with child image! 'panImages' can only be used with divs contain images.";
    }

    // globals across all panImages
    portalWidth     = $(pan).width();
    portalAbsoluteX = $(pan).offset().left;
    // globals to one panImage
    var newX        = 0;
    var mouseDownX  = 0;
    var mouseIsDown = false;
    var beyondImage = false;
    var atEdge      = false;

    initialize();

    function initialize(){

        $(pan).parents('td.tdData').mousemove(function(e) {
            if (e.shiftKey)
                $(this).css('cursor',"crosshair");  // shift-dragZoom
            else if ( theClient.isIePre11() )     // IE will override map item cursors if this gets set
                $(this).css('cursor',"");  // normal pointer when not over clickable item
        });

        panAdjustHeight(prevX);

        pan.mousedown(function(e){
             if (e.which > 1 || e.button > 1 || e.shiftKey || e.metaKey || e.altKey || e.ctrlKey)
                 return true;
            if (mouseIsDown === false) {
                if (rightClick.menu) {
                    rightClick.menu.hide();
                }
                mouseIsDown = true;
                mouseDownX = e.clientX;
                highlightAreas = $('.highlightItem');
                atEdge = (!beyondImage && (prevX >= leftLimit || prevX <= rightLimit));
                $(document).bind('mousemove',panner);
                $(document).bind( 'mouseup', panMouseUp);  // Will exec only once
                return false;
            }
        });
    }

    function panner(e) {
        //if (!e) e = window.event;
        if ( mouseIsDown ) {
            var relativeX = (e.clientX - mouseDownX);

            if (relativeX !== 0) {
                if (posting.mapClicksAllowed()) {
                    // need to throw up a z-index div.  Wait mask?
                    savedPosition = genomePos.get();
                    dragMaskShow();
                    posting.blockMapClicks();
                }
                var decelerator = 1;
                //var wingSize    = 1000; // 0 stops the scroll at the edges.
                // Remeber that offsetX (prevX) is negative
                newX = prevX + relativeX;
                if ( newX >= leftLimit ) { // scrolled all the way to the left
                    if (atEdge) {  // Do not drag straight off edge.  Force second drag
                        beyondImage = true;
                        newX = leftLimit + (newX - leftLimit)/decelerator;// slower
                        //if (newX >= leftLimit + wingSize) // Don't go too far over the edge!
                        //    newX =  leftLimit + wingSize;
                    } else
                        newX = leftLimit;

                } else if ( newX < rightLimit ) { // scrolled all the way to the right
                    if (atEdge) {  // Do not drag straight off edge.  Force second drag
                        beyondImage = true;
                        newX = rightLimit - (rightLimit - newX)/decelerator;// slower
                        //if (newX < rightLimit - wingSize) // Don't go too far over the edge!
                        //    newX = rightLimit - wingSize;
                    } else
                        newX = rightLimit;

                } else if (newX >= rightLimit && newX < leftLimit)
                    beyondImage = false; // could have scrolled back without mouse up

                posStatus = panUpdatePosition(newX,true);
                newX = posStatus.newX;
                // do not update highlights if we are at the end of a chromsome
                if (!posStatus.isOutsideChrom)
                    scrollHighlight(relativeX);

                var nowPos = newX.toString() + "px";
                $(".panImg").css( {'left': nowPos });
                $('.tdData').css( {'backgroundPosition': nowPos } );
                if (!only1xScrolling)
                    panAdjustHeight(newX);  // Will dynamically resize image while scrolling.
            }
        }
    }
    function panMouseUp(e) {  // Must be a separate function instead of pan.mouseup event.
        //if (!e) e = window.event;
        if (mouseIsDown) {

            dragMaskClear();
            $(document).unbind('mousemove',panner);
            $(document).unbind('mouseup',panMouseUp);
            mouseIsDown = false;
            // timeout incase the dragSelect.selectEnd was over a map item. select takes precedence.
            setTimeout(posting.allowMapClicks,50); 

            // Outside image?  Then abandon.
            var curY = e.pageY;
            var imgTbl = $('#imgTbl');
            var north = $(imgTbl).offset().top;
            var south = north + $(imgTbl).height();
            if (curY < north || curY > south) {
                atEdge = false;
                beyondImage = false;
                if (savedPosition)
                    genomePos.set(savedPosition);
                var oldPos = prevX.toString() + "px";
                $(".panImg").css( {'left': oldPos });
                $('.tdData').css( {'backgroundPosition': oldPos } );
                if (highlightAreas)
                    imageV2.highlightRegion();
                return true;
            }

            // Do we need to fetch anything?
            if (beyondImage) {
                if (imageV2.inPlaceUpdate) {
                    var pos = parsePosition(genomePos.get());
                    imageV2.navigateInPlace("position=" +
                            encodeURIComponent(pos.chrom + ":" + pos.start + "-" + pos.end),
                            null, true);
                } else {
                    document.TrackHeaderForm.submit();
                }
                return true; // Make sure the setTimeout below is not called.
            }

            // Just a normal scroll within a >1X image
            if (prevX !== newX) {
                prevX = newX;
                if (!only1xScrolling) {
                    //panAdjustHeight(newX); // Will resize image AFTER scrolling.
                    // Important, since AJAX could lead to reinit after this within bounds scroll
                    hgTracks.imgBoxPortalOffsetX = (prevX * -1) - hgTracks.imgBoxLeftLabel;
                    hgTracks.imgBoxPortalLeft = newX.toString() + "px";
                }
            }
        }
    }
    });  // end of this.each(function(){

    function panUpdatePosition(newOffsetX,bounded)
    {
        // Updates the 'position/search" display with change due to panning
        var closedPortalStart = hgTracks.imgBoxPortalStart + 1;   // Correction for half open
        var portalWidthBases = hgTracks.imgBoxPortalEnd - closedPortalStart;
        var portalScrolledX  = hgTracks.imgBoxPortalOffsetX+hgTracks.imgBoxLeftLabel + newOffsetX;
        var recalculate = false;

        var newPortalStart = 0;
        if (hgTracks.revCmplDisp)
            newPortalStart = closedPortalStart +     // As offset goes down, so do bases seen.
                                Math.round(portalScrolledX*hgTracks.imgBoxBasesPerPixel);
        else
            newPortalStart = closedPortalStart -     // As offset goes down, bases seen goes up!
                                Math.round(portalScrolledX*hgTracks.imgBoxBasesPerPixel);
        if (newPortalStart < hgTracks.chromStart && bounded) {     // Stay within bounds
            newPortalStart = hgTracks.chromStart;
            recalculate = true;
        }
        var newPortalEnd = newPortalStart + portalWidthBases;
        if (newPortalEnd > hgTracks.chromEnd && bounded) {
            newPortalEnd = hgTracks.chromEnd;
            newPortalStart = newPortalEnd - portalWidthBases;
            recalculate = true;
        }
        if (newPortalStart > 0) {
            var newPos = hgTracks.chromName + ":" + newPortalStart + "-" + newPortalEnd;
            genomePos.set(newPos); // no need to change the size
        }
        if (recalculate && hgTracks.imgBoxBasesPerPixel > 0) { 
            // Need to recalculate X for bounding drag
            portalScrolledX = (closedPortalStart - newPortalStart) / hgTracks.imgBoxBasesPerPixel;
            newOffsetX = portalScrolledX - (hgTracks.imgBoxPortalOffsetX+hgTracks.imgBoxLeftLabel);
        }

        ret = {};
        ret.newX = newOffsetX;
        ret.isOutsideChrom = recalculate;
        return ret;
    }
    function mapTopAndBottom(mapName,east,west)
    {
    // Find the top and bottom px given left and right boundaries
        var mapPortal = { top: -10, bottom: -10 };
        var items = $("map[name='"+mapName+"']").children();
        if ($(items).length>0) {
            $(items).each(function(t) {
                var loc = this.coords.split(",");
                var aleft   = parseInt(loc[0]);
                var aright  = parseInt(loc[2]);
                if (aleft < west && aright >= east) {
                    var atop    = parseInt(loc[1]);
                    var abottom = parseInt(loc[3]);
                    if (mapPortal.top    < 0 ) {
                        mapPortal.top    = atop;
                        mapPortal.bottom = abottom;
                    } else if (mapPortal.top > atop) {
                            mapPortal.top = atop;
                    } else if (mapPortal.bottom < abottom) {
                            mapPortal.bottom = abottom;
                    }
                }
            });
        }
        return mapPortal;
    }
    function panAdjustHeight(newOffsetX) {
        // Adjust the height of the track data images so that bed items scrolled off screen
        // do not waste vertical real estate

        // Is the > 1x?
        if (only1xScrolling)
            return;

        var east = newOffsetX * -1;
        var west = east + portalWidth;
        $(".panImg").each(function(t) {
            var mapid  = this.id.replace('img_','map_');
            var hDiv   = $(this).parent();
            var north  = parseInt($(this).css("top")) * -1;
            var south  = north + $(hDiv).height();

            var mapPortal = mapTopAndBottom(mapid,east,west);
            if (mapPortal.top > 0) {
                var topdif = Math.abs(mapPortal.top - north);
                var botdif = Math.abs(mapPortal.bottom - south);
                if (topdif > 2 || botdif > 2) {
                    $(hDiv).height( mapPortal.bottom - mapPortal.top );
                    north = mapPortal.top * -1;
                    $(this).css( {'top': north.toString() + "px" });

                    // Need to adjust side label height as well!
                    var imgId = this.id.split("_");
                    var titlePx = 0;
                    var center = $("#img_center_"+imgId[2]);
                    if (center.length > 0) {
                        titlePx = $(center).parent().height();
                        north += titlePx;
                    }
                    var side = $("#img_side_"+imgId[2]);
                    if (side.length > 0) {
                        $(side).parent().height( mapPortal.bottom - mapPortal.top + titlePx);
                        $(side).css( {'top': north.toString() + "px" });
                    }
                    var btn = $("#p_btn_"+imgId[2]);
                    if (btn.length > 0) {
                        $(btn).height( mapPortal.bottom - mapPortal.top + titlePx);
                    } else {
                        btn = $("#img_btn_"+imgId[2]);
                        if (btn.length > 0) {
                            $(btn).parent().height( mapPortal.bottom - mapPortal.top + titlePx);
                            $(btn).css( {'top': top.toString() + "px" });
                        }
                    }
                }
            }
        });
        dragMaskResize();  // Resizes the dragMask to match current image size
    }

    function dragMaskShow() 
    {   // Sets up the dragMask to show grabbing cursor within image
        // and not allowed north and south of image
        var imgTbl = $('#imgTbl');
        // Find or create the waitMask (which masks the whole page)
        var dragMask = normed($('div#dragMask'));
        if (!dragMask) {
            $("body").prepend("<div id='dragMask' class='waitMask'></div>");
            dragMask = $('div#dragMask');
        }

        $('body').css('cursor','not-allowed');
        $(dragMask).css('cursor',"url(../images/grabbing.cur),w-resize");
        $(dragMask).css({opacity:0.0,display:'block',
                        top: $(imgTbl).position().top.toString() + 'px',
                        height: $(imgTbl).height().toString() + 'px' });
    }

    function dragMaskResize() 
    {   // Resizes dragMask (called when image is dynamically resized in >1x scrolling)
        var imgTbl = $('#imgTbl');
        // Find or create the waitMask (which masks the whole page)
        var dragMask = normed($('div#dragMask'));
        if (dragMask) {
            $(dragMask).height( $(imgTbl).height() );
        }
    }

    function dragMaskClear() {        // Clears the dragMask
        $('body').css('cursor','auto');
        var  dragMask = normed($('#dragMask'));
        if (dragMask)
            $(dragMask).hide();
    }

    function scrollHighlight(relativeX) 
    // Scrolls the highlight region if one exists
    {        
        if (highlightAreas) {
            for (i=0; i<highlightAreas.length; i++) {
                highlightArea = highlightAreas[i];
                // Best to have a left and right, then min/max the edges, then set width
                var hiOffset = $(highlightArea).offset();
                var hiDefinedLeft  = $(highlightArea).data('leftPixels');
                var hiDefinedWidth = $(highlightArea).data('widthPixels');
                hiOffset.left = Math.max(hiDefinedLeft + relativeX,
                                         portalAbsoluteX);
                var right     = Math.min(hiDefinedLeft + hiDefinedWidth + relativeX,
                                         portalAbsoluteX + portalWidth);
                var newWidth = Math.max(right - hiOffset.left,0);
                if (hiDefinedWidth !== newWidth)
                    $(highlightArea).width(newWidth);
                $(highlightArea).offset(hiOffset);
            }
        }
    }

};

  ///////////////////////////////////////
 //// rightClick (aka context menu) ////
///////////////////////////////////////
var rightClick = {

    menu: null,
    selectedMenuItem: null,   // currently choosen context menu item (via context menu).
    floatingMenuItem: null,
    currentMapItem:   null,
    supportZoomCodon: false,  // turns on experimental feature (currently only in larry's tree).
    clickedHighlightIdx : null,  // the index (0,1,...) of the highlight item that overlaps the last right-click

    makeMapItem: function (id)
    {   // Create a dummy mapItem on the fly
        // (for objects that don't have corresponding entry in the map).
        if (id && id.length > 0 && hgTracks.trackDb) {
            var title;
            var rec = hgTracks.trackDb[id];
            if (rec) {
                title = rec.shortLabel;
            } else {
                title = id;
            }
            return {id: id, title: "configure " + title};
        } else {
            return null;
        }
    },

    findMapItem: function (e)
    {   // Find mapItem for given event; returns item object or null if none found.

        if (rightClick.currentMapItem) {
            return rightClick.currentMapItem;
        }

        // rightClick for non-map items that can be resolved to their parent tr and
        // then trackName (e.g. items in gray bar)
        var tr = $( e.target ).parents('tr.imgOrd');
        if ($(tr).length === 1) {
            var a = /tr_(.*)/.exec($(tr).attr('id'));  // voodoo
            if (a && a[1]) {
                var id = a[1];
                return rightClick.makeMapItem(id);
            }
        }
        return null;
    },

    windowOpenFailedMsg: function ()
    {
        warn("Your web browser prevented us from opening a new window.\n\n" +
             "Please change your browser settings to allow pop-up windows from " +
             document.domain + ".");
    },

    handleZoomCodon: function (response, status)
    {
        var json = JSON.parse(response);
        if (json.pos) {
            genomePos.set(json.pos, 3);
            if (document.TrackForm)
                document.TrackForm.submit();
            else
                document.TrackHeaderForm.submit();
        } else {
            alert(json.error);
        }
    },

    handleViewImg: function (response, status)
    {   // handles view image response, which must get new image without imageV2 gimmickery
        jQuery('body').css('cursor', '');
        var str = "<IMG[^>]*SRC='([^']+)'";
        var reg = new RegExp(str);
        var a = reg.exec(response);
        if (a && a[1]) {
            if ( ! window.open(a[1]) ) {
                rightClick.windowOpenFailedMsg();
            }
            return;
        }
        warn("Couldn't parse out img src");
    },

    myPrompt: function (msg, callback)
    {   // replacement for prompt; avoids misleading/confusing security warnings which are caused
        // by prompt in IE 7+.   Callback is called if user presses "OK".
        $("body").append("<div id = 'myPrompt'><div id='dialog' title='Basic dialog'><form>" +
                            msg + "<input id='myPromptText' value=''></form>");
        $("#myPrompt").dialog({
                                modal: true,
                                closeOnEscape: true,
                                buttons: { "OK": function() {
                                                    var myPromptText = $("#myPromptText").val();
                                                    $(this).dialog("close");
                                                    callback(myPromptText);
                                                }
                                        }
                            });
    },

    hit: function (menuItemClicked, menuObject, cmd, args)
    {
        setTimeout( function() {
                        rightClick.hitFinish(menuItemClicked, menuObject, cmd, args);
                    }, 1);
    },

    hitFinish: function (menuItemClicked, menuObject, cmd, args)
    {   // dispatcher for context menu hits
        var id = rightClick.selectedMenuItem.id;
        var url = null;  // TODO: Break this giant routine with shared vars into some sub-functions
        var href = null;
        var rec = null;
        var row = null;
        var rows = null;
        var selectUpdated = null;
                function mySuccess() {}
        if (menuObject.shown) {
            // warn("Spinning: menu is still shown");
            setTimeout(function() { rightClick.hitFinish(menuItemClicked, menuObject, cmd); }, 10);
            return;
        }
        if (cmd === 'selectWholeGene' || cmd === 'getDna' || cmd === 'highlightItem') {
                // bring whole gene into view or redirect to DNA screen.
                href = rightClick.selectedMenuItem.href;
                var chrom, chromStart, chromEnd;
                // Many links leave out the chrom (b/c it's in the server side cart as "c")
                // var chrom = hgTracks.chromName; // This is no longer acceptable
                // with multi-window capability drawing multiple positions on multiple chroms.
                var a = /hgg_chrom=(\w+)&/.exec(href);
                if (a) {
                    if (a && a[1])
                        chrom = a[1];
                    a = /hgg_start=(\d+)/.exec(href);
                    if (a && a[1])
                        chromStart = parseInt(a[1]) + 1;
                    a = /hgg_end=(\d+)/.exec(href);
                    if (a && a[1])
                        chromEnd = parseInt(a[1]);
                } else {
                    // a = /hgc.*\W+c=(\w+)/.exec(href);
                    a = /hgc.*\W+c=(\w+)/.exec(href);
                    if (a && a[1])
                        chrom = a[1];
                    a = /o=(\d+)/.exec(href);
                    if (a && a[1])
                        chromStart = parseInt(a[1]) + 1;
                    a = /t=(\d+)/.exec(href);
                    if (a && a[1])
                        chromEnd = parseInt(a[1]);
                }
                if (!chrom || chrom.length === 0 || !chromStart || !chromEnd) {// 1-based chromStart
                    warn("couldn't parse out genomic coordinates");
                } else {
                    if (cmd === 'getDna') {
                        // NOTE: this should be shared with URL generation for getDna blue bar menu
                        url = "../cgi-bin/hgc?g=getDna&i=mixed&c=" + chrom;
                        url += "&l=" + (chromStart - 1) + "&r=" + chromEnd;
                        url += "&db=" + getDb() + "&hgsid=" + getHgsid();
                        if ( ! window.open(url) ) {
                            rightClick.windowOpenFailedMsg();
                        }
                    } else if (cmd === 'highlightItem') {
                        if (hgTracks.windows && !hgTracks.virtualSingleChrom) {
                            // orig way only worked if the entire item was visible in the windows.
                            //var result = genomePos.chromToVirtChrom(chrom, parseInt(chromStart-1), parseInt(chromEnd));

                            var result = genomePos.convertChromPosToVirtCoords(chrom, parseInt(chromStart-1), parseInt(chromEnd));

                            if (result.chromStart != -1)
                                {
                                var newPos2 = hgTracks.chromName+":"+(result.chromStart+1)+"-"+result.chromEnd;
                                dragSelect.highlightThisRegion(newPos2, true, dragSelect.hlColorDefault);
                                }

                        } else {
                            var newChrom = hgTracks.chromName;
                            if (hgTracks.windows && hgTracks.virtualSingleChrom) {
                                newChrom = hgTracks.windows[0].chromName;
                            }
                            var newPos3 = newChrom+":"+(parseInt(chromStart))+"-"+parseInt(chromEnd);
                            dragSelect.highlightThisRegion(newPos3, true, dragSelect.hlColorDefault);
                        }
                    } else {
                        var newPosition = genomePos.setByCoordinates(chrom, chromStart, chromEnd);
                        var reg = new RegExp("hgg_gene=([^&]+)");
                        var b = reg.exec(href);
                        var name;
                        // pull item name out of the url so we can set hgFind.matches (redmine 3062)
                        if (b && b[1]) {
                            name = b[1];
                        } else {
                            reg = new RegExp("[&?]i=([^&]+)");
                            b = reg.exec(href);
                            if (b && b[1]) {
                                name = b[1];
                            }
                        }
                        if (imageV2.inPlaceUpdate) {
                            // XXXX This attempt to "update whole track image in place" didn't work
                            // for a variety of reasons (e.g. safari doesn't parse map when we
                            // update on the client side), so this is currently dead code.
                            // However, this now works in all other browsers, so we may turn this
                            // on for non-safari browsers (see redmine #4667).
                            jQuery('body').css('cursor', '');
                            var data = "hgt.trackImgOnly=1&hgt.ideogramToo=1&position=" +
                                       newPosition + "&hgsid=" + getHgsid();
                            if (name)
                                data += "&hgFind.matches=" + name;
                            $.ajax({
                                    type: "GET",
                                    url: "../cgi-bin/hgTracks",
                                    data: cart.addUpdatesToUrl(data),
                                    dataType: "html",
                                    trueSuccess: imageV2.updateImgAndMap,
                                    success: catchErrorOrDispatch,
                                    error: errorHandler,
                                    cmd: cmd,
                                    loadingId: showLoadingImage("imgTbl"),
                                    cache: false
                                });
                        } else {
                            // do a full page refresh to update hgTracks image
                            jQuery('body').css('cursor', 'wait');
                            var ele;
                            if (document.TrackForm)
                                ele = document.TrackForm;
                            else
                                ele = document.TrackHeaderForm;
                            if (name)
                                cart.addVarsToQueue(['hgFind.matches'], [name]);
                            ele.submit();
                        }
                    }
                }
        } else if (cmd === 'zoomCodon' || cmd === 'zoomExon') {
            var num, ajaxCmd, msg;
            if (cmd === 'zoomCodon') {
                msg = "Please enter the codon number to jump to:";
                ajaxCmd = 'codonToPos';
            } else {
                msg = "Please enter the exon number to jump to:";
                ajaxCmd = 'exonToPos';
            }
            rightClick.myPrompt(msg, function(results) {
                $.ajax({
                        type: "GET",
                        url: "../cgi-bin/hgApi",
                        data: cart.varsToUrlData({ 'db': getDb(), 'cmd': ajaxCmd, 'num': results,
                              'table': args.table, 'name': args.name }),
                        trueSuccess: rightClick.handleZoomCodon,
                        success: catchErrorOrDispatch,
                        error: errorHandler,
                        cache: true
                    });
                    });
        } else if (cmd === 'hgTrackUi_popup') {

            // Launches the popup but shields the ajax with a waitOnFunction
            popUp.hgTrackUi( rightClick.selectedMenuItem.id, false );  

        } else if (cmd === 'hgTrackUi_follow') {

            url = "hgTrackUi?hgsid=" + getHgsid() + "&g=";
            rec = hgTracks.trackDb[id];
            if (tdbHasParent(rec) && tdbIsLeaf(rec))
                url += rec.parentTrack;
            else {
                // The button already has the ref
                var link = normed($( 'td#td_btn_'+ rightClick.selectedMenuItem.id ).children('a')); 
                if (link)
                    url = $(link).attr('href');
                else
                    url += rightClick.selectedMenuItem.id;
            }
            location.assign(url);

        } else if (cmd === 'newCollection') {
            $.ajax({
                type: "PUT",
                async: false,
                url: "../cgi-bin/hgCollection",
                data:  "cmd=newCollection&track=" + id + "&hgsid=" + getHgsid(),
                trueSuccess: mySuccess,
                success: catchErrorOrDispatch,
                error: errorHandler,
            });

            imageV2.fullReload();
        } else if (cmd === 'addCollection') {
            var shortLabel = $(menuItemClicked).text().substring(9).slice(0,-1); 
            var ii;
            var collectionName;
            for(ii=0; ii < hgTracks.collections.length; ii++) {
                if ( hgTracks.collections[ii].shortLabel === shortLabel) {
                    collectionName = hgTracks.collections[ii].track;
                    break;
                }
            }

            $.ajax({
                type: "PUT",
                async: false,
                url: "../cgi-bin/hgCollection",
                data: "cmd=addTrack&track=" + id + "&collection=" + collectionName + "&hgsid=" + getHgsid(),
                trueSuccess: mySuccess,
                success: catchErrorOrDispatch,
                error: errorHandler,
            });

            imageV2.fullReload();
        } else if ((cmd === 'sortExp') || (cmd === 'sortSim')) {
            url = "hgTracks?hgsid=" + getHgsid() + "&" + cmd + "=";
            rec = hgTracks.trackDb[id];
            if (tdbHasParent(rec) && tdbIsLeaf(rec))
                url += rec.parentTrack;
            else {
                // The button already has the ref
                var link2 = normed($( 'td#td_btn_'+ rightClick.selectedMenuItem.id ).children('a')); 
                if (link2)
                    url = $(link2).attr('href');
                else
                    url += rightClick.selectedMenuItem.id;
            }
            location.assign(url);

        } else if (cmd === 'viewImg') {
            // Fetch a new copy of track img and show it to the user in another window. This code
            // assume we have updated remote cart with all relevant chages (e.g. drag-reorder).
            jQuery('body').css('cursor', 'wait');
            $.ajax({
                    type: "GET",
                    url: "../cgi-bin/hgTracks",
                    data: cart.varsToUrlData({ 'hgt.imageV1': '1','hgt.trackImgOnly': '1', 
                                               'hgsid': getHgsid() }),
                    dataType: "html",
                    trueSuccess: rightClick.handleViewImg,
                    success: catchErrorOrDispatch,
                    error: errorHandler,
                    cmd: cmd,
                    cache: false
                });
        } else if (cmd === 'openLink' || cmd === 'followLink') {
            href = rightClick.selectedMenuItem.href;
            var vars = new Array("c", "l", "r", "db");
            var valNames = new Array("chromName", "winStart", "winEnd");
            for (var i in vars) {
                // make sure the link contains chrom and window width info
                // (necessary b/c we are stripping hgsid and/or the cart may be empty);
                // but don't add chrom to wikiTrack links (see redmine #2476).
                var v = vars[i];
                var val;
                if (v === "db") {
                    val = getDb();
                } else {
                    val = hgTracks[valNames[i]];
                }
                if (val
                && id !== "wikiTrack"
                && (href.indexOf("?" + v + "=") === -1)
                && (href.indexOf("&" + v + "=") === -1)) {
                    href = href + "&" + v + "=" + val;
                }
            }
            if (cmd === 'followLink') {
                // XXXX This is blocked by Safari's popup blocker (without any warning message).
                location.assign(href);
            } else {
                // Remove hgsid to force a new session (see redmine ticket 1333).
                href = removeHgsid(href);
                if ( ! window.open(href) ) {
                    rightClick.windowOpenFailedMsg();
                }
            }
        } else if (cmd === 'float') {
            if (rightClick.floatingMenuItem && rightClick.floatingMenuItem === id) {
                $.floatMgr.FOArray = [];
                rightClick.floatingMenuItem = null;
            } else {
                if (rightClick.floatingMenuItem) {
                    // This doesn't work.
                    $('#img_data_' + rightClick.floatingMenuItem).parent().restartFloat();
                    // This does work
                    $.floatMgr.FOArray = [];
                }
                rightClick.floatingMenuItem = id;
                rightClick.reloadFloatingItem();
                imageV2.requestImgUpdate(id, "hgt.transparentImage=0", "");
            }
        } else if (cmd === 'hideSet') {
            row = $( 'tr#tr_' + id );
            rows = dragReorder.getContiguousRowSet(row);
            if (rows && rows.length > 0) {
                var varsToUpdate = {};
                // from bottom up, just in case remove screws with us
                for (var ix=rows.length - 1; ix >= 0; ix--) { 
                    var rowId = $(rows[ix]).attr('id').substring('tr_'.length);
                    // Remove subtrack level vis and explicitly uncheck.
                    varsToUpdate[rowId]        = '[]';
                    varsToUpdate[rowId+'_sel'] = 0;
                    $(rows[ix]).remove();
                }
                if (objNotEmpty(varsToUpdate)) {
                    cart.setVarsObj(varsToUpdate);
                }
                imageV2.afterImgChange(true);
            }
        } else if (cmd === 'hideComposite') {
            rec = hgTracks.trackDb[id];
            if (tdbIsSubtrack(rec)) {
                row = $( 'tr#tr_' + id );
                rows = dragReorder.getCompositeSet(row);
                // from bottom up, just in case remove screws with us
                if (rows && rows.length > 0) {
                    for (var rIx=rows.length - 1; rIx >= 0; rIx--) {
                        $(rows[rIx]).remove();
                    }
                selectUpdated = vis.update(rec.parentTrack, 'hide');
                cart.setVars( [rec.parentTrack], ['hide']);
                imageV2.afterImgChange(true);
                }
            }
        } else if (cmd === 'jumpToHighlight') { // If highlight exists for this assembly, jump to it
            if (hgTracks.highlight && rightClick.clickedHighlightIdx!==null) {
                var newPos = parsePositionWithDb(hgTracks.highlight.split("|")[rightClick.clickedHighlightIdx]);
                if (newPos && newPos.db === getDb()) {
                    if ( $('#highlightItem').length === 0) { // not visible? jump to it
                        var curPos = parsePosition(genomePos.get());
                        var diff = ((curPos.end - curPos.start) - (newPos.end - newPos.start));
                        if (diff > 0) { // new position is smaller then current, then center it
                            newPos.start = Math.max( Math.floor(newPos.start - (diff/2) ), 0 );
                            newPos.end   = newPos.start + (curPos.end - curPos.start);
                        }
                    }
                    if (imageV2.inPlaceUpdate) {
                        var params = "position=" + newPos.chrom+':'+newPos.start+'-'+newPos.end;
                        imageV2.navigateInPlace(params, null, true);
                    } else {
                        genomePos.setByCoordinates(newPos.chrom, newPos.start, newPos.end);
                        jQuery('body').css('cursor', 'wait');
                        document.TrackHeaderForm.submit();
                    }
                }
            }

        } else if (cmd === 'removeHighlight') {

            var highlights = hgTracks.highlight.split("|");
            highlights.splice(rightClick.clickedHighlightIdx, 1); // splice = remove element from array
            hgTracks.highlight = highlights.join("|");
            cart.setVarsObj({'highlight' : hgTracks.highlight});
            imageV2.highlightRegion();

        } else {   // if ( cmd in 'hide','dense','squish','pack','full','show' )
            // Change visibility settings:
            //
            // First change the select on our form:
            rec = hgTracks.trackDb[id];
            selectUpdated = vis.update(id, cmd);

            // Now change the track image
            if (imageV2.enabled && cmd === 'hide') {
                // Hide local display of this track and update server side cart.
                // Subtracks controlled by 2 settings so del vis and set sel=0.
                if (tdbIsSubtrack(rec)) {
                    // Remove subtrack level vis and explicitly uncheck.
                    cart.setVars( [ id, id+"_sel" ], [ '[]', 0 ] ); 
                } else if (tdbIsFolderContent(rec)) {
                    // supertrack children need to have _sel set to trigger superttrack reshaping
                    cart.setVars( [ id, id+"_sel" ], [ 'hide', 0 ] ); 
                } else {
                    cart.setVars([id], ['hide']);  // Others, just set vis hide.
                }
                $(document.getElementById('tr_' + id)).remove();
                imageV2.afterImgChange(true);
            } else if (!imageV2.mapIsUpdateable) {
                jQuery('body').css('cursor', 'wait');
                if (selectUpdated) {
                    // assert(document.TrackForm);
                    document.TrackForm.submit();
                } else {
                        // Add vis update to queue then submit
                        cart.addVarsToQueue([id], [cmd]);
                        document.TrackHeaderForm.submit();
                }
            } else {
                imageV2.requestImgUpdate(id, id + "=" + cmd, "", cmd);
            }
        }
    },

    makeHitCallback: function (title)
    {   // stub to avoid problem with a function closure w/n a loop
        return function(menuItemClicked, menuObject) {
            rightClick.hit(menuItemClicked, menuObject, title); return true;
        };
    },

    reloadFloatingItem: function ()
    {   // currently dead (experimental code)
        if (rightClick.floatingMenuItem) {
            $('#img_data_' + rightClick.floatingMenuItem).parent().makeFloat(
                {x:"current",y:"current", speed: 'fast', alwaysVisible: true, alwaysTop: true});
        }
    },

    makeImgTag: function (img)
    {   // Return img tag with explicit dimensions for img (dimensions are currently hardwired).
        // This fixes the "weird shadow problem when first loading the right-click menu"
        // seen in FireFox 3.X, which occurred b/c FF doesn't actually fetch the image until
        // the menu is being shown.
        return "<img style='width:16px; height:16px; border-style:none;' src='../images/" +
                img + "' />";
    },


    load: function (img)
    {
        rightClick.menu = img.contextMenu(function() {
            popUp.cleanup();   // Popup box is not getting closed properly so must do it here
            if ( ! rightClick.selectedMenuItem )  // This is literally an edge case so ignore
                return;

            var o; // TODO: Break this giant routine with shared vars into some sub-functions
            var str;
            var rec = null;
            var menu = [];
            var selectedImg = rightClick.makeImgTag("greenChecksm.png");
            var blankImg    = rightClick.makeImgTag("invisible16.png");
            var done = false;
            if (rightClick.selectedMenuItem && rightClick.selectedMenuItem.id) {
                var href = rightClick.selectedMenuItem.href;
                var isHgc, isGene;
                if (href) {
                    isGene = href.match("hgGene");
                    isHgc = href.match("hgc");
                }
                var id = rightClick.selectedMenuItem.id;
                rec = hgTracks.trackDb[id];
                var offerHideSubset    = false;
                var offerHideComposite = false;
                var offerSingles       = true;
                var row = $( 'tr#tr_' + id );
                if (row) {
                    var btn = $(row).find('p.btnBlue'); // btnBlue means cursor over left button
                    if (btn.length === 1) {
                        var compositeSet = dragReorder.getCompositeSet(row);
                        if (compositeSet && compositeSet.length > 0) {  // There is composite set
                            offerHideComposite = true;
                            $( compositeSet ).find('p.btn').addClass('blueButtons');// blue persists

                            var subSet = dragReorder.getContiguousRowSet(row);
                            if (subSet && subSet.length > 1) {
                                offerSingles = false;
                                if (subSet.length < compositeSet.length) {
                                    offerHideSubset = true;
                                    $( subSet ).addClass("greenRows"); // green persists
                                }
                            }
                        }
                    }
                }

                // First option is hide sets
                if (offerHideComposite) {
                    if (offerHideSubset) {
                        o = {};
                        o[blankImg + " hide track subset (green)"] = {
                                            onclick: rightClick.makeHitCallback('hideSet')};
                        menu.push(o);
                    }

                    o = {};
                    str = blankImg + " hide track set";
                    if (offerHideSubset)
                        str += " (blue)";
                    o[str] = {onclick: rightClick.makeHitCallback('hideComposite')};
                    menu.push(o);
                }

                // Second set of options: visibility for single track
                if (offerSingles) {
                    if (offerHideComposite)
                        menu.push($.contextMenu.separator);

                    // XXXX what if select is not available (b/c trackControlsOnMain is off)?
                    // Move functionality to a hidden variable?
                    var select = $("select[name=" + escapeJQuerySelectorChars(id) + "]");
                    if (select.length > 1)  
                        // Not really needed if $('#hgTrackUiDialog').html(""); has worked
                        select =  [ $(select)[0] ];
                    var cur = $(select).val();
                    if (cur) {
                        $(select).children().each(function(index, o) {
                            var title = $(this).val();
                            str = blankImg + " " + title;
                            if (title === cur)
                                str = selectedImg + " " + title;
                            o = {};
                            o[str] = {onclick: function (menuItemClicked, menuObject) {
                                        rightClick.hit(menuItemClicked, menuObject, title);
                                        return true;}};
                            menu.push(o);
                        });
                        done = true;
                    } else {
                        if (rec) {
                            // XXXX check current state from a hidden variable.
                            var visStrings = new Array("hide","dense","squish","pack","full");
                            for (var i in visStrings) {
                                // use maxVisibility and change hgTracks so it can hide subtracks
                                o = {};
                                str = blankImg + " " + visStrings[i];
                                if (rec.canPack
                                || (visStrings[i] !== "pack" && visStrings[i] !== "squish")) {
                                    if (rec.localVisibility) {
                                        if (visStrings[i] === rec.localVisibility) {
                                            str = selectedImg + " " + visStrings[i];
                                        }
                                    } else if (visStrings[i] === vis.enumOrder[rec.visibility]) {
                                        str = selectedImg + " " + visStrings[i];
                                    }
                                    o[str] = { onclick:
                                                rightClick.makeHitCallback(visStrings[i])
                                             };
                                    menu.push(o);
                                }
                            }
                            done = true;
                        }
                    }
                }

                if (done) {
                    o = {};
                    var any = false;
                    var title = rightClick.selectedMenuItem.title || "feature";
                    var maxLength = 60;
                    if (title.length > maxLength) {
                        title = title.substring(0, maxLength) + "...";
                    }
                    if (isGene || isHgc || id === "wikiTrack") {
                        // Add "Open details..." item
                        var displayItemFunctions = false;
                        if (rec) {
                            if (rec.type.indexOf("wig") === 0
                            ||  rec.type.indexOf("bigWig") === 0
                            ||  id === "wikiTrack") {
                                displayItemFunctions = false;
                            } else if (rec.type.indexOf("expRatio") === 0) {
                                displayItemFunctions = title !== "zoomInMore";
                            } else {
                                displayItemFunctions = true;
                            }
                            // For barChart mouseovers, replace title (which may be a category 
                            // name+value) with item name
                            if (rec.type.indexOf("barChart") === 0
                            || rec.type.indexOf("bigBarChart") === 0) {
                                a = /i=([^&]+)/.exec(href);
                                if (a && a[1]) {
                                    title = a[1];
                                }
                            }
                        }
                        if (isHgc && href.indexOf('g=gtexGene') !== -1) {
                            // For GTEx gene mouseovers, replace title (which may be a tissue name) with 
                            // item (gene) name
                            a = /i=([^&]+)/.exec(href);
                            if (a && a[1]) {
                                title = a[1];
                            }
                        }
                        if (displayItemFunctions) {
                            o[rightClick.makeImgTag("magnify.png") + " Zoom to " +  title] = {
                                onclick: function(menuItemClicked, menuObject) {
                                            rightClick.hit(menuItemClicked, menuObject,
                                                    "selectWholeGene"); return true;
                                          }
                                };
                            o[rightClick.makeImgTag("highlight.png") + " Highlight " + title] = 
                                {   onclick: function(menuItemClicked, menuObject) {
                                        rightClick.hit(menuItemClicked, menuObject,
                                                       "highlightItem"); 
                                        return true;
                                    }
                                };
                            if (rightClick.supportZoomCodon && rec.type.indexOf("genePred") !== -1) {
                                // http://hgwdev-larrym.cse.ucsc.edu/cgi-bin/hgGene?hgg_gene=uc003tqk.2&hgg_prot=P00533&hgg_chrom=chr7&hgg_start=55086724&hgg_end=55275030&hgg_type=knownGene&db=hg19&c=chr7
                                var name, table;
                                var reg = new RegExp("hgg_gene=([^&]+)");
                                var a = reg.exec(href);
                                if (a && a[1]) {
                                    name = a[1];
                                    reg = new RegExp("hgg_type=([^&]+)");
                                    a = reg.exec(href);
                                    if (a && a[1]) {
                                        table = a[1];
                                    }
                                } else {
                                    // http://hgwdev-larrym.cse.ucsc.edu/cgi-bin/hgc?o=55086724&t=55275031&g=refGene&i=NM_005228&c=chr7
                                    // http://hgwdev-larrym.cse.ucsc.edu/cgi-bin/hgc?o=55086713&t=55270769&g=wgEncodeGencodeManualV4&i=ENST00000455089&c=chr7
                                    reg = new RegExp("i=([^&]+)");
                                    a = reg.exec(href);
                                    if (a && a[1]) {
                                        name = a[1];
                                        reg = new RegExp("g=([^&]+)");
                                        a = reg.exec(href);
                                        if (a && a[1]) {
                                            table = a[1];
                                        }
                                    }
                                }
                                if (name && table) {
                                    o[rightClick.makeImgTag("magnify.png")+" Zoom to codon"] =
                                    {   onclick: function(menuItemClicked, menuObject) {
                                            rightClick.hit(menuItemClicked, menuObject,
                                                        "zoomCodon",
                                                        {name: name, table: table});
                                            return true;}
                                    };
                                    o[rightClick.makeImgTag("magnify.png")+" Zoom to exon"] = {
                                        onclick: function(menuItemClicked, menuObject) {
                                            rightClick.hit(menuItemClicked, menuObject,
                                                          "zoomExon",
                                                          {name: name, table: table});
                                            return true; }
                                    };
                                }
                            }
                            o[rightClick.makeImgTag("dnaIcon.png")+" Get DNA for "+title] = {
                                onclick: function(menuItemClicked, menuObject) {
                                    rightClick.hit(menuItemClicked, menuObject, "getDna");
                                    return true; }
                            };
                        }
                        o[rightClick.makeImgTag("bookOut.png")+
                                                " Open details page in new window..."] = {
                            onclick: function(menuItemClicked, menuObject) {
                                rightClick.hit(menuItemClicked, menuObject, "openLink");
                                return true; }
                        };
                        any = true;
                    }
                    if (href && href.length  > 0) {
                        // Add "Show details..." item
                        if (title.indexOf("Click to alter ") === 0) {
                            // suppress the "Click to alter..." items
                        } else if (rightClick.selectedMenuItem.href.indexOf("cgi-bin/hgTracks")
                                                                                        !== -1) {
                            // suppress menu items for hgTracks links (e.g. Next/Prev map items).
                        } else {
                            var item;
                            if (title === "zoomInMore")
                                // avoid showing menu item that says
                                // "Show details for zoomInMore..." (redmine 2447)
                                item = rightClick.makeImgTag("book.png") + " Show details...";
                            else
                                item = rightClick.makeImgTag("book.png")+" Show details for "+
                                       title + "...";
                            o[item] = {onclick: function(menuItemClicked, menuObject) {
                                       rightClick.hit(menuItemClicked,menuObject,"followLink");
                                       return true; }
                            };
                            any = true;
                        }
                    }
                    if (any) {
                        menu.push($.contextMenu.separator);
                        menu.push(o);
                    }
                }
            }

            if (rightClick.selectedMenuItem && rec) {
                // Add cfg options at just shy of end...
                o = {};
                if (tdbIsLeaf(rec)) {

                    if (rec.configureBy !== 'none'
                    && (!tdbIsCompositeSubtrack(rec) || rec.configureBy !== 'clickThrough')) {
                        // Note that subtracks never do clickThrough because
                        // parentTrack cfg is the desired clickThrough
                        o[rightClick.makeImgTag("wrench.png")+" Configure "+rec.shortLabel] = {
                            onclick: function(menuItemClicked, menuObject) {
                                rightClick.hit(menuItemClicked, menuObject, "hgTrackUi_popup");
                                return true; }
                        };
                    }
                    if (rec.parentTrack) {
                        o[rightClick.makeImgTag("folderWrench.png")+" Configure "+
                          rec.parentLabel + " track set..."] = {
                            onclick: function(menuItemClicked, menuObject) {
                                rightClick.hit(menuItemClicked,menuObject,"hgTrackUi_follow");
                                return true; }
                          };
                    }
                } else {

                    o[rightClick.makeImgTag("folderWrench.png")+" Configure "+rec.shortLabel +
                      " track set..."] = {
                        onclick: function(menuItemClicked, menuObject) {
                            rightClick.hit(menuItemClicked, menuObject, "hgTrackUi_follow");
                            return true; }
                      };
                }
                if (jQuery.floatMgr) {
                    o[(rightClick.selectedMenuItem.id === rightClick.floatingMenuItem ?
                            selectedImg : blankImg) + " float"] = {
                        onclick: function(menuItemClicked, menuObject) {
                            rightClick.hit(menuItemClicked, menuObject, "float");
                            return true; }
                    };
                }
                menu.push($.contextMenu.separator);
                menu.push(o);
            }

            menu.push($.contextMenu.separator);
            if (hgTracks.highlight && rightClick.clickedHighlightIdx!==null) {

                if (hgTracks.highlight.search(getDb() + '.') === 0) {
                    var currentlySeen = ($('#highlightItem').length > 0); 
                    o = {};
                    // Jumps to highlight when not currently seen in image
                    var text = (currentlySeen ? " Zoom" : " Jump") + " to highlight";
                    o[rightClick.makeImgTag("highlightZoom.png") + text] = {
                        onclick: rightClick.makeHitCallback('jumpToHighlight')
                    };

                    if ( currentlySeen ) {   // Remove only when seen
                        o[rightClick.makeImgTag("highlightRemove.png") + 
                                                                   " Remove highlight"] = {
                            onclick: rightClick.makeHitCallback('removeHighlight')
                        };
                    }
                    menu.push(o);
                }
            }

            if (rec.isCustomComposite)
                {
                // add delete from composite
                }
            else if ((!rec.hasChildren && !rec.type.startsWith("wigMaf")) &&
                (rec.type.startsWith("bigWig") || rec.type.startsWith("multiWig") || rec.type.startsWith("wig") || rec.type.startsWith("bedGraph"))) {
                o = {};
                o[" Make a New Collection with \"" + rec.shortLabel + "\""] = {
                    onclick: rightClick.makeHitCallback("newCollection")
                };  
                menu.push(o);

                if (hgTracks.collections) {
                    var ii;
                    for(ii=0; ii < hgTracks.collections.length; ii++) {
                        o = {};
                        o[" Add to \"" + hgTracks.collections[ii].shortLabel + "\""] = {
                            onclick: rightClick.makeHitCallback("addCollection")
                        };  
                        menu.push(o);
                    }
                }
                menu.push($.contextMenu.separator);
            }

            // add sort options if this is a custom composite
            if (rec.isCustomComposite) {

                o = {};
                o[" Sort by Magnitude "] = {
                    onclick: function(menuItemClicked, menuObject) {
                        rightClick.hit(menuItemClicked, menuObject, "sortExp");
                        return true; }
                };  
                menu.push(o);

                o = {};
                o[" Sort by Similarity "] = {
                    onclick: function(menuItemClicked, menuObject) {
                        rightClick.hit(menuItemClicked, menuObject, "sortSim");
                        return true; }
                };  
                menu.push(o);

                menu.push($.contextMenu.separator);
            }

            // Add view image at end
            o = {};
            o[rightClick.makeImgTag("eye.png") + " View image"] = {
                onclick: function(menuItemClicked, menuObject) {
                    rightClick.hit(menuItemClicked, menuObject, "viewImg");
                    return true; }
            };
            menu.push(o);

            return menu;
        },
        {
            beforeShow: function(e) {
                // console.log(mapItems[rightClick.selectedMenuItem]);
                rightClick.selectedMenuItem = rightClick.findMapItem(e);
                
                // find the highlight that was clicked
                var imageX = (imageV2.imgTbl[0].getBoundingClientRect().left) + imageV2.LEFTADD;
                var xDiff = (e.clientX) - imageX;
                var clickPos = genomePos.pixelsToBases(img, xDiff, xDiff+1, hgTracks.winStart, hgTracks.winEnd);
                rightClick.clickedHighlightIdx = dragSelect.findHighlightIdxForPos(clickPos);

                // XXXX? posting.blockUseMap = true;
                return true;
            },
            hideTransition:'hide', // hideCallback fails if these are not defined.
            hideSpeed:10,
            hideCallback: function() {
                $('p.btn.blueButtons').removeClass('blueButtons');
                $('tr.trDraggable.greenRows').removeClass('greenRows');
            }
        });
        return;
    }
};

  //////////////////////////////////
 //// external tools           ////
//////////////////////////////////

function showExtToolDialog() {
        /* show the 'send to external tool' dialog */
        // information about external tools is stored in the extTools global list
        // defined by a <script> at the end of the body

        // remove an existing dialog box
        var extToolDialog = $("#extToolDialog").remove();

        // construct the contents
        var htmlLines = ["<ul class='indent'>"];
        var winSize = hgTracks.winEnd - hgTracks.winStart;
        for (i = 0; i < extTools.length; i++) {
            var tool = extTools[i];
            var toolId = tool[0];
            var shortLabel = tool[1];
            var longLabel = tool[2];
            var maxSize = tool[3];
            if ((maxSize===0) || (winSize < maxSize))
                {
                var url = "hgTracks?hgsid="+getHgsid()+"&hgt.redirectTool="+toolId;
                //var onclick = "$('#extToolDialog').dialog('close');";
                //htmlLines.push("<li><a onclick="+'"'+onclick+'"'+"id='extToolLink' target='_BLANK' href='"+url+"'>"+shortLabel+"</a>: <small>"+longLabel+"</small></li>");
		// onclick js code moved to jsInline
                htmlLines.push("<li><a class='extToolLink2' target='_BLANK' href='"+url+"'>"+shortLabel+"</a>: <small>"+longLabel+"</small></li>");
                }
            else
                {
                note = "<br><b>Needs zoom to &lt; "+maxSize/1000+" kbp.</b></small></span></li>";
                htmlLines.push('<li><span style="color:grey">'+shortLabel+": <small>"+longLabel+note);
                }
        }
        htmlLines.push("</ul>");
        content = htmlLines.join("");
            
        var title = hgTracks.chromName + ":" + (hgTracks.winStart+1) + "-" + hgTracks.winEnd;
        if (hgTracks.nonVirtPosition)
            title = hgTracks.nonVirtPosition;
        title += " on another website";
        $("body").append("<div id='extToolDialog' title='"+title+"'><p>" + content + "</p>");

	// GALT 
	$('a.extToolLink2').click(function(){$('#extToolDialog').dialog('close');});

        // copied from the hgTrackUi function below
        var popMaxHeight = ($(window).height() - 40);
        var popMaxWidth  = ($(window).width() - 40);
        var popWidth     = 600;
        if (popWidth > popMaxWidth)
            popWidth = popMaxWidth;

        // also copied from the hgTrackUi code below
        $('#extToolDialog').dialog({
            resizable: true,               // Let description scroll vertically
            height: popMaxHeight,
            width: popWidth,
            minHeight: 200,
            minWidth: 600,
            maxHeight: popMaxHeight,
            maxWidth: popMaxWidth,
            modal: true,
            closeOnEscape: true,
            autoOpen: false,
            buttons: { "Close": function() {
                    $(this).dialog("close");
            }},
        });
        
        $('#extToolDialog').dialog('open');
}

  /////////////////////////////////////////////////////////
 //// popupHgt popup for hgTracks  (aka modal dialog) ////
/////////////////////////////////////////////////////////
var popUpHgt = {

    whichHgTracksMethod: "",
    title: "",

    cleanup: function ()
    {  // Clean out the popup box on close
        if ($('#hgTracksDialog').html().length > 0 ) {
            // clear out html after close to prevent problems caused by duplicate html elements
            $('#hgTracksDialog').html("");
            popUpHgt.whichHgTracksMethod = "";
            popUpHgt.title = "";
        }
    },

    _uiDialogRequest: function (whichHgTracksMethod)
    { // popup cfg dialog
        popUpHgt.whichHgTracksMethod = whichHgTracksMethod;
        var myLink = "../cgi-bin/hgTracks?hgsid=" + getHgsid() + "&db=" + getDb();
        if (popUpHgt.whichHgTracksMethod === "multi-region config") {
            myLink += "&hgTracksConfigMultiRegionPage=multi-region";
            popUpHgt.title = "Configure Multi-Region View";
        }

        $.ajax({
                    type: "GET",
                    url: cart.addUpdatesToUrl(myLink),
                    dataType: "html",
                    trueSuccess: popUpHgt.uiDialog,
                    success: catchErrorOrDispatch,
                    error: errorHandler,
                    cache: false
                });
    },

    hgTracks: function (whichHgTracksMethod)
    {   // Launches the popup but shields the ajax with a waitOnFunction
        waitOnFunction( popUpHgt._uiDialogRequest, whichHgTracksMethod);
    },

    uiDialogOk: function (popObj)
    {   // When popup closes with ok

    },

    uiDialog: function (response, status)
    {
    // Take html from hgTracks and put it up as a modal dialog.

        // make sure all links (e.g. help links) open up in a new window
        response = response.replace(/<a /ig, "<a target='_blank' ");

        var cleanHtml = response;
        cleanHtml = stripCspHeader(cleanHtml,false); // DEBUG msg with true
        cleanHtml = stripJsFiles(cleanHtml,false);   // DEBUG msg with true
        cleanHtml = stripCssFiles(cleanHtml,false);  // DEBUG msg with true
        //cleanHtml = stripJsEmbedded(cleanHtml,false);// DEBUG msg with true // Obsolete by CSP2?
        var nonceJs = {};
        cleanHtml = stripCSPAndNonceJs(cleanHtml, false, nonceJs); // DEBUG msg with true

        cleanHtml = stripMainMenu(cleanHtml,false);  // DEBUG msg with true

        $('#hgTracksDialog').html("<div id='pop' style='font-size:.9em;'>"+ cleanHtml +"</div>");

        appendNonceJsToPage(nonceJs);


        // Strategy for popups with js:
        // - jsFiles and CSS should not be included in html.  Here they are shluped out.
        // - The resulting files ought to be loadable dynamically (with getScript()), 
        //   but this was not working nicely with the modal dialog
        //   Therefore include files must be included with hgTracks CGI !
        // - embedded js should not be in the popup box.
        // - Somethings should be in a popup.ready() function, and this is emulated below, 
        //   as soon as the cleanHtml is added
        //   Since there are many possible popup cfg dialogs, the ready should be all inclusive.

        // -- popup.ready() -- Here is the place to do things that might otherwise go
        //                     into a $('#pop').ready() routine!

        // Searching for some semblance of size suitability
        var popMaxHeight = ($(window).height() - 40);
        var popMaxWidth  = ($(window).width() - 40);
        var popWidth     = 700;
        if (popWidth > popMaxWidth)
            popWidth = popMaxWidth;

        $('#hgTracksDialog').dialog({
            ajaxOptions: {
                // This doesn't work
                cache: true
            },
            resizable: true,               // Let scroll vertically
            height: 'auto',
            width: popWidth,
            minHeight: 200,
            minWidth: 400,
            maxHeight: popMaxHeight,
            maxWidth: popMaxWidth,
            modal: true,
            closeOnEscape: true,
            autoOpen: false,
            buttons: { 
                /* NOT NOW
                "OK": function() {
                    popUpHgt.uiDialogOk($('#pop'));
                    $(this).dialog("close");
                }
                */
            },
            // popup.ready() doesn't seem to work in open.

            //create: function () { 
                //$(this).siblings().find(".ui-dialog-title").html('<span style="">Test </span>'); 
                //$(this).siblings().find(".ui-dialog-title").html('<span style="       visibility: hidden;"></span>'); 
            //},
            
            open: function () {
                $('#hgTracksDialog').find('.filterBy,.filterComp').each(
                    function(i) {  // ddcl.js is dropdown checklist lib support
                        if ($(this).hasClass('filterComp'))
                            ddcl.setup(this);
                        else
                            ddcl.setup(this, 'noneIsAll');
                    }
                );
            },

            close: function() {
                popUpHgt.cleanup();
            }
        });
        
    
        $('#hgTracksDialog').dialog('option' , 'title' , popUpHgt.title);
        $('#hgTracksDialog').dialog('open');
    
    }
};

// A function to show the keyboard help dialog box, bound to ? and called from the menu bar
function showHotkeyHelp() {
    $("#hotkeyHelp").dialog({width:'600'});
}

// A function to add an entry for the keyboard help dialog box to the menubar 
// and add text that indicates the shortcuts to many static menubar items as suggested by good old IBM CUA/SAA
function addKeyboardHelpEntries() {
    var html = '<li><a id="keybShorts" title="List all possible keyboard shortcuts" href="#">Keyboard Shortcuts</a><span class="shortcut">?</span></li>';
    $('#help .last').before(html);
    $("#keybShorts").click( function(){showHotkeyHelp();} );

    html = '<span class="shortcut">s s</span>';
    $('#sessionsMenuLink').after(html);

    html = '<span class="shortcut">p s</span>';
    $('#publicSessionsMenuLink').after(html);

    html = '<span class="shortcut">c t</span>';
    $('#customTracksMenuLink').after(html);

    html = '<span class="shortcut">t c</span>';
    $('#customCompositeMenuLink').after(html);

    html = '<span class="shortcut">t h</span>';
    $('#trackHubsMenuLink').after(html);

    html = '<span class="shortcut">t b</span>';
    $('#blatMenuLink').after(html);

    html = '<span class="shortcut">t t</span>';
    $('#tableBrowserMenuLink').after(html);

    html = '<span class="shortcut">t i</span>';
    $('#ispMenuLink').after(html);

    html = '<span class="shortcut">t s</span>';
    $('#trackSearchMenuLink').after(html);

    html = '<span class="shortcut">c f</span>';
    $('#configureMenuLink').after(html);

    html = '<span class="shortcut">c r</span>';
    $('#cartResetMenuLink').after(html);
}

// A function for the keyboard shortcut:
// View DNA
function gotoGetDnaPage() {
    var position = hgTracks.chromName+":"+hgTracks.winStart+"-"+hgTracks.winEnd;
    if (hgTracks.virtualSingleChrom && (pos.chrom.search("virt") === 0)) {
        position = genomePos.get().replace(/,/g,'');
    } else if (hgTracks.windows && hgTracks.nonVirtPosition) {
        position = hgTracks.nonVirtPosition;
    }
    var pos = parsePosition(position);
    if (pos) {
        var url = "hgc?hgsid="+getHgsid()+"&g=getDna&i=mixed&c="+pos.chrom+"&l="+pos.start+"&r="+pos.end+"&db="+getDb();
        window.location.href = url;
    }
    return false;
}

// A function for the keyboard shortcuts "zoom to x bp"
function zoomTo(zoomSize) {
    var flankSize = Math.floor(zoomSize/2);
    var pos = parsePosition(genomePos.get());
    var mid = pos.start+(Math.floor((pos.end-pos.start)/2));
    var newStart = Math.max(mid - flankSize, 0);
    var newEnd = mid + flankSize - 1;
    var newPos = genomePos.setByCoordinates(pos.chrom, newStart, newEnd);
    if (hgTracks.virtualSingleChrom && (newPos.search("virt:")===0))
        newPos = genomePos.disguisePosition(newPosition); // DISGUISE?
    imageV2.navigateInPlace("position="+newPos, null, true);
}

// A function for the keyboard shortcuts "highlight add/clear/new"
function highlightCurrentPosition(mode) {
    var pos = genomePos.get();
    if (mode=="new")
        dragSelect.highlightThisRegion(pos, false);
    else if (mode=="add")
        dragSelect.highlightThisRegion(pos, true);
    else {
        hgTracks.highlight = "";
        var cartSettings = {'highlight': ""};
        cart.setVarsObj(cartSettings);
        imageV2.highlightRegion();
    }
}

  //////////////////////////////////
 //// popup (aka modal dialog) ////
//////////////////////////////////
var popUp = {

    trackName:            "",
    trackDescriptionOnly: false,
    saveAllVars:          null,

    cleanup: function ()
    {  // Clean out the popup box on close
        if ($('#hgTrackUiDialog').html().length > 0 ) {
            // clear out html after close to prevent problems caused by duplicate html elements
            $('#hgTrackUiDialog').html("");
            popUp.trackName = ""; //set to defaults
            popUp.trackDescriptionOnly = false;
            popUp.saveAllVars = null;
        }
    },

    _uiDialogRequest: function (trackName,descriptionOnly)
    { // popup cfg dialog
        popUp.trackName = trackName;
        var myLink = "../cgi-bin/hgTrackUi?g=" + trackName + "&hgsid=" + getHgsid() +
                     "&db=" + getDb();
        popUp.trackDescriptionOnly = descriptionOnly;
        if (popUp.trackDescriptionOnly)
            myLink += "&descriptionOnly=1";

        var rec = hgTracks.trackDb[trackName];
        if (!descriptionOnly && rec && rec.configureBy) {
            if (rec.configureBy === 'none')
                return;
            else if (rec.configureBy === 'clickThrough') {
                jQuery('body').css('cursor', 'wait');
                window.location = cart.addUpdatesToUrl(myLink);
                return;
            }  // default falls through to configureBy popup
        }
        myLink += "&ajax=1";
        $.ajax({
                    type: "GET",
                    url: cart.addUpdatesToUrl(myLink),
                    dataType: "html",
                    trueSuccess: popUp.uiDialog,
                    success: catchErrorOrDispatch,
                    error: errorHandler,
                    cmd: rightClick.selectedMenuItem,
                    cache: false
                });
    },

    hgTrackUi: function (trackName,descriptionOnly)
    {   // Launches the popup but shields the ajax with a waitOnFunction
        waitOnFunction( popUp._uiDialogRequest, trackName, descriptionOnly );  
    },

    uiDialogOk: function (popObj, trackName)
    {   // When hgTrackUi Cfg popup closes with ok, then update cart and refresh parts of page
        var rec = hgTracks.trackDb[trackName];
        var subtrack = tdbIsSubtrack(rec) ? trackName : undefined;  // subtrack vis rules differ
        // For unknown reasons IE8 fails to find $('#pop'), occasionally
        var allVars = getAllVars($('#hgTrackUiDialog'), subtrack );
        //  Since 2010, when Tim changed this to only report changed vars instead of all form vars,
        // it no longer matches the behavior of hgTrackUi when called the non-popup way.
        // A few places in the hgTracks C code have been patched to explicitly set the cart vars
        // for some default checkboxes. So now this still means that QA must explicitly test
        // both paths through the code: as a separate full hgTracksUi page, and as a popup config window.
        // hgTrackUi always sends in its form all variables causing them to be explicitly set in the cart.  
        // The popup only sends things that have changed, causing those changes to appear explicitly
        // and even then it skips over disabled form items.
        // There is some C code that was written before the popup config, 
        // and it expects all the variables are set or none are.
        // If just some are set, and not the default ones, it gets confused.
        // I fixed just such a bug in the code that handles refSeq.
        // See commit daf92c0f9eb331ea60740e6802aabd241d4be363.
        var changedVars = varHashChanges(allVars,popUp.saveAllVars);
         // DEBUG Examples:
        //debugDumpFormCollection("saveAllVars", popUp.saveAllVars);
        //debugDumpFormCollection("allVars", allVars);
        //debugDumpFormCollection("changedVars", changedVars);
        var newVis = changedVars[trackName];
        // subtracks do not have "hide", thus '[]'
        var hide = (newVis && (newVis === 'hide' || newVis === '[]'));  
        if ( ! normed($('#imgTbl')) ) { // On findTracks or config page
            if (objNotEmpty(changedVars))
                cart.setVarsObj(changedVars);
        }
        else {  // On image page
            if (hide) {
                if (objNotEmpty(changedVars))
                    cart.setVarsObj(changedVars);
                $(document.getElementById('tr_' + trackName)).remove();
                imageV2.afterImgChange(true);
            } else {
                // Keep local state in sync if user changed visibility
                if (newVis) {
                    vis.update(trackName, newVis);
                }
                if (objNotEmpty(changedVars)) {
                    var urlData = cart.varsToUrlData(changedVars);
                    if (imageV2.mapIsUpdateable) {
                        imageV2.requestImgUpdate(trackName,urlData,"");
                    } else {
                        window.location = "../cgi-bin/hgTracks?" + urlData + "&hgsid=" + getHgsid();
                    }
                }
            }
        }
    },

    uiDialog: function (response, status)
    {
    // Take html from hgTrackUi and put it up as a modal dialog.

        // make sure all links (e.g. help links) open up in a new window
        response = response.replace(/<a /ig, "<a target='_blank' ");

        var cleanHtml = response;
        cleanHtml = stripJsFiles(cleanHtml,false);   // DEBUG msg with true
        cleanHtml = stripCssFiles(cleanHtml,false);  // DEBUG msg with true
        //cleanHtml = stripJsEmbedded(cleanHtml,false);// DEBUG msg with true // OBSOLETE BY CSP2?
	var nonceJs = {};
	cleanHtml = stripCSPAndNonceJs(cleanHtml, false, nonceJs); // DEBUG msg with true

	//alert(cleanHtml);  // DEBUG REMOVE
        $('#hgTrackUiDialog').html("<div id='pop' style='font-size:.9em;'>"+ cleanHtml +"</div>");

	appendNonceJsToPage(nonceJs);

        // Strategy for popups with js:
        // - jsFiles and CSS should not be included in html.  Here they are shluped out.
        // - The resulting files ought to be loadable dynamically (with getScript()), 
        //   but this was not working nicely with the modal dialog
        //   Therefore include files must be included with hgTracks CGI !
        // - embedded js should not be in the popup box.
        // - Somethings should be in a popup.ready() function, and this is emulated below, 
        //   as soon as the cleanHtml is added
        //   Since there are many possible popup cfg dialogs, the ready should be all inclusive.

        if ( ! popUp.trackDescriptionOnly ) {
            // If subtrack then vis rules differ
            var subtrack = tdbIsSubtrack(hgTracks.trackDb[popUp.trackName]) ? popUp.trackName :"";  
            // Saves the original vars (and vals) that may get changed by the popup cfg.
            popUp.saveAllVars = getAllVars( $('#hgTrackUiDialog'), subtrack ); 

            // -- popup.ready() -- Here is the place to do things that might otherwise go
            //                     into a $('#pop').ready() routine!
        }

        // Searching for some semblance of size suitability
        var popMaxHeight = ($(window).height() - 40);
        var popMaxWidth  = ($(window).width() - 40);
        var popWidth     = 640;
        if (popWidth > popMaxWidth)
            popWidth = popMaxWidth;

        // Create dialog buttons for UI popup
        var uiDialogButtons = {};
        if (popUp.trackDescriptionOnly) {
            uiDialogButtons.OK = function() {
                $(this).dialog("close");
            };
        } else {
            uiDialogButtons.Apply = function() {
                 popUp.uiDialogOk($('#pop'), popUp.trackName);
                 // thanks to JAT for this cleverness to keep button functioning
                 popUp.saveAllVars = getAllVars( $('#hgTrackUiDialog'), popUp.trackName);
                 if (popUp.saveAllVars[popUp.trackName+"_sel"] === 0) {       // hide
                    // NOTE: once hidden, can't be unhidden by popup, so shut it down
                    $(this).dialog("close");
                }
            };
            uiDialogButtons.OK = function() {
                popUp.uiDialogOk($('#pop'), popUp.trackName);
                $(this).dialog("close");
            };
        }

        $('#hgTrackUiDialog').dialog({
            ajaxOptions: {
                // This doesn't work
                cache: true
            },
            resizable: true,               // Let description scroll vertically
            height: (popUp.trackDescriptionOnly ? popMaxHeight : 'auto'),
            width: popWidth,
            minHeight: 200,
            minWidth: 400,
            maxHeight: popMaxHeight,
            maxWidth: popMaxWidth,
            modal: true,
            closeOnEscape: true,
            autoOpen: false,
            buttons: uiDialogButtons,

            // popup.ready() doesn't seem to work in open.
            open: function(event) {
                // fix popup to a location -- near the top and somewhat centered on the browser image
                $(event.target).parent().css('position', 'fixed');
                $(event.target).parent().css('top', '18%');
                $(event.target).parent().css('left', '30%');

                if (!popUp.trackDescriptionOnly) {
                    $('#hgTrackUiDialog').find('.filterBy,.filterComp').each(
                        function(i) {
                            if ($(this).hasClass('filterComp'))
                                ddcl.setup(this);
                            else
                                ddcl.setup(this, 'noneIsAll');
                        }
                    );
                }
            },
            close: function() {
                popUp.cleanup();
            }
        });
        
        // FIXME: Why are open and close no longer working!!!
        if (popUp.trackDescriptionOnly) {
            var myWidth =  $(window).width() - 300;
            if (myWidth > 900)
                myWidth = 900;
            $('#hgTrackUiDialog').dialog("option", "maxWidth", myWidth);
            $('#hgTrackUiDialog').dialog("option", "width", myWidth);
            $('#hgTrackUiDialog').dialog('option' , 'title' ,
                               hgTracks.trackDb[popUp.trackName].shortLabel+" Track Description");
            $('#hgTrackUiDialog').dialog('open');
        } else {
            $('#hgTrackUiDialog').dialog('option' , 'title' ,
                                  hgTracks.trackDb[popUp.trackName].shortLabel+" Track Settings");
            $('#hgTrackUiDialog').dialog('open');
        }
        var buttOk = $('button.ui-state-default');
        $(buttOk).focus();
    }
};

  ///////////////////////////////
 //// imageV2  (aka imgTbl) ////
///////////////////////////////
var imageV2 = {

    enabled:        false,  // Will be set to true unless advancedJavascriptFeatures
                            // is turned off OR if track search of config page
    imgTbl:         null,   // formerly "trackImgTbl"  The imgTbl or null if non-imageV2.
    inPlaceUpdate:  false,  // modified based on value of hgTracks.inPlaceUpdate & mapIsUpdateable
    mapIsUpdateable:true,
    lastTrack:      null,   // formerly (lastMapItem) this is used to try to keep what the
                            // last track the cursor passed.

    LEFTADD: 3,             // when going from pixels to chrom coords, these 3 pixels
                            // are somehow used for "borders or cgi item calc ?" (original comment)

    markAsDirtyPage: function ()
    {   // Page is marked as dirty so that the back-button knows page doesn't match cart
        var dirty = normed($('#dirty'));
        if (dirty)
            $(dirty).val('true');
    },

    markAsCleanPage: function ()
    {   // Clears signal that history may be out of sync with cart.
        var dirty = normed($('#dirty'));
        if (dirty)
            $(dirty).val('false');
    },

    isDirtyPage: function ()
    { // returns true if page was marked as dirty
    // This will allow the backbutton to be overridden

        var dirty = normed($('#dirty'));
        if (dirty && $(dirty).val() === 'true')
            return true;
        return false;
    },
    
    manyTracks: function ()
    {   // image-reload is slower than whole page reload when there are too many tracks
        if (!hgTracks || !hgTracks.trackDb || objKeyCount(hgTracks.trackDb) > 50)
            return true;
        return false;
    },

    updateTiming: function (response)
    {   // update measureTiming text on current page based on what's in the response
        var reg = new RegExp("(<span class='timing'>.+?</span>)", "g");
        var strs = [];
        for (var a = reg.exec(response); a && a[1]; a = reg.exec(response)) {
            strs.push(a[1]);
        }
        if (strs.length > 0) {
            $('.timing').remove();
            for (var ix = strs.length; ix > 0; ix--) {
                $('body').prepend(strs[ix - 1]);
            }
        }
        reg = new RegExp("(<span class='trackTiming'>[\\S\\s]+?</span>)");
        a = reg.exec(response);
        if (a && a[1]) {
            $('.trackTiming').replaceWith(a[1]);
        }
    },

    loadSuggestBox: function ()
    {
        if ($('#positionInput').length) {
            if (!suggestBox.initialized) { // only call init once
                 suggestBox.init(getDb(), 
                            $("#suggestTrack").length > 0,
                            function (item) {
                                genomePos.set(item.id, getSizeFromCoordinates(item.id));
                            },
                            function (position) {
                                genomePos.set(position, getSizeFromCoordinates(position));
                            });
            }
            // Make sure suggestTrack is visible when user chooses via gene select (#3484).
            if ($("#suggestTrack").length) {
                $(document.TrackForm || document.TrackHeaderForm).submit(function(event) {
                                               if ($('#hgFindMatches').length) {
                                                   vis.makeTrackVisible($("#suggestTrack").val());
                                               }
                                           });
            }
        }
    },
    
    afterImgChange: function (dirty)
    {   // Standard things to do when manipulations change image without ajax update.
        dragReorder.init();
        dragSelect.load(false);
        imageV2.highlightRegion();
        if (dirty)
            imageV2.markAsDirtyPage();
    },
    
    afterReload: function (id)
    {   // Reload various UI widgets after updating imgTbl map.
        dragReorder.init();
        dragSelect.load(false);
        // Do NOT reload context menu (otherwise we get the "context menu sticks" problem).
        // rightClick.load($('#tr_' + id));
        if (imageV2.imgTbl.tableDnDUpdate)
            imageV2.imgTbl.tableDnDUpdate();
        rightClick.reloadFloatingItem();
        // Turn on drag scrolling.
        if (hgTracks.imgBoxPortal) {
            $("div.scroller").panImages();
        }
        if (imageV2.backSupport) {
            if (id) { // The remainder is only needed for full reload
                imageV2.markAsDirtyPage(); // vis of cfg change
                imageV2.highlightRegion();
                return;
            }
        }
        
        imageV2.loadRemoteTracks();
        makeItemsByDrag.load();
        imageV2.loadSuggestBox();
        imageV2.highlightRegion();

        if (imageV2.backSupport) {
            imageV2.setInHistory(false);    // Set this new position into History stack
        } else {
            imageV2.markAsDirtyPage();
        }
    },

    updateImgForId: function (html, id, fullImageReload, newJsonRec)
    {   // update row in imgTbl for given id.
        // return true if we successfully pull slice for id and update it in imgTrack.
        var newTr = $(html).find("tr[id='tr_" + id + "']");
        if (newTr.length > 0) {
            var tr = $(document.getElementById("tr_" + id));
            if (tr.length > 0) {
                $(tr).html(newTr.children());

                // Need to update tr class list too
                var classes = $(html).find("tr[id='tr_"+ id + "']")[0].className;
                if (classes && classes.length > 0) {
                    $(tr).removeClass();
                    $(tr).addClass(classes);
                }

                // NOTE: Want to examine the png? Uncomment:
                //var img = $('#tr_' + id).find("img[id^='img_data_']").attr('src');
                //warn("Just parsed image:<BR>"+img);

                // >1x dragScrolling needs some extra care.
                if (hgTracks.imgBoxPortal && (hgTracks.imgBoxWidth > hgTracks.imgBoxPortalWidth)) {
                    if (hgTracks.imgBoxPortalLeft !== undefined
                    &&  hgTracks.imgBoxPortalLeft !== null) {
                        $(tr).find('.panImg').css({'left': hgTracks.imgBoxPortalLeft });
                        $(tr).find('.tdData').css(
                                {'backgroundPosition': hgTracks.imgBoxPortalLeft});
                    }
                }

                // Need to update vis box (in case this is reached via back-button)
                if (imageV2.backSupport && fullImageReload) {
                    // Update abbr so that rows can be resorted properly
                    var abbr = $(newTr).attr('abbr');

                    if (abbr) {
                        $(tr).attr('abbr', abbr);
                    }

                    if (newJsonRec)
                        vis.update(id, vis.enumOrder[newJsonRec.visibility]);
                }

                return true;
            }
        }
        return false;
    },

    updateImgForAllIds: function (response, oldJson, newJson)
    {   // update all rows in imgTbl based upon navigateInPlace response.
        var imgTbl = $('#imgTbl');

        // We update rows one at a time 
        // (b/c updating the whole imgTbl at one time doesn't work in IE).
        var id;
        for (id in newJson.trackDb) {
            var newJsonRec = newJson.trackDb[id];
            var oldJsonRec = oldJson.trackDb[id];
            
            if (newJsonRec.visibility === 0)  // hidden 'ruler' is in newJson.trackDb!
                continue;
            if (newJsonRec.type === "remote")
                continue;
            if (oldJsonRec &&  oldJsonRec.visibility !== 0 && $('tr#tr_' + id).length === 1) {
                // New track replacing old:
                if (!imageV2.updateImgForId(response, id, true, newJsonRec))
                    warn("Couldn't parse out new image for id: " + id);
            } else { //if (!oldJsonRec || oldJsonRec.visibility === 0)
                // New track seen for the first time
                if (imageV2.backSupport) {
                    $(imgTbl).append("<tr id='tr_" + id + "' abbr='0'" + // abbr gets filled in
                                        " class='imgOrd trDraggable'></tr>");
                    if (!imageV2.updateImgForId(response, id, true, newJsonRec))
                        warn("Couldn't insert new image for id: " + id);
                }
            }
        }
        if (imageV2.backSupport) {
            // Removes OLD: those in oldJson but not in newJson
            for (id in oldJson.trackDb) {
                if ( ! newJson.trackDb[id] )
                    $(document.getElementById('tr_' + id)).remove();
            }
            
            // Need to reorder the rows based upon abbr
            dragReorder.sort($(imgTbl));
        }
    },
    
    updateChromImg: function (response)
    {   // Parse out new chrom 'ideoGram' (if available)
        // e.g.: <IMG SRC = "../trash/hgtIdeo/hgtIdeo_hgwdev_larrym_61d1_8b4a80.gif"
        //                BORDER=1 WIDTH=1039 HEIGHT=21 USEMAP=#ideoMap id='chrom' style='display: inline;'>
        // If the ideo is hidden or missing, we supply a place-holder for dynamic update later.
        // e.g.: <IMG SRC = ""
        //                BORDER=1 WIDTH=1039 HEIGHT=0 USEMAP=#ideoMap id='chrom' style='display: none'>
        // Larry's regex voodoo:
        var a = /<IMG([^>]+SRC[^>]+id='chrom'[^>]*)>/.exec(response);
        if (a && a[1]) {
            var b = /SRC\s*=\s*"([^")]*)"/.exec(a[1]);
            if (b) { // tolerate empty SRC= string when no ideo
                $('#chrom').attr('src', b[1]);
                var c = /style\s*=\s*'([^')]+)'/.exec(a[1]);
                if (c && c[1]) {
                    $('#chrom').attr('style', c[1]);
                }
                var d = /HEIGHT\s*=\s*(\d*)/.exec(a[1]);
                if (d && d[1]) {
                    $('#chrom').attr('HEIGHT', d[1]);
                }
                // Even if we're on the same chrom, ideoMap may change because the label
                // on the left changes width depending on band name, and that changes px scaling.
                var ideoMapMatch = /<MAP Name=ideoMap>[\s\S]+?<\/MAP>/.exec(response);
                if (ideoMapMatch) {
                    var $oldMap = $("map[name='ideoMap']");
                    var $container = $oldMap.parent();
                    $oldMap.remove();
                    $container.append(ideoMapMatch[0]);
                }
                if (imageV2.backSupport) {
                    // Reinit chrom dragging.
                    if ($('area.cytoBand').length >= 1) {
                        $('img#chrom').chromDrag();
                    }
                }
            }
        }
    },

    updateBackground: function (response)
    {
        // Added by galt to update window separators
        // Parse out background image url
        // background-image:url("../trash/hgt/blueLines1563-118-12_hgwdev_galt_9df9_e33b30.png")
        // background-image:url("../trash/hgt/winSeparators_hgwdev_galt_5bcb_baff60.png")
        // This will only need to update when multi-region is on and is using winSeparators.

        var a = /background-image:url\("(..\/trash\/hgt\/winSeparators[^"]+[.]png)"\)/.exec(response);
        if (a && a[1]) {
            $('td.tdData').css("background-image", "url("+a[1]+")");
        }

    },

        
    requestImgUpdate: function (trackName,extraData,loadingId,newVisibility)
    {
        // extraData, loadingId and newVisibility are optional
        var data = "hgt.trackImgOnly=1&hgsid=" + getHgsid() + "&hgt.trackNameFilter=" + trackName;
        if (extraData && extraData !== "")
            data += "&" + extraData;
        if (!loadingId || loadingId === "")
            loadingId = showLoadingImage("tr_" + trackName);
        var getOrPost = "GET";
        if ((data.length) > 2000) // extraData could contain a bunch of changes from the cfg dialog
            getOrPost = "POST";
        $.ajax({
                    type: getOrPost,
                    url: "../cgi-bin/hgTracks",
                    data: cart.addUpdatesToUrl(data),
                    dataType: "html",
                    trueSuccess: imageV2.updateImgAndMap,
                    success: catchErrorOrDispatch,
                    error: errorHandler,
                    cmd: 'refresh',
                    loadingId: loadingId,
                    id: trackName,
                    newVisibility: newVisibility,
                    cache: false
                });
    },

    fullReload: function(extraData)
    {
        // force reload of whole page via trackform submit
        // This function does not return
        jQuery('body').css('cursor', 'wait');
        if (extraData || cart.updatesWaiting()) {
            var url = cart.addUpdatesToUrl(window.location.href);
            if (extraData) {
                if ( url.lastIndexOf("?") === -1)
                    url += "?" + extraData;
                else
                    url += '&' + extraData;
            }
            window.location.assign(url);
            return false;
        }
        document.TrackHeaderForm.submit();
    },

    updateImgAndMap: function (response, status)
    {   // Handle ajax response with an updated trackMap image, map and optional ideogram. 
        //    and maybe the redLines background too.
        // this.cmd can be used to figure out which menu item triggered this.
        // this.id === appropriate track if we are retrieving just a single track.

        // update local hgTracks.trackDb to reflect possible side-effects of ajax request.

        var newJson = scrapeVariable(response, "hgTracks");

        //alert(JSON.stringify(newJson)); // DEBUG Example

        var oldJson = hgTracks;
        var valid = false;
        if (!newJson) {
            var stripped = {};
            stripJsEmbedded(response, true, stripped);
            if ( ! stripped.warnMsg )
                warn("hgTracks object is missing from the response");
        } else {
            if (this.id) {
                if (newJson.trackDb[this.id]) {
                    var visibility = vis.enumOrder[newJson.trackDb[this.id].visibility];
                    var limitedVis;
                    if (newJson.trackDb[this.id].limitedVis)
                        limitedVis = vis.enumOrder[newJson.trackDb[this.id].limitedVis];
                    if (this.newVisibility && limitedVis && this.newVisibility !== limitedVis)
                        // see redmine 1333#note-9
                        alert("There are too many items to display the track in " +
                                this.newVisibility + " mode.");
                    var rec = oldJson.trackDb[this.id];
                    rec.limitedVis = newJson.trackDb[this.id].limitedVis;
                    vis.update(this.id, visibility);
                    valid = true;
                } else {
                    warn("Invalid hgTracks.trackDb received from the server");
                }
            } else {
                valid = true;
            }
        }
        if (valid) {
            if (imageV2.enabled
            && this.id
            && this.cmd
            && this.cmd !== 'wholeImage'
            && this.cmd !== 'selectWholeGene'
            && !newJson.virtChromChanged) {
                // Extract <TR id='tr_ID'>...</TR> and update appropriate row in imgTbl;
                // this updates src in img_left_ID, img_center_ID and img_data_ID
                // and map in map_data_ID
                var id = this.id;
                if (imageV2.updateImgForId(response, id, false)) {
                    imageV2.afterReload(id);
                    imageV2.updateBackground(response);  // Added by galt to update window separators
                } else {
                    warn("Couldn't parse out new image for id: " + id);
                    // Very helpful when debugging and alert doesn't render the html:
                    //alert("Couldn't parse out new image for id: " + id+"BR"+response);
                }
            } else {
                if (imageV2.enabled) {
                    // Implement in-place updating of hgTracks image
                    // GALT delaying this until after newJson updated in hgTracks so disguising works
                    //genomePos.setByCoordinates(newJson.chromName, newJson.winStart + 1, newJson.winEnd);
                    $("input[name='c']").val(newJson.chromName);
                    $("input[name='l']").val(newJson.winStart);
                    $("input[name='r']").val(newJson.winEnd);

                    if (newJson.cgiVersion !== oldJson.cgiVersion || newJson.virtChromChanged) {
                        // Must reload whole page because of a new version on the server;
                        // this should happen very rarely. Note that we have already updated 
                        // position based on the user's action.
                        imageV2.fullReload();
                    } else {
                        // Will rebuild image adding new, removing old and resorting tracks
                        imageV2.updateImgForAllIds(response,oldJson,newJson);
                        imageV2.updateChromImg(response);
                        imageV2.updateBackground(response);  // Added by galt to update window separators
                        hgTracks = newJson;
                        genomePos.original = undefined;
                        genomePos.setByCoordinates(hgTracks.chromName, hgTracks.winStart + 1, hgTracks.winEnd); // MOVED HERE GALT
                        initVars();
                        imageV2.afterReload();
                    }
                } else {
                    warn("ASSERT: Attempt to update track without advanced javascript features.");
                }
            }
            if (hgTracks.measureTiming) {
                imageV2.updateTiming(response);
            }
        }
        if (this.disabledEle) {
            this.disabledEle.removeAttr('disabled');
        }
        if (this.loadingId) {
            hideLoadingImage(this.loadingId);
        }
        jQuery('body').css('cursor', '');
        if (valid && this.currentId) {
            var top = $(document.getElementById("tr_" + this.currentId)).position().top;
            $(window).scrollTop(top - this.currentIdYOffset);
        }
    },

    loadRemoteTracks: function ()
    {
        if (hgTracks.trackDb) {
            for (var id in hgTracks.trackDb) {
                var rec = hgTracks.trackDb[id];
                if (rec.type === "remote") {
                    if ($("#img_data_" + id).length > 0) {
                        // load the remote track renderer via jsonp
                        rec.loadingId = showLoadingImage("tr_" + id);
                        var script = document.createElement('script');
                        var pos = parsePosition(genomePos.get());
                        var name = rec.remoteTrack || id;
                        script.setAttribute('src',
                                rec.url + "?track=" + name +
                                "&jsonp=imageV2.remoteTrackCallback&position=" +
                                encodeURIComponent(pos.chrom + ":" + pos.start + "-" + pos.end) +
                                "&pix=" + $('#imgTbl').width()
                        );
                        document.getElementsByTagName('head')[0].appendChild(script);
                    }
                }
            }
        }
    },

    remoteTrackCallback: function (rec)
    // jsonp callback to load a remote track.
    {
        if (rec.error) {
            alert("retrieval from remote site failed with error: " + rec.error);
        } else {
            var remoteTrack = rec.track;
            for (var track in hgTracks.trackDb) {
                if (hgTracks.trackDb[track].remoteTrack === remoteTrack) {
                    $('#img_data_' + track).attr('style', "left:-116px; top: -23px;");
                    $('#img_data_' + track).attr('height', rec.height);
                    // XXXX use width in some way?
           //       $('#img_data_' + track).attr('width', rec.width);
                    $('#img_data_' + track).attr('width', $('#img_data_ruler').width());
                    $('#img_data_' + track).attr('src', rec.img);
          /* jshint loopfunc: true */// function inside loop works and replacement is awkward.
                    $('#td_data_' + track + ' > div').each(function(index) {
                        if (index === 1) {
                            var style = $(this).attr('style');
                            style = style.replace(/height:\s*\d+/i, "height:" + rec.height);
                            $(this).attr('style', style);
                        }
                    });
                    var style = $('#p_btn_' + track).attr('style');
                    style = style.replace(/height:\s*\d+/i, "height:" + rec.height);
                    $('#p_btn_' + track).attr('style', style);
                    if (hgTracks.trackDb[track].loadingId) {
                        hideLoadingImage(hgTracks.trackDb[track].loadingId);
                    }
                }
            }
        }
    },

    navigateButtonClick: function (ele) // called from hgTracks.c
    {   // code to update just the imgTbl in response to navigation buttons (zoom-out etc.).
        if (imageV2.inPlaceUpdate) {
            var params = ele.name + "=" + ele.value;
            $(ele).attr('disabled', 'disabled');
            // dinking navigation needs additional data
            if (ele.name === "hgt.dinkLL" || ele.name === "hgt.dinkLR") {
                params += "&dinkL=" + $("input[name='dinkL']").val();
            } else if (ele.name === "hgt.dinkRL" || ele.name === "hgt.dinkRR") {
                params += "&dinkR=" + $("input[name='dinkR']").val();
            }
            imageV2.navigateInPlace(params, $(ele), false);
            return false;
        } else {
            return true;
        }
    },

    updateButtonClick: function (ele) // UNUSED?
    {   // code to update the imgTbl based on changes in the track controls.
        // This is currently experimental code and is dead in the main branch.
        if (imageV2.mapIsUpdateable) {
            var data = "";
            $("select").each(function(index, o) {
                var cmd = $(this).val();
                if (cmd === "hide") {
                    if (hgTracks.trackDb[this.name]) {
                        alert("Need to implement hide");
                    }
                } else {
                    if ( ! hgTracks.trackDb[this.name]
                    ||  cmd !== vis.enumOrder[hgTracks.trackDb[this.name].visibility]) {
                        if (data.length > 0) {
                            data = data + "&";
                        }
                        data = data + this.name + "=" + cmd;
                    }
            }
            });
            if (data.length > 0) {
                imageV2.navigateInPlace(data, null, false);
            }
            return false;
        } else {
            return true;
        }
    },

    navigateInPlace: function (params, disabledEle, keepCurrentTrackVisible)
    {   // request an hgTracks image, using params
        // disabledEle is optional; this element will be enabled when update is complete
        // If keepCurrentTrackVisible is true, we try to maintain relative position of the item
        // under the mouse after the in-place update.
        // Tim thinks we should consider disabling all UI input while we are doing in-place update.
        // TODO: waitOnFuction?
    
        // No ajax image update if there are too many tracks!
        if (imageV2.manyTracks()) {
            imageV2.fullReload(params);
            return false;  // Shouldn't return from fullReload but I have seen it in FF
        }
    
        // If UCSC Genes (or any suggestion) is supposed to be made visible, then do so
        if ($("#suggestTrack").length && $('#hgFindMatches').length)
            vis.makeTrackVisible($("#suggestTrack").val());

        jQuery('body').css('cursor', 'wait');
        var currentId, currentIdYOffset;
        if (keepCurrentTrackVisible) {
            var item = rightClick.currentMapItem || imageV2.lastTrack;
            if (item) {
                var top = $(document.getElementById("tr_" + item.id)).position().top;
                if (top >= $(window).scrollTop()
                || top < $(window).scrollTop() + $(window).height()) {
                    // don't bother if the item is not currently visible.
                    currentId = item.id;
                    currentIdYOffset = top - $(window).scrollTop();
                }
            }
        }
        $.ajax({
                type: "GET",
                url: "../cgi-bin/hgTracks",
                data: cart.addUpdatesToUrl(params + 
                                      "&hgt.trackImgOnly=1&hgt.ideogramToo=1&hgsid=" + getHgsid()),
                dataType: "html",
                trueSuccess: imageV2.updateImgAndMap,
                success: catchErrorOrDispatch,
                error: errorHandler,
                cmd: 'wholeImage',
                loadingId: showLoadingImage("imgTbl"),
                disabledEle: disabledEle,
                currentId: currentId,
                currentIdYOffset: currentIdYOffset,
                cache: false
            });
    },

    disguiseHighlight: function(position)
    // disguise highlight position
    {
        pos = parsePositionWithDb(position);
        // DISGUISE
        if (hgTracks.virtualSingleChrom && (pos.chrom.search("virt") === 0)) {
            var positionStr = pos.chrom+":"+pos.start+"-"+pos.end;
            var newPosition = genomePos.disguisePosition(positionStr);
            var newPos = parsePosition(newPosition);
            pos.chrom = newPos.chrom;
            pos.start = newPos.start;
            pos.end   = newPos.end;
        }
        return pos.db+"."+pos.chrom+":"+pos.start+"-"+pos.end+pos.color;
    },

    undisguiseHighlight: function(pos)
    // undisguise highlight pos
    {
        // UN-DISGUISE
        if (hgTracks.virtualSingleChrom && (pos.chrom.search("virt") !== 0)) {
            var position = pos.chrom+":"+pos.start+"-"+pos.end;
            var newPosition = genomePos.undisguisePosition(position);
            var newPos = parsePosition(newPosition);
            if (newPos) {
                pos.chrom = newPos.chrom;
                pos.start = newPos.start;
                pos.end   = newPos.end;
            }
        }
    },

    highlightRegion: function()
    // highlight vertical region in imgTbl based on hgTracks.highlight (#709).
    {
        var pos;
        var hexColor = dragSelect.hlColorDefault;
        $('.highlightItem').remove();
        if (hgTracks.highlight) {
            var hlArray = hgTracks.highlight.split("|"); // support multiple highlight items
            for (var i = 0; i < hlArray.length; i++) {
                hlString = hlArray[i];
                pos = parsePositionWithDb(hlString);
                // UN-DISGUISE
                imageV2.undisguiseHighlight(pos);
                if (pos) {
                    pos.start--;  // make start 0-based to match hgTracks.winStart
                    if (pos.color)
                        hexColor = pos.color;
                }

                if (pos && pos.chrom === hgTracks.chromName && pos.db === getDb() 
                &&  pos.start <= hgTracks.imgBoxPortalEnd && pos.end >= hgTracks.imgBoxPortalStart) {
                    var portalWidthBases = hgTracks.imgBoxPortalEnd - hgTracks.imgBoxPortalStart;
                    var portal = $('#imgTbl td.tdData')[0];
                    var leftPixels = $(portal).offset().left + imageV2.LEFTADD;
                    var pixelsPerBase = ($(portal).width() - 2) / portalWidthBases;
                    var clippedStartBases = Math.max(pos.start, hgTracks.imgBoxPortalStart);
                    var clippedEndBases = Math.min(pos.end, hgTracks.imgBoxPortalEnd);
                    var widthPixels = (clippedEndBases - clippedStartBases) * pixelsPerBase;
                    if (hgTracks.revCmplDisp)
                        leftPixels += (hgTracks.imgBoxPortalEnd - clippedEndBases) * pixelsPerBase - 1;
                    else
                        leftPixels += (clippedStartBases - hgTracks.imgBoxPortalStart) * pixelsPerBase;
                    // Impossible to get perfect... Okay to overrun by a pixel on each side
                    leftPixels  = Math.floor(leftPixels);
                    widthPixels = Math.ceil(widthPixels);
                    if (widthPixels < 2) {
                        widthPixels = 3;
                        leftPixels -= 1;
                    }
    
                    var area = jQuery("<div id='highlightItem' class='highlightItem'></div>");
                    $(area).css({ backgroundColor: hexColor, // display: 'none'
                                left: leftPixels + 'px', top: $('#imgTbl').offset().top + 1 + 'px',
                                width: widthPixels + 'px',
                                height: $('#imgTbl').css('height') });
                    $(area).data({leftPixels: leftPixels, widthPixels: widthPixels});// needed by dragScroll
    
                    // Larry originally appended to imgTbl, but discovered that doesn't work on IE 8 and 9.
                    $('body').append($(area)); 
                    // z-index is done in css class, so highlight is beneath transparent data images.
                    // NOTE: ideally highlight would be below transparent blue-lines, but THAT is a  
                    // background-image so z-index can't get below it!  PS/PDF looks better for blue-lines!
                }
            }
        }
    },

    backSupport: (window.History.enabled !== undefined), // support of our back button via: 
    history: null,                                     //  jquery.history.js and HTML5 history API
    
    setupHistory: function ()
    {   // Support for back-button using jquery.history.js.
        // Sets up the history and initializes a state.
    
        // Since ajax updates leave the browser cached pages different from the server state, 
        // simple back-button fails.  Using a 'dirty flag' we had forced an update from server,
        // whenever the back button was hit, meaning there was no going back from server-state!
        // NOW using the history API, the back-button triggers a 'statechange' event which can 
        // contain data.  We save the position in the data and ajax update the image when the
        // back-button is pressed.  This works great for going back through ajax-updated position
        // changes, but is a bit messier when going back past a full-page retrieved state (as
        // described below).
    
        // NOTE: many things besides position could be ajax updated (e.g. track visibility). We are
        // using the back-button to keep track of position only.  Since the image should be updated
        // every-time the back button is pressed, all track settings should persist (not go back).
        // What will occasionally fail is vis box state and group expansion state. This is because
        // the back-button goes to a browser cached page and then the image alone is updated.
    
        imageV2.history = window.History;
        
        // The 'statechange' function triggerd by the back-button.
        // Whenever the position changes, then use ajax-update to refetch the position
        imageV2.history.Adapter.bind(window,'statechange',function(){
            var prevDbPos = imageV2.history.getState().data.lastDbPos;
            var prevPos = imageV2.history.getState().data.position;
            var curDbPos = hgTracks.lastDbPos;
            if (prevDbPos && prevDbPos !== curDbPos) {
                // NOTE: this function is NOT called when backing past a full retrieval boundary
                genomePos.set(decodeURIComponent(prevPos));
                imageV2.navigateInPlace("" + prevDbPos, null, false);
            }
        });
        
        // With history support it is best that most position changes will ajax-update the image
        // This ensures that the 'go' and 'refresh' button will do so unless the chrom changes.
        $("input[value='go'],input[value='refresh']").click(function () {
            var newPos = genomePos.get().replace(/,/g,'');
            if (newPos.length > 2000) {
               alert("Sorry, you cannot paste identifiers or sequences with more than 2000 characters into this box.");
               $('input[name="hgt.positionInput"]').val("");
               return false;
            }

            var newDbPos = hgTracks.lastDbPos;
            if ( ! imageV2.manyTracks() ) {
                var newChrom = newPos.split(':')[0];
                var oldChrom  = genomePos.getOriginalPos().split(':')[0];
                if (newChrom === oldChrom) {
                    imageV2.markAsDirtyPage();
                    imageV2.navigateInPlace("position=" + newPos, null, false);
                    window.scrollTo(0,0);
                    return false;
                }
            }
            
            // If not just image update AND there are vis updates waiting...
            if (cart.updatesWaiting()) {
                var url = "../cgi-bin/hgTracks?position=" + newPos + "&" + cart.varsToUrlData({ 'db': getDb(), 'hgsid': getHgsid() });
                window.location.assign(url);
                return false;
            }

            // redirect to hgBlat if the input looks like a DNA sequence
            // minimum length=19 so we do not accidentally redirect to hgBlat for a gene identifier 
            // like ATG5
            var dnaRe = new RegExp("^(>[^\n\r ]+[\n\r ]+)?(\\s*[actgnACTGN \n\r]{19,}\\s*)$");
            if (dnaRe.test(newPos)) {
                var blatUrl = "hgBlat?type=BLAT%27s+guess&userSeq="+newPos;
                window.location.href = blatUrl;
                return false;
            }
                
            return true;
        });
        // Have vis box changes update cart through ajax.  This helps keep page/cart in sync.
        vis.initForAjax();

        // We reach here from these possible paths:
        // A) Forward: Full page retrieval: hgTracks is first navigated to (or chrom change)
        // B) Back-button past a full retrieval (B in: ->A,->b,->c(full page),->d,<-c,<-B(again))
        //    B1) Dirty page: at least one non-position change (e.g. 1 track vis changed in b)
        //    B2) Clean page: only position changes from A->b->| 
        var curPos = encodeURIComponent(genomePos.get().replace(/,/g,''));
        var curDbPos = hgTracks.lastDbPos;
        var cachedPos = imageV2.history.getState().data.position;
        var cachedDbPos = imageV2.history.getState().data.lastDbPos;
        // A) Forward: Full page retrieval: hgTracks is first navigated to (or chrom change)
        if (!cachedDbPos) { // Not a back-button operation
            // set the current position into history outright (will replace). No img update needed
            imageV2.setInHistory(true);
        } else { // B) Back-button past a full retrieval
            genomePos.set(decodeURIComponent(cachedPos));
            // B1) Dirty page: at least one non-position change 
            if (imageV2.isDirtyPage()) {
                imageV2.markAsCleanPage();
                // Only forcing a full page refresh if chrom changes
                var cachedChrom = decodeURIComponent(cachedPos).split(':')[0];
                var curChrom    = decodeURIComponent(   curPos).split(':')[0];
                if (cachedChrom === curChrom) {
                    imageV2.navigateInPlace("db="+getDb()+"&"+cachedDbPos, null, false);
                } else {
                    imageV2.fullReload();
                }
            } else {
                // B2) Clean page: only position changes from a->b 
                if (cachedDbPos !== curDbPos) {
                    imageV2.navigateInPlace("db="+getDb()+"&"+cachedDbPos, null, false);
                }
            }
            // Special because FF is leaving vis drop-downs disabled
            vis.restoreFromBackButton();
        }
    },
    
    setInHistory: function (fullPageLoad)
    {   // Keep a position history and allow the back-button to work (sort of)
        // replaceState on initial page load, pushState on each advance
        // When call triggered by back button, the lastPos===newPos, so no action.
        var lastDbPos = imageV2.history.getState().data.lastDbPos;
        var newPos  = encodeURIComponent(genomePos.get().replace(/,/g,''));  // no commas
        var newDbPos  = hgTracks.lastDbPos;
        
        // A full page load could be triggered by back-button, but then there will be a lastPos
        // if this is the case then don't set the position in history again!
        if (fullPageLoad && lastDbPos)
            return;

        if (!lastDbPos || lastDbPos !== newDbPos) {
            // Swap the position into the title
            var title = $('TITLE')[0].text;
            var ttlWords = title.split(' ');
            if (ttlWords.length >= 2) {
                for (var i=1; i < ttlWords.length; i++) {
                    if (ttlWords[i].indexOf(':') >= 0) {
                        ttlWords[i] = genomePos.get();
                    }
                }
                title = ttlWords.join(' ');
            } else
                title = genomePos.get();

            var sid = getHgsid();
            if (fullPageLoad) { 
                // Should only be on initial set-up: first navigation to page
                imageV2.history.replaceState({lastDbPos: newDbPos, position: newPos, hgsid: + sid },title,
                                 "hgTracks?db="+getDb()+"&"+newDbPos+"&hgsid="+sid);
            } else {  
                // Should be when advancing (not-back-button)
                imageV2.history.pushState({lastDbPos: newDbPos, position: newPos, hgsid: + sid },title,
                                 "hgTracks?db="+getDb()+"&"+newDbPos+"&hgsid="+sid);
            }
        }
    }
};

  //////////////////////
 //// track search ////
//////////////////////
var trackSearch = {

    searchKeydown: function (event)
    {
        if (event.which === 13) {
            // Required to fix problem on IE and Safari where value of hgt_tSearch is "-"
            //    (i.e. not "Search").
            // NOTE: must match TRACK_SEARCH_PAGER in hg/inc/searchTracks.h
            $("input[name=hgt_tsPage]").val(0);  
            $('#trackSearch').submit();
            // This doesn't work with IE or Safari.
            // $('#searchSubmit').click();
        }
    },

    init: function ()
    {
        // Track search uses tabs
        if ($("#tabs").length > 0) {
            // Search page specific code

            var val = $('#currentTab').val();
            $("#tabs").tabs({
                                show: function(event, ui) {
                                    $('#currentTab').val(ui.panel.id);
                                },
                                select: function(event, ui) { findTracks.switchTabs(ui); }
                            });
            $('#tabs').show();
            $("#tabs").tabs('option', 'selected', '#' + val);
            if (val === 'simpleTab' && $('div#found').length < 1) {
                $('input#simpleSearch').focus();
            }
            $("#tabs").css('font-family', jQuery('body').css('font-family'));
            $("#tabs").css('font-size', jQuery('body').css('font-size'));
            $('.submitOnEnter').keydown(trackSearch.searchKeydown);
            findTracks.normalize();
            findTracks.updateMdbHelp(0);
        }
    }
};


  ///////////////
 //// READY ////
///////////////
$(document).ready(function()
{
    // The page may be reached via browser history (back button)
    // If so, then this code should detect if the image has been changed via js/ajax
    // and will reload the image if necessary.
    // NOTE: this is needed for IE but other browsers can detect the dirty page much earlier
    if (!imageV2.backSupport) {
        if (imageV2.isDirtyPage()) {
            // mark as non dirty to avoid infinite loop in chrome.
            imageV2.markAsCleanPage();
            jQuery('body').css('cursor', 'wait');
            window.location = "../cgi-bin/hgTracks?hgsid=" + getHgsid();
            return false;
        }
    }
    initVars();
    imageV2.loadSuggestBox();
    if ($('#pdfLink').length === 1) {
        $('#pdfLink').click(function(i) {
            var thisForm = normed($('#TrackForm'));
            if (thisForm) {
                //alert("posting form:"+$(thisForm).attr('name'));
                updateOrMakeNamedVariable($(thisForm),'hgt.psOutput','on');
                return postTheForm($(thisForm).attr('name'),this.href);
            }
            return true;
        });
    }

    if (imageV2.enabled) {

        // Make imgTbl allow drag reorder of imgTrack rows
        dragReorder.init();
        var imgTable = $(".tableWithDragAndDrop");
        if ($(imgTable).length > 0) {
            $(imgTable).tableDnD({
                onDragClass: "trDrag",
                dragHandle: "dragHandle",
                scrollAmount: 40,
                onDragStart: function(ev, table, row) {
                    mouse.saveOffset(ev);
                    $(document).bind('mousemove',posting.blockTheMapOnMouseMove);

                    // Can drag a contiguous set of rows if dragging blue button
                    table.tableDnDConfig.dragObjects = [ row ]; // defaults to just the one
                    var btn = $( row ).find('p.btnBlue');  // btnBlue means cursor over left button
                    if (btn.length === 1) {
                        table.tableDnDConfig.dragObjects = dragReorder.getContiguousRowSet(row);
                        var compositeSet = dragReorder.getCompositeSet(row);
                        if (compositeSet && compositeSet.length > 0)
                            $( compositeSet ).find('p.btn').addClass('blueButtons');// blue persists
                    }
                },
                onDrop: function(table, row, dragStartIndex) {
                    var compositeSet = dragReorder.getCompositeSet(row);
                    if (compositeSet && compositeSet.length > 0)
                        $( compositeSet ).find('p.btn').removeClass('blueButtons');// blue persists
                    if ($(row).attr('rowIndex') !== dragStartIndex) {
                        // NOTE Even if dragging a contiguous set of rows,
                        // still only need to check the one under the cursor.
                        if (dragReorder.setOrder) {
                            dragReorder.setOrder(table);
                        }
                        dragReorder.zipButtons( table );
                    }
                    $(document).unbind('mousemove',posting.blockTheMapOnMouseMove);
                    // Timeout necessary incase the onDrop over map item. onDrop takes precedence.
                    setTimeout(posting.allowMapClicks,100);
                }
            });
        }

        // Drag scroll init
        if (hgTracks.imgBoxPortal) {
            // Turn on drag scrolling.
            $("div.scroller").panImages();
        }

        // Retrieve tracks via AJAX that may take too long to draw initialliy (i.e. a remote bigWig)
        var retrievables = $('#imgTbl').find("tr.mustRetrieve");
        if ($(retrievables).length > 0) {
            $(retrievables).each( function (i) {
                var trackName = $(this).attr('id').substring(3);
                imageV2.requestImgUpdate(trackName,"","");
            });
        }
        imageV2.loadRemoteTracks();
        makeItemsByDrag.load();
        
        // Any highlighted region must be shown and warnBox must play nice with it.
        imageV2.highlightRegion();
        // When warnBox is dismissed, any image highlight needs to be redrawn.
        $('#warnOK').click(function (e) { imageV2.highlightRegion();});
        // Also extend the function that shows the warn box so that it too redraws the highlight.
        showWarnBox = (function (oldShowWarnBox) {
            function newShowWarnBox() {
                oldShowWarnBox.apply();
                imageV2.highlightRegion();
            }
            return newShowWarnBox;
        })(showWarnBox);
    }

    // Drag select in chromIdeogram
    if ($('img#chrom').length === 1) {
        if ($('area.cytoBand').length >= 1) {
            $('img#chrom').chromDrag();
        }
    }

    // Track search uses tabs
    trackSearch.init();

    // Drag select initialize
    if (imageV2.enabled) {   // moved from window.load().
        dragSelect.load(true);

        if ($('#hgTrackUiDialog'))
            $('#hgTrackUiDialog').hide();

        // Don't load contextMenu if jquery.contextmenu.js hasn't been loaded
        if (jQuery.fn.contextMenu) {
            rightClick.load(imageV2.imgTbl);
        }
    }

    // jquery.history.js Back-button support
    if (imageV2.enabled && imageV2.backSupport) {
        imageV2.setupHistory();
    }

});
