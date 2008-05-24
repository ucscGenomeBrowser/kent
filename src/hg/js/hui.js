// JavaScript Especially for hui.c

var debug = false;

// The 'mat_' functions are especially designed to support subtrack configuration by 2D matrix of controls

function mat_selectViewForSubTracks(obj,view)
{
// Handle any necessary changes to subtrack checkboxes when the view changes
// views are "select" drop downs on a subtrack configuration page
    if( obj.selectedIndex == 0) { // hide
        setCheckBoxesIdContains1(view,false); 
    } else { 
        // essentially reclick all 'checked' matrix checkboxes (run onclick script)
        if (document.getElementsByTagName) {
            var list = document.getElementsByTagName('input');
            for (var ix=0;ix<list.length;ix++) {
                var ele = list[ix];
                if(ele.type.match("checkbox") == null)
                    continue;
                if(ele.name.indexOf("mat_cb_") == 0) {
                    if(ele.checked) {
                        mat_setCheckBoxesIdContains1(ele.name.substring(4,ele.name.length),ele.checked);
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

function mat_setCheckBoxesThatContain(nameOrId, sub1, sub2, sub3, state)
{
// Set all checkboxes with 1, 2 or 3 given substrings in NAME or ID to state boolean
// Unlike the std setCheckBoxesThatContain() this also recognizes whether views 
// additionally control which subtracks will be checked
        
   if (document.getElementsByTagName)
   {
        if(debug)
            alert("mat_setCheckBoxesThatContain is about to set the checkBoxes to "+state);

        var views = getViewsSelected("dd_",true); // get views that are on
        var list = document.getElementsByTagName('input');
        for (var ix=0;ix<list.length;ix++) {
            var ele = list[ix];
            if(ele.type.match("checkbox") == null)
                continue;
            var identifier = ele.name;
            if(nameOrId.search(/id/i) != -1)
                identifier = ele.id;
            if(identifier.indexOf(sub1) == -1)
                continue;
            if(sub2 != null && sub2.length > 0 && identifier.indexOf(sub2) == -1)
                continue;
            if(sub3 != null && sub3.length > 0 && identifier.indexOf(sub3) == -1)
                continue;
            if(debug)
                alert("mat_setCheckBoxesThatContain found '"+sub1+"','"+sub2+"','"+sub3+"' in '"+identifier+"'.");

            if(ele.checked != state) {
                if(!state) { 
                    ele.click();
                } else {
                    for(var vIx=0;vIx<views.length;vIx++) {
                        if(identifier.indexOf("_"+views[vIx]) >= 0) {
                            if(ele.checked != state) { // only turn on views that are on
                                ele.click();
                            }
                            break;
                        }
                    }
                }
            }
        }
   } else if (document.all) {
        if(debug)
            alert("mat_setCheckBoxesThatContain is unimplemented for this browser");
   } else {
        // NS 4.x - I gave up trying to get this to work.
        if(debug)
           alert("mat_setCheckBoxesThatContain is unimplemented for this browser");
   }
}

function mat_setCheckBoxesIdContains1(sub1, state)
{
    mat_setCheckBoxesThatContain('id',sub1, null, null, state);
}
