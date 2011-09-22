// JavaScript Especially for hui.c
// $Header: /projects/compbio/cvsroot/kent/src/hg/js/hui.js,v 1.59 2010/06/03 20:27:26 tdreszer Exp $

var compositeName = "";
//var browser;                // browser ("msie", "safari" etc.)
//var now = new Date();
//var start = now.getTime();
//$(window).load(function () {
//    if(start != null) {
//        now = new Date();
//        alert("Loading took "+(now.getTime() - start)+" msecs.");
//    }
//});


// The 'mat*' functions are especially designed to support subtrack configuration by 2D matrix of controls

function _matSelectViewForSubTracks(obj,view)
{
// viewDD:onchange Handle any necessary changes to subtrack checkboxes when the view changes
// views are "select" drop downs on a subtrack configuration page
    var classesHidden = ""; // Needed for later

    if( obj.selectedIndex == 0) { // hide
        matSubCBsEnable(false,view);
        hideConfigControls(view);

        // Needed for later
        classesHidden = matViewClasses('hidden');
        classesHidden = classesHidden.concat( matAbcCBclasses(false) );
    } else {
        // Make main display dropdown show full if currently hide
        compositeName = obj.name.substring(0,obj.name.indexOf(".")); // {trackName}.{view}.vis
        matSubCBsEnable(true,view);

        // Needed for later
        classesHidden = matViewClasses('hidden');
        classesHidden = classesHidden.concat( matAbcCBclasses(false) );

        // If hide => show then check all subCBs matching view and matCBs
        // If show => show than just enable subCBs for the view.
        if (obj.lastIndex == 0) { // From hide to show
            // If there are matCBs then we will check some subCBs if we just went from hide to show
            var matCBs = $("input.matCB:checked");
            if (matCBs.length > 0) {
                // Get list of all checked abc classes first
                var classesAbcChecked = new Array();
                matCBs.filter(".abc").each( function (i) {
                    var classList = $( this ).attr("class").split(" ");
                    classesAbcChecked.push( aryRemove(classList,"matCB","halfVis","abc") );
                });

                // Walk through checked non-ABC matCBs and sheck related subCBs
                var subCBs = $("input.subCB").filter("."+view).not(":checked");
                matCBs.not(".abc").each( function (i) {
                    var classList = $( this ).attr("class").split(" ");
                    classList = aryRemove(classList,"matCB","halfVis");
                    var subCBsMatching = objsFilterByClasses(subCBs,"and",classList);
                    if (classesAbcChecked.length>0)
                        subCBsMatching = objsFilterByClasses(subCBsMatching,"or",classesAbcChecked);
                    // Check the subCBs that belong to this view and checked matCBs
                    subCBsMatching.each( function (i) {
                        this.checked = true;
                        matSubCBsetShadow(this);
                        hideOrShowSubtrack(this);
                    });
                });
            } // If no matrix, then enabling is all that was needed.

            // fix 3-way which may need to go from unchecked to .halfVis
            var matCBs = $("input.matCB").not(".abc").not(".halfVis").not(":checked");
            if(matCBs.length > 0) {
                $( matCBs ).each( function (i) { matChkBoxNormalize( this, classesHidden ); });
            }
        }
    }
    // fix 3-way matCBs which may need to go from halfVis to checked or unchecked depending
    var matCBs = $("input.matCB").not(":checked").not(".halfVis");
    var matCBs = matCBsWhichAreComplete(false);
    if(matCBs.length > 0) {
        if($("select.viewDD").not("[selectedIndex=0]").length = 0) { // No views visible so nothing is inconsistent
            $( matCBs ).each( function (i) { matCbComplete( this, true ); });
        } else {
            $( matCBs ).each( function (i) { matChkBoxNormalize( this, classesHidden ); });
        }
    }
    matSubCBsSelected();
    obj.lastIndex = obj.selectedIndex;
}

function matSelectViewForSubTracks(obj,view)
{
    waitOnFunction( _matSelectViewForSubTracks, obj,view);
}

function exposeAll()
{
    // Make main display dropdown show full if currently hide
    var visDD = normed($("select.visDD")); // limit to hidden
    if (visDD != undefined) {
        if ($(visDD).attr('selectedIndex') == 0) {
            $(visDD).attr('selectedIndex',$(visDD).children('option').length - 1);
            $(visDD).change(); // triggers onchange code effecting inherited subtrack vis
        }
    }
}

function matSubCbClick(subCB)
{
// subCB:onclick  When a subtrack checkbox is clicked, it may result in
// Clicking/unclicking the corresponding matrix CB.  Also the
// subtrack may be hidden as a result.
    if (common.subCfg)
        scm.checkOneSubtrack(subCB,subCB.checked,!subCB.disabled);
    //////////matSubCBsetShadow(subCB);
    //////////hideOrShowSubtrack(subCB);
    // When subCBs are clicked, 3-state matCBs may need to be set
    var classes = matViewClasses('hidden');
    classes = classes.concat( matAbcCBclasses(false) );
    var matCB = matCbFindFromSubCb( subCB );
    if( matCB != undefined ) {
        matChkBoxNormalize( matCB, classes );
    }
    //var abcCB = matAbcCbFindFromSubCb( subCB );
    //if( abcCB != undefined ) {
    //    matChkBoxNormalize( abcCB, classes );
    //}

    if(subCB.checked)
        exposeAll();  // Unhide composite vis?

    matSubCBsSelected();
}

function matCbClick(matCB)
{
// matCB:onclick  When a matrix CB is clicked, the set of subtracks checked may change
// Also called indirectly by matButton:onclick via matSetMatrixCheckBoxes

    var classList = $( matCB ).attr("class").split(" ");
    var isABC = (aryFind(classList,"abc") != -1);
    classList = aryRemove(classList,"matCB","halfVis","abc");
    if(classList.length == 0 )
       matSubCBsCheck(matCB.checked);
    else if(classList.length == 1 )
       matSubCBsCheck(matCB.checked,classList[0]);               // dimX or dimY or dim ABC
    else if(classList.length == 2 )
       matSubCBsCheck(matCB.checked,classList[0],classList[1]);  // dimX and dimY
    else
        warn("ASSERT in matCbClick(): There should be no more than 2 entries in list:"+classList)

    if(!isABC)
        matCbComplete(matCB,true); // No longer partially checked

    if(isABC) {  // if dim ABC then we may have just made indeterminate X and Ys determinate
        if(matCB.checked == false) { // checking new dim ABCs cannot change indeterminate state.   IS THIS TRUE ?  So far.
            var matCBs = matCBsWhichAreComplete(false);
            if(matCBs.length > 0) {
                if($("input.matCB.abc:checked").length == 0) // No dim ABC checked, so leave X&Y checked but determined
                    $( matCBs ).each( function (i) { matCbComplete( this, true ); });
                else {
                    var classes = matViewClasses('hidden');
                    classes = classes.concat( matAbcCBclasses(false) );
                    $( matCBs ).each( function (i) { matChkBoxNormalize( this, classes ); });
                }
            }
        }
    }

    if(matCB.checked)
        exposeAll();  // Unhide composite vis?
    matSubCBsSelected();
}

function _matSetMatrixCheckBoxes(state)
{
// matButtons:onclick Set all Matrix checkboxes to state.  If additional arguments are passed in, the list of CBs will be narrowed by the classes
    //jQuery(this).css('cursor', 'wait');
    var matCBs = $("input.matCB").not(".abc");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        matCBs = $( matCBs ).filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    $( matCBs ).each( function (i) {
        this.checked = state;
        matCbComplete(this,true);
    });
    var subCbs = $("input.subCB");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        subCbs = $( subCbs ).filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    if(state) { // If clicking [+], further limit to only checked ABCs
        var classes = matAbcCBclasses(false);
        subCbs = objsFilterByClasses(subCbs,"not",classes);  // remove unchecked abcCB classes
    }
    if (common.subCfg) {
        $( subCbs ).each( function (i) {
            scm.checkOneSubtrack(this,state,!(this.disabled));
        /////////this.checked = state;
        /////////matSubCBsetShadow(this);
        /////////scm.enableCfg(this,null,this.checked);
        });
    }
    if(state)
        exposeAll();  // Unhide composite vis?
    ////////showOrHideSelectedSubtracks();
    matSubCBsSelected();
    //jQuery(this).css('cursor', '');

    var tbody = $( subCbs[0] ).parents('tbody.sorting');
    if (tbody != undefined)
         $(tbody).removeClass('sorting');
    return true;
}
function matSetMatrixCheckBoxes(state)
{
    var tbody = $( 'tbody.sortable');
    if (tbody != undefined)
         $(tbody).addClass('sorting');

    if (arguments.length >= 5)
        waitOnFunction(_matSetMatrixCheckBoxes,state,arguments[1],arguments[2],arguments[3],arguments[4]);
    else if (arguments.length >= 4)
        waitOnFunction(_matSetMatrixCheckBoxes,state,arguments[1],arguments[2],arguments[3]);
    else if (arguments.length >= 3)
        waitOnFunction(_matSetMatrixCheckBoxes,state,arguments[1],arguments[2]);
    else if (arguments.length >= 2)
        waitOnFunction(_matSetMatrixCheckBoxes,state,arguments[1]);
    else
        waitOnFunction(_matSetMatrixCheckBoxes,state);

}

