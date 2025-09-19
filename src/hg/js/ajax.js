// AJAX Utilities

// Don't complain about line break before '||' etc:
/* jshint -W014 */

var debug = false;

var req;

function nullProcessReqChange()
{
    if (debug && this.readyState === 4)
            alert("req.responseText: " + req.responseText);
}

// The ajaxWaitCount code is currently NOT used, but we are keeping it in case in the future we
// decide we really need to support sequential AJAX calls (without just using async = false).
// Why? to send several async ajax calls but wait for them all to finish
// 
// When setting vars with ajax, it may be necessary to wait for response before newer calls
// Therefore this counter is set up to allow an "outside callback" when the ajax is done
// The typical scenario is setCartVar by ajax, followed immmediately by a form submit.
// To avoid the race condition, the form gets submitted after the ajax returns

var ajaxWaitCount = 0;
function ajaxWaitIsDone()     { return ( ajaxWaitCount <= 0 ); }
function ajaxWaitCountUp()    { ajaxWaitCount++; }
function ajaxWaitCountReset() { ajaxWaitCount = 0; }
//function ajaxWaitCountShow()  { warn("ajaxWait calls outstanding:"+ajaxWaitCount); }

// Here is where the "outside callback" gets set up and called
var ajaxWaitCallbackFunction = null;
var ajaxWaitCallbackTimeOut = null;
function ajaxWaitCallbackRegister(func)
{ // register a function to be called when the ajax waiting is done.
    if (ajaxWaitIsDone())
        func();
    else {
        ajaxWaitCallbackFunction = func;
        ajaxWaitCallbackTimeOut = setTimeout(ajaxWaitCallback,5000);  // just in case
    }
}

function ajaxWaitCallback()
{ // Perform the actual function (which could have been because of a callback or a timeout)
    // Clear the timeout if this is not due to ajaxWaitDone
    //warn("ajaxWaitCallback: "+ajaxWaitIsDone());
    if (ajaxWaitCallbackTimeOut) {
        clearTimeout(ajaxWaitCallbackTimeOut);
        ajaxWaitCallbackTimeOut = null;
    }
    // Clear the wait stack incase something failed and we are called by a timeout
    ajaxWaitCountReset();

    // Finally do the function
    if (ajaxWaitCallbackFunction
    && jQuery.isFunction(ajaxWaitCallbackFunction)) {
        ajaxWaitCallbackFunction();
    }
    ajaxWaitCallbackFunction = null;
}

function ajaxWaitCountDown()
{ // called whenever an ajax request is done
    if ((req && req.readyState === 4)
    || (this.readyState === 4))
    { // It is only state 4 that means done
        ajaxWaitCount--;
        if (ajaxWaitIsDone())
            ajaxWaitCallback();
    }
    //warn(req.readyState + " waiters:"+ajaxWaitCount);
}

// UNUSED but useful ?
// var formToSubmit = null; // multistate: null, {form}, "COMPLETE", "ONCEONLY"
// function formSubmit()
// { // This will be called as a callback on timeout or ajaxWaitCallback
//
//     if (formToSubmit) {
//         //warn("submitting form:"+$(formToSubmit).attr('name') + ": "+ajaxWaitIsDone());
//         var form = formToSubmit;
//         formToSubmit = "GO"; // Flag to wait no longer
//         $(form).submit();
//     }
//     waitMaskClear(); // clear any outstanding waitMask.  overkill if the form has just been submitted
// }
// function formSubmitRegister(form)
// { // Registers the form submit to be done by ajaxWaitCallback or timeout
//     if (formToSubmit) // Repeated submission got through, so ignore it
//         return false;
//     waitMaskSetup(5000);     // Will prevent repeated submissions, I hope.
//     formToSubmit = form;
//     //warn("Registering form to submit:"+$(form).attr('name'));
//     ajaxWaitCallbackRegister(formSubmit);
//     return false; // Don't submit until ajax is done.
// }
//
// function formSubmitWaiter(e)
// { // Here we will wait for up to 5 seconds before continuing.
//     if (!formToSubmit)
//         return formSubmitRegister(e.target); // register on first time through
//
//     if (formToSubmit === "GO") {  // Called again as complete
//         //warn("formSubmitWaiter(): GO");
//         formToSubmit = "STOP";  // Do this only once!
//         return true;
//     }
//     return false;
// }
//
// function formSubmitWaitOnAjax(form)
// { // Most typically, we block a form submit until all ajax has returned
//     $(form).unbind('submit', formSubmitWaiter ); // prevents multiple bind requests
//     $(form).bind(  'submit', formSubmitWaiter );
// }

