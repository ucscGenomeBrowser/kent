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
function tdbGetJsonRecord(trackName)  { return hgTracks.trackDb[trackName]; }
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


function metadataIsVisible(trackName)
{
    var divit = $("#div_"+trackName+"_meta");
    if (divit == undefined || divit.length == 0)
        return false;
    return ($(divit).css('display') != 'none');
}

function metadataShowHide(trackName,showLonglabel,showShortLabel)
{
// Will show subtrack specific configuration controls
// Config controls not matching name will be hidden
    var divit = $("#div_"+trackName+"_meta");
    if (divit == undefined || divit.length == 0)
        return false;
    var img = $(divit).prev('a').find("img");
    if (img != undefined && $(img).length == 1) {
        img = $(img)[0];
        if ($(divit).css('display') == 'none')
            $(img).attr('src','../images/upBlue.png');
        else
            $(img).attr('src','../images/downBlue.png');
    }
    if($(divit).css('display') == 'none') {
        $("#div_"+trackName+"_cfg").hide();  // Hide any configuration when opening metadata

        if($(divit).find('table').length == 0) {
            lookupMetadata(trackName,showLonglabel,showShortLabel);
            return false;
        }
    }
    var tr = $(divit).parents('tr');
    if (tr.length > 0) {
        tr = tr[0];
        var bgClass = null;
        var classes = $( tr ).attr("class").split(" ");
        for (var ix=0;ix<classes.length;ix++) {
            if (classes[ix].substring(0,'bgLevel'.length) == 'bgLevel')
                bgClass = classes[ix];
        }
        if (bgClass) {
            $(divit).children('table').removeClass('bgLevel1 bgLevel2 bgLevel3 bgLevel4');
            $(divit).children('table').addClass(bgClass);
        }
    }
    $(divit).toggle();  // jQuery hide/show
    return false;
}

function setTableRowVisibility(button, prefix, hiddenPrefix, titleDesc, doAjax)
{
// Show or hide one or more table rows whose id's begin with prefix followed by "-".
// This code also modifies the corresponding hidden field (cart variable) and the
// src of the +/- img button.
    var retval = true;
    var hidden = $("input[name='"+hiddenPrefix+"_"+prefix+"_close']");
    if($(button) != undefined && $(hidden) != undefined && $(hidden).length > 0) {
        var oldSrc = $(button).attr("src");
	var newVal;
        if(arguments.length > 5)
            newVal = arguments[5] ? 0 : 1;
        else
            newVal = oldSrc.indexOf("/remove") > 0 ? 1 : 0;
        var newSrc;
        if(newVal == 1) {
            newSrc = oldSrc.replace("/remove", "/add");
            $(button).attr('title', 'Expand this '+titleDesc);
            $("tr[id^='"+prefix+"-']").hide();
        } else {
            newSrc = oldSrc.replace("/add", "/remove");
            $(button).attr('title', 'Collapse this '+titleDesc);
            $("tr[id^='"+prefix+"-']").show();
        }
        $(button).attr("src", newSrc);
        $(hidden).val(newVal);
	if (doAjax) {
	    setCartVar(hiddenPrefix+"_"+prefix+"_close", newVal);
	}
        retval = false;
    }
    return retval;
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

var gWarnSinceMSecs = 0;
function warnSince(msg)
{   // Warn messages with msecs since last warnSince msg
    // This is necessary because IE Developer tools are hanging
    var now = new Date();
    var msecs = now.getTime();
    var since = 0;
    if (gWarnSinceMSecs > 0)
        since = msecs - gWarnSinceMSecs;
    gWarnSinceMSecs = msecs;
    warn('['+since+'] '+msg);
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
            } else {
                if ($.isArray( val ) && val.length > 1) {
                    urlData[name] = "[" + val.toString() + "]";
                } else
                    urlData[name] = val;
            }
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
        var val = varHash[aVar];
        if (typeof(val) == 'string'
        && val.length >= 2
        && val.indexOf('[') == 0
        && val.lastIndexOf(']') == (val.length - 1)) {
            var vals = val.substr(1,val.length - 2).split(',');
            $(vals).each(function (ix) {
                if (ix > 0)
                    retVal += "&";
                retVal += aVar + "=" + encodeURIComponent(this);
            });
        } else {
            retVal += aVar + "=" + encodeURIComponent(val);
        }
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
    }
    $(waitMask).css({opacity:0.0,display:'block',top: '0px', height: $(document).height().toString() + 'px' });
    // Special for IE, since it takes so long, make mask obvious
    if ($.browser.msie)
        $(waitMask).css({opacity:0.4,backgroundColor:'gray'});

    // Things could fail, so always have a timeout.
    if(timeOutInMs == undefined || timeOutInMs ==0)
        timeOutInMs = 30000; // IE can take forever!

    if (timeOutInMs > 0)
        setTimeout('waitMaskClear();',timeOutInMs); // Just in case

    return waitMask;  // The caller could add css if they wanted.
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

    waitMaskSetup(0);  // Find or create the waitMask (which masks the whole page) but gives up after 5sec

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

    setTimeout('_launchWaitOnFunction();',10);

}

