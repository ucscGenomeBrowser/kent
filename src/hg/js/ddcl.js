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

// Don't complain about line break before '||' etc:
/* jshint -W014 */

var ddcl = {
    //mySelf: null, // There is no need for a "mySelf" unless this object is being instantiated.

    textOfObjWrappedInStyle: function (obj)
    { // returns the obj text and if there is obj style, the text gets span wrapped with it
        var text = '';
        var style = $(obj).attr('style');
        if (style && style.length > 0)
            text = "<span style='"+style+"'>";
        text += $(obj).text();
        if (style && style.length > 0)
            text += "</span>";

        return text;
    },

    textOfCurrentSelections: function (options) {
        // Generates a multi-line string of currently selected options
        var chosen = $(options).filter(':selected');  // Works with FF and Chrome but not IE!
        if (chosen.length === 0 && theClient.isIe())
            chosen = $(options).find(':selected');  // Works with IE but not FF and Chrome!
        var chosenCount = $(chosen).length;
        var msg = '';
        if (chosenCount === 0) {
            msg = 'Please select...';
        } else if (chosenCount === 1) {
            msg = ddcl.textOfObjWrappedInStyle(chosen[0]);
        } else if (chosenCount === options.length) {
            msg = ddcl.textOfObjWrappedInStyle(options[0]);
        } else {
            for (var ix=0;ix<chosenCount;ix++) {
                if (ix > 0)
                    msg += "<BR>";
                msg += ddcl.textOfObjWrappedInStyle(chosen[ix]);
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
        $(controlLabel).css('color',newTextColor ); // could be empty string, thus removing color
        $(controlLabel).html(msg);
    },

    onOpen: function (event) {
        // Called by a DDCL onClick event (when the drop list is opened)
        var controlSelector = this;

        // Set the label
        var control = $(controlSelector).parent();
        ddcl.labelSet(control,"Select multiple...",'#000088','Selecting...');

        // Find the active 'items' and original 'options'
        var id = $(control).attr('id').substring('ddcl-'.length);
        var dropWrapper = $('#ddcl-' + id + '-ddw');//.first();
        var multiSelect = $(document.getElementById(id));
        var allCheckboxes = $(dropWrapper).find("input.active");
        var selectOptions = multiSelect[0].options;

        // Special juice to handle "exclude" options based upon competing filterBoxes
        try {
            if ((  $(multiSelect).hasClass('filterComp')  
                && filterCompositeExcludeOptions(multiSelect))
            ||  (  $(multiSelect).hasClass('filterTable') 
                && filterTable.excludeOptions(multiSelect))) {

                // "exclude" items based upon the exclude tag of the true options
                allCheckboxes.each(function(index) {
                    var item = $(this).parent();
                    if ($(selectOptions[index]).hasClass('excluded')) {
                        $(item).addClass("ui-state-excluded");
                    } else //if ($(item).hasClass("ui-state-excluded"))
                        $(item).removeClass("ui-state-excluded");
                });
            }
        }
        catch (err) {} // OK if filterCompositeExcludeOptions is not defined.

        // Show only first as selected if it is selected
        if (allCheckboxes[0].checked === true) {
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
        if (chosen.length === 0) {
            if ($(multiSelect).hasClass('noneIsAll')) {
                //$(multiSelect).first('option').first().attr('selected',true);
                multiSelect.options[0].selected = true;
                // How to check the first item?
                var dropWrapper = $('#ddcl-' + id + '-ddw');
                $(dropWrapper).find("input").first().attr("checked",true);
            }
        } else if (chosen.length === $(multiSelect).find('option').length) {
            // If all are chosen then select only the first!
            $(chosen).each(function(index) {
                if (index > 0)
                    $(this).attr('selected',false);
            });
        }

        var msg = ddcl.textOfCurrentSelections(multiSelect.options);

        var control = $('#ddcl-' + id);
        var newColor = '';
        if ($(multiSelect).find('option:selected').length === 0)
            newColor = '#AA0000'; // red
        //else if (msg.search(/color:/i) === -1)
        //    newColor = 'black';
        ddcl.labelSet(control,msg,newColor,'Click to select...');

        // Notice special handling for a custom event
        $(multiSelect).trigger('done',multiSelect);
    },

    reinit: function (filterBys,force) {
        // ReInitialize the DDCLs (drop-down checkbox-list)
        // This is done when the track search with tabs gets switched to advanced tab
        // because the DDCLs were setup on hidden filterBys and dimensiuons are wrong.
        // if not force, then only reinit when the dimensions are suspect
// NOTE: If force is true, this doesn't do anything!!

        if (filterBys.length < 1)
            return;

        $(filterBys).each( function(i) { // Do this by 'each' to set noneIsAll individually
            var multiSelect = this;
            if (!force) { // condition on bad dimensions
                var id = $(multiSelect).attr('id');
                var control = normed($('#ddcl-' + id));
                if (!control) {
                    // Object never initialized so do it now.
                    //$(multiSelect).show(); // necessary to get dimensions
                    ddcl.setup(multiSelect,'noneIsAll');
                } else {
                    // This is being called before normal init
                    var controlSelector = $(control).find(".ui-dropdownchecklist-selector");
                    if ($(controlSelector).width() <= 20) {
                        // Initialized before fully visible so do it again.
                        $(multiSelect).dropdownchecklist("destroy");
                        $(multiSelect).show(); // necessary to get dimensions
                        ddcl.setup(multiSelect,'noneIsAll');
                    } // else dimensions look okay
                }
            }
        });
    },

    setup: function (obj) {
        // Initialize the multiselect as a DDCL (drop-down checkbox-list)
        //mySelf = this; // There is no need for a "mySelf" unless this object is being instantiated

        // Defaults
        var myFirstIsAll = true;
        var myNoneIsAll  = false;
        var myIcon       = null;
        var myEmptyText  = 'Select...';
        var myClose      = 'close&nbsp;&nbsp;';
        var myDropHeight = filterByMaxHeight(obj);
        // parse optional args
        for (var vIx=1;vIx<arguments.length;vIx++) {
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
        if (myFirstIsAll === false)
            myNoneIsAll  = false;

        // Make sure there is an id!
        var id = $(obj).attr('id');
        if (!id || id.length === 0) {
            var name = $(obj).attr('name');
            if (name && name.length > 0)
                id = 'dd-' + name;
            else {
                id = 'ix' +  $('select').index(obj);
            }
            $(obj).attr('id',id);
        } else {
            if (normed($('#ddcl-' + id)))    // Don't set up twice!
                return;
        }

        // These values can only be taken from the select before it becomes a DDCL
        var maxWidth = $(obj).width();
        if (maxWidth === 0) // currently hidden so wait for a reinit();
            return;
        var minWidth = $(obj).css('min-width');
        if (minWidth && minWidth.length > 0) { // Is a string, so convert and compare
            minWidth = parseInt(minWidth);
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
                            onComplete: ddcl.onComplete
        });
        if (myNoneIsAll)
            $(obj).addClass('noneIsAll'); // Declare this as none selected same as all selected
        ddcl.onComplete(obj); // shows selected items in multiple lines

        // Set up the selector (control seen always and replacing select)
        var control = $('#ddcl-' + id);
        if (!control) {
            warn('ddcl.setup('+id+') failed to create drop-down checkbox-list');
            return;
        }
        var controlSelector = $(control).find(".ui-dropdownchecklist-selector");
        $(controlSelector).click(ddcl.onOpen);
        $(controlSelector).css({width:maxWidth+'px'});
        var controlText = $(control).find(".ui-dropdownchecklist-text");
        $(controlText).css({width:maxWidth+'px'});

        // Set up the drop list (control seen only on fucus and with items to choose)
        var dropWrapper = $('#ddcl-' + id + '-ddw');
        if (!dropWrapper) {
            warn('ddcl.setup('+id+') failed to create drop-down checkbox-list');
            return;
        }
        // Individual items need styling
        var itemHeight = 22;
        // Exclude the close button
        var dropItems = $(dropWrapper).find(".ui-dropdownchecklist-item");
        $(dropItems).hover(function () {$(this).css({backgroundColor:'#CCFFCC'});},
                           function () {$(this).css({backgroundColor:'white'});});
        dropItems = $(dropItems).not('.ui-dropdownchecklist-close');
        $(dropItems).css({background:'white', borderStyle:'none', height:itemHeight+'px'});
        var itemCount = dropItems.length;
        if (myClose && myClose.length > 0) {  // target the close button
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

        // Finally we can get style from the original select and apply it to the whole control
        if (style && style.length > 0) {
            var styles = style.split(';');
            for (var ix = 0;ix < styles.length;ix++) {
                var aStyleDef = styles[ix].split(':');
                aStyleDef[0] = aStyleDef[0].replace(' ',''); // no spaces
                if (aStyleDef[0] !== 'display') // Need to see if other styles should be restricted.
                    $(control).css(aStyleDef[0],aStyleDef[1]);
                if (aStyleDef[0].substring(0,4) === 'font')  // Fonts should be applied too
                    $(dropItems).css(aStyleDef[0],aStyleDef[1]);
            }
        }

    },

    start: function () {  // necessary to delay startup till all selects get ids.
        $('.filterBy,.filterComp').each( function(i) {
            if ($(this).hasClass('filterComp'))
                ddcl.setup(this); // Not nonIsAll
            else
                ddcl.setup(this,'noneIsAll');
        });
    }
};

  ///////////////////////
 //// Filter Tables ////
///////////////////////
var filterTable = {
// Filter Tables are HTML tables that can be filtered by drop-down-checkbox-lists (ddcl) controls
// To use this feature trs must have class 'filterble' and contain 
// tds with classes matching category and value
//    eg: <TR class='filterable'><TD class='cell 8988T'>8988T</td>
// and ddcl 'filterBy' controls with additional classes 'filterTable' and the category,
// and contains options with the the value to be filtered.
// Also, the ddcl objects onChange event must call filterTable.filter(this)
//    eg: <SELECT name='cell' MULTIPLE class='filterTable cell filterBy' 
//                onchange='filterTable.filter(this);' style='font-size:.9em;'>
//        <OPTION SELECTED VALUE='All'>All</OPTION>
//        <OPTION VALUE='8988T'>8988T</OPTION>
// Multiple filterTable controls can be defined, but only one table can be the target of filtering.

    variable: function (filter)
    { // returns the variable associated with a filter

        if ($(filter).hasClass('filterBy') === false)
            return undefined;
        if ($(filter).hasClass('filterTable') === false)
            return undefined;

        // Find the var for this filter
        var classes = $(filter).attr("class").split(' ');
        classes = aryRemove(classes,["filterBy","filterTable","noneIsAll"]);
        if (classes.length > 1 ) {
            warn('Too many classes for filterBy: ' + classes);
            return undefined;
        }
        return classes.pop();
    },
    
    applyOneFilter: function (filter,remainingTrs)
    { // Applies a single filter to a filterTables TRs
        var classes = $(filter).val();
        if (!classes || classes.length === 0)
            return null; // Nothing selected so exclude all rows

        if (classes[0] === 'All')
            return remainingTrs;  // nothing excluded by this filter

        // Get the filter variable
        var filterVar = filterTable.variable(filter);
        if (!filterVar || filterVar.length === 0)
            return null;

        var varTds = $(remainingTrs).children('td.' + filterVar);
        var filteredTrs = null;
        var ix =0;
        for (;ix<classes.length;ix++) {
            var tds = $(varTds).filter('.' + classes[ix]);
            if (tds.length > 0) {
                var trs = $(tds).parent();

                if (!filteredTrs)
                    filteredTrs = trs;
                else
                    filteredTrs = jQuery.merge( filteredTrs, trs );  // takes too long in IE!
            }
        }
        return filteredTrs;
    },

    trsSurviving: function (filterClass)
    // returns a list of trs that satisfy all filters
    // If defined, will exclude filter identified by filterClass
    {
        // find all filterable table rows
        var showTrs = $('tr.filterable'); // Default all
        if (showTrs.length === 0)
            return undefined;

        // Find all filters
        var filters = $("select.filterBy");
        if (filters.length === 0)
            return undefined;

        // Exclude one if requested.
        if (filterClass && filterClass.length > 0)
            filters = $(filters).not('.' + filterClass);

        for (var ix=0;showTrs && ix < filters.length;ix++) {
            showTrs = filterTable.applyOneFilter(filters[ix],showTrs);
        }
        return showTrs;
    },

    _filter: function ()
    { // Called by filter onchange event.  Will show/hide trs based upon all filters
        var showTrs = filterTable.trsSurviving();
        //$('tr.filterable').hide();  // <========= This is what is taking so long!
        $('tr.filterable').css('display', 'none');

        var counter = $('.filesCount');
        if (showTrs && showTrs.length > 0) {
            //$(showTrs).show();
            $(showTrs).css('display', '');

            // Update count
            if (counter)
                $(counter).text($(showTrs).length + " / ");
        } else {
            if (counter)
                $(counter).text(0 + " / ");
        }

        var tbody = $( $('tr.filterable')[0] ).parent('tbody.sorting');
        if (tbody)
            $(tbody).removeClass('sorting');
    },

    trigger: function ()
    { // Called by filter onchange event.  Will show/hide trs based upon all filters
        var tbody = $( $('tr.filterable')[0] ).parent('tbody');
        if (tbody)
            $(tbody).addClass('sorting');

        waitOnFunction(filterTable._filter);
    },

    done: function (event)
    { // Called by custom 'done' event
        event.stopImmediatePropagation();
        $(this).unbind( event );
        filterTable.trigger();
    },

    filter: function (multiSelect)
    { // Called by filter onchange event.  Will show/hide trs based upon all filters
        // IE takes tooo long, so this should be called only when leaving the filterBy box
        if ( $('tr.filterable').length > 300) {
            $(multiSelect).one('done',filterTable.done);
            return;
        } else
            filterTable.trigger();
    },

    excludeOptions: function (filter)
    { // bound to 'click' event inside ddcl.js.
    // Will mark all options in one filterBy box that are inconsistent with the current
    // selections in other filterBy boxes.  Mark with class ".excluded"

        // Compare to the list of all trs
        var allTrs = $('tr.filterable'); // Default all
        if (allTrs.length === 0)
            return false;

        // IE takes tooo long, so this should be called only when leaving the filterBy box
        if (theClient.isIePre11() && $(allTrs).length > 300) 
            return false;

        // Find the var for this filter
        var filterVar = filterTable.variable(filter);
        if (!filterVar)
            return false;

        // Look at list of visible trs.
        var visibleTrs = filterTable.trsSurviving(filterVar);
        if (!visibleTrs)
            return false;

        if (allTrs.length === visibleTrs.length) {
            $(filter).children('option.excluded').removeClass('excluded');
            return true;  // Nothing more to do.  All are already excluded
        }

        // Find the tds that belong to this var
        var tds = $(visibleTrs).find('td.'+filterVar);
        if (tds.length === 0) {
            $(filter).children('option').addClass('excluded');   // add .excluded" to all
            return true;
        }

        // Find the val classes
        var classes = []; // i.e. new Array();
        $(tds).each(function (i) {
            var someClass = $(this).attr("class").split(' ');
            someClass = aryRemove(someClass,[filterVar]);
            var val = someClass.pop();
            if (aryFind(classes,val) === -1)
                classes.push(val);
        });
        if (classes.length === 0) {
            $(filter).children('option').addClass('excluded');   // add .excluded" to all
            return true;
        }

        // Find all options with those classes
        $(filter).children('option').each(function (i) {
            if (aryFind(classes,$(this).val()) !== -1)
                $(this).removeClass('excluded'); // remove .excluded from matching
            else
                $(this).addClass('excluded');    // add .excluded" to non-matching
        });

        // If all options except "all" are included then all should nt be excluded
        var excluded = $(filter).children('option.excluded');
        if (excluded.length === 1) {
            var text = $(excluded[0]).text();
            if (text === 'All' || text === 'Any')
                $(excluded[0]).removeClass('excluded');
        }
        return true;
    }
};

$(document).ready(function() {

    setTimeout(ddcl.start,2);  // necessary to delay startup till all selects get ids.
});

