// JavaScript Especially for hui.c

// Don't complain about line break before '||' etc:
/* jshint -W014 */
/* jshint esnext: true */

// The 'mat*' functions are designed to support subtrack config by 2D+ matrix of controls

function _matSelectViewForSubTracks(obj,view)
{
// viewDD:onchange Handle any necessary changes to subtrack checkboxes when the view changes
// views are "select" drop downs on a subtrack configuration page
    var classesHidden = ""; // Needed for later
    var matCBs = null;

    if (obj.selectedIndex === 0) { // hide
        matSubCBsEnable(false,view);
        hideConfigControls(view);

        // Needed for later
        classesHidden = matViewClasses('hidden');
        classesHidden = classesHidden.concat( matAbcCBclasses(false) );
    } else {
        // Make main display dropdown show full if currently hide
        exposeAll();  // TODO: was removed for subCfg... is there a problem?
        matSubCBsEnable(true,view);

        // Needed for later
        classesHidden = matViewClasses('hidden');
        classesHidden = classesHidden.concat( matAbcCBclasses(false) );

        // If hide => show then check all subCBs matching view and matCBs
        // If show => show than just enable subCBs for the view.
        if (obj.lastIndex === 0) { // From hide to show
            // If there are matCBs then we will check some subCBs if we just went from hide to show
            matCBs = $("input.matCB:checked");
            if (matCBs.length > 0) {
                // Get list of all checked abc classes first
                var classesAbcChecked = [];
                matCBs.filter(".abc").each( function (i) {
                    var classList = $( this ).attr("class").split(" ");
                    classesAbcChecked.push( 
                            aryRemove(classList,["matCB","changed","disabled","abc"]) );
                });

                // Walk through checked non-ABC matCBs and check related subCBs
                var subCbsSelected = filterCompSubCBsSurviving();
                var subCBs = $(subCbsSelected).filter("."+view).not(":checked");
                matCBs.not(".abc").each( function (i) {
                    var classList = $( this ).attr("class").split(" ");
                    classList = aryRemove(classList,["matCB","changed","disabled"]);
                    var subCBsMatching = objsFilterByClasses(subCBs,"and",classList);
                    if (classesAbcChecked.length>0)
                        subCBsMatching = objsFilterByClasses(subCBsMatching,"or",classesAbcChecked);
                    // Check the subCBs that belong to this view and checked matCBs
                    subCBsMatching.each( function (i) {
                        this.checked = true;
                        matSubCBsetShadow(this,true);  // will update "subCfg" if needed
                        hideOrShowSubtrack(this);
                    });

                });
            } // If no matrix, then enabling is all that was needed.

            // fix 3-way which may need to go from unchecked to .disabled
            matCBs = $("input.matCB").not(".abc").not(".disabled").not(":checked");
            if (matCBs.length > 0) {
                $( matCBs ).each( function (i) { matChkBoxNormalize( this, classesHidden ); });
            }
        }
    }
    // fix 3-way matCBs which may need to go from disabled to checked or unchecked depending
    matCBs = matCBsWhichAreComplete(false);
    if (matCBs.length > 0) {
        if ($("select.viewDD").not("[selectedIndex=0]").length === 0) {      // No views visible so
            $( matCBs ).each( function (i) { matCbComplete( this, true ); });// nothing inconsistent
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
    if (visDD) {
        if ($(visDD).prop('selectedIndex') === 0) {
            $(visDD).prop('selectedIndex',$(visDD).children('option').length - 1);
	        $(visDD).trigger("change");// trigger on change code, which may trigger supertrack reshaping
        }                         // and effecting inherited subtrack vis

        // If superChild and hidden by supertrack, wierd things go on unless we trigger reshape
        if ($(visDD).hasClass('superChild'))
            visTriggersHiddenSelect(visDD);
    }
}

function matSubCbClick(subCB)
{
// subCB:onclick  When a subtrack checkbox is clicked, it may result in
// Clicking/unclicking the corresponding matrix CB.  Also the
// subtrack may be hidden as a result.

    // NOTE: if "subCfg" then 'change' event will update it
    if (isFauxDisabled(subCB,false)) { // disabled subCB is still clickable when "subCfg"
        subCB.checked = true;
        fauxDisable(subCB,false,""); // enable and get rid of message
    }
    matSubCBsetShadow(subCB,false);
    hideOrShowSubtrack(subCB);
    // When subCBs are clicked, 3-state matCBs may need to be set
    var classes = matViewClasses('hidden');
    classes = classes.concat( matAbcCBclasses(false) );
    var matCB = matCbFindFromSubCb( subCB );
    if (matCB) {
        matChkBoxNormalize( matCB, classes );
    }

    if (subCB.checked)
        exposeAll();  // Unhide composite vis?

    matSubCBsSelected();
}

function matCbClick(matCB)
{
// matCB:onclick  When a matrix CB is clicked, the set of subtracks checked may change
// Also called indirectly by matButton:onclick via matSetMatrixCheckBoxes

    var classList = $( matCB ).attr("class").split(" ");
    var isABC = (aryFind(classList,"abc") !== -1);
    classList = aryRemove(classList,["matCB","changed","disabled","abc"]);
    if (classList.length === 0 )
       matSubCBsCheck(matCB.checked);
    else if (classList.length === 1 )
       matSubCBsCheck(matCB.checked,classList[0]);               // dimX or dimY or dim ABC
    else if (classList.length === 2 )
       matSubCBsCheck(matCB.checked,classList[0],classList[1]);  // dimX and dimY
    else
        warn("ASSERT in matCbClick(): There should be no more than 2 entries in list:"+classList);

    if (!isABC)
        matCbComplete(matCB,true); // No longer partially checked

    if (isABC) {  // if dim ABC then we may have just made indeterminate X and Ys determinate
        if (matCB.checked === false) { // checking new dim ABCs cannot change indeterminate state.
            var matCBs = matCBsWhichAreComplete(false);
            if (matCBs.length > 0) {
                if ($("input.matCB.abc:checked").length === 0) // No dim ABC checked
                    $( matCBs ).each( function (i) { matCbComplete( this, true ); });
                else {
                    var classes = matViewClasses('hidden');
                    classes = classes.concat( matAbcCBclasses(false) );
                    $( matCBs ).each( function (i) { matChkBoxNormalize( this, classes ); });
                }
            }
        }
    }

    if (matCB.checked)
        exposeAll();  // Unhide composite vis?
    matSubCBsSelected();
}

function _matSetMatrixCheckBoxes(state)
{   // matButtons:onclick Set all Matrix checkboxes to state.
    // If additional arguments are passed in, the list of CBs will be narrowed by the classes

    var matCBs = $("input.matCB").not(".abc");
    for (var vIx=1;vIx<arguments.length;vIx++) {
        matCBs = $( matCBs ).filter("."+arguments[vIx]);  // Successively limit list
    }
    $( matCBs ).each( function (i) {
        this.checked = state;
        matCbComplete(this,true);
    });
    var subCbs = $("input.subCB");
    for (var sIx=1;sIx<arguments.length;sIx++) {
        subCbs = $( subCbs ).filter("."+arguments[sIx]);  // Successively limit list
    }
    if (state) { // If clicking [+], further limit to only checked ABCs
        var classes = matAbcCBclasses(false);
        subCbs = objsFilterByClasses(subCbs,"not",classes);  // remove unchecked abcCB classes
    }
    $( subCbs ).each( function (i) {
        if (this.checked !== state) {
            this.checked = state;
            matSubCBsetShadow(this,false);
            $(this).trigger("change");  // NOTE: if "subCfg" then 'change' event will update it
        }
    });
    if (state)
        exposeAll();  // Unhide composite vis?
    showOrHideSelectedSubtracks();
    matSubCBsSelected();

    var tbody = normed($('tbody.sortable'));
    if (tbody)
         $(tbody).removeClass('sorting');
    return true;
}

function matSetMatrixCheckBoxes(state)
{ // Called exclusively by matrix [+][-] buttons on click
    var tbody = normed($('tbody.sortable'));
    if (tbody)
         $(tbody).addClass('sorting');

    if (arguments.length >= 5)
        waitOnFunction(_matSetMatrixCheckBoxes,state,arguments[1],arguments[2],arguments[3],
                                                                               arguments[4]);
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
// matCB - matrix dimX and dimY CB controls (in some cases this set contains abcCBs as well
//         because they are part of the matrix)
// abcCB - matrix dim (ABC) CB controls
// subCB - subtrack CB controls
// What does work
// 1) 4 state subCBs: checked/unchecked enabled/disabled (which is visible/hidden)
// 2) 3 state matCBs for dimX and Y but not for Z (checked,unchecked,indeterminate)
//     (incomplete set of subCBs for this matCB))
// 3) cart vars for viewDD, abcCBs and subCBs but matCBs set by the state of those 3
// What is awkward or does not work
// A) Awkward: matCB could be 5 state (all,none,subset,superset,excusive non-matching set)
function matSubCBsCheck(state)
{   // Set all subtrack checkboxes to state.  If additional arguments are passed in,
    // the list of CBs will be narrowed by the classes
    // called by matCB clicks (matCbClick()) !
    var subCBs = $("input.subCB");
    for (var vIx=1;vIx<arguments.length;vIx++) {
        subCBs = subCBs.filter("."+arguments[vIx]);  // Successively limit list
    }

    if (state) { 
        // If checking subCBs, then make sure up to 3 dimensions of matCBs
        // agree with each other on subCB verdict
        var classes = matAbcCBclasses(false);
        if (classes.length > 0)
            subCBs = objsFilterByClasses(subCBs,"not",classes);  // remove unchecked abcCB classes
        if (arguments.length === 1 || arguments.length === 3) { // Requested dimX&Y: check ABC state
            $( subCBs ).each( function (i) { matSubCBcheckOne(this,state); });
        } else {//if (arguments.length === 2) { // Requested dim ABC (or only 1 dim so harmless)
            var matXY = $("input.matCB").not(".abc");  // check X&Y state
            matXY = $( matXY ).filter(":checked");
            if (matXY.length === 0)
                subCBs.each( function (i) { matSubCBcheckOne(this,state); });
            for (var mIx=0;mIx<matXY.length;mIx++) {
                classes = $(matXY[mIx]).attr("class").split(' ');
                classes = aryRemove(classes,["matCB","changed","disabled"]);
                /* jshint loopfunc: true */// function inside loop works and replacement is awkward.
                $( subCBs ).filter('.'+classes.join(".")).each( function (i) { 
                                                                   matSubCBcheckOne(this,state); 
                                                            });
            }
        }
    } else  // state not checked so no filtering by other matCBs needed
        subCBs.each( function (i) { matSubCBcheckOne(this,state); });

    return true;
}

function matSubCBsEnable(state)
{   // Enables/Disables subtracks checkboxes.
    // If additional arguments are passed in, the list of CBs will be narrowed by the classes
    var subCBs = $("input.subCB");
    for (var vIx=1;vIx<arguments.length;vIx++) {
        if (arguments[vIx].length > 0)
            subCBs = subCBs.filter("."+arguments[vIx]);  // Successively limit list
    }
    subCBs.each( function (i) {
        if (state) {
            fauxDisable(this,false,'');
            $(this).parent().attr('cursor','');
        } else {
            fauxDisable(this,true, 'view is hidden');
            $(this).parent().attr('cursor','pointer');
        }
        matSubCBsetShadow(this,true);    // will update "subCfg" if needed
        hideOrShowSubtrack(this);
    });

    return true;
}

function matSubCBcheckOne(subCB,state)
{
// setting a single subCB may cause it to appear/disappear
    if (subCB.checked !== state) {
        subCB.checked = state;
        matSubCBsetShadow(subCB,false);
        $(subCB).trigger("change");  // NOTE: if "subCfg" then 'change' event will update it
        hideOrShowSubtrack(subCB);
    }
}

function matSubCBsetShadow(subCB,triggerChange)
{   // Since CBs only get into cart when enabled/checked,
    // the shadow control enables cart to know other states
    //  will update "subCfg" if needed
    var shadowState = 0;
    if (subCB.checked)
        shadowState = 1;
    if (isFauxDisabled(subCB,true))
        shadowState -= 2;
    var noDot = subCB.id.replace(/\./g, "\\.");
    var fourWay = normed($("input.cbShadow#boolshad\\."+noDot));
    if (!fourWay) {
        warn("DEBUG: Failed to find fourWay shadow for '#"+noDot);
        return;
    }
    if ($(fourWay).val() !== shadowState.toString()) {
        $(fourWay).val(shadowState);
        if (typeof(subCfg) === "object") { // Is subCfg.js file included
            subCfg.enableCfg(subCB,(shadowState === 1));
            if (triggerChange)
                $(subCB).trigger("change"); // 'change' event will update "subCfg"
        }
    }
}

function matChkBoxNormalize(matCB)
{   // Makes sure matCBs are in one of 3 states (checked,unchecked,indeterminate)
    // based on matching set of subCBs
    var classList = $( matCB ).attr("class").split(" ");
    var isABC = (aryFind(classList,"abc") !== -1);
    if (isABC)
        alert("ASSERT: matChkBoxNormalize() called for dim ABC!");
    classList = aryRemove(classList,["matCB","changed","disabled"]);

    // create string filter of classes converting "matCB K562 H3K4me1" as ".K562.H3K4me1"
    var classes = '.' + classList.join(".");
    var subCBs = mtxSubMap[classes]; // All subtrack CBs that match matrix CB

    if (arguments.length > 1 && arguments[1].length > 0) { // dim ABC NOT classes
        subCBs = objsFilterByClasses(subCBs,"not",arguments[1]);
    }

    // Only look at visible views
    subCBs = $(subCBs).not('.disabled').not(":disabled");

    if (subCBs.length > 0) {
        var CBsChecked = subCBs.filter(":checked");
        if (!isABC) {
            if (CBsChecked.length === subCBs.length) {
                matCbComplete(matCB,true);
                $(matCB).attr('checked',true);
            } else if (CBsChecked.length === 0) {
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
    if (complete) {
        fauxDisable(matCB,false,"");
    } else {
        fauxDisable(matCB,true, "Not all associated subtracks have been selected");
    }
}

function matCBsWhichAreComplete(complete)
{   // Returns a list of currently indeterminate matCBs.
    // This is encapsulated to keep consistent with matCbComplete()
    if (complete)
        return $("input.matCB").not(".disabled");
    else
        return $("input.matCB.disabled");
}

function matCbFindFromSubCb(subCB)
{   // returns the one matCB associated with a subCB (or undefined)
    var classList =  $( subCB ).attr("class").split(" ");
    // we need one or 2 classes, depending upon how many dimensions in matrix
    // (e.g. "subDB GM10847 NFKB aNone IGGrab Signal")
    classList = aryRemove(classList,["subCB","changed","disabled"]);
    var classes = classList.slice(0,2).join('.');   // Assume X and Y they are the first 2
    if (classes==="")
        // no hit, so this must be a 1D matrix. 
        // New jquery does not accept selectors that end with . anymore, so return right away
        return undefined;

    var matCB = $("input.matCB."+classes).not(".abc"); // Note, this works for filtering because we want 'AND'
    if (matCB.length === 1)
        return matCB;

    matCB = $("input.matCB."+classList[0]).not(".abc"); // No hit so this must be a 1D matrix
    if (matCB.length === 1)
        return matCB;

    return undefined;
}

function matAbcCBfindFromSubCb(subCB)
{
// returns the abcCBs associated with a subCB (or undefined)
    var abcCBs = $("input.matCB.abc");
    if (abcCBs.length > 0) {
        var classList =  $( subCB ).attr("class").split(" ");
        classList = aryRemove(classList,["subCB","changed","disabled"]);
        classList.shift(); // Gets rid of X and Y associated classes (first 2 after subCB)
        classList.shift();
        classList.pop();   // gets rid of view associated class (at end)
        if (classList.length >= 1) {
            var abcCB = $(abcCBs).filter('.'+classList.join("."));
            if (abcCB.length >= 1)
                return abcCB;
        }
    }
    return undefined;
}

function objsFilterByClasses(objs,keep,classes)
{
// Accepts an obj list and an array of classes, then filters successively by that list
    if (classes && classes.length > 0 ) {
        if (keep === "and") {
            objs = $( objs ).filter( '.' + classes.join('.') ); // Must belong to all
        } else if (keep === "or") {
            objs = $( objs ).filter( '.' + classes.join(',.') ); // Must belong to one or more
        } else if (keep === "not") {
            for (var cIx=classes.length-1;cIx>-1;cIx--) {
                // not('class1.class2') is different from not('.class1').not('.class2')
                objs = $( objs ).not( '.' + classes[cIx] ); 
            }
        }
    }
    return objs;
}

function matViewClasses(limitTo)
{   // returns an array of classes from the ViewDd: 
    // converts "viewDD normalText SIG"[]s to "SIG","zRAW"
    var classes = [];
    var viewDDs = $("select.viewDD");//.filter("[selectedIndex='0']");
    if (limitTo === 'hidden') {
        viewDDs = $(viewDDs).filter("[selectedIndex=0]");
    } else if (limitTo === 'visible') {
        viewDDs = $(viewDDs).not("[selectedIndex=0]");
    }
    $(viewDDs).each( function (i) {
        var classList = $( this ).attr("class").split(" ");
        classList = aryRemove(classList,["viewDD","normalText","changed"]);
        classes.push( classList[0] );
    });
    return classes;
}

function matAbcCBclasses(wantSelected)
{   // returns an array of classes from the dim ABC CB classes:
    // converts "matCB abc rep1"[]s to "rep1","rep2"
    var classes = [];
    var abcCBs = $("input.matCB.abc");
    if (abcCBs.length > 0) {
        if (!wantSelected) {
            abcCBs = abcCBs.not(":checked");
        } else {
            abcCBs = abcCBs.filter(":checked");
        }
        $(abcCBs).each( function (i) {
            var classList = $( this ).attr("class").split(" ");
            classList = aryRemove(classList,["matCB","changed","disabled","abc"]);
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
    var counter = normed($('.subCBcount'));
    if (counter) {
        var subCBs =  $("input.subCB");                   // subCfg uses fauxDisabled
        $(counter).text($(subCBs).filter(":enabled:checked").not('.disabled').length + 
                                                            " of " +$(subCBs).length+ " selected");
    }

    if (typeof(ddcl) === "object") {
        setTimeout(function(){ ddcl.reinitFacetCounts(); }, 50);
    }

}

/////////////////// subtrack configuration support ////////////////
function compositeCfgUpdateSubtrackCfgs(inp)
{
// Updates all subtrack configuration values when the composite cfg is changed
    // If view association then find it:
    var view = "";
    var list = null;
    var daddy = normed($(inp).parents(".blueBox"));
    if (daddy) {
        var classList = $(daddy).attr("class").split(" ");
        if (classList.length === 2) {
            view = classList[1];
        }
    }
    var suffix = inp.name.substring(inp.name.indexOf("."));  // Includes '.'
    if (suffix.length === 0) {
        warn("Unable to parse '"+inp.name+"'");
        return true;
    }
    if (inp.type.indexOf("select") === 0) {
        list = $("select[name$='"+suffix+"']").not("[name='"+inp.name+"']"); // Exclude self
        if (view !== "") { 
            list =  $(list).filter(function(index) { 
                              return (normed($(this).parents(".blueBox." + view)) !== undefined); 
                    });
        }
        if ($(list).length > 0) {
            if (inp.multiple !== true)
                $(list).attr('selectedIndex',inp.selectedIndex);
            else {
                $(list).each(function(view) {  // for all dependent (subtrack) multi-selects
                    sel = this;
                    if (view !== "") {
                        var hasClass = $(this).parents(".blueBox." + view);
                        if (hasClass.length !== 0)
                            return;
                    }
                    // for all options of dependent mult-selects set selected state
                    // to independent (parent) selected state
                    $(this).children('option').each(function() {  
                        $(this).attr('selected',$(inp).children(
                                                    'option:eq('+this.index+')').attr('selected')); 
                    });
                    $(this).attr('size',$(inp).attr('size'));
                });
            }
        }
    }
    else if (inp.type.indexOf("checkbox") === 0) {
        list = $("checkbox[name$='"+suffix+"']").not("[name='"+inp.name+"']"); // Exclude self
        if (view !== "") { 
            list =  $(list).filter(function(index) { 
                                return (normed($(this).parents(".blueBox." + view)) !== undefined);
                    });
        }
        if ($(list).length > 0)
            $(list).attr("checked",$(inp).attr("checked"));
    }
    else if (inp.type.indexOf("radio") === 0) {
        list = $("input:radio[name$='"+suffix+"']").not("[name='"+inp.name+"']");
        list = $(list).filter("[value='"+inp.value+"']");
        if (view !== "") { 
            list =  $(list).filter(function(index) { 
                                return (normed($(this).parents(".blueBox." + view)) !== undefined); 
                    });
        }
        if ($(list).length > 0)
            $(list).attr("checked",true);
    }
    else {  // Various types of inputs
        list = $("input[name$='"+suffix+"']").not("[name='"+inp.name+"']");// Exclude self from list
        if (view !== "") { 
            list =  $(list).filter(function(index) { 
                                return (normed($(this).parents(".blueBox." + view)) !== undefined);
                    });
        }
        if ($(list).length > 0)
            $(list).val(inp.value);
    }
    return true;
}

function compositeCfgRegisterOnchangeAction(prefix)    // FIXME: OBSOLETE when subCfg is released
{
// After composite level cfg settings written to HTML it is necessary to go back and
// make sure that each time they change, any matching subtrack level cfg setting are changed.
    var list = $("input[name^='"+prefix+".']").not("[name$='.vis']");
    $(list).on("change", function(){compositeCfgUpdateSubtrackCfgs(this);});

    list = $("select[name^='"+prefix+".']").not("[name$='.vis']");
    $(list).on("change", function(){compositeCfgUpdateSubtrackCfgs(this);});
}

function subtrackCfgHideAll(table)
{
// hide all the subtrack configuration stuff
    $("div[id^='div_cfg_']").each( function (i) {
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
    if (($(divit).css('display') === 'none')
    && metadataIsVisible(tableName))
        metadataShowHide(tableName,"","");

    // Could have all inputs commented out, then uncommented when clicked:
    // But would need to:
    // 1) be able to find composite view level input
    // 2) know if subtrack input is non-default (if so then subtrack value overrides
    //    composite view level value)
    // 3) know whether so composite view level value has changed since hgTrackUi displayed
    //    (if so composite view level value overrides)
    $(divit).toggle();
    return false;
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
    var trs  = $("tr[id^='tr_cfg_']");
    $("input[name$='.showCfg']").val("off"); // Turn all off
    $( trs ).each( function (i) {
        if (this.id === 'tr_cfg_'+name && this.style.display === 'none') {
            $( this ).css('display','');
	    $("input[name$='."+name+".showCfg']").val("on");
            var cfgBox = this;
            // Since filterBys amy have been hidden on page load, must reinit them now.
            $(cfgBox).find('.filterBy').each( function(i) {
                ddcl.reinit(this,false); // false means conditioned on failed initial setup.
            });
        }
        else if (this.style.display === '') {
            $( this ).css('display','none');
        }
    });

    // Close the cfg controls in the subtracks
    $("table.subtracks").each( function (i) { subtrackCfgHideAll(this);} );
    return true;
}

function hideOrShowSubtrack(obj)
{
// This can show/hide a tablerow that contains a specific object
// Containing <tr>'s must be id'd with 'tr_' + obj.id
// Also, this relies upon the "displaySubtracks" radio button control
    var tr = normed($(obj).parents('tr#tr_'+obj.id));
    if (tr) {
        if (!obj.checked || isFauxDisabled(obj,true))  {

	    var g = common.track;
	    var sel = normed($("#"+g+"_displaySubtracks_selected"));
	    if (!sel)
		sel = normed($("#displaySubtracks_selected"));
	    if (sel.checked && sel.value === "selected") {
		$(tr).hide();
		return;
	    }

        }
        $(tr).show();
    }
}

function showSubTrackCheckBoxes(onlySelected)
{
// If a Subtrack configuration page has show "only selected subtracks" option,
// This can show/hide tablerows that contain the checkboxes
// Containing <tr>'s must be id'd with 'tr_' + the checkbox id,
// while checkbox id must have 'cb_' prefix (ie: 'tr_cb_checkThis' & 'cb_checkThis')
    var trs = $('table.subtracks').children('tbody').children('tr');
    if (!onlySelected)
        $(trs).show();
    else {
        $(trs).each(function (ix) {
            var subCB = normed($(this).find('input.subCB'));
            if (subCB) {
                if (subCB.checked && isFauxDisabled(subCB,true) === false)
                    $(this).show();
                else
                    $(this).hide();
            }
            //else
            //    warn('DEBUG: subtrack table row '+ix+' without subCB?');
        });
    }
}
function showOrHideSelectedSubtracks(inp)
{
// Show or Hide subtracks based upon radio toggle
    var showHide;
    if (arguments.length > 0)
        showHide=inp;
    else {
        var onlySelected = $("input.allOrOnly");
        if (onlySelected.length > 0)
            showHide = onlySelected[0].checked;
        else
            return;
    }
    showSubTrackCheckBoxes(showHide);

    var tbody = $("tbody.sortable");
    $(tbody).each(function (i) {
        sortTable.alternateColors(this);
    });
}

///// Following functions called on page load
function matInitializeMatrix()
{
// Called at Onload to coordinate all subtracks with the matrix of check boxes
    //var start = startTiming();

    matSubCBsSelected(); // counts
    //showOrHideSelectedSubtracks();    // Don't need this because hui.c is doing it

    //showTiming(start,"matInitializeMatrix()");
}

function multiSelectLoad(div,sizeWhenOpen)
{
    //var div = $(obj);//.parent("div.multiSelectContainer");
    var sel = normed($(div).children("select:first"));
    if (div && sel && sizeWhenOpen <= $(sel).length) {
        $(div).css('width', ( $(sel).clientWidth ) +"px");
        $(div).css('overflow',"hidden");
        $(div).css('borderRight',"2px inset");
    }
    $(sel).show();
}

function multiSelectCollapseToSelectedOnly(obj)
{
    var items = $(obj).children("option");
    if (items.length > 0) {
        $(items).not(":selected").hide();
    }
    $(obj).attr("size",$(items).filter(":selected").length);
}

function multiSelectBlur(obj)
{ // Closes a multiselect and colapse it to a single option when appropriate
    var val = $(obj).val();
    if (!val || val === "") {
        $(obj).val("All");
        $(obj).attr('selectedIndex',0);
    }
    if ($(obj).attr('selectedIndex') === 0) // Close if selected index is 1
        $(obj).attr('size',1);
    else
        multiSelectCollapseToSelectedOnly(obj);
}

function filterCompositeSet(obj,all)
{ // Sets/Clears all subtrack checkboxes via [+] or [-] buttons
  // Resets any filter composites so they work as expected after button press
    var filterComp = $("select.filterComp");
    $(filterComp).each(function(i) {
        $(this).val("All");
        $(this).dropdownchecklist("refresh");
        ddcl.onComplete($(this));
    });
    matSubCBsCheck(all);
    matSubCBsSelected(); // Be sure to update the counts!
}

function matCbFilterClasses(matCb,joinThem)
{ // returns the var associated with a filterComp

    var classes = $( matCb ).attr("class").split(" ");
    classes = aryRemove(classes,["matCB","changed","disabled","abc"]);
    if (joinThem)
        return '.' + classes.join('.');
    return classes;
}

function filterCompFilterVar(filter)
{ // returns the var associated with a filterComp

    if ($(filter).hasClass('filterComp') === false)
        return undefined;

    // Find the var for this filter
    var parts = $(filter).attr("name").split('.');
    return parts[parts.length - 1];
}

function filterCompApplyOneFilter(filter,subCbsRemaining)
{ // Applies a single filter to a filterTables TRs
    var classes = $(filter).val();
    if (!classes || classes.length === 0)
        return []; // Nothing selected so exclude all rows

    if (classes[0] === 'All')
        return subCbsRemaining;  // nothing excluded by this filter

    var subCbsAccumulating = [];
    for (var ix=0;ix<classes.length;ix++) {
        var subCBOneFIlter = $(subCbsRemaining).filter('.' + classes[ix]);
        if (subCBOneFIlter.length > 0)
            subCbsAccumulating = jQuery.merge( subCbsAccumulating, subCBOneFIlter );
    }
    return subCbsAccumulating;
}

function filterCompApplyOneMatCB(matCB,remainingCBs)
{ // Applies a single filter to a filterTables TRs
    var classes = matCbFilterClasses(matCB,true);
    if (!classes || classes.length === 0)
        return []; // Nothing selected so exclude all rows

    return $(remainingCBs).filter(classes);
}

function filterCompSubCBsSurviving(filterClass)
// returns a list of trs that satisfy all filters
// If defined, will exclude filter identified by filterClass
{
    // find all filterable table rows
    var subCbsFiltered = $("input.subCB");// Default all
    if (!subCbsFiltered || subCbsFiltered.length === 0)
        return [];

    // Find all filters
    var filters = $("select.filterComp");
    if (filters.length === 0)
        return [];

    // Exclude one if requested.
    if (filterClass && filterClass.length > 0)
        filters = $(filters).not("[name$='" + filterClass + "']");

    for (var ix=0;subCbsFiltered.length > 0 && ix < filters.length;ix++) {
        subCbsFiltered = filterCompApplyOneFilter(filters[ix],subCbsFiltered);//successively reduces
    }

    // Filter for Matrix CBs too
    if (subCbsFiltered.length > 0) {
        var matCbAll = $("input.matCB");
        var matCb = $(matCbAll).filter(":checked");
        if (matCbAll.length > matCb.length) {
            if (matCb.length === 0)
                subCbsFiltered = [];
            else {
                var subCbsAccumulating = [];
                for (var mIx=0;mIx<matCb.length;mIx++) {
                    var subCBOneFIlter = filterCompApplyOneMatCB(matCb[mIx],subCbsFiltered);
                    if (subCBOneFIlter.length > 0)
                        filteredCBs = jQuery.merge( subCbsAccumulating, subCBOneFIlter );
                }
                subCbsFiltered = subCbsAccumulating;
            }
        }
    }
    return subCbsFiltered;
}

function _filterComposite()
{ // Called when filterComp selection changes.  Will check/uncheck subtracks
    var subCbsSelected = filterCompSubCBsSurviving();

    // Uncheck:
    var subCbsToUnselect = $("input.subCB:checked");// Default all
    if (subCbsToUnselect.length > 0)
        $(subCbsToUnselect).each( function (i) { matSubCBcheckOne(this,false); });

    // check:
    if (subCbsSelected.length > 0)
        $(subCbsSelected).each( function (i) { matSubCBcheckOne(this,true); });

    // Update count
    matSubCBsSelected();

    var tbody = normed($('tbody.sortable'));
    if (tbody)
         $(tbody).removeClass('sorting');
}

function filterCompositeTrigger()
{ // Called when filterComp selection changes.  Will check/uncheck subtracks
    var tbody = normed($('tbody.sortable'));
    if (tbody)
         $(tbody).addClass('sorting');

    waitOnFunction(_filterComposite);
}

function filterCompositeDone(event)
{ // Called by custom 'done' event
    event.stopImmediatePropagation();
    $(this).unbind( event );
    filterCompositeTrigger();

}

function filterCompositeSelectionChanged(obj)
{   // filterComposite:onchange Set subtrack selection based upon the changed filter
    // [Not called for filterBy]
    var subCBs = $("input.subCB");
    if (subCBs.length > 300) {
        $(obj).one('done',filterCompositeDone);
        return;
    } else
        filterCompositeTrigger();

}

function filterCountFacets(possibleClasses, hiddenViewClasses, possibleSelections)
{

    possibleClasses = aryRemove(possibleClasses,[ "subCB","changed","disabled" ]);

    if ($(possibleClasses).length > 0) {

	if (aryFind(hiddenViewClasses, possibleClasses[possibleClasses.length-1])=== -1) {

	    for (var vIx=0; vIx < possibleClasses.length; vIx++) {
		var onePoss = possibleClasses[vIx];
		if (possibleSelections[onePoss] === undefined) {
		    possibleSelections[onePoss] = 1;
		} else {
		    ++possibleSelections[onePoss];
		}
	    }

	}
    }
}

function filterCompositeExcludeOptionsQuick(multiSelect, possibleSelections)
{ // Will mark all options in one filterComposite boxes that are inconsistent with the current
  // selections in other filterComposite boxes.  Mark is class ".excluded"

    var magic = "Magic23541236746574569670787890877563457346846578452346457435645t794312";

    if (possibleSelections[magic] === undefined) {

	var allSubCBs = $("input.subCB");
	if (allSubCBs.length === 0) {
	    return false;
	}

	// IE takes tooo long, so this should be called only when leaving the filterBy box
	if (theClient.isIePre11() && $(allSubCBs).filter(":checked").length > 300) 
	    return false;

	// Walk through all subCBs to get other related tags
	$( allSubCBs ).each( function (i)  {
	    // is it viz?
	    if (this.checked && isFauxDisabled(this,true) === false) {
		var possibleClasses = $( this ).attr("class").split(" ");
		// [] because it does not need view filtering
		filterCountFacets(possibleClasses, [], possibleSelections);  
	    }
	});
	possibleSelections[magic] = 1;  // mark that it has been initialized
    } 


    // Walk through all options in this filterBox to set excluded class
    var opts = $(multiSelect).children("option");
    for (var ix = 1;ix < opts.length;ix++) { // All is always allowed
	var count = possibleSelections[opts[ix].value];
	if (count === undefined) {
	    count = 0;
	}
	opts[ix].facetCount = count;
    }

    return true;
    
}


function filterCompositeExcludeOptions(multiSelect)
{ // Will mark all options in one filterComposite boxes that are inconsistent with the current
  // selections in other filterComposite boxes.  Mark is class ".excluded"

    // Compare to the list of all trs
    var allSubCBs = $("input.subCB");
    if (allSubCBs.length === 0) {
        return false;
    }

    // IE takes tooo long, so this should be called only when leaving the filterBy box
    if (theClient.isIePre11() && $(allSubCBs).filter(":checked").length > 300) 
        return false;

    var filterClass = filterCompFilterVar(multiSelect);  // returns facet name e.g. sample_source
    if (!filterClass || filterClass.length === 0) {
        return false; 
    }

    var updated = false;
    // Look at list of CBs that would be selected if all were selected for this filter
    var subCbsSelected = filterCompSubCBsSurviving(filterClass);  // apply facet filters for all columns except this one.
    if (subCbsSelected.length === 0) {
	updated = true;
    }

    if (allSubCBs.length === subCbsSelected.length) {
        $(multiSelect).children('option.excluded').removeClass('excluded');
	updated = true;
    }


    // Walk through all selected subCBs to get other related tags
    var possibleSelections  = {};    // empty object acting as hash for selected tracks
    var possibleSelectionsX = {};    //  for non-selected tracks, used for not selected but choosable facet values.

    hiddenViewClasses=matViewClasses('hidden');

    $( subCbsSelected ).each( function (i)  {
	var possibleClasses = $( this ).attr("class").split(" ");
	// is subtrack selected and viz?
	if (this.checked && isFauxDisabled(this,true) === false) {
	    filterCountFacets(possibleClasses, hiddenViewClasses, possibleSelections);
        } else {
	    filterCountFacets(possibleClasses, hiddenViewClasses, possibleSelectionsX);
	}
    });

    // Walk through all options in this filterBox to set excluded class
    var opts = $(multiSelect).children("option");
    for (var ix = 1;ix < opts.length;ix++) { // All is always allowed
	var count = possibleSelections[opts[ix].value];
	if (count === undefined) {
	    count = possibleSelectionsX[opts[ix].value];
	    if (count === undefined) {
		count = 0;
	    }
	}
	opts[ix].facetCount = count;
	if (count === 0) {
	    if ($(opts[ix]).hasClass('excluded') === false) {
		$(opts[ix]).addClass('excluded');
		updated = true;
	    }
	} else {
	    if ($(opts[ix]).hasClass('excluded')) {
		$(opts[ix]).removeClass('excluded');
		updated = true;
	    }
	}
    }

    return updated;
}

function filterCompositeClasses(wantSelected)
{   // returns an array of classes from the dim ABC filterComp classes:
    // converts "matCB abc rep1"[]s to "rep1","rep2"
    var classes = [];
    var abcFBs = $("select.filterComp");
    if (abcFBs.length > 0) {
        $(abcFBs).each( function(i) {
            // Need to walk through list of options
            var ix=0;
            var allAreSelected = false;
            if (this.options[ix].value === "All") {
                allAreSelected = this.options[ix].selected;
                ix++;
            }
            for (; ix<this.length; ix++) {
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
{ // Opens multiselect whenever it gets focus  // MAY BE DEAD CODE - unused
    if ($(obj).attr('size') !== sizeWhenOpen) {
    $(obj).children("option").show();
    $(obj).attr('size',sizeWhenOpen);
    }
}

function multiSelectClick(obj,sizeWhenOpen)
{ // Opens multiselect whenever it is clicked  // MAY BE DEAD CODE - unused
    if ($(obj).attr('size') !== sizeWhenOpen) {
        multiSelectFocus(obj,sizeWhenOpen);
        return false;
    } else if ($(obj).attr('selectedIndex') === 0)
        $(obj).attr('size',1);
return true;
}

function navigationLinksSetup()
{ // Navigation links let you jump to places in the document
  // If they exist, then they need to be well placed to fit window dimensions

    // Put navigation links in top corner
    var navDown = $("#navDown");
    if (navDown && navDown.length > 0) {
        navDown = navDown[0];
        var winWidth = ($(window).width() - 20) + "px"; // Room for borders
        $('.windowSize').css({maxWidth:winWidth,width:winWidth});
        var sectTtl = $("#sectTtl");
        if (sectTtl && sectTtl.length > 0) {
            $(sectTtl).css({clear: 'none'});
            $(sectTtl).prepend($(navDown));
        }
        $(navDown).css({'float':'right', 'font-weight':'normal','font-size':'medium'});
        $(navDown).show();
    }

    // Decide if top links are needed
    var navUp = $('span.navUp');
    if (navUp && navUp.length > 0) {
        $(navUp).each(function(i) {
            var offset = $(this).parent().offset();
            if (offset.top > ($(window).height()*(2/3))) {
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
        waitOnFunction( sortTable._sortOnButtonPress, anchor, tagId);
    }
    return false;  // called by link so return false means don't try to go anywhere
}

function fauxDisable(obj,disable,title)
{// Makes an obj appear disabled, but it isn't
 //  span.disabled & input.disabled is opacity 0.5
 //   div.disabled is border-color: gray; color: gray;
    if ($(obj).hasClass('subCB') === false || typeof(subCfg) === "object") { // subCfg.js ?
        if (disable) {
            if (theClient.isIePre11())
                $(obj).css('opacity', '0.5');
            $(obj).addClass('disabled');
        } else {
            // For some reason IE<9 accepts direct change but isn't happy with simply adding class!
            if (theClient.isIePre11())
                $(obj).css('opacity', '1'); 
            $(obj).removeClass('disabled');
        }
    } else {
        obj.disabled = disable;
    }
    if (arguments.length > 2)
        $(obj).attr("title",title);
}

function isFauxDisabled(obj,orReallyDisabled)
{// Is object [faux] disabled?
    if (orReallyDisabled && obj.disabled)
        return true;

    return ($(obj).hasClass('disabled'));
}

  ////////////////////
 //// superTrack ////
////////////////////
var superT = {

    submitAndLink: function (obj)
    {
        var thisForm=$(obj).parents('form');
        if (thisForm && $(thisForm).length === 1) {
           thisForm = thisForm[0];
           $(thisForm).attr('action',obj.href); // just attach the straight href
           $(thisForm).trigger("submit");
           return false;  // should not get here!
        }
        return true;
    },

    topVis: function (show)
    {
        var superSel = $('select.visDD');
        if (superSel && superSel.length === 1) {
            superSel = superSel[0];
            if (show) {
                $(superSel).addClass('normalText');
                $(superSel).prop('selectedIndex',1);
                $(superSel).removeClass('hiddenText');
            } else {
                $(superSel).prop('selectedIndex',0);
                $(superSel).removeClass('normalText');
                $(superSel).addClass('hiddenText');
            }
        }
    },

    plusMinus: function (check)
    {
        $("input:checkbox").each(function (i) {
            $(this).prop("checked",check);
            superT.childChecked(this,1);
            if (!check) // all are unchecked so we can hide this whole thing.
                superT.topVis(check);
        });
    },

    childChecked: function (cb,defaultVis)
    {
        var name = cb.id;
        name = name.substring(0, name.length - "_check".length);
        var sel = $('select[name="' + name + '"]');
        if (sel && sel.length === 1) {
            sel = sel[0];
            var selIx = sel.selectedIndex;
            if (cb.checked && selIx === 0) {
                // What can be done to more intelligently default this?
                // All to dense?  Probably the best idea
                // When first rendering page?  Then how to save?
                // Logic is: from [+][-] then dense;  from cb, then full
                if (defaultVis === undefined || defaultVis === null)
                    defaultVis = (sel.options.length - 1); // full
                superT.selChanged(sel,defaultVis);
            } else if (!(cb.checked) && selIx !== 0) {
                superT.selChanged(sel,0);
            }
        }
    },

    selChanged: function(sel,val)
    {
        var selIx = val;
        if (val === undefined || val === null) // onchange event
            selIx = sel.selectedIndex;
        else // called from childChecked() so set value
            $(sel).prop('selectedIndex',val);

        if (selIx === 0) {
            $(sel).removeClass('normalText');
            $(sel).addClass('hiddenText');
        } else {
            $(sel).removeClass('hiddenText');
            $(sel).addClass('normalText');
            superT.topVis(true);
        }
        if (val === undefined || val === null) { // onchange event only
            var cb = $('input#'+sel.name+'_check');
            if (cb && cb.length === 1) {
                cb = cb[0];
                $(cb).prop('checked', (selIx > 0));
            }
        }
    }
};

var mat = { // Beginings of matrix object

    matrix: undefined,
    dimensions: 0,
    cellInFocus: undefined,

    cellHover: function (cell,on)
    {
        if (on) {
            if (mat.cellInFocus)
                mat.clearGhostHilites();  // Necessary to clear ghosts
            mat.cellInFocus = cell;
        } else
            mat.cellInFocus = undefined;

        var classList = $( cell ).attr("class").split(" ");
        classList = aryRemove(classList,["matCell"]);
        var color = (on ? "#FEF3CC" : "");// setting to "" removes the hilite ("#FCECC0" is LEVEL3)
        for (var ix=0;ix < classList.length;ix++) {
            if (classList[ix] === 'all')
                continue;
            if (ix === 0) {
                $(".matCell."+classList[ix]).css({backgroundColor: color });
            } else {
                $(cell).closest('tr').css({backgroundColor: color }); // faster?
            }
        }
        if (on && cell.title.length === 0) {
            for (var cIx=0;cIx < classList.length;cIx++) {
                if (classList[cIx] === 'all') { // on a label already
                    cell.title = "";
                    break;
                }
                if (cell.title.length > 0)
                    cell.title += " and ";
                cell.title += $("th."+classList[cIx]).first().text();
            }
        }
    },

    clearGhostHilites: function ()
    {
        $('.matCell').css({backgroundColor:""});
        $(mat.matrix).find('tr').css({backgroundColor:""});
    },

    resizeAngleLabels: function ()
    {   // Sets the height on the angled matrix labels
        var longest = "";
        var up45 = $('div.up45');
        $(up45).each(function (i) {
            if (longest.length < $(this).text().length)
                longest = $(this).text();
        });
        if (longest.length > 5) {
            $(mat.matrix).append("<span id='noShow' style='color:#FFF9D2;'>"+longest+"</span>");
            var noShow = $('span#noShow');
            var newHeight = ($(noShow).width() * 0.9) + 10;
            if (newHeight < 20)
                newHeight = longest.length * 7;
            $(up45).first().parent('th').css('height',newHeight + 'px');
            $(up45).show();
            var dn45 = $('div.dn45');
            if (dn45 && dn45.length > 0) {
                $(dn45).first().parent('th').css('height',newHeight + 'px');
                $(dn45).show();
            }
            $(noShow).remove();
        }
    },

    init: function ()
    {
        mat.matrix = $('table.matrix');
        if (mat.matrix && mat.matrix.length === 1) {
            mat.resizeAngleLabels();
            if (theClient.isIePre11() === false) { // IE pre v11 can't handle the hover!
                var cells = $('td.matCell');
                if (cells && cells.length > 0) {
                    var classList = $( cells[0] ).attr("class").split(" ");
                    classList = aryRemove(classList,["matCell"]);
                    mat.dimensions = classList.length;
                    if (mat.dimensions > 1) { // No need unless this is a 2D matrix
                        $('.matCell').hover(
                            function (e) {mat.cellHover(this,true);},
                            function (e) {mat.cellHover(this,false);}
                        );
                        // blur doesn't work because the screen isn't repainted
                        $(mat.matrix).on("blur", mat.clearGhostHilites());
                        $(window).on('focus',function (e) {mat.clearGhostHilites();});
                    }
                }
            }
        }
    }
};

// The following js depends upon the jQuery library
$(document).ready(function()
{
    mat.init();

    if (normed($('table.subtracks'))) {
        matInitializeMatrix();

        // If divs with class 'subCfg' then initialize the subtrack cfg code
        // NOTE: must be before any ddcl setup
        if (typeof(subCfg) === "object"  // subCfg.js file included?
        && (normed($("div.subCfg")) || normed($("div.subVisDD")))) {
            subCfg.initialize();
        }
    }

    // Initialize sortable tables
    $('table.sortable').each(function (ix) {
        sortTable.initialize(this,true,true);
    });

    // Register tables with drag and drop
    $("table.tableWithDragAndDrop").each(function (ix) {
        if ($(this)[0].id !== "imgTbl") {
            tableDragAndDropRegister(this);
        }
    });

    // Put navigation links in top corner
    navigationLinksSetup();
});

function selectLinkChanges($changed, $affected, mapping) {
// When the $changed changes, examine the value of $affected to see if it
// needs to be tweaked according to mapping, which is an object of objects
// like this: { 'pickySetting' : { 'notOKValue': 'OKValue' } }
// If $changed changes to 'pickySetting', and $affected is 'notOKValue',
// then $affected is changed to 'OKValue'.  Any notOKValue for pickySetting
// is disabled.  If $changed doesn't have a pickySetting, then all options
// are enabled.
    var $affectedOptions = $affected.children('option');
    $changed.on("change", function () {
        var changedVal = $changed.val();
        var subst = mapping[changedVal];
        $affectedOptions.prop('disabled', false);
        if (subst) {
            var affectedVal = $affected.val();
            if (subst[affectedVal]) {
                $affected.val(subst[affectedVal]);
            }
            for (var notOK in subst) {
                /* jshint loopfunc: true */// function inside loop works and replacement is awkward.
                var $option = $affectedOptions.filter(
                    function() { return $(this).text() === notOK; }
                    );
                $option.attr('disabled','disabled');
            }
        }
    });
}

function multiWigSetupOnChange(track) {
    var $overlay = $('select[name="' + track + '.aggregate"]');
    var $winFunc = $('select[name="' + track + '.windowingFunction"]');
    if ($overlay && $winFunc) {
        selectLinkChanges($overlay, $winFunc, {
            'stacked': { 'mean+whiskers': 'mean' }
        });
    } else {
        $("#message").text('$ cant find my selectors for ' + track + '!');
    }
}

// toggle the visibility of advanced controls in the filters
function advancedSearchOnChange(controlName) {
        $(document.getElementById(controlName)).on("click", function() {
            // get the list of advanced controls 
            advancedControls = document.getElementsByClassName('advanced'); 

            var newStyle; 
            if ($(advancedControls).css('display') === 'none') {
                newStyle='display:visible';
                $(this).find('img').attr('src','../images/upBlue.png');
            } else {
                newStyle = 'display:none'; 
                $(this).find('img').attr('src','../images/downBlue.png');
            }

            for (var control in advancedControls ) 
                advancedControls[control].style = newStyle;
        });
}


var hlColor = '#aac6ff';
var prevHlColor;
var hlColorDefault = '#aac6ff';
function makeHighlightPicker(cartVar, parentEl, trackName, label, cartColor = hlColorDefault) {
/* Create an input with a color selection field, optionally append the resulting
 * html to parentEl, if parent is not null */
    /* Some helper function for keeping track of colors */
    let saveHlColor = function(hlColor, trackName) {
        hlColor = hlColor;
        if (typeof common !== "undefined" && common.track) {
            // regular hgTrackUi
            setCartVars([cartVar], [hlColor], null, true);
        } else if (trackName) {
            // hgTrackUi pop up
            cart.setVars([cartVar], [hlColor], null, false);
        } else {
            // hgTracks dragSelect, uses different cart variable
            cart.setVars(["prevHlColor"], [hlColor], null, false);
        }
        prevHlColor = hlColor;
        return hlColor;
    };

    let loadHlColor = function() {
        // load hlColor from prevHlColor in the cart, or use default color, set and return it
        // color is a 6-char hex string prefixed by #
        if (typeof prevHlColor !== "undefined" && prevHlColor.length > 0) {
            hlColor = prevHlColor;
        } else if (typeof cartHighlightColor !== "undefined" && cartHighlightColor.length > 0) {
            hlColor = cartHighlightColor;
        } else {
            hlColor = cartColor;
        }
        return hlColor;
    };

    let colorPickerContainer = document.createElement("div");
    colorPickerContainer.innerHTML = typeof label !== "undefined" && label.length > 0 ? label : "Highlight color: ";
    // display: inline means if there is an info icon it will show up in line with the color picker
    colorPickerContainer.style = "display: inline";
    let inpText = document.createElement("input");
    // special case the drag select highlight feature because it has special code:
    if (cartVar === "hlColor") {
        inpText.id = cartVar + "Input";
    } else {
        inpText.id = "colorPicker." + cartVar + "Input";
    }
    inpText.value = loadHlColor();
    inpText.type = "text";
    inpText.style = "width: 70px";
    // The actual color picker:
    let inpSpec = document.createElement("input");
    if (cartVar === "hlColor") {
        inpSpec.id = cartVar + "Picker";
    } else {
        inpSpec.id = "colorPicker." + cartVar + "Picker";
    }
    let inpResetLink  = document.createElement("a");
    inpResetLink.href = "#";
    inpResetLink.id = cartVar + "Reset";
    inpResetLink.textContent = "Reset";
    colorPickerContainer.appendChild(inpText);
    colorPickerContainer.append(" ");
    colorPickerContainer.appendChild(inpSpec);
    colorPickerContainer.append(" ");
    colorPickerContainer.appendChild(inpResetLink);

    if (typeof parentEl !== undefined) {
        parentEl.appendChild(colorPickerContainer);
    } else {
        alert("Must supply parentNode to append color picker");
        throw new Error();
    }
    let opt = {
        hideAfterPaletteSelect: true,
        color: $(inpSpec).val(),
        showPalette: true,
        showInput: true,
        showSelectionPalette: true,
        showInitial: true,
        preferredFormat: "hex",
        localStorageKey: "genomebrowser",
        change: function() {
            let color = $(inpSpec).spectrum("get");
            $(inpText).val(color);
            saveHlColor(color, trackName);
        },
    };
    $(inpSpec).spectrum(opt);

    // update the color picker if you change the input box
    $(inpText).on("change", function() {
        $(inpSpec).spectrum("set", $(inpText).val());
        saveHlColor($(inpText).val(), trackName);
    });
    // Restore the default on Reset link click
    $(inpResetLink).on("click", function() {
        let hlDefault = hlColorDefault;
        $(inpText).val(hlDefault);
        $(inpSpec).spectrum("set", hlDefault);
        saveHlColor(hlDefault, trackName);
    });
    $(inpSpec).spectrum("set", $(inpText).val());
}

function onClickShowAllSubtracks(ev) {
    /* user clicked the 'show all tracks' button */
    $(".subCB").prop("checked", true);
}