///////////// CB support routines ///////////////
// Terms:
// viewDD - view drop-down control
// matButton: the [+][-] button controls associated with the matrix
// matCB - matrix dimX and dimY CB controls (in some cases this set contains abcCBs as well because they are part of the matrix)
// abcCB - matrix dim (ABC) CB controls
// subCB - subtrack CB controls
// What does work
// 1) 4 state subCBs: checked/unchecked enabled/disabled (which is visible/hidden)
// 2) 3 state matCBs for dimX and Y but not for Z (checked,unchecked,indeterminate (incomplete set of subCBs for this matCB))
// 3) cart vars for viewDD, abcCBs and subCBs but matCBs set by the state of those 3
// What is awkward or does not work
// A) Awkward: matCB could be 5 state (all,none,subset,superset,excusive non-matching set)
function matSubCBsCheck(state)
{
// Set all subtrack checkboxes to state.  If additional arguments are passed in, the list of CBs will be narrowed by the classes
// called by matCB clicks (matCbClick()) !
    var subCBs = $("input.subCB");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        subCBs = subCBs.filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }

    if(state) { // If checking subCBs, then make sure up to 3 dimensions of matCBs agree with each other on subCB verdict
        var classes = matAbcCBclasses(false);
        if (classes.length > 0)
            subCBs = objsFilterByClasses(subCBs,"not",classes);  // remove unchecked abcCB classes
        if(arguments.length == 1 || arguments.length == 3) { // Requested dimX&Y: check dim ABC state
            $( subCBs ).each( function (i) { matSubCBcheckOne(this,state); });
        } else {//if(arguments.length == 2) { // Requested dim ABC (or only 1 dimension so this code is harmless)
            var matXY = $("input.matCB").not(".abc");  // check X&Y state
            matXY = $( matXY ).filter(":checked");
            for(var mIx=0;mIx<matXY.length;mIx++) {
                var classes = $(matXY[mIx]).attr("class").split(' ');
                classes = aryRemove(classes,"matCB","halfVis");
                $( subCBs ).filter('.'+classes.join(".")).each( function (i) { matSubCBcheckOne(this,state); });
            }
        }
    } else  // state not checked so no filtering by other matCBs needed
        subCBs.each( function (i) { matSubCBcheckOne(this,state); });

    return true;
}

function matSubCBsEnable(state)
{
// Enables/Disables subtracks checkboxes.  If additional arguments are passed in, the list of CBs will be narrowed by the classes
    var subCBs = $("input.subCB");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        if(arguments[vIx].length > 0)
            subCBs = subCBs.filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    subCBs.each( function (i) {
        if(state) {
            $(this).parent().attr('title','');
            $(this).parent().attr('cursor','pointer');
        } else {
            $(this).parent().attr('title','view is hidden');
            $(this).parent().attr('cursor','pointer');
        }
        this.disabled = !state;
        matSubCBsetShadow(this);
        hideOrShowSubtrack(this);
    });

    return true;
}

function matSubCBcheckOne(subCB,state)
{
// setting a single subCB may cause it to appear/disappear
    subCB.checked = state;
    if (common.subCfg)
       scm.checkOneSubtrack(subCB,state,!subCB.disabled);
    matSubCBsetShadow(subCB);
    hideOrShowSubtrack(subCB);
}

function matSubCBsetShadow(subCB)
{
// Since CBs only get into cart when enabled/checked, the shadow control enables cart to know other states
    var shadowState = 0;
    if(subCB.checked)
        shadowState = 1;
    if(subCB.disabled)
        shadowState -= 2;
    var fourWay = normed($("input.fourWay#boolshad_-"+subCB.id));
    if (fourWay == undefined && subCB.name != undefined) {
        fourWay = normed($("input.fourWay[name='boolshad\\."+subCB.name+"']"));
        if (fourWay == undefined)
            fourWay = normed($("#"+subCB.name+"_4way"));  // FIXME: obsolete as soon as subCfg is working
    }
    if (fourWay != undefined)
        $(fourWay).val(shadowState);
    else
        warn("DEBUG: Failed to find fourWay shadow for '#"+subCB.id+"' ["+subCB.name+"]");
    if (common.subCfg)
        scm.enableCfg(subCB,null,(shadowState == 1));
}

function matChkBoxNormalize(matCB)
{
// Makes sure matCBs are in one of 3 states (checked,unchecked,indeterminate) based on matching set of subCBs
    var classList = $( matCB ).attr("class").split(" ");
    var isABC = (aryFind(classList,"abc") != -1);
    if(isABC)
        alert("ASSERT: matChkBoxNormalize() called for dim ABC!");
    classList = aryRemove(classList,"matCB","halfVis");

    var classes = '.' + classList.join(".");// created string filter of classes converting "matCB K562 H3K4me1" as ".K562.H3K4me1"
    var subCBs = $("input.subCB").filter(classes); // All subtrack CBs that match matrix CB

    if(arguments.length > 1 && arguments[1].length > 0) { // dim ABC NOT classes
        subCBs = objsFilterByClasses(subCBs,"not",arguments[1]);
    }

    // Only look at visible views
    subCBs = $(subCBs).not(":disabled");

    if(subCBs.length > 0) {
        var CBsChecked = subCBs.filter(":checked");
        if(!isABC) {
            if(CBsChecked.length == subCBs.length) {
                matCbComplete(matCB,true);
                $(matCB).attr('checked',true);
            } else if(CBsChecked.length == 0) {
                matCbComplete(matCB,true);
                $(matCB).attr('checked',false);
            } else {
                matCbComplete(matCB,false);
                $(matCB).attr('checked',true);
            }
        }
    }
    else
        matCbComplete(matCB,true); // If no subs match then this is determined !
}

function matCbComplete(matCB,complete)
{
// Makes or removes the 3rd (indeterminate) matCB state
    // Too many options:
    // 1) addClass()/removeClass() (which does not directly support title)
    // 2) wrap div which could contain border, color, content.  content is not on one line: size is difficult
    // 3) wrap font which could contain border, color, content.  content is on one line: size is difficult
    // 4) *[ ]* ?[ ]?  *[ ]*  No text seems right;  borders? colors? opacity? Yes, minimum
    if(complete) {
        $(matCB).css('opacity', '1');  // For some reason IE accepts direct change but isn't happy with simply adding class!
        $(matCB).removeClass('halfVis');
        $(matCB).attr("title","");
    } else {
        $(matCB).css('opacity', '0.5');
        $(matCB).addClass('halfVis');
        $(matCB).attr("title","Not all associated subtracks have been selected");
        //$('.halfVis').css('opacity', '0.5');
    }
}

function matCBsWhichAreComplete(complete)
{
// Returns a list of currently indeterminate matCBs.  This is encapsulated to keep consistent with matCbComplete()
    if(complete)
        return $("input.matCB").not(".halfVis");
    else
        return $("input.matCB.halfVis");
}

function matCbFindFromSubCb(subCB)
{
// returns the one matCB associated with a subCB (or undefined)
    var classList =  $( subCB ).attr("class").split(" ");
    // we need one or 2 classes, depending upon how many dimensions in matrix (e.g. "subDB GM10847 NFKB aNone IGGrab Signal")
    classList = aryRemove(classList,"subCB","changed");
    var classes = classList.slice(0,2).join('.');   // How to get only X and Y classes?  Assume they are the first 2
    var matCB = $("input.matCB."+classes); // Note, this works for filtering multiple classes because we want AND
    if(matCB.length == 1)
        return matCB;

    matCB = $("input.matCB."+classList[0]); // No hit so this must be a 1D matrix
    if(matCB.length == 1)
        return matCB;

    return undefined;
}

function matAbcCBfindFromSubCb(subCB)
{
// returns the abcCBs associated with a subCB (or undefined)
    var abcCBs = $("input.matCB.abc");
    if( abcCBs.length > 0 ) {
        var classList =  $( subCB ).attr("class").split(" ");
        classList = aryRemove(classList,"subCB","changed");
        classList.shift(); // Gets rid of X and Y associated classes (first 2 after subCB)
        classList.shift();
        classList.pop();   // gets rid of view associated class (at end)
        if(classList.length >= 1) {
            var abcCB = $(abcCBs).filter('.'+classList.join("."));
            if(abcCB.length >= 1)
                return abcCB;
        }
    }
    return undefined;
}

function objsFilterByClasses(objs,keep,classes)
{
// Accepts an obj list and an array of classes, then filters successively by that list
    if( classes != undefined && classes.length > 0 ) {
        if(keep == "and") {
            objs = $( objs ).filter( '.' + classes.join('.') ); // Must belong to all
        } else if(keep == "or") {
            objs = $( objs ).filter( '.' + classes.join(',.') ); // Must belong to one or more
        } else if(keep == "not") {
            for(var cIx=classes.length-1;cIx>-1;cIx--) {
                objs = $( objs ).not( '.' + classes[cIx] ); // not('class1.class2') is different from not('.class1').not('.class2')
            }
        }
    }
    return objs;
}

function matViewClasses(limitTo)
{
// returns an array of classes from the ViewDd: converts "viewDD normalText SIG"[]s to "SIG","zRAW"
    var classes = new Array;
    var viewDDs = $("select.viewDD");//.filter("[selectedIndex='0']");
    if(limitTo == 'hidden') {
        viewDDs = $(viewDDs).filter("[selectedIndex=0]");
    } else if(limitTo == 'visible') {
        viewDDs = $(viewDDs).not("[selectedIndex=0]");
    }
    $(viewDDs).each( function (i) {
        var classList = $( this ).attr("class").split(" ");
        classList = aryRemove(classList,"viewDD","normalText","changed");
        classes.push( classList[0] );
    });
    return classes;
}

