// JavaScript Especially for hui.c

var debugLevel = 0;

// The 'mat*' functions are especially designed to support subtrack configuration by 2D matrix of controls

function matSelectViewForSubTracks(obj,view)
{
// Handle any necessary changes to subtrack checkboxes when the view changes
// views are "select" drop downs on a subtrack configuration page
    if( obj.selectedIndex == 0) { // hide
        setCheckBoxesThatContain('id',false,true,view); // No need for matrix version
        //matSetCheckBoxesThatContain('id',false,true,view);  // Use matrix version to turn off buttons for other views that are "hide"
    } else {
        // Make main display dropdown show full if currently hide
        var trackName = obj.name.substring(0,obj.name.indexOf("_dd_"))
        var displayDD = document.getElementsByName(trackName);
        if(displayDD.length >= 1 && displayDD[0].selectedIndex == 0)
            displayDD[0].selectedIndex = displayDD[0].options.length - 1;
        // if matrix used then: essentially reclick all 'checked' matrix checkboxes (run onclick script)
        var list = inputArrayThatMatches("checkbox","name","mat_","_cb");
        for (var ix=0;ix<list.length;ix++) {
            var ele = list[ix];
            if(list[ix].checked) {
                clickIt(list[ix],true,true); // force a double click() which will invoke cb specific js;
            }
        }
        if(list.length == 0) {
            setCheckBoxesThatContain('id',true,true,view); // No need for matrix version
        }
    }
}

function matSetCheckBoxesThatContain(nameOrId, state, force, sub1)
{
// Set all checkboxes which contain 1 or more given substrings in NAME or ID to state boolean
// Unlike the std setCheckBoxesThatContain() this also recognizes whether views
// additionally control which subtracks will be checked
    if(debugLevel>2)
        alert("matSetCheckBoxesThatContain is about to set the checkBoxes to "+state);

    var views = getViewsSelected("_dd_",true); // get views that are on

    var list;
    if(arguments.length == 4)
        list = inputArrayThatMatches("checkbox",nameOrId,"","",sub1);
    else if(arguments.length == 5)
        list = inputArrayThatMatches("checkbox",nameOrId,"","",sub1,arguments[4]);
    else if(arguments.length == 6)
        list = inputArrayThatMatches("checkbox",nameOrId,"","",sub1,arguments[4],arguments[5]);
    for (var ix=0;ix<list.length;ix++) {
        var identifier = list[ix].name;
        if(nameOrId.search(/id/i) != -1)
            identifier = list[ix].id;
        if(debugLevel>3)
            alert("matSetCheckBoxesThatContain found '"+sub1+"' in '"+identifier+"'.");

        if(!state) {
            clickIt(list[ix],state,force);
        } else {
            if(views.length == 0) {
                clickIt(list[ix],state,force);
            } else {
                for(var vIx=0;vIx<views.length;vIx++) {
                    if(identifier.indexOf("_"+views[vIx]+"_") >= 0) {
                        clickIt(list[ix],state,force);
                        break;
                    }
                }
            }
        }
    }
    return true;
}

function matSubtrackCbClick(subCb)
{
// When a matrix subrtrack checkbox is clicked, it may result in
// Clicking/unclicking the corresponding matrix CB.  Also the
// subtrack may be hidden as a result.
    matChkBoxNormalizeMatching(subCb);
    hideOrShowSubtrack(subCb);
}

function compositeCfgUpdateSubtrackCfgs(inp)
{
// Updates all subtrack configuration values when the composite cfg is changed
    var count=0;
    var suffix = inp.name.substring(inp.name.indexOf("."));
    var list = inputArrayThatMatches(inp.type,"name","",suffix);
    for (var ix=0;ix<list.length;ix++) {
        list[ix].value = inp.value;
        count++;
    }
    if(list.length==0) {
        var list = document.getElementsByTagName('select');
        for (var ix=0;ix<list.length;ix++) {
            if(list[ix].name.lastIndexOf(suffix) == list[ix].name.length - suffix.length ) {
                list[ix].value = inp.value;
                count++;
            }
        }
    }
    //alert("compositeCfgUpdateSubtrackCfgs("+suffix+") updated "+count+" inputs.")
}

