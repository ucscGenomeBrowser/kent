// The "ddcl" object that contains all the DDCL extension/support code.
// DDCL: drop-down checkbox-list wrapper code to ui.dropdownchecklist.js jQuery widget.
// The reason for extension/support code beyond the jquery widget is that
// We have changed some widget functionality:
// - Closed control show all currently selected options in multiple lines with CSS
// - Open control can gray out invalid choices

// NOTE:
// Currently tied to ui.dropdownchecklist.js v1.3, with 2 "UCSC" changes:
// - Chrome multi-select required changing ddcl code as per issue 176.
// - IE needed special code to block window.resize event in DDCL.
// *** v1.4 has been released which works with jquery 1.6.1 ***

// Some useful names defined:
// ddcl: This object just contains these supporting functions in one package.
// multiSelect: original <SELECT multiple> that ddcl wraps around
//  (AKA filterBy: all mutiSelects are of class .filterBy [may also be .filterComp, .filterTable])
// selectOptions: original multiSelect.options (get updated, sent to cart, etc.)
// control: The whole gaggle of elements that get created when one multiSelect becomes a ddcl
// controlLabel: Just the text inside the closed control
// controlSelector: the closed box waiting to be clicked
// dropWrapper: The div that contains all the checkboxes and is only seen when the DDCL is open
// allCheckboxes: one to one correspondence with selectOptions

