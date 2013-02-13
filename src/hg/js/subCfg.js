// subCfg the subtrack Configureation module (scm) for hgTrackUi
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

// Issues:
// - SOLVED: checkboxes: working with name = boolshad.{name}   FIXME: multishad? [no problem currently]
// - DECIDED: When parent vis makes subs hidden, should they go to unchecked?   No, disabled!
// - DECIDED: Should user be able to click on disabled vis to check the CB?  No, not important.
// - DECIDED: Make vis changes "reshape" composite!  NO, I am against this as too disruptive.  We may want to end reshaping in CGIs as well!
// - Verified all composites work (hg19 hg18 mm9 mm8 panTro3 galGal3 braFlo1 dm3 ce10 homPan20 sacCer3 hg19Patch5 tested so far)
// - Speed up massive composites!  HAIB TFBS SYDH TFBS
// - subVis DOES NOT set compVis to full (when compVis set to hide, but subVis already populated) OK because this slips towords reshaping.
// NOTE:
// Current implementation relies upon '.' delimiter in name and no '_-' in name.  Nothing breaks rule yet...
// Can remove "DEBUG" messages when fully QA'd.  They are asserts.

var subCfg = { // subtrack config module.
    //mySelf: null, // There is no need for a "mySelf" unless this object is being instantiated.

    // There is one instance and these vars are page wide
    compositeName: undefined,
    canPack: true,  // if composite vis is only hide,dense,full, then all children will also be restricted.
    visIndependent: false,
    viewTags: [],

    markChange: function (eventObj, obj)
    { // Marks a control as having been changed by the user.  Naming will send value to cart.
      // Note this is often called directly as the onchange event function
        if (obj == undefined || $.type(obj) === "string")
            obj = this;

        $(obj).addClass('changed');

        // checkboxes have hidden boolshads which should be marked when unchecked
        if(obj.type === "checkbox") {
            var boolshad = normed($("input.cbShadow[name='boolshad\\." + obj.name+"']"));
            if (boolshad != undefined) {
                $(boolshad).addClass('changed');
            }
        }
    },

    clearChange: function (obj)
    { // Mark as unchanged
        $(obj).removeClass('changed');

        // checkboxes have hidden boolshads which should be cleared in tandem
        if(obj.type === "checkbox") {
            var boolshad = normed($("input.cbShadow[name='boolshad\\." + obj.name+"']"));
            if (boolshad != undefined) {
                $(boolshad).removeClass('changed');
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
        if (suffix != undefined) {
            compObj = normed($("[name='"+subCfg.compositeName + "\\." + suffix+"']"));
            if (compObj === undefined) {
                compObj = normed($("[name='"+subCfg.compositeName + "_" + suffix+"']"));
            }
        } else {
            compObj = normed($("[name='"+subCfg.compositeName+"']"));
        }
        return compObj;
    },

    viewTagFind: function (childObj)
    { // returns the name of the view that is a parent of this subtrack control
        var cfg = normed($(childObj).parents('div.subCfg'));
        if (cfg == undefined) {
            // Child obj could be vis itself...
            var subCb = normed($(childObj).closest('tr').find('input.subCB'));
            if (subCb != undefined) {
                var classList = $(subCb).attr("class").split(" ");
                classList = aryRemove(classList,["changed","disabled"]);
                return classList[classList.length - 1];
            }
            // could be vis outside of cfg div
            warn("DEBUG: Can't find containing div.subCfg of child '"+childObj.name+"'.");
            return undefined;
        }
        var classList = $( cfg ).attr("class").split(" ");
        classList = aryRemove(classList,["subCfg","filled"]);
        if (classList.length == 0) {
            warn("DEBUG: Subtrack cfg div does not have view class for child '"+childObj.name+"'.");
            return undefined;
        } else if (classList.length > 1) {
            warn("DEBUG: Subtrack cfg div for '"+childObj.name+"' has unexpected class: "+classList);
            return undefined;
        }
        if (classList[0] == 'noView') // valid case
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
        if (suffix != undefined) {
            var viewCfg = subCfg.viewCfgFind(viewTag);
            viewObj = normed($(viewCfg).find("[name$='\\."+suffix+"']"));
            if (viewObj == undefined)
                viewObj = normed($(viewCfg).find("[name$='_"+suffix+"']"));
        } else
            viewObj = normed($("select.viewDD."+viewTag));

        return viewObj;
    },

    childObjsFind: function (viewTag,suffix)
    { // returns an array of objs for this view and suffix
      // Assumes composite wide if viewTag is not provided
      // Assumes vis if suffix is not provided

        if (viewTag != undefined) {
            var childCfgs = $('div.subCfg.filled.'+viewTag);
        } else {
            var childCfgs = $('div.subCfg.filled');
        }
        if (childCfgs == undefined)
            return [];

        var childObjs = [];
        if (suffix != undefined)
            childObjs = $(childCfgs).find('select,input').filter("[name$='\\."+suffix+"']");
        else
            childObjs = $(childCfgs).find('select.visDD');

        if (childObjs == undefined)
            return [];

        return childObjs;
    },

    objSuffixGet: function (obj)
    { // returns the identifying suffix of a control.  obj can be parent or child
        //var nameParts = $(obj).attr('id').split('.');
        var nameParts = obj.name.split('.');
        if (nameParts == undefined || nameParts.length == 0) {
            warn("DEBUG: Can't resolve name for '"+obj.name+"'.");
            return undefined;
        }
        if (nameParts.length < 2)
            return undefined;

        nameParts.shift();
        if (subCfg.viewTags.length > 0 && nameParts.length > 1) // FIXME: I expect more problems with this!
            nameParts.shift();

        return nameParts.join('.');
    },

    childCfgFind: function (childObj)
    { // returns the cfg wrapper for a child vis object
        var childCfg = normed($(childObj).parents('div.subCfg.filled'));
        if (childCfg == undefined)
            warn("DEBUG: Can't find childCfg for "+childObj.name);
        return childCfg;
    },

    subCbFind: function (childObj)
    { // returns the subCB for a child vis object
        // Look directly first
        var subCb = normed($(childObj).closest('tr').find('input.subCB'));
        if (subCb != undefined)
            return subCb;

        warn("DEBUG: Failed to find subCb for "+childObj.name);
        var childCfg = subCfg.childCfgFind(childObj);
        if (childCfg == undefined)
            return undefined;

        var tr = $(childCfg).parents('tr').first();
        var subtrack = childCfg.id.substring(8); // 'div_cfg_'.length
        var subCb = normed($(tr).find("input[name='"+subtrack+"_sel']"));
        if (subCb == undefined)
            warn("DEBUG: Can't find subCB for subtrack: "+subtrack);
        return subCb;
    },

    parentsFind: function (childObj)
    { // returns array of composite and/or view level parent controls
        var myParents = [];
        var suffix = subCfg.objSuffixGet(childObj);
        //var notRadio = (childObj.type.indexOf("radio") != 0);
        var notRadio = (childObj.type !== "radio");

        // find view name
        var viewTag = subCfg.viewTagFind(childObj);
        if (viewTag != undefined) {
            var viewObj = subCfg.viewObjFind(viewTag,suffix);
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

        var compObj = subCfg.compositeObjFind(suffix);
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
        var suffix = subCfg.objSuffixGet(parentObj);
        isVis = (suffix != undefined && suffix == 'vis');  // vis inside of subCfg

        var viewTag = undefined;
        if (isVis) { // This is a view control

            isComposite = (suffix == undefined);
            suffix = undefined;
            if (!isComposite) {
                var classList = $( parentObj ).attr("class").split(" ");
                classList = aryRemove(classList,["viewDD","normalText","changed"]);
                if (classList.length != 1) {
                    warn("DEBUG: Unexpected view vis class list:"+classList);
                    return [];
                }
                viewTag = classList[0];
            }
        } else { // normal obj

            var viewCfg = normed($(parentObj).parents("tr[id^='tr_cfg_']"));
            isComposite = (viewCfg == undefined); // is composite
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
                    if (viewObj != undefined)
                        allChildren[allChildren.length] = viewObj;
                    // Now get children
                    var viewChildren = subCfg.childObjsFind(viewTag,suffix);
                    if (viewChildren.length > 0) {
                        if (allChildren.length == 0)
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
    { // returns array of all currently faux and populated vis child controls (which are not in subCfg div)
      // parentObj could be composite level or view level

        var subVis = $('.subVisDD');  // select:vis or faux:div
        if ($(parentObj).hasClass('visDD')) // composite.  views have .viewDD and sub .subVisDD
            return subVis;

        var classList = $( parentObj ).attr("class").split(" ");
        classList = aryRemove(classList,["viewDD","normalText","changed"]);
        if (classList.length != 1) {
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
        if(parentObj.type === "checkbox") {
            var parentChecked = parentObj.checked;
            $(children).each(function (i) {
                if (this.type != 'hidden')
                    this.checked = parentObj.checked;
                subCfg.clearChange(this);
            });
        //} else if(parentObj.type.indexOf("radio") == 0) {
        } else if(parentObj.type === "radio") {
            var parentVal = $(parentObj).val();
            $(children).each(function (i) {
                this.checked = ($(this).val() == parentVal);
                subCfg.clearChange(this);
            });
        } else {// selects and inputs are easy
            var parentVal = $(parentObj).val();
            var updateDdcl = ($(parentObj).hasClass('filterBy'));// || $(parentObj).hasClass('filterComp'));
            $(children).each(function (i) {                      // Should never be filterComp
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
        if (limitedVis == 1)
            visText = 'dense';
        else if (limitedVis == 2) {
            if (subCfg.canPack)
                visText = 'squish';
            else
                visText = 'full';
        } else if (limitedVis == 3)
            visText = 'pack';
        else if (limitedVis == 4)
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
        if (viewTag == undefined) {
            // Walk through views and set with this
            var parentVis = parentObj.selectedIndex;
            if (subCfg.viewTags.length > 0) {
                for (var ix=0;ix<subCfg.viewTags.length;ix++) {
                    var viewObj = subCfg.viewObjFind(subCfg.viewTags[ix]);//,undefined);
                    if (viewObj != undefined)
                        subCfg.propagateViewVis(viewObj,parentVis);
                }
            } else { // No view so, simple

                var visText = 'hide';
                if (parentVis == 1)
                    visText = 'dense';
                else if (parentVis == 2) {
                    if (subCfg.canPack)
                        visText = 'squish';
                    else
                        visText = 'full';
                } else if (parentVis == 3)
                    visText = 'pack';
                else if (parentVis == 4)
                    visText = 'full';
                var children = subCfg.visChildrenFind(parentObj);
                $(children).each(function (i) {
                    if ($(this).hasClass('fauxInput')) {
                        $(this).text(visText);
                    } else {
                        $(this).attr('selectedIndex',parentVis);
                        subCfg.clearChange(this);
                    }
                });
            }
        } else {
            // First get composite vis to limit with
            var compObj = subCfg.compositeObjFind(undefined);
            if (compObj == undefined) {
                warn('DEBUG: Could not find composite vis object!');
                return false;
            }
            subCfg.propagateViewVis(parentObj,compObj.selectedIndex);
        }
    },

    inheritSetting: function (childObj,force)
    { // update value if parents values override child values.
        var myParents = subCfg.parentsFind(childObj);
        if (myParents == undefined || myParents.length < 1) {
            //warn('DEBUG: No parents were found for childObj: '+childObj.name); // Not really a problem.
            return true;
        }
        var isVis = (undefined == subCfg.objSuffixGet(childObj));
        if (isVis) {
            var subCb = subCfg.subCbFind(childObj);
            if (subCb != undefined) {
                var limitedVis = 9;
                if (myParents.length == 1 && (force || subCfg.hasChanged(myParents[0])))
                    limitedVis = myParents[0].selectedIndex;
                else if (myParents.length == 2) {
                    if (force || subCfg.hasChanged(myParents[0]) || subCfg.hasChanged(myParents[1]))
                        limitedVis = Math.min(myParents[0].selectedIndex,myParents[1].selectedIndex);
                }
                if (limitedVis < 9)
                    $(childObj).attr('selectedIndex',limitedVis);
           }
        } else {
            var count = 0;
            if(childObj.type === "checkbox") {
                $(myParents).each(function (i) {
                    if (force || subCfg.hasChanged(this)) {
                        childObj.checked = this.checked;
                        count++;
                    }
                });
            //} else if(childObj.type.indexOf("radio") == 0) {
            } else if(childObj.type === "radio") {
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
                warn('DEBUG: Both composite and view are seen as updated!  Named update is not working.');
        }
    },

    currentCfg: undefined, // keep track of cfg while ajaxing, man
    currentSub: undefined, // keep track of subtrack while ajaxing, dude

    cfgFill: function (content, status)
    { // Finishes the population of a subtrack cfg.  Called by ajax return.
        var cfg = subCfg.currentCfg;
        subCfg.currentCfg = undefined;
        var cleanHtml = content;
        cleanHtml = stripJsFiles(cleanHtml,true);   // DEBUG msg with true
        cleanHtml = stripCssFiles(cleanHtml,true);  // DEBUG msg with true
        cleanHtml = stripJsEmbedded(cleanHtml,true);// DEBUG msg with true
        if (subCfg.visIndependent) {
            var ix = cleanHtml.indexOf('</SELECT>');
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
                cleanHtml = cleanHtml.substring(ix+'<B>Display&nbsp;mode:&nbsp;</B>'.length); // Excludes vis!
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
            warn('DEBUG: Did not find controls for cfg: ' + cfg.id);
            return;
        }
        $(subObjs).each(function (i) {
            if (this.name != undefined) { // The filter("[name]") above didn't do it!
                if (this.type != 'hidden') {
                    subCfg.inheritSetting(this,false); // updates any values that have been changed on this page
                    var suffix = subCfg.objSuffixGet(this);
                    if (suffix != undefined)
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
        var myWidth = $(cfg).width();
        var shiftLeft = -1;
        if (subCfg.visIndependent) {
            shiftLeft *= ($(cfg).position().left - 125);
            var subVis = normed($('div#' + subCfg.currentSub+'_faux'));
            if (subVis != undefined)
                subCfg.replaceWithVis(subVis,subCfg.currentSub,false);
        }
        else
            shiftLeft *= ($(cfg).position().left - 40);
        $(cfg).css({ width: myWidth+'px',position: 'relative', left: shiftLeft + 'px' });

        // Setting up filterBys must follow show because sizing requires visibility
        $(cfg).find('.filterBy').each( function(i) { // Should never be filterComp
            this.id = this.name.replace(/\./g,'_-'); // Must have id unique to page!
            ddcl.setup(this, 'noneIsAll');
        });

        // Adjust cfg box size?
        // Especially needed for filterBys, but generic in case some other overrun occurs
        var curRight = $(cfg).offset().left + $(cfg).width();
        var calcRight = curRight;
        $(cfg).find(':visible').each( function(i) { // All visible, including labels, etc.
            var childRight = $(this).offset().left + $(this).width();
            if (calcRight < childRight)
                calcRight = childRight;
        });
        if (curRight < calcRight) {
            var cfgWidth = calcRight - $(cfg).offset().left + 16; // clip offset but add some for border
            $(cfg).css({ width: cfgWidth + 'px' });

            // Now containing tables may need adjustment
            var tables = $(cfg).parents('table');
            var maxWidth = 0;
            $(tables).each(function(i) {
                var myWidth = $(this).width();
                if (maxWidth < myWidth)
                    maxWidth = myWidth;
                else if (maxWidth > myWidth)
                    $(this).css('width',maxWidth + 'px');
            });
        }
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
        if (subCfg.canPack == false)
            visibilities = ['hide','dense','full'];
        $(visibilities).each( function (ix) {
             selectHtml += "<OPTION"+(visibilities[ix] == selected ? " SELECTED":"")+">";
             selectHtml += visibilities[ix]+"</OPTION>";
        });
        selectHtml += "</SELECT>";
        $(obj).replaceWith(selectHtml);
        var newObj = $("select[name='"+subtrack+"']");
        if (open) {
            $(newObj).css({'zIndex':'2','vertical-align':'top'});
            $(newObj).attr('size',visibilities.length);
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
            if (this.selectedIndex == 0) { // setting to hide so uncheck and disable.
                // Easiest is to uncheck subCB and reset vis
                //    so that the reverse action makes sense
                var subCb = normed($("input[name='" + this.name + "_sel']"));
                if (subCb != undefined) {
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
                    if (visDD != undefined) {
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
        if (tr == undefined) {
            warn("DEBUG: subCfg.enableCfg() could not find TR for CB: "+subCb.name);
            return false;
        }
        var subFaux = normed($(tr).find('div.subVisDD'));
        if (subFaux != undefined) {
            if (setTo == true)
                $(subFaux).removeClass('disabled');
            else
                $(subFaux).addClass('disabled');
        } else {
            var subVis = normed($(tr).find('select.subVisDD'));
            if (subVis != undefined) {
                $(subVis).attr('disabled',!setTo);
            }
        }
        var wrench = normed($(tr).find('span.clickable')); // TODO tighten this
        if (wrench != undefined) {
            if (setTo == true)
                $(wrench).removeClass('disabled');
            else {
                $(wrench).addClass('disabled');
                var cfg = normed($(tr).find('div.subCfg'));
                if (cfg != undefined)
                    $(cfg).hide();
            }
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
            var subCb = normed($(tr).find("input[name='"+subtrack+"_sel']"));
            if (subCb == undefined) {
                warn("DEBUG: Can't find subCB for "+subtrack);
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
                waitOnFunction( subCfg.cfgPopulate, cfg, subtrack );
        } else
            $(cfg).hide();
        return false; // called by link!
    },

    viewInit: function (viewTag)
    { // sets up view controls for propagation
        var tr = normed($('tr#tr_cfg_'+viewTag));
        if (tr != undefined) {
            // iterate through all matching controls and setup for propgation and change flagging
            var viewObjs = $(tr).find('input,select');
            if (viewObjs.length > 0) {
                $(viewObjs).each(function (i) {
                    if (this.type != 'hidden') {
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
        if (viewVis == undefined) {
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
        if (compVis == undefined || compVis.length < 1) {
            warn('DEBUG: Did not find visibility control for composite.');
            return;
        }
        if (compVis.length > 1) {
            warn('DEBUG: Multiple visibility controls for composite???');
            return;
        }
        if ($(compVis).children('option').length < 5)
            subCfg.canPack = false; // Note that if composite vis has no pack then no child should have pack

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
            if (classList.length == 0)
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
        // NOTE: excluding sortOrder and showCfg which are special cases we don't care about in subCfg
        // Excluding views and subCBs because a {composite}_{subtrack} naming scheme may be used
        var compObjs = $('select,input').filter("[name^='"+subCfg.compositeName+"\\.'],[name^='"+subCfg.compositeName+"_']");
        // viewDD, subCB already done. non-abc matCBs not needed, as they shouldn't go to the cart.!
        compObjs = $(compObjs).not(".viewDD,.subCB,.matCB:not(.abc)");
        if (compObjs.length > 0) {
            $(compObjs).each(function (i) {
                if (this.type != 'hidden') {
                    // DEBUG -------------
                    if (this.id != undefined
                    && this.id.length > 0
                    && $(this).hasClass('filterBy') == false
                    && $(this).hasClass('filterComp') == false)
                        warn('DEBUG: Unexpected control with name ['+this.name + '], and id #'+this.id);
                    // DEBUG ------------- Helps find unexpected problems.

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
				$(el).change(function(e) { subCfg.markChange(e, el); } ) });

        // Because of fauxDisabled subCBs, it is necessary to truly disable them before submitting.
        $("FORM").submit(function (i) {
            $('input.subCB.changed.disabled').attr('disabled',true);  // shadows will go to cart as they should

            // Unchanged controls will be disabled to avoid sending to the cart
            $('select,input').filter("[name]").not(".allOrOnly").not('.changed').each( function (i) {
                if (this.type != 'hidden' || $(this).hasClass('trPos')
                || $(this).hasClass('cbShadow') || $(this).hasClass('sortOrder')) {
                    // hiddens except priority and boolshad are all sent to the cart
                    // disable instead of unname because unname fills cart with a lot of garbage (Linux/FF)!
                    this.disabled = true;
                }
            });
        });
    }
};

