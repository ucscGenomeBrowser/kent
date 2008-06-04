// Javascript for use in hgTracks CGI

var debug = false;

function maximizePix(name, value)
{
    // Unfortunately, meaning of offsetWidth is slightly different in IE.
    document.getElementById("pix").value = document.body.offsetWidth;
}

function getURLParam(strParamName)
{
// XXXX Move to utils.js
  var strReturn = "";
  var strHref = window.location.href;
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

function toggleTrackGroupVisibility(obj, prefix)
{
// toggle visibility of a track group; prefix is the prefix of all the id's of tr's in the 
// relevant group. This code also modifies the corresponding hidden fields and the gif of the +/- img tag.
    var retval = true;
    if (document.getElementsByTagName){
        // Tested in FireFox 1.5 & 2, IE 6 & 7, and Safari
        var button = document.getElementById(prefix + "_button");
        var hidden1 = document.getElementById("hgtgroup_" + prefix + "_close_1");
        var hidden2 = document.getElementById("hgtgroup_" + prefix + "_close_2");

        // Hidding tr's works diff b/n IE and FF: http://snook.ca/archives/html_and_css/dynamically_sho/
        // This browser sniffing is very kludgy; s/d find a better way to do determine IE vs. other browsers.
        // Currently we are going to see if just '' will work in all browsers (which means we can
        // avoid this browser sniffing).
        // var canSee = (navigator.userAgent.indexOf("MSIE") > -1) ? 'block' : 'table-row';
        var canSee = '';

        var src = button.src;
        if(button && hidden1 && hidden2 && src) {
            var newSrc = src.replace("/remove", "/add");
            var newVal;
            if(newSrc == src) {
                // we're going + => -
                newVal = '0';
                hidden2.value = '0';
                newSrc = src.replace("/add", "/remove");
            } else {
                newVal = '1';
            }
            hidden1.value = newVal;
            hidden2.value = newVal;
            button.src = newSrc;
        }
        var loc = window.location.href;
        if(loc.indexOf("?") > -1) {
            loc = loc.substring(0, loc.indexOf("?"));
        }
        // XXXX currently dead code; this sends a message to hgTracks to record user choice, in case they use
        // a link to navigate.

        // loadXMLDoc(loc + "?setCart=1&" + "hgtgroup_" + prefix + "_close=" + newVal + "&hgsid=" + getURLParam("hgsid"));
        var list = document.getElementsByTagName('tr');
        for (var i=0;i<list.length;i++) {
            var ele = list[i];
            // arbitrary numbers are used to make id's unique (e.g. "map-1"), so look for prefix + "-"
            if(ele.id.indexOf(prefix + "-") == 0) {
                styleObj = ele.style;
                if(styleObj.display == 'none') {
                    styleObj.display = canSee;
                } else {
                    styleObj.display = 'none';
                }
            }
        }
        retval = false;
    } else if (document.all){
        // IE 4.X/5.0 - I don't have access to a browser to get this to work
        if(debug)
            alert("toggleTrackGroupVisibility is unimplemented in this browser");
        if(0) {
            // core of code that could perhaps work.
            // Loop looking for the appropriate style object
            styleObj = getStyle("#" + id);
            if(styleObj.display == 'none'){
                styleObj.display = '';
                obj.innerText = "hide answer";
            } else {
                styleObj.display = 'none';
                obj.innerText = "show answer";
            }
        }
    } else {
        // NS 4.x - I gave up trying to get this to work.
        if(debug)
            alert("toggleTrackGroupVisibility is unimplemented in this browser");
    }
    return retval;
}
