// subCfg the subtrack Configuration module (scm) for hgTrackUi
//
// This module is for subtrack level config embedded dialogs in hgTrackUi.
// Subtrack config dialogs are embedded in the subtrack table and get populated when first
// opened.  Composite and view level controls (parents) when updated override related
// subtrack controls (children).  Subtrack controls, when updated overide parent controls
// for the one subtrack.  Controls wil get class 'changed' added when changes are made.
// When the form is submitted, all controls not marked as "changed" will be unnamed and will
// therefore not make it into the cart.

// Definitions as used here:
// obj: an input or select style html control which may be marked as "changed"
//   parentObj: composite or view level obj which has subtrack level childObjs associated
//   childObj: subtrack level obj that has composite and or view level parentObjs
// cfg: subtrack level embedded dialog which can be opened or closed (hidden) and isn't
//      populated till first opened.  Can also be a viewCfg and maybe a compositeCfg
// populate: act of filling a subtrack cfg with controls
// fauxVis: fake control for subtrack visDD, which will be replaced with true vis when clicked.
//
// NOTE: there is code inside hgTracks and cart.c that is removing subtrack values in the cart
//    hgTracks.c::parentChildCartCleanup() uses oldVars and calls cart.c::cartTdbTreeCleanupOverrides().
//    It seems to have a long-existing bug such that if you make two changes to the view level at once before submitting,
//    the cart ends up with incorrect state as seen by reloading in hgTrackUi.
//    Making one change to view and one change to subtrack and then submitting also causes the problem.
//    If you only make one change at a time to an element and submit then it works.
//
// NOTE:
// Current implementation relies upon '.' delimiter in name and no '_-' in name.
//   Nothing breaks rule yet...

// Don't complain about line break before '||' etc:
/* jshint -W014 */