// --- yielding iterator ---
function _yieldingIteratorObject(yieldingFunc)
{ // This is the "recusive object" or ro which is instantiated in waitOnIteratingFunction
  // yieldingFunc is passed in from waitOnIteratingFunction
  // and will recurse which recursively calls an iterator
    this.step = function(msecs,args) {
        setTimeout(function() { yieldingFunc(args); }, msecs); // recursive timeouts
        return;
    }
}

function yieldingIterator(interatingFunc,continuingFunc,args)
{   // Will run interatingFunc function with "yields", then run continuingFunc
    // Based upon design by Guido Tapia, PicNet
    // interatingFunc must return number of msecs to pause before next interation.
    //                return 0 ends iteration with call to continuingFunc
    //                return < 0 ends iteration with no call to continuingFunc
    // Both interatingFunc and continuingFunc will receive the single "args" param.
    // Hint. for multiple args, create a single struct object

    var ro = new _yieldingIteratorObject(function() {
            var msecs = interatingFunc(args);
            if (msecs > 0)
                ro.step(msecs,args);      // recursion
            else if (msecs == 0)
                continuingFunc(args);     // completion
            // else (msec < 0) // abandon
        });
    ro.step(1,args);                      // kick-off
}

function showLoadingImage(id)
{
// Show a loading image above the given id; return's id of div added (so it can be removed when loading is finished).
// This code was mostly directly copied from hgHeatmap.js, except I also added the "overlay.appendTo("body");"
    var loadingId = id + "LoadingOverlay";
    // make an opaque overlay to partially hide the image
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
    // now add the overlay image itself in the center of the overlay.
    var imgWidth = 220;   // hardwired based on width of loading.gif
    var imgLeft = (width / 2) - (imgWidth / 2);
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
        if ($.browser.msie) { // Special case for IE since CSS :hover doesn't work (note pointer and hand because older IE calls it hand)
            $(this).hover(
                function () { $(this).css( { backgroundColor: '#CCFFCC', cursor: 'pointer', cursor: 'hand' } ); },
                function () { $(this).css( { backgroundColor: '#FCECC0', cursor: '' } ); }
            );
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
            function(){ $(this).addClass('bgLevel3');
                        $(this).find('table').addClass('bgLevel3'); },      // Will highlight the rows, including '...'
            function(){ $(this).removeClass('bgLevel3');
                        $(this).find('table').removeClass('bgLevel3'); }
        );
    }

    // Finally, make visible
    $(tbody).removeClass('sorting');
    $(tbody).show();
}

function setCheckboxList(list, value)
{
// set value of all checkboxes in semicolon delimited list
    var names = list.split(";");
    for(var i=0;i<names.length;i++) {
        $("input[name='" + names[i] + "']").attr('checked', value);
    }
}

function calculateHgTracksWidth()
{
// return appropriate width for hgTracks image given users current window width
    return $(window).width() - 20;
}

function hgTracksSetWidth()
{
    var winWidth = calculateHgTracksWidth();
    if($("#imgTbl").length == 0) {
        // XXXX what's this code for?
        $("#TrackForm").append('<input type="hidden" name="pix" value="' + winWidth + '"/>');
        //$("#TrackForm").submit();
    } else {
        $("input[name=pix]").val(winWidth);
    }
}

/////////////////////////////////////////////////////
// findTracks functions

function updateMetaDataHelpLinks(index)
{
// update the metadata help links based on currently selected values.
// If index == 0 we update all help items, otherwise we only update the one == index.
    var db = getDb();
    var disabled = {  // blackList
        'accession': 1, 'dataType':        1, 'dataVersion': 1, 'geoSample':    1, 'grant':     1,
        'lab':       1, 'labExpId':        1, 'labVersion':  1, 'origAssembly': 1, 'replicate': 1,
        'setType':   1, 'softwareVersion': 1, 'subId':       1, 'view':         1
    }
    var expected = $('tr.mdbSelect').length;
    var ix=1;
    if (index!=0) {
        ix=index;
        expected=index;
    }
    for(;ix <= expected;ix++) {
        var helpLink = $("span#helpLink" + ix);
        if (helpLink.length > 0) {
            var val = $("select[name='hgt_mdbVar" + ix + "']").val();  // NOTE must match METADATA_NAME_PREFIX in hg/hgTracks/searchTracks.c
            var text = $("select[name='hgt_mdbVar" + ix + "'] option:selected").text();
            helpLink.html("&nbsp;"); // Do not want this with length == 0 later!
            if (typeof(disabled[val]) == 'undefined') {
                var str;
                if (val == 'cell') {
                    if (db.substr(0, 2) == "mm") {
                        str = "../ENCODE/cellTypesMouse.html";
                    } else {
                        str = "../ENCODE/cellTypes.html";
                    }
                } else if (val.toLowerCase() == 'antibody') {
                    str = "../ENCODE/antibodies.html";
                } else {
                    str = "../ENCODE/otherTerms.html#" + val;
                }
                helpLink.html("<a target='_blank' title='detailed descriptions of terms' href='" + str + "'>" + text + "</a>");
            }
        }
    }
}

