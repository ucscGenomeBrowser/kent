// JavaScript Especially for the Gene Haplotype Alleles section of hgGene CGI

// Tell jslint and browser to use strict mode for this file:
"use strict";

// Alleles supports gene haplotype alleles section
var alleles = (function()
{

    var sectionName         = "haplotypes";  // This define is used throughout
    var seqCharsPerPos      = 1;  // These globals should only need to be calculated once
    var seqPxPerPos         = 7;
    var ajaxUpdates         = 0;

    // persistence between ajax calls
    var persistRareHapsShown = false; 
    var persistScoresShown   = false; 
    var persistSortReverse   = false;
    var persistSortColId     = '';


    function initSortTable()
    { // Initialize the sortable table
        var allelesTable = $('table#alleles.sortable');
        if (allelesTable.length === 1) {
            sortTable.initialize(allelesTable[0],false);
            sortTable.sortCaseSensitive(true);
        }
    }

    function update(content, status)
    { // Update the geneAlleles section based upon ajax request
        hideLoadingImage(this.loadingId);  // Do this first
        
        var geneAlleles = $('div#' + sectionName);
        if (geneAlleles.length > 0) {
            
            var cleanHtml = content;
            //cleanHtml = stripJsFiles(cleanHtml,true);   // DEBUG msg with true
            //cleanHtml = stripCssFiles(cleanHtml,true);  // DEBUG msg with true
            //cleanHtml = stripJsEmbedded(cleanHtml,true);// DEBUG msg with true
            var sectionBegin = "<!-- " + sectionName + " begin -->";
            var sectionEnd   = "<!-- " + sectionName + " end -->";
            var ix = cleanHtml.indexOf(sectionBegin);
            if (ix > 0)
                cleanHtml = cleanHtml.substring(ix);
            ix = cleanHtml.indexOf(sectionEnd);
            if (ix > 0)
                cleanHtml = cleanHtml.substring(0,ix + sectionEnd.length);

            if (cleanHtml.length > 0) {
                ajaxUpdates++;
                $(geneAlleles[0]).html( cleanHtml );
                hiliteRemove();
                alleles.initialize();  // Must have prefix, since ajax call
            }
        }
    }
    
    function ajaxRequest(data)
    { // Request an ajax update of this section
    
        // Use current url but make sure it is relative
        var thisUrl = window.location.href;
        var ix = thisUrl.indexOf("cgi-bin");
        if (ix > 0)
            thisUrl = "../" + thisUrl.substring(ix);
        
        $.ajax({
                type: "GET",
                url: thisUrl,
                data: "ajax=1&" + data + "&ajaxSection=" + sectionName,
                dataType: "html",
                trueSuccess: update,
                success: catchErrorOrDispatch,
                error: errorHandler,
                //async: false,
                //cmd: cmd,
                loadingId: showLoadingImage(sectionName),
                cache: false
            });
    }
    
    function afterSort(obj)
    {   // The sort table is controlled by utls.js, but this is some allele specific stuff
    
        var thisId = $(obj).attr('id');
        if (persistSortColId != thisId)
            persistSortColId  = thisId;
        else
            persistSortReverse = ( !persistSortReverse );
        
        // Sorted variant is hilited special
        if ($(obj).hasClass('var')) {
            hiliteId = thisId;
            hiliteSpecial( hiliteId );
        }
    }

    function persistThroughUpdates()
    { // When an ajax update occurs, restore the non-ajax settings state
        
        // See if table was sorted previously
        if (persistSortColId != '') {
    
            var col = $('table#alleles').find('TH#' + persistSortColId);
            if (col != undefined && col.length != 0) {
                var colIx = Number( $(col).attr('cellIndex') );
                $(col).click();
                if (persistSortReverse) {// click twice if reverse order!
                    persistSortReverse = false; // Needed so that it is set again
                    setTimeout("$('table#alleles').find('TH#" + persistSortColId + 
                                                                          "').click();", 50);
                }
            }
        }
        
        // Make sure that sort command saves the coumn for persistence
        $('table#alleles').find('TH').click(function (e) { afterSort(this); });
        
        // Persist on lighlite as red
        if (hiliteId != '')
            hiliteSpecial( hiliteId );
    }

    function propgateTitle(obj)
    { // Adds this objects title to all other objs with the same class
      // Relies upon a span and fixed width text
        // Find pointer position within text
    
        // Is there a title?
        var theTitle = obj.title;
        if (theTitle == undefined || theTitle.length === 0)
            return;

        var varId = $( obj ).attr('id');
        $('table#alleles').find('TD.' + varId).not("[title]").attr('title', theTitle);
    }
    
    var hilites  = [];  // Hold on to all the hilite ids that are currently displayed
    var hiliteId = '';
    
    function hiliteTag(xPx, widthPx)
    { // returns an id based upon dimensions 
        return "hilite" + xPx.toFixed(0) + "-" + widthPx.toFixed(0);
    }
    
    function hiliteAdd(xPx, widthPx, yPx, heightPx)
    {   // Adds a hilite div with the specific dimensions.
        // Returns the div for further customization
        if (xPx === 0)
            return;
    
        // make sure it doesn't already exist!
        var hId = hiliteTag(xPx,widthPx);
        if (hilites.indexOf(hId) != -1)
            return hId;
        
        var tbl = $('table#alleles');
        var tripleView = ($(tbl).find('TD.dnaToo').length > 0);
        
        if (yPx == undefined) {
            if (tripleView)
                yPx = $(tbl).position().top + 2;  // span whole height of table
            else
                yPx = $(tbl).find('TH#seq').position().top; // skip first row of header
        }
        if (heightPx == undefined || heightPx === 0) {
            if (tripleView)
                heightPx = $(tbl).height() - 8;  // span whole height of table
            else
                heightPx = $(tbl).height() - (2 * $(tbl).find('TH#seq').height()) + 4;
        }
        
        var div = $("<div class='hilite' id='"+hId+"' style='display:none' />");
        $(tbl).after(div);
        $(div).css({ width: widthPx + 'px', height: heightPx + 'px', 
                      left: xPx     + 'px',    top: yPx      + 'px'});
        $(div).show();
        
        hilites.push( hId );

        return div;
    }
    
    function hiliteRemove(id)
    {   // removes a specific hilite or all hilite divs if id is not supplied. 
        if (id == undefined || id === "") {
            $('div.hilite').remove();
            hilites = [];
        } else {
            $('div.hilite').remove('#' + id);
            var ix = hilites.indexOf(hId);
            if (ix != -1)
                hilites.splice(ix,1);
        }
    }
    
    function hiliteSpecial(id)
    {   // makes one single hilite more noticeable
 
        $('div.hiliteSpecial').removeClass('hiliteSpecial');
        if (id != undefined && id != "") {
            $('div.hilite.'+id).addClass('hiliteSpecial');
            //$('td.var.'+id).addClass('hiliteSpecial');
            // Need to find column, then highlight each cell in column with some additional style.
        }
    }

    function hilitesResize()
    {   // resizes all hilite divs based upon the table height 
        // In theory just changing height will work.  In practice, need to recalc all
        //$('div.hilite').css('height',($('table#alleles').height() - 4) + 'px');
        if (hilites.length > 0) {
            hiliteRemove();
            hiliteAllDiffs();
            if (hiliteId != '')
                hiliteSpecial( hiliteId );
        }
    }
    
    function hiliteAllDiffs()
    {   // Adds all hilites
    
        // Don't even bother if full sequence isn't showing
        var fullSeq = $('input#'+sectionName+'_fullSeq');
        if (fullSeq.length != 0 && $(fullSeq).val().indexOf('Hide') === -1)
            return;
        
        // DNA view or AA view?
        var spans;
        var dnaView = $('input#'+sectionName+'_dnaView');
        if (dnaView != undefined && $(dnaView).val().indexOf('DNA') === -1) {
            spans = $('table#alleles').find('TH.seq').find('B');
        } else { // AA view
            spans = $('table#alleles').find('TH.seq').find('span');
        }
        // At this point, hilighting should be identical
        $(spans).each(function (ix) {
            var xPx     = $(this).position().left;
            var widthPx = $(this).width();
            if (widthPx === 0)
                widthPx = seqPxPerPos;
            var div = hiliteAdd(xPx,widthPx);
            
            // Can we give it a title and class?
            var varClass = $( this ).attr("class").split(' ');
            if (varClass.length > 0) {
                // One of the classes should be the id for the TH defining the variant
                for (var ix=0; ix < varClass.length; ix++ ) {
                    if (varClass[ix].indexOf('text') != 0) {
                        var variant = $('table#alleles').find('TH.var#' + varClass[ix]);
                        if (variant != undefined && variant.length === 1) {
                            $(div).attr('title',$(variant).attr('title'));
                            $(div).addClass(varClass[ix]);
                            break;
                        }
                    }
                }
            }
        });
    }

    // - - - - - Public methods - - - - - 
    return { // returns an object with public methods
    
        positionTitle: function (e, obj)
        { // sets the current title to show the position of the pointer
          // Relies upon a span and fixed width text
          // Note: charsPerPos == 3 to show aa position when showing DNA triplets
    
            var e = e || window.event;
            var over = ((e.pageX - $(obj).offset().left) / seqPxPerPos) + 0.5; // round up
            $(obj).attr('title',over.toFixed(0)); // title is simply position
        },
    
        rareAlleleToggle: function (btn,setCart)
        { // toggle the visibility of rare alleles
            var trs = $('table#alleles tbody tr.allele');
            persistRareHapsShown = ($(btn).val().indexOf('Show') != -1);
            if (persistRareHapsShown) {
                $(trs).filter('.rare').removeClass('hidden');
                var counts = $(trs).filter(':visible').length + ' of ' + $(trs).length + ".";
                $('span#alleleCounts').text( 'All gene haplotypes shown: ' + counts);
                $('span#alleleCounts').addClass('textAlert'); 
                $(btn).val('Hide rare haplotypes');
                if (setCart == undefined || setCart)
                    setCartVar(btn.id,'1');
            } else {
                $(trs).filter('.rare').addClass('hidden');
                var counts = $(trs).filter(':visible').length + ' of ' + $(trs).length + ".";
                $('span#alleleCounts').text( 'Common gene haplotypes shown: ' + counts );
                $('span#alleleCounts').removeClass('textAlert'); 
                $(btn).val('Show rare haplotypes');
                if (setCart == undefined || setCart)
                    setCartVar(btn.id,'[]');
            }
            hilitesResize();
        },
        
        scoresToggle: function (btn,setCart)
        { // toggle the visibility of scores
            persistScoresShown = ($(btn).val().indexOf('Show') != -1);
            if (persistScoresShown) {
                $('table#alleles').find('.score').removeClass('hidden');
                $('table#alleles').find('.andScore').each(function (ix) { 
                    $(this).attr('colspan',Number($(this).attr('colspan')) + 1);
                });
                $(btn).val('Hide scoring');
                if (setCart == undefined || setCart)
                    setCartVar(btn.id,'1');
            } else {
                $('table#alleles').find('.score').addClass('hidden');
                $('table#alleles').find('.andScore').each(function (ix) { 
                    $(this).attr('colspan',Number($(this).attr('colspan')) - 1);
                });
                $(btn).val('Show scoring');
                if (setCart == undefined || setCart)
                    setCartVar(btn.id,'[]');
            }
            hilitesResize();
        },
        
        scoresShow: function (obj,val)
        { // toggle the visibility of scores
            persistScoresShown = (val == 'set');
            if (persistScoresShown) {
                $('table#alleles').find('.score').removeClass('hidden');
                $(obj).val('[]');
            } else {
                $(obj).val('set');
                $('table#alleles').find('.score').addClass('hidden');
            }
            setCartVar(obj.id,val);
            hilitesResize();
        },
        
        // TODO: Would be good to hide pop and popScore columns, instead of ajax setAndRefresh
        /* popShow: function (obj,val)
        { // toggle the visibility of scores
            persistScoresShown = (val == 'set');
            if (val == 'set') {
                return setAndRefresh(obj.id,val)
                $('table#alleles').find('.score').removeClass('hidden');
                //$(obj).val('[]');
            } else {
                $(obj).val('set');
                $('table#alleles').find('.pop').addClass('hidden');
            }
            setCartVar(obj.id,val);
            hilitesResize();
        },*/
        
        setAndRefresh: function  (varName,val)
        { // Resets all display options to defaults
            ajaxRequest(varName + '=' + val);
            return false; // Fake link
        },
        
        delayedHilites: function ()
        { // Delayed call of private function
            hiliteAllDiffs();
            // Persist on lighlite as red
            if (hiliteId != '')
                hiliteSpecial( hiliteId );
        },
        
        initialize: function  (sectionId)
        { // Initialize or reinitailze (after ajax) the sortable table
        
            // This whole section could be renamed
            //if (sectionId != undefined && sectionId.length !== 0)
            //    sectionName = sectionId;
            
            initSortTable();
            
            var tbl = $('table#alleles');
            $(tbl).find('TH.var').each(function (ix) { 
                propgateTitle(this); 
            });
            
            // Calculate some useful constants:
            seqCharsPerPos = 1; // Most cases
            var tripletButton = normed( $('input#'+sectionName+'_triplets') );
            if (tripletButton != undefined && $(tripletButton).val().indexOf('Show') === -1)
                seqCharsPerPos = 3;
            seqPxPerPos = 7;
            var fullSeqHeader = normed( $(tbl).find('TH.seq') );
            if (fullSeqHeader != undefined) {
                var seqLen = $(fullSeqHeader).text().length;
                //if (seqCharsPerPos === 3)
                //    seqLen /= 2;
                seqPxPerPos = Math.round( $(fullSeqHeader).width() / (seqLen / seqCharsPerPos));
                if ($.browser.msie) 
                    seqPxPerPos = $(fullSeqHeader).width() / (seqLen / seqCharsPerPos);
            }
            
            // Highlight variants in full sequence
            setTimeout("alleles.delayedHilites()", 200);  // Delay till after page settles
            
            // Want to sort on previos column if there was a sort before ajax update
            persistThroughUpdates();
        }
        
    };
}()); // end & invocation of function that makes alleles

// The following js depends upon the jQuery library
$(document).ready(function()
{
    // Gene Alleles table is sortable
    alleles.initialize();
    
    // This is really hgGene specific at this point...
    // If the haplotypes section has full sequence displayed, then other sections would
    // scroll horrizontally off the page without setting the max-width of those sections.
    $('div.subheadingBar').parents('table').css({maxWidth:$(window).width()  + "px"});
});