var subCfg = { // subtrack config module.
    //mySelf: null, // There is no need for a "mySelf" unless this object is being instantiated.

    // There is one instance and these vars are page wide
    compositeName: undefined,
    canPack: true,  // if composite vis is only hide,dense,full, then all children also restricted.
    visIndependent: false,
    viewTags: [],

    markChange: function (eventObj, obj)
    { // Marks a control as having been changed by the user.  Naming will send value to cart.
      // Note this is often called directly as the onchange event function
        if (!obj || $.type(obj) === "string")
            obj = this;

        $(obj).addClass('changed');

        // if the user checked a child checkbox that used to be unchecked, and the parent composite is on hide,
        // then set the parent composite to pack
        var compEl = $("[name='"+this.compositeName+"'");
        if (compEl.length!==0 && obj.type==="checkbox" && obj.checked && compEl.val()==="hide")
            compEl.val("pack");

        // if the user unchecked a child checkbox that used to be checked, and the parent composite is on pack,
        // and no more child is checked, then set the parent composite to hide.
        var subCfgs = $(".subCB:checked");
        if (subCfgs.length===0)
            compEl.val("hide");
        
        // checkboxes have hidden boolshads which should be marked when unchecked
        if (obj.type === "checkbox") {
            var boolshad = normed($("input.cbShadow#boolshad\\."+obj.id));
            if (boolshad) {
                $(boolshad).addClass('changed');
	    } else {
		boolshad = normed($("input.cbShadow[name='boolshad\\." + obj.name+"']"));
		if (boolshad) {
		    $(boolshad).addClass('changed');
		}
            }
        }
    },

    clearChange: function (obj)
    { // Mark as unchanged
        $(obj).removeClass('changed');

        // checkboxes have hidden boolshads which should be cleared in tandem
        if (obj.type === "checkbox") {
            var boolshad = normed($("input.cbShadow#boolshad\\."+obj.id));
            if (boolshad) {
                $(boolshad).removeClass('changed');
	    } else {
		boolshad = normed($("input.cbShadow[name='boolshad\\." + obj.name+"']"));
		if (boolshad) {
		    $(boolshad).removeClass('changed');
		}
            }
        }
    },

    hasChanged: function (obj)
    { // Is this object updated (value changed by a user)?
        return $(obj).hasClass('changed');
    },

    compositeCfgFind: function ()
    { // returns the cfg container for the composite controls
        // TODO: write, create a composite cfg div!
    },

    compositeObjFind: function (suffix)
    { // returns the composite level control for this suffix
        //var compCfg = subCfg.compositeCfgFind();  // TODO: when there is a composite cfg div!
        var compObj;
        if (suffix && suffix.length > 0) {
            compObj = normed($("[name='"+subCfg.compositeName + "\\." + suffix+"']"));
            if (!compObj) {
                compObj = normed($("[name='"+subCfg.compositeName + "_" + suffix+"']"));
            }
        } else {
            compObj = normed($("[name='"+subCfg.compositeName+"']"));
        }
        return compObj;
    },

    viewTagFind: function (childObj)
    { // returns the name of the view that is a parent of this subtrack control
        var classList = null;
        var cfg = normed($(childObj).parents('div.subCfg'));
        if (!cfg) {
            // Child obj could be vis itself...
            var subCb = normed($(childObj).closest('tr').find('input.subCB'));
            if (subCb) {
                classList = $(subCb).attr("class").split(" ");
                classList = aryRemove(classList,["changed","disabled"]);
                return classList[classList.length - 1];
            }
            // could be vis outside of cfg div
            warn("DEBUG: Can't find containing div.subCfg of child '"+childObj.name+"'.");
            return undefined;
        }
        classList = $( cfg ).attr("class").split(" ");
        classList = aryRemove(classList,["subCfg","filled"]);
        if (classList.length === 0) {
            warn("DEBUG: Subtrack cfg div does not have view class for child '"+childObj.name+"'.");
            return undefined;
        } else if (classList.length > 1) {
            warn("DEBUG: Subtrack cfg div for '"+childObj.name+"' has unexpected class: "+classList);
            return undefined;
        }
        if (classList[0] === 'noView') // valid case
            return undefined;
        return classList[0];
    },

    viewCfgFind: function (viewTag)
    { // returns the cfg container for a given view
        var viewCfg = normed($('tr#tr_cfg_'+viewTag));
        return viewCfg;
    },

    viewObjFind: function (viewTag,suffix)
    { // returns the control belonging to this view and suffix
        var viewObj;
        if (suffix && suffix.length > 0) {
            var viewCfg = subCfg.viewCfgFind(viewTag);
            viewObj = normed($(viewCfg).find("[name$='\\."+suffix+"']"));
            if (!viewObj)
                viewObj = normed($(viewCfg).find("[name$='_"+suffix+"']"));
        } else
            viewObj = normed($("select.viewDD."+viewTag));

        return viewObj;
    },

    childObjsFind: function (viewTag,suffix)
    { // returns an array of objs for this view and suffix
      // Assumes composite wide if viewTag is not provided
      // Assumes vis if suffix is not provided

        var childCfgs = null;
        if (viewTag && viewTag.length > 0) {
            childCfgs = $('div.subCfg.filled.'+viewTag);
        } else {
            childCfgs = $('div.subCfg.filled');
        }
        if (!childCfgs)
            return [];

        var childObjs = [];
        if (suffix && suffix.length > 0)
            childObjs = $(childCfgs).find('select,input').filter("[name$='\\."+suffix+"']");
        else
            childObjs = $(childCfgs).find('select.visDD');

        if (!childObjs)
            return [];

        return childObjs;
    },

    objSuffixGet: function (obj)
    { // returns the identifying suffix of a control.  obj can be parent or child
        //var nameParts = $(obj).attr('id').split('.');
        var nameParts = obj.name.split('.');
        if (!nameParts || nameParts.length === 0) {
            warn("DEBUG: Can't resolve name for '"+obj.name+"'.");
            return undefined;
        }
        if (nameParts.length < 2)
            return undefined;

        nameParts.shift();
        if (subCfg.viewTags.length > 0 && nameParts.length > 1)
            nameParts.shift();

        return nameParts.join('.');
    },

    childCfgFind: function (childObj)
    { // returns the cfg wrapper for a child vis object
        var childCfg = normed($(childObj).parents('div.subCfg.filled'));
        if (!childCfg)
            warn("DEBUG: Can't find childCfg for "+childObj.name);
        return childCfg;
    },

    subCbFind: function (childObj)
    { // returns the subCB for a child vis object
        // Look directly first
        var subCb = normed($(childObj).closest('tr').find('input.subCB'));
        if (subCb)
            return subCb;

        warn("DEBUG: Failed to find subCb for "+childObj.name);
        var childCfg = subCfg.childCfgFind(childObj);
        if (!childCfg)
            return undefined;

        var tr = $(childCfg).parents('tr').first();
        var subtrack = childCfg.id.substring(8); // 'div_cfg_'.length
        subCb = normed($(tr).find("input[name='"+subtrack+"_sel']"));
        if (!subCb)
            warn("DEBUG: Can't find subCB for subtrack: "+subtrack);
        return subCb;
    },

    parentsFind: function (childObj)
    { // returns array of composite and/or view level parent controls
        var myParents = [];
        var suffix = subCfg.objSuffixGet(childObj);
        //var notRadio = (childObj.type.indexOf("radio") !== 0);
        var notRadio = (childObj.type !== "radio");

        // find view name
        var viewTag = subCfg.viewTagFind(childObj);
        if (viewTag && viewTag.length > 0) {
            var viewObj = subCfg.viewObjFind(viewTag,suffix);
            if (viewObj) {
                if (notRadio)
                    myParents[0] = viewObj;
                else {
                    $(viewObj).each(function (i) { // radios will have more than one
                        if (childObj.value === this.value) {
                            myParents[0] = this;
                            return false; // breaks each, but not parentsFind
                        }
                    });
                }
            }
        }

        var compObj = subCfg.compositeObjFind(suffix);
        if (compObj) {
            if (notRadio)
                myParents[myParents.length] = compObj;
            else {
                $(compObj).each(function (i) { // radios will have more than one
                    if (childObj.value === this.value) {
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
        var suffix = subCfg.objSuffixGet(parentObj);
        isVis = (suffix && suffix === 'vis');  // vis inside of subCfg

        var viewTag = null;
        if (isVis) { // This is a view control

            isComposite = (!suffix || suffix.length === 0);
            suffix = undefined;
            if (!isComposite) {
                var classList = $( parentObj ).attr("class").split(" ");
                classList = aryRemove(classList,["viewDD","normalText","changed"]);
                if (classList.length !== 1) {
                    warn("DEBUG: Unexpected view vis class list:"+classList);
                    return [];
                }
                viewTag = classList[0];
            }
        } else { // normal obj

            var viewCfg = normed($(parentObj).parents("tr[id^='tr_cfg_']"));
            isComposite = (!viewCfg); // is composite
            if (!isComposite) {
                viewTag = viewCfg.id.substring(7); // 'tr_cfg_'.length
            }
        }

        if (isComposite) {

            // There may be views
            if (subCfg.viewTags.length > 0) {
                var allChildren = [];
                for (var ix = 0;ix < subCfg.viewTags.length;ix++) {
                    viewTag = subCfg.viewTags[ix];
                    // Get any view objs first
                    var viewObj = subCfg.viewObjFind(viewTag,suffix);
                    if (viewObj)
                        allChildren[allChildren.length] = viewObj;
                    // Now get children
                    var viewChildren = subCfg.childObjsFind(viewTag,suffix);
                    if (viewChildren.length > 0) {
                        if (allChildren.length === 0)
                            allChildren = viewChildren;
                        else
                            allChildren = jQuery.merge( allChildren, viewChildren );
                    }
                }
                return allChildren;
            } else { // if no views then just get them all
                return subCfg.childObjsFind(undefined,suffix);
            }

        } else {
            return subCfg.childObjsFind(viewTag,suffix);
        }
    },

    visChildrenFind: function (parentObj)
    { // returns array of all currently faux and populated vis child controls
      // (which are not in subCfg div).  parentObj could be composite level or view level

        var subVis = $('.subVisDD');  // select:vis or faux:div
        if ($(parentObj).hasClass('visDD')) // composite.  views have .viewDD and sub .subVisDD
            return subVis;

        var classList = $( parentObj ).attr("class").split(" ");
        classList = aryRemove(classList,["viewDD","normalText","changed"]);
        if (classList.length !== 1) {
            warn("DEBUG: Unexpected view vis class list:"+classList);
            return [];
        }
        return $(subVis).filter('.' + classList[0]);
    },

    checkOneSubtrack: function (subCb,check,enable)
    { // Handle a single check of a single subCb
      // called by changing subVis to/from 'hide'
        subCb.checked = check;
        if (enable)
            fauxDisable(subCb,false,"");
        else
            fauxDisable(subCb,true, "View is hidden");
        subCfg.markChange(null,subCb);
        subCfg.enableCfg(subCb,check);
        matSubCbClick(subCb); // needed to mark matCBs, shadow and show/hide
    },

    propagateSetting: function (parentObj)
    { // propagate composite/view level setting to subtrack children
        var children = subCfg.childrenFind(parentObj);
        var parentVal = null;
        if (parentObj.type === "checkbox") {
            var parentChecked = parentObj.checked;
            $(children).each(function (i) {
                if (this.type !== 'hidden')
                    this.checked = parentObj.checked;
                subCfg.clearChange(this);
            });
        } else if (parentObj.type === "radio") {
            parentVal = $(parentObj).val();
            $(children).each(function (i) {
                this.checked = ($(this).val() === parentVal);
                subCfg.clearChange(this);
            });
        } else {// selects and inputs are easy
            parentVal = $(parentObj).val();
            var updateDdcl = ($(parentObj).hasClass('filterBy'));// Should never be filterComp
            $(children).each(function (i) {
                $(this).val(parentVal);
                subCfg.clearChange(this);
                if (updateDdcl)
                    ddcl.onComplete(this);
            });
        }
    },

    propagateViewVis: function (viewObj,compositeVis)
    { // propagate vis from a view limiting with compositeVis
        var limitedVis = Math.min(compositeVis,viewObj.selectedIndex);
        var visText = 'hide';
        if (limitedVis === 1)
            visText = 'dense';
        else if (limitedVis === 2) {
            if (subCfg.canPack)
                visText = 'squish';
            else
                visText = 'full';
        } else if (limitedVis === 3)
            visText = 'pack';
        else if (limitedVis === 4)
            visText = 'full';

        var children = subCfg.visChildrenFind(viewObj);
        $(children).each(function (i) {
            if ($(this).hasClass('fauxInput')) {
                $(this).text(visText);
            } else {
                $(this).attr('selectedIndex',limitedVis);
                subCfg.clearChange(this);
            }
        });
    },

    propagateVis: function (parentObj,viewTag)
    { // propagate vis settings to subtrack children
        if (!viewTag || viewTag.length === 0) {
            // Walk through views and set with this
            var parentVis = parentObj.selectedIndex;
            if (subCfg.viewTags.length > 0) {
                for (var ix=0;ix<subCfg.viewTags.length;ix++) {
                    var viewObj = subCfg.viewObjFind(subCfg.viewTags[ix]);
                    if (viewObj)
                        subCfg.propagateViewVis(viewObj,parentVis);
                }
            } else { // No view so, simple

                var visText = 'hide';
                if (parentVis === 1)
                    visText = 'dense';
                else if (parentVis === 2) {
                    if (subCfg.canPack)
                        visText = 'squish';
                    else
                        visText = 'full';
                } else if (parentVis === 3)
                    visText = 'pack';
                else if (parentVis === 4)
                    visText = 'full';
                var children = subCfg.visChildrenFind(parentObj);

                var checkedCount = 0;
                $(children).each(function (i) {
                    // the checkbox is in the <td> right before the one of $(this)
                    if ($(this).parent().prev().find("[type='checkbox']")[0].checked)
                        checkedCount++;

                    // apply the visibility to the subtrack
                    if ($(this).hasClass('fauxInput')) {
                        $(this).text(visText);
                    } else {
                        $(this).attr('selectedIndex',parentVis);
                        subCfg.clearChange(this);
                    }
                });

                // when nothing is checked, the visibility has been applied but everything is still hidden. 
                // This is probably not what the user wanted, so in lack of a better idea, we select all checkboxes
                if (checkedCount === 0)
                    // check all checkboxes
                    $(".subCB[type='checkbox']").prop("checked", true);
            }
        } else {
            // First get composite vis to limit with
            var compObj = subCfg.compositeObjFind(undefined);
            if (!compObj) {
                warn('DEBUG: Could not find composite vis object!');
                return false;
            }
            subCfg.propagateViewVis(parentObj,compObj.selectedIndex);
        }
    },

    inheritSetting: function (childObj,force)
    { // update value if parents values override child values.
        var myParents = subCfg.parentsFind(childObj);
        if (!myParents || myParents.length === 0) {
            //warn('DEBUG: No parents were found for childObj: '+childObj.name); // Not a problem.
            return true;
        }
        var isVis = (!subCfg.objSuffixGet(childObj));
        if (isVis) {
            var subCb = subCfg.subCbFind(childObj);
            if (subCb) {
                var limitedVis = 9;
                if (myParents.length === 1 && (force || subCfg.hasChanged(myParents[0])))
                    limitedVis = myParents[0].selectedIndex;
                else if (myParents.length === 2) {
                    if (force || subCfg.hasChanged(myParents[0]) || subCfg.hasChanged(myParents[1]))
                        limitedVis = Math.min(myParents[0].selectedIndex,myParents[1].selectedIndex);
                }
                if (limitedVis < 9)
                    $(childObj).attr('selectedIndex',limitedVis);
           }
        } else {
            var count = 0;
            if (childObj.type === "checkbox") {
                $(myParents).each(function (i) {
                    if (force || subCfg.hasChanged(this)) {
                        childObj.checked = this.checked;
                        count++;
                    }
                });
            } else if (childObj.type === "radio") {
                $(myParents).each(function (i) {
                    if (force || subCfg.hasChanged(this)) {
                        childObj.checked = this.checked;
                        // parentObj is already "tuned" to the correct radio, so this works
                        count++;
                    }
                });
            } else {// selects and inputs are easy
                $(myParents).each(function (i) {
                    if (force || subCfg.hasChanged(this)) {
                        $(childObj).val($(this).val());
                        count++;
                    }
                });
            }
            if (count > 1) // if hasChanged() is working, there should never be more than one
                warn('DEBUG: Both composite and view are seen as updated!  ' +
                                                                    'Named update is not working.');
        }
    },

    currentCfg: undefined, // keep track of cfg while ajaxing, man
    currentSub: undefined, // keep track of subtrack while ajaxing, dude

    cfgFill: function (content, status)
    { // Finishes the population of a subtrack cfg.  Called by ajax return.

        var ix;
        var cfg = subCfg.currentCfg;
        subCfg.currentCfg = undefined;
        var cleanHtml = content;
        cleanHtml = stripJsFiles(cleanHtml,true);   // DEBUG msg with true
        cleanHtml = stripCssFiles(cleanHtml,true);  // DEBUG msg with true
	// Obsoleted by CSP2 nonce js? 
        //cleanHtml = stripJsEmbedded(cleanHtml,true);// DEBUG msg with true 
	var nonceJs = {};
	cleanHtml = stripCSPAndNonceJs(cleanHtml, false, nonceJs); // DEBUG msg with true

        if (subCfg.visIndependent) {
            ix = cleanHtml.indexOf('</SELECT>');
            if (ix > 0)
                cleanHtml = cleanHtml.substring(ix+'</SELECT>'.length);
            while(cleanHtml.length > 0) {
                ix = cleanHtml.search("<");
                cleanHtml = cleanHtml.substring(ix);
                ix = cleanHtml.search(/<BR\>/i);
                if (ix !== 0)
                    break; // Not found or not at start.
                else
                    cleanHtml = cleanHtml.substring(4); // skip past <BR> and continue
            }
        } else {
            ix = cleanHtml.indexOf('<B>Display&nbsp;mode:&nbsp;</B>');
            if (ix > 0)                            // Excludes vis!
                cleanHtml = cleanHtml.substring(ix+'<B>Display&nbsp;mode:&nbsp;</B>'.length);
        }
        ix = cleanHtml.indexOf('</FORM>'); // start of form already chipped off
        if (ix > 0)
            cleanHtml = cleanHtml.substring(0,ix - 1);

        cleanHtml = "<div class='blueBox' style='background-color:#FFF9D2; padding:0.5em 1em 1em; overflow: scroll'>"
                    + cleanHtml + "</div>";

        $(cfg).html(cleanHtml);

	appendNonceJsToPage(nonceJs);

        $(cfg).addClass('filled');
        var boxWithin = $(cfg).find('.blueBox');
        if (boxWithin.length > 1)
            $(boxWithin[1]).removeClass('blueBox');

        var subObjs = $(cfg).find('input,select').filter("[name]").not('[type="submit"]');
        if (subObjs.length === 0) {
            warn('DEBUG: Did not find controls for cfg: ' + cfg.id);
            return;
        }
        $(subObjs).each(function (i) {
            if (this.name) { // The filter("[name]") above didn't do it!
                if (this.type !== 'hidden') {
                    subCfg.inheritSetting(this,false); // updates any values that have been changed
                    var suffix = subCfg.objSuffixGet(this);                         // on this page
                    if (suffix && suffix.length > 0)
                        $(this).change( subCfg.markChange );
                    else
                        warn("DEBUG: couldn't find suffix for subtrack control: "+this.name);
                }
            }
        });
        // finally show
        $(cfg).show();
        // Tricks to get this in the size and position I want
        $(cfg).css({ position: 'absolute'});
        var maxWidth = $("[name='mainForm']")[0].clientWidth;
        var schemaLink = $(cfg).parents('td').first().next();
        var schemaLinkWidth = schemaLink !== null ? schemaLink[0].clientWidth : 0;
        var shiftLeft = -1;
        if (subCfg.visIndependent) {
            shiftLeft *= ($(cfg).position().left - 125);
            var subVis = normed($('div#' + subCfg.currentSub+'_faux'));
            if (subVis)
                subCfg.replaceWithVis(subVis,subCfg.currentSub,false);
        }
        else
            shiftLeft *= ($(cfg).position().left - 40);
        // width: set to the config box size - (but shiftLeft is negative so +) the left shift
        // margin-right: the negative number forces the schema links to stay near where they
        //     are when the sub-config isn't open
        $(cfg).css({ width: (maxWidth + shiftLeft) +'px',position: 'relative', left: shiftLeft + 'px', "margin-right": shiftLeft + 'px'});

        // Setting up filterBys must follow show because sizing requires visibility
        $(cfg).find('.filterBy').each( function(i) { // Should never be filterComp
            this.id = this.name.replace(/\./g,'_-'); // Must have id unique to page!
            ddcl.setup(this, 'noneIsAll');
        });
    },

    cfgPopulate: function (cfg,subtrack)
    { // Populates a subtrack cfg dialog via ajax and update from composite/view parents
        subCfg.currentCfg = cfg;
        subCfg.currentSub = subtrack;

        $.ajax({
            type: "GET",
            url: "../cgi-bin/hgTrackUi?ajax=1&g="+subtrack+"&hgsid="+getHgsid()+"&db="+getDb(),
            dataType: "html",
            trueSuccess: subCfg.cfgFill,
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
        classList = aryRemove(classList,["disabled"]);
        var view = classList[classList.length - 1]; // This relies on view being the last class!!!
        var selectHtml  = "<SELECT name='"+subtrack+"' class='normalText subVisDD "+view+"'";
            selectHtml += " style='width:70px;'>";
        var selected = $(obj).text();
        var visibilities = ['hide','dense','squish','pack','full'];
        if (subCfg.canPack === false)
            visibilities = ['hide','dense','full'];
        $(visibilities).each( function (ix) {
             selectHtml += "<OPTION"+(visibilities[ix] === selected ? " SELECTED":"")+">";
             selectHtml += visibilities[ix]+"</OPTION>";
        });
        selectHtml += "</SELECT>";
        $(obj).replaceWith(selectHtml);
        var newObj = $("select[name='"+subtrack+"']");
        if (open) {
            $(newObj).css({'zIndex':'2','vertical-align':'top'});
            // For some reason IE11 will hang if the sect is opened to start with!
            // This ungraceful fix avoids the hang, but a nicer solution would be apprciated!
            if (theClient.isIePost11() === false) { 
                $(newObj).attr('size',visibilities.length);
            }
            $(newObj).one('blur',function (e) {
                $(this).attr('size',1);
                $(this).unbind('click');
            });
            $(newObj).one('click',function (e) {
                $(this).attr('size',1);
                $(this).unbind('blur');
            });
            $(newObj).focus();
        }
        $(newObj).change(function (e) {
            if ($(this).attr('size') > 1) {
                $(this).attr('size',1);
                $(this).unbind('blur');
                $(this).unbind('click');
            }
            if (this.selectedIndex === 0) { // setting to hide so uncheck and disable.
                // Easiest is to uncheck subCB and reset vis
                //    so that the reverse action makes sense
                var subCb = normed($("input[name='" + this.name + "_sel']"));
                if (subCb) {
                    subCfg.checkOneSubtrack(subCb,false,true);
                    subCfg.inheritSetting(this,true);
                } else {
                    warn('DEBUG: Cant find subCB for ' + this.name);
                }
            } else {
                subCfg.markChange(e,this);
                // If made visible, be sure to make composite visible
                // But do NOT turn composite from hide to full, since it will turn on other subs
                // Just trigger a supertrack reshaping
                if (this.selectedIndex > 0) {
                    //exposeAll();
                    var visDD = normed($("select.visDD"));
                    if (visDD) {
                        if ($(visDD).hasClass('superChild'))
                            visTriggersHiddenSelect(visDD);
                    }
                }
            }
        });
    },

    enableCfg: function (subCb,setTo)
    { // Enables or disables subVis and wrench
        var tr = normed($(subCb).closest('tr'));
        if (!tr) {
            warn("DEBUG: subCfg.enableCfg() could not find TR for CB: "+subCb.name);
            return false;
        }
        var subFaux = normed($(tr).find('div.subVisDD'));
        if (subFaux) {
            if (setTo === true)
                $(subFaux).removeClass('disabled');
            else
                $(subFaux).addClass('disabled');
        } else {
            var subVis = normed($(tr).find('select.subVisDD'));
            if (subVis) {
                $(subVis).attr('disabled',!setTo);
            }
        }
        var wrench = normed($(tr).find('span.clickable'));
        if (wrench) {
            if (setTo === true)
                $(wrench).removeClass('disabled');
            else {
                $(wrench).addClass('disabled');
                var cfg = normed($(tr).find('div.subCfg'));
                if (cfg)
                    $(cfg).hide();
            }
        }
    },

    cfgToggle: function (wrench,subtrack)
    { // Opens/closes subtrack cfg dialog, populating if empty
        var cfg = normed($(document.getElementById("div_cfg_"+subtrack)));
        if (!cfg) {
            warn("DEBUG: Can't find div_cfg_"+subtrack);
            return false;
        }

        if ($(cfg).css('display') === 'none') {
            if ($(wrench).hasClass('disabled')) {
                return;
            }
            // Don't allow if this composite is not enabled!
            // find the cb
            var tr = $(cfg).parents('tr').first();
            var subCb = normed($(tr).find("input[name='"+subtrack+"_sel']"));
            if (!subCb) {
                warn("DEBUG: Can't find subCB for "+subtrack);
                return false;
            }
            //if (subCb.disabled === true) // || subCb.checked === false)
            if (isFauxDisabled(subCb,true))
                return false;

            if (metadataIsVisible(subtrack))
                metadataShowHide(subtrack,"","");
            if ($(cfg).hasClass('filled'))
                $(cfg).show();
            else
                waitOnFunction( subCfg.cfgPopulate, cfg, subtrack );
        } else
            $(cfg).hide();
        return false; // called by link!
    },

    viewInit: function (viewTag)
    { // sets up view controls for propagation
        var tr = normed($('tr#tr_cfg_'+viewTag));
        if (tr) {
            // iterate through all matching controls and setup for propgation and change flagging
            var viewObjs = $(tr).find('input,select');
            if (viewObjs.length > 0) {
                $(viewObjs).each(function (i) {
                    if (this.type !== 'hidden') {
                        $(this).bind('change',function (e) {
                            subCfg.markChange(e,this);
                            subCfg.propagateSetting(this);
                        });
                    }
                });
            }
        }

        // Now vis control
        var viewVis = normed($("select.viewDD."+viewTag));
        if (!viewVis) {
            warn('DEBUG: Did not find visibility control for view: ' + viewTag);
            return;
        }
        $(viewVis).bind('change',function (e) {
            subCfg.markChange(e,viewVis);
            subCfg.propagateVis(viewVis,viewTag);
        });
    },

    initialize: function ()
    { // sets up all composite controls and then all view controls
      // onchange gets set to mark controls as 'changed'.  Unchanged controls will
      // be disabled on 'submit'.  Disabled controls will not get to the cart!

        var compVis = $('.visDD');
        if (!compVis || compVis.length === 0) {
            warn('DEBUG: Did not find visibility control for composite.');
            return;
        }
        if (compVis.length > 1) {
            warn('DEBUG: Multiple visibility controls for composite???');
            return;
        }
        // Note that if composite vis has no pack then no child should have pack
        if ($(compVis).children('option').length < 5)
            subCfg.canPack = false; 

        subCfg.compositeName = $(compVis).attr('name');
        subCfg.visIndependent = ($('.subVisDD').length > 0);  // Can subtracks have their own vis?

        // Set up vis propagation and change flagging
        compVis = compVis[0];
        $(compVis).bind('change',function (e) {
            subCfg.markChange(e,compVis);
            subCfg.propagateVis(compVis,undefined);
        });

        // SubCBs will may enable/diable vis/wrench and will be flagged on change
        var subCbs = $('input.subCB');
        $(subCbs).change( function (e) {
            subCfg.enableCfg(this, (this.checked && !isFauxDisabled(this, true)));
            subCfg.markChange(e,this);
        });

        // iterate through views
        var viewVis = $('select.viewDD');
        $(viewVis).each(function (i) {
            var classList = $( this ).attr("class").split(" ");
            classList = aryRemove(classList,["viewDD","normalText","changed"]);
            if (classList.length === 0)
                warn('DEBUG: View classlist is missing view class.');
            else if (classList.length > 1)
                warn('DEBUG: View classlist contains unexpected classes:' + classList);
            else {
                subCfg.viewTags[i] = classList[0];
                subCfg.viewInit(subCfg.viewTags[i]);
            }
        });

        // Tricky for composite level controls.  Could wrap cfg controls in new div.
        // DO THIS AFTER Views
        // NOTE: excluding sortOrder and showCfg which are special cases we don't care about here
        // Excluding views and subCBs because a {composite}_{subtrack} naming scheme may be used
        var compObjs = $('select,input').filter("[name^='"+subCfg.compositeName+"\\.'],[name^='"+
                                                                        subCfg.compositeName+"_']");
        // viewDD, subCB already done. non-abc matCBs not needed, as they shouldn't go to the cart.!
        compObjs = $(compObjs).not(".viewDD,.subCB,.matCB:not(.abc)");
        if (compObjs.length > 0) {
            $(compObjs).each(function (i) {
                if (this.type !== 'hidden') {
                    // DEBUG -------------
                    //if (this.id && this.id.length > 0
                    //&& $(this).hasClass('filterBy') === false
                    //&& $(this).hasClass('filterComp') === false)
                    //    warn('DEBUG: Unexpected control with name ['+this.name + '], and id #'+ this.id);

                    $(this).change(function (e) {
                        subCfg.markChange(e,this);
                        subCfg.propagateSetting(this);
                    });
                }
            });
        }
	// Bugfix #8048: Tim suggested using a special class to handle subtrack settings
	// that appear alongside composite settings, as in ldUi (HapMap LD).
	var subsInComp = $('select.subtrackInCompositeUi,input.subtrackInCompositeUi');
	$(subsInComp).each( function(ix, el) {
				$(el).change(function(e) { subCfg.markChange(e, el); } ); });

        // Because of fauxDisabled subCBs, it is necessary to truly disable them before submitting.
        $("FORM").submit(function (i) {          // shadows will go to cart as they should
            $('input.subCB.changed.disabled').attr('disabled',true);

            // Unchanged controls will be disabled to avoid sending to the cart
            $('select,input').filter("[name]").not(".allOrOnly").not('.changed').each(function (i) {
                if (this.type !== 'hidden' || $(this).hasClass('trPos')
                || $(this).hasClass('cbShadow') || $(this).hasClass('sortOrder')) {
                    // hiddens except priority and boolshad are all sent to the cart
                    // disable instead of unname because unname fills cart with a lot of garbage
                    this.disabled = true;                                            // (Linux/FF)!
                }
            });
        });
    }
};