function findTracksMdbVarChanged(obj)
{ // Ajax call to repopulate a metadata vals select when mdb var changes
  // This handles the currnet case when 2 vars have the same name (e.g. advanced, files tabs)

    findTracksClearFound();  // Changing values so abandon what has been found

    var newVar = $(obj).val();
    var a = /hgt_mdbVar(\d+)/.exec(obj.name); // NOTE must match METADATA_NAME_PREFIX in hg/hgTracks/searchTracks.c
    if(newVar != undefined && a && a[1]) {
        var num = a[1];
        if ($('#advancedTab').length == 1 && $('#filesTab').length == 1) {
            $("select.mdbVar[name='hgt_mdbVar"+num+"'][value!='"+newVar+"']").val(newVar);
        }
        var cgiVars = "db=" + getDb() +  "&cmd=hgt_mdbVal" + num + "&var=" + newVar;
        if (document.URL.search('hgFileSearch') != -1)
            cgiVars += "&fileSearch=1";
        else
            cgiVars += "&fileSearch=0";

        $.ajax({
                   type: "GET",
                   url: "../cgi-bin/hgApi",
                   data: cgiVars,
                   dataType: 'html',
                   trueSuccess: findTracksHandleNewMdbVals,
                   success: catchErrorOrDispatch,
                   error: errorHandler,
                   cache: true,
                   cmd: "hgt_mdbVal" + num, // NOTE must match METADATA_VALUE_PREFIX in hg/hgTracks/searchTracks.c
                   num: num
               });
    }
    // NOTE: with newJquery, the response is getting a new error (missing ; before statement)
    //       There were also several XML parsing errors.
    // This error is fixed with the addition of "dataType: 'html'," above.
}

function findTracksHandleNewMdbVals(response, status)
{ // Handle ajax response (repopulate a metadata val select)
  // This handles the currnet case when 2 vars have the same name (e.g. advanced, files tabs)

    var td = $('td#' + this.cmd );
    if (td != undefined) {
        var usesFilterBy = ($("select.mdbVar[name='hgt_mdbVar"+this.num+"']").hasClass('noMulti') == false);
        td.empty();
        td.append(response);
        var inp = $(td).find('.mdbVal');
        var tdIsLike = $('td#isLike'+this.num);
        if (inp != undefined && tdIsLike != undefined) {
            if ($(inp).hasClass('freeText')) {
                $(tdIsLike).text('contains');
            } else if ($(inp).hasClass('wildList')
                   || (usesFilterBy && $(inp).hasClass('filterBy'))) {
                $(tdIsLike).text('is among');
            } else {
                $(tdIsLike).text('is');
            }
        }
        $(td).find('.filterBy').each( function(i) { // Do this by 'each' to set noneIsAll individually
            if (usesFilterBy) {
                if (newJQuery)
                    ddcl.setup(this,'noneIsAll');
                else
                    $(this).dropdownchecklist({ firstItemChecksAll: true, noneIsAll: true, maxDropHeight: filterByMaxHeight(this) });
            } else {
                $(this).attr("multiple",false);
                $(this).removeClass('filterBy');
                $(this).show();
            }
        });
    }
    updateMetaDataHelpLinks(this.num);
}

function findTracksMdbValChanged(obj)
{ // Keep all tabs with same selects in sync  TODO: Change from name to id based identification and only have one set of inputs in form
  // This handles the currnet case when 2 vars have the same name (e.g. advanced, files tabs)

    findTracksClearFound();  // Changing values so abandon what has been found

    if ($('#advancedTab').length == 1 && $('#filesTab').length == 1) {
        var newVal = $(obj).val();
        var a = /hgt_mdbVal(\d+)/.exec(obj.name); // NOTE must match METADATA_NAME_PREFIX in hg/hgTracks/searchTracks.c
        if(newVal != undefined && a && a[1]) {
            var num = a[1];
            $("input.mdbVal[name='hgt_mdbVal"+num+"'][value!='"+newVal+"']").val(newVal);
            $("select.mdbVal[name='hgt_mdbVal"+num+"'][value!='"+newVal+"']").each( function (i) {
                $(this).val(newVal);
                if ($(this).hasClass('filterBy')) {
                    $(this).dropdownchecklist("destroy");
                    if (newJQuery)
                        ddcl.setup(this,'noneIsAll');
                    else
                        $(this).dropdownchecklist({ firstItemChecksAll: true, noneIsAll: true, maxDropHeight: filterByMaxHeight(this) });
                }
            });
        }
    }
    //findTracksSearchButtonsEnable(true);
}

function findTracksChangeVis(seenVis)
{ // called by onchange of vis
    var visName = $(seenVis).attr('id');
    var trackName = visName.substring(0,visName.length - "_id".length)
    var hiddenVis = $("input[name='"+trackName+"']");
    var tdb = tdbGetJsonRecord(trackName);
    if($(seenVis).val() != "hide")
        $(hiddenVis).val($(seenVis).val());
    else {
        var selCb = $("input#"+trackName+"_sel_id");
        $(selCb).attr('checked',false);  // Can't set it to [] because that means default setting is used.  However, we are explicitly hiding this!
        $(seenVis).attr('disabled',true);  // Can't set it to [] because that means default setting is used.  However, we are explicitly hiding this!
        var needSel = (tdb.parentTrack != undefined);
        if (needSel) {
            var hiddenSel = $("input[name='"+trackName+"_sel']");
            $(hiddenSel).val('0');  // Can't set it to [] because that means default setting is used.  However, we are explicitly hiding this!
            $(hiddenSel).attr('disabled',false);
        }
        if(tdbIsSubtrack(tdb))
            $(hiddenVis).val("[]");
        else
            $(hiddenVis).val("hide");
    }
    $(hiddenVis).attr('disabled',false);

    $('input.viewBtn').val('View in Browser');
    //warn("Changed "+trackName+" to "+$(hiddenVis).val())
}

