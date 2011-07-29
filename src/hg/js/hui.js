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

    var subtrackName = subCB.name;
    if (subtrackName == undefined || subtrackName.length == 0)
        subtrackName = subCB.id;
    if (subtrackName != undefined && subtrackName.length > 0) {
        subtrackName = subtrackName.substring(0,subtrackName.length - 4); // '_sel'.length
        scm.enableCfg(subCB,subtrackName,subCB.checked);
    }

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

var scm = { // subtrack config module.
    // This module is for subtrack level config embedded dialogs in hgTrackUi.
    // Subtrack config dialogs are embedded in the subtrack table and get populated when first
    // opened.  Composite and view level controls (parents) when updated override related
    // subtrack controls (children).  Subtrack controls, when updated overide parent controls
    // for the one subtrack.  Controls have no 'name' attribute until the user alters their value.
    // Unnamed controls are not sent to the cart.

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
    // 1) outside in: matrix and subtrack checkboxes to affect vis
    // 2) Non-configurable will need to show/change vis independently! (separate vis control instead of wrench?)
    // 3) SOLVED: Radio buttons work BUT they require name to keep unified set, so rely upon 'changed' class to detect changes.
    // 4) SOLVED: checkboxes: working with name = boolshad.{name}   FIXME: multishad?
    // 5) SOLVED: filterBy,filterComp working: they rely upon id, and id with '.' screwed it all up.  So replaced '.' with '_-'
    // 6) SOLVED: subCfg not matching parent: solved.
    // 7) SOLVED: OpenChromSynth: subtrack filterby needs to be updated by composite filterBy.
    // 8) Remove debug code when ready
    // - check subtrack to enable/disable fauxVis and wrench
    // - matCB should effect subCb including enable/disable fauxVis and wrench
    // - composite/view vis should effect subVis and enable/disable fauxVis and wrench
    // NOTE:
    // Current implementation relies upon '.' delimiter in name and no '_-' in name.  Nothing breaks rule yet...

    //mySelf: null, // There is no need for a "mySelf" unless this object is being instantiated.

    // There is one instance and these vars are page wide
    compositeId: null,
    visIndependent: false,
    viewIds: [],

    markChange: function (obj)
    { // Marks a control as having been changed by the user.  Naming will send value to cart.
        $(obj).addClass('changed');
        if(obj.type.indexOf("radio") != 0) {   // radios must keep their names!
            var oldName = obj.id.replace(/\_\-/g,'.');   // sanitized id replaces '.' with '_-'
            $(obj).attr('name',oldName);
        }
    },

    clearChange: function (obj)
    { // Mark as unchanged
        $(obj).removeClass('changed');
        if(obj.type.indexOf("radio") != 0)   // radios must keep their names!
            $(obj).removeAttr('name');
    },

    markCheckboxChange: function (obj)
    { // Marks a checkbox as having changed and the boolshad too if there is one
        scm.markChange(obj);

        //var safeId = obj.id.replace(/\./g,"\\.");
        //var boolshad = $('input#boolshad\\.' + safeId);
        var boolshad = $('input#boolshad_-' + obj.id);
        if (boolshad != undefined && boolshad.length > 0) {
            if (boolshad.length == 1)
                boolshad = boolshad[0];
            if(obj.checked == false)
                scm.markChange(boolshad);
            else
                scm.clearChange(boolshad);
        }
    },

    hasChanged: function (obj)
    { // Is this object updated (value changed by a user)?
        return $(obj).hasClass('changed');
    },

    unnameIt: function (obj,setId)
    { // removes a control's name so that it will not be updated to the cart upon submit
      // If setId and obj has a name, will set id to current name
        if (setId) {
            var myName = $(obj).attr('name');
            if (myName && myName.length > 0) {
                // DEBUG -------------
                if (myName.indexOf('_-') >= 0)
                    warn("Found control with '_-' in name: "+myName + "<BR>This violates the naming rules and will break javascript code.");
                // DEBUG -------------
                myName = myName.replace(/\./g,'_-');   // sanitized id replaces '.' with '_-'
                $(obj).attr('id',myName);
            }
        }
        scm.clearChange(obj);
    },

    compositeCfgFind: function ()
    { // returns the cfg container for the composite controls
        // TODO: write, create a composite cfg div!
    },

    compositeObjFind: function (suffix)
    { // returns the composite level control for this suffix
        //var compCfg = scm.compositeCfgFind();
        var compObj = undefined;
        if (suffix != undefined) {
            //compObj = $('#'+ scm.compositeId + '\\.' + suffix);
            compObj = $('#'+ scm.compositeId + '_-' + suffix);
            if (compObj == undefined || compObj.length == 0) {
                compObj = $('#'+ scm.compositeId + '_' + suffix);
            }
        } else {
            compObj = $("#"+scm.compositeId);
        }
        if (compObj == undefined || compObj.length == 0) {
            //warn('Could not find viewObj for '+suffix);
            return undefined;
        }
        if (compObj.length == 1)
            compObj = compObj[0];
        return compObj;
    },

    viewIdFind: function (childObj)
    { // returns the name of the view that is a parent of this subtrack control
        var cfg = $(childObj).parents('div.subCfg');
        if (cfg == undefined || cfg.length == 0) {
            warn("Can't find containing div.subCfg of child '"+$(childObj).attr('id')+"'.");
            return undefined;
        }
        var classList = $( cfg[0] ).attr("class").split(" ");
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
        var viewCfg = $('tr#tr_cfg_'+viewId);
        if (viewCfg == undefined || viewCfg.length == 0) {
            warn('Could not find viewCfg for '+viewId);
        }
        if (viewCfg.length == 1)
            viewCfg = viewCfg[0];
        return viewCfg;
    },

    viewObjFind: function (viewId,suffix)
    { // returns the control belonging to this view and suffix
        var viewObj = undefined;
        if (suffix != undefined) {
            var viewCfg = scm.viewCfgFind(viewId);
            //viewObj = $(viewCfg).find("[id$='\\."+suffix+"']");
            viewObj = $(viewCfg).find("[id$='_-"+suffix+"']");
            if (viewObj == undefined || viewObj.length == 0) {
                viewObj = $(viewCfg).find("[id$='_"+suffix+"']");
            }
        } else {
            //viewObj = $("#"+scm.compositeId+"\\."+viewId+"\\.vis");
            viewObj = $("#"+scm.compositeId+"_-"+viewId+"_-vis");
        }
        if (viewObj == undefined || viewObj.length == 0) {
            // Okay not to find a view for this composite level obj
            //warn('Could not find viewObj for '+viewId+'.'+suffix);
            return undefined;
        }
        if (viewObj.length == 1)
            viewObj = viewObj[0];
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
        if (childCfgs == undefined || childCfgs.length == 0)
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
        var childCfg = $(childObj).parents('div.subCfg.filled');
        if (childCfg == undefined || childCfg.length == 0) {
            warn("Can't find childCfg for "+childObj.id);
            return false;
        }
        if (childCfg.length == 1)
            childCfg = childCfg[0];
        return childCfg;
    },

    subCbFind: function (childObj)
    { // returns the subCB for a child vis object
        var childCfg = scm.childCfgFind(childObj);
        if (childCfg == undefined)
            return undefined;

        var tr = $(childCfg).parents('tr').first();
        var subtrack = childCfg.id.substring(8); // 'div_cfg_'.length
        var subCb = $(tr).find("input[name='"+subtrack+"_sel']");
        if (subCb == undefined || subCb.length == 0) {
            warn("Can't find subCB for subtrack: "+subtrack);
            return undefined;
        }
        if (subCb.length == 1)
            subCb = subCb[0];
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
        isVis = (suffix != undefined && suffix == 'vis');

        var viewId = undefined;
        if (isVis) { // This is a view control

            isComposite = (suffix == undefined);
            suffix = undefined;
            if (!isComposite) {
                var classList = $( parentObj ).attr("class").split(" ");
                classList = aryRemove(classList,"viewDD","normalText");
                if (classList.length != 1) {
                    warn("Unexpected view vis class list:"+classList);
                    return [];
                }
                viewId = classList[0];
            }
        } else { // normal obj

            var viewCfg = $(parentObj).parents("tr[id^='tr_cfg_']");
            isComposite = (viewCfg == undefined || viewCfg.length == 0); // is composite
            if (!isComposite) {
                if (viewCfg.length == 1)
                    viewCfg = viewCfg[0];
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
            // deal with hidden boolshads!
            //var parentIdSafe = parentObj.id.replace(/\./g,"\\.");
            //var boolshad = $('input#boolshad\\.'+parentIdSafe);
            var boolshad = $('input#boolshad_-'+parentObj.id);
            if (boolshad != undefined && boolshad.length > 0) {
                if (boolshad.length == 1)
                    boolshad = boolshad[0];
                if (parentObj.checked == false)
                    scm.markChange(boolshad);
                else
                    scm.clearChange(boolshad);
            }
        } else if(parentObj.type.indexOf("radio") == 0) {
            var parentChecked = parentObj.checked;
            var parentVal = $(parentObj).val();
            $(children).each(function (i) {
                if ($(this).val() == parentVal)
                    this.checked = parentObj.checked;
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
        var limitedVis = Math.min (compositeVis,viewObj.selectedIndex);
        var children = scm.childrenFind(viewObj);
        $(children).each(function (i) {
            // TODO: only set if selected?
            $(this).attr('selectedIndex',limitedVis);
            scm.clearChange(this);
        });
    },
    propagateVis: function (parentObj,viewId)
    { // propagate vis settings to subtrack children
        if (viewId == null) {
            // Walk through views and set with this
            var parentVis = parentObj.selectedIndex;
            if (scm.viewIds.length > 0) {
                for (ix=0;ix<scm.viewIds;ix++) {
                    var viewObj = scm.viewObjFind(viewIds[ix],undefined);
                    if (viewObj != undefined)
                        scm.propagateViewVis(viewObj,parentVis);
                }
            } else { // No view so, simple
                var children = scm.childrenFind(parentObj);
                $(children).each(function (i) {
                    var subCb = scm.subCbFind(this);
                    if (subCb != undefined) {
                        if (subCb.disabled != true && subCb.checked) {  // TODO: Integrate with things that enable/check!
                            $(this).attr('selectedIndex',parentVis);
                            scm.clearChange(this);
                        }
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

    inheritSetting: function (childObj)
    { // update value if parents values override child values.
        var myParents = scm.parentsFind(childObj);
        if (myParents == undefined || myParents.length < 1) {
            // DEBUG -------------  It is probably okay to subCfg without parent cfg, but we need to be sure
            warn('No parents were found for childObj: '+childObj.id);
            // DEBUG -------------
            return true;
        }
        var isVis = (undefined == scm.objSuffixGet(childObj));
        if (isVis) {
            var subCb = scm.subCbFind(childObj);
            if (subCb != undefined) {
                if (subCb.disabled == true || subCb.checked == false)   // TODO: Integrate with _sel subCbs and matCbs
                    $(childObj).attr('selectedIndex',0);
                else {
                    var limitedVis = 9;
                    if (myParents.length == 1 && scm.hasChanged(myParents[0]))
                        limitedVis = myParents[0].selectedIndex;
                    else if (myParents.length == 2) {
                        if (scm.hasChanged(myParents[0]) || scm.hasChanged(myParents[1]))
                            limitedVis = Math.min(myParents[0].selectedIndex,myParents[1].selectedIndex);
                    }
                    if (limitedVis < 9)
                        $(childObj).attr('selectedIndex',limitedVis);
                }
            }
        } else {
            var count = 0;
            if(childObj.type.indexOf("checkbox") == 0) {
                $(myParents).each(function (i) {
                    if (scm.hasChanged(this)) {
                        childObj.checked = this.checked;
                        // can ignore boolshad because this does not change
                        count++;
                    }
                });
            } else if(childObj.type.indexOf("radio") == 0) {
                $(myParents).each(function (i) {
                    if (scm.hasChanged(this)) {
                        childObj.checked = this.checked;
                        // parentObj is already "tuned" to the correct radio, so this works
                        count++;
                    }
                });
            } else {// selects and inputs are easy
                $(myParents).each(function (i) {
                    if (scm.hasChanged(this)) {
                        $(childObj).val($(this).val());
                        count++;
                    }
                });
            }
            if (count > 1) // if hasChanged() is working, there should never be more than one
                warn('Both composite and view are seen as updated!  Named update is not working.');
        }
    },

    currentCfg: null, // keep track of cfg while ajaxing, man
    currentSub: null, // keep track of subtrack while ajaxing, dude

    cfgFill: function (content, status)
    { // Finishes the population of a subtrack cfg.  Called by ajax return.
        var cfg = scm.currentCfg;
        scm.currentCfg = null;
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
                    scm.inheritSetting(this); // updates any values that have been changed on this page
                    // if view vis do more than just name it on change
                    var suffix = scm.objSuffixGet(this);
                    if (suffix == undefined) // vis
                        $(this).bind('change',function (e) {
                            scm.visChanged(this);
                        });
                    else {
                        $(this).bind('change',function (e) {
                            if(this.type.indexOf("checkbox") == 0)
                                scm.markCheckboxChange(this);
                            else
                                scm.markChange(this);
                        });
                    }
                } else {//if (this.type == 'hidden') {
                    // Special for checkboixes with name = boolshad.{name}.
                    if ("boolshad." == this.name.substring(0,9)) {
                        scm.unnameIt(this,true);
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
            var subVis = $('div#' + scm.currentSub+'_faux');
            if (subVis != undefined && subVis.length == 1) {
                // SHOULD NOT NEED $(subVis[0]).removeClass('disabled');
                scm.replaceWithVis(subVis[0],scm.currentSub,false);
            }
        }
        else
            shiftLeft *= ($(cfg).position().left - 40);
        $(cfg).css({ width: myWidth+'px',position: 'relative', left: shiftLeft + 'px' });

        // Setting up filterBys must follow show because sizing requires visibility
        if (newJQuery) {
            $(cfg).find('.filterBy,.filterComp').each( function(i) {
                if ($(this).hasClass('filterComp'))
                    ddcl.setup(this);
                else
                    ddcl.setup(this, 'noneIsAll');
            });
        }
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
        var selectHtml = "<SELECT id='"+subtrack+"' class='normalText subVisDD' style='width: 70px'>";
        var selected = $(obj).text();
        if (selected == 'hide')
            selectHtml += "<OPTION SELECTED>hide</OPTION><OPTION>dense</OPTION><OPTION>squish</OPTION><OPTION>pack</OPTION><OPTION>full</OPTION>";
        else if (selected == 'dense')
            selectHtml += "<OPTION>hide</OPTION><OPTION SELECTED>dense</OPTION><OPTION>squish</OPTION><OPTION>pack</OPTION><OPTION>full</OPTION>";
        else if (selected == 'squish')
            selectHtml += "<OPTION>hide</OPTION><OPTION>dense</OPTION><OPTION SELECTED>squish</OPTION><OPTION>pack</OPTION><OPTION>full</OPTION>";
        else if (selected == 'full')
            selectHtml += "<OPTION>hide</OPTION><OPTION>dense</OPTION><OPTION>squish</OPTION><OPTION>pack</OPTION><OPTION SELECTED>full</OPTION>";
        else
            selectHtml += "<OPTION>hide</OPTION><OPTION>dense</OPTION><OPTION>squish</OPTION><OPTION>pack</OPTION><OPTION>full</OPTION>";
        selectHtml += "</SELECT>";
        $(obj).replaceWith(selectHtml);
        if (open) {
            var newObj = $('select#'+subtrack);
            $(newObj).attr('size',5)
            $(newObj).one('blur',function (e) {
                $(this).attr('size',1);
                $(this).unbind()
            });
            $(newObj).one('change',function (e) {
                $(this).attr('size',1);
            });
            // Doesn't work!
            //$(newObj).parents('td').first().attr('valign','top');
            $(newObj).focus();
        }
    },

    enableCfg: function (cb,subtrack,setTo)
    { // Enables or disables subVis and wrench
        if (cb == null || cb == undefined) {
            if (subtrack == null || subtrack.length == 0) {
                warn("scm.enableCfg() called without CB or subtrack.");
                return false;
            }
            cb = $("input[name='"+subtrack+"'_sel]");
            if (cb == undefined || cb.length == 0) {
                warn("scm.enableCfg() could not find CB for subtrack: "+subtrack);
                return false;
            }
        }
        if (cb.length == 1)
            cb = cb[0];

        var td = $(cb).parent('td');
        if (td == undefined || td.length == 0) {
            warn("scm.enableCfg() could not TD for CB: "+cb.name);
            return false;
        }
        if (td.length == 1)
            td = td[0];
        var subFaux = $(td).find('div.subVisDD');
        if (subFaux != undefined && subFaux.length > 0) {
            if (setTo == true)
                $(subFaux).removeClass('disabled');
            else
                $(subFaux).addClass('disabled');
        } else {
            var subVis = $(td).find('select.subVisDD');
            if (subVis != undefined && subVis.length > 0) {
                $(subVis).attr('disable',!setTo);
            }
        }
        var wrench = $(td).find('span.clickable'); // TODO tighten this
        if (wrench != undefined && wrench.length > 0) {
            if (setTo == true)
                $(wrench).removeClass('halfVis');
            else
                $(wrench).addClass('halfVis');
        }
    },
    cfgToggle: function (wrench,subtrack)
    { // Opens/closes subtrack cfg dialog, populating if empty
        var cfg = $("div#div_cfg_"+subtrack);
        if (cfg == undefined || cfg.length == 0) {
            warn("Can't find div_cfg_"+subtrack);
            return false;
        }
        if (cfg.length == 1)
            cfg = cfg[0];

        if ($(cfg).css('display') == 'none') {
            if ($(wrench).hasClass('halfVis'))
                return;
            // Don't allow if this composite is not enabled!
            // find the cb
            var tr = $(cfg).parents('tr').first();
            var subCb = $(tr).find("input[name='"+subtrack+"_sel']");
            if (subCb == undefined || subCb.length == 0) {
                warn("Can't find subCB for "+subtrack);
                return false;
            }
            if (subCb.length == 1)
                subCb = subCb[0];
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

    viewInit: function (viewId)
    { // unnames all view controls
        // iterate through all matching controls and unname
        var tr = $('tr#tr_cfg_'+viewId);
        if (tr == undefined || tr.length == 0) {
            warn('Did not find view: ' + viewId);
            return;
        }
        if (tr.length == 1)
            tr = tr[0];
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
        var visObj = $("select[name='"+scm.compositeId+"\\."+viewId+"\\.vis']");
        if (visObj == undefined || visObj.length == 0) {
            warn('Did not find visibility control for view: ' + viewId);
            return;
        }
        if (visObj.length == 1)
            visObj = visObj[0];
        scm.unnameIt(visObj,true);
        $(visObj).bind('change',function (e) {
            scm.propagateVis(visObj,viewId);
        });
    },

    initialize: function ()
    { // unnames all composite controls and then all view controls
        // mySelf = this; // There is no need for a "mySelf" unless this object is being instantiated.
        scm.compositeId = $('.visDD').first().attr('name');
        scm.visIndependent = ($('.subVisDD').length > 0);

        // Find all appropriate controls and unname

        // matCBs are easy, they never get named again
        var matCbs = $('input.matCB');
        $(matCbs).each(function (i) {
            scm.unnameIt(this,false);
        });

        // Now vis control
        var visObj = $("select[name='"+scm.compositeId+"']");
        if (visObj == undefined || visObj.length == 0) {
            warn('Did not find visibility control for composite.');
            return;
        }
        if (visObj.length == 1)
            visObj = visObj[0];
        scm.unnameIt(visObj,true);
        $(visObj).bind('change',function (e) {
            scm.propagateVis(visObj,null);
        });

        // iterate through views
        var viewVis = $('select.viewDD');
        $(viewVis).each(function (i) {
            var classList = $( this ).attr("class").split(" ");
            classList = aryRemove(classList,"viewDD","normalText");
            if (classList.length == 0)
                warn('View classlist is missing view class.');
            else if (classList.length > 1)
                warn('View classlist contains unexpected classes:' + classList);
            else {
                scm.viewIds[scm.viewIds.length] = classList[0];
                scm.viewInit(classList[0]);
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
                    warn('['+this.name + ']  #'+this.id);
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
    var divs = $("div.subCfg");
    if (divs != undefined && divs.length > 0) {
        scm.initialize();
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

    $('.filterComp').each( function(i) { // Do this by 'each' to set noneIsAll individually
        if (newJQuery == false)
            $(this).dropdownchecklist({ firstItemChecksAll: true, noneIsAll: $(this).hasClass('filterBy'), maxDropHeight: filterByMaxHeight(this) });
    });

    // Put navigation links in top corner
    navigationLinksSetup();
});
