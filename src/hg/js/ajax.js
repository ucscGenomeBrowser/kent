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
    var str = "encountered ajax error";
    if(textStatus && textStatus.length) {
        str += ": '" + textStatus + "'";
    }
    str += "; please retry the action you just performed";
    showWarning(str);
    jQuery('body').css('cursor', '');
    if(this.disabledEle) {
        this.disabledEle.attr('disabled', '');
    }
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
    if(td.length > 0 && $(td[0]).width() > 200) {
        $(td[0]).css('maxWidth',$(td[0]).width() + "px");
        $(div).css('overflow','visible');
    }
    $(div).html(response);
    if(td.length > 0 && $(td[0]).width() > 200) {
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
    if(href.indexOf("?hgsid=") == -1) {
        href = href.replace(/\&hgsid=\d+/, "");
    } else {
        href = href.replace(/\?hgsid=\d+\&/, "?");
    }
    return href;
}


/////////////////////////
// Retrieve extra html from a file
var gAppendTo = null;

function _retrieveHtml(fileName,obj)
{ // popup cfg dialog
    if (obj && obj != undefined && $(obj).length == 1) {
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
  //    if obj and toggle == true, and obj has html, it will be emptied, resulting in toggle like calls
    if (toggle && obj && obj != undefined && $(obj).length == 1) {
        if ($(obj).html().length > 0) {
            $(obj).html("")
            return;
        }
    }

    waitOnFunction( _retrieveHtml, fileName, obj );  // Launches the popup but shields the ajax with a waitOnFunction
}

function retrieveHtmlHandle(response, status)
{
// Take html from hgTrackUi and put it up as a modal dialog.

    // make sure all links (e.g. help links) open up in a new window
    response = response.replace(/<a /ig, "<a target='_blank' ");

    // TODO: Shlurp up any javascript files from the response and load them with $.getScript()
    // example <script type='text/javascript' SRC='../js/tdreszer/jquery.contextmenu-1296177766.js'></script>
    var cleanHtml = response;
    var shlurpPattern=/\<script type=\'text\/javascript\' SRC\=\'.*\'\>\<\/script\>/gi;
    var jsFiles = cleanHtml.match(shlurpPattern);
    cleanHtml = cleanHtml.replace(shlurpPattern,"");
    shlurpPattern=/\<script type=\'text\/javascript\'>.*\<\/script\>/gi;
    var jsEmbeded = cleanHtml.match(shlurpPattern);
    cleanHtml = cleanHtml.replace(shlurpPattern,"");
    //<LINK rel='STYLESHEET' href='../style/ui.dropdownchecklist-1276528376.css' TYPE='text/css' />
    shlurpPattern=/\<LINK rel=\'STYLESHEET\' href\=\'.*\' TYPE=\'text\/css\' \/\>/gi;
    var cssFiles = cleanHtml.match(shlurpPattern);
    cleanHtml = cleanHtml.replace(shlurpPattern,"");

    if (gAppendTo && gAppendTo != undefined) {
        //warn($(gAppendTo).html());
        //return;
        $(gAppendTo).html(cleanHtml);
        return;
    }

    var popUp = $('#popupDialog');
    if (popUp == undefined || $(popUp).length == 0) {
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
            if (popUpClose != undefined && $(popUpClose).length == 1) {
                $(popUpClose).html("");  // clear out html after close to prevent problems caused by duplicate html elements
            }
        }
    });

    var myWidth =  $(window).width() - 300;
    if(myWidth > 900)
        myWidth = 900;
    $(popUp).dialog("option", "maxWidth", myWidth);
    $(popUp).dialog("option", "width", myWidth);
    $(popUp).dialog('open');
    var buttOk = $('button.ui-state-default');
    if($(buttOk).length == 1)
        $(buttOk).focus();
}

function scrapeVariable(html, name)
{
// scrape a variable defintion out of html (see jsHelper.c::jsPrintHash)
    var re = new RegExp("^// START " + name + "\\nvar " + name + " = ([\\S\\s]+);\\n// END " + name + "$", "m");
    var a = re.exec(html);
    var json;
    if(a && a[1]) {
        json = eval("(" + a[1] + ")");
    }
    return json;
}
