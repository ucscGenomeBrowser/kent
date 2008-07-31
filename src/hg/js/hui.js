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
                        clickIt(ele,true,true); // force a double click();
                        //matSetCheckBoxesThatContain('id',true,false,"cb_"+ele.name.substring(4,ele.name.length - 3)+view+'_cb');
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

function matInitializeMatrix()
{
// Called at Onload to coordinate all subtracks with the matrix of check boxes
    if (document.getElementsByTagName) {
        var list = document.getElementsByTagName('input');
        for (var ix=0;ix<list.length;ix++) {
            var ele = list[ix];
            if(ele.type.match("checkbox") != null
            && ele.name.indexOf("mat_") == 0 
            && ele.name.indexOf("_cb") == (ele.name.length - 3)) {
                clickIt(ele,ele.checked,true); // force a double click();
            }
        }
    }
    else if(debug) {
        alert("matInitializeMatrix is unimplemented for this browser)");
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

        var views = getViewsSelected("_dd_",true); // get views that are on
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
                clickIt(ele,state,force);
            } else {
                if(views.length == 0) {
                    clickIt(ele,state,force);
                } else {
                    for(var vIx=0;vIx<views.length;vIx++) {
                        if(identifier.indexOf("_"+views[vIx]+"_") >= 0) {
                            clickIt(ele,state,force);
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

function showConfigControls(name)
{
// Will show wig configuration controls
// Config controls not matching name will be hidden
    var retval = false;
    if (document.getElementsByTagName)
    {
        var list = document.getElementsByTagName('tr');
        for (var ix=0;ix<list.length;ix++) {
            var tblRow = list[ix];
            if(tblRow.id.indexOf("tr_cfg_") == 0) {  // marked as tr containing a cfg's
                if(tblRow.id.indexOf(name) == 7 && tblRow.style.display == 'none') {
                    tblRow.style.display = '';
                } else {
                    tblRow.style.display = 'none';
                }
            }
        }
        retval = true;
    }
    else if (document.all) {
        if(debug)
            alert("showConfigControls is unimplemented for this browser");
    } else {
        // NS 4.x - I gave up trying to get this to work.
        if(debug)
           alert("showConfigControls is unimplemented for this browser");
    }
    return retval;
}