var ddcl = {
    mySelf: null,

    textOfObjWrappedInStyle: function (obj)
    { // returns the obj text and if there is obj style, the text gets span wrapped with it
        var text = '';
        var style = $(obj).attr('style');
        if (style != undefined && style.length > 0)
            text = "<span style='"+style+"'>";
        text += $(obj).text();
        if (style != undefined && style.length > 0)
            text += "</span>";

        return text;
    },

    textOfCurrentSelections: function (options) {
        // Generates a multi-line string of currently selected options
        var chosen = $(options).filter(':selected');  // Works with FF and Chrome but not IE!
        if (chosen.length == 0 && $.browser.msie)
            chosen = $(options).find(':selected');  // Works with IE but not FF and Chrome!
        var chosenCount = $(chosen).length;
        var msg = '';
        if(chosenCount == 0) {
            msg = 'Please select...';
        } else if(chosenCount == 1) {
            msg = mySelf.textOfObjWrappedInStyle(chosen[0]);
        } else if(chosenCount == options.length) {
            msg = mySelf.textOfObjWrappedInStyle(options[0]);
        } else {
            for(var ix=0;ix<chosenCount;ix++) {
                if (ix > 0)
                    msg += "<BR>";
                msg += mySelf.textOfObjWrappedInStyle(chosen[ix]);
            }
        }
        return msg;
    },

    labelSet: function (control,msg,newTextColor,newTitle) {
        // Sets the label text (as opposed to the drop-down options)
        var controlLabel    = $(control).find(".ui-dropdownchecklist-text");
        var controlSelector = $(control).find(".ui-dropdownchecklist-selector");
        var newHeight = msg.split('<BR>').length * 20;
        //$(control).css('height',newHeight + 'px');
        $(controlSelector).css({height: newHeight + 'px', background: '#fff'});
        $(controlLabel).attr('title',newTitle);
        $(controlLabel).css({height: newHeight + 'px'});
        $(controlLabel).css('color',newTextColor ); // could be empty string, thus removing the color
        $(controlLabel).html(msg);
    },

    onOpen: function (event) {
        // Called by a DDCL onClick event (when the drop list is opened)
        var controlSelector = this;

        // Set the label
        var control = $(controlSelector).parent();
        mySelf.labelSet(control,"Select multiple...",'#000088','Selecting...');

        // Find the active 'items' and original 'options'
        var id = $(control).attr('id').substring('ddcl-'.length);
        var dropWrapper = $('#ddcl-' + id + '-ddw');//.first();
        var multiSelect = $('#' + id);
        var allCheckboxes = $(dropWrapper).find("input.active");
        var selectOptions = multiSelect[0].options;

        // Special juice to handle "exclude" options based upon competing filterBoxes
        try {
            if(($(multiSelect).hasClass('filterComp')  && filterCompositeExcludeOptions(multiSelect))
            || ($(multiSelect).hasClass('filterTable') && filterTableExcludeOptions(multiSelect))) {

                // "exclude" items based upon the exclude tag of the true options
                allCheckboxes.each(function(index) {
                    var item = $(this).parent();
                    if($(selectOptions[index]).hasClass('excluded')) {
                        $(item).addClass("ui-state-excluded");
                    } else //if($(item).hasClass("ui-state-excluded"))
                        $(item).removeClass("ui-state-excluded");
                });
            }
        }
        catch (err) {} // OK if filterCompositeExcludeOptions is not defined.

        // Show only first as selected if it is selected
        if (allCheckboxes[0].checked == true) {
            allCheckboxes.each(function(index) {
                if (index > 0)
                    $(this).attr('checked',false);
            });
        }
    },

    onComplete: function (multiSelect) {
        // Called by ui.dropdownchecklist.js when selections have been made
        // Also called at init to fill the selector with current choices

        // Warning: In IE this gets called when still selecting!

        var id = $(multiSelect).attr('id');

        // If no  options are selected, may have to force all
        var chosen = $(multiSelect).find('option:selected');
        if (chosen.length == 0) {
            if ($(multiSelect).hasClass('noneIsAll')) {
                //$(multiSelect).first('option').first().attr('selected',true);
                multiSelect.options[0].selected = true;
                // How to check the first item?
                var dropWrapper = $('#ddcl-' + id + '-ddw');
                $(dropWrapper).find("input").first().attr("checked",true);
            }
        } else if (chosen.length == $(multiSelect).find('option').length) {
            // If all are chosen then select only the first!
            $(chosen).each(function(index) {
                if (index > 0)
                    $(this).attr('selected',false);
            });
        }

        var msg = ddcl.textOfCurrentSelections(multiSelect.options);

        var control = $('#ddcl-' + id);
        var newColor = '';
        if ($(multiSelect).find('option:selected').length == 0)
            newColor = '#AA0000'; // red
        //else if (msg.search(/color:/i) == -1)
        //    newColor = 'black';
        mySelf.labelSet(control,msg,newColor,'Click to select...');

        // Notice special handling for a custom event
        $(multiSelect).trigger('done',multiSelect);
    },

    reinit: function (filterBys,force) {
        // ReInitialize the DDCLs (drop-down checkbox-list)
        // This is done when the track search with tabs gets switched to advanced tab
        // because the DDCLs were setup on hidden filterBys and dimensiuons are wrong.
        // if not force, then only reinit when the dimensions are suspect

        if (filterBys.length < 1)
            return;

        $(filterBys).each( function(i) { // Do this by 'each' to set noneIsAll individually
            var multiSelect = this;
            if (!force) { // condition on bad dimensions
                var id = $(multiSelect).attr('id');
                control = $('#ddcl-' + id);
                if (control != null && control != undefined) {
                    var controlSelector = $(control).find(".ui-dropdownchecklist-selector");
                    if ($(controlSelector).width() > 20)
                        return;  // Dimensions look okay
                }
            }
            $(multiSelect).dropdownchecklist("destroy");
            $(multiSelect).show(); // necessary to get dimensions
            if (newJQuery)
                ddcl.setup(multiSelect,'noneIsAll');
            else
                $(multiSelect).dropdownchecklist({ firstItemChecksAll: true,
                                                   noneIsAll: true,
                                                   maxDropHeight: filterByMaxHeight(multiSelect) });
        });
    },

    setup: function (obj) {
        // Initialize the multiselect as a DDCL (drop-down checkbox-list)
        mySelf = this;

        // Defaults
        var myFirstIsAll = true;
        var myNoneIsAll  = false;
        var myIcon       = null;
        var myEmptyText  = 'Select...';
        var myClose      = 'close&nbsp;&nbsp;';
        var myDropHeight = filterByMaxHeight(obj);
        // parse optional args
        for(var vIx=1;vIx<arguments.length;vIx++) {
            switch(arguments[vIx]) {
                case 'noneIsAll':   myNoneIsAll = true;
                                    break;
                case 'firstNotAll': myFirstIsAll = false;
                                    break;
                case 'arrows':      myIcon = {};
                                    break;
                case 'noClose':     myClose = null;
                                    break;
                case 'label':       vIx++;
                                    if (vIx<arguments.length)
                                        myEmptyText  = arguments[vIx];
                                    break;
                default:            warn('ddcl.setup() unexpected argument: '+arguments[vIx]);
                                    break;
            }
        }
        if (myFirstIsAll == false)
            myNoneIsAll  = false;

        // Make sure there is an id!
        var id = $(obj).attr('id');
        if (id == null || id.length == 0) {
            var name = $(obj).attr('name');
            if (name != null && name.length > 0)
                id = 'dd-' + name;
            else {
                var ix = $('select').index(obj);
                id = 'ix' + ix;
            }
            $(obj).attr('id',id);
        }

        // These values can only be taken from the select before it becomes a DDCL
        var maxWidth = $(obj).width();
        var minWidth = $(obj).css('min-width');
        if (minWidth != undefined && minWidth.length > 0) {
            minWidth = parseInt(maxWidth);
            if (maxWidth < minWidth)
                maxWidth = minWidth;
        }
        maxWidth = (Math.ceil(maxWidth / 10) * 10) + 10; // Makes for more even boxes
        var style = $(obj).attr('style');

        // The magic starts here:
        $(obj).dropdownchecklist({
                            firstItemChecksAll: true,
                            noneIsAll: myNoneIsAll,
                            maxDropHeight: myDropHeight,
                            icon: myIcon,
                            emptyText: myEmptyText,
                            explicitClose: myClose,
                            textFormatFunction: function () { return 'selecting...'; } ,
                            onComplete: mySelf.onComplete
        });
        if (myNoneIsAll)
            $(obj).addClass('noneIsAll'); // Declare this as none selected same as all selected
        mySelf.onComplete(obj); // shows selected items in multiple lines

        // Set up the selector (control seen always and replacing select)
        control = $('#ddcl-' + id);
        if (control == null || control == undefined) {
            warn('ddcl.setup('+id+') failed to create drop-down checkbox-list');
            return;
        }
        var controlSelector = $(control).find(".ui-dropdownchecklist-selector");
        $(controlSelector).click(mySelf.onOpen);
        $(controlSelector).css({width:maxWidth+'px'});
        var controlText = $(control).find(".ui-dropdownchecklist-text");
        $(controlText).css({width:maxWidth+'px'});

        // Set up the drop list (control seen only on fucus and with items to choose)
        var dropWrapper = $('#ddcl-' + id + '-ddw');
        if (dropWrapper == null || dropWrapper == undefined) {
            warn('ddcl.setup('+id+') failed to create drop-down checkbox-list');
            return;
        }
        // Individual items need styling
        var itemHeight = 22;
        // Exclude the close button
        var dropItems = $(dropWrapper).find(".ui-dropdownchecklist-item");//.not('.ui-dropdownchecklist-close');
        $(dropItems).hover(function () {$(this).css({backgroundColor:'#CCFFCC'});},
                           function () {$(this).css({backgroundColor:'white'});});
        var dropItems = $(dropItems).not('.ui-dropdownchecklist-close');
        $(dropItems).css({background:'white', borderStyle:'none', height:itemHeight+'px'});
        var itemCount = dropItems.length;
        if (myClose != null) {  // target the close button
            var dropClose = $(dropWrapper).find(".ui-dropdownchecklist-close");
            $(dropClose).css({height:(itemHeight - 1)+'px',textAlign:'center'});
            itemCount++;
        }

        // The whole droplist needs styling
        var dropContainerDiv = dropWrapper.find(".ui-dropdownchecklist-dropcontainer");
        var maxHeight = (itemHeight*itemCount) + 1; // extra prevents unwanted vertical scrollbar
        var divHeight = dropContainerDiv.outerHeight();
        if (divHeight > maxHeight) {
            $(dropContainerDiv).css({height:maxHeight+'px'});
            $(dropWrapper).css({height:maxHeight+'px'});
        }
        maxWidth += 30; // extra avoids horizontal scrollBar when vertical one is included
        $(dropContainerDiv).css({width:(maxWidth)+'px'});
        $(dropWrapper).css({width:maxWidth+'px'});

        // Finally we can get style from the original select and apply it to the whole control (hopefully)
        if (style != undefined && style.length > 0) {
            var styles = style.split(';');
            for(var ix = 0;ix < styles.length;ix++) {
                var aStyleDef = styles[ix].split(':');
                aStyleDef[0] = aStyleDef[0].replace(' ',''); // no spaces
                if (aStyleDef[0] != 'display') // WARNING: Need to see if other styles should be restricted.
                    $(control).css(aStyleDef[0],aStyleDef[1]);
                if (aStyleDef[0].substring(0,4) == 'font')  // Fonts should be applied too
                    $(dropItems).css(aStyleDef[0],aStyleDef[1]);
            }
        }

    }
};

$(document).ready(function() {
    $('.filterBy,.filterComp').each( function(i) {
        if ($(this).hasClass('filterComp'))
            ddcl.setup(this); // Not nonIsAll
        else
            ddcl.setup(this,'noneIsAll')
    });
});
