// Utility JavaScript

var debug = false;

function setCheckBoxesWithPrefix(obj, prefix, state)
{
// Set all checkboxes with given prefix to state boolean
   if (document.getElementsByTagName)
   {
        // Tested in FireFox 1.5 & 2, IE 6 & 7, and Safari
        var list = document.getElementsByTagName('input');
        var first = true;
        for (var i=0;i<list.length;i++, first=false) {
            var ele = list[i];
            // if(first) alert(ele.checked);
            // arbitrary numbers are used to make id's unique (e.g. "map-1"), so look for prefix
            if(ele.id.indexOf(prefix) == 0) {
                ele.checked = state;
            }
            first = false;
        }
   } else if (document.all) {
        if(debug)
            alert("setCheckBoxesWithPrefix is unimplemented for this browser");
   } else {
        // NS 4.x - I gave up trying to get this to work.
        if(debug)
           alert("setCheckBoxesWithPrefix is unimplemented for this browser");
   }
}


function toggleVisibilityWithPrefix(obj, prefix)
{
// toggle visibility of all elements with given prefix. This code is currently pretty specific too
// hgTracks, b/c it also modifies the corresponding hidden fields and the gif of the +/- img tag.

    if (document.getElementsByTagName){
        // Tested in FireFox 1.5 & 2, IE 6 & 7, and Safari
        var button = document.getElementById(prefix + "_button");
        var hidden = document.getElementById("hgtgroup_" + prefix + "_close");

        // Hidding tr's works diff b/n IE and FF: http://snook.ca/archives/html_and_css/dynamically_sho/
        // This browser sniffing is very kludgy; s/d find a better way to do determine IE vs. other browsers.
        // Currently we are going to see if just '' will work in all browsers (which means we can
        // avoid this browser sniffing).
        // var canSee = (navigator.userAgent.indexOf("MSIE") > -1) ? 'block' : 'table-row';
        var canSee = '';

        var src = button.src;
        if(button && hidden && src) {
            var newSrc = src.replace("/remove", "/add");
            if(newSrc == src) {
                // we're going + => -
                hidden.value = '0';
                newSrc = src.replace("/add", "/remove");
            } else {
                hidden.value = '1';
            }
            button.src = newSrc;
        }
        var list = document.getElementsByTagName('tr');
        for (var i=0;i<list.length;i++) {
            var ele = list[i];
            // arbitrary numbers are used to make id's unique (e.g. "map-1"), so look for prefix
            if(ele.id.indexOf(prefix) == 0) {
                styleObj = ele.style;
                if(styleObj.display == 'none') {
                    styleObj.display = canSee;
                } else {
                    styleObj.display = 'none';
                }
            }
        }
    } else if (document.all){
        // IE 4.X/5.0
        if(debug)
            alert("toggleVisibilityWithPrefix is unimplemented in this browser");

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
    } else {
        // NS 4.x - I gave up trying to get this to work.
        if(debug)
            alert("toggleVisibilityWithPrefix is unimplemented in this browser");
    }
}
