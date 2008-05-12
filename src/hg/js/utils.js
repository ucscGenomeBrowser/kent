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