function findTracksClickedOne(selCb,justClicked)
{ // called by on click of CB and findTracksCheckAll()
    var selName = $(selCb).attr('id');
    var trackName = selName.substring(0,selName.length - "_sel_id".length)
    var hiddenSel = $("input[name='"+trackName+"_sel']");
    var seenVis = $('select#' + trackName + "_id");
    var hiddenVis = $("input[name='"+trackName+"']");
    var tr = $(selCb).parents('tr.found');
    var tdb = tdbGetJsonRecord(trackName);
    var needSel = (tdb.parentTrack != undefined);
    var shouldPack = tdb.canPack && tdb.kindOfParent == 0; // If parent then not pack but full
    if (shouldPack && tdb.shouldPack != undefined && !tdb.shouldPack)
        shouldPack = false;
    var checked = $(selCb).attr('checked');
    //warn(trackName +" selName:"+selName +" justClicked:"+justClicked +" hiddenSel:"+$(hiddenSel).attr('name') +" seenVis:"+$(seenVis).attr('id') +" hiddenVis:"+$(hiddenVis).attr('name') +" needSel:"+needSel +" shouldPack:"+shouldPack);

    // First deal with seenVis control
    if(checked) {
        $(seenVis).attr('disabled', false);
        if($(seenVis).attr('selectedIndex') == 0) {
            if(shouldPack)
                $(seenVis).attr('selectedIndex',3);  // packed
            else
                $(seenVis).attr('selectedIndex',$(seenVis).attr('length') - 1);
        }
    } else {
        $(seenVis).attr('selectedIndex',0);  // hide
        $(seenVis).attr('disabled', true );
    }

    // Deal with hiddenSel and hiddenVis so that submit does the right thing
    // Setting these requires justClicked OR seen vs. hidden to be different
    var setHiddenInputs = justClicked;
    if(!justClicked) {
        if(needSel)
            setHiddenInputs = (checked != ($(hiddenSel).val() == '1'));
        else if (checked)
            setHiddenInputs = ($(seenVis).val() != $(hiddenVis).val());
        else
            setHiddenInputs = ($(hiddenVis).val() != "hide" && $(hiddenVis).val() != "[]");
    }
    if(setHiddenInputs) {
        if(checked)
            $(hiddenVis).val($(seenVis).val());
        else if(tdbIsSubtrack(tdb))
            $(hiddenVis).val("[]");
        else
            $(hiddenVis).val("hide");
        $(hiddenVis).attr('disabled',false);

        if(needSel) {
            if(checked)
                $(hiddenSel).val('1');
            else
                $(hiddenSel).val('0');  // Can't set it to [] because that means default setting is used.  However, we are explicitly hiding this!
            $(hiddenSel).attr('disabled',false);
        }
    }

    // The "view in browser" button should be enabled/disabled
    if(justClicked) {
        $('input.viewBtn').val('View in Browser');
        findTracksCounts();
    }
}


function findTracksNormalize()
{ // Normalize the page based upon current state of all found tracks
    $('div#found').show()
    var selCbs = $('input.selCb');

    // All should have their vis enabled/disabled appropriately (false means don't update cart)
    $(selCbs).each( function(i) { findTracksClickedOne(this,false); });

    findTracksCounts();
}

function findTracksNormalizeWaitOn()
{ // Put up wait mask then Normalize the page based upon current state of all found tracks
    waitOnFunction( findTracksNormalize );
}

function findTracksCheckAll(check)
{ // Checks/unchecks all found tracks.
    var selCbs = $('input.selCb');
    $(selCbs).attr('checked',check);

    // All should have their vis enabled/disabled appropriately (false means don't update cart)
    $(selCbs).each( function(i) { findTracksClickedOne(this,false); });

    $('input.viewBtn').val('View in Browser');
    findTracksCounts();
    return false;  // Pressing button does nothing more
}

function findTracksCheckAllWithWait(check)
{
    waitOnFunction( findTracksCheckAll, check);
}

function findTracksSearchButtonsEnable(enable)
{ // Displays visible and checked track count
    var searchButton = $('input[name="hgt_tSearch"]'); // NOTE: must match TRACK_SEARCH in hg/inc/searchTracks.h
    var clearButton  = $('input.clear');
    if(enable) {
        $(searchButton).attr('disabled',false);
        $(clearButton).attr('disabled',false);
    } else {
        $(searchButton).attr('disabled',true);
        $(clearButton).attr('disabled',true);
    }
}

function findTracksCounts()
{// Displays visible and checked track count
    var counter = $('.selCbCount');
    if(counter != undefined) {
        var selCbs =  $("input.selCb");
        $(counter).text("("+$(selCbs).filter(":enabled:checked").length + " of " +$(selCbs).length+ " selected)");
    }
}

function findTracksClearFound()
{// Clear found tracks and all input controls
    var found = $('div#found');
    if(found != undefined)
        $(found).remove();
    found = $('div#filesFound');
    if(found != undefined)
        $(found).remove();
    return false;
}

