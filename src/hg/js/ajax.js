// AJAX Utilities

var debug = false;

var req;

function nullProcessReqChange()
{
    if(debug && this.readyState == 4)
            alert("req.responseText: " + req.responseText);
}

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
    if(ajaxWaitIsDone())
        func();
    else {
        ajaxWaitCallbackFunction = func;
        ajaxWaitCallbackTimeOut = setTimeout("ajaxWaitCallback();",5000);  // just in case
    }
}

function ajaxWaitCallback()
{ // Perform the actual function (which could have been because of a callback or a timeout)
    // Clear the timeout if this is not due to ajaxWaitDone
    //warn("ajaxWaitCallback: "+ajaxWaitIsDone());
    if(ajaxWaitCallbackTimeOut != null) {
        clearTimeout(ajaxWaitCallbackTimeOut);
        ajaxWaitCallbackTimeOut = null;
    }
    // Clear the wait stack incase something failed and we are called by a timeout
    ajaxWaitCountReset();

    // Finally do the function
    if(ajaxWaitCallbackFunction
    && jQuery.isFunction(ajaxWaitCallbackFunction)) {
        ajaxWaitCallbackFunction();
    }
    ajaxWaitCallbackFunction = null;
}

function ajaxWaitCountDown()
{ // called whenever an ajax request is done
    if((req && req.readyState == 4)
    || (this.readyState == 4))
    { // It is only state 4 that means done
        ajaxWaitCount--;
        if(ajaxWaitIsDone())
            ajaxWaitCallback();
    }
    //warn(req.readyState + " waiters:"+ajaxWaitCount);
}

var formToSubmit = null; // multistate: null, {form}, "COMPLETE", "ONCEONLY"
var formSubmitPhase = 0;
function formSubmit()
{ // This will be called as a callback on timeout or ajaxWaitCallback

    if(formToSubmit != null) {
        //warn("submitting form:"+$(formToSubmit).attr('name') + ": "+ajaxWaitIsDone());
        var form = formToSubmit;
        formToSubmit = "GO"; // Flag to wait no longer
        $(form).submit();
    }
    waitMaskClear(); // clear any outstanding waitMask.  overkill if the form has just been submitted
}
function formSubmitRegister(form)
{ // Registers the form submit to be done by ajaxWaitCallback or timeout
    if(formToSubmit != null) // Repeated submission got through, so ignore it
        return false;
    waitMaskSetup(5000);     // Will prevent repeated submissions, I hope.
    formToSubmit = form;
    //warn("Registering form to submit:"+$(form).attr('name'));
    ajaxWaitCallbackRegister(formSubmit);
    return false; // Don't submit until ajax is done.
}

function formSubmitWaiter(e)
{ // Here we will wait for up to 5 seconds before continuing.
    if(formToSubmit == null)
        return formSubmitRegister(e.target); // register on first time through

    if(formToSubmit == "GO") {  // Called again as complete
        //warn("formSubmitWaiter(): GO");
        formToSubmit = "STOP";  // Do this only once!
        return true;
    }
    return false;
}

function formSubmitWaitOnAjax(form)
{ // Most typically, we block a form submit until all ajax has returned
    $(form).unbind('submit', formSubmitWaiter ); // prevents multiple bind requests
    $(form).bind(  'submit', formSubmitWaiter );
}

function loadXMLDoc(url)
{
// Load XML without a request handler; this is useful if you are sending one-way messages.
        loadXMLDoc(url, null);
}

function loadXMLDoc(url, callBack)
{
// From http://developer.apple.com/internet/webcontent/xmlhttpreq.html
    //warn("AJAX started: "+url);
    if(callBack == null)
        callBack = nullProcessReqChange;
    req = false;
    // branch for native XMLHttpRequest object
    if(window.XMLHttpRequest && !(window.ActiveXObject)) {
        try {
            req = new XMLHttpRequest();
        } catch(e) {
            req = false;
        }
    } else if(window.ActiveXObject) {
        // branch for IE/Windows ActiveX version
        try {
            req = new ActiveXObject("Msxml2.XMLHTTP");
        } catch(e) {
            try {
                req = new ActiveXObject("Microsoft.XMLHTTP");
            } catch(e) {
                req = false;
            }
        }
    }
    if(debug)
        alert(url);
    if(req) {
        req.onreadystatechange = callBack;
        req.open("GET", url, true);
        req.send();
        //req.send("");
    }
}

