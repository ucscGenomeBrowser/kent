// JavaScript Especially for hui.c
// $Header: /projects/compbio/cvsroot/kent/src/hg/js/hui.js,v 1.37 2009/10/09 19:57:57 tdreszer Exp $

var debugLevel = 0;
var viewDDtoSubCB = true;
var viewDDtoSubCBhide = false;
var compositeName = "";
//var viewDDtoMatCB = true; //true;
//var matCBwithViewDD = true;
//var subCBtoMatCB = true;
//var matCBtoSubCB = true; // Always
//var now = new Date();
//var start = now.getTime();
//$(window).load(function () {
//    if(start != null) {
//        now = new Date();
//        alert("Loading took "+(now.getTime() - start)+" msecs.");
//    }
//});


// The 'mat*' functions are especially designed to support subtrack configuration by 2D matrix of controls

function matSelectViewForSubTracks(obj,view)
{
// Handle any necessary changes to subtrack checkboxes when the view changes
// views are "select" drop downs on a subtrack configuration page
    if( obj.selectedIndex == 0) { // hide
        if(viewDDtoSubCBhide) {
            matSetSubtrackCheckBoxes(false,view);
            //if(viewDDtoMatCB)
            //    $("input.matrixCB").filter(":checked").each( function (i) { matChkBoxNormalize(this); } );
        }
        matEnableSubtrackCheckBoxes(false,view);
        hideConfigControls(view);
        //enableViewCfgLink(false,view);  // Could "disable" view cfg when hidden!
    } else {
        //enableViewCfgLink(true,view);   // Would need to reeanble view cfg when visible

        // Make main display dropdown show full if currently hide
        compositeName = obj.name.substring(0,obj.name.indexOf(".")); // {trackName}.{view}.vis
        exposeComposite(compositeName);
        // if matrix used then: essentially reclick all 'checked' matrix checkboxes
        if(viewDDtoSubCB) {
            var CBs = $("input.matrixCB").filter(":checked");
            if(CBs.length > 0) {
                var classSets = new Array();
                CBs.each( function (i) { classSets.push( $(this).attr("class") ); } );
                if(classSets.length > 0) {
                    // Now it would be good to create a list of all subtrack CBs that match view,unchecked, and a class set (pair or triplet!)
                    CBs = $("input.subtrackCB").filter("."+view).not(":checked");
                    if(CBs.length > 0) {
                        while(classSets.length > 0) {
                            var OneOrMoreClasses = classSets.pop();
                            var JustTheseCBs = CBs;
                            if(OneOrMoreClasses.length > 0) {
                                OneOrMoreClasses = OneOrMoreClasses.replace("matrixCB ",""); // "matrixCB K562 CTCF" to "K562 CTCF"
                                var classes = OneOrMoreClasses.split(" ");
                                while(classes.length > 0) {
                                    JustTheseCBs = JustTheseCBs.filter("."+classes.pop());
                                }
                                JustTheseCBs.each( function (i) {
                                    this.checked = true;
                                    setCheckBoxShadow(this);
                                    hideOrShowSubtrack(this);
                                });
                            }
                        }
                    }
                }
            }
        } else {
            matSetSubtrackCheckBoxes(true,view);
        }
        //if(viewDDtoMatCB)
        //    matChkBoxesNormalized();
        //    //$("input.matrixCB").not(":checked").each( function (i) { matChkBoxNormalize(this); } );
        matEnableSubtrackCheckBoxes(true,view);
    }
}

function exposeComposite(compositeName)
{
    // Make main display dropdown show full if currently hide
    var compositeDD = $("select[name='"+compositeName+"']");
    if($(compositeDD).attr('selectedIndex') < 1) { // Composite vis display is HIDE
        var maxVis = ($(compositeDD).children('option').length - 1);
        $(compositeDD).attr('selectedIndex',maxVis);
    }
}

// Obsolete because matCBwithViewDD is not true
//function getViewNamesSelected(on)
//{
//// Returns an array of all views that are on or off (hide)
//// views are "select" drop downs containing 'hide','dense',...
//// To be clear, an array of strings with the view name is returned.
//    var views = new Array();
//    var list = $(".viewDd");
//    if(on)
//        list = $(list).filter("[selectedIndex!=0]")
//    else
//        list = $(list).filter("[selectedIndex=0]")
//    $(list).each( function (i) {
//        views.push(this.name.substring(this.name.indexOf('.') + 1, this.name.lastIndexOf('.')));
//    });
//   return( views );
//}