function filterByMaxHeight(multiSel)
{   // Setting a max hieght to scroll dropdownchecklists but
    // multiSel is hidden when this is done, so it's position and height must be estimated.
    var pos = $(multiSel).closest(':visible').offset().top + 30;
    if (pos <= 0)
        pos = 260;

    // Special mess since the filterBy's on non-current tabs will calculate pos badly.
    var tabbed = $('input#currentTab');
    if (tabbed != undefined) {
        var tabDiv = $(multiSel).parents('div#'+ $(tabbed).attr('value'));
        if (tabDiv == null || tabDiv == undefined || $(tabDiv).length == 0) {
            pos = 360;
        }
    }
    var maxHeight = $(window).height() - pos;
    var selHeight = $(multiSel).children().length * 21;
    if (maxHeight > selHeight)
        maxHeight = null;
    //else if($.browser.msie && maxHeight > 500)  // DDCL bug on IE only.
    //    maxHeight = 500;          // Seems to be solved by disbling DDCL's window.resize event for IE

    return maxHeight;
}

function findTracksClear()
{// Clear found tracks and all input controls
    findTracksClearFound();
    $('input[type="text"]').val(''); // This will always be found
    //$('select.mdbVar').attr('selectedIndex',0); // Do we want to set the first two to cell/antibody?
    $('select.mdbVal').attr('selectedIndex',0); // Should be 'Any'
    $('select.filterBy').each( function(i) { // Do this by 'each' to set noneIsAll individually
        //$(this).dropdownchecklist("refresh");  // requires v1.1
        $(this).dropdownchecklist("destroy");
        $(this).show();
        if (newJQuery)
            ddcl.setup(this,'noneIsAll');
        else
            $(this).dropdownchecklist({ firstItemChecksAll: true, noneIsAll: true, maxDropHeight: filterByMaxHeight(this) });
    });

    $('select.groupSearch').attr('selectedIndex',0);
    $('select.typeSearch').attr('selectedIndex',0);
    //findTracksSearchButtonsEnable(false);
    return false;
}

function findTracksSortNow(obj)
{// Called by radio button to sort tracks
    if( $('#sortIt').length == 0 )
        $('form#trackSearch').append("<input TYPE=HIDDEN id='sortIt' name='"+$(obj).attr('name')+"' value='"+$(obj).val()+"'>");
    else
        $('#sortIt').val($(obj).val());

    // How to hold onto selected tracks?
    // There are 2 separate forms.  Scrape named inputs from searchResults form and dup them on trackSearch?
    var inp = $('form#searchResults').find('input:hidden').not(':disabled').not("[name='hgsid']");
    if($(inp).length > 0) {
        $(inp).appendTo('form#trackSearch');
        $('form#trackSearch').attr('method','POST'); // Must be post to avoid url too long  NOTE: probably needs to be post anyway
    }

    $('#searchSubmit').click();
    return true;
}

function findTracksPage(pageVar,startAt)
{// Called by radio button to sort tracks
    var pager = $("input[name='"+pageVar+"']");
    if( $(pager).length == 1)
        $(pager).val(startAt);

    // How to hold onto selected tracks?
    // There are 2 separate forms.  Scrape named inputs from searchResults form and dup them on trackSearch?
    var inp = $('form#searchResults').find('input:hidden').not(':disabled').not("[name='hgsid']");
    if($(inp).length > 0) {
        $(inp).appendTo('form#trackSearch');
        $('form#trackSearch').attr('method','POST'); // Must be post to avoid url too long  NOTE: probably needs to be post anyway
    }

    $('#searchSubmit').click();
    return false;
}

function findTracksConfigureSet(name)
{// Called when configuring a composite or superTrack
    var thisForm =  $('form#searchResults');
    $(thisForm).attr('action',"../cgi-bin/hgTrackUi?hgt_tSearch=Search&g="+name);
    $(thisForm).find('input.viewBtn').click();
}

function findTracksMdbSelectPlusMinus(obj, rowNum)
{ // Now [+][-] mdb var rows with javascript rather than cgi roundtrip
  // Will remove row or clone new one.  Complication is that 'advanced' and 'files' tab duplicate the tables!

 var objId = $(obj).attr('id');
    rowNum = objId.substring(objId.length - 1);
    if ($(obj).val() == '+') {
        var buttons = $("input#plusButton"+rowNum);  // Two tabs may have the exact same buttons!
        if (buttons.length > 0) {
            var table = null;
            $(buttons).each(function (i) {
                var tr = $(this).parents('tr.mdbSelect')[0];
                if (tr != undefined) {
                    table = $(tr).parents('table')[0];
                    if(newJQuery) {
                        var newTr = $(tr).clone();
                        var element = $(newTr).find("select.mdbVar")[0];
                        if (element != undefined)
                            $(element).attr('selectedIndex',-1);

                        element = $(newTr).find("td[id^='hgt_mdbVal']")[0];
                        if (element != undefined)
                            $(element).empty();
                        element = $(newTr).find("td[id^='isLike']")[0];
                        if (element != undefined)
                            $(element).empty();
                        $(tr).after( newTr );
                    } else
                        $(tr).after( $(tr).clone() );
                }
            });
            if (table)
                findTracksMdbSelectRowsNormalize(table); // magic is in this function
            return false;
        }
    } else { // == '-'
        var buttons = $("input#minusButton"+rowNum);  // Two tabs may have the exact same buttons!
        if (buttons.length > 0) {
            var remaining = 0;
            $(buttons).each(function (i) {
                var tr = $(this).parents('tr')[0];
                var table = $(tr).parents('table')[0];
                if (tr != undefined)
                    $(tr).remove();
                remaining = findTracksMdbSelectRowsNormalize(table);  // Must renormalize since 2nd of 3 rows may have been removed
            });
            if (remaining > 0) {
                removeNum = remaining + 1;  // Got to remove the cart vars, though it doesn't matter which as count must not be too many.
                setCartVars( [ "hgt_mdbVar"+removeNum, "hgt_mdbVal"+removeNum ], [ "[]","[]" ] );
            }

            findTracksClearFound();  // Changing values so abandon what has been found
            return false;
        }
    }
    return true;
}