function loadXMLDoc(url)
{
// Load XML without a request handler; this is useful if you are sending one-way messages.
        loadXMLDoc(url, null);
}

function loadXMLDoc(url, callBack)
{
// From http://developer.apple.com/internet/webcontent/xmlhttpreq.html
    //warn("AJAX started: "+url);
    if (!callBack)
        callBack = nullProcessReqChange;
    req = false;
    // branch for native XMLHttpRequest object
    if (window.XMLHttpRequest && !(window.ActiveXObject)) {
        try {
            req = new XMLHttpRequest();
        } catch(e) {
            req = false;
        }
    } else if (window.ActiveXObject) {
        // branch for IE/Windows ActiveX version
        try {
            req = new ActiveXObject("Msxml2.XMLHTTP");
        } catch(e) {
            try {
                req = new ActiveXObject("Microsoft.XMLHTTP");
            } catch(ev) {
                req = false;
            }
        }
    }
    if (debug)
        alert(url);
    if (req) {
        req.onreadystatechange = callBack;
        req.open("GET", url, true);
        req.send();
        //req.send("");
    }
}

function setCartVars(names, values, errFunc, async)
{
// Asynchronously sets the array of cart vars with values
    if (names.length <= 0)
        return;

    if (!errFunc)
        errFunc = errorHandler;
    if (async === null || async === undefined) // async is boolean so be explicit!
        async = true;

    // Set up constant portion of url
    var loc = window.location.href;
    if (loc.indexOf("?") > -1) {
        loc = loc.substring(0, loc.indexOf("?"));
    }
    if (loc.lastIndexOf("/") > -1) {
        loc = loc.substring(0, loc.lastIndexOf("/"));
    }
    loc = loc + "/cartDump";
    var hgsid = getHgsid();
    var data = "submit=1&noDisplay=1&hgsid=" + hgsid;
    var track = getTrack();
    if (track && track.length > 0)
        data = data + "&g=" + track;
    for(var ix=0; ix<names.length; ix++) {
        data = data + "&" + encodeURIComponent(names[ix]) + "=" + encodeURIComponent(values[ix]);
    }
    var type;
    // We prefer GETs so we can analyze logs, but use POSTs if data is longer than a
    // (conservatively small)  maximum length to avoid problems on older versions of IE.
    if ((loc.length + data.length) > 2000) {
        type = "POST";
    } else {
        type = "GET";
    }

    // on Safari and Edge, sendBeacon() has a limit of 32k, so fall back to ajax if the cart is very big
    if (!async || (typeof navigator.sendBeacon == 'undefined' || data.length > 30000)) {
        // XmlHttpRequest is used for all synchronous updates and for async updates in IE,
        // because IE doesn't support sendBeacon.  If access to bowser is blocked, the default
        // is to assume the browser is not IE.
        $.ajax({
                   type: type,
                   async: async,
                   url: loc,
                   data: data,
                   trueSuccess: function () {},
                   success: catchErrorOrDispatch,
                   error: errFunc,
                   cache: false
               });
    }
    else {
        navigator.sendBeacon(loc, data);
    }
}

function setCartVar(name, value, errFunc, async)
{
// Asynchronously set a cart variable.
    setCartVars( [ name ], [ value ], errFunc, async );
}

function setVarsFromHash(varHash, errFunc, async)
{
// Set all vars in a var hash
// If obj is undefined then obj is document!
    var names = [];
    var values = [];
    for (var aVar in varHash) {
        names.push(aVar);
        values.push(varHash[aVar]);
    }
    if (names.length > 0) {
        setCartVars(names,values, errFunc, async);
    }
}

function setAllVars(obj,subtrackName) // DEAD CODE ?
{
// Set all enabled inputs and selects found as children obj with names to cart with ajax
// If obj is undefined then obj is document!
    var names = [];
    var values = [];
    if (!obj)
        obj = $('document');

    setVarsFromHash(getAllVars(obj,subtrackName));
}

// Unused but useful
// function setCartVarFromObjId(obj)
// {
//     setCartVar($(obj).attr('id'),$(obj).val());
// }

function submitMain()  // DEAD CODE ?
{
    $('form[name="mainForm"]').submit();
}

function setCartVarAndRefresh(name,val) // DEAD CODE ?
{
    setCartVar(name,val);
    var main=$('form[name="mainForm"]');
    $(main).attr('action',window.location.href);
    setTimeout(submitMain,50);  // Delay helps ensure that cart var has gotten there first.

    return false;
}