function matAbcCBclasses(wantSelected)
{// returns an array of classes from the dim ABC CB classes: converts "matCB abc rep1"[]s to "rep1","rep2"
    var classes = new Array;
    var abcCBs = $("input.matCB.abc");
    if(abcCBs.length > 0) {
        if (!wantSelected) {
            abcCBs = abcCBs.not(":checked");
        } else {
            abcCBs = abcCBs.filter(":checked");
        }
        $(abcCBs).each( function (i) {
            var classList = $( this ).attr("class").split(" ");
            classList = aryRemove(classList,"matCB","abc");
            classes.push( classList[0] );
        });
    } else { // No abcCBs so look for filterBox classes
        return filterCompositeClasses(wantSelected);
    }
    return classes;
}

function matSubCBsSelected()
{
// Displays visible and checked track count
    var counter = $('.subCBcount');
    if(counter != undefined) {
        var subCBs =  $("input.subCB");
        $(counter).text($(subCBs).filter(":enabled:checked").length + " of " +$(subCBs).length+ " selected");
    }
}

/////////////////// subtrack configuration support ////////////////
function compositeCfgUpdateSubtrackCfgs(inp)
{
// Updates all subtrack configuration values when the composite cfg is changed
    // If view association then find it:
    var view = "";
    var daddy = normed($(inp).parents(".blueBox"));
    if(daddy != undefined) {
        var classList = $(daddy).attr("class").split(" ");
        if(classList.length == 2) {
            view = classList[1];
        }
    }
    var suffix = inp.name.substring(inp.name.indexOf("."));  // Includes '.'
    //if(suffix.length==0)
    //    suffix = inp.name.substring(inp.name.indexOf("_"));
    if(suffix.length==0) {
        warn("Unable to parse '"+inp.name+"'");
        return true;
    }
    if(inp.type.indexOf("select") == 0) {
        var list = $("select[name$='"+suffix+"']").not("[name='"+inp.name+"']"); // Exclude self from list
        if(view != "") { list =  $(list).filter(function(index) { return normed($(this).parents(".blueBox." + view)) != undefined; });}
        if($(list).length>0) {
            if(inp.multiple != true)
                $(list).attr('selectedIndex',inp.selectedIndex);
            else {
                $(list).each(function(view) {  // for all dependent (subtrack) multi-selects
                    sel = this;
                    if(view != "") {
                        var hasClass = $(this).parents(".blueBox." + view);
                        if(hasClass.length != 0)
                            return;
                    }
                    $(this).children('option').each(function() {  // for all options of dependent mult-selects
                        $(this).attr('selected',$(inp).children('option:eq('+this.index+')').attr('selected')); // set selected state to independent (parent) selected state
                    });
                    $(this).attr('size',$(inp).attr('size'));
                });
            }
        }
    }
    else if(inp.type.indexOf("checkbox") == 0) {
        var list = $("checkbox[name$='"+suffix+"']").not("[name='"+inp.name+"']"); // Exclude self from list
        if(view != "") { list =  $(list).filter(function(index) { return normed($(this).parents(".blueBox." + view)) != undefined; });}
        if($(list).length>0)
            $(list).attr("checked",$(inp).attr("checked"));
    }
    else if(inp.type.indexOf("radio") == 0) {
        var list = $("input:radio[name$='"+suffix+"']").not("[name='"+inp.name+"']");
        list = $(list).filter("[value='"+inp.value+"']")
        if(view != "") { list =  $(list).filter(function(index) { return normed($(this).parents(".blueBox." + view)) != undefined; });}
        if($(list).length>0)
            $(list).attr("checked",true);
    }
    else {  // Various types of inputs
        var list = $("input[name$='"+suffix+"']").not("[name='"+inp.name+"']");//.not("[name^='boolshad.']"); // Exclude self from list
        if(view != "") { list =  $(list).filter(function(index) { return normed($(this).parents(".blueBox." + view)) != undefined; });}
        if($(list).length>0)
            $(list).val(inp.value);
        //else {
        //    alert("Unsupported type of multi-level cfg setting type='"+inp.type+"'");
        //    return false;
        //}
    }
    return true;
}

function compositeCfgRegisterOnchangeAction(prefix)
{
// After composite level cfg settings written to HTML it is necessary to go back and
// make sure that each time they change, any matching subtrack level cfg setting are changed.
    var list = $("input[name^='"+prefix+".']").not("[name$='.vis']");
    $(list).change(function(){compositeCfgUpdateSubtrackCfgs(this);});

    var list = $("select[name^='"+prefix+".']").not("[name$='.vis']");
    $(list).change(function(){compositeCfgUpdateSubtrackCfgs(this);});
}

function visTriggersHiddenSelect(obj)
{ // SuperTrack child changing vis should trigger superTrack reshaping.
  // This is done by setting hidden input "_sel"
    var trackName_Sel = $(obj).attr('name') + "_sel";
    var theForm = $(obj).closest("form");
    var visible = (obj.selectedIndex != 0);
    if (visible) {
        updateOrMakeNamedVariable(theForm,trackName_Sel,"1");
    } else
        disableNamedVariable(theForm,trackName_Sel);
    return true;
}

function subtrackCfgHideAll(table)
{
// hide all the subtrack configuration stuff
    $("div[id $= '_cfg']").each( function (i) {
        $( this ).css('display','none');
        $( this ).children("input[name$='.childShowCfg']").val("off");
    });
    // Hide all "..." metadata displayed
    $("div[id $= '_meta']:visible").toggle();
    $("img[src$='../images/upBlue.png']").attr('src','../images/downBlue.png');
}

function subtrackCfgShow(tableName)
{
// Will show subtrack specific configuration controls
// Config controls not matching name will be hidden
    var divit = $("#div_"+tableName+"_cfg");
    if(($(divit).css('display') == 'none')
    && metadataIsVisible(tableName))
        metadataShowHide(tableName,"","");

    // Could have all inputs commented out, then uncommented when clicked:
    // But would need to:
    // 1) be able to find composite view level input
    // 2) know if subtrack input is non-default (if so then subtrack value overrides composite view level value)
    // 3) know whether so composite view level value has changed since hgTrackUi displayed (if so composite view level value overrides)
    $(divit).toggle();
    return false;
}

function enableViewCfgLink(enable,view)
{
// Enables or disables a single configuration link.
    var link = $('#a_cfg_'+view);
    if(enable)
        $(link).attr('href','#'+$(link).attr('id'));
    else
        $(link).removeAttr('href');
}

function enableAllViewCfgLinks()
{
    $( ".viewDD").each( function (i) {
        var view = this.name.substring(this.name.indexOf(".") + 1,lastIndexOf(".vis"));
        enableViewCfgLink((this.selectedIndex > 0),view);
    });
}

function hideConfigControls(view)
{
// Will hide the configuration controls associated with one name
    $("input[name$='"+view+".showCfg']").val("off");      // Set cart variable
    $("tr[id^='tr_cfg_"+view+"']").css('display','none'); // Hide controls
}

function showConfigControls(name)
{
// Will show configuration controls for name= {tableName}.{view}
// Config controls not matching name will be hidden
    var trs  = $("tr[id^='tr_cfg_']")
    $("input[name$='.showCfg']").val("off"); // Turn all off
    $( trs ).each( function (i) {
        if( this.id == 'tr_cfg_'+name && this.style.display == 'none') {
            $( this ).css('display','');
	    $("input[name$='."+name+".showCfg']").val("on");
        }
        else if( this.style.display == '') {
            $( this ).css('display','none');
        }
    });

    // Close the cfg controls in the subtracks
    $("table.subtracks").each( function (i) { subtrackCfgHideAll(this);} );
    return true;
}

function showOrHideSelectedSubtracks(inp)
{
// Show or Hide subtracks based upon radio toggle
    var showHide;
    if(arguments.length > 0)
        showHide=inp;
    else {
        var onlySelected = $("input.allOrOnly'");
        if(onlySelected.length > 0)
            showHide = onlySelected[0].checked;
        else
            return;
    }
    showSubTrackCheckBoxes(showHide);

    var tbody = $("tbody.sortable")
    $(tbody).each(function (i) {
        sortedTableAlternateColors(this);
    });
}

///// Following functions called on page load
function matInitializeMatrix()
{
// Called at Onload to coordinate all subtracks with the matrix of check boxes
//var start = startTiming();
jQuery('body').css('cursor', 'wait');
    if (document.getElementsByTagName) {
        matSubCBsSelected();
        showOrHideSelectedSubtracks();
    }
jQuery('body').css('cursor', '');
//showTiming(start,"matInitializeMatrix()");
}

function multiSelectLoad(div,sizeWhenOpen)
{
    //var div = $(obj);//.parent("div.multiSelectContainer");
    var sel = $(div).children("select:first");
    if(div != undefined && sel != undefined && sizeWhenOpen <= $(sel).length) {
        $(div).css('width', ( $(sel).clientWidth ) +"px");
        $(div).css('overflow',"hidden");
        $(div).css('borderRight',"2px inset");
    }
    $(sel).show();
}

function multiSelectCollapseToSelectedOnly(obj)
{
    var items = $(obj).children("option");
    if(items.length > 0) {
        $(items).not(":selected").hide();
    }
    $(obj).attr("size",$(items).filter(":selected").length);
}

function multiSelectBlur(obj)
{ // Closes a multiselect and colapse it to a single option when appropriate
    if($(obj).val() == undefined || $(obj).val() == "") {
        $(obj).val("All");
        $(obj).attr('selectedIndex',0);
    }
    //if(obj.value == "All") // Close if selected index is 1
    if($(obj).attr('selectedIndex') == 0) // Close if selected index is 1
        $(obj).attr('size',1);
    else
        multiSelectCollapseToSelectedOnly(obj);

    /*else if($.browser.msie == false && $(obj).children('option[selected]').length==1) {
        var ix;
        for(ix=0;ix<obj.options.length;ix++) {
            if(obj.options[ix].value == obj.value) {
                //obj.options[ix].selected = true;
                obj.selectedIndex = ix;
                obj.size=1;
                $(obj).trigger('change');
                break;
            }
        }
    }*/
}

