// JavaScript Especially for hui.c

var debug = false;

// The 'mat*' functions are especially designed to support subtrack configuration by 2D matrix of controls

function matSelectViewForSubTracks(obj,view)
{
// Handle any necessary changes to subtrack checkboxes when the view changes
// views are "select" drop downs on a subtrack configuration page
    if( obj.selectedIndex == 0) { // hide
        matSetCheckBoxesThatContain('id',false,true,view); 
    } else { 
        // essentially reclick all 'checked' matrix checkboxes (run onclick script)
        if (document.getElementsByTagName) {
            var list = document.getElementsByTagName('input');
            for (var ix=0;ix<list.length;ix++) {
                var ele = list[ix];
                if(ele.type.match("checkbox") == null)
                    continue;
                if(ele.name.indexOf("mat_") == 0 && ele.name.indexOf("_cb") == (ele.name.length - 3)) {
                    if(ele.checked) {
                        ele.click();
                        ele.click();
                        //matSetCheckBoxesThatContain('id',ele.checked,false,"cb_"+ele.name.substring(4,ele.name.length - 3)+view+'_cb');
                    }
                }
            }
        }
        else if (document.all) {
                if(debug)
                    alert("selectViewForSubTracks is unimplemented for this browser)");
        } else {
                // NS 4.x - I gave up trying to get this to work.
                if(debug)
                alert("selectViewForSubTracks is unimplemented for this browser");
        }
    }
}

function matSetCheckBoxesThatContain(nameOrId, state, force, sub1)
{
// Set all checkboxes which contain 1 or more given substrings in NAME or ID to state boolean
// Unlike the std setCheckBoxesThatContain() this also recognizes whether views 
// additionally control which subtracks will be checked
    var retval = false;
    if (document.getElementsByTagName)
    {
        if(debug)
            alert("matSetCheckBoxesThatContain is about to set the checkBoxes to "+state);

        var views = getViewsSelected("dd_",true); // get views that are on
        var list = document.getElementsByTagName('input');
        for (var ix=0;ix<list.length;ix++) {
            var ele = list[ix];
            if(ele.type.match("checkbox") == null)
                continue;
            var identifier = ele.name;
            if(nameOrId.search(/id/i) != -1)
                identifier = ele.id;
            var failed = false;
            for(var aIx=3;aIx<arguments.length;aIx++) {
                if(identifier.indexOf(arguments[aIx]) == -1) {
                    failed = true;
                    break;
                }
            }
            if(failed)
                continue;
            if(debug)
                alert("matSetCheckBoxesThatContain found '"+sub1+"' in '"+identifier+"'.");

            if(!state) { 
                if(ele.checked != state) {
                    ele.click();
                } else if (force) {
                    ele.click();
                    ele.click();
                }
            } else {
                if(views.length == 0) {
                    if(ele.checked != state) {
                        ele.click();
                    } else if (force) {
                        ele.click();
                        ele.click();
                    }
                } else {
                    for(var vIx=0;vIx<views.length;vIx++) {
                        if(identifier.indexOf("_"+views[vIx]+"_") >= 0) {
                            if(ele.checked != state) {
                                ele.click();
                            } else if (force) {
                                ele.click();
                                ele.click();
                            }
                            break;
                        }
                    }
                }
            }
        }
        retval = true;
    } else if (document.all) {
        if(debug)
            alert("matSetCheckBoxesThatContain is unimplemented for this browser");
    } else {
        // NS 4.x - I gave up trying to get this to work.
        if(debug)
           alert("matSetCheckBoxesThatContain is unimplemented for this browser");
    }
    return retval;
}
