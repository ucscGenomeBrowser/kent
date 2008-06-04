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