function findTracksMdbSelectRowsNormalize(table)
{ // Called when [-][+] buttons changed the number of mdbSelects in findTracks\
  // Will walk through each row and get the numberings of addressable elements correct.
    //var table = $('table#'+tableId);
    if (table != undefined) {
        var mdbSelectRows = $(table).find('tr.mdbSelect');
        var needMinus = (mdbSelectRows.length > 2);
        $(table).find('tr.mdbSelect').each( function (ix) {
            var rowNum = ix + 1;  // Each [-][+] and mdb var=val pair of selects must be numbered

            // First the [-][+] buttons
            var plusButton = $(this).find("input[value='+']")[0];
            if (plusButton != undefined) {
                $(plusButton).attr('id',"plusButton"+rowNum);
                // rebinding click appears to be not needed and screws up IE as well.
                //$(plusButton).unbind('click')
                //$(plusButton).click(function() { return findTracksMdbSelectPlusMinus($(plusButton), rowNum); });
                var minusButton = $(this).find("input[value='-']")[0];
                if (needMinus) {
                    if (minusButton == undefined) {
                        $(plusButton).before("<input type='button' id='minusButton"+rowNum+"' value='-' style='font-size:.7em;' title='delete this row' onclick='return findTracksMdbSelectPlusMinus(this,"+rowNum+");'>");
                        minusButton = $(this).find("input[value='-']")[0];
                    } else {
                        $(minusButton).attr('id',"minusButton"+rowNum);
                        $(minusButton).unbind('click');
                        $(minusButton).click(function() { return findTracksMdbSelectPlusMinus($(minusButton), rowNum); });
                    }
                } else if (minusButton != undefined)
                    $(minusButton).remove();
            }
            // Now the mdb var=val pair of selects
            var element = $(this).find("select[name^='hgt_mdbVar']")[0];
            if (element != undefined)
                $(element).attr('name','hgt_mdbVar' + rowNum);

            element = $(this).find("select[name^='hgt_mdbVal']")[0];
            if (element != undefined)
                $(element).attr('name','hgt_mdbVal' + rowNum);

            // A couple more things
            element = $(this).find("td[id^='isLike']")[0];
            if (element != undefined)
                $(element).attr('id','isLike' + rowNum);
            element = $(this).find("td[id^='hgt_mdbVal']")[0];
            if (element != undefined)
                $(element).attr('id','hgt_mdbVal' + rowNum);
        });
        return mdbSelectRows.length;
    }
    return 0;
}

function findTracksSwitchTabs(ui)
{ // switching tabs on findTracks page

    if( ui.panel.id == 'simpleTab' && $('div#found').length < 1) {
        setTimeout("$('input#simpleSearch').focus();",20); // delay necessary, since select event not afterSelect event
    } else if( ui.panel.id == 'advancedTab') {
        // Advanced tab has DDCL wigets which were sized badly because the hidden width was unknown
        // delay necessary, since select event not afterSelect event
        setTimeout("ddcl.reinit($('div#advancedTab').find('select.filterBy'),false);",20);
    }
    if( $('div#filesFound').length == 1) {
        if( ui.panel.id == 'filesTab')
            $('div#filesFound').show();
        else
            $('div#filesFound').hide();
    }
    if( $('div#found').length == 1) {
        if( ui.panel.id != 'filesTab')
            $('div#found').show();
        else
            $('div#found').hide();
    }
}

/////////////////////////////////////////////////////
// filterTable functions

function filterTableFilterVar(filter)
{ // returns the var associated with a fileFilter

    if($(filter).hasClass('filterBy') == false)
        return undefined;
    if($(filter).hasClass('filterTable') == false)
        return undefined;

    // Find the var for this filter
    var classes = $(filter).attr("class").split(' ');
    classes = aryRemove(classes,"filterBy","filterTable","noneIsAll");
    if (classes.length > 1 ) {
        warn('Too many classes for filterBy: ' + classes);
        return undefined;
    }
    return classes.pop();
}