function filterCompositeSet(obj,all)
{ // Will set all filter composites via [+] or [-] buttons

    matSubCBsCheck(all);
    var vars = [];
    var vals = [];
    var filterComp = $("select.filterComp");
    if(all) {
        $(filterComp).each(function(i) {
            $(this).trigger("checkAll");
            $(this).val("All");
            //vars.push($(this).attr('name'));  // Don't bother ajaxing this over
            //vals.push($(this).val());
        });
    } else {
        $(filterComp).each(function(i) {
            $(this).trigger("uncheckAll");
            $(this).val("");
            vars.push($(this).attr('name'));
            vals.push("[empty]");
        });
    }
    if(vars.length > 0) {
        setCartVars(vars,vals);// FIXME: setCartVar conflicts with "submit" button paradigm
    }
    matSubCBsSelected(); // Be sure to update the counts!
}

function matCbFilterClasses(matCb,joinThem)
{ // returns the var associated with a filterComp

    var classes = $( matCb ).attr("class").split(" ");
    classes = aryRemove(classes,"matCB","halfVis","abc");
    if (joinThem)
        return '.' + classes.join('.');
    return classes;
}

function filterCompFilterVar(filter)
{ // returns the var associated with a filterComp

    if($(filter).hasClass('filterComp') == false)
        return undefined;

    // Find the var for this filter
    var parts = $(filter).attr("name").split('.');
    return parts[parts.length - 1];
}

function filterCompApplyOneFilter(filter,subCbsRemaining)
{ // Applies a single filter to a filterTables TRs
    var classes = $(filter).val();
    if (classes == null || classes.length == 0)
        return []; // Nothing selected so exclude all rows

    if(classes[0] == 'All')
        return subCbsRemaining;  // nothing excluded by this filter

    var subCbsAccumulating = [];
    for(var ix =0;ix<classes.length;ix++) {
        var subCBOneFIlter = $(subCbsRemaining).filter('.' + classes[ix]);
        if (subCBOneFIlter.length > 0)
            subCbsAccumulating = jQuery.merge( subCbsAccumulating, subCBOneFIlter );
    }
    //warnSince('filterCompApplyOneFilter('+filterCompFilterVar(filter)+','+subCbsRemaining.length+')');
    return subCbsAccumulating;
}

function filterCompApplyOneMatCB(matCB,remainingCBs)
{ // Applies a single filter to a filterTables TRs
    var classes = matCbFilterClasses(matCB,true);
    if (classes == null || classes.length == 0)
        return []; // Nothing selected so exclude all rows

    //warnSince('filterCompApplyOneMatCB('+classes+','+remainingCBs.length+');
    return $(remainingCBs).filter(classes);
}

function filterCompSubCBsSurviving(filterClass)
// returns a list of trs that satisfy all filters
// If defined, will exclude filter identified by filterClass
{
    // find all filterable table rows
    var subCbsFiltered = $("input.subCB");// Default all
    if (subCbsFiltered.length == 0)
        return [];

    //warnSince('filterCompSubCBsSurviving() start: '+ subCbsFiltered.length);

    // Find all filters
    var filters = $("select.filterComp");
    if (filters.length == 0)
        return [];

    // Exclude one if requested.
    if (filterClass != undefined && filterClass.length > 0)
        filters = $(filters).not("[name$='" + filterClass + "']");

    for(var ix =0;subCbsFiltered.length > 0 && ix < filters.length;ix++) {
        subCbsFiltered = filterCompApplyOneFilter(filters[ix],subCbsFiltered);  // successively reduces
    }

    // Filter for Matrix CBs too
    if (subCbsFiltered.length > 0) {
        var matCbAll = $("input.matCB");
        var matCb = $(matCbAll).filter(":checked");
        if (matCbAll.length > matCb.length) {
            if (matCb.length == 0)
                subCbsFiltered = [];
            else {
                var subCbsAccumulating = [];
                for(var ix =0;ix<matCb.length;ix++) {
                    var subCBOneFIlter = filterCompApplyOneMatCB(matCb[ix],subCbsFiltered);
                    if (subCBOneFIlter.length > 0)
                        filteredCBs = jQuery.merge( subCbsAccumulating, subCBOneFIlter );
                }
                subCbsFiltered = subCbsAccumulating;
            }
        }
    }
    //warnSince('filterCompSubCBsSurviving() matrix: '+subCbsFiltered.length);
    return subCbsFiltered;
}

function _filterComposite()
{ // Called when filterComp selection changes.  Will check/uncheck subtracks
    var subCbsSelected = filterCompSubCBsSurviving();

    // Uncheck:
    var subCbsToUnselect = $("input.subCB:checked");// Default all
    if (subCbsToUnselect.length > 0)
        $(subCbsToUnselect).each( function (i) { matSubCBcheckOne(this,false); });
    //warnSince('done with uncheck');

    // check:
    if (subCbsSelected.length > 0)
        $(subCbsSelected).each( function (i) { matSubCBcheckOne(this,true); });
    //warnSince('done with check');

    // Update count
    matSubCBsSelected();
    //warnSince('done');

    var tbody = $( subCbsSelected[0] ).parents('tbody.sorting');
    if (tbody != undefined)
         $(tbody).removeClass('sorting');
}

function filterCompositeTrigger()
{ // Called when filterComp selection changes.  Will check/uncheck subtracks
    var tbody = $( 'tbody.sortable');
    if (tbody != undefined)
         $(tbody).addClass('sorting');

    waitOnFunction(_filterComposite);
}

function filterCompositeDone(event)
{ // Called by custom 'done' event
    event.stopImmediatePropagation();
    $(this).unbind( event );
    filterCompositeTrigger();
    //waitOnFunction(filterCompositeTrigger,$(this));
}

function filterCompositeSelectionChanged(obj)
{ // filterComposite:onchange Set subtrack selection based upon the changed filter  [Not called for filterBy]

    var subCBs = $("input.subCB");
    if ( subCBs.length > 300) {
        //if ($.browser.msie) { // IE takes tooo long, so this should be called only when leaving the filterBy box
            $(obj).one('done',filterCompositeDone);
            return;
        //}
    } else
        filterCompositeTrigger();
}

function filterCompositeExcludeOptions(multiSelect)
{ // Will mark all options in one filterComposite boxes that are inconsistent with the current
  // selections in other filterComposite boxes.  Mark is class ".excluded"
    // Compare to the list of all trs
    var allSubCBs = $("input.subCB");
    if (allSubCBs.length == 0)
        return false;

    if ($.browser.msie && $(allSubCBs).filter(":checked").length > 300) // IE takes tooo long, so this should be called only when leaving the filterBy box
        return false;

    var filterClass = filterCompFilterVar(multiSelect);
    if (filterClass == undefined)
        return false;

    // Look at list of CBs that would be selected if all were selected for this filter
    var subCbsSelected = filterCompSubCBsSurviving(filterClass);
    if (subCbsSelected.length == 0)
        return false;

    if (allSubCBs.length == subCbsSelected.length) {
        $(multiSelect).children('option.excluded').removeClass('excluded');   // remove .excluded" from all
        return true;
    }

    //warnSince('filterCompositeExcludeOptions('+filterClass+'): subCbsSelected: '+subCbsSelected.length);
    // Walk through all selected subCBs to get other related tags
    var possibleSelections = [];  // empty array
    $( subCbsSelected ).each( function (i)  {

        var possibleClasses = $( this ).attr("class").split(" ");
        if( $(possibleClasses).length > 0)
            possibleClasses = aryRemoveVals(possibleClasses,[ "subCB","changed" ]);
        if( $(possibleClasses).length > 0)
            possibleSelections = possibleSelections.concat(possibleClasses);
    });
    //warnSince('filterCompositeExcludeOptions('+filterClass+'): possibleSelections: '+possibleSelections.length);

    // Walk through all options in this filterBox to set excluded class
    var updated = false;
    if(possibleSelections.length > 0) {
        var opts = $(multiSelect).children("option");
        for(var ix = 1;ix < $(opts).length;ix++) { // All is always allowed
            if(aryFind(possibleSelections,opts[ix].value) == -1) {
                if($(opts[ix]).hasClass('excluded') == false) {
                    $(opts[ix]).addClass('excluded');
                    updated = true;
                }
            } else if($(opts[ix]).hasClass('excluded')) {
                $(opts[ix]).removeClass('excluded');
                updated = true;
            }
        }
    }
    return updated;
}

function filterCompositeClasses(wantSelected)
{// returns an array of classes from the dim ABC filterComp classes: converts "matCB abc rep1"[]s to "rep1","rep2"
    var classes = new Array;
    var abcFBs = $("select.filterComp");
    if(abcFBs.length > 0) {
        $(abcFBs).each( function(i) {
            // Need to walk through list of options
            var ix=0;
            var allAreSelected = false;
            if (this.options[ix].value == "All") {
                allAreSelected = this.options[ix].selected;
                ix++;
            }
            for(;ix<this.length;ix++) {
                if (allAreSelected || this.options[ix].selected) {
                    if (wantSelected)
                        classes.push(this.options[ix].value);
                } else {
                    if (!wantSelected)
                        classes.push(this.options[ix].value);
                }
            }
        });
    }
    return classes;
}

