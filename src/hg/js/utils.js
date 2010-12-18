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
    var trs = $('table.subtracks').children('tbody').children('tr');
    if(!onlySelected)
        $(trs).show();
    else {
        $(trs).each(function (ix) {
            var subCB = $(this).find('input.subCB');
            if (subCB.length > 0 && subCB[0].checked && subCB[0].disabled == false)
                $(this).show();
            else
                $(this).hide();
        });
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

        if($(divit).find('table').length == 0)
            lookupMetadata(trackName,showLonglabel,showShortLabel);
    }
    var tr = $(divit).parents('tr');
    if (tr.length > 0) {
        $(divit).children('table').css('backgroundColor',$(tr[0]).css('backgroundColor'));
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


//////////// Drag and Drop ////////////
function tableDragAndDropRegister(thisTable)
{// Initialize a table with tableWithDragAndDrop
    if ($(thisTable).hasClass("tableWithDragAndDrop") == false)
        return;

    $(thisTable).tableDnD({
        onDragClass: "trDrag",
        dragHandle: "dragHandle",
        onDrop: function(table, row, dragStartIndex) {
                if(tableSetPositions) {
                    tableSetPositions(table);
                }
            }
    });
    $(thisTable).find("td.dragHandle").hover(
        function(){ $(this).closest('tr').addClass('trDrag'); },
        function(){ $(this).closest('tr').removeClass('trDrag'); }
    );
}

//////////// Sorting ////////////
// Sorting a table by columns relies upon the sortColumns structure

// The sortColumns structure looks like:
//{
//    char *  tags[];     // a list of field names in sort order (e.g. 'cell', 'shortLabel')
//    boolean reverse[];  // the sort direction for each sort field
//    int     cellIxs[];  // The indexes of the columns in the table to be sorted
//    boolean useAbbr[];  // Compare on Abbr or on innerHtml?
//}
// These 2 globals are used by setTimeout, so that rows can be hidden while sorting and javascript timeout is less likely
var gSortColumns;
var gTbody

function sortField(value,index)
{
this.value=value;
this.index=index;
}

function sortRow(tr,sortColumns,row)  // UNUSED: sortField works fine
{
    this.fields  = new Array();
    this.reverse = new Array();
    this.row     = row;
    for(var ix=0;ix<sortColumns.cellIxs.length;ix++)
        {
        var th = tr.cells[sortColumns.cellIxs[ix]];
        this.fields[ix]  = (sortColumns.useAbbr[ix] ? th.abbr : $(th).text()).toLowerCase(); // case insensitive sorts
        this.reverse[ix] = sortColumns.reverse[ix];
        }
}

function sortRowCmp(a,b)  // UNUSED: sortField works fine
{
    for(var ix=0;ix<a.fields.length;ix++) {
        if (a.fields[ix] > b.fields[ix])
            return (a.reverse[ix] ? -1:1);
        else if (a.fields[ix] < b.fields[ix])
            return (a.reverse[ix] ? 1:-1);
    }
    return 0;
}

function sortField(value,reverse,row)
{
    this.value   = value.toLowerCase(); // case insensitive sorts NOTE: Do not need to define every field
    this.reverse = reverse;
    this.row     = row;
}
function sortFieldCmp(a,b)
{
    if (a.value > b.value)
        return (a.reverse ? -1:1);
    else if (a.value < b.value)
        return (a.reverse ? 1:-1);
    return 0;
}

function tableSort(tbody,sortColumns)
{// Sorts table based upon rules passed in by function reference
 // Expects tbody to not sort thead, but could take table

    // The sort routine available is javascript array.sort(), which cannot sort rows directly
    // Until we have jQuery >=v1.4, we cannot easily convert tbody.rows[] inot a javascript array
    // So we will make our own array, sort, then then walk through the table and reorder
    // FIXME: Until better methods are developed, only sortOrder based sorts are supported and fnCompare is obsolete

    // Create array of the primary sort column's text
    var cols = new Array();
    var trs = tbody.rows;
    $(trs).each(function(ix) {
        //cols.push(new sortRow(this,sortColumns,$(this).clone()));
        var th = this.cells[sortColumns.cellIxs[0]];
        if(sortColumns.useAbbr[0])
            cols.push(new sortField(th.abbr,sortColumns.reverse[0],$(this).clone())); // When jQuery >= v1.4, use detach() insterad of clone()
        else
            cols.push(new sortField($(th).text(),sortColumns.reverse[0],$(this).clone()));
    });

    // Sort the array
    //cols.sort(sortRowCmp);
    cols.sort(sortFieldCmp);

    // Now reorder the table
    for(var cIx=0;cIx<cols.length;cIx++) {
        $(tbody.rows[cIx]).replaceWith(cols[cIx].row);
    }

    gTbody=tbody;
    gSortColumns=sortColumns;
    setTimeout('tableSortFinish(gTbody,gSortColumns)',5); // Avoid javascript timeouts!
}

function tableSortFinish(tbody,sortColumns)
{// Additional sort cleanup.
 // This is in a separate function to allow calling with setTimeout() which will prevent javascript timeouts (I hope)
    tableSetPositions(tbody);
    if ($(tbody).hasClass('altColors'))
        sortedTableAlternateColors(tbody,sortColumns);
    $(tbody).parents("table.tableWithDragAndDrop").each(function (ix) {
        tableDragAndDropRegister(this);
    });
    //$(tbody).show();
    $(tbody).removeClass('sorting');
}

function tableSortByColumns(tbody,sortColumns)
{// Will sort the table based on the abbr values on a set of <TH> colIds
 // Expects tbody to not sort thead, but could take table
    //$(tbody).hide();
    $(tbody).addClass('sorting');
    gTbody=tbody;
    gSortColumns=sortColumns;
    setTimeout('tableSort(gTbody,gSortColumns)',50); // This allows hiding the rows while sorting!
}

function trAlternateColors(tbody,rowGroup,cellIx)
{// Will alternate colors for visible table rows.
 // If cellIx(s) provided then color changes when the column(s) abbr or els innerHtml changes
 // If no cellIx is provided then alternates on rowGroup (5= change color 5,10,15,...)
 // Expects tbody to not color thead, but could take table
    var darker   = false; // == false will trigger first row to be change color = darker

    if (arguments.length<3) { // No columns to check so alternate on rowGroup

        if (rowGroup == undefined || rowGroup == 0)
            rowGroup = 1;
        var curCount = 0; // Always start with a change
        $(tbody).children('tr:visible').each( function(i) {
            if (curCount == 0 ) {
                curCount  = rowGroup;
                darker = (!darker);
            }
            if (darker) {
                $(this).removeClass("bgLevel1");
                $(this).addClass(   "bgLevel2");
            } else {
                $(this).removeClass("bgLevel2");
                $(this).addClass(   "bgLevel1");
            }
            curCount--;
        });

    } else {

        var lastContent = "startWithChange";
        var cIxs = new Array();
        for(var aIx=2;aIx<arguments.length;aIx++) {   // multiple columns
            cIxs[aIx-2] = arguments[aIx];
        }
        $(tbody).children('tr:visible').each( function(i) {
            curContent = "";
            for(var ix=0;ix<cIxs.length;ix++) {
                if (this.cells[cIxs[ix]]) {
                    curContent += (this.cells[cIxs[ix]].abbr != "" ?
                                   this.cells[cIxs[ix]].abbr       :
                                   this.cells[cIxs[ix]].innerHTML );
                }
            }
            if (lastContent != curContent ) {
                lastContent  = curContent;
                darker = (!darker);
            }
            if (darker) {
                $(this).removeClass("bgLevel1");
                $(this).addClass(   "bgLevel2");
            } else {
                $(this).removeClass("bgLevel2");
                $(this).addClass(   "bgLevel1");
            }
        });
    }
}

function sortedTableAlternateColors(tbody)
{ // Will alternate colors based upon sort columns (which may be passed in as second arg, or discovered)
// Expects tbody to not color thead, but could take table
    var sortColumns;
    if (arguments.length > 1)
        sortColumns = arguments[1];
    else {
        var table = tbody;
        if ($(table).is('tbody'))
            table = $(tbody).parent();
        sortColumns = new sortColumnsGetFromTable(table);
    }

    if (sortColumns) {
        if (sortColumns.cellIxs.length==1)
            trAlternateColors(tbody,0,sortColumns.cellIxs[0]);
        else if (sortColumns.cellIxs.length==2)
            trAlternateColors(tbody,0,sortColumns.cellIxs[0],sortColumns.cellIxs[1]);
        else // Three columns is plenty
            trAlternateColors(tbody,0,sortColumns.cellIxs[0],sortColumns.cellIxs[1],sortColumns.cellIxs[2]);
    } else {
        trAlternateColors(tbody,5); // alternates every 5th row
    }
}

function sortOrderFromColumns(sortColumns)
{// Creates the trackDB setting entry sortOrder subGroup1=+ ... from a sortColumns structure
    fields = new Array();
    for(var ix=0;ix < sortColumns.cellIxs.length;ix++) {
        if (sortColumns.tags[ix] != undefined && sortColumns.tags[ix].length > 0)
            fields[ix] = sortColumns.tags[ix] + "=" + (sortColumns.reverse[ix] ? "-":"+");
        else
            fields[ix] = sortColumns.cellIxs[ix] + "=" + (sortColumns.reverse[ix] ? "-":"+");
    }
    var sortOrder = fields.join(' ');
    //warn("sortOrderFromColumns("+sortColumns.cellIxs.length+"):["+sortOrder+"]");
    return sortOrder;
}

function sortOrderUpdate(table,sortColumns,addSuperscript)
{// Updates the sortOrder in a sortable table
    if (addSuperscript == undefined)
        addSuperscript = false;
    if ($(table).is('tbody'))
        table = $(table).parent();
    var tr = $(table).find('tr.sortable')[0];
    if (tr) {
        //warn("sortOrderUpdate("+sortColumns.cellIxs.length+")");
        for(cIx=0;cIx<sortColumns.cellIxs.length;cIx++) {
            var th = tr.cells[sortColumns.cellIxs[cIx]];
            $(th).each(function(i) {
                // First remove old sort classes
                var classList = $( this ).attr("class").split(" ");
                if (classList.length < 2) // assertable
                    return;
                classList = aryRemove(classList,"sortable");
                while( classList.length > 0 ) {
                    var aClass = classList.pop();
                    if (aClass.indexOf("sort") == 0)
                        $(this).removeClass(aClass);
                }

                // Now add current sort classes
                $(this).addClass("sort"+(cIx+1));
                if (sortColumns.reverse[cIx])
                    $(this).addClass("sortRev");

                // update any superscript
                sup = $(this).find('sup')[0];
                if (sup || addSuperscript) {
                    var content = (sortColumns.reverse[cIx] == false ? "&darr;":"&uarr;");

                    if (sortColumns.cellIxs.length>1) { // Number only if more than one
                        if (cIx < 5)  // Show numbering and direction only for the first 5
                            content += (cIx+1);
                        else
                            content = "";
                    }

                    if (sup)
                        sup.innerHTML = content;
                    else
                        $(th).append("<sup>"+content+"</sup>");
                }
            });
        }
        // There may be a hidden input that gets updated to the cart
        var inp = $(tr).find('input.sortOrder')[0];
        if (inp)
            $(inp).val(sortOrderFromColumns(sortColumns));
    }
}

function sortOrderFromTr(tr)
{// Looks up the sortOrder input value from a *.sortable header row of a sortable table
    var inp = $(tr).find('input.sortOrder')[0];
    if (inp)
        return $(inp).val();
    else {
        // create something like "cellType=+ rep=+ protocol=+ treatment=+ factor=+ view=+"
        var fields = new Array();
        var cells = $(tr).find('th.sortable');
        $(cells).each(function (i) {
            var classList = $( this ).attr("class").split(" ");
            if (classList.length < 2) // assertable
                return;
            classList = aryRemove(classList,"sortable");
            var reverse = false;
            var sortIx = -1;
            while( classList.length > 0 ) {
                var aClass = classList.pop();
                if (aClass.indexOf("sort") == 0) {
                    if (aClass == "sortRev")
                        reverse = true;
                    else {
                        aClass = aClass.substring(4);  // clip off the "sort" portion
                        var ix = parseInt(aClass);
                        if (ix != NaN) {
                            sortIx = ix;
                        }
                    }
                }
            }
            if (sortIx >= 0) {
                if (this.id != undefined && this.id.length > 0)
                    fields[sortIx] = this.id + "=" + (reverse ? "-":"+");
                else
                    fields[sortIx] = this.cellIndex + "=" + (reverse ? "-":"+");
            }
        });
        if (fields.length > 0) {
            if (fields[0] == undefined)
                fields.shift();  // 1 based sort ix and 0 based fields ix
            return fields.join(' ');
        }
    }
    return "";
}
function sortColumnsGetFromSortOrder(sortOrder)
{// Creates sortColumns struct (without cellIxs[]) from a trackDB.sortOrder setting string
    this.tags = new Array();
    this.reverse = new Array();
    var fields = sortOrder.split(" "); // sortOrder looks like: "cell=+ factor=+ view=+"
    while(fields.length > 0) {
        var pair = fields.shift().split("=");  // Take first and split into
        if (pair.length == 2) {
            this.tags.push(pair[0]);
            this.reverse.push(pair[1] != '+');
        }
    }
}
function sortColumnsGetFromTr(tr,silent)
{// Creates a sortColumns struct from the entries in the 'tr.sortable' heading row of a sortable table
    this.inheritFrom = sortColumnsGetFromSortOrder;
    var sortOrder = sortOrderFromTr(tr);
    if (sortOrder.length == 0 && silent == undefined) {
        warn("Unable to obtain sortOrder from sortable table.");   // developer needs to know something is wrong
        return;
    }

    this.inheritFrom(sortOrder);
    // Add two additional arrays
    this.cellIxs = new Array();
    this.useAbbr = new Array();
    var ths = $(tr).find('th.sortable');
    for(var tIx=0;tIx<this.tags.length;tIx++) {
        for(ix=0; ix<ths.length; ix++) {
            if ((ths[ix].id != undefined && ths[ix].id == this.tags[tIx])
            ||  (ths[ix].cellIndex == this.tags[tIx]))
            {
                this.cellIxs[tIx] = ths[ix].cellIndex;
                this.useAbbr[tIx] = (ths[ix].abbr.length > 0);
            }
        }
    }
    if (this.cellIxs.length == 0 && silent == undefined) {
        warn("Unable to find any sortOrder.cells for sortable table.  ths.length:"+ths.length + " tags.length:"+this.tags.length + " sortOrder:["+sortOrder+"]");
        return;
    }
}

function sortColumnsGetFromTable(table)
{// Creates a sortColumns struct from the contents of a 'table.sortable'
    this.inheritNow = sortColumnsGetFromTr;
    var tr = $(table).find('tr.sortable')[0];
    //if (tr == undefined && debug) warn("Couldn't find 'tr.sortable' rows:"+table.rows.length);
    this.inheritNow(tr);
}


function _tableSortOnButtonPressEncapsulated(anchor)
{// Updates the sortColumns struct and sorts the table when a column header has been pressed
 // If the current primary sort column is pressed, its direction is toggled then the table is sorted
 // If a secondary sort column is pressed, it is moved to the primary spot and sorted in fwd direction
    var th=$(anchor).closest('th')[0];  // Note that anchor is <a href> within th, not th
    var tr=$(th).parent();
    var theOrder = new sortColumnsGetFromTr(tr);
    var oIx = th.cellIndex;
    for(oIx=0;oIx<theOrder.cellIxs.length;oIx++) {
        if (theOrder.cellIxs[oIx] == th.cellIndex)
            break;
    }
    if (oIx == theOrder.cellIxs.length) {
        warn("Failure to find '"+th.id+"' in sort columns."); // Developer must be warned that something is wrong with sortable table setup
        return;
    }
    // assert(th.id == theOrder.tags[oIx] || th.id == undefined);
    if (oIx > 0) { // Need to reorder
        var newOrder = new sortColumnsGetFromTr(tr);
        var nIx=0; // button pushed puts this 'tagId' column first in new order
        newOrder.tags[nIx] = theOrder.tags[oIx];
        newOrder.reverse[nIx] = false;  // When moving to the first position sort forward
        newOrder.cellIxs[nIx] = theOrder.cellIxs[oIx];
        newOrder.useAbbr[nIx] = theOrder.useAbbr[oIx];
        for(var ix=0;ix<theOrder.cellIxs.length;ix++) {
            if (ix != oIx) {
                nIx++;
                newOrder.tags[nIx]    = theOrder.tags[ix];
                newOrder.reverse[nIx] = theOrder.reverse[ix];
                newOrder.cellIxs[nIx] = theOrder.cellIxs[ix];
                newOrder.useAbbr[nIx] = theOrder.useAbbr[ix];
            }
        }
        theOrder = newOrder;
    } else { // if (oIx == 0) {   // need to reverse directions
        theOrder.reverse[oIx] = (theOrder.reverse[oIx] == false);
    }
    var table=$(tr).closest("table.sortable")[0];
    if (table) { // assertable
        sortOrderUpdate(table,theOrder);  // Must update sortOrder first!
        var tbody = $(table).find("tbody.sortable")[0];
        //if (tbody == undefined && debug) warn("Couldn't find 'tbody.sortable' 5");
        tableSortByColumns(tbody,theOrder);
    }
    return;

}

function tableSortOnButtonPress(anchor,tagId)
{
    var table = $( anchor ).closest("table.sortable")[0];
    if (table) {
        waitOnFunction( _tableSortOnButtonPressEncapsulated, anchor, tagId);
    }
    return false;  // called by link so return false means don't try to go anywhere
}

function tableSortUsingSortColumns(table) // NOT USED
{// Sorts a table body based upon the marked columns
    var columns = new sortColumnsGetFromTable(table);
    tbody = $(table).find("tbody.sortable")[0];
    if (tbody)
        tableSortByColumns(tbody,columns);
}

function hintOverSortableColumnHeader(th) // NOT USED
{// Upodates the sortColumns struct and sorts the table when a column headder has been pressed
    //th.title = "Click to make this the primary sort column, or toggle direction";
    //var tr=th.parentNode;
    //th.title = "Current Sort Order: " + sortOrderFromTr(tr);
}

function tableSetPositions(table)
{// Sets the value for the input.trPos of a table row.  Typically this is a "priority" for a track
 // This gets called by sort or dragAndDrop in order to allow the new order to affect hgTracks display
    var inputs = $(table).find("input.trPos");
    $( inputs ).each( function(i) {
        var tr = $( this ).closest('tr')[0];
        $( this ).val( $(tr).attr('rowIndex') );
    });
}

///// Following functions are for Sorting by priority
function trFindPosition(tr)
{
// returns the position (*.priority) of a sortable table row
    var inp = $(tr).find('input.trPos')[0];
    if (inp)
        return $(inp).val();
    return 999999;
}

function trComparePriority(tr1,tr2)  // UNUSED FUNCTION
{
// Compare routine for sorting by *.priority
    var priority1 = trFindPosition(tr1);
    var priority2 = trFindPosition(tr2);
    return priority2 - priority1;
}

function tablesSortAtStartup()
{// Called at startup if you want javascript to initialize and sort all your class='sortable' tables
 // IMPORTANT: This function WILL ONLY sort by first column.
 // If there are multiple sort columns, please presort the list for accurtacy!!!
    var tables = $("table.sortable");
    $(tables).each(function(i) {
        sortTableInitialize(this,true); // Will initialize superscripts
        tableSortUsingSortColumns(this);
    });
}

function sortTableInitialize(table,addSuperscript,altColors)
{// Called if you want javascript to initialize your class='sortable' table.
 // A sortable table requires:
 // TABLE.sortable: TABLE class='sortable' containing a THEAD header and sortable TBODY filled with the rows to sort.
 // THEAD.sortable: (NOTE: created if not found) The THEAD can contain multiple rows must contain:
 //   TR.sortable: exactly 1 header TH (table row) class='sortable' which will declare the sort columnns:
 //   TH.sortable: 1 or more TH (table column headers) with class='sortable sort1 [sortRev]' (or sort2, sort3) declaring sort order and whether reversed
 //     e.g. <TH id='factor' class='sortable sortRev sort3' nowrap>...</TH>  (this means that factor is currently the third sort column and reverse sorted)
 //     (NOTE: If no TH.sortable is found, then every th in the TR.sortable will be converted for you and will be in sort1,2,3 order.)
 //     ONCLICK: Each TH.sortable must call tableSortOnButtonPress(this) directly or indirectly in the onclick event :
 //       e.g. <TH id='factor' class='sortable sortRev sort3' nowrap title='Sort list on this column' onclick="return tableSortOnButtonPress(this);">
 //       (NOTE: If no onclick function is found in a TH.sortable, then it will automatically be added.)
 //     SUP: Each TH.sortable *may* contain a <sup> which will be filled with an up or down arrow and the column's sort order: e.g. <sup>&darr;2</sup>
 //       (NOTE: If no sup is found but addSuperscript is requested, then they will be added.)
 // TBODY.sortable: (NOTE: created if not found) The TBODY class='sortable' contains the table rows that get sorted:
 //   TBODY->TR & ->TD: Each row contains a TD for each sortable column. The innerHTML (entire contents) of the cell will be used for sorting.
 //   TRICK: You can use the 'abbr' field to subtly alter the sortable contents.  Otherwise sorts on td contents ($(td).text()).
 //          Use the abbr field to make case-insensitive sorts or force exceptions to alpha-text order (e.g. ZCTRL vs Control forcing controls to bottom)
 //          e.g. <TD id='wgEncodeBroadHistoneGm12878ControlSig_factor' nowrap abbr='ZCTRL' align='left'>Control</TD>
 //          IMPORTANT: You must add abbr='use' to the TH.sortable definitions.
 // Finally if you want the tableSort to alternate the table row colors (using #FFFEE8 and #FFF9D2) then TBODY.sortable should also have class 'altColors'
 // NOTE: This class can be added by using the altColors option to this function
 //
 // PRESERVING TO CART: To send the sort column on a form 'submit', the header tr (TR.sortable) needs a named hidden input of class='sortOrder' as:
 //   e.g.: <INPUT TYPE=HIDDEN NAME='wgEncodeBroadHistone.sortOrder' class='sortOrder' VALUE="factor=- cell=+ view=+">
 //   AND each sortable column header (TH.sortable) must have id='{name}' which is the name of the sortable field (e.g. 'factor', 'shortLabel')
 //   The value preserves the column sort order and direction based upon the id={name} of each sort column.
 //   In the example, while 'cell' may be the first column, the table is currently reverse ordered by 'factor', then by cell and view.
 // And to send the sorted row orders on form 'submit', each TBODY->TR will need a named hidden input field of class='trPos':
 //   e.g. <INPUT TYPE=HIDDEN NAME='wgEncodeHaibTfbsA549ControlPcr2xDexaRawRep1.priority' class='trPos' VALUE="2">
 //   A reason to preserve the order in ther cart is if the order will affect other cgis.  For instance: sort subtracks and see that order in the hgTracks image.

    if ($(table).hasClass('sortable') == false) {
        warn('Table is not sortable');
        return;
    }
    var tr = $(table).find('tr.sortable')[0];
    if(tr == undefined) {
        tr = $(table).find('tr')[0];
        if(tr == undefined) {
            warn('Sortable table has no rows');
            return;
        }
        $(tr).addClass('sortable');
        //warn('Made first row tr.sortable');
    }
    if ($(table).find('tr.sortable').length != 1) {
        warn('sortable table contains more than 1 header row declaring sort columns.');
        return;
    }

    // If not TBODY is found, then create, wrapping all but those already in a thead
    tbody = $(table).find('tbody')[0];
    if(tbody == undefined) {
        trs = $(table).find('tr').not('thead tr');
        $(trs).wrapAll("<TBODY class='sortable' />")
        tbody = $(table).find('tbody')[0];
        //warn('Wrapped all trs not in thead.sortable in tbody.sortable');
    }
    if ($(tbody).hasClass('sortable') == false) {
        $(tbody).addClass('sortable');
        //warn('Added sortable class to tbody');
    }
    if(altColors != undefined && $(tbody).hasClass('altColors') == false) {
        $(tbody).addClass('altColors');
        //warn('Added altColors class to tbody.sortable');
    }
    $(tbody).hide();

    // If not THEAD is found, then create, wrapping first row.
    thead = $(table).find('thead')[0];
    if(thead == undefined) {
        $(tr).wrapAll("<THEAD class='sortable' />")
        thead = $(table).find('thead')[0];
        $(thead).insertBefore(tbody);
        //warn('Wrapped tr.sortable with thead.sortable');
    }
    if ($(thead).hasClass('sortable') == false) {
        $(thead).addClass('sortable');
        //warn('Added sortable class to thead');
    }

    var sortColumns = new sortColumnsGetFromTr(tr,"silent");
    if (sortColumns == undefined || sortColumns.cellIxs.length == 0) {
        // could mark all columns as sortable!
        $(tr).find('th').each(function (ix) {
            $(this).addClass('sortable');
            $(this).addClass('sort'+(ix+1));
            //warn("Added class='sortable sort"+(ix+1)+"' to th:"+this.innerHTML);
        });
        sortColumns = new sortColumnsGetFromTr(tr,"silent");
        if (sortColumns == undefined || sortColumns.cellIxs.length == 0) {
            warn("sortable table's header row contains no sort columns.");
            return;
        }
    }
    // Can wrap all columnn headers with link
    $(tr).find("th.sortable").each(function (ix) {
        //if ( $(this).queue('click').length == 0 ) {
        if ( $(this).attr('onclick') == undefined ) {
            $(this).click( function () { tableSortOnButtonPress(this);} );
        }
        if ( $(this).attr('title').length == 0) {
            var title = $(this).text().replace(/[^a-z0-9 ]/ig,'');
            if (title.length > 0 && $(this).find('sup'))
                title = title.replace(/[0-9]$/g,'');
            if (title.length > 0)
                $(this).attr('title',"Sort list on '" + title + "'." );
            else
                $(this).attr('title',"Sort list on column." );
        }
    })
    // Now update all of those cells
    sortOrderUpdate(table,sortColumns,addSuperscript);

    // Alternate colors if requested
    if(altColors != undefined)
        sortedTableAlternateColors(tbody);

    // Highlight rows?  But on subtrack list, this will mess up the "..." coloring.  So just exclude tables with drag and drop
    if ($(table).hasClass('tableWithDragAndDrop') == false) {
        $('tbody.sortable').find('tr').hover(
            function(){ $(this).addClass('bgLevel3'); },      // Will highlight the rows
            function(){ $(this).removeClass('bgLevel3');}
        );
    }

    // Finally, make visible
    $(tbody).removeClass('sorting');
    $(tbody).show();
}

