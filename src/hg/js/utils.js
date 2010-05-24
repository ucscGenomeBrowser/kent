// Utility JavaScript
// $Header: /projects/compbio/cvsroot/kent/src/hg/js/utils.js,v 1.30 2010/05/24 20:42:20 tdreszer Exp $

var debug = false;

function clickIt(obj,state,force)
{
// calls click() for an object, and click();click() if force
    if(obj.checked != state) {
        obj.click();
    } else if (force) {
        obj.click();
        obj.click();    //force onclick event
    }
}
function setCheckBoxesWithPrefix(obj, prefix, state)
{
// Set all checkboxes with given prefix to state boolean
    var list = inputArrayThatMatches("checkbox","id",prefix,"");
    for (var i=0;i<list.length;i++) {
        var ele = list[i];
            if(ele.checked != state)
                ele.click();  // Forces onclick() javascript to run
    }
}

function setCheckBoxesThatContain(nameOrId, state, force, sub1)
{
// Set all checkboxes which contain 1 or more given substrings in NAME or ID to state boolean
// First substring: must begin with it; 2 subs: beg and end; 3: begin, middle and end.
// This can force the 'onclick() js of the checkbox, even if it is already in the state
    if(debug)
        alert("setCheckBoxesContains is about to set the checkBoxes to "+state);
    var list;
    if(arguments.length == 4)
        list = inputArrayThatMatches("checkbox",nameOrId,sub1,"");
    else if(arguments.length == 5)
        list = inputArrayThatMatches("checkbox",nameOrId,sub1,arguments[4]);
    else if(arguments.length == 6)
        list = inputArrayThatMatches("checkbox",nameOrId,sub1,arguments[4],arguments[5]);
    for (var ix=0;ix<list.length;ix++) {
        clickIt(list[ix],state,force);
    }
    return true;
}

