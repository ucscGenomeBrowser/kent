// Utility JavaScript
// $Header: /projects/compbio/cvsroot/kent/src/hg/js/utils.js,v 1.31 2010/06/02 19:11:53 tdreszer Exp $

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
    if(inp != undefined && inp.length > 0) {
        inp.val(aValue);
        inp.disabled = false;
    } else
        makeHiddenInput(theForm,aName,aValue);
}

function disableNamedVariable(theForm,aName)
{   // Store a value to a named input.  Will make the input if necessary
    var inp = $(theForm).find("input[name='"+aName+"']:last");
    if(inp != undefined && inp.length > 0)
        inp.disabled = true;
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

// json help routines
function tdbGetJsonRecord(trackName)  { return trackDbJson[trackName]; }
function tdbIsFolder(tdb)             { return (tdb.kindOfParent == 1); } // NOTE: These must jive with tdbKindOfParent() and tdbKindOfChild() in trackDb.h
function tdbIsComposite(tdb)          { return (tdb.kindOfParent == 2); }
function tdbIsMultiTrack(tdb)         { return (tdb.kindOfParent == 3); }
function tdbIsView(tdb)               { return (tdb.kindOfParent == 4); } // Don't expect to use
function tdbIsContainer(tdb)          { return (tdb.kindOfParent == 2 || tdb.kindOfParent == 3); }
function tdbIsLeaf(tdb)               { return (tdb.kindOfParent == 0); }
function tdbIsFolderContent(tdb)      { return (tdb.kindOfChild  == 1); }
function tdbIsCompositeSubtrack(tdb)  { return (tdb.kindOfChild  == 2); }
function tdbIsMultiTrackSubtrack(tdb) { return (tdb.kindOfChild  == 3); }
function tdbIsSubtrack(tdb)           { return (tdb.kindOfChild  == 2 || tdb.kindOfChild == 3); }
function tdbHasParent(tdb)            { return (tdb.kindOfChild  != 0 && tdb.parentTrack); }

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

function metadataShowHide(trackName,showLonglabel,showShortLabel)
{
// Will show subtrack specific configuration controls
// Config controls not matching name will be hidden
    var divit = $("#div_"+trackName+"_meta");
    if($(divit).css('display') == 'none') {
        $("#div_"+trackName+"_cfg").hide();  // Hide any configuration when opening metadata

        if($(divit).find('table').length == 0) {
            lookupMetadata(trackName,showLonglabel,showShortLabel);
        }
    }
    $(divit).toggle();  // jQuery hide/show
    return false;
}

function warnBoxJsSetup()
{   // Sets up warnBox if not already established.  This is duplicated from htmshell.c
    var html = "";
    html += "<center>";
    html += "<div id='warnBox' style='display:none; background-color:Beige; ";
    html += "border: 3px ridge DarkRed; width:640px; padding:10px; margin:10px; ";
    html += "text-align:left;'>";
    html += "<CENTER><B id='warnHead' style='color:DarkRed;'></B></CENTER>";
    html += "<UL id='warnList'></UL>";
    html += "<CENTER><button id='warnOK' onclick='hideWarnBox();return false;'></button></CENTER>";
    html += "</div></center>";

    html += "<script type='text/javascript'>";
    html += "function showWarnBox() {";
    html += "document.getElementById('warnOK').innerHTML='&nbsp;OK&nbsp;';";
    html += "var warnBox=document.getElementById('warnBox');";
    html += "warnBox.style.display=''; warnBox.style.width='65%%';";
    html += "document.getElementById('warnHead').innerHTML='Error(s):';";
    html += "}";
    html += "function hideWarnBox() {";
    html += "var warnBox=document.getElementById('warnBox');";
    html += "warnBox.style.display='none';warnBox.innerHTML='';";
    html += "var endOfPage = document.body.innerHTML.substr(document.body.innerHTML.length-20);";
    html += "if(endOfPage.lastIndexOf('-- ERROR --') > 0) { history.back(); }";
    html += "}";
    html += "</script>";

    $('body').prepend(html);
}

function warn(msg)
{ // adds warnings to the warnBox
    var warnList = $('#warnList'); // warnBox contains warnList
    if( warnList == undefined || $(warnList).length == 0 ) {
        warnBoxJsSetup();
        warnList = $('#warnList');
    }
    if( $(warnList).length == 0 )
        alert(msg);
    else {
        $( warnList ).append('<li>'+msg+'</li>');
        if(showWarnBox != undefined)
            showWarnBox();
        else
            alert(msg);
    }
}

function cgiBooleanShadowPrefix()
// Prefix for shadow variable set with boolean variables.
// Exact copy of code in cheapcgi.c
{
    return "boolshad.";
}

function getAllVars(obj,subtrackName)
{
// Returns a hash for all inputs and selects in an obj.
// If obj is undefined then obj is document!
    var urlData = new Object();
    if($(obj) == undefined)
        obj = $('document');
    var inp = $(obj).find('input');
    var sel = $(obj).find('select');
    //warn("obj:"+$(obj).attr('id') + " inputs:"+$(inp).length+ " selects:"+$(sel).length);
    $(inp).filter('[name]:enabled').each(function (i) {
        var name  = $(this).attr('name');
        var val = $(this).val();
        if($(this).attr('type') == 'checkbox') {
            name = cgiBooleanShadowPrefix() + name;
            val = $(this).attr('checked') ? 1 : 0;
        } else if($(this).attr('type') == 'radio') {
            if(!$(this).attr('checked')) {
                name = undefined;
            }
        }
        if(name != undefined && name != "Submit" && val != undefined) {
            urlData[name] = val;
        }
    });
    $(sel).filter('[name]:enabled').each(function (i) {
        var name  = $(this).attr('name');
        var val = $(this).val();
        if(name != undefined && val != undefined) {
            if(subtrackName != undefined && name == subtrackName) {
                if(val == 'hide') {
                   urlData[name+"_sel"] = 0;    // Can't delete "_sel" because default takes over
                   urlData[name]        = "[]";  // can delete vis because subtrack vis should be inherited.
                } else {
                    urlData[name+"_sel"] = 1;
                    urlData[name]        = val;
                }
            } else
                urlData[name] = val;
        }
    });
    return urlData;
}

function setIdRemoveName(obj)
{ // This function removes the name of an obj and sets it as the id.  This is very useful
  // to override forms submitting named inputs and instead setCartVarFromObjId() can be used selectively
    var id = $(obj).attr('name');
    if(id != undefined) {
        $(obj).attr('id',id);
        $(obj).removeAttr('name');
    }
    //warn($(obj).attr('id')+'='+$(obj).val()+" name:"+$(obj).attr('name'));
}


function varHashChanges(newVars,oldVars)
{
// Returns a hash of all vars that are changed between old and new hash.  New vars not found in old are changed.
    var changedVars = new Object();
    for (var newVar in newVars) {
        if(oldVars[newVar] == null || oldVars[newVar] != newVars[newVar])
            changedVars[newVar] = newVars[newVar];
    }
    return changedVars;
}

function varHashToQueryString(varHash)
{
// return a CGI QUERY_STRING for name/vals in given object
    var retVal = "";
    var count = 0;
    for (var aVar in varHash) {
        if(count++ > 0) {
            retVal += "&";
        }
        // XXXX encode var=val ?
        retVal += aVar + "=" + varHash[aVar];
    }
    return retVal;
}

function getAllVarsAsUrlData(obj)
{
// Returns a string in the form of var1=val1&var2=val2... for all inputs and selects in an obj
// If obj is undefined then obj is document!
    return varHashToQueryString(getAllVars(obj));
}

/*
function popupBox(popit, content, popTitle)
{
// Kicks off a Modal Dialog for the provided content.
// Requires jquery-ui.js
// NEEDS SOME WORK

    warn(content);

    // Set up the popit div if necessary
    if(popit == undefined) {
        popit = $('#popit');

        if(popit == undefined) {
            $('body').prepend("<div id='popit' style='display: none'></div>");
            popit = $('#popit');
        }
    }

    // Set up the modal dialog
    $(popit).html("<div style='font-size:80%'>" + content + "</div>");
    $(popit).dialog({
                               ajaxOptions: {
                                   // This doesn't work
                                   cache: true
                               },
                               resizable: true,
                               bgiframe: true,
                               height: 'auto',
                               width: 'auto',
                               minHeight: 200,
                               minWidth: 400,
                               modal: true,
                               closeOnEscape: true,
                               autoOpen: false,
                               close: function() {
                                   // clear out html after close to prevent problems caused by duplicate html elements
                                   $(popDiv).empty();
                               }
                           });
    // Apparently the options above to dialog take only once, so we set title explicitly.
    if(popTitle != undefined && popTitle.length > 0)
        $(popit).dialog('option' , 'title' , popTitle );
    else
        $(popit).dialog('option' , 'title' , "Please Respond");
    jQuery('body').css('cursor', '');
    $(popit).dialog('open');
}
*/

function embedBoxOpen(boxit, content, reenterable) // 4 extra STRING Params: boxWidth, boxTitle, applyFunc, applyName
{
// embeds a box for the provided content.
// This box has 1 button (close) by default and 2 buttons if the name of an applyFunc is provided (apply, cancel)
// If there is no apply function, the box may be reentrent, meaning subsequent calls do not need to provide content

    // Define extra params now
    var boxWidth = "80%";
    var boxTitle = "";
    var applyFunc = "";
    var applyName = "Apply";
    if (arguments.length > 3 && arguments[3].length > 0) // FIXME: could check type
        boxWidth = arguments[3];
    if (arguments.length > 4 && arguments[4].length > 0)
        boxTitle = arguments[4];
    if (arguments.length > 5 && arguments[5].length > 0)
        applyFunc = arguments[5];
    if (arguments.length > 6 && arguments[6].length > 0)
        applyName = arguments[6];

    // Set up the popit div if necessary
    if (boxit == undefined) {
        boxit = $('div#boxit');

        if (boxit == undefined) {
            $('body').prepend("<div id='boxit'></div>");
            //$('body').prepend("<div id='boxit' style='display: none'></div>");
            boxit = $('div#boxit');
        }
    }
    if (!reenterable || (content.length > 0)) { // Can reenter without changing content!

        var buildHtml = "<center>";
        if (boxTitle.length > 0)
            buildHtml += "<div style='background-color:#D9E4F8;'><B>" +  boxTitle + "</B></div>";

        buildHtml += "<div>" + content + "</div>";

        // Set up closing code
        var closeButton = "Close";
        var closeHtml = "embedBoxClose($(\"#"+ $(boxit).attr('id') + "\"),";
        if (reenterable && applyFunc.length == 0)
            closeHtml += "true);"
        else
            closeHtml += "false);";

        // Buttons
        buildHtml += "<div>";
        if (applyFunc.length > 0) { // "Apply" button and "Cancel" button.  Apply also closes!
            buildHtml += "&nbsp;<INPUT TYPE='button' value='" + applyName + "' onClick='"+ applyFunc + "(" + $(boxit).attr('id') + "); " + closeHtml + "'>&nbsp;";
            closeButton = "Cancel"; // If apply button then close is cancel
        }
        buildHtml += "&nbsp;<INPUT TYPE='button' value='" + closeButton + "' onClick='" + closeHtml + "'>&nbsp;";
        buildHtml += "</div>";

        $(boxit).html("<div class='blueBox' style='width:" + boxWidth + "; background-color:#FFF9D2;'>" + buildHtml + "</div>");  // Make it boxed
    }

    if ($(boxit).html() == null || $(boxit).html().length == 0)
        warn("embedHtmlBox() called without content");
    else
        $(boxit).show();
}

function embedBoxClose(boxit, reenterable) // 4 extra STRING Params: boxWidth, boxTitle, applyFunc, applyName
{
// Close an embedded box
    if (boxit != undefined) {
        $(boxit).hide();
        if(!reenterable)
            $(boxit).empty();
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

function getDb()
{
    var db = document.getElementsByName("db");
    if(db == undefined || db.length == 0)
        {
        db = $("#db");
        if(db == undefined || db.length == 0)
            return ""; // default?
        }
    return db[0].value;
}

function getTrack()
{
    var track = $("#track");
    if(track == undefined || track.length == 0)
        return ""; // default?
    return track[0].value;
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
    } else if(arguments.length > 0)  {
        var coords = arguments[0].split(",");
        this.startX = coords[0];
        this.endX = coords[2];
        this.startY = coords[1];
        this.endY = coords[3];
    } else { // what else to do?
        this.startX = 0;
        this.endX = 100;
        this.startY = 0;
        this.endY = 100;
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

function parsePosition(position)
{
// Parse chr:start-end string into a chrom, start, end object
    position = position.replace(/,/g, "");
    var a = /(\S+):(\d+)-(\d+)/.exec(position);
    if(a != null && a.length == 4) {
        var o = new Object();
        o.chrom = a[1];
        o.start = parseInt(a[2])
        o.end = parseInt(a[3]);
        return o;
    }
    return null;
}

function getSizeFromCoordinates(position)
{
// Parse size out of a chr:start-end string
    var o = parsePosition(position);
    if(o != null) {
        return o.end - o.start + 1;
    }
    return null;
}

// This code is intended to allow setting up a wait cursor while waiting on the function
var gWaitFuncArgs = [];
var gWaitFunc;

function waitMaskClear()
{ // Clears the waitMask
    var  waitMask = $('#waitMask');
    if( waitMask != undefined )
        $(waitMask).hide();
}

function waitMaskSetup(timeOutInMs)
{ // Sets up the waitMask to block page manipulation until cleared

    // Find or create the waitMask (which masks the whole page)
    var  waitMask = $('#waitMask');
    if( waitMask == undefined || waitMask.length != 1) {
        // create the waitMask
        $("body").append("<div id='waitMask' class='waitMask');'></div>");
        waitMask = $('#waitMask');
        // Special for IE
        if ($.browser.msie)
            $(waitMask).css('filter','alpha(opacity= 0)');
    }
    $(waitMask).css('display','block');

    // Things could fail, so always have a timeout.
    if(timeOutInMs == undefined || timeOutInMs <=0)
        timeOutInMs = 5000; // Don't ever leave this as infinite
    setTimeout('waitMaskClear();',timeOutInMs); // Just in case
}

function _launchWaitOnFunction()
{ // should ONLY be called by waitOnFunction()
  // Launches the saved function
    var func = gWaitFunc;
    gWaitFunc = null;
    var funcArgs = gWaitFuncArgs;
    gWaitFuncArgs = [];

    if(func == undefined || !jQuery.isFunction(func))
        warn("_launchWaitOnFunction called without a function");
    else {
        if(funcArgs.length == 0)
            func();
        else if (funcArgs.length == 1)
            func(funcArgs[0]);
        else if (funcArgs.length == 2)
            func(funcArgs[0],funcArgs[1]);
        else if (funcArgs.length == 3)
            func(funcArgs[0],funcArgs[1],funcArgs[2]);
        else if (funcArgs.length == 4)
            func(funcArgs[0],funcArgs[1],funcArgs[2],funcArgs[3]);
        else if (funcArgs.length == 5)
            func(funcArgs[0],funcArgs[1],funcArgs[2],funcArgs[3],funcArgs[4]);
        else
            warn("_launchWaitOnFunction called with " + funcArgs.length + " arguments.  Only 5 are supported.");
    }
    // Special if the first var is a button that can visually be inset
    if(funcArgs.length > 0 && funcArgs[0].type != undefined) {
        if(funcArgs[0].type == 'button' && $(funcArgs[0]).hasClass('inOutButton')) {
            $(funcArgs[0]).css('borderStyle',"outset");
        }
    }
    // Now we can get rid of the wait cursor
    waitMaskClear();
}

function waitOnFunction(func)
{ // sets the waitMask (wait cursor and no clicking), then launches the function with up to 5 arguments
    if(!jQuery.isFunction(func)) {
        warn("waitOnFunction called without a function");
        return false;
    }
    if(arguments.length > 6) {
        warn("waitOnFunction called with " + arguments.length - 1 + " arguments.  Only 5 are supported.");
        return false;
    }

    waitMaskSetup(5000);  // Find or create the waitMask (which masks the whole page) but gives up after 5sec

    // Special if the first var is a button that can visually be inset
    if(arguments.length > 1 && arguments[1].type != undefined) {
        if(arguments[1].type == 'button' && $(arguments[1]).hasClass('inOutButton')) {
            $(arguments[1]).css( 'borderStyle',"inset");
        }
    }

    // Build up the aruments array
    for(var aIx=1;aIx<arguments.length;aIx++) {
        gWaitFuncArgs.push(arguments[aIx])
    }
    gWaitFunc = func;

    setTimeout('_launchWaitOnFunction();',50);

}

function showLoadingImage(id)
{
// Show a loading image above the given id; return's id of div added (so it can be removed when loading is finished).
// This code was mostly directly copied from hgHeatmap.js, except I also added the "overlay.appendTo("body");"
    var loadingId = id + "LoadingOverlay";
    var overlay = $("<div></div>").attr("id", loadingId).css("position", "absolute");
    overlay.appendTo("body");
    overlay.css("top", $('#'+ id).position().top);
    var divLeft = $('#'+ id).position().left + 2;
    overlay.css("left",divLeft);
    var width = $('#'+ id).width() - 5;
    var height = $('#'+ id).height();
    overlay.width(width);
    overlay.height(height);
    overlay.css("background", "white");
    overlay.css("opacity", 0.75);
    var imgLeft = (width / 2) - 110;
    var imgTop = (height / 2 ) - 10;
    $("<img src='../images/loading.gif'/>").css("position", "relative").css('left', imgLeft).css('top', imgTop).appendTo(overlay);
    return loadingId;
}

function hideLoadingImage(id)
{
    $('#' + id).remove();
}

function codonColoringChanged(name)
{
// Updated disabled state of codonNumbering checkbox based on current value of track coloring select.
    var val = $("select[name='" + name + ".baseColorDrawOpt'] option:selected").text();
    $("input[name='" + name + ".codonNumbering']").attr('disabled', val == "OFF");
}