function compositeCfgRegisterOnchangeAction(prefix)
{
// After composite level cfg settings written to HTML it is necessary to go back and
// make sure that each time they change, any matching subtrack level cfg setting are changed.
    var count=0;
    var list = inputArrayThatMatches("","name",prefix,"");
    for (var ix=0;ix<list.length;ix++) {
        list[ix].onchange = function(){compositeCfgUpdateSubtrackCfgs(this);};
        count++;
    }
    var list = document.getElementsByTagName('select');
    for (var ix=0;ix<list.length;ix++) {
        if(list[ix].name.indexOf(prefix) == 0) {
            list[ix].onchange = function(){compositeCfgUpdateSubtrackCfgs(this);};
            count++;
        }
    }
    //alert("compositeCfgRegisterOnchangeAction("+prefix+") updated "+count+" inputs and selects.")
}


function subtrackCfgHideAll(table)
{
// hide all the subtrack configuration stuff
    var div = table.getElementsByTagName("div");
    for (var ix=0;ix<div.length;ix++) {
        if (div[ix].id.lastIndexOf(".cfg") == div[ix].id.length - 4)
        div[ix].style.display = 'none';
    }
}

function subtrackCfgShow(anchor)
{
// Will show subtrack specific configuration controls
// Config controls not matching name will be hidden
    var td=anchor.parentNode;
    var tr=td.parentNode;
    var tbody=tr.parentNode;
    var div = tr.getElementsByTagName("div");
    if (div!=undefined && div[0].id.lastIndexOf(".cfg") == div[0].id.length - 4) {
        if (div[0].style.display == 'none') {
            subtrackCfgHideAll(tbody);
            div[0].style.display = '';
        } else
            div[0].style.display = 'none';
    }
    return true;
}

function showConfigControls(name)
{
// Will show configuration controls
// Config controls not matching name will be hidden
    var retval = false;
    if (document.getElementsByTagName)
    {
        var list = document.getElementsByTagName('tr');
        for (var ix=0;ix<list.length;ix++) {
            var tblRow = list[ix];
            if(tblRow.id.indexOf("tr_cfg_") == 0) {  // marked as tr containing a cfg's
                if(tblRow.id.lastIndexOf(name) == 7 && tblRow.id.length == name.length + 7 && tblRow.style.display == 'none') {
                    tblRow.style.display = '';
                } else {
                    tblRow.style.display = 'none';
                }
            }
        }
        retval = true;
        var list = document.getElementsByTagName('table');
        for (var ix=0;ix<list.length;ix++) {
            if(list[ix].id.indexOf("subtracks.") == 0)
                subtrackCfgHideAll(list[ix]);
        }
    }
    else if (document.all) {
        if(debugLevel>2)
            alert("showConfigControls is unimplemented for this browser");
    } else {
        // NS 4.x - I gave up trying to get this to work.
        if(debugLevel>2)
           alert("showConfigControls is unimplemented for this browser");
    }
    return retval;
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
    //alert("trCompareColumnAbbr("+tr1.id+","+tr2.id+"): "+sortColumns.tags[0]+"="+(sortColumns.reverse[0]?"-":"+"));
    for(var ix=0;ix < sortColumns.cellIxs.length;ix++) {
        if(tr1.cells[sortColumns.cellIxs[ix]].abbr < tr2.cells[sortColumns.cellIxs[ix]].abbr)
            return (sortColumns.reverse[ix] ? -1: 1);
        else if(tr1.cells[sortColumns.cellIxs[ix]].abbr > tr2.cells[sortColumns.cellIxs[ix]].abbr)
            return (sortColumns.reverse[ix] ? 1: -1);
    }
    return 0;
}