/* // This version of filterTable() uses the yieldingIterator methods.
   // These methods and the yieldingIterator were developed because IE was so slow.
   // HOWEVER: IE was sped up by avoiding .add() and .parent(),
   // so I reverted to simpler code but wish to retain this example for
   // future reference of how to deael with slow javascript.

function _filterTableByClassesIterative(args)
{ // Applies a single class filter to a filterTable TRs
  // Called via yieldingIterator
    if (args.curIx >= args.classes.length)
        return 0;

    var tds = $(args.tdsRemaining).filter('.' + args.classes[args.curIx]);
    if (tds.length > 0) {
        if (args.tdsFiltered == null)
            args.tdsFiltered = tds;
        else
            args.tdsFiltered = jQuery.merge( args.tdsFiltered, tds );  // This one takes too long in IE!
    }
    //warnSince("Iterating class:"+args.curIx);
    args.curIx++;
    if (args.curIx >= args.classes.length)
        return 0;
    return 1;
}

function _filterTableByClassesComplete(args)
{ // Continues after filterTableByClassesIterative
  // Called via yieldingIterator
    var filtersStruct = args.filtersStruct;

    //warnSince("Completing classes...");
    if (args.tdsFiltered == null)
        filtersStruct.trsRemaining = null;
    else {
        //filtersStruct.trsRemaining = $(args.tdsFiltered).parent(); // Very slow in IE!!!
        var tds = args.tdsFiltered;
        var trs = [];
        $(tds).each(function (ix) {
            trs[ix] = this.parentNode;
        });
        filtersStruct.trsRemaining = trs;
    }
    //warnSince("Mostly complete classes...");
    filtersStruct.curIx++;
    yieldingIterator(_filterTableIterative,_filterTableComplete,filtersStruct);
    //warnSince("Really complete classes.");
}

function _filterTableIterative(args)
{ // Applies a single filter to a filterTable TRs
  // Called via yieldingIterator

    //warnSince("Filter "+args.curIx+" iterating...");
    if (args.curIx >= args.filters.length)
        return 0;

    var filter = args.filters[args.curIx];

    var classes = $(filter).val();
    if (classes == null || classes.length == 0)
        {
        args.trsRemaining = null;
        return 0; // Nothing selected so exclude all rows
        }

    if(classes[0] != 'All') { // nothing excluded by this filter
        // Get the filter variable
        var filterVar = filterTableFilterVar(filter);
        if (filterVar != undefined) {// No filter variable?!
            if ($.browser.msie) {   // Special for IE, since it takes so long
                var classesStruct = new Object;
                classesStruct.filtersStruct = args;
                classesStruct.classes       = classes;
                classesStruct.curIx         = 0;
                classesStruct.tdsRemaining = $(args.trsRemaining).children('td.' + filterVar);
                classesStruct.tdsFiltered = null;
                yieldingIterator(_filterTableByClassesIterative,_filterTableByClassesComplete,classesStruct);
                return -1; // Stops itteration now, but will be resumed in _filterTableByClassesComplete
            } else {
                var varTds = $(args.trsRemaining).children('td.' + filterVar);
                var filteredTrs = null;
                for(var ix=0;ix<classes.length;ix++) {
                    var tds = $(varTds).filter('.' + classes[ix]);
                    if (tds.length > 0) {
                        var trs = [];
                        $(tds).each(function (ix) {
                            trs[ix] = this.parentNode;
                        });
                        if (filteredTrs == null)
                            filteredTrs = trs; // $(tds).parent('tr'); // parent() takes too long in IE
                        else
                            filteredTrs = jQuery.merge( filteredTrs, trs );// $(tds).parent() );  // takes too long in IE!
                    }
                }
                args.trsRemaining = filteredTrs;
            }
        }
    }
    args.curIx++;
    if (args.curIx >= args.filters.length)
        return 0;
    return 1;
}

function _filterTableComplete(args)
{ // Continuation after all the filters have been applied
  // Called via yieldingIterator

    //warnSince("Completing...");
    //$('tr.filterable').hide();  // <========= This is what is taking so long!
    $('tr.filterable').css('display', 'none');

    if (args.trsRemaining != null) {
        //$(args.trsRemaining).show();
        $(args.trsRemaining).css('display', '');

        // Update count
        var counter = $('.filesCount');
        if(counter != undefined)
            $(counter).text($(args.trsRemaining).length + " / ");
    } else {
        var counter = $('.filesCount');
        if(counter != undefined)
            $(counter).text(0 + " / ");
    }

    var tbody = $( $('tr.filterable')[0] ).parent('tbody.sorting');
    if (tbody != undefined)
         $(tbody).removeClass('sorting');
    //warnSince("Really complete.");
}

function _filterTableYielding()
{ // Called by filter onchange event.  Will show/hide trs based upon all filters
    var trsAll = $('tr.filterable'); // Default all
    if (trsAll.length == 0)
        return undefined;

    // Find all filters
    var filters = $("select.filterBy");
    if (filters.length == 0)
        return undefined;

    var filtersStruct = new Object;
    filtersStruct.filters = filters;
    filtersStruct.curIx = 0;
    filtersStruct.trsRemaining = trsAll;

    yieldingIterator(_filterTableIterative,_filterTableComplete,filtersStruct);
}
*/

function filterTablesApplyOneFilter(filter,remainingTrs)
{ // Applies a single filter to a filterTables TRs
    var classes = $(filter).val();
    if (classes == null || classes.length == 0)
        return null; // Nothing selected so exclude all rows

    if(classes[0] == 'All')
        return remainingTrs;  // nothing excluded by this filter

    // Get the filter variable
    var filterVar = filterTableFilterVar(filter);
    if (filterVar == undefined)
        return null;

    var varTds = $(remainingTrs).children('td.' + filterVar);
    var filteredTrs = null;
    var ix =0;
    for(;ix<classes.length;ix++) {
        var tds = $(varTds).filter('.' + classes[ix]);
        if (tds.length > 0) {
            var trs = [];
            $(tds).each(function (ix) {
                trs[ix] = this.parentNode;
            });

            if (filteredTrs == null)
                filteredTrs = trs;
            else
                filteredTrs = jQuery.merge( filteredTrs, trs );  // This one takes too long in IE!
        }
    }
    return filteredTrs;
}

