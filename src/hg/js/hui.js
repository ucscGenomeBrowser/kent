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
        exposeAll();
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
    var visDD = $("select.visDD"); // limit to hidden
    if ($(visDD).length == 1 && $(visDD).attr('selectedIndex') == 0)   // limit to hidden
        $(visDD).attr('selectedIndex',$(visDD).children('option').length - 1);
}

function matSubCbClick(subCB)
{
// subCB:onclick  When a subtrack checkbox is clicked, it may result in
// Clicking/unclicking the corresponding matrix CB.  Also the
// subtrack may be hidden as a result.
    matSubCBsetShadow(subCB);
    hideOrShowSubtrack(subCB);
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
    subCbs = $("input.subCB");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        subCbs = $( subCbs ).filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    if(state) { // If clicking [+], further limit to only checked ABCs
        var classes = matAbcCBclasses(false);
        subCbs = objsFilterByClasses(subCbs,"not",classes);  // remove unchecked abcCB classes
    }
    $( subCbs ).each( function (i) {
        this.checked = state;
        matSubCBsetShadow(this);
    });
    if(state)
        exposeAll();  // Unhide composite vis?
    showOrHideSelectedSubtracks();
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
    $("#"+subCB.name+"_4way").val(shadowState);
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
        $('.halfVis').css('opacity', '0.5');
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
    classList = aryRemove(classList,"subCB");
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
        classList = aryRemove(classList,"subCB");
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
        classList = aryRemove(classList,"viewDD","normalText");
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
    var daddy = $(inp).parents(".blueBox");
    if(daddy.length == 1) {
        var classList = $(daddy[0]).attr("class").split(" ");
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
        if(view != "") { list =  $(list).filter(function(index) { return $(this).parents(".blueBox." + view).length == 1; });}
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
        if(view != "") { list =  $(list).filter(function(index) { return $(this).parents(".blueBox." + view).length == 1; });}
        if($(list).length>0)
            $(list).attr("checked",$(inp).attr("checked"));
    }
    else if(inp.type.indexOf("radio") == 0) {
        var list = $("input:radio[name$='"+suffix+"']").not("[name='"+inp.name+"']");
        list = $(list).filter("[value='"+inp.value+"']")
        if(view != "") { list =  $(list).filter(function(index) { return $(this).parents(".blueBox." + view).length == 1; });}
        if($(list).length>0)
            $(list).attr("checked",true);
    }
    else {  // Various types of inputs
        var list = $("input[name$='"+suffix+"']").not("[name='"+inp.name+"']");//.not("[name^='boolshad.']"); // Exclude self from list
        if(view != "") { list =  $(list).filter(function(index) { return $(this).parents(".blueBox." + view).length == 1; });}
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

function registerViewOnchangeAction(viewTrackName)
{
// After composite level view settings are written to HTML it is necessary to go back and
// make sure that each time they change, the change is ajaxed over
    var list = $("input[name^='"+viewTrackName+"\.']");
    $(list).each(function(){setIdRemoveName(this);});
    $(list).change(function(){setCartVarFromObjId(this);});

    list = $("select[name^='"+viewTrackName+"\.']"); // includes composite.view.vis
    $(list).each(function(){setIdRemoveName(this);});
    $(list).change(function(){setCartVarFromObjId(this);});

    list = $("select[name='"+viewTrackName+"']"); // is 'composite' vis
    $(list).each(function(){setIdRemoveName(this);});
    $(list).change(function(){setCartVarFromObjId(this);});
}

function registerFormSubmit(formName)
{
    $('form[name="'+formName+'"]').each(function(i) { formSubmitWaitOnAjax(this)});
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

var popUpTrackName;
var popUpTitle = "";
var popSaveAllVars = null;
function popUpCfgOk(popObj, trackName)
{ // Kicks off a Modal Dialog for the provided content.
    var allVars = getAllVars(popObj, trackName );   // always subtrack cfg
    var changedVars = varHashChanges(allVars,popSaveAllVars);
    //warn("cfgVars:"+varHashToQueryString(changedVars));
    setVarsFromHash(changedVars);
    var newVis = changedVars[trackName];
    if(newVis != null) {
        var sel = $('input[name="'+trackName+'_sel"]:checkbox');
        var checked = (newVis != 'hide' && newVis != '[]');  // subtracks do not have "hide", thus '[]'
        if( $(sel) != undefined ) {
            $(sel).each( function (i) { matSubCBcheckOne(this,checked); });  // Though there is only one, the each works but the direct call does not!
            matSubCBsSelected();
        }
    }
}

function popUpCfg(content, status)
{ // Kicks off a Modal Dialog for the provided content.
    // Set up the modal dialog
    var popit = $('#popit');
    $(popit).html("<div style='font-size:80%'>" + content + "</div>");
    $(popit).dialog({
        ajaxOptions: { cache: true }, // This doesn't work
        resizable: false,
        height: 'auto',
        width: 'auto',
        minHeight: 200,
        minWidth: 700,
        modal: true,
        closeOnEscape: true,
        autoOpen: false,
        buttons: { "OK": function() {
            popUpCfgOk(this,popUpTrackName);
            $(this).dialog("close");
        }},
        open: function() { popSaveAllVars = getAllVars( this, popUpTrackName ); }, // always subtrack cfg
        close: function() { $('#popit').empty(); }
    });
    // Apparently the options above to dialog take only once, so we set title explicitly.
    if(popUpTitle != undefined && popUpTitle.length > 0)
        $(popit).dialog('option' , 'title' , popUpTitle );
    else
        $(popit).dialog('option' , 'title' , "Please Respond");
    $(popit).dialog('open');
}

function _popUpSubrackCfg(trackName,label)
{ // popup cfg dialog
    popUpTrackName = trackName;
    popUpTitle = label;

    $.ajax({
        type: "GET",
        url: "../cgi-bin/hgTrackUi?ajax=1&g=" + trackName + "&hgsid=" + getHgsid() + "&db=" + getDb(),
        dataType: "html",
        trueSuccess: popUpCfg,
        success: catchErrorOrDispatch,
        error: errorHandler,
        cmd: "cfg",
        cache: false
    });
}

function popUpSubtrackCfg(trackName,label)
{
    waitOnFunction( _popUpSubrackCfg, trackName, label );  // Launches the popup but shields the ajax with a waitOnFunction
    return false;
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
        var onlySelected = $("input[name='displaySubtracks']");
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
            possibleClasses = aryRemoveVals(possibleClasses,[ "subCB" ]);
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

// The following js depends upon the jQuery library
$(document).ready(function()
{
    // Initialize sortable tables
    $('table.sortable').each(function (ix) {
        sortTableInitialize(this,true,true);
    });

    // Register tables with drag and drop
    $("table.tableWithDragAndDrop").each(function (ix) {
        tableDragAndDropRegister(this);
    });

    $('.halfVis').css('opacity', '0.5'); // The 1/2 opacity just doesn't get set from cgi!

    $('.filterComp').each( function(i) { // Do this by 'each' to set noneIsAll individually
        if (newJQuery == false)
            $(this).dropdownchecklist({ firstItemChecksAll: true, noneIsAll: $(this).hasClass('filterBy'), maxDropHeight: filterByMaxHeight(this) });
    });

    // Put navigation links in top corner
    navigationLinksSetup();
});
