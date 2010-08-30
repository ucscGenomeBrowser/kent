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

function setAllVars(obj,subtrackName)
{
// Set all enabled inputs and selects found as children obj with names to cart with ajax
// If obj is undefined then obj is document!
    var names = [];
    var values = [];
    if($(obj) == undefined)
        obj = $('document');
    var inp = $(obj).find('input');
    var sel = $(obj).find('select');
    //warn("obj:"+$(obj).attr('id') + " inputs:"+$(inp).length+ " selects:"+$(sel).length);
    $(inp).filter('[name]:enabled').each(function (i) {
        var name  = $(this).attr('name');
        var val = $(this).val();
        if(name != undefined && name != "Submit" && val != undefined) {
            names.push(name);
            values.push(val);
        }
    });
    $(sel).filter('[name]:enabled').each(function (i) {
        var name  = $(this).attr('name');
        var val = $(this).val();
        if(name != undefined && val != undefined) {
            if(subtrackName != undefined && name == subtrackName) {
                names.push(name+"_sel");  // subtrack is controld by two vars
                names.push(name);
                if(val == 'hide') {
                   values.push("0");    // Can't delete "_sel" because default takes over
                    values.push("[]");  // can delete vis because subtrack vis should be inherited.
                } else {
                    values.push("1");
                    values.push(val);
                }
            } else {
                names.push(name);
                values.push(val);
            }
        }
    });
    if(names.length > 0) {
        //warn("variables:"+names+"  values:"+values);
        setCartVars(names,values);
    }
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

// Specific calls...
function lookupMetadata(tableName,showLonglabel,showShortLabel)
{ // Ajax call to repopulate a metadata vals select when mdb var changes
    //warn("lookupMetadata for:"+tableName);
    var thisData = "db=" + getDb() +  "&cmd=tableMetadata&track=" + tableName;
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
        cmd: tableName
    });
}

function loadMetadataTable(response, status)
// Handle ajax response (repopulate a metadata val select)
{
    var div = $("div#div_"+this.cmd+"_meta");
    $(div).html(response);
    $(div).show();
}