function multiSelectFocus(obj,sizeWhenOpen)
{ // Opens multiselect whenever it gets focus
    if($(obj).attr('size') != sizeWhenOpen) {
    $(obj).children("option").show();
    $(obj).attr('size',sizeWhenOpen);
    //warn("Selected:"+$(obj).children("option").filter(":selected").length);
    }
    //return false;
}

function multiSelectClick(obj,sizeWhenOpen)
{ // Opens multiselect whenever it is clicked
    if($(obj).attr('size') != sizeWhenOpen) {
        multiSelectFocus(obj,sizeWhenOpen);
        //$(obj).children("option").show();
        //$(obj).attr('size',sizeWhenOpen);
        return false;
    } else if($(obj).attr('selectedIndex') == 0)
        $(obj).attr('size',1);
return true;
}

function navigationLinksSetup()
{ // Navigation links let you jump to places in the document
  // If they exist, then they need to be well placed to fit window dimensions

    // Put navigation links in top corner
    var navDown = $("#navDown");
    if(navDown != undefined && navDown.length > 0) {
        navDown = navDown[0];
        var winWidth = ($(window).width() - 20) + "px"; // Room for borders
        $('.windowSize').css({maxWidth:winWidth,width:winWidth});
        var sectTtl = $("#sectTtl");
        if(sectTtl != undefined && sectTtl.length > 0) {
            $(sectTtl).css({clear: 'none'});
            $(sectTtl).prepend($(navDown));
        }
        $(navDown).css({'float':'right', 'font-weight':'normal','font-size':'medium'});
        $(navDown).show();
    }

    // Decide if top links are needed
    var navUp = $('span.navUp');
    if(navUp != undefined && navUp.length > 0) {
        $(navUp).each(function(i) {
            var offset = $(this).parent().offset();
            if(offset.top  > ($(window).height()*(2/3))) {
                $(this).show();
            }
        });
    }
}

function tableSortAtButtonPress(anchor,tagId)
{ // Special ONLY for hgTrackUi sorting.  Others use utils.js::tableSortOnButtonPress()
    var table = $( anchor ).parents("table.sortable");
    if (table) {
        subtrackCfgHideAll(table);
        waitOnFunction( _tableSortOnButtonPressEncapsulated, anchor, tagId);
    }
    return false;  // called by link so return false means don't try to go anywhere
}