function filterTablesTrsSurviving(filterClass)
// returns a list of trs that satisfy all filters
// If defined, will exclude filter identified by filterClass
{
    // find all filterable table rows
    var showTrs = $('tr.filterable'); // Default all
    if (showTrs.length == 0)
        return undefined;

    // Find all filters
    var filters = $("select.filterBy");
    if (filters.length == 0)
        return undefined;

    // Exclude one if requested.
    if (filterClass != undefined && filterClass.length > 0)
        filters = $(filters).not('.' + filterClass);

    for(var ix =0;showTrs != null && ix < filters.length;ix++) {
        showTrs = filterTablesApplyOneFilter(filters[ix],showTrs)
    }
    return showTrs;
}

function _filterTable()
{ // Called by filter onchange event.  Will show/hide trs based upon all filters
    var showTrs = filterTablesTrsSurviving();
    //$('tr.filterable').hide();  // <========= This is what is taking so long!
    $('tr.filterable').css('display', 'none')

    if (showTrs != undefined && showTrs.length > 0) {
        //$(showTrs).show();
        $(showTrs).css('display', '');

        // Update count
        var counter = $('.filesCount');
        if(counter != undefined)
            $(counter).text($(showTrs).length + " / ");
    } else {
        var counter = $('.filesCount');
        if(counter != undefined)
            $(counter).text(0 + " / ");
    }

    var tbody = $( $('tr.filterable')[0] ).parent('tbody.sorting');
    if (tbody != undefined)
         $(tbody).removeClass('sorting');
}

function filterTableTrigger()
{ // Called by filter onchange event.  Will show/hide trs based upon all filters
    var tbody = $( $('tr.filterable')[0] ).parent('tbody');
    if (tbody != undefined)
         $(tbody).addClass('sorting');

    waitOnFunction(_filterTable);
}

function filterTableDone(event)
{ // Called by custom 'done' event
    event.stopImmediatePropagation();
    $(this).unbind( event );
    filterTableTrigger();
}

function filterTable(multiSelect)
{ // Called by filter onchange event.  Will show/hide trs based upon all filters
    // IE takes tooo long, so this should be called only when leaving the filterBy box
    if ( $('tr.filterable').length > 300) {
        //if ($.browser.msie) { // IE takes tooo long, so this should be called only when leaving the filterBy box
            $(multiSelect).one('done',filterTableDone);
            return;
        //}
    } else
        filterTableTrigger();
}

function filterTableExcludeOptions(filter)
{ // bound to 'click' event inside ui.dorpdownchecklist.js.
  // Will mark all options in one filterBy box that are inconsistent with the current
  // selections in other filterBy boxes.  Mark with class ".excluded"

    // Compare to the list of all trs
    var allTrs = $('tr.filterable'); // Default all
    if (allTrs.length == 0)
        return false;

    if ($.browser.msie && $(allTrs).length > 300) // IE takes tooo long, so this should be called only when leaving the filterBy box
        return false;

    // Find the var for this filter
    var filterVar = filterTableFilterVar(filter);
    if (filterVar == undefined)
        return false;

    // Look at list of visible trs.
    var visibleTrs = filterTablesTrsSurviving(filterVar);
    if (visibleTrs == undefined)
        return false;

    //if ($.browser.msie && $(visibleTrs).length > 300) // IE takes tooo long, so this should be called only when leaving the filterBy box
    //    return false;

    if (allTrs.length == visibleTrs.length) {
        $(filter).children('option.excluded').removeClass('excluded');   // remove .excluded" from all
        return true;  // Nothing more to do.  All are already excluded
    }

    // Find the tds that belong to this var
    var tds = $(visibleTrs).find('td.'+filterVar);
    if (tds.length == 0) {
        $(filter).children('option').addClass('excluded');   // add .excluded" to all
        return true;
    }

    // Find the val classes
    var classes = new Array();
    $(tds).each(function (i) {
        var someClass = $(this).attr("class").split(' ');
        someClass = aryRemove(someClass,filterVar);
        var val = someClass.pop()
        if (aryFind(classes,val) == -1)
            classes.push(val);
    });
    if (classes.length == 0) {
        $(filter).children('option').addClass('excluded');   // add .excluded" to all
        return true;
    }

    // Find all options with those classes
    $(filter).children('option').each(function (i) {
        if (aryFind(classes,$(this).val()) != -1)
            $(this).removeClass('excluded'); // remove .excluded from matching
        else
            $(this).addClass('excluded');    // add .excluded" to non-matching
    });

    // If all options except "all" are included then all should nt be excluded
    var excluded = $(filter).children('option.excluded');
    if (excluded.length == 1) {
        var text = $(excluded[0]).text();
        if (text == 'All' || text == 'Any')
            $(excluded[0]).removeClass('excluded');
    }
    return true;
}