function inputArrayThatMatches(inpType,nameOrId,prefix,suffix)
{
    // returns an array of input controls that match the criteria
    var found = new Array();
    var fIx = 0;
    if (document.getElementsByTagName)
    {
        var list;
        if(inpType == 'select')
            list = document.getElementsByTagName('select');
        else
            list = document.getElementsByTagName('input');
        for (var ix=0;ix<list.length;ix++) {
            var ele = list[ix];
            if(inpType.length > 0 && inpType != 'select' && ele.type != inpType)
                continue;
            var identifier = ele.name;
            if(nameOrId.search(/id/i) != -1)
                identifier = ele.id;
            var failed = false;
            if(prefix.length > 0)
                failed = (identifier.indexOf(prefix) != 0)
            if(!failed && suffix.length > 0)
                failed = (identifier.lastIndexOf(suffix) != (identifier.length - suffix.length))
            if(!failed) {
                for(var aIx=4;aIx<arguments.length;aIx++) {
                    if(identifier.indexOf(arguments[aIx]) == -1) {
                        failed = true;
                        break;
                    }
                }
            }
            if(!failed) {
                found[fIx] = ele;
                fIx++;
            }
        }
    } else {
        // NS 4.x - I gave up trying to get this to work.
        if(debugLevel>2)
           alert("arrayOfInputsThatMatch is unimplemented for this browser");
    }
    return found;
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
                    if(chkBox!=undefined && chkBox.checked && chkBox.disabled == false)
                        tblRow.style.display = '';
                    else
                        tblRow.style.display = 'none';  // hides
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

    if(!obj.checked || obj.disabled)
    {
        var list = document.getElementsByName("displaySubtracks");
        for (var ix=0;ix<list.length;ix++) {
            if(list[ix].value == "selected") {
                if(list[ix].checked)
                    tblRow.style.display = 'none';  // hides
                else
                    tblRow.style.display = ''; //'table-row' doesn't work in some browsers (ie: IE)
                break;
            }
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

function makeHiddenInput(theForm,aName,aValue)
{   // Create a hidden input to hold a value
    $(theForm).find("input:last").after("<input type=hidden name='"+aName+"' value='"+aValue+"'>");
}

function updateOrMakeNamedVariable(theForm,aName,aValue)
{   // Store a value to a named input.  Will make the input if necessary
    var inp = $(theForm).find("input[name='"+aName+"']:last");
    if(inp != undefined && inp.length > 0)
        inp.val(aValue);
    else
        makeHiddenInput(theForm,aName,aValue);
}

function parseUrlAndUpdateVars(theForm,href)
{   // Parses the URL and converts GET vals to POST vals
    var url = href;
    var extraIx = url.indexOf("?");
    if(extraIx > 0) {
        var extra = url.substring(extraIx+1);
        url = url.substring(0,extraIx);
        // now extra must be repeatedly broken into name=var
        extraIx = extra.indexOf("=");
        for(;extraIx > 0;extraIx = extra.indexOf("=")) {
            var aValue;
            var aName = extra.substring(0,extraIx);
            var endIx = extra.indexOf("&");
            if( endIx>0) {
                aValue = extra.substring(extraIx+1,endIx);
                extra  = extra.substring(endIx+1);
            } else {
                aValue = extra.substring(extraIx+1);
                extra  = "";
            }
            if(aName.length>0 && aValue.length>0)
                updateOrMakeNamedVariable(theForm,aName,aValue);
        }
    }
    return url;
}

function postTheForm(formName,href)
{   // posts the form with a passed in href
    var goodForm=$("form[name='"+formName+"']");
    if(goodForm.length == 1) {
        if(href != undefined && href.length > 0) {
            $(goodForm).attr('action',href); // just attach the straight href
        }
        $(goodForm).attr('method','POST');

        $(goodForm).submit();
    }
    return false; // Meaning do not continue with anything else
}
function setVarAndPostForm(aName,aValue,formName)
{   // Sets a specific variable then posts
    var goodForm=$("form[name='"+formName+"']");
    if(goodForm.length == 1) {
        updateOrMakeNamedVariable(goodForm,aName,aValue);
    }
    return postTheForm(formName,window.location.href);
}

function aryFind(ary,val)
{// returns the index of a value on the array or -1;
    for(var ix=0;ix<ary.length;ix++) {
        if(ary[ix] == val) {
            return ix;
        }
    }
    return -1;
}

function aryRemoveVals(ary,vals)
{ // removes one or more variables that are found in the array
    for(var vIx=0;vIx<vals.length;vIx++) {
        var ix = aryFind(ary,vals[vIx]);
        if(ix != -1)
            ary.splice(ix,1);
    }
    return ary;
}

function aryRemove(ary,val)
{ // removes one or more variables that are found in the array
    for(var vIx=1;vIx<arguments.length;vIx++) {
        var ix = aryFind(ary,arguments[vIx]);
        if(ix != -1)
            ary.splice(ix,1);
    }
    return ary;
}

function isInteger(s)
{
    return (!isNaN(parseInt(s)) && isFinite(s) && s.toString().indexOf('.') < 0);
}
function isFloat(s)
{
    return (!isNaN(parseFloat(s)) && isFinite(s));
}

function validateInt(obj,min,max)
{   // validates an integer which may be restricted to a range (if min and/or max are numbers)
    var title = obj.title;
    var rangeMin=parseInt(min);
    var rangeMax=parseInt(max);
    if(title.length == 0)
        title = "Value";
    var popup=( $.browser.msie == false );
    for(;;) {
        if((obj.value == undefined || obj.value == "") && isInteger(obj.defaultValue))
            obj.value = obj.defaultValue;
        if(!isInteger(obj.value)) {
            if(popup) {
                obj.value = prompt(title +" is invalid.\nMust be an integer.",obj.value);
                continue;
            } else {
                alert(title +" of '"+obj.value +"' is invalid.\nMust be an integer."); // try a prompt box!
                obj.value = obj.defaultValue;
                return false;
            }
        }
        var val = parseInt(obj.value);
        if(isInteger(min) && isInteger(max)) {
            if(val < rangeMin || val > rangeMax) {
                if(popup) {
                    obj.value = prompt(title +" is invalid.\nMust be between "+rangeMin+" and "+rangeMax+".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be between "+rangeMin+" and "+rangeMax+".");
                    obj.value = obj.defaultValue;
                    return false;
                }
            }
        } else if(isInteger(min)) {
            if(val < rangeMin) {
                if(popup) {
                    obj.value = prompt(title +" is invalid.\nMust be no less than "+rangeMin+".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be no less than "+rangeMin+".");
                    obj.value = obj.defaultValue;
                    return false;
                }
            }
        } else if(isInteger(max)) {
            if(val > rangeMax) {
                if(popup) {
                    obj.value = prompt(title +" is invalid.\nMust be no greater than "+rangeMax+".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be no greater than "+rangeMax+".");
                    obj.value = obj.defaultValue;
                    return false;
                }
            }
        }
        return true;
    }
}

function validateFloat(obj,min,max)
{   // validates an float which may be restricted to a range (if min and/or max are numbers)
    var title = obj.title;
    var rangeMin=parseFloat(min);
    var rangeMax=parseFloat(max);
    if(title.length == 0)
        title = "Value";
    var popup=( $.browser.msie == false );
    for(;;) {
        if((obj.value == undefined || obj.value == "") && isFloat(obj.defaultValue))
            obj.value = obj.defaultValue;
        if(!isFloat(obj.value)) {
            if(popup) {
                obj.value = prompt(title +" is invalid.\nMust be a number.",obj.value);
                continue;
            } else {
                alert(title +" of '"+obj.value +"' is invalid.\nMust be a number."); // try a prompt box!
                obj.value = obj.defaultValue;
                return false;
            }
        }
        var val = parseFloat(obj.value);
        if(isFloat(min) && isFloat(max)) {
            if(val < rangeMin || val > rangeMax) {
                if(popup) {
                    obj.value = prompt(title +" is invalid.\nMust be between "+rangeMin+" and "+rangeMax+".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be between "+rangeMin+" and "+rangeMax+".");
                    obj.value = obj.defaultValue;
                    return false;
                }
            }
        } else if(isFloat(min)) {
            if(val < rangeMin) {
                if(popup) {
                    obj.value = prompt(title +" is invalid.\nMust be no less than "+rangeMin+".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be no less than "+rangeMin+".");
                    obj.value = obj.defaultValue;
                    return false;
                }
            }
        } else if(isFloat(max)) {
            if(val > rangeMax) {
                if(popup) {
                    obj.value = prompt(title +" is invalid.\nMust be no greater than "+rangeMax+".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be no greater than "+rangeMax+".");
                    obj.value = obj.defaultValue;
                    return false;
                }
            }
        }
        return true;
    }
}

function metadataShowHide(tableName)
{
// Will show subtrack specific configuration controls
// Config controls not matching name will be hidden
    var divit = $("#div_"+tableName+"_meta");
    if($(divit).css('display') == 'none')
        $("#div_"+tableName+"_cfg").hide();  // Hide any configuration when opening metadata
    var htm = $(divit).html();
    // Seems to be faster if this undisplayed junk is commented out.
    if(htm.substring(0,4) == "<!--") {
        htm = htm.substring(4,htm.length-7);
        $(divit).html(htm);
    }
    $(divit).toggle();  // jQuery hide/show
    return false;
}

function warn(msg)
{ // adds warnings to the warnBox
    var warnList = $('#warnList'); // warnBox contains warnList
    if( $(warnList).length == 0 )
        alert(msg);
    else {
        $( warnList ).append('<li>'+msg+'</li>');
        showWarnBox();
    }
}

function startTiming()
{
    var now = new Date();
    return now.getTime();
}

function showTiming(start,whatTookSoLong)
{
    var now = new Date();
    var end = (now.getTime() - start);
    warn(whatTookSoLong+" took "+end+" msecs.");
    return end;
}

function getHgsid()
{
// return current session id
    var hgsid;
    var list = document.getElementsByName("hgsid");
    if(list.length) {
        var ele = list[0];
        hgsid = ele.value;
    }
    if(!hgsid) {
        hgsid = getURLParam(window.location.href, "hgsid");
    }
    return hgsid;
}

function Rectangle()
{
// Rectangle object constructor:
// calling syntax:
//
// new Rectangle(startX, endX, startY, endY)
// new Rectangle(coords) <-- coordinate string from an area item
    if(arguments.length == 4) {
        this.startX = arguments[0];
        this.endX = arguments[1];
        this.startY = arguments[2];
        this.endY = arguments[3];
    } else {
        var coords = arguments[0].split(",");
        this.startX = coords[0];
        this.endX = coords[2];
        this.startY = coords[1];
        this.endY = coords[3];
    }
}

Rectangle.prototype.contains = function(x, y)
{
// returns true if given points are in the rectangle
    var retval = x >= this.startX && x <= this.endX && y >= this.startY && y <= this.endY;
    return retval;
}

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

function getSizeFromCoordinates(position)
{
// Parse size out of a chr:start-end string
    var a = /(\d+)-(\d+)/.exec(position);
    if(a && a[1] && a[2]) {
        return a[2] - a[1] + 1;
    }
    return null;
}