var scm = { // subtrack config module.
    // This module is for subtrack level config embedded dialogs in hgTrackUi.
    // Subtrack config dialogs are embedded in the subtrack table and get populated when first
    // opened.  Composite and view level controls (parents) when updated override related
    // subtrack controls (children).  Subtrack controls, when updated overide parent controls
    // for the one subtrack.  Controls have no 'name' attribute until the user alters their value.
    // Unnamed controls are not sent to the cart!!!

    // Definitions as used here:
    // obj: an input or select style html control which may be named or unnamed
    //   parentObj: composite or view level obj which has subtrack level childObjs associated
    //   childObj: subtrack level obj that has composite and or view level parentObjs
    // cfg: subtrack level embedded dialog which can be opened or closed (hidden) and isn't
    //      populated till first opened.  Can also be a viewCfg and maybe a compositeCfg
    // populate: act of filling a subtrack cfg with controls
    // named: control with a name attribute which means it has been updated by the user.
    //        Unfortunately radio buttons are an exception, since they must keep their names.
    // unnamed: control that has not been updated by the user.  Will not be sent to cart.

    // TODO:
    //  1) SOLVED: Radio buttons work BUT they require name to keep unified set, so rely upon 'changed' class to detect changes.
    //  2) SOLVED: checkboxes: working with name = boolshad.{name}   FIXME: multishad?
    //  3) SOLVED: filterBy,filterComp working: they rely upon id, and id with '.' screwed it all up.  So replaced '.' with '_-'
    //  4) SOLVED: subCfg not matching parent: solved.
    //  5) SOLVED: OpenChromSynth: subtrack filterby needs to be updated by composite filterBy.
    //  6) SOLVED: check subtrack to enable/disable fauxVis and wrench
    //  7) SOLVED: matCB should effect subCb including enable/disable fauxVis and wrench
    //  8) SOLVED: composite/view vis should effect subVis and enable/disable fauxVis and wrench
    //  9) SOLVED: inside out: changing subtrack vis should affect subCB and matCB
    // 10) SOLVED: Loosing checked tracks!!!  Setting vis clears checkboxes?
    // 11) When parent vis makes subs hidden, shouuld they go to unchecked?
    // 12) Should user be able to click on disabled vis to check the CB?
    // 13) Make vis changes "reshape" composite!  NOTE: Do we want to do this???
    // 14) Non-configurable will need to show/change vis independently! (separate vis control but no wrench)
    // - Verify all composites work (conservation and SNPs are likely failures)
    // - Decide on a name (scm, subCfg, ? ) and then make a module file like ddcl.js.
    // - Remove debug code when ready
    // NOTE:
    // Current implementation relies upon '.' delimiter in name and no '_-' in name.  Nothing breaks rule yet...

    //mySelf: null, // There is no need for a "mySelf" unless this object is being instantiated.

    // There is one instance and these vars are page wide
    compositeId: undefined,
    visIndependent: false,
    viewIds: [],

    markChange: function (obj)
    { // Marks a control as having been changed by the user.  Naming will send value to cart.
        $(obj).addClass('changed');

        if(obj.type.indexOf("radio") == 0)   // radios must keep their names!
            return;

        var oldName = obj.id.replace(/\_\-/g,'.');   // sanitized id replaces '.' with '_-'
        $(obj).attr('name',oldName);

        // checkboxes have hidden boolshads which should be marked when unchecked
        if(obj.type.indexOf("checkbox") == 0) {
            var boolshad = normed($('input#boolshad_-' + obj.id));
            if (boolshad != undefined) {
                if(obj.checked == false) {
                    var oldName = boolshad.id.replace(/\_\-/g,'.');   // sanitized id replaces '.' with '_-'
                    $(obj).addClass('changed');
                    $(boolshad).attr('name',oldName);
                }
                else
                    scm.clearChange(boolshad);
            }
        }
    },

    clearChange: function (obj)
    { // Mark as unchanged
        $(obj).removeClass('changed');

        if(obj.type.indexOf("radio") == 0)    // radios must keep their names!
            return;

        $(obj).removeAttr('name');

        // checkboxes have hidden boolshads which should be cleared in tandem
        if(obj.type.indexOf("checkbox") == 0) {
            var boolshad = normed($('input#boolshad_-' + obj.id));
            if (boolshad != undefined) {
                $(boolshad).removeClass('changed');
                $(boolshad).removeAttr('name');
            }
        }
    },

    hasChanged: function (obj)
    { // Is this object updated (value changed by a user)?
        return $(obj).hasClass('changed');
    },

    unnameIt: function (obj,setId)
    { // removes a control's name so that it will not be updated to the cart upon submit
      // If setId and obj has a name, will set id to current name
        var myName = $(obj).attr('name');
        if (myName && myName.length > 0) {

            // checkboxes have hidden boolshads which should be unamed in tandem
            if(obj.type.indexOf("checkbox") == 0) {
                var boolshad = normed( $("input[name='boolshad\\."+myName+"']") );  // Not yet sanitized name
                if (boolshad != undefined) {
                    scm.unnameIt(boolshad,setId);  // self referencing but boolshad not(':checkbox')
                }
            }

            if (setId) {
                // DEBUG -------------
                if (myName.indexOf('_-') >= 0)
                    warn("DEBUG: Found control with '_-' in name: "+myName + "<BR>This violates the naming rules and will break javascript code.");
                // DEBUG -------------
                var newId = myName.replace(/\./g,'_-');   // sanitized id replaces '.' with '_-'

                // DEBUG -------------
                if (obj.id && obj.id.length > 0 && obj.id != myName)
                    warn("DEBUG: Obj is being re-ID'd but it already has an ID.  Old:'"+obj.id+"'  New:'"+newId+"'");
                // DEBUG -------------
                $(obj).attr('id',newId);
            }
        }

        scm.clearChange(obj);  // actually removes the name!
    },

    compositeCfgFind: function ()
    { // returns the cfg container for the composite controls
        // TODO: write, create a composite cfg div!
    },

    compositeObjFind: function (suffix)
    { // returns the composite level control for this suffix
        //var compCfg = scm.compositeCfgFind();  // TODO: when there is a composite cfg div!
        var compObj;
        if (suffix != undefined) {
            //compObj = $('#'+ scm.compositeId + '\\.' + suffix);
            compObj = normed($('#'+ scm.compositeId + '_-' + suffix));
            if (compObj == undefined) {
                compObj = normed($('#'+ scm.compositeId + '_' + suffix));
            }
        } else {
            compObj = normed($("#"+scm.compositeId));
        }
        return compObj;
    },

    viewIdFind: function (childObj)
    { // returns the name of the view that is a parent of this subtrack control
        var cfg = normed($(childObj).parents('div.subCfg'));
        if (cfg == undefined) {
            // could be vis outside of cfg div
            var subCb = normed($(childObj).closest('tr').find('input.subCB'));
            if (subCb != undefined) {
                var classList = $(subCb).attr("class").split(" ");
                classList = aryRemove(classList,"changed");
                return classList[classList.length - 1];
            }
            warn("Can't find containing div.subCfg of child '"+$(childObj).attr('id')+"'.");
            return undefined;
        }
        var classList = $( cfg ).attr("class").split(" ");
        classList = aryRemove(classList,"subCfg","filled");
        if (classList.length == 0) {
            warn("Subtrack cfg div does not have view class for child '"+$(childObj).attr('id')+"'.");
            return undefined;
        } else if (classList.length > 1) {
            warn("Subtrack cfg div for '"+$(childObj).attr('id')+"' has unexpected class: "+classList);
            return undefined;
        }
        if (classList[0] == 'noView') // valid case
            return undefined;
        return classList[0];
    },

    viewCfgFind: function (viewId)
    { // returns the cfg container for a given view
        var viewCfg = normed($('tr#tr_cfg_'+viewId));
        if (viewCfg == undefined) {
            warn('Could not find viewCfg for '+viewId);
        }
        return viewCfg;
    },

    viewObjFind: function (viewId,suffix)
    { // returns the control belonging to this view and suffix
        var viewObj;
        if (suffix != undefined) {
            var viewCfg = scm.viewCfgFind(viewId);
            viewObj = normed($(viewCfg).find("[id$='_-"+suffix+"']"));
            if (viewObj == undefined)
                viewObj = normed($(viewCfg).find("[id$='_"+suffix+"']"));
        } else
            viewObj = normed($("#"+scm.compositeId+"_-"+viewId+"_-vis"));

        return viewObj;
    },

    childObjsFind: function (viewId,suffix)
    { // returns an array of objs for this view and suffix
      // Assumes composite wide if viewId is not provided
      // Assumes vis if suffix is not provided

        if (viewId != undefined) {
            var childCfgs = $('div.subCfg.filled.'+viewId);
        } else {
            var childCfgs = $('div.subCfg.filled');
        }
        if (childCfgs == undefined)
            return [];

        var childObjs = [];
        if (suffix != undefined)
            childObjs = $(childCfgs).find('select,input').filter("[id$='_-"+suffix+"']");
        else
            childObjs = $(childCfgs).find('select.visDD');

        if (childObjs == undefined)
            return [];

        return childObjs;
    },

    objSuffixGet: function (obj)
    { // returns the identifying suffix of a control.  obj can be parent or child
        //var nameParts = $(obj).attr('id').split('.');
        var nameParts = $(obj).attr('id').split('_-');
        if (nameParts == undefined || nameParts.length == 0) {
            warn("Can't resolve id for '"+$(obj).attr('id')+"'.");
            return undefined;
        }
        if (nameParts.length < 2)
            return undefined;

        nameParts.shift();
        if (scm.viewIds.length > 0 && nameParts.length > 1) // FIXME: I expect more problems with this!
            nameParts.shift();

        return nameParts.join('_-');
    },

    childCfgFind: function (childObj)
    { // returns the cfg wrapper for a child vis object
        var childCfg = normed($(childObj).parents('div.subCfg.filled'));
        if (childCfg == undefined)
            warn("Can't find childCfg for "+childObj.id);
        return childCfg;
    },

    subCbFind: function (childObj)
    { // returns the subCB for a child vis object
        // Look directly first
        var subCb = normed($(childObj).closest('tr').find('input.subCB'));
        if (subCb != undefined)
            return subCb;

        warn("DEBUG: Failed to find subCb for "+childObj.id);
        var childCfg = scm.childCfgFind(childObj);
        if (childCfg == undefined)
            return undefined;

        var tr = $(childCfg).parents('tr').first();
        var subtrack = childCfg.id.substring(8); // 'div_cfg_'.length
        var subCb = normed($(tr).find("input[name='"+subtrack+"_sel']"));
        if (subCb == undefined)
            warn("Can't find subCB for subtrack: "+subtrack);
        return subCb;
    },

    visChanged: function (childObj)
    { // called on change for a child vis control
        var subCb = scm.subCbFind(childObj);
        if (subCb != undefined) {
            if (childObj.selectedIndex > 0)
                subCb.checked = true;
                matSubCbClick(subCb);
        }
        $(childObj).attr('name',childObj.id);
    },

    parentsFind: function (childObj)
    { // returns array of composite and/or view level parent controls
        var myParents = [];
        var suffix = scm.objSuffixGet(childObj);
        var notRadio = (childObj.type.indexOf("radio") != 0);

        // find view name
        var viewId = scm.viewIdFind(childObj);
        if (viewId != undefined) {
            var viewObj = scm.viewObjFind(viewId,suffix);
            if (viewObj != undefined) {
                if (notRadio)
                    myParents[0] = viewObj;
                else {
                    $(viewObj).each(function (i) { // radios will have more than one
                        if(childObj.value == this.value) {
                            myParents[0] = this;
                            return false; // breaks each, but not parentsFind
                        }
                    });
                }
            }
        }

        var compObj = scm.compositeObjFind(suffix);
        if (compObj != undefined) {
            if (notRadio)
                myParents[myParents.length] = compObj;
            else {
                $(compObj).each(function (i) { // radios will have more than one
                    if(childObj.value == this.value) {
                        myParents[myParents.length] = this;
                        return false;
                    }
                });
            }
        }
        return myParents;
    },

    childrenFind: function (parentObj)
    { // returns array of all currently populated child controls related to this parent control
      // parentObj could be composite level or view level
      // parent object could be for vis which has special rules

        var isComposite = false;
        var isVis = false;
        var suffix = scm.objSuffixGet(parentObj);
        isVis = (suffix != undefined && suffix == 'vis');  // vis inside of subCfg

        var viewId = undefined;
        if (isVis) { // This is a view control

            isComposite = (suffix == undefined);
            suffix = undefined;
            if (!isComposite) {
                var classList = $( parentObj ).attr("class").split(" ");
                classList = aryRemove(classList,"viewDD","normalText","changed");
                if (classList.length != 1) {
                    warn("Unexpected view vis class list:"+classList);
                    return [];
                }
                viewId = classList[0];
            }
        } else { // normal obj

            var viewCfg = normed($(parentObj).parents("tr[id^='tr_cfg_']"));
            isComposite = (viewCfg == undefined); // is composite
            if (!isComposite) {
                viewId = viewCfg.id.substring(7); // 'tr_cfg_'.length
            }
        }

        if (isComposite) {

            // There may be views
            if (scm.viewIds.length > 0) {
                var allChildren = [];
                for (var ix = 0;ix < scm.viewIds.length;ix++) {
                    viewId = scm.viewIds[ix];
                    // Get any view objs first
                    var viewObj = scm.viewObjFind(viewId,suffix);
                    if (viewObj != undefined)
                        allChildren[allChildren.length] = viewObj;
                    // Now get children
                    var viewChildren = scm.childObjsFind(viewId,suffix);
                    if (viewChildren.length > 0) {
                        if (allChildren.length == 0)
                            allChildren = viewChildren;
                        else
                            allChildren = jQuery.merge( allChildren, viewChildren );
                    }
                }
                return allChildren;
            } else { // if no views then just get them all
                return scm.childObjsFind(undefined,suffix);
            }

        } else {
            return scm.childObjsFind(viewId,suffix);
        }
    },

    visChildrenFind: function (parentObj)
    { // returns array of all currently faux and populated vis child controls (which are not in subCfg div)
      // parentObj could be composite level or view level

        var isVis = false;
        var suffix = scm.objSuffixGet(parentObj);
        isVis = (suffix == undefined || suffix == 'vis');
        var isComposite = (suffix == undefined);

        var subVis = $('.subVisDD');  // select:vis or faux:div
        if (isComposite) {

            return subVis;

        } else {
            var classList = $( parentObj ).attr("class").split(" ");
            classList = aryRemove(classList,"viewDD","normalText","changed");
            if (classList.length != 1) {
                warn("Unexpected view vis class list:"+classList);
                return [];
            }
            return $(subVis).filter('.' + classList[0]);
        }
    },

    checkOneSubtrack: function (subCb,check,enable)
    { // Handle a single check of a single subCb
        subCb.checked = check;
        subCb.dsabled = enable;
        scm.markChange(subCb);
        matSubCBsetShadow(subCb);
        hideOrShowSubtrack(subCb);
        scm.enableCfg(subCb,undefined,check);
    },

    propagateSetting: function (parentObj)
    { // propagate composite/view level setting to subtrack children
        var children = scm.childrenFind(parentObj);
        if(parentObj.type.indexOf("checkbox") == 0) {
            var parentChecked = parentObj.checked;
            $(children).each(function (i) {
                // Note checkbox and boolshad are children.
                if (this.type != 'hidden')
                    this.checked = parentObj.checked;
                scm.clearChange(this);
            });
        } else if(parentObj.type.indexOf("radio") == 0) {
            var parentVal = $(parentObj).val();
            $(children).each(function (i) {
                this.checked = ($(this).val() == parentVal);
                scm.clearChange(this);
            });
        } else {// selects and inputs are easy
            var parentVal = $(parentObj).val();
            var updateDdcl = ($(parentObj).hasClass('filterBy') || $(parentObj).hasClass('filterComp'));
            $(children).each(function (i) {
                $(this).val(parentVal);
                scm.clearChange(this);
                if (updateDdcl)
                    ddcl.onComplete(this);
            });
        }
        scm.markChange(parentObj);
    },

    propagateViewVis: function (viewObj,compositeVis)
    { // propagate vis from a view limiting with compositeVis
        var limitedVis = Math.min(compositeVis,viewObj.selectedIndex);
        var visText = 'hide';
        if (limitedVis == 1)
            visText = 'dense';
        else if (limitedVis == 2)
            visText = 'squish';
        else if (limitedVis == 3)
            visText = 'pack';
        else if (limitedVis == 4)
            visText = 'full';

        var children = scm.visChildrenFind(viewObj);
        $(children).each(function (i) {
            if ($(this).hasClass('fauxInput')) {
                $(this).text(visText);
            } else {
                $(this).attr('selectedIndex',limitedVis);
                scm.clearChange(this);
            }
        });
    },

    propagateVis: function (parentObj,viewId)
    { // propagate vis settings to subtrack children
        if (viewId == undefined) {
            // Walk through views and set with this
            var parentVis = parentObj.selectedIndex;
            if (scm.viewIds.length > 0) {
                for (var ix=0;ix<scm.viewIds.length;ix++) {
                    var viewObj = scm.viewObjFind(scm.viewIds[ix]);//,undefined);
                    if (viewObj != undefined)
                        scm.propagateViewVis(viewObj,parentVis);
                }
            } else { // No view so, simple

                var visText = 'hide';
                if (parentVis == 1)
                    visText = 'dense';
                else if (parentVis == 2)
                    visText = 'squish';
                else if (parentVis == 3)
                    visText = 'pack';
                else if (parentVis == 4)
                    visText = 'full';
                var children = scm.visChildrenFind(parentObj);
                $(children).each(function (i) {
                    if ($(this).hasClass('fauxInput')) {
                        $(this).text(visText);
                    } else {
                        $(this).attr('selectedIndex',parentVis);
                        scm.clearChange(this);
                    }
                });
            }
        } else {
            // First get composite vis to limit with
            var compObj = scm.compositeObjFind(undefined);
            if (compObj == undefined) {
                warn('Could not find composite vis object!');
                return false;
            }
            scm.propagateViewVis(parentObj,compObj.selectedIndex);
        }
    },

    inheritSetting: function (childObj,force)
    { // update value if parents values override child values.
        var myParents = scm.parentsFind(childObj);
        if (myParents == undefined || myParents.length < 1) {
            // DEBUG -------------  It is probably okay to subCfg without parent cfg, but we need to be sure
            warn('DEBUG No parents were found for childObj: '+childObj.id);
            // DEBUG -------------
            return true;
        }
        var isVis = (undefined == scm.objSuffixGet(childObj));
        if (isVis) {
            var subCb = scm.subCbFind(childObj);
            if (subCb != undefined) {
                var limitedVis = 9;
                if (myParents.length == 1 && (force || scm.hasChanged(myParents[0])))
                    limitedVis = myParents[0].selectedIndex;
                else if (myParents.length == 2) {
                    if (force || scm.hasChanged(myParents[0]) || scm.hasChanged(myParents[1]))
                        limitedVis = Math.min(myParents[0].selectedIndex,myParents[1].selectedIndex);
                }
                if (limitedVis < 9)
                    $(childObj).attr('selectedIndex',limitedVis);
           }
        } else {
            var count = 0;
            if(childObj.type.indexOf("checkbox") == 0) {
                $(myParents).each(function (i) {
                    if (force || scm.hasChanged(this)) {
                        childObj.checked = this.checked;
                        // can ignore boolshad because this does not change
                        count++;
                    }
                });
            } else if(childObj.type.indexOf("radio") == 0) {
                $(myParents).each(function (i) {
                    if (force || scm.hasChanged(this)) {
                        childObj.checked = this.checked;
                        // parentObj is already "tuned" to the correct radio, so this works
                        count++;
                    }
                });
            } else {// selects and inputs are easy
                $(myParents).each(function (i) {
                    if (force || scm.hasChanged(this)) {
                        $(childObj).val($(this).val());
                        count++;
                    }
                });
            }
            if (count > 1) // if hasChanged() is working, there should never be more than one
                warn('Both composite and view are seen as updated!  Named update is not working.');
        }
    },

    currentCfg: undefined, // keep track of cfg while ajaxing, man
    currentSub: undefined, // keep track of subtrack while ajaxing, dude

    cfgFill: function (content, status)
    { // Finishes the population of a subtrack cfg.  Called by ajax return.
        var cfg = scm.currentCfg;
        scm.currentCfg = undefined;
        var cleanHtml = content;
        var shlurpPattern=/\<script type=\'text\/javascript\' SRC\=\'.*\'\>\<\/script\>/gi;
        // DEBUG -------------
            var jsFiles = cleanHtml.match(shlurpPattern);
            if (jsFiles && jsFiles.length > 0)
                alert("jsFiles:'"+jsFiles+"'\n---------------\n"+cleanHtml); // warn() interprets html, etc.
        // DEBUG -------------
        cleanHtml = cleanHtml.replace(shlurpPattern,"");
        shlurpPattern=/\<script type=\'text\/javascript\'>.*\<\/script\>/gi;
        // DEBUG -------------
            var jsEmbeded = cleanHtml.match(shlurpPattern);
            if (jsEmbeded && jsEmbeded.length > 0)
                alert("jsEmbeded:'"+jsEmbeded+"'\n---------------\n"+cleanHtml);
        // DEBUG -------------
        cleanHtml = cleanHtml.replace(shlurpPattern,"");
        shlurpPattern=/\<LINK rel=\'STYLESHEET\' href\=\'.*\' TYPE=\'text\/css\' \/\>/gi;
        // DEBUG -------------
            var cssFiles = cleanHtml.match(shlurpPattern);
            if (cssFiles && cssFiles.length > 0)
                alert("cssFiles:'"+cssFiles+"'\n---------------\n"+cleanHtml);
        // DEBUG -------------
        cleanHtml = cleanHtml.replace(shlurpPattern,"");
        if (scm.visIndependent) {
            ix = cleanHtml.indexOf('</SELECT>');
            if (ix > 0)
                cleanHtml = cleanHtml.substring(ix+'</SELECT>'.length);
            while(cleanHtml.length > 0) {
                ix = cleanHtml.search("<");
                cleanHtml = cleanHtml.substring(ix);
                ix = cleanHtml.search(/\<BR\>/i);
                if (ix != 0)
                    break; // Not found or not at start.
                else
                    cleanHtml = cleanHtml.substring(4); // skip past <BR> and continue
            }
        } else {
            var ix = cleanHtml.indexOf('<B>Display&nbsp;mode:&nbsp;</B>');
            if (ix > 0)
                cleanHtml = cleanHtml.substring(ix+'<B>Display&nbsp;mode:&nbsp;</B>'.length);
        }
            //cleanHtml = cleanHtml.substring(ix);
        ix = cleanHtml.indexOf('</FORM>'); // start of form already chipped off
        if (ix > 0)
            cleanHtml = cleanHtml.substring(0,ix - 1);

        //cleanHtml = "<div class='blueBox' style='background-color:#FFF9D2; padding:0.2em 1em 1em; float:left;'><CENTER><B>Subtrack Configuration</B></CENTER><BR>" + cleanHtml + "</div>"
        //cleanHtml = "<div class='blueBox' style='background-color:#FFF9D2; padding:0.2em 1em 1em; float:left;'><B>Subtrack visibility:</B>&nbsp;" + cleanHtml + "</div>"
        cleanHtml = "<div class='blueBox' style='background-color:#FFF9D2; padding:0.5em 1em 1em;'>" + cleanHtml + "</div>"
        $(cfg).html(cleanHtml);
        $(cfg).addClass('filled');
        var boxWithin = $(cfg).find('.blueBox');
        if (boxWithin.length > 1)
            $(boxWithin[1]).removeClass('blueBox');

        //$(cfg).html("<div style='font-size:.9em;'>" + cleanHtml + "</div>");
        var subObjs = $(cfg).find('input,select').filter("[name]");
        if (subObjs.length == 0) {
            warn('Did not find controls for cfg: ' + cfg.id);
            return;
        }
        $(subObjs).each(function (i) {
            if (this.name != undefined) { // The filter("[name]") above didn't do it!
                if (this.type != 'hidden') {
                    scm.unnameIt(this,true);
                    scm.inheritSetting(this,false); // updates any values that have been changed on this page
                    // if view vis do more than just name it on change
                    var suffix = scm.objSuffixGet(this);
                    if (suffix == undefined) // vis
                        $(this).bind('change',function (e) {
                            scm.visChanged(this);
                        });
                    else {
                        $(this).bind('change',function (e) {
                            scm.markChange(this);
                        });
                    }
                }
            }
        });
        // finally show
        $(cfg).show();
        // Tricks to get this in the size and position I want
        $(cfg).css({ position: 'absolute'});
        var myWidth = $(cfg).width();
        var shiftLeft = -1;
        if (scm.visIndependent) {
            shiftLeft *= ($(cfg).position().left - 125);
            var subVis = normed($('div#' + scm.currentSub+'_faux'));
            if (subVis != undefined)
                scm.replaceWithVis(subVis,scm.currentSub,false);
        }
        else
            shiftLeft *= ($(cfg).position().left - 40);
        $(cfg).css({ width: myWidth+'px',position: 'relative', left: shiftLeft + 'px' });

        // Setting up filterBys must follow show because sizing requires visibility
        $(cfg).find('.filterBy,.filterComp').each( function(i) {
            if ($(this).hasClass('filterComp'))
                ddcl.setup(this);
            else
                ddcl.setup(this, 'noneIsAll');
        });
    },

    cfgPopulate: function (cfg,subtrack)
    { // Populates a subtrack cfg dialog via ajax and update from composite/view parents
        scm.currentCfg = cfg;
        scm.currentSub = subtrack;

        $.ajax({
            type: "GET",
            url: "../cgi-bin/hgTrackUi?ajax=1&g=" + subtrack + "&hgsid=" + getHgsid() + "&db=" + getDb(),
            dataType: "html",
            trueSuccess: scm.cfgFill,
            success: catchErrorOrDispatch,
            error: errorHandler,
            cmd: "cfg",
            cache: false
        });
    },

    replaceWithVis: function (obj,subtrack,open)
    { // Replaces the current fauxVis object with a true visibility selector

        if ($(obj).hasClass('disabled'))
            return;
        var classList = $( obj ).attr("class").split(" ");
        var view = classList[classList.length - 1];
        var classList = $(obj).attr('class').split(' '); // This relies on view being the last class!!!
        var selectHtml = "<SELECT id='"+subtrack+"' class='normalText subVisDD "+view+"' style='width:70px;'";
        selectHtml += ">";
        var selected = $(obj).text();
        var visibilities = ['hide','dense','squish','pack','full'];
        $(visibilities).each( function (ix) {
             selectHtml += "<OPTION" + (visibilities[ix] == selected ? " SELECTED":"") + ">"+visibilities[ix]+"</OPTION>";
        });
        selectHtml += "</SELECT>";
        $(obj).replaceWith(selectHtml);
        if (open) {
            var newObj = $('select#'+subtrack);
            $(newObj).css({'zIndex':'2','vertical-align':'top'});
            $(newObj).attr('size',5);
            $(newObj).one('blur',function (e) {
                $(this).attr('size',1);
                $(this).unbind('blur');
            });
            $(newObj).change(function (e) {
                if ($(this).attr('size') > 1)
                    $(this).attr('size',1);
                if (this.selectedIndex == 0) { // setting to hide so uncheck and disable.
                    // Easiest is to uncheck subCB and reset vis
                    //    so that the reverse action makes sense
                    var subCb = normed($('input#' + this.id + '_sel'));
                    if (subCb != undefined) {
                        scm.checkOneSubtrack(subCb,false,true);
                        matSubCbClick(subCb);
                        scm.inheritSetting(this,true);
                        scm.unnameIt(this);
                    // DEBUG -------------
                    } else {
                        warn('DEBUG: Cant find subCB for ' + this.id);
                    // DEBUG -------------
                    }
                } else
                    scm.markChange(this);
            });
            $(newObj).focus();
        }
    },

    enableCfg: function (subCb,subtrack,setTo)
    { // Enables or disables subVis and wrench
        if (subCb == undefined) {
            if (subtrack == undefined || subtrack.length == 0) {
                warn("DEBUG: scm.enableCfg() called without CB or subtrack.");
                return false;
            }
            subCb = normed($("input[name='"+subtrack+"'_sel]"));
            if (subCb == undefined) {
                warn("DEBUG: scm.enableCfg() could not find CB for subtrack: "+subtrack);
                return false;
            }
        }

        var td = normed($(subCb).parent('td'));
        if (td == undefined) {
            warn("DEBUG: scm.enableCfg() could not find TD for CB: "+subCb.name);
            return false;
        }
        var subFaux = normed($(td).find('div.subVisDD'));
        if (subFaux != undefined) {
            if (setTo == true)
                $(subFaux).removeClass('disabled');
            else
                $(subFaux).addClass('disabled');
        } else {
            var subVis = normed($(td).find('select.subVisDD'));
            if (subVis != undefined) {
                $(subVis).attr('disabled',!setTo);
            }
        }
        var wrench = normed($(td).find('span.clickable')); // TODO tighten this
        if (wrench != undefined) {
            if (setTo == true)
                $(wrench).removeClass('halfVis');
            else
                $(wrench).addClass('halfVis');
        }
    },

    cfgToggle: function (wrench,subtrack)
    { // Opens/closes subtrack cfg dialog, populating if empty
        var cfg = normed($("div#div_cfg_"+subtrack));
        if (cfg == undefined) {
            warn("DEBUG: Can't find div_cfg_"+subtrack);
            return false;
        }

        if ($(cfg).css('display') == 'none') {
            if ($(wrench).hasClass('halfVis'))
                return;
            // Don't allow if this composite is not enabled!
            // find the cb
            var tr = $(cfg).parents('tr').first();
            var subCb = normed($(tr).find("#"+subtrack+"_sel"));
            if (subCb == undefined) {
                warn("Can't find subCB for "+subtrack);
                return false;
            }
            if (subCb.disabled == true) // || subCb.checked == false)
                return false;

            if(metadataIsVisible(subtrack))
                metadataShowHide(subtrack,"","");
            if ($(cfg).hasClass('filled'))
                $(cfg).show();
            else
                waitOnFunction( scm.cfgPopulate, cfg, subtrack );
        } else
            $(cfg).hide();
        return false; // called by link!
    },

    subCbInit: function (subCb)
    { // unnames the subCb control and sets up on change envent
        scm.unnameIt(subCb,true);

        var boolshad = normed( $("input[name='boolshad\\."+subCb.id+"']") );  // Not yet sanitized name
        if (boolshad != undefined) {
            scm.unnameIt(boolshad,true);
        }
        $(subCb).bind('click',function (e) {
            scm.markChange(subCb);
        });
    },

    viewInit: function (viewId)
    { // unnames all view controls
        // iterate through all matching controls and unname
        var tr = normed($('tr#tr_cfg_'+viewId));
        if (tr == undefined) {
            warn('DEBUG: Did not find view: ' + viewId);
            return;
        }
        var viewAllObjs = $(tr).find('input,select')
        var viewObjs = $(viewAllObjs).not("input[type='hidden']");
        if (viewObjs.length > 0) {
            $(viewObjs).each(function (i) {
                scm.unnameIt(this,true);
                $(this).bind('change',function (e) {
                    scm.propagateSetting(this);
                });
            });
        }
        // Special for checkboxes: boolshad
        var boolObjs = $(viewAllObjs).filter("input[type='hidden']").filter("[name^='boolshad\\.']");
        $(boolObjs).each(function (i) {
            scm.unnameIt(this,true);
        });

        // Now vis control
        var visObj = normed($("select[name='"+scm.compositeId+"\\."+viewId+"\\.vis']"));
        if (visObj == undefined) {
            warn('DEBUG: Did not find visibility control for view: ' + viewId);
            return;
        }
        scm.unnameIt(visObj,true);
        $(visObj).bind('change',function (e) {
            scm.propagateVis(visObj,viewId);
            scm.markChange(visObj);
        });
    },

    initialize: function ()
    { // unnames all composite controls and then all view controls
      // names will be added back in onchange events
        // mySelf = this; // There is no need for a "mySelf" unless this object is being instantiated.

        var visObj = $('.visDD');
        if (visObj == undefined || visObj.length < 1) {
            warn('DEBUG: Did not find visibility control for composite.');
            return;
        }
        if (visObj.length > 1) {
            warn('DEBUG: Multiple visibility controls for composite???');
            return;
        }

        scm.compositeId = $(visObj).attr('name');
        scm.visIndependent = ($('.subVisDD').length > 0);  // Can subtracks have their own vis?

        // Unname and set up vis propagation
        visObj = visObj[0];
        scm.unnameIt(visObj,true);
        $(visObj).bind('change',function (e) {
            scm.propagateVis(visObj,undefined);
            scm.markChange(visObj);
        });

        // Find all appropriate controls and unname

        // matCBs are easy, they never get named again
        var matCbs = $('input.matCB');
        $(matCbs).each(function (i) {
            scm.unnameIt(this,false);
        });

        // SubCBs will get renamed and on change will name them back.
        var subCbs = $('input.subCB');
        $(subCbs).each(function (i) {
            scm.subCbInit(this);
        });

        // iterate through views
        var viewVis = $('select.viewDD');
        $(viewVis).each(function (i) {
            var classList = $( this ).attr("class").split(" ");
            classList = aryRemove(classList,"viewDD","normalText","changed");
            if (classList.length == 0)
                warn('DEBUG: View classlist is missing view class.');
            else if (classList.length > 1)
                warn('DEBUG: View classlist contains unexpected classes:' + classList);
            else {
                scm.viewIds[i] = classList[0];
                scm.viewInit(scm.viewIds[i]);
            }
        });

        // Tricky for composite level controls.  Could wrap cfg controls in new div.
        // DO THIS AFTER Views
        // NOTE: excluding sortOrder and showCfg which are special cases we don't care about in scm
        var compObjs = $('select,input').filter("[name^='"+scm.compositeId+"\\.'],[name^='"+scm.compositeId+"_']");
        if (compObjs != undefined && compObjs.length > 0) {
            compObjs = $(compObjs).not("[name$='showCfg']");
            $(compObjs).each(function (i) {
                // DEBUG -------------
                if (this.id != undefined
                && this.id.length > 0
                && $(this).hasClass('filterBy') == false
                && $(this).hasClass('filterComp') == false)
                    warn('DEBUG: Not expected control with name ['+this.name + '], and id #'+this.id);
                // DEBUG -------------
                scm.unnameIt(this,true);
                $(this).bind('change',function (e) {
                    scm.propagateSetting(this);
                });
            });
        }
        // Special for checkboxes: boolshad
        var boolObjs = $("input[type='hidden']");
        var boolObjs = $(boolObjs).filter("[name^='boolshad\\."+scm.compositeId+"\\.'],[name^='boolshad\\."+scm.compositeId+"_']");
        $(boolObjs).each(function (i) {
            scm.unnameIt(this,true);
        });

    }
};

// The following js depends upon the jQuery library
$(document).ready(function()
{
    // If divs with class 'subCfg' then initialize the subtrack cfg code
    // NOTE: must be before any ddcl setup
    if (common.subCfg) {
        var divs = $("div.subCfg");
        if (divs != undefined && divs.length > 0) {
            scm.initialize();
        }
    }

    // Initialize sortable tables
    $('table.sortable').each(function (ix) {
        sortTableInitialize(this,true,true);
    });

    // Register tables with drag and drop
    $("table.tableWithDragAndDrop").each(function (ix) {
        tableDragAndDropRegister(this);
    });

    //$('.halfVis').css('opacity', '0.5'); // The 1/2 opacity just doesn't get set from cgi!

    // Put navigation links in top corner
    navigationLinksSetup();
});