function setCartVars(names, values)
{
// Asynchronously sets the array of cart vars with values
    if(names.length <= 0)
        return;

    // Set up constant portion of url
    var loc = window.location.href;
    if(loc.indexOf("?") > -1) {
        loc = loc.substring(0, loc.indexOf("?"));
    }
    if(loc.lastIndexOf("/") > -1) {
        loc = loc.substring(0, loc.lastIndexOf("/"));
    }
    loc = loc + "/cartDump";
    var hgsid = getHgsid();
    loc = loc + "?submit=1&noDisplay=1&hgsid=" + hgsid;
    var track = getTrack();
    if(track && track.length > 0)
        loc = loc + "&g=" + track;

    // Set up dynamic portion of url
    var ix=0;
    while( ix < names.length ) { // Sends multiple messages if the URL gets too long
        var pairs = "";
        for(  ;ix<names.length && pairs.length < 5000;ix++) {  // FIXME: How big is too big?
            //pairs = pairs + "&cartDump.varName=" + escape(names[ix]) + "&cartDump.newValue=" + escape(values[ix]);
            pairs = pairs + "&" + escape(names[ix]) + "=" + escape(values[ix]);
        }
        if(pairs.length == 0)
            return;
        //warn(pairs);
        ajaxWaitCountUp();
        loadXMLDoc(loc + pairs,ajaxWaitCountDown);
    }
}

function setCartVar(name, value)
{
// Asynchronously set a cart variable.
    setCartVars( [ name ], [ value ] );
}

function setVarsFromHash(varHash)
{
// Set all vars in a var hash
// If obj is undefined then obj is document!
    var names = [];
    var values = [];
    for (var aVar in varHash) {
        names.push(aVar);
        values.push(varHash[aVar]);
    }
    if(names.length > 0) {
        setCartVars(names,values);
    }
}

function setAllVars(obj,subtrackName)
{
// Set all enabled inputs and selects found as children obj with names to cart with ajax
// If obj is undefined then obj is document!
    var names = [];
    var values = [];
    if($(obj) == undefined)
        obj = $('document');

    setVarsFromHash(getAllVars(obj,subtrackName));
}

function setCartVarFromObjId(obj)
{
    setCartVar($(obj).attr('id'),$(obj).val());
}

function submitMain()
{
    $('form[name="mainForm"]').submit();
}

function setCartVarAndRefresh(name,val)
{
    setCartVar(name,val);
    var main=$('form[name="mainForm"]')
    $(main).attr('action',window.location.href);
    setTimeout("submitMain()",50);  // Delay in submit helps ensure that cart var has gotten there first.

    return false;
}

function errorHandler(request, textStatus)
{
    showWarning("ajax error: " + textStatus);
    jQuery('body').css('cursor', '');
    if(this.loadingId) {
	hideLoadingImage(this.loadingId);
    }
}

function catchErrorOrDispatch(obj, textStatus)
{
// generic ajax success handler (handles fact that success is not always success).
    if(textStatus == 'success')
        this.trueSuccess(obj, textStatus);
    else
        errorHandler.call(this, obj, textStatus);
}

function showWarning(str)
{
    $("#warningText").text(str);
    $("#warning").show();
}

// Specific calls...
function lookupMetadata(trackName,showLonglabel,showShortLabel)
{ // Ajax call to repopulate a metadata vals select when mdb var changes
    var thisData = "db=" + getDb() +  "&cmd=tableMetadata&track=" + trackName;
    if(showLonglabel)
        thisData += "&showLonglabel=1";
    if(showShortLabel)
        thisData += "&showShortLabel=1";
    $.ajax({
        type: "GET",
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
    if(td.length > 0 && $(td[0]).width() > 200) {
        $(td[0]).css('maxWidth',$(td[0]).width() + "px");
        $(div).css('overflow','visible');
    }
    $(div).html(response);
    if(td.length > 0 && $(td[0]).width() > 200) {
        tr = $(td[0]).parent('tr');
        if (tr.length > 0) {
            $(div).children('table').css('backgroundColor',$(tr[0]).css('backgroundColor'));
        }
    }
    $(div).show();
}

function removeHgsid(href)
{
// remove session id from url parameters
    if(href.indexOf("?hgsid=") == -1) {
        href = href.replace(/\&hgsid=\d+/, "");
    } else {
        href = href.replace(/\?hgsid=\d+\&/, "?");
    }
    return href;
}