function errorHandler(request, textStatus)
{
    var str;
    var tryAgain = true;
    if (textStatus && textStatus.length && textStatus !== "error") {
        str = "Encountered network error : '" + textStatus + "'.";
    } else {
        if (request.responseText) {
            tryAgain = false;
            str = "Encountered error: '" + request.responseText + "'";
        } else {
            str = "Encountered a network error.";
        }
    }
    if (tryAgain)
        str += " Please try again. If the problem persists, please check your network connection.";
    warn(str);
    jQuery('body').css('cursor', '');
    if (this.disabledEle) {
        this.disabledEle.removeAttr('disabled');
    }
    if (this.loadingId) {
        hideLoadingImage(this.loadingId);
    }
}

function catchErrorOrDispatch(obj, textStatus)
{
// generic ajax success handler (handles fact that success is not always success).
    if (textStatus === 'success')
        this.trueSuccess(obj, textStatus);
    else
        errorHandler.call(this, obj, textStatus);
}

// Specific calls...
function lookupMetadata(trackName,showLonglabel,showShortLabel)
{ // Ajax call to repopulate a metadata vals select when mdb var changes
    var thisData = "hgsid=" + getHgsid() + "db=" + getDb() +  "&cmd=tableMetadata&track=" + trackName;
    if (showLonglabel)
        thisData += "&showLonglabel=1";
    if (showShortLabel)
        thisData += "&showShortLabel=1";
    $.ajax({
        type: "GET",
        dataType: "html",
        url: "../cgi-bin/hgApi",
        data: thisData,
        trueSuccess: loadMetadataTable,
        success: catchErrorOrDispatch,
        cache: true,
        cmd: trackName
    });
}

function loadMetadataTable(response, status)
// Handle ajax response (repopulate a metadata val select)
{
    var div = $("div#div_"+this.cmd+"_meta");
    var td = $(div).parent('td');
    if (td.length > 0 && $(td[0]).width() > 200) {
        $(td[0]).css('maxWidth',$(td[0]).width() + "px");
        $(div).css('overflow','visible');
    }
    $(div).html(response);
    if (td.length > 0 && $(td[0]).width() > 200) {
        tr = $(td[0]).parent('tr');
        if (tr.length > 0) {
            if ($(tr).hasClass("bgLevel2"))
                $(div).children('table').addClass('bgLevel2');
            if ($(tr).hasClass("bgLevel3"))
                $(div).children('table').addClass('bgLevel3');
        }
    }
    $(div).show();
}

function removeHgsid(href)
{
// remove session id from url parameters
    if (href.indexOf("?hgsid=") === -1) {
        href = href.replace(/\&hgsid=\w+/, "");
    } else {
        href = href.replace(/\?hgsid=\w+\&/, "?");
    }
    return href;
}


/////////////////////////
// Retrieve extra html from a file
var gAppendTo = null;

function _retrieveHtml(fileName,obj)
{ // popup cfg dialog
    if (obj && $(obj).length === 1) {
        gAppendTo = obj;
    } else
        gAppendTo = null;

    $.ajax({
                type: "GET",
                url: fileName,
                dataType: "html",
                trueSuccess: retrieveHtmlHandle,
                success: catchErrorOrDispatch,
                error: errorHandler,
                cmd: "info",
                cache: true
            });
}

function retrieveHtml(fileName,obj,toggle)
{ // Retrieves html from file
  // If no obj is supplied then html will be shown in a popup.
  // if obj supplied then object's html will be replaced by what is retrieved.
  //    if obj and toggle === true, and obj has html, it will be emptied, thus in toggle like calls
    if (toggle && obj && $(obj).length === 1) {
        if ($(obj).html().length > 0) {
            $(obj).html("");
            return;
        }
    }

    // Launches the popup but shields the ajax with a waitOnFunction
    waitOnFunction( _retrieveHtml, fileName, obj );  
}

