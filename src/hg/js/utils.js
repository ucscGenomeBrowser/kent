// Utility JavaScript

// "use strict";

// Don't complain about line break before '||' etc:
/* jshint -W014 */
/* jshint -W087 */
/* jshint esnext: true */

var debug = false;

/* Support these formats for range specifiers.  Note the ()'s around chrom,
 * start and end portions for substring retrieval: */
var canonicalRangeExp = /^[\s]*([\w._#-]+)[\s]*:[\s]*([-0-9,]+)[\s]*[-_][\s]*([0-9,]+)[\s]*$/;
var gbrowserRangeExp =  /^[\s]*([\w._#-]+)[\s]*:[\s]*([0-9,]+)[\s]*\.\.[\s]*([0-9,]+)[\s]*$/;
var lengthRangeExp =    /^[\s]*([\w._#-]+)[\s]*:[\s]*([0-9,]+)[\s]*\+[\s]*([0-9,]+)[\s]*$/;
var bedRangeExp =       /^[\s]*([\w._#-]+)[\s]+([0-9,]+)[\s]+([0-9,]+)[\s]*$/;
var sqlRangeExp =       /^[\s]*([\w._#-]+)[\s]*\|[\s]*([0-9,]+)[\s]*\|[\s]*([0-9,]+)[\s]*$/;
var singleBaseExp =     /^[\s]*([\w._#-]+)[\s]*:[\s]*([0-9,]+)[\s]*$/;

function copyToClipboard(ev) {
    /* copy a piece of text to clipboard. event.target is some DIV or SVG that is an icon. 
     * The attribute data-target of this element is the ID of the element that contains the text to copy. 
     * The text is either in the attribute data-copy or the innerText.
     * see C function printCopyToClipboardButton(iconId, targetId);
     * */
     
    ev.preventDefault();

    var buttonEl = ev.target.closest("button"); // user can click SVG or BUTTON element

    var targetId = buttonEl.getAttribute("data-target");
    if (targetId===null)
        targetId = ev.target.parentNode.getAttribute("data-target");
    var textEl = document.getElementById(targetId);
    var text = textEl.getAttribute("data-copy");
    if (text===null)
        text = textEl.innerText;

    var textArea = document.createElement("textarea");
    textArea.value = text;
    // Avoid scrolling to bottom
    textArea.style.top = "0";
    textArea.style.left = "0";
    textArea.style.position = "fixed";
    document.body.appendChild(textArea);
    textArea.focus();
    textArea.select();
    document.execCommand('copy');
    document.body.removeChild(textArea);
    buttonEl.innerHTML = 'Copied';
    ev.preventDefault();
}

function cfgPageOnVisChange(ev) {
    /* configuration page event listener when user changes visibility in dropdown */
    if (ev.target.value === 'hide')
        ev.target.classList.replace("normalText", "hiddenText");
    else
        ev.target.classList.replace("hiddenText", "normalText");
}

function cfgPageAddListeners() {
    /* add event listener to dropdowns */
    var els = document.querySelectorAll(".trackVis");
    for (var i=0; i < els.length; i++) {
        var el = els[i];
        el.addEventListener("change", cfgPageOnVisChange );
    }
}

// Google Analytics helper functions to send events, see src/hg/lib/googleAnalytics.c

function gaOnButtonClick(ev) {
/* user clicked a button: send event to GA, then execute the old handler */
    var button = ev.currentTarget;
    var buttonName = button.name;
    if (buttonName==="")
        buttonName = button.id;
    if (buttonName==="")
        buttonName = button.value;
    // add the original label, makes logs a lot easier to read
    buttonName = button.value + " / "+buttonName;

    ga('send', 'event', 'buttonClick', buttonName);
    if (button.oldOnClick) // most buttons did not have an onclick function at all (the default click is a listener)
    {
        button.oldOnClick(ev);
    }
}

function gaTrackButtons() {
  /* replace the click handler on all buttons with one the sends a GA event first, then handles the click */
  if (!window.ga || ga.loaded) // When using an Adblocker, the ga object does not exist
      return;
  var buttons = document.querySelectorAll('input[type=submit],input[type=button]');
  var isFF = theClient.isFirefox();
  for (var i = 0; i < buttons.length; i++) {
       var b = buttons[i];
       // some old Firefox versions <= 78 do not allow submit buttons to also send AJAX requests
       // so Zoom/Move buttons are skipped in FF (even though newer versions allow it again, certainly FF >= 90)
       if (isFF && b.name.match(/\.out|\.in|\.left|\.right/))
           continue;
       b.oldOnClick = b.onclick;
       b.onclick = gaOnButtonClick; // addEventHandler would not work here, the default click stops propagation.
  }
}
// end Google Analytics helper functions

function clickIt(obj,state,force)
{
// calls click() for an object, and click();click() if force
    if (obj.checked !== state) {
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
            if (ele.checked !== state)
                ele.click();  // Forces onclick() javascript to run
    }
}

function setCheckBoxesThatContain(nameOrId, state, force, sub1)
{
// Set all checkboxes which contain 1 or more given substrings in NAME or ID to state boolean
// First substring: must begin with it; 2 subs: beg and end; 3: begin, middle and end.
// This can force the 'onclick() js of the checkbox, even if it is already in the state
    if (debug)
        alert("setCheckBoxesContains is about to set the checkBoxes to "+state);
    var list;
    if (arguments.length === 4)
        list = inputArrayThatMatches("checkbox",nameOrId,sub1,"");
    else if (arguments.length === 5)
        list = inputArrayThatMatches("checkbox",nameOrId,sub1,arguments[4]);
    else if (arguments.length === 6)
        list = inputArrayThatMatches("checkbox",nameOrId,sub1,arguments[4],arguments[5]);
    for (var ix=0;ix<list.length;ix++) {
        clickIt(list[ix],state,force);
    }
    return true;
}

function inputArrayThatMatches(inpType,nameOrId,prefix,suffix)
{
    // returns an array of input controls that match the criteria
    var found = [];
    var fIx = 0;
    if (document.getElementsByTagName)
    {
        var list;
        if (inpType === 'select')
            list = document.getElementsByTagName('select');
        else
            list = document.getElementsByTagName('input');
        for (var ix=0;ix<list.length;ix++) {
            var ele = list[ix];
            if (inpType.length > 0 && inpType !== 'select' && ele.type !== inpType)
                continue;
            var identifier = ele.name;
            if (nameOrId.search(/id/i) !== -1)
                identifier = ele.id;
            var failed = false;
            if (prefix.length > 0)
                failed = (identifier.indexOf(prefix) !== 0);
            if (!failed && suffix.length > 0)
                failed = (identifier.lastIndexOf(suffix) !== (identifier.length - suffix.length));
            if (!failed) {
                for (var aIx=4;aIx<arguments.length;aIx++) {
                    if (identifier.indexOf(arguments[aIx]) === -1) {
                        failed = true;
                        break;
                    }
                }
            }
            if (!failed) {
                found[fIx] = ele;
                fIx++;
            }
        }
    } else {
        // NS 4.x - I gave up trying to get this to work.
        if (debugLevel>2)
           alert("arrayOfInputsThatMatch is unimplemented for this browser");
    }
    return found;
}


function normed(thing)
{   // RETURNS undefined, the lone member of the set or the full set if more than one member.
    // Used for normalizing returns from jquery DOM selects (e.g. $('tr.track').children('td.data'))
    // jquery returns an "array like 'object'" with 0 or more entries.  
    // May be used on non-jquery objects and will reduce single element arrays to the element.
    // Use this to treat 0 entries the same as undefined and 1 entry as the item itself
    if (typeof(thing) === 'undefined' || thing === null
    ||  (thing.length !== undefined && thing.length === 0)  // Empty array (or 'array like object')
    ||  ($.isPlainObject(thing) && $.isEmptyObject(thing))) // Empty simple object
        return undefined;
    if (thing.length && thing.length === 1 && typeof thing !== 'string') // string is overkill
        return thing[0]; // Container of one item should return the item itself.
    return thing;
}

var theClient = (function() {
// Object that detects client browser if requested              
    
    // - - - - - Private variables and/or methods - - - - - 
    var ieVersion = null;
    var browserNamed = null;
    
    // - - - - - Public methods - - - - - 
    return { // returns an object with public methods
    
        getIeVersion: function ()
        { // Adapted from the web: stackOverflow.com answer by Joachim Isaksson
            if (ieVersion === null) {
                ieVersion = -1.0;
                var re = null;
                if (navigator.appName === 'Microsoft Internet Explorer') {
                    browserNamed = 'MSIE';
                    re = new RegExp("MSIE ([0-9]{1,}[.0-9]{0,})");
                    if (re.exec(navigator.userAgent) !== null)
                        ieVersion = parseFloat( RegExp.$1 );
                } else if (navigator.appName === 'Netscape') {
                    re = new RegExp("Trident/.*rv:([0-9]{1,}[.0-9]{0,})");
                    if (re.exec(navigator.userAgent) !== null) {
                        browserNamed = 'MSIE';
                        ieVersion = parseFloat( RegExp.$1 );
                    }
                }
            }
            return ieVersion;
        },
        
        isIePre11: function ()
        {
            var ieVersion = theClient.getIeVersion();
            if ( ieVersion !== -1.0 && ieVersion < 11.0 )
                return true;
            return false;
        },
        
        isIePost11: function ()
        {
            if (theClient.getIeVersion() >= 11.0 )
                return true;
            return false;
        },
        
        isIe: function ()
        {
            if (theClient.getIeVersion() === -1.0 )
                return false;
            return true;
        },
        
        isChrome: function () 
        {
            if (browserNamed !== null)
                return (browserNamed === 'Chrome');
            
            if (navigator.userAgent.indexOf("Chrome") !== -1) {
                browserNamed = 'Chrome';
                return true;
            }
            return false;
        },
        
        isNetscape: function () 
        {   // IE sometimes mimics netscape
            if (browserNamed !== null)
                return (browserNamed === 'Netscape');
        
            if (navigator.appName === 'Netscape'
            &&  navigator.userAgent.indexOf("Trident") === -1) {
                browserNamed = 'Netscape';
                return true;
            }
            return false;
        },
        
        isFirefox: function () 
        {
            if (browserNamed !== null)
                return (browserNamed === 'FF');
            
            if (navigator.userAgent.indexOf("Firefox") !== -1) {
                browserNamed = 'FF';
                return true;
            }
            return false;
        },
        
        isSafari: function () 
        {   // Chrome sometimes mimics Safari
            if (browserNamed !== null)
                return (browserNamed === 'Safari');
            
            if (navigator.userAgent.indexOf("Safari") !== -1 
            &&  navigator.userAgent.indexOf('Chrome') === -1) {
                browserNamed = 'Safari';
                return true;
            }
            return false;
        },
        
        isOpera: function () 
        {
            if (browserNamed !== null)
                return (browserNamed === 'Opera');
            
            if (navigator.userAgent.indexOf("Presto") !== -1) {
                browserNamed = 'Opera';
                return true;
            }
            return false;
        },
        
        nameIs: function () 
        {   // simple enough, this needs no comment!
            if (browserNamed === null
            &&  theClient.isChrome() === false   // Looking in the order of popularity
            &&  theClient.isFirefox() === false
            &&  theClient.isIe()       === false
            &&  theClient.isSafari()    === false
            &&  theClient.isOpera()      === false
            &&  theClient.isNetscape()    === false)
                browserNamed = navigator.appName; // Don't know what else to look for.
            
            return browserNamed;
        }

    };
}());

function waitCursor(obj)  // DEAD CODE?
{
    //document.body.style.cursor="wait"
    obj.style.cursor="wait";
}

function endWaitCursor(obj)  // DEAD CODE?
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
    if (arguments.length === 1) {
          strHref = window.location.href;
          strParamName = arguments[0];
    } else {
          strHref = arguments[0];
          strParamName = arguments[1];
    }
    if ( strHref.indexOf("?") > -1) {
      var strQueryString = strHref.substr(strHref.indexOf("?")).toLowerCase();
      var aQueryString = strQueryString.split("&");
      for (var iParam = 0; iParam < aQueryString.length; iParam++) {
         if (aQueryString[iParam].indexOf(strParamName.toLowerCase() + "=") > -1) {
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
    var inp = normed($(theForm).find("input[name='"+aName+"']:last"));
    if (inp) {
        $(inp).val(aValue);
        inp.disabled = false;
    } else
        makeHiddenInput(theForm,aName,aValue);
}

function disableNamedVariable(theForm,aName)
{   // Store a value to a named input.  Will make the input if necessary
    var inp = normed($(theForm).find("input[name='"+aName+"']:last"));
    if (inp)
        inp.disabled = true;
}

function parseUrlAndUpdateVars(theForm,href)  // DEAD CODE?
{   // Parses the URL and converts GET vals to POST vals
    var url = href;
    var extraIx = url.indexOf("?");
    if (extraIx > 0) {
        var extra = url.substring(extraIx+1);
        url = url.substring(0,extraIx);
        // now extra must be repeatedly broken into name=var
        extraIx = extra.indexOf("=");
        for (; extraIx > 0;extraIx = extra.indexOf("=")) {
            var aValue;
            var aName = extra.substring(0,extraIx);
            var endIx = extra.indexOf("&");
            if (endIx>0) {
                aValue = extra.substring(extraIx+1,endIx);
                extra  = extra.substring(endIx+1);
            } else {
                aValue = extra.substring(extraIx+1);
                extra  = "";
            }
            if (aName.length > 0 && aValue.length > 0)
                updateOrMakeNamedVariable(theForm,aName,aValue);
        }
    }
    return url;
}

function postTheForm(formName,href)
{   // posts the form with a passed in href
    var goodForm = normed($("form[name='"+formName+"']"));
    if (goodForm) {
        if (href && href.length > 0) {
            $(goodForm).attr('action',href); // just attach the straight href
        }
        $(goodForm).attr('method','POST');

        $(goodForm).submit();
    }
    return false; // Meaning do not continue with anything else
}
function setVarAndPostForm(aName,aValue,formName)
{   // Sets a specific variable then posts
    var goodForm = normed($("form[name='"+formName+"']"));
    if (goodForm) {
        updateOrMakeNamedVariable(goodForm,aName,aValue);
    }
    return postTheForm(formName,window.location.href);
}

// json help routines
function tdbGetJsonRecord(trackName)  { return hgTracks.trackDb[trackName]; }
// NOTE: These must jive with tdbKindOfParent() and tdbKindOfChild() in trackDb.h
function tdbIsFolder(tdb)             { return (tdb.kindOfParent === 1); } 
function tdbIsComposite(tdb)          { return (tdb.kindOfParent === 2); }
function tdbIsMultiTrack(tdb)         { return (tdb.kindOfParent === 3); }
function tdbIsView(tdb)               { return (tdb.kindOfParent === 4); } // Don't expect to use
function tdbIsContainer(tdb)          { return (tdb.kindOfParent === 2 || tdb.kindOfParent === 3); }
function tdbIsLeaf(tdb)               { return (tdb.kindOfParent === 0); }
function tdbIsFolderContent(tdb)      { return (tdb.kindOfChild  === 1); }
function tdbIsCompositeSubtrack(tdb)  { return (tdb.kindOfChild  === 2); }
function tdbIsMultiTrackSubtrack(tdb) { return (tdb.kindOfChild  === 3); }
function tdbIsSubtrack(tdb)           { return (tdb.kindOfChild  === 2 || tdb.kindOfChild === 3); }
function tdbHasParent(tdb)            { return (tdb.kindOfChild  !== 0 && tdb.parentTrack); }

function cartHideAnyTrack (id, cartVars, cartVals) {
    /* set the right cart variables to hide a track, changes cartVars and cartVals */
    var rec = hgTracks.trackDb[id];
    if (tdbIsSubtrack(rec)) {
        cartVars.push(id);
        cartVals.push('[]');

        cartVars.push(id+"_sel");
        cartVals.push(0);
    } else if (tdbIsFolderContent(rec)) {
        // supertrack children need to have _sel set to trigger superttrack reshaping
        cartVars.push(id);
        cartVals.push('hide');

        cartVars.push(id+"_sel");
        cartVals.push(0);
    } else {
        // normal, top-level track
        cartVars.push(id);
        cartVals.push('hide');
    }
}

function tdbCountChildren(trackDb, parentType) {
    /* return dicts with count of children, uses either the .parentTrack or .topParent trackDb attributes */
    var familySize = {};
    var families = {};
    // sort trackDb into object topParent -> count of children
    for (var trackName of Object.keys(hgTracks.trackDb)) {
        var rec = hgTracks.trackDb[trackName];
        // when looking at superTracks, only look at those children that are in superTracks
        if (rec[parentType]===undefined) {
            continue; // ignore tracks without parents
        }

        var parentTrack = rec[parentType];
        if (!familySize.hasOwnProperty(parentTrack)) {
            familySize[parentTrack] = 0;
            families[parentTrack] = [];
        }
        familySize[parentTrack]++;
        families[parentTrack].push(trackName);
    }

    var ret = {};
    ret.familySize = familySize;
    ret.families = families;
    return ret;
}

function tdbFindChildless(trackDb, delTracks) {
    /* Find parents that have no children left anymore in hgTracks.trackDb if you remove delTracks.
     * return obj with o.loneParents as array of [parent, array of children] , and o.others as an array of all other tracks
     * The caller needs the children names, as we want to hide the children with Javascript right away */

    // This functions uses a somewhat weird strategy: it counts the children of the composites, also counts the children of superTracks,
    // and compares both at the end. There may be a better strategy but our data structures are so strange that I didn't know what
    // else to do.
    var parentType = "parentTrack";
    others = [];

    var compLinks = tdbCountChildren(trackDb, "parentTrack"); // look only at direct parents: superTracks and composites
    var topLinks = tdbCountChildren(trackDb, "topParent");  // look at top parents, so only superTracks

    // decrease the parent's count for each track to delete
    for (var delTrack of delTracks) {
        var tdbRec = hgTracks.trackDb[delTrack];
        let parentName = tdbRec.parentTrack;
        if (parentName)
            compLinks.familySize[parentName]--;

        parentName = tdbRec.topParent;
        if (parentName)
            topLinks.familySize[parentName]--;
    }

    // for the parents of deleted tracks with a count of 0, create an array of [parentName, children]
    var loneParents = [];
    var doneParents = [];
    for (delTrack of delTracks) {
        var parentName = hgTracks.trackDb[delTrack].parentTrack;
        var topParentName = hgTracks.trackDb[delTrack].topParent;
        if (parentName) {
            // hide a superTrack parent only if that superTrack does not have any other tracks open
            // This addresses the case where you hide the last child of a composite under a superTrack
            // E.g. All Gencode / Gencode V46 or FANTOM5/Max count 
            if (compLinks.familySize[parentName]===0 && topLinks.familySize[topParentName]===0) {
                if (!doneParents.includes(parentName)) {
                    loneParents.push([topParentName, compLinks.families[parentName]]);
                    doneParents.push(topParentName);
                }
            } else
                for (var child of compLinks.families[parentName])
                    // do not hide tracks of lone parents that are not in delTracks
                    if (delTracks.includes(child))
                        others.push(child);
        } else {
            others.push(delTrack);
        }
    }

    o = {};
    o.loneParents = loneParents;
    o.others = Array.from(new Set(others)); // remove duplicates (rare bug in the above)
    return o;
}

function aryFind(ary,val)
{// returns the index of a value on the array or -1;
    for (var ix=0; ix < ary.length; ix++) {
        if (ary[ix] === val) {
            return ix;
        }
    }
    return -1;
}

function aryRemove(ary,vals)
{ // removes one or more variables that are found in the array
    for (var vIx=0; vIx < vals.length; vIx++) {
        var ix = aryFind(ary,vals[vIx]);
        if (ix !== -1)
            ary.splice(ix,1);
    }
    return ary;
}

function arysToObj(names,values)
{   // Make hash type obj with two parallel arrays.
    var obj = {};
    for (var ix=0; ix < names.length; ix++) {
        obj[names[ix]] = values[ix]; 
    }
    return obj;
}

function objNotEmpty(obj)
{   // returns true on non empty object.  Obj should pass $.isPlainObject()
    if ($.isPlainObject(obj) === false)
        warn("Only use plain js objects in objNotEmpty()");
    return ($.isEmptyObject(obj) === false);
}

function objKeyCount(obj)
{   // returns number of keys in object.
    if (!Object.keys) {
        var count = 0;
        for (var key in obj) {
            count++;
        }
        return count;
    } else
        return Object.keys(obj).length;
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
    if (title.length === 0)
        title = "Value";
    var popup=( theClient.isIePre11() === false );
    for (;;) {
        if ((obj.value === undefined || obj.value === null || obj.value === "") 
        &&  isInteger(obj.defaultValue))
            obj.value = obj.defaultValue;
        if (!isInteger(obj.value)) {
            if (popup) {
                obj.value = prompt(title +" is invalid.\nMust be an integer.",obj.value);
                continue;
            } else {
                alert(title +" of '"+obj.value +"' is invalid.\nMust be an integer.");
                obj.value = obj.defaultValue;
                return false;
            }
        }
        var val = parseInt(obj.value);
        if (isInteger(min) && isInteger(max)) {
            if (val < rangeMin || val > rangeMax) {
                if (popup) {
                    obj.value = prompt(title +" is invalid.\nMust be between "+rangeMin+
                                                                   " and "+rangeMax+".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be between "+
                                                                     rangeMin+" and "+rangeMax+".");
                    obj.value = obj.defaultValue;
                    return false;
                }
            }
        } else if (isInteger(min)) {
            if (val < rangeMin) {
                if (popup) {
                    obj.value = prompt(title +" is invalid.\nMust be no less than "+
                                                                           rangeMin+".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be no less than "+
                                                                                      rangeMin+".");
                    obj.value = obj.defaultValue;
                    return false;
                }
            }
        } else if (isInteger(max)) {
            if (val > rangeMax) {
                if (popup) {
                    obj.value = prompt(title +" is invalid.\nMust be no greater than "+
                                                                           rangeMax+".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be no greater than "+
                                                                                      rangeMax+".");
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
    if (title.length === 0)
        title = "Value";
    var popup=( theClient.isIePre11() === false );
    for (;;) {
        if ((obj.value === undefined || obj.value === null || obj.value === "") 
        &&  isFloat(obj.defaultValue))
            obj.value = obj.defaultValue;
        if (!isFloat(obj.value)) {
            if (popup) {
                obj.value = prompt(title +" is invalid.\nMust be a number.",obj.value);
                continue;
            } else {
                alert(title +" of '"+obj.value +"' is invalid.\nMust be a number."); // try a prompt box!
                obj.value = obj.defaultValue;
                return false;
            }
        }
        var val = parseFloat(obj.value);
        if (isFloat(min) && isFloat(max)) {
            if (val < rangeMin || val > rangeMax) {
                if (popup) {
                    obj.value = prompt(title +" is invalid.\nMust be between "+rangeMin+" and "+
                                                                           rangeMax+".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be between "+rangeMin+
                                                                              " and "+rangeMax+".");
                    obj.value = obj.defaultValue;
                    return false;
                }
            }
        } else if (isFloat(min)) {
            if (val < rangeMin) {
                if (popup) {
                    obj.value = prompt(title +" is invalid.\nMust be no less than "+rangeMin+
                                                                                   ".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be no less than "+
                                                                                      rangeMin+".");
                    obj.value = obj.defaultValue;
                    return false;
                }
            }
        } else if (isFloat(max)) {
            if (val > rangeMax) {
                if (popup) {
                    obj.value = prompt(title +" is invalid.\nMust be no greater than "+
                                                                           rangeMax+".",obj.value);
                    continue;
                } else {
                    alert(title +" of '"+obj.value +"' is invalid.\nMust be no greater than "+
                                                                                      rangeMax+".");
                    obj.value = obj.defaultValue;
                    return false;
                }
            }
        }
        return true;
    }
}

function validateLabel(label)
{   // returns true if label is valid in trackDb as short or long label

    var regexp = /^[a-z][ a-z0-9/'!\$()*,\-.:;<=>?@\[\]^_`{|}~]*$/i;
    if (regexp.test(label)) {
        return true;
    } else {
        alert(label + " is an invalid label. The first character must be alphabetical and the rest of the string be alphanumeric or the following puncuation ~`!@$/^*.()_-=[{]}?|;:'<,>");
        return false;
    }
}
function validateUrl(url)
{   // returns true if url is a valid url, otherwise returns false and shows an alert

    // I got this regexp from http://stackoverflow.com/questions/1303872/url-validation-using-javascript
    var regexp = /^(https?|ftp|gs|s3|drs):\/\/(((([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&amp;'\(\)\*\+,;=]|:)*@)?(((\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5])\.(\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5])\.(\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5])\.(\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5]))|((([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))\.)+(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))\.?)(:\d*)?)(\/((([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&amp;'\(\)\*\+,;=]|:|@)+(\/(([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&amp;'\(\)\*\+,;=]|:|@)*)*)?)?(\?((([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&amp;'\(\)\*\+,;=]|:|@)|[\uE000-\uF8FF]|\/|\?)*)?(\#((([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&amp;'\(\)\*\+,;=]|:|@)|\/|\?)*)?$/i;
    if (regexp.test(url)) {
        return true;
    } else {
        alert(url + " is an invalid url");
        return false;
    }
}

function metadataIsVisible(trackName)
{
    var divit = normed($("#div_"+trackName+"_meta"));
    if (!divit)
        return false;
    return ($(divit).css('display') !== 'none');
}

function metadataShowHide(trackName,showLonglabel,showShortLabel)
{
// Will show subtrack specific configuration controls
// Config controls not matching name will be hidden
    var divit = normed($("#div_"+trackName+"_meta"));
    if (!divit)
        return false;
    var img = normed($(divit).prev('a').find("img"));
    if (img) {
        if ($(divit).css('display') === 'none')
            $(img).attr('src','../images/upBlue.png');
        else
            $(img).attr('src','../images/downBlue.png');
    }
    if ($(divit).css('display') === 'none') {
        if (typeof(subCfg) === "object") {// subCfg.js file included?
            var cfg = normed($("#div_cfg_"+trackName));
            if (cfg)   // Hide any configuration when opening metadata
                $(cfg).hide();
        }

    }
    var tr = $(divit).parents('tr');
    if (tr.length > 0) {
        tr = tr[0];
        var bgClass = null;
        var classes = $( tr ).attr("class").split(" ");
        for (var ix=0;ix<classes.length;ix++) {
            if (classes[ix].substring(0,'bgLevel'.length) === 'bgLevel')
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
    var hidden = normed($("input[name='"+hiddenPrefix+"_"+prefix+"_close']"));
    if (button && hidden) {
        var newVal = -1;
        if (arguments.length > 5)
            newVal = arguments[5] ? 0 : 1;
        var oldSrc = $(button).attr("src");
        if (oldSrc && oldSrc.length > 0) {
            // Old img version of the toggleButton
            if (newVal === -1)
                newVal = oldSrc.indexOf("/remove") > 0 ? 1 : 0;
            if (newVal === 1)
                $(button).attr("src", oldSrc.replace("/remove", "/add") );
            else
                $(button).attr("src", oldSrc.replace("/add", "/remove") );
        } else {
            // new BUTTONS_BY_CSS
            if (newVal === -1) {
                oldSrc = $(button).text();
                if (oldSrc && oldSrc.length === 1)
                    newVal = (oldSrc === '+') ? 0 : 1;
                else {
                    warn("Uninterpretable toggleButton!");
                    newVal = 0;
                }
            }
            if (newVal === 1)
                $(button).text('+');
            else
                $(button).text('-');
        }
        var contents = $("tr[id^='"+prefix+"-']");
        if(newVal === 1) {
            $(button).attr('title', 'Expand this '+titleDesc);
            contents.hide();
        } else {
            $(button).attr('title', 'Collapse this '+titleDesc);
            contents.show().trigger('show');
        }
        $(hidden).val(newVal);
        if (doAjax) {
            setCartVar(hiddenPrefix+"_"+prefix+"_close", newVal);
        }
        retval = false;
    }
    return retval;
}

function getNonce(debug)
{   // Gets nonce value from page meta header
var content = $("meta[http-equiv='Content-Security-Policy']").attr("content");
if (!content)
    return "";
// parse nonce like 'nonce-JDPiW8odQkiav4UCeXsa34ElFm7o'
var sectionBegin = "'nonce-";
var sectionEnd   = "'";
var ix = content.indexOf(sectionBegin);
if (ix < 0)
    return "";
content = content.substring(ix+sectionBegin.length);
ix = content.indexOf(sectionEnd);
if (ix < 0)
    return "";
content = content.substring(0,ix);
if (debug)
    alert('page nonce='+content);
return content;
}

function notifBoxShow(cgiName, keyName) {
    /* move the notification bar div under '#TrackHeaderForm' */
    let lsKey = cgiName + "_" + keyName;
    if (localStorage.getItem(lsKey))
        return;
    var notifEl = document.getElementById(lsKey + "notifBox");
    if (!notifEl) {
        // missing call to setup function (ie generated server side like a udcTimeout message)
        notifBoxSetup(cgiName, keyName);
    }
    // TODO: make a generic element for positioning this
    var parentEl = document.getElementById('TrackHeaderForm');
    if (parentEl) {
        parentEl.appendChild(notifEl);
        notifEl.style.display = 'block';
    }
}

function notifBoxSetup(cgiName, keyName, msg, skipCloseButton) {
/* Create a notification box if one hasn't been created, and
 * add msg to the list of shown notifications.
 * cgiName.keyName will be saved to localStorage in order to show
 * or hide this notification.
 * Must call notifBoxShow() in order to display the notification */
    lsKey = cgiName + "_" + keyName;
    if (localStorage.getItem(lsKey))
        return;
    let alreadyPresent = false;
    let notifBox = document.getElementById(lsKey+"notifBox");
    if (notifBox) {
        alreadyPreset = true;
        if (msg) {
            notifBox.innerHTML += "<br>" + msg;
        }
    } else {
        notifBox = document.createElement("div");
        notifBox.className = "notifBox";
        notifBox.style.display = "none";
        notifBox.style.width = "90%";
        notifBox.style.marginLeft = "100px";
        notifBox.id = lsKey+"notifBox";
        if (msg) {
            notifBox.innerHTML = msg;
        }
    }
    var closeHtml = "<button id='" + lsKey + "notifyHide'>Close</button>&nbsp;";
    var buttonStyles = "text-align: center";
    // XX code review: The close button does not sense at all, why not just always remove it?
    if (skipCloseButton===true) {
        closeHtml = "";
        buttonStyles += "; display: inline; margin-left: 20px";
    }

    notifBox.innerHTML += "<div style='"+buttonStyles+"'>"+
        closeHtml +
        "<button id='" + lsKey + "notifyHideForever'>Don't show again</button>"+
        "</div>";
    if (!alreadyPresent) {
        document.body.appendChild(notifBox);
    }
    $("#"+lsKey+"notifyHide").click({"id":lsKey}, function() {
        let key = arguments[0].data.id;
        $("#"+key+"notifBox").remove();
    });
    $("#"+lsKey+"notifyHideForever").click({"id": lsKey}, function() {
        let key = arguments[0].data.id;
        $("#"+key+"notifBox").remove();
        localStorage.setItem(key, "1");
    });
}

function warnBoxJsSetup()
{   // Sets up warnBox if not already established.  This is duplicated from htmshell.c
    var html = "";
    html += "<center>";
    html += "<div id='warnBox' style='display:none;'>";
    html += "<CENTER><B id='warnHead'></B></CENTER>";
    html += "<UL id='warnList'></UL>";
    html += "<CENTER><button id='warnOK'></button></CENTER>";
    html += "</div></center>";


    // GALT TODO either add nonce or move the showWarnBox and hideWarnBox to some universal javascript 
    //   file that is always included. Or consider if we can just dynamically define the functions
    //   right here inside this function?  maybe prepend function names with "window." to (re)define the global functions.
    //  maybe something like window.showWarnBox = function(){stuff here}; 
    html += "<script type='text/javascript'>";
    html += "function showWarnBox() {";
    html += "document.getElementById('warnOK').innerHTML='&nbsp;OK&nbsp;';";
    html += "var warnBox=document.getElementById('warnBox');";
    html += "warnBox.style.display=''; warnBox.style.width='65%%';";
    html += "document.getElementById('warnHead').innerHTML='Warning/Error(s):';";
    html +=  "window.scrollTo(0, 0);";
    html += "}";
    html += "function hideWarnBox() {";
    html += "var warnBox=document.getElementById('warnBox');";
    html += "warnBox.style.display='none';";
    html += "var warnList=document.getElementById('warnList');";
    html += "warnList.innerHTML='';";
    html += "var endOfPage = document.body.innerHTML.substr(document.body.innerHTML.length-20);";
    html += "if(endOfPage.lastIndexOf('-- ERROR --') > 0) { history.back(); }";
    html += "}";
    html += "</script>";

    $('body').prepend(html);
    document.getElementById('warnOK').onclick = function() {hideWarnBox();return false;};
}

function warn(msg)
{ // adds warnings to the warnBox
    var warnList = normed($('#warnList')); // warnBox contains warnList
    if (!warnList) {
        warnBoxJsSetup();
        warnList = normed($('#warnList'));
    }
    if (!warnList)
        alert(msg);
    else {
        // don't add warnings that already exist:
        var oldMsgs = [];
        $('#warnList li').each(function(i, elem) {
            oldMsgs.push(elem.innerHTML);
        });
        // make the would-be new message into an <li> element so the case and quotes
        // match any pre-existing ones
        var newNode = document.createElement('li');
        newNode.innerHTML = msg;
        if (oldMsgs.indexOf(newNode.innerHTML) === -1) {
            $( warnList ).append(newNode);
        }
        if ($.isFunction(showWarnBox))
            showWarnBox();
        else
            alert(msg);
    }
}

var gWarnSinceMSecs = 0;
function warnSince(msg)  // DEAD CODE?
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
    var urlData = {};
    if (!obj)
        obj = $('document');
    var inp = $(obj).find('input');
    var sel = $(obj).find('select');
    //warn("obj:"+$(obj).attr('id') + " inputs:"+$(inp).length+ " selects:"+$(sel).length);
    $(inp).filter(':not([name^="boolshad"]):enabled').each(function (i) {
        var name  = $(this).attr('name');
        var val = $(this).val();
        if ($(this).attr('type') === 'checkbox' || $(this).attr('type') === "CHECKBOX") {
            name = cgiBooleanShadowPrefix() + name;
            val = $(this).prop('checked') ? 1 : 0;
        } else if ($(this).attr('type') === 'radio') {
            if (!$(this).prop('checked')) {
                name = undefined;
            }
        }
        if (name && name !== "Submit" && val !== undefined && val !== null) {
            urlData[name] = val;
        }
    });
    // special case the vcfSampleOrder variable because it is a hidden input type that
    // changes based on click-drag
    $(inp).filter('[name$="vcfSampleOrder"]').each(function (i) {
        var name  = $(this).attr('name');
        var val = $(this).val();
        if (name && name !== "Submit" && val !== undefined && val !== null) {
            urlData[name] = val;
        }
    });
    // special case the highlight/color picker inputs
    $(inp).filter('[id^=colorPicker],[id^=colorInput]').each(function(i) {
        // remove the prefix that lets us recognize this setting, what remains
        // is specific to the track or for the back end
        var name = $(this)[0].id.replace("colorPicker.", "").replace("colorInput.", "");
        var val = $(this).val();
        urlData[name] = val;
    });
    $(sel).filter('[name]:enabled').each(function (i) {
        var name  = $(this).attr('name');
        var val = $(this).val();
        if (name && val !== undefined && val !== null) {
            if (subtrackName && name === subtrackName) {
                if (val === 'hide') {
                   urlData[name+"_sel"] = 0;    // Can't delete "_sel" because default takes over
                   urlData[name]        = "[]"; // Can delete vis because
                } else {                        //     subtrack vis should be inherited.
                    urlData[name+"_sel"] = 1;
                    urlData[name]        = val;
                }
            } else {
                if (Array.isArray( val) && val.length > 1) {
                    urlData[name] = "[" + val.toString() + "]";
                } else
                    urlData[name] = val;
            }
        }
    });
    return urlData;
}

function debugDumpFormCollection(collectionName,vars)
{ // dumps form vars collection in an alert
    var debugStr = ""; 
    for (var thisVar in vars) {
        debugStr += thisVar + "==" + vars[thisVar]+"\n";
    }
    alert("DEBUG "+ collectionName + ":\n"+debugStr);  
}

function varHashChanges(newVars,oldVars)
{   // Returns a hash of all vars that are changed between old and new hash.
    // New vars not found in old are changed.
    var changedVars = {};
    for (var newVar in newVars) {
        if (oldVars[newVar] === null || oldVars[newVar] !== newVars[newVar])
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
        if (count++ > 0) {
            retVal += "&";
        }
        var val = varHash[aVar];
        if (typeof(val) === 'string'
        && val.length >= 2
        && val.indexOf('[') === 0
        && val.lastIndexOf(']') === (val.length - 1)) {
            var vals = val.substr(1,val.length - 2).split(',');
            /* jshint loopfunc: true */// function inside loop works and replacement is awkward.
            $(vals).each(function (ix) {
                if (ix > 0)
                    retVal += "&";
                retVal += aVar + "=" + encodeURIComponent(this);
            });
        } else if (typeof(val) === 'string') {
            // sometimes val is already encoded or partially encoded
            // the CGI cannot know if user input is double encoded
            // so test for already encoded characters here and only
            // encode what we need to
            retVal += aVar + "=" + val.replace(/(%[0-9A-Fa-f]{2})+|[^%]+/g, (match) => {
                if (/%[0-9A-Fa-f]{2}/.test(match)) {
                    // Already percent-encoded, leave as-is
                    return match;
                }
                // Encode unencoded parts
                return encodeURIComponent(match);
            });
        } else {
            retVal += aVar + "=" + encodeURIComponent(val);
        }
    }
    return retVal;
}

function getAllVarsAsUrlData(obj)  // DEAD CODE?
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
    if (!popit) {
        popit = $('#popit');

        if (!popit ) {
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
                               close: function() {    // clear out html after close to prevent
                                   $(popDiv).empty(); // problems caused by duplicate html elements
                               }
                           });
    // Apparently the options above to dialog take only once, so we set title explicitly.
    if (popTitle && popTitle.length > 0)
        $(popit).dialog('option' , 'title' , popTitle );
    else
        $(popit).dialog('option' , 'title' , "Please Respond");
    jQuery('body').css('cursor', '');
    $(popit).dialog('open');
}
*/

function embedBoxOpen(boxit, content, reenterable)  // DEAD CODE?
{   // embeds a box for the provided content.
    // This box has 1 button (close) by default and 2 buttons if the name of an applyFunc
    // is provided (apply, cancel) If there is no apply function, the box may be reentrent, 
    // meaning subsequent calls do not need to provide content
    // NOTE: 4 extra STRING Params: boxWidth, boxTitle, applyFunc, applyName

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
    if (!boxit) {
        boxit = $('div#boxit');

        if (!boxit) {
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
        if (reenterable && applyFunc.length === 0)
            closeHtml += "true);";
        else
            closeHtml += "false);";

        // Buttons
        buildHtml += "<div>";
        if (applyFunc.length > 0) { // "Apply" button and "Cancel" button.  Apply also closes!
            buildHtml += "&nbsp;<INPUT TYPE='button' value='" + applyName + "' onClick='"+ 
                            applyFunc + "(" + $(boxit).attr('id') + "); " + closeHtml + "'>&nbsp;";
            closeButton = "Cancel"; // If apply button then close is cancel
        }
        buildHtml += "&nbsp;<INPUT TYPE='button' value='" + closeButton + "' onClick='" + closeHtml;
        buildHtml += "'>&nbsp;</div>";

        $(boxit).html("<div class='blueBox' style='width:" + boxWidth + 
                          "; background-color:#FFF9D2;'>" + buildHtml + "</div>"); // Make it boxed
    }

    var boxedHtml = $(boxit).html();
    if (!boxedHtml || boxedHtml.length === 0)
        warn("embedHtmlBox() called without content");
    else
        $(boxit).show();
}

function embedBoxClose(boxit, reenterable)  // DEAD CODE?
{   // Close an embedded box
    // NOTE  4 extra STRING Params: boxWidth, boxTitle, applyFunc, applyName
    if (boxit) {
        $(boxit).hide();
        if (!reenterable)
            $(boxit).empty();
    }
}

function startTiming()  // DEAD CODE?
{
    var now = new Date();
    return now.getTime();
}

function showTiming(start,whatTookSoLong)  // DEAD CODE?
{
    var now = new Date();
    var end = (now.getTime() - start);
    warn(whatTookSoLong+" took "+end+" msecs.");
    return end;
}

function getHgsid()
{// return current session id

    // .first() because hgTracks turned up 3 of these!
    var hgsid = normed($("input[name='hgsid']").first());
    if (hgsid)
        return hgsid.value;

    hgsid = getURLParam(window.location.href, "hgsid");
    if (hgsid.length > 0)
        return hgsid;

    // This may be moved to 1st position as the most likely source
    if (typeof(common) !== 'undefined' && common.hgsid !== undefined && common.hgsid !== null)
        return common.hgsid;

    hgsid = normed($("input#hgsid").first());
    if (hgsid)
        return hgsid.value;

    return "";
}

function undecoratedDb(db)
// return the db name with any hub_id_ stripped
{
var retDb = db;
if (db.startsWith("hub_")) {
    retDb = db.split('_').slice(2).join('_');
}
return retDb;
}

function getDb()
{
    var db = normed($("input[name='db']").first());
    if (db)
        return db.value;

    db = getURLParam(window.location.href, "db");
    if (db.length > 0)
        return db;

    // This may be moved to 1st position as the most likely source
    if (typeof(common) !== 'undefined' && common.db)
        return common.db;

    db = normed($("input#db").first());
    if (db)
        return db.value;

    if (typeof uiState !== "undefined" && uiState.db)
        return uiState.db;

    db = document.getElementById("selectAssembly");
    if (db)
        return db.selectedOptions[0].value;

    return "";
}

function undecoratedTrack(track)
// return the track name with any hub_id_ stripped
{
var retTrack = track;
if (track.startsWith("hub_")) {
    retTrack = track.split('_').slice(2).join("_");
}
return retTrack;
}

function getTrack()
{
    var track = normed($("input#g").first());
    if (track)
        return track.value;

    track = normed($("input[name='g']").first());
    if (track)
        return track.value;

    track = getURLParam(window.location.href, "g");
    if (track.length > 0)
        return track;

    // This may be moved to 1st position as the most likely source
    if (typeof(common) !== 'undefined' && common.track)
        return common.track;

    return "";
}

function Rectangle()  // DEAD CODE?
{
// Rectangle object constructor:
// calling syntax:
//
// new Rectangle(startX, endX, startY, endY)
// new Rectangle(coords) <-- coordinate string from an area item
    if (arguments.length === 4) {
        this.startX = arguments[0];
        this.endX = arguments[1];
        this.startY = arguments[2];
        this.endY = arguments[3];
    } else if (arguments.length > 0)  {
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

Rectangle.prototype.contains = function(x, y)  // DEAD CODE?
{
// returns true if given points are in the rectangle
    var retval = x >= this.startX && x <= this.endX && y >= this.startY && y <= this.endY;
    return retval;
};

function commify (str) {
    if (typeof(str) === "number")
        str = str + "";
    var n = str.length;
    if (n <= 3) {
        return str;
    } else {
        var pre = str.substring(0, n-3);
        var post = str.substring(n-3);
        pre = commify(pre);
        return pre + "," + post;
    }
}

function parsePosition(position)
// Parse chr:start-end string into a chrom, start, end object
{
    if (position && position.length > 0) {
        position = position.replace(/,/g, "");
        var a = /(\S+):(\d+)-(\d+)/.exec(position);
        if (a && a.length === 4) {
            var o = {};
            o.chrom = a[1];
            o.start = parseInt(a[2]);
            o.end = parseInt(a[3]);
            return o;
        }
    }
    return null;
}

function makeHighlightString(db, chrom, start, end, color) {
/* given db and a range on it and a color (color must be prefixed by #),
 * return the highlight string in the cart for it. See parsePositionWithDb for the history
 * of the various accepted highlight strings */
    return db+"#"+chrom+"#"+start+"#"+end+color;
}

function parsePositionWithDb(position)
// returns an object with chrom, start, end and optionally color attributes
// position is a string and can be in one of five different formats:
// 0) chr:start-end 
// 1) db.chr:start-end 
// 2) db.chr:start-end#color
// 3) db#chr#start#end#color
// Formats 0-2 are only supported for backwards compatibility with old carts
{
    var out = {};
    var parts = null;
    if (position.split("#").length !==5 ) {
        // formats of old carts: 0-2
        parts = position.split(".");
        // handle the db part
        if (parts.length === 2) {
            out.db = parts[0];
            position = parts[1];
        } else {
            out.db = getDb(); // default the db 
        }
        // position now contains chr:start-end#color
        parts = position.split("#"); // Highlight Region may carry its color
        if (parts.length === 2) {
            position = parts[0];
            out.color = '#' + parts[1];
        }
        var pos = parsePosition(position);
        if (pos) {
            out.chrom = pos.chrom;
            out.start = pos.start;
            out.end   = pos.end;
        }
    } else {
        // new format
        parts = position.split("#");
        out.db = parts[0];
        out.chrom = parts[1];
        out.start = parseInt(parts[2]);
        out.end = parseInt(parts[3]);
        out.color = "#" + parts[4];
    }
    return out;
}

function getHighlight(highlightStr, index) 
/* Parse out highlight at index and return as a position object (see parsePositionWithDb) */
{
    var hlStrings = highlightStr.split("|");
    var myHlStr = hlStrings[index];
    var posObj = parsePositionWithDb(myHlStr);
    return posObj;
}

function getSizeFromCoordinates(position)
{
// Parse size out of a chr:start-end string
    var o = parsePosition(position);
    if (o) {
        return o.end - o.start + 1;
    }
    return null;
}

// This code is intended to allow setting up a wait cursor while waiting on the function
var gWaitFuncArgs = [];
var gWaitFunc;

function waitMaskClear()
{ // Clears the waitMask
    var  waitMask = normed($('#waitMask'));
    if (waitMask)
        $(waitMask).hide();
}

function waitMaskSetup(timeOutInMs)
{ // Sets up the waitMask to block page manipulation until cleared

    // Find or create the waitMask (which masks the whole page)
    var  waitMask = normed($('#waitMask'));
    if (!waitMask) {
        // create the waitMask
        $("body").append("<div id='waitMask' class='waitMask'></div>");
        waitMask = normed($('#waitMask'));
    }
    $(waitMask).css({opacity:0.0,display:'block',top: '0px', 
                        height: $(document).height().toString() + 'px' });
    // Special for IE, since it takes so long, make mask obvious
    //if (theClient.isIePre11())
    //    $(waitMask).css({opacity:0.4,backgroundColor:'gray'});

    // Things could fail, so always have a timeout.
    if (!timeOutInMs)  // works for undefined, null and 0
        timeOutInMs = 30000; // IE can take forever!

    if (timeOutInMs > 0)
        setTimeout(waitMaskClear,timeOutInMs); // Just in case

    return waitMask;  // The caller could add css if they wanted.
}

function _launchWaitOnFunction()
{ // should ONLY be called by waitOnFunction()
  // Launches the saved function
    var func = gWaitFunc;
    gWaitFunc = null;
    var funcArgs = gWaitFuncArgs;
    gWaitFuncArgs = [];

    if (!func || !jQuery.isFunction(func))
        warn("_launchWaitOnFunction called without a function");
    else
        func.apply(this, funcArgs);

    // Special if the first var is a button that can visually be inset
    if (funcArgs.length > 0 && funcArgs[0].type) {
        if (funcArgs[0].type === 'button' && $(funcArgs[0]).hasClass('inOutButton')) {
            $(funcArgs[0]).css('borderStyle',"outset");
        }
    }
    // Now we can get rid of the wait cursor
    waitMaskClear();
}

function waitOnFunction(func)
{   // sets the waitMask (wait cursor and no clicking),
    // then launches the function with up to 5 arguments
    if (!jQuery.isFunction(func)) {
        warn("waitOnFunction called without a function");
        return false;
    }
    if (gWaitFunc) {
        if (gWaitFunc === func) // already called (sometimes hapens when onchange event is triggered
            return true;       // by js (rather than direct user action).  Happens in IE8
        warn("waitOnFunction called but already waiting on a function");
        return false;
    }

    waitMaskSetup(0);  // Find or create waitMask (which masks whole page) but gives up after 5sec

    // Special if the first var is a button that can visually be inset
    if (arguments.length > 1 && arguments[1].type) {
        if (arguments[1].type === 'button' && $(arguments[1]).hasClass('inOutButton')) {
            $(arguments[1]).css( 'borderStyle',"inset");
        }
    }

    // Build up the aruments array
    for (var aIx=1; aIx < arguments.length; aIx++) {
        gWaitFuncArgs.push(arguments[aIx]);
    }
    gWaitFunc = func;

    setTimeout(_launchWaitOnFunction,10);

}

// --- yielding iterator ---
function _yieldingIteratorObject(yieldingFunc)  // DEAD CODE?
{ // This is the "recusive object" or ro which is instantiated in waitOnIteratingFunction
  // yieldingFunc is passed in from waitOnIteratingFunction
  // and will recurse which recursively calls an iterator
    this.step = function(msecs,args) {
        setTimeout(function() { yieldingFunc(args); }, msecs); // recursive timeouts
        return;
    };
}

function yieldingIterator(iteratingFunc,continuingFunc,args)  // DEAD CODE?
{   // Will run iteratingFunc function with "yields", then run continuingFunc
    // Based upon design by Guido Tapia, PicNet
    // iteratingFunc must return number of msecs to pause before next interation.
    //                return 0 ends iteration with call to continuingFunc
    //                return < 0 ends iteration with no call to continuingFunc
    // Both iteratingFunc and continuingFunc will receive the single "args" param.
    // Hint. for multiple args, create a single struct object

    var ro = new _yieldingIteratorObject(function() {
            var msecs = iteratingFunc(args);
            if (msecs > 0)
                ro.step(msecs,args);      // recursion
            else if (msecs === 0)
                continuingFunc(args);     // completion
            // else (msec < 0) // abandon
        });
    ro.step(1,args);                      // kick-off
}

function showLoadingImage(id)
// Show a loading image above the given id; return's id of div added (allowing later removal).
{
    var loadingId = id + "LoadingOverlay";
    var overlay = $("<div id='"+loadingId+"' class='loading'></div>");
    var ele = $(document.getElementById(id));
    overlay.appendTo("body");
    var divLeft = ele.position().left + 2;
    var width = ele.width() - 1;
    var height = ele.height();
    overlay.width(width);
    overlay.height(height);
    overlay.css({top: (ele.position().top + 1) + 'px', left: divLeft + 'px'});
    return loadingId;
}

function hideLoadingImage(id)
{
    $(document.getElementById(id)).remove();
}

function codonColoringChanged(name)
{   // Updated disabled state of codonNumbering checkbox based on current value
    // of track coloring select.
    var val = $("select[name='" + name + ".baseColorDrawOpt'] option:selected").text();
    $("input[name='" + name + ".codonNumbering']").attr('disabled', val === "OFF");
    $("#" + name + "CodonNumberingLabel").toggleClass("disabled", val === "OFF" ? true : false);
}

function gtexTransformChanged(name)
{ // Disable view limits settings if log transform enabled

    // NOTE: selector strings are a bit complex due to dots GB vars/attributes (track.var)
    // so can't use more concise jQuery syntax

    // check log transform
    var logCheckbox = $("input[name='" + name + ".logTransform']");
    var isLogChecked = logCheckbox.attr('checked');

    // enable/disable view limits
    var maxTextbox = $("input[name='" + name + ".maxViewLimit']");
    maxTextbox.attr('disabled', isLogChecked);
    var maxTextLabel = $("." + name + "ViewLimitsMaxLabel");
    maxTextLabel.toggleClass("disabled", isLogChecked ? true : false);
}

function barChartUiTransformChanged(name) {
// Disable view limits settings if log transform enabled
    gtexTransformChanged(name);
}

function gtexSamplesChanged(name)
{ // Disable and comparison controls if all samples selected

    // check sample select
    var sampleSelect = $("input[name='" + name + ".samples']:checked");
    var isAllSamples = (sampleSelect.val() === 'all');

    // enable/disable comparison options
    // limiting to radio buttons as there is a problem with tissue checkbox naming on popup
    var comparisonButtons = $("input[type='radio' name='" + name + ".comparison']");
    comparisonButtons.attr('disabled', isAllSamples);
    var comparisonLabel = $("." + name + "ComparisonLabel");
    comparisonLabel.toggleClass("disabled", isAllSamples ? true : false);
}


var bindings = {
    // This object is for finding a subtring using tokens as bounds
    // The tokens can be literal strings or regular expressions.
    // If regular expressions are used, then only the first expression found will count
    // If not using regexp, then you can pass in limits to the original string

    _raw: function (begToken,endToken,someString,ixBeg,ixEnd)
    { // primitive not meant to be called directly but by bindings.inside and bindings.outside
        if (someString.length <= 0)
            return '';
        if (ixBeg === undefined || ixBeg === null)
            ixBeg = 0;
        if (ixEnd === undefined || ixEnd === null)
            ixEnd = someString.length;
        var insideBeg = ixBeg;
        var insideEnd = ixEnd;
        if (begToken.constructor.name === "RegExp")
            insideBeg = someString.search(begToken);
        else if (begToken.length > 0)
            insideBeg = someString.indexOf(begToken,ixBeg);
        if (endToken.constructor.name === "RegExp")
            insideEnd = someString.search(endToken);
        else if (endToken.length > 0)
            insideEnd = someString.indexOf(endToken,ixBeg);
        if (ixBeg <= insideBeg && insideBeg <= insideEnd && insideEnd <= ixEnd)
            return {start : insideBeg, stop : insideEnd};

        return {start : -1, stop : -1};
    },

    inside: function (begToken,endToken,someString,ixBeg,ixEnd)
    { // returns the inside bounds of 2 tokens within a string
    // Note ixBeg and ixEnd are optional bounds already established within string
    // Pattern match can be used instead of literal token if a regexp is passed in for the tokens
        var bounds = bindings._raw(begToken,endToken,someString,ixBeg,ixEnd);
        if (bounds.start > -1) {
            if (begToken.constructor.name === "RegExp")
                bounds.start += someString.match(begToken)[0].length;
            else
                bounds.start += begToken.length;
        }
        return bounds;
    },

    outside: function (begToken,endToken,someString,ixBeg,ixEnd)
    { // returns the outside bounds of 2 tokens within a string
    // Note ixBeg and ixEnd are optional bounds already established within string
    // Pattern match can be used instead of literal token if a regexp is passed in for the tokens
        var bounds = bindings._raw(begToken,endToken,someString,ixBeg,ixEnd);
        if (bounds.start > -1) {
            if (endToken.constructor.name === "RegExp") 
                bounds.stop  += someString.match(endToken)[0].length;
            else
                bounds.stop  += endToken.length;
        }
        return bounds;
    },

    insideOut: function (begToken,endToken,someString,ixBeg,ixEnd)
    { // returns what falls between begToken and endToken as found in the string provided
    // Note ixBeg and ixEnd are optional bounds already established within string
        var bounds = bindings.inside(begToken,endToken,someString,ixBeg,ixEnd);
        if (bounds.start < bounds.stop)
            return someString.slice(bounds.start,bounds.stop);

        return '';
    }
};

function stripHgErrors(returnedHtml, whatWeDid)
{   // strips HGERROR style 'early errors' and shows them in the warnBox
    // If whatWeDid !== null, we use it to return info about what we stripped out and
    // processed (current just warnMsg).
    var cleanHtml = returnedHtml;
    var begToken = '<!-- HGERROR-START -->';
    var endToken = '<!-- HGERROR-END -->';
    while (cleanHtml.length > 0) {
        var bounds = bindings.outside(begToken,endToken,cleanHtml);
        if (bounds.start === -1)
            break;
        // OLD WAY var warnMsg = bindings.insideOut('<P>','</P>',cleanHtml,bounds.start,bounds.stop);
	var warnMsg = cleanHtml.slice(bounds.start+begToken.length,bounds.stop-endToken.length);
        if (warnMsg.length > 0) {
            warn(warnMsg);
            if (whatWeDid)
                whatWeDid.warnMsg = warnMsg;
        }
        cleanHtml = cleanHtml.slice(0,bounds.start) + cleanHtml.slice(bounds.stop);
    }
    return cleanHtml;
}

function stripJsFiles(returnedHtml, debug, whatWeDid)
{ // strips javascript files from html returned by ajax
    var cleanHtml = returnedHtml;
    var shlurpPattern=/<script type=\'text\/javascript\' SRC\=\'.*\'\><\/script\>/gi;
    if (debug || whatWeDid) {
        var jsFiles = cleanHtml.match(shlurpPattern);
        if (jsFiles && jsFiles.length > 0) {
	    if (debug)
		alert("jsFiles:'"+jsFiles+"'\n---------------\n"+cleanHtml); // warn() interprets html
	    if (whatWeDid)
		whatWeDid.jsFiles = jsFiles;
	}
    }
    cleanHtml = cleanHtml.replace(shlurpPattern,"");

    return cleanHtml;
}

function stripCspHeader(html, debug, whatWeDid)
{ // strips CSP Header from html returned by ajax
    var shlurpPattern=/<meta http-equiv=\'Content-Security-Policy\' content=".*"\>/i;
    if (debug || whatWeDid) {
        var csp = html.match(shlurpPattern);
        if (csp && csp.length > 0) {
	    if (debug)
		alert("csp:'"+csp+"'\n---------------\n"+html); // warn() interprets html
	    if (whatWeDid)
		whatWeDid.csp = csp[0];
	}
    }
    return html.replace(shlurpPattern,""); // Clean CSP meta tag.
}

function parseNonce(content, debug)
{ // parse nonce from returned ajax page csp header

    if (!content)
	return "";
    // parse nonce like 'nonce-JDPiW8odQkiav4UCeXsa34ElFm7o'
    var sectionBegin = "'nonce-";
    var sectionEnd   = "'";
    var ix = content.indexOf(sectionBegin);
    if (ix < 0)
	return "";
    content = content.substring(ix+sectionBegin.length);
    ix = content.indexOf(sectionEnd);
    if (ix < 0)
	return "";
    content = content.substring(0,ix);
    if (debug)
	alert('ajax nonce='+content);
    return content;
}

function stripCssFiles(returnedHtml,debug)
{ // strips csst files from html returned by ajax
    var cleanHtml = returnedHtml;
    var shlurpPattern=/<LINK rel=\'STYLESHEET\' href\=\'.*\' TYPE=\'text\/css\' \/\>/gi;
    if (debug) {
        var cssFiles = cleanHtml.match(shlurpPattern);
        if (cssFiles && cssFiles.length > 0)
            alert("cssFiles:'"+cssFiles+"'\n---------------\n"+cleanHtml);
    }
    cleanHtml = cleanHtml.replace(shlurpPattern,"");

    return cleanHtml;
}

function stripJsNonce(html, nonce, debug, whatWeDid)
{ // Strips and returns embedded javascript from html returned by ajax with nonce
    var results=[];
    var content = "";
    var sectionBegin = "<script type='text/javascript' nonce='"+nonce+"'>";
    var sectionEnd   = "</script>";
    var lastIx = 0;
    while (1) {
	var ix = html.indexOf(sectionBegin, lastIx);
	if (ix < 0)
	    break;
	var ix2 = ix + sectionBegin.length;
	var ex = html.indexOf(sectionEnd, ix2);
	if (ex < 0)
	    break;
	content += html.substring(lastIx,ix);
	var jsNonce = html.substring(ix2,ex);
	if (debug)
	    alert("jsNonce:"+jsNonce);
	results.push(jsNonce);
	lastIx = ex + sectionEnd.length;
	}
    // grab the last piece.
    content += html.substring(lastIx);

    //return results;
    if (whatWeDid)
	whatWeDid.js = results;

    return content;

}

function charsAreHex(s)
// are all the chars found hex?
{
    var hexChars = "01234566789abcdefABCDEF";
    var d = false;
    var i = 0;
    if (s) {
	d = true;
	while (i < s.length) {
	    if (hexChars.indexOf(s.charAt(i++)) < 0)
		d = false;
	}
    }
    return d;
}

function nonAlphaNumericHexDecodeText(s, prefix, postfix)
// For html tag attributes, it decodes non-alphanumeric characters
// with <prefix>HH<postfix> hex codes.
// Decoding happens in-place, changing the input string s.
// prefix must not be empty string or null, but postfix can be empty string.
// Because the decoded string is always equal to or shorter than the input string,
// the decoding is just done in-place modifying the input string.
// Accepts upper and lower case values in entities.
//
{
    var d = ""; 
    var pfxLen = prefix.length;
    var postLen = postfix.length;
    var i = 0;
    if (s) {
	while (i < s.length) {
	    var matched = false;
	    if (i+pfxLen+postLen+2 <= s.length) {
		var pre = s.substr(i, pfxLen).toLowerCase();
		if (pre === prefix) {
		    var post = s.substr(i+pfxLen+2, postLen).toLowerCase();
		    if (post === postfix) {
			var hex = s.substr(i+pfxLen, 2);
			if (charsAreHex(hex)) {
			    d = d + String.fromCharCode(parseInt(hex,16));
			    i += pfxLen + 2 + postLen;
			    matched = true;
			}
		    }
		}
	    }
	    if (!matched)
		d = d + s.charAt(i++);
	    }
    }
    return d;
}


function jsDecode(s)
// For JS string values decode "\xHH" 
{
    return nonAlphaNumericHexDecodeText(s, "\\x", "");
}


function stripCSPAndNonceJs(content,  debug, whatWeDid)
// Strip CSP Header and script blocks with the ajax nonce.
{

    var pageNonce = getNonce(debug);

    var csp = {};
    content = stripCspHeader(content, debug, csp);

    var ajaxNonce = parseNonce(csp.csp, debug);
	
    var jsBlocks = {};
    content = stripJsNonce(content, ajaxNonce, debug, jsBlocks);

    if (whatWeDid) {
	whatWeDid.pageNonce = pageNonce;
	whatWeDid.ajaxNonce = ajaxNonce;  // Not in use yet.
	whatWeDid.js = jsBlocks.js;
    }

    return stripHgErrors(content, whatWeDid); // Certain early errors are not called via warnBox
	
}

function appendNonceJsToPage(jsNonce)
// Append ajax js blocks with nonce.
// Create jsNonce by calling stripCSPAndNonceJs.
// Call this after ajax html content has been added to the page/DOM.
{
    var i;
    for (i=0; i<jsNonce.js.length; ++i) {
	var sTag = document.createElement("script");
	sTag.type = "text/javascript";
	sTag.text = jsNonce.js[i];
	sTag.setAttribute('nonce', jsNonce.pageNonce); // CSP2 Requires
	document.head.appendChild(sTag);
    }		
}

function stripJsEmbedded(returnedHtml, debug, whatWeDid)
{ 
  // GALT NOTE: this may have been mostly obsoleted by CSP2 changes.
  // There were 3 or 4 places in the code that even in production
  // had called this function stripJsEmbedded with debug=true, which means that
  // if any script tag blocks are present, they would be seen and shown
  // to the user.  This probably was because if these blocks were found
  // simply adding them to the div html from the ajax callback would result in 
  // their being ignored by the browser. It seems to be a security feature of browsers.
  // Meanwhile however inline event handlers in the html worked and were allowed.
  // So this was just a way to warn developers that their script blocks would have been ignored
  // and have no effect. I think this concern no longer applies after my CSP2 changes
  // because it is able to pull in all the js, whether from event handlers or what would
  // have been individual script blocks in the old days, and adds it to
  // the page with a nonce and appendChild.
  //
  // strips embedded javascript from html returned by ajax
  // NOTE: any warnBox style errors will be put into the warnBox
  // If whatWeDid !== null, we use it to return info about
  // what we stripped out and processed (current just warnMsg).
    var cleanHtml = returnedHtml;
    
    // embedded javascript?
    while (cleanHtml.length > 0) {
        var begPattern = /<script.*\>/i;
        var endPattern = /<\/script\>/i;
        var bounds = bindings.outside(begPattern,endPattern,cleanHtml);
        if (bounds.start === -1)
            break;
        var jsEmbeded = cleanHtml.slice(bounds.start,bounds.stop);
        if (-1 === jsEmbeded.indexOf("showWarnBox")) {
            if (debug)
                alert("jsEmbedded:'"+jsEmbeded+"'\n---------------\n"+cleanHtml);
        } else {
            var warnMsg = bindings.insideOut('<li>','</li>',cleanHtml,bounds.start,bounds.stop);
            if (warnMsg.length > 0) {
                warnMsg = jsDecode(warnMsg);
                warn(warnMsg);
                if (whatWeDid)
                    whatWeDid.warnMsg = warnMsg;
            }
        }
        cleanHtml = cleanHtml.slice(0,bounds.start) + cleanHtml.slice(bounds.stop);
    }
    return stripHgErrors(cleanHtml, whatWeDid); // Certain early errors are not called via warnBox
}

function stripMainMenu(returnedHtml, debug, whatWeDid)
{ // strips main menu div from html returned by ajax
  // NOTE: any warnBox style errors will be put into the warnBox
  // If whatWeDid !== null, we use it to return info about
  // what we stripped out and processed (current just warnMsg).
    var cleanHtml = returnedHtml;
    // embedded javascript?
    while (cleanHtml.length > 0) {
        
        var begPattern = '<div id="main-menu-whole">';
        var endPattern = '</div><!-- end main-menu-whole -->';
        var bounds = bindings.outside(begPattern,endPattern,cleanHtml);
        if (bounds.start === -1)
            break;
        var mainMenu = cleanHtml.slice(bounds.start,bounds.stop);
        if (-1 === mainMenu.indexOf("showWarnBox")) {
            if (debug)
                alert("mainMenu:'"+mainMenu+"'\n---------------\n"+cleanHtml);
        } else {
            var warnMsg = bindings.insideOut('<li>','</li>',cleanHtml,bounds.start,bounds.stop);
            if (warnMsg.length > 0) {
                warn(warnMsg);
                if (whatWeDid)
                    whatWeDid.warnMsg = warnMsg;
            }
        }
        cleanHtml = cleanHtml.slice(0,bounds.start) + cleanHtml.slice(bounds.stop);
    }
    return stripHgErrors(cleanHtml, whatWeDid); // Certain early errors are not called via warnBox
}

function visTriggersHiddenSelect(obj)
{ // SuperTrack child changing vis should trigger superTrack reshaping.
  // This is done by setting hidden input "_sel"
    var trackName_Sel = $(obj).attr('name') + "_sel";
    var theForm = $(obj).closest("form");
    var visible = (obj.selectedIndex !== 0);
    if (visible) {
        updateOrMakeNamedVariable(theForm,trackName_Sel,"1");
    } else
        disableNamedVariable(theForm,trackName_Sel);
    return true;
}

function setCheckboxList(list, value)
{ // set value of all checkboxes in semicolon delimited list
    var names = list.split(";");
    for (var i=0; i < names.length; i++) {
        $("input[name='" + names[i] + "']").attr('checked', value);
    }
}

function calculateHgTracksWidth()
{
// return appropriate width for hgTracks image given users current window width
    return $(window).width() - 20;
}

function addPixAndReloadPage()
/* users who do not come in from hgGateway have no pix variable in the URL nor the cart.
 * This is a rare case, and the solution is brute force: if it happens, set pix, then reload the entire page.
 * This will only happen once to these users, as afterwards the cookie is set. */
{
    var winWidth = calculateHgTracksWidth();
    var myUrl = window.location.href;
    var sep = '?';
    if (myUrl.indexOf('?')!==-1)
        sep = '&';
    var newUrl = myUrl+sep+"pix="+winWidth;
    window.location.href = newUrl;
}

function hgTracksSetWidth()
{
    var winWidth = calculateHgTracksWidth();
    if ($("#imgTbl").length === 0) {
        // XXXX what's this code for?
        $("#TrackForm").append('<input type="hidden" name="pix" value="' + winWidth + '"/>');
        //$("#TrackForm").submit();
    } else {
        $("input[name=pix]").val(winWidth);
    }
}

function filterByMaxHeight(multiSel)
{   // Setting a max height to scroll dropdownchecklists but
    // multiSel is hidden when this is done, so it's position and height must be estimated.
    var pos = $(multiSel).closest(':visible').offset().top + 30;
    if (pos <= 0)
        pos = 260;

    // Special mess since the filterBy's on non-current tabs will calculate pos badly.
    var tabbed = normed($('input#currentTab'));
    if (tabbed) {
        var tabDiv = $(multiSel).parents('div#'+ $(tabbed).attr('value'));
        if (!tabDiv || $(tabDiv).length === 0) {
            pos = 360;
        }
    }
    var maxHeight = $(window).height() - pos;
    var selHeight = ($(multiSel).children().length + 1) * 22;
    if (maxHeight > selHeight)
        maxHeight = null;

    return maxHeight;
}

//////////// Drag and Drop ////////////
function tableDragAndDropRegister(thisTable)
{// Initialize a table with tableWithDragAndDrop
    if ($(thisTable).hasClass("tableWithDragAndDrop") === false)
        return;

    $(thisTable).tableDnD({
        onDragClass: "trDrag",
        dragHandle: "dragHandle",
        onDrop: function(table, row, dragStartIndex) {
                if (row.rowIndex !== dragStartIndex) {
                    if (sortTable.savePositions) {
                        sortTable.savePositions(table);
                    }
                }
            }
    });
    $(thisTable).find("td.dragHandle").hover(
        function(){ $(this).closest('tr').addClass('trDrag'); },
        function(){ $(this).closest('tr').removeClass('trDrag'); }
    );
}

  ///////////////////////////////
 ////////// Sort Table /////////
///////////////////////////////
var sortTable = {
// The sortTable object handles sorting HTML tables on columns.
// Just add the 'sortable' class to your table and in ready() call 
//                                                       sortTable.initialize($('table.sortable')).
//
// Details you don't need to know until you want to do something fancy.
// A sortable table requires:
// TABLE.sortable: TABLE class='sortable' containing a THEAD header and sortable TBODY filled
//                 with the rows to sort.
//   THEAD.sortable: (NOTE: created if not found) THEAD can contain multiple rows must contain:
//     TR.sortable: exactly 1 header TH (tr) class='sortable' which will declare the sort columns:
//     TH.sortable: 1 or more TH (column headers) with class='sortable sort1 [sortRev]'
//                  (or sort2, sort3) declaring sort order and whether reversed.  e.g.:
//                  <TH id='factor' class='sortable sortRev sort3' nowrap>...</TH>
//                  (this means that factor is currently the third sort column and reverse sorted)
//            NOTE: If no TH.sortable is found, then every th in the TR.sortable will be converted
//                  for you and will be in sort1,2,3 order.
//       ONCLICK: Each TH.sortable must call sortTable.sortOnButtonPress(this) directly
//                or indirectly in the onclick event e.g.:
//                <TH id='factor' class='sortable sortRev sort3' nowrap title='Sort on this column' 
//                                           onclick="return sortTable.sortOnButtonPress(this);">
//          NOTE: onclick function will automatically be added if not found.
//       SUP: Each TH.sortable *may* contain a <sup> which will be filled with an
//                          up or down arrow and the column's sort order: e.g. <sup>&darr;2</sup>
//            NOTE: The sup can be added via the addSuperscript option in sortTable.initialize().
//   TBODY.sortable: (NOTE: created if not found) The TBODY class='sortable' contains the
//                   table rows that get sorted:
//                   TBODY->TR & ->TD: Each row contains a TD for each sortable column.
//                   The innerHTML (entire contents) of the cell will be used for sorting.
//     TRICK: You can use the 'abbr' field to subtly alter the sortable contents.
//            Otherwise sorts on td contents ($(td).text()).  Use the abbr field to make
//            case-insensitive sorts or force exceptions to alpha-text order
//            (such as. ZCTRL vs Control forcing controls to bottom)  e.g.:
//                <TD id='wgEncodeBroadHist...' nowrap abbr='ZCTRL' align='left'>Control</TD>
//            This is also the method to ensure a numeric sort e.g.:
//                <td align="right" abbr="000003416800354">3.2 GB</td>
//            IMPORTANT: You must add abbr='use' to the TH.sortable definitions.
// Finally if you want the tableSort to alternate the table row colors (using #FFFEE8 and #FFF9D2)
// then TBODY.sortable should also have class 'altColors'
// NOTE: This class can be added by using the altColors option to sortTable.initialize().
//
// PRESERVING TO CART: To send the sort column on a form 'submit', the header tr (TR.sortable)
//   needs a named hidden input of class='sortOrder' as:
//      <INPUT TYPE=HIDDEN NAME='wgEncodeBroadHistone.sortOrder'
//                                              class='sortOrder' VALUE="factor=- cell=+ view=+">
//   AND each sortable column header (TH.sortable) must have id='{name}' which is the name of
//   the sortable field (e.g. 'factor', 'shortLabel').  The value preserves the column sort order
//   and direction based upon the id={name} of each sort column.  In the example, while 'cell' may
//   be the first column, the table is currently reverse ordered by 'factor', then by cell and view.
// And to send the sorted row orders on form 'submit', each TBODY->TR will need a named hidden
//   input field of class='trPos'.  e.g.:
//      <INPUT TYPE=HIDDEN NAME='wgEncodeHaibTfbsA549ControlPcr2xDexaRawRep1.priority'
//                                                                        class='trPos' VALUE="2">
//   A reason to preserve the order in the cart is if the order will affect other cgis.
//   For instance: sort subtracks and see that order in the hgTracks image.

    // Sorting a table by columns relies upon the columns obj, whose C equivalent would look like:
    //struct column
    //    {
    //    char *  tags[];     // a list of field names in sort order (e.g. 'cell', 'shortLabel')
    //    boolean reverse[];  // the sort direction for each sort field
    //    int     cellIxs[];  // The indexes of the columns in the table to be sorted
    //    boolean useAbbr[];  // Compare on Abbr or on text()?
    //    };

    // These 2 globals are used during setTimeout, so that rows can be hidden while sorting
    // and javascript timeout on slow (IE) browsers is less likely
    columns: null,
    tbody: null,
    loadingId: null,
    caseSensitive: false, // sorts are case INSENSITIVE by default

    sortCaseSensitive: function (sensitive)
    {   // set case senstivity, which can be added to each sortable columnn's onclick event.
        // or set for the whole table right after initialize()
        sortTable.caseSensitive = sensitive;
    },

    row: function (tr,sortColumns)
    {
        this.fields  = [];
        this.reverse = [];
        this.row = tr;
        for (var ix=0; ix < sortColumns.cellIxs.length; ix++)
            {
            var cell = tr.cells[sortColumns.cellIxs[ix]];
            this.fields[ix]  = (sortColumns.useAbbr[ix] ? cell.abbr : $(cell).text());
            if (!sortTable.caseSensitive) 
                this.fields[ix]  = this.fields[ix].toLowerCase(); // case insensitive sorts
            this.reverse[ix] = sortColumns.reverse[ix];
            }
    },

    rowCmp: function (a,b)
    {
        for (var ix=0; ix < a.fields.length; ix++) {
            if (a.fields[ix] > b.fields[ix])
                return (a.reverse[ix] ? -1:1);
            else if (a.fields[ix] < b.fields[ix])
                return (a.reverse[ix] ? 1:-1);
        }
        return 0;
    },

    field: function (value,reverse,row)
    {
        if (sortTable.caseSensitive || typeof(value) !== 'string') 
            this.value   = value;
        else
            this.value   = value.toLowerCase(); // case insensitive sorts
        this.reverse = reverse;
        this.row     = row;
    },

    fieldCmp: function (a,b)
    {
        if (a.value > b.value)
            return (a.reverse ? -1:1);
        else if (a.value < b.value)
            return (a.reverse ? 1:-1);
        return 0;
    },

    sort: function (tbody,sortColumns)
    {// Sorts table based upon rules passed in by function reference
    // Expects tbody to not sort thead, but could take table

        // The sort routine available is javascript array.sort(), which cannot sort rows directly
        // Until we have jQuery >=v1.4, we cannot easily convert tbody.rows[] inot a javascript array
        // So we will make our own array, sort, then then walk through the table and reorder
        // FIXME: Until better methods are developed, only sortOrder based sorts are supported
        //        and fnCompare is obsolete

        // Create an array of rows to sort
        var rows = [];
        var trs = tbody.rows;
        $(trs).each(function(ix) {
            rows.push(new sortTable.row(this, sortColumns));
        });

        // Sort the array
        rows.sort(sortTable.rowCmp);

        // most efficient reload of sorted rows I have found
        var sortedRows = jQuery.map(rows, function(row, i) { 
                return row.row; 
        });
        $(tbody).append(sortedRows);

        sortTable.tbody=tbody;
        sortTable.columns=sortColumns;
        // Avoid js timeout
        setTimeout(function() { 
                        sortTable.sortFinish(sortTable.tbody,sortTable.columns); 
                    },5);
    },

    sortFinish: function (tbody,sortColumns)
    {   // Additional sort cleanup.
        // This is in a separate function to allow calling with setTimeout() which will
        // prevent javascript timeouts (I hope)
        sortTable.savePositions(tbody);
        if ($(tbody).hasClass('altColors'))
            sortTable.alternateColors(tbody,sortColumns);
        $(tbody).parents("table.tableWithDragAndDrop").each(function (ix) {
            tableDragAndDropRegister(this);
        });
        if (sortTable.loadingId)
            hideLoadingImage(sortTable.loadingId);
    },

    sortByColumns: function (tbody,sortColumns)
    {   // Will sort the table based on the abbr values on a set of <TH> colIds
        // Expects tbody to not sort thead, but could take table

        // Used to use 'sorting' class, but showLoadingImage results in much less screen redrawing
        // For IE especially this was difference between dead/timedout scripts and working sorts!
        var id = $(tbody).attr('id');
        if (!id || id.length === 0) {
            $(tbody).attr('id',"tbodySort"); // Must have some id!
            id = $(tbody).attr('id');
        }
        if ($(tbody).css('display') === 'none') {
            // suppress loading image if element is hidden (and consequently has no position)
            sortTable.loadingId = null;
        } else {
            sortTable.loadingId = showLoadingImage(id);
        }
        sortTable.tbody=tbody;
        sortTable.columns=sortColumns;
        // This allows hiding the rows while sorting!
        setTimeout(function() { 
                        sortTable.sort(sortTable.tbody,sortTable.columns); 
                    },50); 
    },

    trAlternateColors: function (tbody,rowGroup,cellIx)
    {   // Will alternate colors for visible table rows.
        // If cellIx(s) provided then color changes when the column(s) abbr or els innerHtml changes
        // If no cellIx is provided then alternates on rowGroup (5= change color 5,10,15,...)
        // Expects tbody to not color thead, but could take table
        var darker   = false; // === false will trigger first row to be change color = darker

        if (arguments.length<3) { // No columns to check so alternate on rowGroup

            if (!rowGroup || rowGroup === 0)
                rowGroup = 1;
            var curCount = 0; // Always start with a change
            $(tbody).children('tr:visible').each( function(i) {
                if (curCount === 0) {
                    curCount = rowGroup;
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
            var cIxs = [];
            for (var aIx=2; aIx < arguments.length; aIx++) {   // multiple columns
                cIxs[aIx-2] = arguments[aIx];
            }
            $(tbody).children('tr:visible').each( function(i) {
                curContent = "";
                for (var ix=0; ix < cIxs.length; ix++) {
                    if (this.cells[cIxs[ix]]) {
                        curContent += (this.cells[cIxs[ix]].abbr !== "" ?
                                    this.cells[cIxs[ix]].abbr       :
                                    this.cells[cIxs[ix]].innerHTML );
                    }
                }
                if (lastContent !== curContent) {
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
    },

    alternateColors: function (tbody)
    {   // Will alternate colors based upon sort columns (which may be passed in as second arg,
        // or discovered).  Expects tbody to not color thead, but could take table
        var sortColumns;
        if (arguments.length > 1)
            sortColumns = arguments[1];
        else {
            var table = tbody;
            if ($(table).is('tbody'))
                table = $(tbody).parent();
            sortColumns = new sortTable.columnsFromTable(table);
        }

        if (sortColumns) {
            if (sortColumns.cellIxs.length === 1)
                sortTable.trAlternateColors(tbody,0,sortColumns.cellIxs[0]);
            else if (sortColumns.cellIxs.length === 2)
                sortTable.trAlternateColors(tbody,0,sortColumns.cellIxs[0],sortColumns.cellIxs[1]);
            else // Three columns is plenty
                sortTable.trAlternateColors(tbody,0,sortColumns.cellIxs[0],sortColumns.cellIxs[1],
                                                                            sortColumns.cellIxs[2]);
        } else {
            sortTable.trAlternateColors(tbody,5); // alternates every 5th row
        }
    },

    orderFromColumns: function (sortColumns)
    {// Creates the trackDB setting entry sortOrder subGroup1=+ ... from a sortColumns structure
        fields = [];
        for (var ix=0; ix < sortColumns.cellIxs.length; ix++) {
            if (sortColumns.tags[ix] && sortColumns.tags[ix].length > 0)
                fields[ix] = sortColumns.tags[ix] + "=" + (sortColumns.reverse[ix] ? "-":"+");
            else
                fields[ix] = sortColumns.cellIxs[ix] + "=" + (sortColumns.reverse[ix] ? "-":"+");
        }
        var sortOrder = fields.join(' ');
        return sortOrder;
    },

    orderUpdate: function (table,sortColumns,addSuperscript)
    {// Updates the sortOrder in a sortable table
        if (addSuperscript === undefined || addSuperscript === null)
            addSuperscript = false;
        if ($(table).is('tbody'))
            table = $(table).parent();
        var tr = $(table).find('tr.sortable')[0];
        if (tr) {
            for (cIx=0; cIx < sortColumns.cellIxs.length; cIx++) {
                var th = tr.cells[sortColumns.cellIxs[cIx]];
                /* jshint loopfunc: true */// function inside loop works and replacement is awkward.
                $(th).each(function(i) {
                    // First remove old sort classes
                    var classList = $( this ).attr("class").split(" ");
                    if (classList.length < 2) // assertable
                        return;
                    classList = aryRemove(classList,["sortable"]);
                    while (classList.length > 0) {
                        var aClass = classList.pop();
                        if (aClass.indexOf("sort") === 0)
                            $(this).removeClass(aClass);
                    }

                    // Now add current sort classes
                    $(this).addClass("sort"+(cIx+1));
                    if (sortColumns.reverse[cIx])
                        $(this).addClass("sortRev");

                    // update any superscript
                    sup = $(this).find('sup')[0];
                    if (sup || addSuperscript) {
                        var content = (sortColumns.reverse[cIx] === false ? "&darr;":"&uarr;");

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
            if (inp) {
                $(inp).val(sortTable.orderFromColumns(sortColumns));
                if (!addSuperscript && typeof(subCfg) === "object") // subCfg.js file included?
                    subCfg.markChange(null,inp);     // use instead of change() because type=hidden!
            }
        }
    },

    orderFromTr: function (tr)
    {   // Looks up the sortOrder input value from a *.sortable header row of a sortable table
        var inp = $(tr).find('input.sortOrder')[0];
        if (inp)
            return $(inp).val();
        else {
            // create something like "cellType=+ rep=+ protocol=+ treatment=+ factor=+ view=+"
            var fields = [];
            var cells = $(tr).find('th.sortable');
            $(cells).each(function (i) {
                var classList = $( this ).attr("class").split(" ");
                if (classList.length < 2) // assertable
                    return;
                classList = aryRemove(classList,["sortable"]);
                var reverse = false;
                var sortIx = -1;
                while (classList.length > 0) {
                    var aClass = classList.pop();
                    if (aClass.indexOf("sort") === 0) {
                        if (aClass === "sortRev")
                            reverse = true;
                        else {
                            aClass = aClass.substring(4);  // clip off the "sort" portion
                            var ix = parseInt(aClass);
                            if (!isNaN(ix)) {
                                sortIx = ix;
                            }
                        }
                    }
                }
                if (sortIx >= 0) {
                    if (this.id && this.id.length > 0)
                        fields[sortIx] = this.id + "=" + (reverse ? "-":"+");
                    else
                        fields[sortIx] = this.cellIndex + "=" + (reverse ? "-":"+");
                }
            });
            if (fields.length > 0) {
                if (!fields[0])
                    fields.shift();  // 1 based sort ix and 0 based fields ix
                return fields.join(' ');
            }
        }
        return "";
    },

    columnsFromSortOrder: function (sortOrder)
    {   // Creates sortColumns struct (without cellIxs[]) from a trackDB.sortOrder setting string
        this.tags = [];
        this.reverse = [];
        var fields = sortOrder.split(" "); // sortOrder looks like: "cell=+ factor=+ view=+"
        while (fields.length > 0) {
            var pair = fields.shift().split("=");  // Take first and split into
            if (pair.length === 2) {
                this.tags.push(pair[0]);
                this.reverse.push(pair[1] !== '+');
            }
        }
    },

    columnsFromTr: function (tr,silent)
    {   // Creates a sortColumns struct from entries in the 'tr.sortable' heading row of the table
        this.inheritFrom = sortTable.columnsFromSortOrder;
        var sortOrder = sortTable.orderFromTr(tr);
        if (sortOrder.length === 0 && !silent) {
            // developer needs to know something is wrong
            warn("Unable to obtain sortOrder from sortable table.");   
            return;
        }

        this.inheritFrom(sortOrder);
        // Add two additional arrays
        this.cellIxs = [];
        this.useAbbr = [];
        var ths = $(tr).find('th.sortable');
        for (var tIx=0; tIx < this.tags.length; tIx++) {
            for (ix=0; ix < ths.length; ix++) {
                if ((ths[ix].id && ths[ix].id === this.tags[tIx])
                ||  (ths[ix].cellIndex === parseInt(this.tags[tIx])))
                {
                    this.cellIxs[tIx] = ths[ix].cellIndex;
                    this.useAbbr[tIx] = (ths[ix].abbr.length > 0);
                }
            }
        }
        if (this.cellIxs.length === 0 && !silent) {
            warn("Unable to find any sortOrder.cells for sortable table.  ths.length:"+ths.length + 
                 " tags.length:"+this.tags.length + " sortOrder:["+sortOrder+"]");
            return;
        }
    },

    columnsFromTable: function (table)
    {// Creates a sortColumns struct from the contents of a 'table.sortable'
        this.inheritNow = sortTable.columnsFromTr;
        var tr = $(table).find('tr.sortable')[0];
        this.inheritNow(tr);
    },

    _sortOnButtonPress: function (anchor)
    {   // Updates the sortColumns struct and sorts the table when a column header has been pressed
        // If the current primary sort column is pressed, its direction is toggled then the table
        // is sorted. If a secondary sort column is pressed, it is moved to the primary spot and
        // sorted in fwd direction
        var th=$(anchor).closest('th')[0];  // Note that anchor is <a href> within th, not th
        var tr=$(th).parent();
        var theOrder = new sortTable.columnsFromTr(tr);
        var oIx = th.cellIndex;
        for (oIx=0; oIx < theOrder.cellIxs.length; oIx++) {
            if (theOrder.cellIxs[oIx] === th.cellIndex)
                break;
        }
        if (oIx === theOrder.cellIxs.length) {
            // Developer must be warned that something is wrong with sortable table setup
            warn("Failure to find '"+th.id+"' in sort columns."); 
            return;
        }
        if (oIx > 0) { // Need to reorder
            var newOrder = new sortTable.columnsFromTr(tr);
            var nIx=0; // button pushed puts this 'tagId' column first in new order
            newOrder.tags[nIx] = theOrder.tags[oIx];
            newOrder.reverse[nIx] = false;  // When moving to the first position sort forward
            newOrder.cellIxs[nIx] = theOrder.cellIxs[oIx];
            newOrder.useAbbr[nIx] = theOrder.useAbbr[oIx];
            for (var ix=0; ix < theOrder.cellIxs.length; ix++) {
                if (ix !== oIx) {
                    nIx++;
                    newOrder.tags[nIx]    = theOrder.tags[ix];
                    newOrder.reverse[nIx] = theOrder.reverse[ix];
                    newOrder.cellIxs[nIx] = theOrder.cellIxs[ix];
                    newOrder.useAbbr[nIx] = theOrder.useAbbr[ix];
                }
            }
            theOrder = newOrder;
        } else { // if (oIx === 0) {   // need to reverse directions
            theOrder.reverse[oIx] = (theOrder.reverse[oIx] === false);
        }
        var table=$(tr).closest("table.sortable")[0];
        if (table) { // assertable
            sortTable.orderUpdate(table,theOrder);  // Must update sortOrder first!
            var tbody = $(table).find("tbody.sortable")[0];
            sortTable.sortByColumns(tbody,theOrder);
        }
        return;
    },

    sortOnButtonPress: function (anchor,tagId)
    {   // wrapper for the real worker: _sortOnButtonPress()
        var table = $( anchor ).closest("table.sortable")[0];
        if (table) {
            waitOnFunction( sortTable._sortOnButtonPress, anchor, tagId);
        }
        return false;  // called by link so return false means don't try to go anywhere
    },

    sortUsingColumns: function (table) // NOT USED
    {   // Sorts a table body based upon the marked columns
        var columns = new sortTable.columnsFromTable(table);
        tbody = $(table).find("tbody.sortable")[0];
        if (tbody)
            sortTable.sortByColumns(tbody,columns);
    },

    savePositions: function (table)
    {   // Sets the value for the input.trPos of a table row.  Typically this is a "priority" for
        // a track.  This gets called by sort or dragAndDrop in order to allow the new order to
        // affect hgTracks display
        var inputs = $(table).find("input.trPos");
        $( inputs ).each( function(i) {
            var tr = $( this ).closest('tr')[0];
            var trIx = $( tr ).attr('rowIndex').toString();
            if ($( this ).val() != trIx) {
                $( this ).val( trIx );
                if (typeof(subCfg) === "object")  // NOTE: couldn't get $(this).change() to work.
                    subCfg.markChange(null,this); //    probably because this is input type=hidden!
            }
        });
    },

    ///// Following functions are for Sorting by priority
    trPriorityFind: function (tr)
    {   // returns the position (*.priority) of a sortable table row
        var inp = $(tr).find('input.trPos')[0];
        if (inp)
            return $(inp).val();
        return 999999;
    },

    trPriorityCmp: function (tr1,tr2)  // UNUSED FUNCTION
    {   // Compare routine for sorting by *.priority
        var priority1 = sortTable.trPriorityFind(tr1);
        var priority2 = sortTable.trPriorityFind(tr2);
        return priority2 - priority1;
    },

    tablesSortAtStartup: function ()  // DEAD CODE?
    {   // Called at startup if you want javascript to initialize and sort all your
        // class='sortable' tables
        // IMPORTANT: This function WILL ONLY sort by first column.
        // If there are multiple sort columns, please presort the list for accurtacy!!!
        var tables = $("table.sortable");
        $(tables).each(function(i) {
            sortTable.initialize(this,true); // Will initialize superscripts
            sortTable.sortUsingColumns(this);
        });
    },

    initialize: function (table,addSuperscript,altColors)
    {// Called if you want javascript to initialize your class='sortable' table.
    // A sortable table requires:
    // TABLE.sortable: TABLE class='sortable' containing a THEAD header and
    //                 sortable TBODY filled with the rows to sort.
    // THEAD.sortable: (NOTE: created if not found) THEAD can contain multiple rows must contain:
    //   TR.sortable: exactly 1 header TH (table row) class='sortable' which will declare
    //                the sort columnns:
    //   TH.sortable: 1 or more TH (table column headers) with class='sortable sort1 [sortRev]'
    //                (or sort2, sort3) declaring sort order and whether reversed. e.g.:
    //                <TH id='factor' class='sortable sortRev sort3' nowrap>...</TH>
    //                (this means that factor is currently the third sort column and reverse sorted)
    //          NOTE: If no TH.sortable is found, then every th in the TR.sortable will be
    //                converted for you and will be in sort1,2,3 order.)
    //     ONCLICK: Each TH.sortable must call sortTable.sortOnButtonPress(this) directly or
    //              indirectly in the onclick event.  e.g.:
    //              <TH id='factor' class='sortable sortRev sort3' nowrap title='Sort on column' 
    //                              onclick="return sortTable.sortOnButtonPress(this);">
    //              NOTE: onclick function will automatically be added if not found.
    //     SUP: Each TH.sortable *may* contain a <sup> which will be filled with an up or down
    //          arrow and the column's sort order: e.g. <sup>&darr;2</sup>
    //          NOTE: If no sup is found but addSuperscript is requested, then they will be added.
    // TBODY.sortable: (NOTE: created if not found) The TBODY class='sortable' contains the
    //                 table rows that get sorted:
    //                 TBODY->TR & ->TD: Each row contains a TD for each sortable column.
    //                 The innerHTML (entire contents) of the cell will be used for sorting.
    //   TRICK: You can use the 'abbr' field to subtly alter the sortable contents.
    //          Otherwise sorts on td contents ($(td).text()).  Use the abbr field to make
    //          case-insensitive sorts or force exceptions to alpha-text order 
    //          (such as ZCTRL vs Control forcing controls to bottom). e.g.:
    //             <TD id='wgEncodeBroadHist...' nowrap abbr='ZCTRL' align='left'>Control</TD>
    //          IMPORTANT: You must add abbr='use' to the TH.sortable definitions.
    // Finally if you want the tableSort to alternate the table row colors
    // (using #FFFEE8 and #FFF9D2) then TBODY.sortable should also have class 'altColors'
    // NOTE: This class can be added by using the altColors option to this function
    // To override, specify <TBODY class='sortable noAltColors'>
    // NOTE: Add class 'initBySortOrder' to have an sort by column performed at document initialization time, using
    // the saved sortOrder cart variable 
    
        if (altColors === undefined || altColors === null) // explicitly default this boolean
            altColors = false;

        if ($(table).hasClass('sortable') === false) {
            warn('Table is not sortable');
            return;
        }
        var tr = $(table).find('tr.sortable')[0];
        if (!tr) {
            tr = $(table).find('tr')[0];
            if (!tr) {
                warn('Sortable table has no rows');
                return;
            }
            $(tr).addClass('sortable');
            //warn('Made first row tr.sortable');
        }
        if ($(table).find('tr.sortable').length !== 1) {
            warn('sortable table contains more than 1 header row declaring sort columns.');
            return;
        }

        // If not TBODY is found, then create, wrapping all but those already in a thead
        tbody = $(table).find('tbody')[0];
        if (!tbody) {
            trs = $(table).find('tr').not('thead tr');
            $(trs).wrapAll("<TBODY class='sortable' />");
            tbody = $(table).find('tbody')[0];
            //warn('Wrapped all trs not in thead.sortable in tbody.sortable');
        }
        if ($(tbody).hasClass('sortable') === false) {
            $(tbody).addClass('sortable');
            //warn('Added sortable class to tbody');
        }
        if (altColors && $(tbody).hasClass('altColors') === false && 
                $(tbody).hasClass('noAltColors') === false) {
            $(tbody).addClass('altColors');
            //warn('Added altColors class to tbody.sortable');
        }
        $(tbody).hide();

        // If not THEAD is found, then create, wrapping first row.
        thead = $(table).find('thead')[0];
        if (!thead) {
            $(tr).wrapAll("<THEAD class='sortable' />");
            thead = $(table).find('thead')[0];
            $(thead).insertBefore(tbody);
            //warn('Wrapped tr.sortable with thead.sortable');
        }
        if ($(thead).hasClass('sortable') === false) {
            $(thead).addClass('sortable');
            //warn('Added sortable class to thead');
        }

        var sortColumns = new sortTable.columnsFromTr(tr,"silent");
        if (!sortColumns || sortColumns.cellIxs.length === 0) {
            // could mark all columns as sortable!
            $(tr).find('th').each(function (ix) {
                $(this).addClass('sortable');
                $(this).addClass('sort'+(ix+1));
                //warn("Added class='sortable sort"+(ix+1)+"' to th:"+this.innerHTML);
            });
            sortColumns = new sortTable.columnsFromTr(tr,"silent");
            if (!sortColumns || sortColumns.cellIxs.length === 0) {
                warn("sortable table's header row contains no sort columns.");
                return;
            }
        }
        // Can wrap all columnn headers with link
        $(tr).find("th.sortable").each(function (ix) {
            if ( ! $(this).attr('onclick') ) {
                $(this).click( function () { sortTable.sortOnButtonPress(this);} );
            }
            if (theClient.isIePre11()) { // Special case for IE since CSS :hover doesn't work
                $(this).hover(
                    function () { $(this).css( { backgroundColor: '#CCFFCC', cursor: 'hand' } ); },
                    function () { $(this).css( { backgroundColor: '#FCECC0', cursor: '' } ); }
                );
            }
            if ( $(this).attr('title') && $(this).attr('title').length === 0) {
                var title = $(this).text().replace(/[^a-z0-9 ]/ig,'');
                if (title.length > 0 && $(this).find('sup'))
                    title = title.replace(/[0-9]$/g,'');
                if (title.length > 0)
                    $(this).attr('title',"Sort list on '" + title + "'." );
                else
                    $(this).attr('title',"Sort list on column." );
            }
        });
        // Now update all of those cells
        sortTable.orderUpdate(table,sortColumns,addSuperscript);
        if ($(tbody).hasClass('initBySortOrder')) {
            sortTable.sortByColumns(tbody,sortColumns);
        }

        // Alternate colors if requested
        if (altColors) {
            sortTable.alternateColors(tbody);
        }

        // Highlight rows?  But on subtrack list, this will mess up "metadata dropdown" coloring.
        // So just exclude tables with drag and drop
        if ($(table).hasClass('tableWithDragAndDrop') === false) {
            $('tbody.sortable').find('tr').hover(
                function(){ $(this).addClass('bgLevel3');
                            $(this).find('table').addClass('bgLevel3'); },
                function(){ $(this).removeClass('bgLevel3');
                            $(this).find('table').removeClass('bgLevel3'); }
            );
        }

        // Finally, make visible
        $(tbody).removeClass('sorting');
        $(tbody).show();
    }
};

function sortTableInitialize(table,addSuperscript,altColors)
{   // legacy in case some static pages still initialize the table the old way
    sortTable.initialize(table,addSuperscript,altColors);
}


 //////////////////////////////
//// findTracks functions ////
/////////////////////////////
var findTracks = {

    updateMdbHelp: function (index)
    {   // update the metadata help links based on currently selected values.
        // If index === 0 we update all help items, otherwise we only update the one === index.
        var db = getDb();
        var disabled = {  // blackList 
            'accession':          1, 'dataVersion':      1, 'dccAccession':    1, 'expId':    1, 
            'geoSampleAccession': 1, 'grant':            1, 'lab':             1, 'labExpId': 1, 
            'labVersion':         1, 'origAssembly':     1, 'obtainedBy':      1, 'region':   1, 
            'replicate':          1, 'setType':          1, 'softwareVersion': 1, 'subId':    1, 
            'tableName':          1, 'tissueSourceType': 1, 'view':            1
        };
        var expected = $('tr.mdbSelect').length;
        var ix=1;
        if (index !== 0) {
            ix=index;
            expected=index;
        }
        for (; ix <= expected; ix++) {
            var helpLink = $("span#helpLink" + ix);
            if (helpLink.length > 0) {
                // NOTE must match METADATA_NAME_PREFIX in hg/hgTracks/searchTracks.c
                var val = $("select[name='hgt_mdbVar" + ix + "']").val();  
                var text = $("select[name='hgt_mdbVar" + ix + "'] option:selected").text();
                helpLink.html("&nbsp;"); // Do not want this with length === 0 later!
                if ( ! disabled[val] ) {
                    var str;
                    if (val === 'cell') {
                        if (db.substr(0, 2) === "mm") {
                            str = "../ENCODE/cellTypesMouse.html";
                        } else {
                            str = "../ENCODE/cellTypes.html";
                        }
                    } else if (val.toLowerCase() === 'antibody') {
                        str = "../ENCODE/antibodies.html";
                    } else {
                        str = "../ENCODE/otherTerms.html#" + val;
                    }
                    helpLink.html("&nbsp;<a target='_blank' " +
                                  "title='detailed descriptions of terms'" + 
                                  " href='" + str + "'>" + text + "</a>");
                }
            }
        }
    },

    mdbVarChanged: function (obj)
    {   // Ajax call to repopulate a metadata vals select when mdb var changes
        // This handles the currnet case when 2 vars have the same name (e.g. advanced, files tabs)

        findTracks.clearFound();  // Changing values so abandon what has been found

        var newVar = $(obj).val();
        // NOTE must match METADATA_NAME_PREFIX in hg/hgTracks/searchTracks.c
        var a = /hgt_mdbVar(\d+)/.exec(obj.name); 
        if (newVar !== undefined && newVar !== null && a && a[1]) {
            var num = a[1];
            if ($('#advancedTab').length === 1 && $('#filesTab').length === 1) {
                $("select.mdbVar[name='hgt_mdbVar"+num+"'][value!='"+newVar+"']").val(newVar);
            }
            var cgiVars = "hgsid=" + getHgsid() + "db=" + getDb() +  "&cmd=hgt_mdbVal" + num + "&var=" + newVar;
            if (document.URL.search('hgFileSearch') !== -1)
                cgiVars += "&fileSearch=1";
            else
                cgiVars += "&fileSearch=0";

            $.ajax({
                    type: "GET",
                    url: "../cgi-bin/hgApi",
                    data: cgiVars,
                    dataType: 'html',
                    trueSuccess: findTracks.handleNewMdbVals,
                    success: catchErrorOrDispatch,
                    error: errorHandler,
                    cache: true,
                    cmd: "hgt_mdbVal" + num, // NOTE must match METADATA_VALUE_PREFIX
                    num: num                 //      in hg/hgTracks/searchTracks.c
                });
        }
        // NOTE: with newJquery, the response is getting a new error (missing ; before statement)
        //       There were also several XML parsing errors.
        // This error is fixed with the addition of "dataType: 'html'," above.
    },

    handleNewMdbVals: function (response, status)
    {   // Handle ajax response (repopulate a metadata val select)
        // This handles the currnet case when 2 vars have the same name (e.g. advanced, files tabs)

        var td = normed($('td#' + this.cmd));
        if (td) {
            $(td).empty();
            $(td).append(response);
            var inp = normed($(td).find('.mdbVal'));
            var tdIsLike = normed($('td#isLike'+this.num));
            if (inp && tdIsLike) {
                if ($(inp).hasClass('freeText')) {
                    $(tdIsLike).text('contains');
                } else if ($(inp).hasClass('wildList') ||  $(inp).hasClass('filterBy')) {
                    $(tdIsLike).text('is among');
                } else {
                    $(tdIsLike).text('is');
                }
            }
            // Do this by 'each' to set noneIsAll individually
            $(td).find('.filterBy').each( function(i) { 
                ddcl.setup(this,'noneIsAll');
            });
        }
        findTracks.updateMdbHelp(this.num);
    },

    mdbValChanged: function (obj)
    {   // Keep all tabs with same selects in sync
        // TODO: Change from name to id based identification and only have one set of inputs in form
        // This handles the currnet case when 2 vars have the same name (e.g. advanced, files tabs)

        findTracks.clearFound();  // Changing values so abandon what has been found

        if ($('#advancedTab').length === 1 && $('#filesTab').length === 1) {
            var newVal = $(obj).val();
            // NOTE must match METADATA_NAME_PREFIX in hg/hgTracks/searchTracks.c
            var a = /hgt_mdbVal(\d+)/.exec(obj.name); 
            if (newVal !== undefined && newVar !== null && a && a[1]) {
                var num = a[1];
                $("input.mdbVal[name='hgt_mdbVal"+num+"'][value!='"+newVal+"']").val(newVal);
                $("select.mdbVal[name='hgt_mdbVal"+num+"'][value!='"+newVal+"']").each( function (i) {
                    $(this).val(newVal);
                    if ($(this).hasClass('filterBy')) {
                        $(this).dropdownchecklist("destroy");
                        ddcl.setup(this,'noneIsAll');
                    }
                });
            }
        }
        //findTracks.searchButtonsEnable(true);
    },

    changeVis: function (seenVis)
    {   // called by onchange of vis
        var visName = $(seenVis).attr('id');
        var trackName = visName.substring(0,visName.length - "_id".length);
        var hiddenVis = $("input[name='"+trackName+"']");
        var tdb = tdbGetJsonRecord(trackName);
        if ($(seenVis).val() !== "hide")
            $(hiddenVis).val($(seenVis).val());
        else {
            var selCb = $("input#"+trackName+"_sel_id");
            $(selCb).attr('checked',false);    // Can't set these to [] because that means default
            $(seenVis).attr('disabled',true);  // setting is used. However, we're explicitly hiding!
            var needSel = (tdb.parentTrack !== undefined && tdb.parentTrack !== null);
            if (needSel) {
                var hiddenSel = $("input[name='"+trackName+"_sel']");
                $(hiddenSel).val('0');  // Can't set it to [] because it means default setting used.
                $(hiddenSel).attr('disabled',false);
            }
            if (tdbIsSubtrack(tdb))
                $(hiddenVis).val("[]");
            else
                $(hiddenVis).val("hide");
        }
        $(hiddenVis).attr('disabled',false);

        $('input.viewBtn').val('View in Browser');
    },

    clickedOne: function (selCb,justClicked)
    {   // called by on click of CB and findTracks.checkAll()
        var selName = $(selCb).attr('id');
        var trackName = selName.substring(0,selName.length - "_sel_id".length);
        var hiddenSel = $("input[name='"+trackName+"_sel']");
        var seenVis = $('select#' + trackName + "_id");
        var hiddenVis = $("input[name='"+trackName+"']");
        var tr = $(selCb).parents('tr.found');
        var tdb = tdbGetJsonRecord(trackName);
        var isHub = trackName.slice(0,4) === "hub_";
        var hubUrl = isHub ? tdb.hubUrl : "";
        var needSel = (typeof(tdb.parentTrack) === 'string' && tdb.parentTrack !== '');
        var shouldPack = tdb.canPack && tdb.kindOfParent === 0; // If parent then not pack but full
        if (shouldPack
        &&  tdb.shouldPack !== undefined && tdb.shouldPack !== null && !tdb.shouldPack)
            shouldPack = false;
        var checked = $(selCb).attr('checked');

        // First deal with seenVis control
        if (checked) {
            $(seenVis).attr('disabled', false);
            if ($(seenVis).attr('selectedIndex') === 0) {
                if (shouldPack)
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
        if (!justClicked) {
            if (needSel)
                setHiddenInputs = (checked !== (parseInt($(hiddenSel).val()) === 1));
            else if (checked)
                setHiddenInputs = ($(seenVis).val() !== $(hiddenVis).val());
            else
                setHiddenInputs = ($(hiddenVis).val() !== "hide" && $(hiddenVis).val() !== "[]");
        }
        if (setHiddenInputs) {
            if (checked)
                $(hiddenVis).val($(seenVis).val());
            else if (tdbIsSubtrack(tdb))
                $(hiddenVis).val("[]");
            else
                $(hiddenVis).val("hide");
            $(hiddenVis).attr('disabled',false);

            if (needSel) {
                if (checked)
                    $(hiddenSel).val('1');
                else
                    $(hiddenSel).val('0');  // Can't set it to [] because it means default is used.
                $(hiddenSel).attr('disabled',false);
            }
        }

        // if we selected a track in a public hub that is unconnected, we need to get the
        // hubUrl into the form so the genome browser knows to load that hub. If the hub
        // was already a connected hub, then we don't need to specify anything because it
        // will already be in the cart and we handle the visibility settings like normal.
        // The hubUrl field present in the json indicates this is an unconnected hub
        if (justClicked && hubUrl !== undefined && hubUrl.length > 0) {
            var form = $("form[id='searchResults'");
            var newHubInput = document.createElement("input");
            // if we are a subtrack we need to explicitly hide the parent
            // track so ALL subtracks of the parent don't show up unexpectedly
            if (needSel) {
                var parentTrack = tdb.parentTrack;
                var parentTrackInput = document.createElement("input");
                parentTrackInput.setAttribute("type", "hidden");
                parentTrackInput.setAttribute("name", parentTrack);
                parentTrackInput.setAttribute("value", "hide");
                form.append(parentTrackInput);
            }
            newHubInput.setAttribute("type", "hidden");
            newHubInput.setAttribute("name", "hubUrl");
            newHubInput.setAttribute("value", hubUrl);
            form.append(newHubInput);
        }

        // The "view in browser" button should be enabled/disabled
        if (justClicked) {
            $('input.viewBtn').val('View in Browser');
            findTracks.counts();
        }
    },


    normalize: function ()
    {   // Normalize the page based upon current state of all found tracks
        $('div#found').show();
        var selCbs = $('input.selCb');

        // All should have their vis enabled/disabled appropriately (false means don't update cart)
        $(selCbs).each( function(i) { findTracks.clickedOne(this,false); });

        findTracks.counts();
    },

    normalizeWaitOn: function ()  // UNUSED ?
    {   // Put up wait mask then Normalize the page based upon current state of all found tracks
        waitOnFunction( findTracks.normalize );
    },

    _checkAll: function (check)
    {   // Checks/unchecks all found tracks.
        var selCbs = $('input.selCb');
        $(selCbs).attr('checked',check);

        // All should have their vis enabled/disabled appropriately (false means don't update cart)
        $(selCbs).each( function(i) { findTracks.clickedOne(this,false); });

        $('input.viewBtn').val('View in Browser');
        findTracks.counts();
        return false;  // Pressing button does nothing more
    },

    checkAllWithWait: function (check)
    {
        waitOnFunction( findTracks._checkAll, check);
    },

    searchButtonsEnable: function (enable)
    {   // Displays visible and checked track count
        // NOTE: must match TRACK_SEARCH in hg/inc/searchTracks.h
        var searchButton = $('input[name="hgt_tSearch"]'); 
        var clearButton  = $('input.clear');
        if (enable) {
            $(searchButton).attr('disabled',false);
            $(clearButton).attr('disabled',false);
        } else {
            $(searchButton).attr('disabled',true);
            $(clearButton).attr('disabled',true);
        }
    },

    counts: function ()
    {   // Displays visible and checked track count
        var counter = normed($('.selCbCount'));
        if (counter) {
            var selCbs =  $("input.selCb");
            if (selCbs && selCbs.length > 0)
                $(counter).text("("+$(selCbs).filter(":enabled:checked").length + " of " +
                                                                    $(selCbs).length+ " selected)");
        }
    },

    clearFound: function ()
    {   // Clear found tracks and all input controls
        var found = normed($('div#found'));
        if (found)
            $(found).remove();
        found = normed($('div#filesFound'));
        if (found)
            $(found).remove();
        return false;
    },

    clear: function ()
    {   // Clear found tracks and all input controls
        findTracks.clearFound();
        $('input[type="text"]').val(''); // This will always be found
        $('select.mdbVal').attr('selectedIndex',0); // Should be 'Any'
        $('select.filterBy').each( function(i) { // Do this by 'each' to set noneIsAll individually
            //$(this).dropdownchecklist("refresh");  // requires v1.1
            $(this).dropdownchecklist("destroy");
            $(this).show();
            ddcl.setup(this,'noneIsAll');
        });

        $('select.groupSearch').attr('selectedIndex',0);
        $('select.typeSearch').attr('selectedIndex',0);
        //findTracks.searchButtonsEnable(false);
        return false;
    },

    sortNow: function (obj)
    {   // Called by radio button to sort tracks
        if ($('#sortIt').length === 0)
            $('form#trackSearch').append("<input TYPE=HIDDEN id='sortIt' name='"+
                                             $(obj).attr('name')+"' value='"+$(obj).val()+"'>");
        else
            $('#sortIt').val($(obj).val());

        // How to hold onto selected tracks?  There are 2 separate forms.  
        // Scrape named inputs from searchResults form and dup them on trackSearch?
        var inp = $('form#searchResults').find('input:hidden').not(':disabled').not(
                                                                                "[name='hgsid']");
        if ($(inp).length > 0) {
            $(inp).appendTo('form#trackSearch');
            // Must be post to avoid url too long  NOTE: probably needs to be post anyway
            $('form#trackSearch').attr('method','POST'); 
        }

        $('#searchSubmit').click();
        return true;
    },

    page: function (pageVar,startAt)
    {   // Called by radio button to sort tracks
        var pager = $("input[name='"+pageVar+"']");
        if ($(pager).length === 1)
            $(pager).val(startAt);

        // How to hold onto selected tracks?  There are 2 separate forms.  
        // Scrape named inputs from searchResults form and dup them on trackSearch?
        var inp = $('form#searchResults').find('input:hidden').not(':disabled').not(
                                                                                "[name='hgsid']");
        if ($(inp).length > 0) {
            $(inp).appendTo('form#trackSearch');
            // Must be post to avoid url too long  NOTE: probably needs to be post anyway
            $('form#trackSearch').attr('method','POST'); 
        }

        $('#searchSubmit').click();
        return false;
    },

    configSet: function (name)
    {   // Called when configuring a composite or superTrack
        var thisForm =  $('form#searchResults');
        $(thisForm).attr('action',"../cgi-bin/hgTrackUi?hgt_tSearch=Search&g="+name);
        $(thisForm).find('input.viewBtn').click();
    },

    mdbSelectPlusMinus: function (obj)
    {   // Now [+][-] mdb var rows with javascript rather than cgi roundtrip
        // Will remove row or clone new one.  Complication is that 'advanced' and 'files'
        // tab duplicate the tables!
        var objId = $(obj).attr('id');
        var rowNum = objId.substring(objId.length - 1);
        var val = $(obj).text();
        if (!val || val.length === 0)
            val = $(obj).val(); // Remove this when non-CSS buttons go away
        var buttons;
        if (val === '+') {
            buttons = $("#plusButton"+rowNum);  // Two tabs may have the exact same buttons!
            if (buttons.length > 0) {
                var table = null;
                $(buttons).each(function (i) {
                    var tr = $(this).parents('tr.mdbSelect')[0];
                    if (tr) {
                        table = $(tr).parents('table')[0];
                        var newTr = $(tr).clone();
                        var element = $(newTr).find("td[id^='hgt_mdbVal']")[0];
                        if (element)
                            $(element).empty();
                        element = $(newTr).find("td[id^='isLike']")[0];
                        if (element)
                            $(element).empty();
                        $(tr).after( newTr );
                        element = $(newTr).find("select.mdbVar")[0];
                        if (element)
                            $(element).attr('selectedIndex',-1); // chrome needs this after 'after'
                    }
                });
                if (table)
                    findTracks.mdbSelectRowsNormalize(table); // magic is in this function
                return false;
            }
        } else { // === '-'
            buttons = $("#minusButton"+rowNum);  // Two tabs may have the exact same buttons!
            if (buttons.length > 0) {
                var remaining = 0;
                $(buttons).each(function (i) {
                    var tr = $(this).parents('tr')[0];
                    var table = $(tr).parents('table')[0];
                    if (tr)
                        $(tr).remove();
                    // Must renormalize since 2nd of 3 rows may have been removed
                    remaining = findTracks.mdbSelectRowsNormalize(table);  
                });
                // Got to remove the cart vars, though it doesn't matter which as
                // count must not be too many.
                if (remaining > 0) {
                    removeNum = remaining + 1;  
                    setCartVars( ["hgt_mdbVar"+removeNum,"hgt_mdbVal"+removeNum ], [ "[]","[]" ] );
                }

                findTracks.clearFound();  // Changing values so abandon what has been found
                return false;
            }
        }
        return true;
    },

    mdbSelectRowsNormalize: function (table)
    { // Called when [-][+] buttons changed the number of mdbSelects in findTracks\
      // Will walk through each row and get the numberings of addressable elements correct.
        if (table) {
            var mdbSelectRows = $(table).find('tr.mdbSelect');
            var needMinus = (mdbSelectRows.length > 2);
            $(table).find('tr.mdbSelect').each( function (ix) {
                var rowNum = ix + 1;  // Each [-][+] and mdb var=val pair of selects must be numbered

                // First the [-][+] buttons
                var plusButton = $(this).find("[id^='plusButton']")[0];
                if (plusButton) {  // should always be a plus button
                    var oldNum =  Number(plusButton.id.substring(plusButton.id.length - 1));
                    if (oldNum === rowNum)
                        return;  // that is continue with the next row

                    $(plusButton).attr('id',"plusButton"+rowNum);
                    var minusButton = $(this).find("[id^='minusButton']")[0];
                    if (needMinus) {
                        if (!minusButton) {
                            if ($(plusButton).hasClass('pmButton'))
                                $(plusButton).before("<span class='pmButton' id='minusButton"+
                                         rowNum+"' title='delete this row' "+
                                         "onclick='findTracks.mdbSelectPlusMinus(this);'>-</span>");
                            else   // Remove this else when non-CSS buttons go away
                                $(plusButton).before("<input type='button' id='minusButton"+rowNum+
                                     "' value='-' style='font-size:.7em;' title='delete this row'" +
                                     " onclick='return findTracks.mdbSelectPlusMinus(this);'>");
                        } else
                            $(minusButton).attr('id',"minusButton"+rowNum);
                    } else if (minusButton)
                        $(minusButton).remove();
                }

                // Now the mdb var=val pair of selects
                var element = $(this).find(".mdbVar")[0];  // select var
                if (element)
                    $(element).attr('name','hgt_mdbVar' + rowNum);

                element = $(this).find(".mdbVal")[0];      // select val
                if (element) {                // not there if new row
                    $(element).attr('name','hgt_mdbVal' + rowNum);
                    if ($(element).hasClass('filterBy')) {
                        $(element).attr('id',''); // removing id ensures renumbering id
                        ddcl.reinit([ element ],true);
                    }
                }

                // A couple more things
                element = $(this).find("td[id^='isLike']")[0];
                if (element)
                    $(element).attr('id','isLike' + rowNum);
                element = $(this).find("td[id^='hgt_mdbVal']")[0];
                if (element)
                    $(element).attr('id','hgt_mdbVal' + rowNum);
            });

            return mdbSelectRows.length;
        }
        return 0;
    },

    switchTabs: function (ui)
    {   // switching tabs on findTracks page

        id = ui.newPanel[0].id;
        if (id === 'simpleTab' && $('div#found').length < 1) {
            // delay necessary, since select event not afterSelect event
            setTimeout(function() { 
                            $('input#simpleSearch').focus(); 
                        },20); 
        } else if (id === 'advancedTab') {
            // Advanced tab has DDCL wigets which were sized badly because the hidden width
            // was unknown delay necessary, since select event not afterSelect event
            setTimeout(function() { 
                            ddcl.reinit($('div#advancedTab').find('select.filterBy'),false); 
                        },20);
        }
        if ($('div#filesFound').length === 1) {
            if (id === 'filesTab')
                $('div#filesFound').show();
            else
                $('div#filesFound').hide();
        }
        if ($('div#found').length === 1) {
            if (id !== 'filesTab')
                $('div#found').show();
            else
                $('div#found').hide();
        }
        // explicitly set the hidden form input for the current tab because jquery-ui won't do
        // it for us anymore
        $("#currentTab").val(id);
    }
};

function escapeJQuerySelectorChars(str)
{   // replace characters which are reserved in jQuery selectors
    // (surprisingly jQuery does not have a built in function to do this).
    return str.replace(/([!"#$%&'()*+,./:;<=>?@[\]^`{|}~"])/g,'\\$1');
}

var preloadImages = [];
var preloadImageCount = 0;
function preloadImg(url)  // DEAD CODE?
{
// force an image to be loaded (e.g. for images in menus or dialogs).
    preloadImages[preloadImageCount] = new Image();
    preloadImages[preloadImageCount].src = url;
    preloadImageCount++;
}


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

  ///////////////////////////
 //// Drag Reorder Code ////
///////////////////////////
var dragReorder = {
    originalHeights: {}, // trackName: startHeight

    setOrder: function (table)
    {   // Sets the 'order' value for the image table after a drag reorder
        var varsToUpdate = {};
        $("tr.imgOrd").each(function (i) {
            if ($(this).prop('abbr') !== $(this).prop('rowIndex').toString()) {
                $(this).attr('abbr', $(this).prop('rowIndex').toString());
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
            if (this.getAttribute('abbr') !== this.getAttribute('rowIndex')) {
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
        var startIndex = $(row).prop('rowIndex');
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
            if ( ! imageV2.lastTrack || imageV2.lastTrack.id !== id) {
                imageV2.lastTrack = rightClick.makeMapItem(id);
                // currentMapItem gets set by mapItemMapOver.   This is just backup
            }

            if (typeof greyBarIcons !== 'undefined' && greyBarIcons === true) {
                // add a gear icon over the grey bar to bring up the context menu
                let tdBtn = document.getElementById("td_btn_" + id);
                if (tdBtn) {
                    if (!document.getElementById("gear_btn_" + id)) {
                        let span = document.createElement("span");
                        span.id = "gear_btn_" + id;
                        span.classList.add("hgTracksGearIcon", "ui-icon", "ui-icon-gear");
                        span.title = "Configure track";
                        tdBtn.appendChild(span);
                        tdBtn.style.position = "relative";
                        addMouseover(span, span.title);
                        span.addEventListener("click", (e) => {
                            // create a contextmenu event that the imgTbl will pick up
                            e.preventDefault();
                            e.stopPropagation();
                            e.stopImmediatePropagation();
                            const rightClickEvent = new MouseEvent("contextmenu", {
                                bubbles: true,
                                cancelable: true,
                                view: window,
                                clientX: tdBtn.getBoundingClientRect().left + 15,
                                clientY: tdBtn.getBoundingClientRect().top,
                                button: 2,
                            });
                            tdBtn.dispatchEvent(rightClickEvent);
                        });
                    }
                }

                // add an 'x' icon in the label area to hide the track
                let tdSide = document.getElementById("td_side_" + id);
                if (tdSide) {
                    // mouseover event fires if you stop moving the mouse while still
                    // hovering the element and then move it again, don't make
                    // duplicate btns in that case
                    if (!document.getElementById("close_btn_" + id)) {
                        let btn = document.createElement("span");
                        btn.id = "close_btn_" + id;
                        btn.classList.add("hgTracksCloseIcon", "ui-icon", "ui-icon-close");
                        btn.title = "Hide track";
                        tdSide.appendChild(btn);
                        addMouseover(btn, btn.title);
                        tdSide.style.position = "relative";
                        if (hgTracks && hgTracks.revCmplDisp) {
                            // set up 'x' icon to the right
                            btn.classList.add("hgTracksCloseIconRight");
                        } else {
                            // set up 'x' icon to the left
                            btn.classList.add("hgTracksCloseIconLeft");
                        }
                        btn.addEventListener("click", (e) => {
                           rightClick.hideTracks([id]);
                        });
                    }
                }
            }
        }
    },

    trMouseLeave: function(e)
    {
        var id = '';
        var a = /tr_(.*)/.exec($(this).attr('id'));  // voodoo
        if (a && a[1]) {
            id = a[1];
        }
        if (id.length > 0) {
            // remove 'x' icon in the label area to hide the track
            let tdSide = document.getElementById("td_side_" + id);
            if (tdSide) {
                let btn = document.getElementById("close_btn_" + id);
                if (btn) {
                    btn.remove();
                }
            }

            // remove gear icon over the grey bar
            let tdBtn = document.getElementById("td_btn_" + id);
            if (tdBtn) {
                let btn = document.getElementById("gear_btn_" + id);
                if (btn) {
                    btn.remove();
                }
            }
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
                // if the custom mouseover code has removed this title, check the attr
                // for the original title
                if (this.title.length === 0) {
                    rightClick.currentMapItem.title = this.getAttribute("originalTitle");
                }

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
            $(btns).on("mouseenter", dragReorder.buttonMouseOver );
            $(btns).on("mouseleave", dragReorder.buttonMouseOut  );
            $(btns).show();
        }
        var handle = $("td.dragHandle");
        if (handle.length > 0) {
            $(handle).on("mouseenter", dragReorder.dragHandleMouseOver );
            $(handle).on("mouseleave", dragReorder.dragHandleMouseOut  );
        }

        // setup mouse callbacks for the area tags
        $("#imgTbl").find("tr").on("mouseover", dragReorder.trMouseOver );
        $("#imgTbl").find("tr").on("mouseleave", dragReorder.trMouseLeave );
        $("#imgTbl").find("tr").each( function (i, row) {
            // save the original y positions of each row
            //if (row.id in dragReorder.originalHeights === false) {
                dragReorder.originalHeights[row.id] = row.getBoundingClientRect().y + window.scrollY;
            //}
        });


        $(".area").each( function(t) {
                            this.onmouseover = dragReorder.mapItemMouseOver;
                            this.onmouseout = dragReorder.mapItemMouseOut;
                            this.onclick = posting.mapClk;
                        });
    }
};

function trackHubSkipHubName(name) {
    // Just like hg/lib/trackHub.c's...
    var matches;
    if (name && (matches = name.match(/^hub_[0-9]+_(.*)/)) !== null) {
        return matches[1];
    } else {
        return name;
    }
}

function replaceReserved(txt) {
    /* This should somehow be made more general so we can stop worrying about
     * user made tracks with whatever characters in it */
    if (!txt) {
        throw new Error("trying to replace null txt");
    }
    return txt.replace(/[^A-Za-z0-9_]/g, "_");
}

function boundingRect(refEl) {
    /* For regular HTML elements, this function wraps getBoundingClientRect(). For area
     * elements like on hgTracks, getBoundingClientRect() won't work and we have to figure
     * everything out from the .coords attribute along with taking into account dragReorder,
     * page scroll, etc */
    if (! (refEl instanceof Element)) {
        // not a map/area element, maybe from some other part of the UI
        console.log("trying to place a mouseover element next to an element that has not been created yet");
        throw new Error();
    }

    // obtain coordinates for placing the mouseover
    let refWidth, refHeight, refX, refY, y1;
    let refRight, refLeft, refTop, refBottom;
    let rect;
    let windowWidth = window.innerWidth;
    let windowHeight = window.innerHeight;
    if (refEl.coords !== undefined && refEl.coords.length > 0 && refEl.coords.split(",").length == 4) {
        // if we are dealing with an <area> element, the refEl width and height
        // are for the whole image and not for just the area, so
        // getBoundingClientRect() will return nothing, sad!
        let refImg = document.querySelector("[usemap='#" + refEl.parentNode.name + "']");
        if (refImg === null) {return;}
        let refImgRect = refImg.getBoundingClientRect();
        let refImgWidth = refImgRect.width;
        let label = document.querySelector("[id^=td_side]");
        let btn = document.querySelector("[id^=td_btn]");
        let labelWidth = 0, btnWidth = 0;
        if (label && btn) {
            labelWidth = label.getBoundingClientRect().width;
            btnWidth = label.getBoundingClientRect().width;
        }
        let imgWidth = refImgWidth;
        if (refEl.parentNode.name !== "ideoMap") {
            imgWidth -= labelWidth - btnWidth;
        }
        let refImgOffsetY = refImgRect.y; // distance from start of image to top of viewport, includes any scroll;
        [x1,y1,x2,y2] = refEl.coords.split(",").map(x => parseInt(x));
        refX = x1 + refImgRect.x;
        refY = y1 + refImgRect.y;
        refRight = x2 + refImgRect.left;
        refLeft = x1 + refImgRect.left;
        refTop = y1 + refImgOffsetY;
        refBottom = y2 + refImgOffsetY;
        refWidth = x2 - x1;
        refHeight = y2 - y1;

    } else {
        rect = refEl.getBoundingClientRect();
        refX = rect.x; refY = rect.y;
        refWidth = rect.width; refHeight = rect.height;
        refRight = rect.right; refLeft = rect.left;
        refTop = rect.top; refBottom = rect.bottom;
    }
    return {bottom: refBottom, height: refHeight,
            left: refLeft, right: refRight,
            top: refTop, width: refWidth,
            x: refX, y: refY};
}

function positionMouseover(ev, refEl, popUpEl, mouseX, mouseY) {
    /* The actual mouseover positioning function.
    * refEl is an already existing element with coords that we use to position popUpEl.
    * popUpEl will try to be as close to the right/above the refEl, except when:
    *     it would extend past the screen in which case it would go left/below appropriately.
    *     the refEl takes up the whole screen, in which case we can cover the refEl
    *     with no consequence */
    rect = boundingRect(refEl);
    refX = rect.x; refY = rect.y;
    refWidth = rect.width; refHeight = rect.height;
    refRight = rect.right; refLeft = rect.left;
    refTop = rect.top; refBottom = rect.bottom;
    let windowWidth = window.innerWidth;
    let windowHeight = window.innerHeight;

    // figure out how large the mouseover will be
    let popUpRect = popUpEl.getBoundingClientRect();
    // position the popUp to the right and above the cursor by default
    // tricky: when the mouse enters the element from the top, we want the tooltip
    // relatively close to the element itself, because the mouse will already be
    // on top of it, leaving it clickable or interactable. But if we are entering
    // the element from the bottom, if we position the tooltip close to the mouse,
    // we obscure the element itself, so we need to leave a bit of extra room
    let topOffset;
    if (Math.abs(mouseY - refBottom) < Math.abs(mouseY - refTop)) {
        // just use the mouseY position for placement, the -15 accounts for enough room
        topOffset = mouseY - window.scrollY - popUpRect.height - 15;
    } else {
        // just use the mouseY position for placement, the -5 accounts for cursor size
        topOffset = mouseY - window.scrollY - popUpRect.height - 5;
    }
    let leftOffset = mouseX; // add 15 for large cursor sizes

    // first case, refEl takes the whole width of the image, so not a big deal to cover some of it
    // this is most common for the track labels
    if (mouseX + popUpRect.width >= (windowWidth - 25)) {
        // move to the left
        leftOffset = mouseX - popUpRect.width;
    }
    // or the mouse is on the right third of the screen
    if (mouseX > (windowWidth* 0.66)) {
        // move to the left
        leftOffset = mouseX - popUpRect.width;
    }

    // the page is scrolled or otherwise off the screen
    if (topOffset <= 0) {
        topOffset = mouseY - window.scrollY;
    }

    if (leftOffset < 0) {
        throw new Error("trying to position off of screen to left");
    }
    popUpEl.style.left = leftOffset + "px";
    popUpEl.style.top = topOffset + "px";
}

// The div that moves around the users screen with the visible mouseover text
let  mouseoverContainer;
// Global needed for contextmenu to disable tooltips
var suppressTooltips = false;

function addMouseover(ele1, text = null, ele2 = null) {
    /* Adds wrapper elements to control various mouseover events using mouseenter/mouseleave. */
    if (!mouseoverContainer) {
        mouseoverContainer = document.createElement("div");
        mouseoverContainer.className = "tooltip";
        mouseoverContainer.style.position = "fixed";
        mouseoverContainer.style.display = "inline-block";
        mouseoverContainer.style.visibility = "hidden";
        mouseoverContainer.style.opacity = "0";
        mouseoverContainer.id = "mouseoverContainer";
        let tooltipTextSize = localStorage.getItem("tooltipTextSize");
        if (tooltipTextSize === null) {tooltipTextSize = window.browserTextSize;}
        mouseoverContainer.style.fontSize =  tooltipTextSize + "px";
        document.body.append(mouseoverContainer);
    }

    if (ele1) {
        ele1.setAttribute("mouseoverText", text);
        // Remove title attribute to prevent default browser tooltip
        if (ele1.title) {
            ele1.setAttribute("originalTitle", ele1.title);
            ele1.title = "";
        }
        // Remove previous listeners if any
        ele1.removeEventListener("mouseenter", ele1._mouseenterHandler);
        ele1.removeEventListener("mouseleave", ele1._mouseleaveHandler);
        // Show tooltip on mouseenter with delay
        ele1._mouseenterHandler = function(e) {
            // Clear any existing hide timeout
            if (ele1._tooltipHideTimeout) {
                clearTimeout(ele1._tooltipHideTimeout);
                ele1._tooltipHideTimeout = null;
            }
            // Determine delay based on tooltip type
            let isDelayedTooltip = ele1.getAttribute("tooltipDelay") === "delayed";
            let delay = isDelayedTooltip ? 1500 : 500;
            ele1._tooltipShowTimeout = setTimeout(function() {
                showTooltipForElement(ele1, e);
            }, delay);
        };
        ele1._mouseleaveHandler = function(e) {
            // Clear show timeout if mouse leaves before delay
            if (ele1._tooltipShowTimeout) {
                clearTimeout(ele1._tooltipShowTimeout);
                ele1._tooltipShowTimeout = null;
            }
            // Use a grace period to allow moving to the tooltip itself
            ele1._tooltipHideTimeout = setTimeout(function() {
                if (!mouseoverContainer._isMouseOver) {
                    hideMouseoverText(mouseoverContainer);
                }
            }, 500); // 500ms grace period
        };
        ele1.addEventListener("mouseenter", ele1._mouseenterHandler);
        ele1.addEventListener("mouseleave", ele1._mouseleaveHandler);
    }

    // Tooltip mouseenter/mouseleave
    mouseoverContainer.addEventListener("mouseenter", function() {
        mouseoverContainer._isMouseOver = true;
        // Clear any hide timeout when entering tooltip
        if (mouseoverContainer._hideTimeout) {
            clearTimeout(mouseoverContainer._hideTimeout);
            mouseoverContainer._hideTimeout = null;
        }
    });
    mouseoverContainer.addEventListener("mouseleave", function() {
        mouseoverContainer._isMouseOver = false;
        // Hide after a short delay to allow for quick mouse movements
        mouseoverContainer._hideTimeout = setTimeout(function() {
            hideMouseoverText(mouseoverContainer);
        }, 100);
    });
}

function showTooltipForElement(ele, ev) {
    // Show the tooltip for the given element
    if (suppressTooltips) {return;}
    let text = ele.getAttribute("mouseoverText");
    if (!text) return;
    mouseoverContainer.replaceChildren();
    let newEl = document.createElement("span");
    newEl.style = "max-width: 400px";
    newEl.innerHTML = text;
    mouseoverContainer.appendChild(newEl);
    positionMouseover(ev, ele, mouseoverContainer, ev.pageX, ev.pageY);
    mouseoverContainer.classList.add("isShown");
    mouseoverContainer.style.opacity = "1";
    mouseoverContainer.style.visibility = "visible";
    mouseoverContainer.setAttribute("origItemMouseoverId", ele.getAttribute("mouseoverid"));
}

function hideMouseoverText(ele) {
    /* Actually hides the tooltip text */
    ele.classList.remove("isShown");
    ele.style.opacity = "0";
    ele.style.visibility = "hidden";
}

function titleTagToMouseover(mapEl) {
    /* for a given area tag, extract the title text into a div that can be positioned
    * like a standard tooltip mouseover next to the item */
    addMouseover(mapEl, mapEl.title);
}

function convertTitleTagsToMouseovers() {
    /* make all the title tags in the document have mouseovers */
    document.querySelectorAll("[title]").forEach(function(a, i) {
        if (a.id !== "" && (a.id === "hotkeyHelp" || a.id.endsWith("Dialog") || a.id.endsWith("Popup"))) {
            // these divs are populated by ui-dialog, they should not have tooltips
            return;
        }
        if (a.title !== undefined &&
                (a.title.startsWith("click & drag to scroll") || a.title.startsWith("drag select or click to zoom")))
            a.title = "";
        else if (a.title !== undefined && a.title.length > 0) {
            if (a.title.startsWith("Click to alter the display density")) {
                // these tooltips have a longer delay:
                a.setAttribute("tooltipDelay", "delayed");
            }
            titleTagToMouseover(a);
        }
    });

    /* Mouseover should clear if you leave the document window altogether */
    document.body.addEventListener("mouseleave", (ev) => {
        hideMouseoverText(mouseoverContainer);
    });

    /* make the mouseovers go away if we are in an input */
    const inps = document.getElementsByTagName("input");
    for (let inp of inps) {
        if (!(inp.type == "hidden" || inp.type == "HIDDEN")) {
            if (inp.type !== "submit") {
                inp.addEventListener("focus", (ev) => {
                    hideMouseoverText(mouseoverContainer);
                });
            } else {
                // the buttons are inputs that don't blur right away (or ever? I can't tell), so
                // be sure to restore the tooltips when they are clicked
                inp.addEventListener("click", (ev) => {
                    hideMouseoverText(mouseoverContainer);
                });
            }
        }
    }
    /* on a select, we can hide the tooltip on focus, but don't disable them
     * altogether, because it's easy to click out of a select without actually
     * losing focus, and we can't detect that because the web browser handles
     * that click separately */
    const sels = document.getElementsByTagName("select");
    for (let sel of sels) {
        sel.addEventListener("focus", (ev) => {
            hideMouseoverText(mouseoverContainer);
        });
        for (let opt of sel.options) {
            opt.addEventListener("click", (evt) => {
                hideMouseoverText(mouseoverContainer);
            });
        }
    }

    /* Make the ESC key hide tooltips */
    document.body.addEventListener("keyup", (ev) => {
        if (ev.keyCode === 27) {
            hideMouseoverText(mouseoverContainer);
        }
    });
}

function parseUrl(url) {
    // turn a url into some of it's components like server, query-string, etc
    let protocol, serverName, pathInfo, queryString, queryArgs = {};
    let temp;
    temp = url.split("?");
    if (temp.length > 1)
        queryString = temp.slice(1).join("?");
    temp = temp[0].split("/");
    protocol = temp[0]; // "https:"
    serverName = temp[2]; // "genome-test.gi.ucsc.edu"
    pathInfo = temp.slice(3).join("/"); // "cgi-bin/hgTracks"
    cgi = pathInfo.startsWith("cgi-bin") ? pathInfo.split('/')[1] : "";
    let i, s = queryString.split('&');
    for (i = 0; i < s.length; i++) {
        argVal = s[i].split('=');
        queryArgs[argVal[0]] = argVal[1];
    }
    return {protocol: protocol, serverName: serverName, pathInfo: pathInfo, queryString: queryString, cgi: cgi, queryArgs: queryArgs};
}

function dumpCart(seconds, skipNotification) {
    // dump current cart
    let currUrl = parseUrl(window.location.href);
    logUrl = currUrl.protocol + "//" + currUrl.serverName + "/" + currUrl.pathInfo + "?hgsid=" + getHgsid() + "&_dumpCart=" + encodeURIComponent(seconds) + "&skipNotif=" + skipNotification;
    let xmlhttp = new XMLHttpRequest();
    xmlhttp.open("GET", logUrl, true);
    xmlhttp.send();  // sends request and exits this function
}

function writeToApacheLog(msg) {
    // send msg to web servers error_log
    // first need to figure out what server and CGI we are requesting:
    let currUrl = parseUrl(window.location.href);
    logUrl = currUrl.protocol + "//" + currUrl.serverName + "/" + currUrl.pathInfo + "?_dumpToLog=" + encodeURIComponent(msg);
    let xmlhttp = new XMLHttpRequest();
    xmlhttp.open("GET", logUrl, true);
    xmlhttp.send();  // sends request and exits this function
}

function addRecentSearch(db, searchTerm, extra={}) {
    // Push a searchTerm onto a stack in localStorage to show users their most recent
    // search terms. If an optional extra argument is supplied (ex: the response from hgSuggest),
    // save that as well
    // The searchStack object (note: saved as a string via JSON.stringify in localStorage) keeps
    // a per database stack of the 5 most recently searched terms, as well as their "result",
    // which can be an autocomplete object from hgSuggest, something from hgSearch, or just nothing
    // Example:
    // var searchStack = {
    //  hg38: {
    //   "stack": ["foxp", "flag", "fla"],
    //   "results: {
    //     "foxp": {
    //       "value": "FOXP1 (Homo sap...",
    //       "id": "chr3:70954708-71583728",
    //       ...
    //     },
    //     "flag": {}, // NOTE: empty object
    //     "fla": {
    //       "value": ...,
    //       "id": ...,
    //     },
    //   }
    // },
    // mm10: {
    //  "stack": [...],
    //  "results": {},
    // }
    let searchStack = window.localStorage.getItem("searchStack");
    let searchObj = {};
    if (searchStack === null) {
        searchObj[db] = {"stack": [searchTerm], "results": {}};
        searchObj[db].results[searchTerm] = extra;
        window.localStorage.setItem("searchStack", JSON.stringify(searchObj));
    } else {
        searchObj = JSON.parse(searchStack);
        if (db in searchObj) {
            let searchList = searchObj[db].stack;
            if (searchList.includes(searchTerm)) {
                // remove it from wherever it is cause it's going to the front
                searchList.splice(searchList.indexOf(searchTerm), 1);
            } else {
                searchObj[db].results[searchTerm] = extra;
                if (searchList.length >= 5) {
                    let toDelete = searchList.pop();
                    delete searchObj[db].results[toDelete];
                }
            }
            searchList.unshift(searchTerm);
            searchObj.stack = searchList;
        } else {
            searchObj[db] = {"stack": [searchTerm], "results": {}};
            searchObj[db].results[searchTerm] = extra;
        }
        window.localStorage.setItem("searchStack", JSON.stringify(searchObj));
    }
}

// variables to parse url arguments correctly
var digitTest = /^\d+$/,
    keyBreaker = /([^\[\]]+)|(\[\])/g,
    plus = /\+/g,
    paramTest = /([^?#]*)(#.*)?$/;

function deparam(params) {
    /* From https://github.com/jupiterjs/jquerymx/blob/master/lang/string/deparam/deparam.js
     * Used by the cell browser */
    if(! params || ! paramTest.test(params) ) {
        return {};
    }

    var data = {},
        pairs = params.split('&'),
        current;

    for (var i=0; i < pairs.length; i++){
        current = data;
        var pair = pairs[i].split('=');

        // if we find foo=1+1=2
        if(pair.length !== 2) {
            pair = [pair[0], pair.slice(1).join("=")];
        }

        var key = decodeURIComponent(pair[0].replace(plus, " ")),
        value = decodeURIComponent(pair[1].replace(plus, " ")),
        parts = key.match(keyBreaker);

        for ( var j = 0; j < parts.length - 1; j++ ) {
            var part = parts[j];
            if (!current[part] ) {
                //if what we are pointing to looks like an array
                current[part] = digitTest.test(parts[j+1]) || parts[j+1] === "[]" ? [] : {};
            }
            current = current[part];
            }
        var lastPart = parts[parts.length - 1];
        if (lastPart === "[]"){
            current.push(value);
        } else{
            current[lastPart] = value;
        }
    }
    return data;
}

function changeUrl(vars, oldVars) {
    /* Save the users search string to the url so web browser can easily
     * cache search results into the browser history
     * vars: object of new key: val pairs like CGI arguments
     * oldVars: arguments we want to keep between calls */
    var myUrl = window.location.href;
    myUrl = myUrl.replace('#','');
    var urlParts = myUrl.split("?");
    var baseUrl;
    if (urlParts.length > 1)
        baseUrl = urlParts[0];
    else
        baseUrl = myUrl;

    var urlVars;
    if (oldVars === undefined) {
        var queryStr = urlParts[1];
        urlVars = deparam(queryStr);
    } else {
        urlVars = oldVars;
    }

    for (var key in vars) {
        var val = vars[key];
        if (val === null || val === "") {
            if (key in urlVars) {
                delete urlVars[key];
            }
        } else {
            urlVars[key] = val;
        }
    }

    var argStr = jQuery.param(urlVars);
    argStr = argStr.replace(/%20/g, "+");

    return {"baseUrl": baseUrl, "args": argStr, "urlVars": urlVars};
}

function saveHistory(obj, urlParts, replace) {
    /* Save an object to the web browser's history stack. When we visit the page for the
     * first time, we replace the basic state after we are done making the initial UI */
    if (replace) {
        history.replaceState(obj, "", urlParts.baseUrl + (urlParts.args.length !== 0 ? "?" + urlParts.args : ""));
    } else {
        history.pushState(obj, "", urlParts.baseUrl + (urlParts.args.length !== 0 ? "?" + urlParts.args : ""));
    }
}

function forceDisplayBedFile(url) {
    var w = window.open('');
    w.document.write('<a class="button" HREF="'+url+'" TARGET=_blank><button>Download File</button></a>&nbsp;');
    w.document.write('<button id="closeWindowLink" HREF="#">Close Tab</button>');
    w.onload = function(ev) {
      // Attach event listeners after the new window is loaded
      w.document.getElementById('closeWindowLink').addEventListener('click', function() { w.close(); } );
    };
    fetch(url).then(response => response.text()) // Read the response as text
    .then(function(text) {
       w.document.write('<pre>' + text + '</pre>'); // Display the content
       w.document.close(); // Close the document to finish rendering
    })
    .catch(error => console.error('Error fetching BED file:', error));
}

