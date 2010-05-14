// AJAX Utilities

var debug = false;

var req;

function nullProcessReqChange()
{
    if(debug)
            alert("req.responseText: " + req.responseText);
}

function loadXMLDoc(url)
{
// Load XML without a request handler; this is useful if you are sending one-way messages.
        loadXMLDoc(url, null);
}

function loadXMLDoc(url, callBack)
{
// From http://developer.apple.com/internet/webcontent/xmlhttpreq.html
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
        req.send("");
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
        loadXMLDoc(loc + pairs);
    }
}

function setCartVar(name, value)
{
// Asynchronously set a cart variable.
    setCartVars( [ name ], [ value ] );
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

function catchErrorOrDispatch(obj, status)
{
// generic ajax success handler (handles fact that success is not always success).
    if(status == 'success')
        this.trueSuccess(obj, status);
    else
    {
        showWarning("ajax error: " + status);
        jQuery('body').css('cursor', '');
    }
}

function showWarning(str)
{
    $("#warningText").text(str);
    $("#warning").show();
}
