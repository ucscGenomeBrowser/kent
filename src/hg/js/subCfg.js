
// subCfg the subtrack Configureation module (scm) for hgTrackUi
//
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
// 11) TESTED: hui.c #ifdef SUBTRACK_CFG should switch full functionality on/off
// 12) SOLVED: Make "disabled" subCB clickable!
// 13) SOLVED: PROBLEM is fauxDisabled.  SOLUTION is convert fauxDisabled to true disabled on form.submit()
//  -) When parent vis makes subs hidden, should they go to unchecked?   No, disabled!
//  -) Should user be able to click on disabled vis to check the CB?  No, not important.
//  -) Make vis changes "reshape" composite!  NOTE: Do we want to do this???   I am against this as too disruptive.  We may want to end reshaping in CGIs as well!
//  -) Non-configurable will need to show/change vis independently! (separate vis control but no wrench)
// - Verify all composites work (conservation and SNPs are likely failures)
// - Decide on a name (scm, subCfg, ? ) and then make a module file like ddcl.js.
// - Remove debug code when ready
// NOTE:
// Current implementation relies upon '.' delimiter in name and no '_-' in name.  Nothing breaks rule yet...

var scm = { // subtrack config module.
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
                var oldName = boolshad.id.replace(/\_\-/g,'.');   // sanitized id replaces '.' with '_-'
                $(obj).addClass('changed');
                $(boolshad).attr('name',oldName);
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
                if (obj.id && obj.id.length > 0 && obj.id != myName && !$(obj).hasClass('filterBy') && !$(obj).hasClass('filterComp'))
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
                classList = aryRemove(classList,["changed","disabled"]);
                return classList[classList.length - 1];
            }
            warn("Can't find containing div.subCfg of child '"+$(childObj).attr('id')+"'.");
            return undefined;
        }
        var classList = $( cfg ).attr("class").split(" ");
        classList = aryRemove(classList,["subCfg","filled"]);
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
                classList = aryRemove(classList,["viewDD","normalText","changed"]);
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
            classList = aryRemove(classList,["viewDD","normalText","changed"]);
            if (classList.length != 1) {
                warn("Unexpected view vis class list:"+classList);
                return [];
            }
            return $(subVis).filter('.' + classList[0]);
        }
    },

    checkOneSubtrack: function (subCb,check,enable)
    { // Handle a single check of a single subCb
      // called by changing subVis to/from 'hide'
        subCb.checked = check;
        if (enable)
            fauxDisable(subCb,false,"");
        else
            fauxDisable(subCb,true, "View is hidden");
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
        classList = aryRemove(classList,["disabled"]);
        var view = classList[classList.length - 1]; // This relies on view being the last class!!!
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
                $(wrench).removeClass('disabled');
            else
                $(wrench).addClass('disabled');
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
            if ($(wrench).hasClass('disabled'))
                return;
            // Don't allow if this composite is not enabled!
            // find the cb
            var tr = $(cfg).parents('tr').first();
            var subCb = normed($(tr).find("#"+subtrack+"_sel"));
            if (subCb == undefined) {
                warn("Can't find subCB for "+subtrack);
                return false;
            }
            //if (subCb.disabled == true) // || subCb.checked == false)
            if (isFauxDisabled(subCb,true))
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

        $(subCb).bind('change',function (e) { // Not 'click' for a reason: other code will trigger 'change'
            scm.markChange(subCb);            // while 'click' code is exclusive for user click.
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
        var viewObjs = $(tr).find('input,select')
        var tempHiddens = $(viewObjs).filter(".filterBy,.filterComp").filter(':hidden');
        viewObjs = $(viewObjs).not(":hidden");
        viewObjs = $(viewObjs).add(tempHiddens);
        if (viewObjs.length > 0) {
            $(viewObjs).each(function (i) {
                scm.unnameIt(this,true);
                $(this).bind('change',function (e) {
                    scm.markChange(this);
                    scm.propagateSetting(this);
                });
            });
        }

        // Now vis control
        var viewVis = normed($("select[name='"+scm.compositeId+"\\."+viewId+"\\.vis']"));
        if (viewVis == undefined) {
            warn('DEBUG: Did not find visibility control for view: ' + viewId);
            return;
        }
        scm.unnameIt(viewVis,true);
        $(viewVis).bind('change',function (e) {
            scm.markChange(viewVis);
            scm.propagateVis(viewVis,viewId);
        });
    },

    initialize: function ()
    { // unnames all composite controls and then all view controls
      // names will be added back in onchange events
        // mySelf = this; // There is no need for a "mySelf" unless this object is being instantiated.

        var compVis = $('.visDD');
        if (compVis == undefined || compVis.length < 1) {
            warn('DEBUG: Did not find visibility control for composite.');
            return;
        }
        if (compVis.length > 1) {
            warn('DEBUG: Multiple visibility controls for composite???');
            return;
        }

        scm.compositeId = $(compVis).attr('name');
        scm.visIndependent = ($('.subVisDD').length > 0);  // Can subtracks have their own vis?

        // Unname and set up vis propagation
        compVis = compVis[0];
        scm.unnameIt(compVis,true);
        $(compVis).bind('change',function (e) {
            scm.markChange(compVis);
            scm.propagateVis(compVis,undefined);
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
            classList = aryRemove(classList,["viewDD","normalText","changed"]);
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
        var compObjs = $('select,input').filter("[name^='"+scm.compositeId+"\\.'],[name^='"+scm.compositeId+"_']").not(".allOrOnly");
        var tempHiddens = $(compObjs).filter(".filterBy,.filterComp").filter(':hidden');
        compObjs = $(compObjs).not(":hidden");
        compObjs = $(compObjs).add(tempHiddens);
        if (compObjs.length > 0) {
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
                    scm.markChange(this);
                    scm.propagateSetting(this);
                });
            });
        }

        // Because of fauxDisabled subCBs, it is necessary to truly disable them before submitting.
        $("FORM").submit(function (i) {
            $('input.subCB.changed.disabled').attr('disabled',true);
        });
    }
};

