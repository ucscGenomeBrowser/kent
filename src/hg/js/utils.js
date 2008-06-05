// Utility JavaScript

var debug = false;

function setCheckBoxesWithPrefix(obj, prefix, state)
{
// Set all checkboxes with given prefix to state boolean
   if (document.getElementsByTagName)
   {
        // Tested in FireFox 1.5 & 2, IE 6 & 7, and Safari
        var list = document.getElementsByTagName('input');
        var first = true;
        for (var i=0;i<list.length;i++, first=false) {
            var ele = list[i];
            if(ele.type.match("checkbox") == null)
                 continue;
            // if(first) alert(ele.checked);
            // arbitrary numbers are used to make id's unique (e.g. "map-1"), so look for prefix
            if(ele.id.indexOf(prefix) == 0) {
                if(ele.checked != state)
                    ele.click();  // Forces onclick() javascript to run 
            }
            first = false;
        }
   } else if (document.all) {
        if(debug)
            alert("setCheckBoxesWithPrefix is unimplemented for this browser");
   } else {
        // NS 4.x - I gave up trying to get this to work.
        if(debug)
           alert("setCheckBoxesWithPrefix is unimplemented for this browser");
   }
}

function setCheckBoxesThatContain(nameOrId, state, force, sub1)
{
// Set all checkboxes which contain 1 or more given substrings in NAME or ID to state boolean
// This can force the 'onclick() js of the checkbox, even if it is already in the state 
    var retval = false;
    if (document.getElementsByTagName)
    {
        if(debug)
            alert("setCheckBoxesContains is about to set the checkBoxes to "+state);
        var list = document.getElementsByTagName('input');
        for (var ix=0;ix<list.length;ix++) {
            var ele = list[ix];
            var identifier = ele.name;
            if(ele.type.match("checkbox") == null)
                 continue;
            if(nameOrId.search(/id/i) != -1)
                identifier = ele.id;
            var failed = false;
            for(var aIx=3;aIx<arguments.length;aIx++) {
                if(identifier.indexOf(arguments[aIx]) == -1) { 
                    failed = true;
                    break;
                }
            }
            if(failed)
                continue;
            if(debug)
                alert("setCheckBoxesContains found '"+sub1+"' in '"+identifier+"'.");

            if(ele.checked != state) {
                    ele.click();
            } else if (force) {
                ele.click();    
                ele.click();    //force onclick event
            }
        }
        retval = true;
    } else if (document.all) {
        if(debug)
            alert("setCheckBoxesContains is unimplemented for this browser");
    } else {
        // NS 4.x - I gave up trying to get this to work.
        if(debug)
           alert("setCheckBoxesContains is unimplemented for this browser");
    }
    return retval;
}

function getViewsSelected(nameMatches,on)
{
// Returns an array of all views that are on or off (hide)
// views are "select" drop downs containing 'hide','dense',...
   var views = new Array();
   
   if (document.getElementsByTagName) {
        var list = document.getElementsByTagName('select');
        var vIx=0;
        for (var ix=0;ix<list.length;ix++) {
            var ele = list[ix];
            if(ele.name.indexOf(nameMatches) == 0) {
                if((on && ele.selectedIndex>0) || (!on && ele.selectedIndex==0)) {
                    views[vIx] = ele.name.substring(3,ele.name.length); 
                    vIx++;
                }
            }
        }
        if(debug)
            alert("getViewsSelected() returns "+views);
   }
   return( views );
}

function showSubTrackCheckBoxes(onlySelected)
{
// If a Subtrack configuration page has show "only selected subtracks" option,
// This can show/hide tablerows that contain the checkboxes
// Containing <tr>'s must be id'd with 'tr_' + the checkbox id,
// while checkbox id must have 'cb_' prefix (ie: 'tr_cb_checkThis' & 'cb_checkThis')
   if (document.getElementsByTagName)
   {
        var list = document.getElementsByTagName('tr');
        for (var ix=0;ix<list.length;ix++) {
            var tblRow = list[ix];
            if(tblRow.id.indexOf("tr_cb_") >= 0) {  // marked as tr containing a cb
                if(!onlySelected) {
                    tblRow.style.display = ''; //'table-row' doesn't work in some browsers (ie: IE)
                } else {
                    var associated_cb = tblRow.id.substring(3,tblRow.id.length);
                    chkBox = document.getElementById(associated_cb);
                    if(chkBox.checked)
                        tblRow.style.display = ''; 
                    else
                        tblRow.style.display = 'none';
                }
            }
        }
   }
   else if (document.all) {
        if(debug)
            alert("showSubTrackCheckBoxes is unimplemented for this browser");
   } else {
        // NS 4.x - I gave up trying to get this to work.
        if(debug)
           alert("showSubTrackCheckBoxes is unimplemented for this browser");
   }
}
        
function hideOrShowSubtrack(obj)
{
// This can show/hide a tablerow that contains a specific object
// Containing <tr>'s must be id'd with 'tr_' + obj.id
// Also, this relies upon the "displaySubtracks" radio button control
    var tblRow = document.getElementById("tr_"+obj.id);

    if(!obj.checked)
    {
        var list = document.getElementsByName("displaySubtracks");
        for (var ix=0;ix<list.length;ix++) {
            if(list[ix].value.match("selected") == null)
                continue;
            if(list[ix].checked)
                tblRow.style.display = 'none';
            else
                tblRow.style.display = ''; //'table-row' doesn't work in some browsers (ie: IE)
            break;
        }
    }
    else
        tblRow.style.display = ''; 
}

function waitCursor(obj)
{
    //document.body.style.cursor="wait"
    obj.style.cursor="wait";
}

function endWaitCursor(obj)
{
    obj.style.cursor="";
}

function getURLParam()
{
// Retrieve variable value from an url.
// Can be called either:
//     getURLParam(url, name)
// or:
//     getURLParam(name)
// Second interface will default to using window.location.href
    var strHref, strParamName;
    var strReturn = "";
    if(arguments.length == 1) {
          strHref = window.location.href;
          strParamName = arguments[0];
    } else {
          strHref = arguments[0];
          strParamName = arguments[1];
    }
    if ( strHref.indexOf("?") > -1 ){
      var strQueryString = strHref.substr(strHref.indexOf("?")).toLowerCase();
      var aQueryString = strQueryString.split("&");
      for ( var iParam = 0; iParam < aQueryString.length; iParam++ ){
         if (aQueryString[iParam].indexOf(strParamName.toLowerCase() + "=") > -1 ){
            var aParam = aQueryString[iParam].split("=");
            strReturn = aParam[1];
            break;
         }
      }
    }
    return unescape(strReturn);
}