function checkBoxSet(CB,state)
{
    CB.checked = state;
    setCheckBoxShadow(CB);
    hideOrShowSubtrack(CB);
}

function matSetMatrixCheckBoxes(state)
{
// Set all Matrix checkboxes to state.  If additional arguments are passed in, the list of CBs will be narrowed by the classes
    var CBs;
    if(state)
        CBs = $("input.matrixCB").not(":checked");
    else
        CBs = $("input.matrixCB").filter(":checked").not(".dimZ");// uncheck should not touch dimZ
    for(var vIx=1;vIx<arguments.length;vIx++) {
        CBs = $( CBs ).filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    CBs.each( function (i) { this.checked = state;} )
    //CBs.each( function (i) { if(this.checked != state) this.click();} )

    if(state) {
        CBs = $("input.subtrackCB").not(":checked");
        // need to weed out non-checked dimZ if there are any
        var zCBs = $("input.matrixCB.dimZ").not(":checked");
        if( $( zCBs ).length > 0) {
            var classes = "";            // make string of classes
            $(zCBs).each( function (i) {
                classes += $( this ).attr("class").replace("matrixCB dimZ ",".");
            });
            CBs = $( CBs ).not(classes); // weed CBs
        }
    } else
        CBs = $("input.subtrackCB").filter(":checked");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        CBs = CBs.filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    CBs.each( function (i) { checkBoxSet(this,state); });

    return true;
}

function subtrackCBsSetAll(state)
{
// Set all subtrack checkboxes to state.  If additional arguments are passed in, the list of CBs will be narrowed by the classes
    var CBs;
    if(state)
        CBs = $("input.subtrackCB").not(":checked");
    else
        CBs = $("input.subtrackCB").filter(":checked");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        CBs = CBs.filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    CBs.each( function (i) { checkBoxSet(this,state); });

    return true;
}

function matSetSubtrackCheckBoxes(state)
{
// Set all subtrack checkboxes to state.  If additional arguments are passed in, the list of CBs will be narrowed by the classes
    var CBs = $("input.subtrackCB");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        CBs = CBs.filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    // This next block of code is needed to make dimZ work with dimsX&Y
    if(state) {
        var dimZ = $("input.matrixCB.dimZ");
        if(dimZ.length > 0) {
            if(arguments.length == 3) { // Requested dimX&Y
                dimZ = dimZ.filter(":checked");
                for(var dIx=0;dIx<dimZ.length;dIx++) {
                    var classes = $(dimZ[dIx]).attr("class");
                    classes = classes.replace("matrixCB ","");
                    classes = classes.replace("dimZ","");
                    classes = classes.replace(/ /g,".");
                    CBs.filter(classes).each( function (i) { checkBoxSet(this,state); });
                }
            // Okay, that works for including dimZ when dimX&Y is clicked.  What about including dimX&Y when dimZ is clicked?
            } if(arguments.length == 2) { // Requested dimZ
                var dimsXY = $("input.matrixCB").not(".dimZ");
                dimsXY = dimsXY.filter(":checked");
                for(var dIx=0;dIx<dimsXY.length;dIx++) {
                    var classes = $(dimsXY[dIx]).attr("class");
                    classes = classes.replace("matrixCB","");
                    classes = classes.replace(/ /g,".");
                    CBs.filter(classes).each( function (i) { checkBoxSet(this,state); });
                }
            }
            return true;  // Notice if dimZ exists (regardless of checked state) then need to return
        }
    }
    // state uncheck or dimZ doesn't exist
    CBs.each( function (i) { checkBoxSet(this,state); });

    return true;
}

function setCheckBoxShadow(CB)
{
// Since CBs only get into cart when enabled/checked, the shadow control enables cart to know other states
    var shadowState = 0;
    if(CB.checked)
        shadowState = 1;
    if(CB.disabled)
        shadowState -= 2;
    $("input[name='boolshad."+CB.name+"']").val(shadowState);
}

function matEnableSubtrackCheckBoxes(state)
{
// Enables/Disables subtracks checkboxes.  If additional arguments are passed in, the list of CBs will be narrowed by the classes
    var CBs = $("input.subtrackCB");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        CBs = CBs.filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    //if(matCBwithViewDD) {
    //    if(state) { // further filter by view
    //        views = getViewNamesSelected(false); // get views (strings) that are off
    //        for(var vIx=0;vIx<views.length;vIx++) {
    //            CBs = CBs.filter(":not(."+views[vIx]+")");  // Successively limit list by additional classes.
    //        }
    //    }
    //}
    CBs.each( function (i) {
        this.disabled = !state;
        setCheckBoxShadow(this);
        hideOrShowSubtrack(this);
    });

    return true;
}

function matSubtrackCbClick(subCb)
{
// When a subrtrack checkbox is clicked, it may result in
// Clicking/unclicking the corresponding matrix CB.  Also the
// subtrack may be hidden as a result.
    //if(subCBtoMatCB)
    //    matChkBoxNormalizeMatching(subCb);
    setCheckBoxShadow(subCb);
    hideOrShowSubtrack(subCb);
}

function compositeCfgUpdateSubtrackCfgs(inp)
{
// Updates all subtrack configuration values when the composite cfg is changed
    var suffix = inp.name.substring(inp.name.indexOf("."));  // Includes '.'
    //if(suffix.length==0)
    //    suffix = inp.name.substring(inp.name.indexOf("_"));
    if(suffix.length==0) {
        alert("Unable to parse '"+inp.name+"'");
        return true;
    }
    if(inp.type.indexOf("select") == 0) {
        var list = $("select[name$='"+suffix+"']").not("[name='"+inp.name+"']"); // Exclude self from list
        if($(list).length>0) {
            if(inp.multiple != true)
                $(list).attr('selectedIndex',inp.selectedIndex);
            else {
                $(list).each(function() {  // for all dependent (subtrack) multi-selects
                    sel = this;
                    $(this).children('option').each(function() {  // for all options of dependent mult-selects
                        $(this).attr('selected',$(inp).children('option:eq('+this.index+')').attr('selected')); // set selected state to independent (parent) selected state
                    });
                    $(this).attr('size',$(inp).attr('size'));
                });
            }
        }
    }
    else if(inp.type.indexOf("checkbox") == 0) {
        var list = $("checkbox[name$='"+suffix+"']").not("[name='"+inp.name+"']"); // Exclude self from list
        if($(list).length>0)
            $(list).attr("checked",$(inp).attr("checked"));
    }
    else if(inp.type.indexOf("radio") == 0) {
        var list = $("radio[name$='"+suffix+"']").not("[name='"+inp.name+"']");
        if($(list).length>0)
            $(list).val(inp.value);
    }
    else {  // Various types of inputs
        var list = $("input[name$='"+suffix+"']").not("[name='"+inp.name+"']");//.not("[name^='boolshad.']"); // Exclude self from list
        if($(list).length>0)
            $(list).val(inp.value);
        //else {
        //    alert("Unsupported type of multi-level cfg setting type='"+inp.type+"'");
        //    return false;
        //}
    }
    return true;
}

function compositeCfgRegisterOnchangeAction(prefix)
{
// After composite level cfg settings written to HTML it is necessary to go back and
// make sure that each time they change, any matching subtrack level cfg setting are changed.
    var list = $("input[name^='"+prefix+".']").not("[name$='.vis']");
    $(list).change(function(){compositeCfgUpdateSubtrackCfgs(this);});

    var list = $("select[name^='"+prefix+".']").not("[name$='.vis']");
    $(list).change(function(){compositeCfgUpdateSubtrackCfgs(this);});
}


function subtrackCfgHideAll(table)
{
// hide all the subtrack configuration stuff
    $("div[id $= '_cfg']").each( function (i) {
        $( this ).css('display','none');
        $( this ).children("input[name$='.childShowCfg']").val("off");
    });
}

function subtrackCfgShow(tableName)
{
// Will show subtrack specific configuration controls
// Config controls not matching name will be hidden
    var divit = $("#div_"+tableName+"_cfg");
    if($(divit).css('display') == 'none')
        $("#div_"+tableName+"_meta").hide();
    // Could have all inputs commented out, then uncommented when clicked:
    // But would need to:
    // 1) be able to find composite view level input
    // 2) know if subtrack input is non-default (if so then subtrack value overrides composite view level value)
    // 3) know whether so composite view level value has changed since hgTrackUi displayed (if so composite view level value overrides)
    $(divit).toggle();
    return false;
}

function enableViewCfgLink(enable,view)
{
// Enables or disables a single configuration link.
    var link = $('#a_cfg_'+view);
    if(enable)
        $(link).attr('href','#'+$(link).attr('id'));
    else
        $(link).removeAttr('href');
}

function enableAllViewCfgLinks()
{
    $( ".viewDd").each( function (i) {
        var view = this.name.substring(this.name.indexOf(".") + 1,lastIndexOf(".vis"));
        enableViewCfgLink((this.selectedIndex > 0),view);
    });
}

function hideConfigControls(view)
{
// Will hide the configuration controls associated with one name
    $("input[name$='"+view+".showCfg']").val("off");      // Set cart variable
    $("tr[id^='tr_cfg_"+view+"']").css('display','none'); // Hide controls
}

function showConfigControls(name)
{
// Will show configuration controls for name= {tableName}.{view}
// Config controls not matching name will be hidden
    //if($( ".viewDd[name$='" + name + ".vis']").attr("selectedIndex") == 0) {
    //    $("input[name$='.showCfg']").val("off");
    //    $("tr[id^='tr_cfg_']").css('display','none');  // hide cfg controls when view is hide
    //    return true;
    //}
    var trs  = $("tr[id^='tr_cfg_']")
    $("input[name$='.showCfg']").val("off"); // Turn all off
    $( trs ).each( function (i) {
        if( this.id == 'tr_cfg_'+name && this.style.display == 'none') {
            $( this ).css('display','');
            $("input[name$='."+name+".showCfg']").val("on");
        }
        else if( this.style.display == '') {
            $( this ).css('display','none');
        }
    });

    $("table[id^='subtracks.']").each( function (i) { subtrackCfgHideAll(this);} ); // Close the cfg controls in the subtracks
    return true;
}

function trAlternateColors(table,cellIx)
{
// Will alternate colors each time the contents of the column(s) change
    var lastContent = "not";
    var bgColor1 = "#FFFEE8";
    var bgColor2 = "#FFF9D2";
    var curColor = bgColor1;
    var lastContent = "start";
    var cIxs = new Array();

    for(var aIx=1;aIx<arguments.length;aIx++) {   // multiple columns
        cIxs[aIx-1] = arguments[aIx];
    }
    if (document.getElementsByTagName)
    {
        for (var trIx=0;trIx<table.rows.length;trIx++) {
            var curContent = "";
            if(table.rows[trIx].style.display == 'none')
                continue;
            for(var ix=0;ix<cIxs.length;ix++) {
                if(table.rows[trIx].cells[cIxs[ix]]) {
                    curContent = (table.rows[trIx].cells[cIxs[ix]].abbr != "" ?
                                  table.rows[trIx].cells[cIxs[ix]].abbr       :
                                  table.rows[trIx].cells[cIxs[ix]].innerHTML  );
                }
            }
            if( lastContent != curContent ) {
                lastContent  = curContent;
                if( curColor == bgColor1)
                    curColor =  bgColor2;
                else
                    curColor =  bgColor1;
            }
            table.rows[trIx].bgColor = curColor;
        }
    } else if(debugLevel>2) {
        alert("trAlternateColors is unimplemented for this browser)");
    }
}

//////////// Sorting ////////////

function tableSort(table,fnCompare)
{
// Sorts table based upon rules passed in by function reference
    //alert("tableSort("+table.id+") is beginning.");
    subtrackCfgHideAll(table);
    var trs=0,moves=0;
    var colOrder = new Array();
    var cIx=0;
    var trTopIx,trCurIx,trBottomIx=table.rows.length - 1;
    for(trTopIx=0;trTopIx < trBottomIx;trTopIx++) {
        trs++;
        var topRow = table.rows[trTopIx];
        for(trCurIx = trTopIx + 1; trCurIx <= trBottomIx; trCurIx++) {
            var curRow = table.rows[trCurIx];
            var compared = fnCompare(topRow,curRow,arguments[2]);
            if(compared < 0) {
                table.insertBefore(table.removeChild(curRow), topRow);
                topRow = curRow; // New top!
                moves++;
            }
        }
    }
    if(fnCompare != trComparePriority)
        tableSetPositions(table);
    //alert("tableSort("+table.id+") examined "+trs+" rows and made "+moves+" moves.");
}

// Sorting a table by columns relies upon the sortColumns structure
// The sortColumns structure looks like:
//{
//    char *  tags[];     // a list of trackDb.subGroupN tags in sort order
//    boolean reverse[];  // the sort direction for that subGroup
//    int     cellIxs[];  // The indexes of the columns in the table to be sorted
//}
///// Following functions are for Sorting by columns:
function trCompareColumnAbbr(tr1,tr2,sortColumns)
{
// Compares a set of columns based upon the contents of their abbr
    for(var ix=0;ix < sortColumns.cellIxs.length;ix++) {
        //if(tr1.cells[sortColumns.cellIxs[ix]].abbr == undefined) {
        //    if(tr1.cells[sortColumns.cellIxs[ix]].value < tr2.cells[sortColumns.cellIxs[ix]].value)
        //        return (sortColumns.reverse[ix] ? -1: 1);
        //    else if(tr1.cells[sortColumns.cellIxs[ix]].value > tr2.cells[sortColumns.cellIxs[ix]].value)
        //        return (sortColumns.reverse[ix] ? 1: -1);
        //} else {
            if(tr1.cells[sortColumns.cellIxs[ix]].abbr < tr2.cells[sortColumns.cellIxs[ix]].abbr)
                return (sortColumns.reverse[ix] ? -1: 1);
            else if(tr1.cells[sortColumns.cellIxs[ix]].abbr > tr2.cells[sortColumns.cellIxs[ix]].abbr)
                return (sortColumns.reverse[ix] ? 1: -1);
        //}
    }
    return 0;
}


function tableSortByColumns(table,sortColumns)
{
// Will sort the table based on the abbr values on a et of <TH> colIds
    if (document.getElementsByTagName)
    {
        tableSort(table,trCompareColumnAbbr,sortColumns);//cellIxs,columns.colRev);
                        var columns = new sortColumnsGetFromTable(table);
        if(sortColumns.tags.length>1)
            trAlternateColors(table,sortColumns.cellIxs[sortColumns.tags.length-2]);

    } else if(debugLevel>2) {
        alert("tableSortByColumns is unimplemented for this browser)");
    }
}

function sortOrderFromColumns(sortColumns)
{// Creates the trackDB setting entry sortOrder subGroup1=+ ... from a sortColumns structure
    var sortOrder ="";
    for(ix=0;ix<sortColumns.tags.length;ix++) {
        sortOrder += sortColumns.tags[ix] + "=" + (sortColumns.reverse[ix] ? "-":"+") + " ";
    }
    if(sortOrder.length > 0)
        sortOrder = sortOrder.substr(0,sortOrder.length-1);
    return sortOrder;
}

function sortOrderFromTr(tr)
{// Looks up the sortOrder input value from a *.sortTr header row of a sortable table
    var inp = tr.getElementsByTagName('input');
    for(var ix=0;ix<inp.length;ix++) {
        var offset = inp[ix].id.lastIndexOf(".sortOrder");
        if(offset > 0 && offset == inp[ix].id.length - 10)
            return inp[ix].value;
    }
    return "";
}
function sortColumnsGetFromSortOrder(sortOrder)
{// Creates sortColumns struct (without cellIxs[]) from a trackDB.sortOrder setting string
    this.tags = new Array();
    this.reverse = new Array();
    var order = sortOrder;
    for(var ix=0;ix<12;ix++) {
        if(order.indexOf("=") <= 0)
            break;
        this.tags[ix]    = order.substring(0,order.indexOf("="));
        this.reverse[ix] = (order.substr(this.tags[ix].length+1,1) != '+');
        if(order.length < (this.tags[ix].length+2))
            break;
        order = order.substring(this.tags[ix].length+3);
    }
}
function sortColumnsGetFromTr(tr)
{// Creates a sortColumns struct from the entries in the '*.sortTr' heading row of a sortable table
    this.inheritFrom = sortColumnsGetFromSortOrder;
    var inp = tr.getElementsByTagName('input');
    var ix;
    for(ix=0;ix<inp.length;ix++) {
        var offset = inp[ix].id.lastIndexOf(".sortOrder");
        if(offset > 0 && offset == inp[ix].id.length - 10) {
            this.inheritFrom(inp[ix].value);
            break;
        }
    }
    if(ix == inp.length)
        return;

    // Add an additional array
    this.cellIxs = new Array();
    var cols = tr.getElementsByTagName('th');
    for(var tIx=0;tIx<this.tags.length;tIx++) {
        var colIdTag = this.tags[tIx] + ".sortTh";
        for(ix=0; ix<cols.length; ix++) {
            var offset = cols[ix].id.lastIndexOf(colIdTag);
            if(offset > 0 && offset == cols[ix].id.length - colIdTag.length) {
                this.cellIxs[tIx] = cols[ix].cellIndex;
            }
        }
    }
}
function sortColumnsGetFromTable(table)
{// Creates a sortColumns struct from the contents of a '*.sortable' table
    this.inheritNow = sortColumnsGetFromTr;
    var ix;
    for(ix=0;ix<table.rows.length;ix++) {
        var offset = table.rows[ix].id.lastIndexOf(".sortTr");
        if(offset > 0 && offset == table.rows[ix].id.length - 7) {
            this.inheritNow(table.rows[ix]);
            break;
        }
    }
}


function tableSortUsingSortColumns(table)
{// Sorts a table body based upon the marked columns
    var columns = new sortColumnsGetFromTable(table);
    tbody = table.getElementsByTagName("tbody")[0];
    tableSortByColumns(tbody,columns);
}

function hintOverSortableColumnHeader(th)
{// Upodates the sortColumns struct and sorts the table when a column headder has been pressed
    if(debugLevel>0) {
        var tr=th.parentNode;
        th.title = "Current Sort Order: " + sortOrderFromTr(tr);
        var sortColumns = new sortColumnsGetFromTr(tr);
    }
}

function tableSortAtButtonPress(anchor,tagId)
{// Updates the sortColumns struct and sorts the table when a column header has been pressed
 // If the current primary sort column is pressed, its direction is toggled then the table is sorted
 // If a secondary sort column is pressed, it is moved to the primary spot and sorted in fwd direction
    var th=anchor.parentNode;
    var sup=th.getElementsByTagName("sup")[0];
    var tr=th.parentNode;
    var inp = tr.getElementsByTagName('input');
    var iIx;
    for(iIx=0;iIx<inp.length;iIx++) {
        var offset = inp[iIx].id.lastIndexOf(".sortOrder");
        if(offset > 0 && offset == inp[iIx].id.length - 10)
            break;
    }
    var theOrder = new sortColumnsGetFromTr(tr);
    var oIx;
    for(oIx=0;oIx<theOrder.tags.length;oIx++) {
        if(theOrder.tags[oIx] == tagId)
            break;
    }
    if(oIx > 0) { // Need to reorder
        var newOrder = new sortColumnsGetFromTr(tr);
        var nIx=0;
        newOrder.tags[nIx] = theOrder.tags[oIx];
        newOrder.reverse[nIx] = false;  // When moving to the first position sort forward
        newOrder.cellIxs[nIx] = theOrder.cellIxs[ oIx];
        sups = tr.getElementsByTagName("sup");
        sups[newOrder.cellIxs[nIx]-1].innerHTML = "&darr;1";
        for(var ix=0;ix<theOrder.tags.length;ix++) {
            if(ix != oIx) {
                nIx++;
                newOrder.tags[nIx]    = theOrder.tags[ix];
                newOrder.reverse[nIx] = theOrder.reverse[ix];
                newOrder.cellIxs[nIx] = theOrder.cellIxs[ix];
                var dir = sups[newOrder.cellIxs[nIx]-1].innerHTML.substring(0,1);
                sups[newOrder.cellIxs[nIx]-1].innerHTML = dir + (nIx+1);
            }
        }
        theOrder = newOrder;
    } else { // need to reverse directions
        theOrder.reverse[oIx] = (theOrder.reverse[oIx] == false);
        var ord = sup.innerHTML.substring(1);
        sup.innerHTML = (theOrder.reverse[oIx] == false ? "&darr;":"&uarr;");
        if(theOrder.tags.length>1)
            sup.innerHTML += ord;
    }
    //alert("tableSortAtButtonPress(): count:"+theOrder.tags.length+" tag:"+theOrder.tags[0]+"="+(theOrder.reverse[0]?"-":"+"));
    var newSortOrder = sortOrderFromColumns(theOrder);
    inp[iIx].value = newSortOrder;
    var thead=tr.parentNode;
    var table=thead.parentNode;
    tbody = table.getElementsByTagName("tbody")[0];
    tableSortByColumns(tbody,theOrder);
    return;

}
function tableSortAtStartup()
{
    //alert("tableSortAtStartup() called");
    var list = document.getElementsByTagName('table');
    for(var ix=0;ix<list.length;ix++) {
        var offset = list[ix].id.lastIndexOf(".sortable");  // TODO: replace with class and jQuery
        if(offset > 0 && offset == list[ix].id.length - 9) {
            tableSortUsingSortColumns(list[ix]);
        }
    }
}


///// Following functions are for Sorting by priority
function tableSetPositions(table)
{
// Sets the value for the *.priority input element of a table row
// This gets called by sort or dradgndrop in order to allow the new order to affect hgTracks display
    if (table.getElementsByTagName)
    {
        for(var trIx=0;trIx<table.rows.length;trIx++) {
            if(table.rows[trIx].id.indexOf("tr_cb_") == 0) {
                var inp = table.rows[trIx].getElementsByTagName('input');
                for(var ix=0;ix<inp.length;ix++) {
                    var offset = inp[ix].name.lastIndexOf(".priority");
                    if( offset > 0 && offset == (inp[ix].name.length - 9)) {
                        inp[ix].value = table.rows[trIx].rowIndex;
                        break;
                    }
                }
            }
        }
    }
}
function trFindPosition(tr)
{
// returns the position (*.priority) of a sortable table row
    var inp = tr.getElementsByTagName('input');
    for(var ix=0;ix<inp.length;ix++) {
        var offset = inp[ix].name.indexOf(".priority");
        if(offset > 0 && offset == (inp[ix].name.length - 9)) {
            return inp[ix].value;
        }
    }
    return "unknown";
}

function hintForDraggableRow(tr)
{
    if(debugLevel>0) {
        tr.title='Position:'+trFindPosition(tr)
    }
}

function trComparePriority(tr1,tr2)
{
// Compare routine for sorting by *.priority
    var priority1 = 999999;
    var priority2 = 999999;
    var inp1 = tr1.getElementsByTagName('input');
    var inp2 = tr2.getElementsByTagName('input');
    for(var ix=0;ix<inp1.length;ix++) { // should be same length
        if(inp1[ix].name.indexOf(".priority") == (inp1[ix].name.length - 9))
            priority1 = inp1[ix].value;
        if(inp2[ix].name.indexOf(".priority") == (inp2[ix].name.length - 9))
            priority2 = inp2[ix].value;
        if(priority1 < 999999 && priority2 < 999999)
            break;
    }
    return priority2 - priority1;
}

///// Following functions support column reorganization
function trReOrderCells(tr,cellIxFrom,cellIxTo)
{
// Reorders cells in a table row: removes cell from one spot and inserts it before another
    //alert("tableSort("+table.id+") is beginning.");
    if(cellIxFrom == cellIxTo)
        return;

    var tdFrom = tr.cells[cellIxFrom];
    var tdTo   = tr.cells[cellIxTo];
    if((cellIxTo - cellIxFrom) == 1) {
        tdFrom = tr.cells[cellIxTo];
        tdTo   = tr.cells[cellIxFrom];
    } else if((cellIxTo - cellIxFrom) > 1)
        tdTo   = tr.cells[cellIxTo + 1];

    tr.insertBefore(tr.removeChild(tdFrom), tdTo);
}

function tableReOrderColumns(table,cellIxFrom,cellIxTo)
{
// Reorders cells in all the rows of a table row, thus reordering columns
    if (table.getElementsByTagName) {
        for(var ix=0;ix<table.rows.length;ix++) {
            var offset = table.rows[ix].id.lastIndexOf(".sortTr");
            if(offset > 0 && offset == table.rows[ix].id.length - 7) {
                trReOrderCells(table.rows[ix],cellIxFrom,cellIxTo);
                break;
            }
        }
        tbody = table.getElementsByTagName('tbody');
        for(var ix=0;ix<tbody[0].rows.length;ix++) {
            trReOrderCells(tbody[0].rows[ix],cellIxFrom,cellIxTo);
        }
    }
}

function matChkBoxNormalize(matCb)
{
    var classes =  $( matCb ).attr("class");
    var dimZ = (classes.indexOf("dimZ") > 0);
    classes = classes.replace("dimZ ","");
    classes = classes.replace("matrixCB "," ");
    classes = classes.replace(/ /g,".");
    var CBs = $("input.subtrackCB").filter(classes); // All subtrack CBs that match matrix CB
    if(arguments.length > 1) { // dimZ NOT classes
        CBs = $( CBs ).not(arguments[1]); // Removes the dimZ associated classes that are not checked
    }
    if(CBs.length > 0) {
        var CBsChecked = CBs.filter(":checked");
        if(CBsChecked.length == CBs.length)
            matCb.checked=true;
        else if(CBsChecked.length == 0)
            matCb.checked=false;
        else if(dimZ)
            matCb.checked=true; // Some are checked and this is dimZ.  This is a best guess
    }
}

function matChkBoxesNormalized()
{
// check/unchecks matrix checkboxes based upon subtrack checkboxes
    var matCBs = $("input.matrixCB");
    var dimZCBs = $("input.matrixCB[name$='_dimZ_cb']");
    if( $(dimZCBs).length > 0) {
        // Should do dimZ first, then go back and do non-dimZ with extra restrictions!
        var classes = "";
        $(dimZCBs).each( function (i) {
            matChkBoxNormalize(this);
            if( $(this).is(':checked') == false ) {
                classes += $( this ).attr("class").replace("matrixCB dimZ ",".")
            }
        } );
        $("input.matrixCB").not("[name$='_dimZ_cb']").each( function (i) { matChkBoxNormalize(this,classes); } );
    } else {
        $("input.matrixCB").each( function (i) { matChkBoxNormalize(this); } );
    }

    // For each viewDD not selected, disable associated subtracks
    $('select.viewDd').not("[selectedIndex]").each( function (i) {
        var viewClass = this.name.substring(this.name.indexOf(".") + 1,this.name.lastIndexOf("."));
        matEnableSubtrackCheckBoxes(false,viewClass);
    });
}

function showOrHideSelectedSubtracks(inp)
{
// Show or Hide subtracks based upon radio toggle
    var showHide;
    if(arguments.length > 0)
        showHide=inp;
    else {
        var onlySelected = $("input[name='displaySubtracks']");
        if(onlySelected.length > 0)
            showHide = onlySelected[0].checked;
        else
            return;
    }
    showSubTrackCheckBoxes(showHide);
    var list = $("table.tableSortable")
    for(var ix=0;ix<list.length;ix++) {
        var columns = new sortColumnsGetFromTable(list[ix]);
        var tbody = list[ix].getElementsByTagName('tbody');
        if(columns.tags.length>1) {
            if(columns.tags.length==2)
                trAlternateColors(tbody[0],columns.cellIxs[0]);
            else if(columns.tags.length==3)
                trAlternateColors(tbody[0],columns.cellIxs[0],columns.cellIxs[1]);
            else
                trAlternateColors(tbody[0],columns.cellIxs[0],columns.cellIxs[1],columns.cellIxs[2]);
        }
    }
}

///// Following functions called on page load
function matInitializeMatrix()
{
// Called at Onload to coordinate all subtracks with the matrix of check boxes
    if (document.getElementsByTagName) {
        matChkBoxesNormalized();  // Note that this needs to be done when the page is first displayed.  But ideally only on clean cart!
        showOrHideSelectedSubtracks();
        //enableAllViewCfgLinks();
    }
    else if(debugLevel>2) {
        alert("matInitializeMatrix is unimplemented for this browser)");
    }
   // alert("Time stamped ");
}

function multiSelectLoad(div,sizeWhenOpen)
{
    //var div = $(obj);//.parent("div.multiSelectContainer");
    var sel = $(div).children("select:first");
    if(div != undefined && sel != undefined && sizeWhenOpen <= $(sel).length) {
        $(div).css('width', ( $(sel).clientWidth ) +"px");
        $(div).css('overflow',"hidden");
        $(div).css('borderRight',"2px inset");
    }
    $(sel).show();
}

function multiSelectBlur(obj)
{
    if($(obj).val() == undefined || $(obj).val() == "") {
        $(obj).val("All");
        $(obj).attr('selectedIndex',0);
    }
    //if(obj.value == "All") // Close if selected index is 1
    if($(obj).attr('selectedIndex') == 0) // Close if selected index is 1
        $(obj).attr('size',1);
    /*else if($.browser.msie == false && $(obj).children('option[selected]').length==1) {
        var ix;
        for(ix=0;ix<obj.options.length;ix++) {
            if(obj.options[ix].value == obj.value) {
                //obj.options[ix].selected = true;
                obj.selectedIndex = ix;
                obj.size=1;
                $(obj).trigger('change');
                break;
            }
        }
    }*/
}

function multiSelectClick(obj,sizeWhenOpen)
{
    if($(obj).attr('size') == 1)
        $(obj).attr('size',sizeWhenOpen);
    else if($(obj).attr('selectedIndex') == 0)
        $(obj).attr('size',1);
}

// The following js depends upon the jQuery library
$(document).ready(function()
{
    //matInitializeMatrix();
    //$("div.multiSelectContainer").each( function (i) {
    //    var sel = $(this).children("select:first");
    //    multiSelectLoad(this,sel.openSize);
    //});

    // Allows rows to have their positions updated after a drag event
    if($(".tableWithDragAndDrop").length > 0) {
        $(".tableWithDragAndDrop").tableDnD({
            onDragClass: "trDrag",
            onDrop: function(table, row) {
                    if(tableSetPositions) {
                        tableSetPositions(table);
                    }
                }
            });
        $(".trDraggable").hover(
            function(){if($(this).hasClass('trDrag') == false) $(this).addClass('pale');},
            function(){$(this).removeClass('pale');}
        );
    }
});