function retrieveHtmlHandle(response, status)
{
// Take html from hgTrackUi and put it up as a modal dialog.

    // make sure all links (e.g. help links) open up in a new window
    response = response.replace(/<a /ig, "<a target='_blank' ");

    // TODO: Shlurp up any javascript files from the response and load them with $.getScript()
    // example <script type='text/javascript' SRC='../js/jquery.contextmenu.js'></script>
    var cleanHtml = response;
    var shlurpPattern=/<script type=\'text\/javascript\' SRC\=\'.*\'\><\/script\>/gi;
    var jsFiles = cleanHtml.match(shlurpPattern);
    cleanHtml = cleanHtml.replace(shlurpPattern,"");
    shlurpPattern=/<script type=\'text\/javascript\'>.*<\/script\>/gi;
    var jsEmbeded = cleanHtml.match(shlurpPattern);
    cleanHtml = cleanHtml.replace(shlurpPattern,"");
    //<LINK rel='STYLESHEET' href='../style/ui.dropdownchecklist-1276528376.css' TYPE='text/css' />
    shlurpPattern=/<LINK rel=\'STYLESHEET\' href\=\'.*\' TYPE=\'text\/css\' \/\>/gi;
    var cssFiles = cleanHtml.match(shlurpPattern);
    cleanHtml = cleanHtml.replace(shlurpPattern,"");

    if (gAppendTo) {
        //warn($(gAppendTo).html());
        //return;
        $(gAppendTo).html(cleanHtml);
        return;
    }

    var popUp = $('#popupDialog');
    if (!popUp || $(popUp).length === 0) {
        $('body').prepend("<div id='popupDialog' style='display: none'></div>");
        popUp = $('#popupDialog');
    }

    $(popUp).html("<div id='pop'>" + cleanHtml + "</div>");

    $(popUp).dialog({
        ajaxOptions: { cache: true },  // This doesn't work
        resizable: true,
        height: 'auto',
        width: 'auto',
        minHeight: 200,
        minWidth: 700,
        modal: true,
        closeOnEscape: true,
        autoOpen: false,
        buttons: { "OK": function() {
            $(this).dialog("close");
        }},
        close: function() {
            var popUpClose = $('#popupDialog');
            if (popUpClose && $(popUpClose).length === 1) {
                $(popUpClose).html("");  // clear out html after close to prevent problems caused
            }                            // by duplicate html elements
        }
    });

    var myWidth =  $(window).width() - 300;
    if (myWidth > 900)
        myWidth = 900;
    $(popUp).dialog("option", "maxWidth", myWidth);
    $(popUp).dialog("option", "width", myWidth);
    $(popUp).dialog('open');
    var buttOk = $('button.ui-state-default');
    if ($(buttOk).length === 1)
        $(buttOk).focus();
}

function scrapeVariable(html, name)
{
// scrape a variable defintion out of html (see jsHelper.c::jsPrintHash)
    // NOTE: newlines are being removed by some ISPs, so regex must tolerate that:
    var re = new RegExp("// START " + name + 
                        "\\n?var " + name + " = ([\\S\\s]+);" + 
                        "\\n?// END " + name, "m");
    var a = re.exec(html);
    var json;
    if (a && a[1]) {
        json = JSON.parse(a[1]);
    }
    return json;
}

// The loadingImage module helps you manage a loading image (for a slow upload; e.g. in hgCustom).

var loadingImage = function ()
{
    // private vars
    var imgEle, msgEle, statusMsg;

    // private methods
    var refreshLoadingImg = function()
    {
        // hack to make sure animation continues in IE after form submission
        // See: http://stackoverflow.com/questions/774515/keep-an-animated-gif-going-after-form-submits
        // and http://stackoverflow.com/questions/780560/animated-gif-in-ie-stopping
        imgEle.attr('src', imgEle.attr('src'));
    };

    // public methods
    return {
        init: function(_imgEle, _msgEle, _msg)
        {
            // This should be called from the ready method; imgEle and msgEle should be jQuery objs
            imgEle = _imgEle;
            msgEle = _msgEle;
            statusMsg = _msg;
            // To make the loadingImg visible on FF, we have to make sure it's visible during
            // page load (i.e. in html) otherwise it doesn't get shown by the submitClick code.
            imgEle.hide();
        },
        run: function() {
            msgEle.append(statusMsg);
            if (navigator.userAgent.indexOf("Chrome") !== -1) {
                // In Chrome, gif animation and setTimeout's are stopped when the browser
                // receives the first blank line/comment of the next page (basically, the current
                // page is unloaded). I have found no way around this problem, so we just show a 
                // simple "Processing..." message
                // (we can't make that blink, b/c Chrome doesn't support blinking text).
                // 
                // (Surprisingly, this is NOT true for Safari, so apparently not a WebKit issue).
                imgEle.replaceWith("<span id='loadingBlinker'>&nbsp;&nbsp;<b>Processing...</b></span>");
            } else {
                imgEle.show();
                setTimeout(refreshLoadingImg, 1000);
            }
        },
        abort: function() {
            imgEle.hide();
            msgEle.hide();
            jQuery('body').css('cursor', '');
        }
    };
}();