function tableSortByColumns(table,sortColumns)
{
// Will sort the table based on the abbr values on a et of <TH> colIds
    if (document.getElementsByTagName)
    {
        //alert("tableSortByColumns(): "+sortColumns.tags[0]+"="+(sortColumns.reverse[0]?"-":"+"));
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
{// Upodates the sortColumns struct and sorts the table when a column header has been pressed
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
// check/unchecks a single matrix checkbox based upon subtrack checkboxes
    var cntChecked=0;
    var tags=matCb.name.substring(3,matCb.name.length - 3);
    if(tags.substring(tags.length - 5,tags.length) == "_dimZ")
        tags = tags.substring(0,tags.length - 6);
    var sublist = inputArrayThatMatches("checkbox","id","cb_","",tags);
    for (var ix=0;ix<sublist.length;ix++) {
        if(sublist[ix].checked)
            cntChecked++;
    }
    if(cntChecked == sublist.length)
        matCb.checked=true;
    else if(cntChecked == 0)
        matCb.checked=false;
}

function matChkBoxNormalizeMatching(subCb)
{
// check/unchecks a matrix checkbox based upon subtrack checkboxes
// the matrix cb is the one that matches the subtrack cb provided
    // cb_tableName_dimX_dimY_view_cb need: _dimX_dimY_
    var tags = subCb.id.split('_');
    if(tags.length < 4)
        return;
    tags[0] = "_" + tags[2] + "_";
    if(tags.length > 4)
        tags[0] += tags[3] + "_"; // Assume 2 dimensions first (remember tags[3] could be unwanted view
    //alert("matChkBoxNormalizeOne() id:"+subCb.id+" tags:"+tags[0]);
    var list = inputArrayThatMatches("checkbox","name","mat_","_cb",tags[0]);
    if(list.length==0) {
        tags[0] = "_" + tags[2] + "_";
        list = inputArrayThatMatches("checkbox","name","mat_","_cb",tags[0]);
    }
    // There should be only one!
    for (var ix=0;ix<list.length;ix++) {
        matChkBoxNormalize(list[ix]);
    }
    // What about the Z dimension?
    if(tags.length > 5) {
        tags[0] = "_" + tags[4] + "_"; // Z dimension should be 3rd tag and has a separate 1D matrix of chkbxs
        var list = inputArrayThatMatches("checkbox","name","mat_","_cb",tags[0]);
        if(list.length>0) {
            // There should be only one!
            for (var ix=0;ix<list.length;ix++) {
                matChkBoxNormalize(list[ix]);
            }
        }
    }
}

function matChkBoxesNormalized()
{
// check/unchecks matrix checkboxes based upon subtrack checkboxes
    var list = inputArrayThatMatches("checkbox","name","mat_","_cb");
    for (var ix=0;ix<list.length;ix++) {
        matChkBoxNormalize(list[ix]);
    }
}

function showOrHideSelectedSubtracks(inp)
{
// Show or Hide subtracks based upon radio toggle
    var showHide;
    if(arguments.length > 0)
        showHide=inp;
    else {
        var onlySelected = inputArrayThatMatches("radio","name","displaySubtracks","");
        if(onlySelected.length > 0)
            showHide = onlySelected[0].checked;
        else
            return;
    }
    showSubTrackCheckBoxes(showHide);
    var list = document.getElementsByTagName('table');
    for(var ix=0;ix<list.length;ix++) {
        if(list[ix].id.lastIndexOf(".sortable") == list[ix].id.length - 9) {
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
}

///// Following functions called on page load
function matInitializeMatrix()
{
// Called at Onload to coordinate all subtracks with the matrix of check boxes
    if (document.getElementsByTagName) {
        matChkBoxesNormalized();
        showOrHideSelectedSubtracks();
    }
    else if(debugLevel>2) {
        alert("matInitializeMatrix is unimplemented for this browser)");
    }
}

// The following js depends upon the jQuery library
$(document).ready(function()
{
    //matInitializeMatrix();

    // Allows rows to have their positions updated after a drag event
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
});
