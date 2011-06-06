/**
 * TableDnD plug-in for JQuery, allows you to drag and drop table rows
 * You can set up various options to control how the system will work
 * Copyright (c) Denis Howlett <denish@isocra.com>
 * Licensed like jQuery, see http://docs.jquery.com/License.
 *
 * NOTE for Browser staff: Tim Dreszer has modified this from original:
 *      onDrop() callback changed: dragStartIndex is returned as third param.
 *      onDragStart() callback changed: the event is returned as first param.
 *
 * Configuration options:
 *
 * onDragStyle
 *     This is the style that is assigned to the row during drag. There are limitations to the styles that can be
 *     associated with a row (such as you can't assign a border--well you can, but it won't be
 *     displayed). (So instead consider using onDragClass.) The CSS style to apply is specified as
 *     a map (as used in the jQuery css(...) function).
 * onDropStyle
 *     This is the style that is assigned to the row when it is dropped. As for onDragStyle, there are limitations
 *     to what you can do. Also this replaces the original style, so again consider using onDragClass which
 *     is simply added and then removed on drop.
 * onDragClass
 *     This class is added for the duration of the drag and then removed when the row is dropped. It is more
 *     flexible than using onDragStyle since it can be inherited by the row cells and other content. The default
 *     is class is tDnD_whileDrag. So to use the default, simply customise this CSS class in your
 *     stylesheet.
 * onDrop
 *     Pass a function that will be called when the row is dropped. The function takes 2 parameters: the table
 *     and the row that was dropped. You can work out the new order of the rows by using
 *     table.rows.  NOTE: function now returns a third param: dragStartIndex (of row that was dragged)
 * onDragStart
 *     Pass a function that will be called when the user starts dragging. The function takes 2 parameters: the
 *     table and the row which the user has started to drag.
 * onAllowDrop
 *     Pass a function that will be called as a row is over another row. If the function returns true, allow
 *     dropping on that row, otherwise not. The function takes 2 parameters: the dragged row and the row under
 *     the cursor. It returns a boolean: true allows the drop, false doesn't allow it.
 * scrollAmount
 *     This is the number of pixels to scroll if the user moves the mouse cursor to the top or bottom of the
 *     window. The page should automatically scroll up or down as appropriate (tested in IE6, IE7, Safari, FF2,
 *     FF3 beta
 * dragHandle
 *     This is the name of a class that you assign to one or more cells in each row that is draggable. If you
 *     specify this class, then you are responsible for setting cursor: move in the CSS and only these cells
 *     will have the drag behaviour. If you do not specify a dragHandle, then you get the old behaviour where
 *     the whole row is draggable.
 *
 * Other ways to control behaviour:
 *
 * Add class="nodrop" to any rows for which you don't want to allow dropping, and class="nodrag" to any rows
 * that you don't want to be draggable.
 *
 * Inside the onDrop method you can also call $.tableDnD.serialize() this returns a string of the form
 * <tableID>[]=<rowID1>&<tableID>[]=<rowID2> so that you can send this back to the server. The table must have
 * an ID as must all the rows.
 *
 * Other methods:
 *
 * $("...").tableDnDUpdate()
 * Will update all the matching tables, that is it will reapply the mousedown method to the rows (or handle cells).
 * This is useful if you have updated the table rows using Ajax and you want to make the table draggable again.
 * The table maintains the original configuration (so you don't have to specify it again).
 *
 * $("...").tableDnDSerialize()
 * Will serialize and return the serialized string as above, but for each of the matching tables--so it can be
 * called from anywhere and isn't dependent on the currentTable being set up correctly before calling
 *
 * Known problems:
 * - Auto-scoll has some problems with IE7  (it scrolls even when it shouldn't), work-around: set scrollAmount to 0
 *
 * Version 0.2: 2008-02-20 First public version
 * Version 0.3: 2008-02-07 Added onDragStart option
 *                         Made the scroll amount configurable (default is 5 as before)
 * Version 0.4: 2008-03-15 Changed the noDrag/noDrop attributes to nodrag/nodrop classes
 *                         Added onAllowDrop to control dropping
 *                         Fixed a bug which meant that you couldn't set the scroll amount in both directions
 *                         Added serialize method
 * Version 0.5: 2008-05-16 Changed so that if you specify a dragHandle class it doesn't make the whole row
 *                         draggable
 *                         Improved the serialize method to use a default (and settable) regular expression.
 *                         Added tableDnDupate() and tableDnDSerialize() to be called when you are outside the table
 */
jQuery.tableDnD = {
    /** Keep hold of the current table being dragged */
    currentTable : null,
    /** Keep hold of the current drag object if any */
    dragObject: null,
    /** The current mouse offset */
    mouseOffset: null,
    /** Remember the old value of Y so that we don't do too much processing */
    oldY: 0,

    /** Actually build the structure */
    build: function(options) {
        // Set up the defaults if any

        this.each(function() {
            // This is bound to each matching table, set up the defaults and override with user options
            this.tableDnDConfig = jQuery.extend({
                onDragStyle: null,
                onDropStyle: null,
                // Add in the default class for whileDragging
                onDragClass: "tDnD_whileDrag",
                onDrop: null,
                onDragStart: null,
                scrollAmount: 5,
                serializeRegexp: /[^\-]*$/, // The regular expression to use to trim row IDs
                serializeParamName: null, // If you want to specify another parameter name instead of the table ID
                dragHandle: null, // If you give the name of a class here, then only Cells with this class will be draggable
                dragObjects: [],   // UCSC: Allows setting multiple rows to be dragged at one time
                downOffset: 0,     // UCSC: Dragging set, then offset Y to bottom of set
                upOffset: 0,       // UCSC: Dragging set, then offset Y to bottom of set
                dragStartIndex : 0
            }, options || {});
            // Now make the rows draggable
            jQuery.tableDnD.makeDraggable(this);
        });

        /////// UCSC  Move the binding so that it is only done on dragStart
        /////// UCSC  // Now we need to capture the mouse up and mouse move event
        /////// UCSC  // We can use bind so that we don't interfere with other event handlers
        /////// UCSC  jQuery(document)
        /////// UCSC      .bind('mousemove', jQuery.tableDnD.mousemove)
        /////// UCSC      .bind('mouseup', jQuery.tableDnD.mouseup);

        // Don't break the chain
        return this;
    },

    /** This function makes all the rows on the table draggable apart from those marked as "NoDrag" */
    makeDraggable: function(table) {
        var config = table.tableDnDConfig;
        if (table.tableDnDConfig.dragHandle) {
            // We only need to add the event to the specified cells
            var cells = jQuery("td."+table.tableDnDConfig.dragHandle, table);
            cells.each(function() {
                // The cell is bound to "this"
                jQuery(this).mousedown(function(ev) {
                    if(ev.button > 1)
                        return true;
                    if(jQuery.tableDnD == undefined)
                        return false;
                    jQuery.tableDnD.dragObject = this.parentNode;
                    jQuery.tableDnD.currentTable = table;
                    jQuery.tableDnD.mouseOffset = jQuery.tableDnD.getMouseOffset(this, ev);
                    config.dragStartIndex = $(jQuery.tableDnD.dragObject).attr('rowIndex');
                    if (config.onDragStart) {
                        // Call the onDrop method if there is one
                        config.onDragStart(ev, table, this.parentNode);

                        /////// UCSC
                        // Initialize oldY to avoid movingDown as first action
                        jQuery.tableDnD.oldY = jQuery.tableDnD.mouseCoords(ev).y - jQuery.tableDnD.mouseOffset.y;

                        // Capture the mouse move events only if dragStart
                        jQuery(document)
                            .bind('mousemove', jQuery.tableDnD.mousemove)
                            .bind('mouseup', jQuery.tableDnD.mouseup);

                        config.downOffset = 0;
                        config.upOffset = 0;
                        if (config.dragObjects.length > 1) {
                            for(var ix = 0; ix < config.dragObjects.length; ix++) {
                                var row = config.dragObjects[ix];
                                var rowIx = $(row).attr('rowIndex');
                                if (rowIx < config.dragStartIndex)
                                    config.upOffset -= $(row).height();
                                else if (rowIx > config.dragStartIndex)
                                    config.downOffset += $(row).height();
                            }
                        }
                        /////// UCSC
                    }
                    return false;
                });
            })
        } else {
            // For backwards compatibility, we add the event to the whole row
            var rows = jQuery("tr", table); // get all the rows as a wrapped set
            rows.each(function() {
                // Iterate through each row, the row is bound to "this"
                var row = jQuery(this);
                if (! row.hasClass("nodrag")) {
                    row.mousedown(function(ev) {
                        if (ev.target.tagName == "TD") {
                            jQuery.tableDnD.dragObject = this;
                            jQuery.tableDnD.currentTable = table;
                            jQuery.tableDnD.mouseOffset = jQuery.tableDnD.getMouseOffset(this, ev);
                            config.dragStartIndex = $(jQuery.tableDnD.dragObject).attr('rowIndex');
                            if (config.onDragStart) {
                                // Call the onDrop method if there is one
                                config.onDragStart(ev, table, this);
                            }
                            return false;
                        }
                    }).css("cursor", "move"); // Store the tableDnD object
                }
            });
        }
    },

    updateTables: function() {
        this.each(function() {
            // this is now bound to each matching table
            if (this.tableDnDConfig) {
                jQuery.tableDnD.makeDraggable(this);
            }
        })
    },

    /** Get the mouse coordinates from the event (allowing for browser differences) */
    mouseCoords: function(ev){
        if(ev.pageX || ev.pageY){
            return {x:ev.pageX, y:ev.pageY};
        }
        return {
            x:ev.clientX + document.body.scrollLeft - document.body.clientLeft,
            y:ev.clientY + document.body.scrollTop  - document.body.clientTop
        };
    },

    /** Given a target element and a mouse event, get the mouse offset from that element.
        To do this we need the element's position and the mouse position */
    getMouseOffset: function(target, ev) {
        ev = ev || window.event;

        var docPos    = this.getPosition(target);
        var mousePos  = this.mouseCoords(ev);
        ///////return {x:mousePos.x - docPos.x, y:mousePos.y - docPos.y};
        return {x:mousePos.x - docPos.x, y:$(target).height()/2}; ///////// y offest is middle of row.  Modified by tim because of tall tracks.
    },

    /** Get the position of an element by going up the DOM tree and adding up all the offsets */
    getPosition: function(e){
        var left = 0;
        var top  = 0;
        /** Safari fix -- thanks to Luis Chato for this! */
        if (e.offsetHeight == 0) {
            /** Safari 2 doesn't correctly grab the offsetTop of a table row
            this is detailed here:
            http://jacob.peargrove.com/blog/2006/technical/table-row-offsettop-bug-in-safari/
            the solution is likewise noted there, grab the offset of a table cell in the row - the firstChild.
            note that firefox will return a text node as a first child, so designing a more thorough
            solution may need to take that into account, for now this seems to work in firefox, safari, ie */
            e = e.firstChild; // a table cell
        }
        var eHeight = e.height;  //////////////// Height was added by tim because of tall tracks

        while (e.offsetParent){
            left += e.offsetLeft;
            top  += e.offsetTop;
            e     = e.offsetParent;
        }

        left += e.offsetLeft;
        top  += e.offsetTop;

        ///////return {x:left, y:top};
        return {x:left, y:top, height:eHeight};  ///////////// Height was added by tim because of tall tracks
    },

    mousemove: function(ev) {
        if(jQuery.tableDnD == undefined
        || jQuery.tableDnD.dragObject == null) {  //// UCSC Binding should occur at dragStart
            jQuery(document)
                .unbind('mousemove')//, jQuery.tableDnD.mousemove);
                .unbind('mouseup');//, jQuery.tableDnD.mouseup);
            return;
        }
        ///// UCSC if (jQuery.tableDnD.dragObject == null) {
        ///// UCSC     return;
        ///// UCSC }

        var dragObj = jQuery(jQuery.tableDnD.dragObject);
        var config = jQuery.tableDnD.currentTable.tableDnDConfig;
        var mousePos = jQuery.tableDnD.mouseCoords(ev);
        var y = mousePos.y - jQuery.tableDnD.mouseOffset.y;

        //auto scroll the window
        var yOffset = window.pageYOffset;
        if (document.all) {
            // Windows version
            //yOffset=document.body.scrollTop;
            if (typeof document.compatMode != 'undefined' &&
                    document.compatMode != 'BackCompat') {
                yOffset = document.documentElement.scrollTop;
            }
            else if (typeof document.body != 'undefined') {
                yOffset=document.body.scrollTop;
            }

        }

        if (mousePos.y - yOffset < config.scrollAmount) {
            window.scrollBy(0, -config.scrollAmount);
        } else {
            var windowHeight = window.innerHeight ? window.innerHeight
                    : document.documentElement.clientHeight ? document.documentElement.clientHeight : document.body.clientHeight;
            if (windowHeight-(mousePos.y - yOffset) < config.scrollAmount) {
                window.scrollBy(0, config.scrollAmount);
            }
        }

        if (y != jQuery.tableDnD.oldY) {
            // work out if we're going up or down...
            var movingDown = y > jQuery.tableDnD.oldY;
            // update the old value
            jQuery.tableDnD.oldY = y;
            // update the style to show we're dragging
            if (config.onDragClass) {
                ////// UCSC
                if (config.dragObjects.length > 1)
                    config.dragObjects.addClass(config.onDragClass);
                else ////// UCSC
                    dragObj.addClass(config.onDragClass);
            } else {
                ////// UCSC
                if (config.dragObjects.length > 1)
                    config.css(config.onDragStyle);
                else ////// UCSC
                    dragObj.css(config.onDragStyle);
            }
            // If we're over a row then move the dragged row to there so that the user sees the
            // effect dynamically

            ////// UCSC
            var currentRow = null;
            // While dragObjects is the set, the order could vary from table order.  Therefore us rowIndexes.
            var rows = jQuery.tableDnD.currentTable.rows;
            var firstDragRowIx = 9999; // Discover indexes
            var lastDragRowIx = 0;

            if (config.dragObjects.length > 1) {
                for (var ix=0;ix < config.dragObjects.length;ix++) {
                    var thisIx = $( config.dragObjects[ix] ).attr('rowIndex');
                    if (firstDragRowIx > thisIx)
                        firstDragRowIx = thisIx;
                    if (lastDragRowIx < thisIx)
                        lastDragRowIx = thisIx;
                }
                if (movingDown)
                    currentRow = jQuery.tableDnD.findDropTargetRow(rows[lastDragRowIx], y + config.downOffset);
                else
                    currentRow = jQuery.tableDnD.findDropTargetRow(rows[firstDragRowIx], y + config.upOffset);
            } else  ////// UCSC
                currentRow = jQuery.tableDnD.findDropTargetRow(dragObj, y);


            if (currentRow) {

                ////// UCSC
                if (config.dragObjects.length > 1) {
                    var targetRowIx = $( currentRow ).attr('rowIndex');
                    if (targetRowIx >= 0 && targetRowIx < rows.length) {

                        if (movingDown) { // && config.dragObjects[config.dragObjects.length - 1] != currentRow) {
                            // A whole new tack:  since movingUp never fails, always move target (and others) up
                            if ((lastDragRowIx - firstDragRowIx) == (config.dragObjects.length - 1)
                            &&  firstDragRowIx >= 0
                            &&  lastDragRowIx < targetRowIx) {
                                var plusIx=0;
                                for(var ix=lastDragRowIx+1; ix <= targetRowIx; ix++) {
                                    $( rows[ix] ).insertBefore( $( rows[firstDragRowIx + plusIx] ) );
                                    plusIx++;
                                }
                            }
                        } else if (!movingDown) { // && config.dragObjects[0] != currentRow) {
                            if ((lastDragRowIx - firstDragRowIx) == (config.dragObjects.length - 1)
                            &&  firstDragRowIx >= 0
                            &&  firstDragRowIx > targetRowIx) {
                                var plusIx=0;
                                for(var ix=firstDragRowIx; ix <= lastDragRowIx; ix++) {
                                    $(rows[ix]).insertBefore( rows[targetRowIx + plusIx] );
                                    plusIx++;
                                }
                            }
                        }
                    }
                } else {  ////// UCSC

                    // TODO worry about what happens when there are multiple TBODIES
                    if (movingDown && jQuery.tableDnD.dragObject != currentRow) {
                        jQuery.tableDnD.dragObject.parentNode.insertBefore(jQuery.tableDnD.dragObject, currentRow.nextSibling);
                    } else if (! movingDown && jQuery.tableDnD.dragObject != currentRow) {
                        jQuery.tableDnD.dragObject.parentNode.insertBefore(jQuery.tableDnD.dragObject, currentRow);
                    }

                }   ////// UCSC
            }
        }

        return false;
    },

    /** We're only worried about the y position really, because we can only move rows up and down */
    findDropTargetRow: function(draggedRow, y) {
        var rows = jQuery.tableDnD.currentTable.rows;
        for (var i=0; i<rows.length; i++) {
            var row = rows[i];
            var rowY    = this.getPosition(row).y;
            var rowHeight = parseInt(row.offsetHeight)/2;
            if (row.offsetHeight == 0) {
                rowY = this.getPosition(row.firstChild).y;
                rowHeight = parseInt(row.firstChild.offsetHeight)/2;
            }
            // Because we always have to insert before, we need to offset the height a bit
            //////////if ((y > rowY - rowHeight) && (y < (rowY + rowHeight))) {
            if ((y > rowY) && (y < (rowY + rowHeight))) { ////////// UCSC This was modified by tim because of tall tracks
                // that's the row we're over
                // If it's the same as the current row, ignore it
                if (row == draggedRow) {return null;}
                var config = jQuery.tableDnD.currentTable.tableDnDConfig;
                if (config.onAllowDrop) {
                    if (config.onAllowDrop(draggedRow, row)) {
                        return row;
                    } else {
                        return null;
                    }
                } else {
                    // If a row has nodrop class, then don't allow dropping (inspired by John Tarr and Famic)
                    var nodrop = jQuery(row).hasClass("nodrop");
                    if (! nodrop) {
                        return row;
                    } else {
                        return null;
                    }
                }
                return row;
            }
        }
        return null;
    },

    mouseup: function(e) {
        if(jQuery.tableDnD == undefined) {
            jQuery(document)
                .unbind('mousemove')//, jQuery.tableDnD.mousemove);
                .unbind('mouseup');//, jQuery.tableDnD.mouseup);
            return;
        }
        if (jQuery.tableDnD.currentTable && jQuery.tableDnD.dragObject) {
            var droppedRow = jQuery.tableDnD.dragObject;
            var config = jQuery.tableDnD.currentTable.tableDnDConfig;
            // If we have a dragObject, then we need to release it,
            // The row will already have been moved to the right place so we just reset stuff
            if (config.onDragClass) {
                ////// UCSC
                if (config.dragObjects.length > 1)
                    config.dragObjects.removeClass(config.onDragClass);
                else ////// UCSC
                    jQuery(droppedRow).removeClass(config.onDragClass);
            } else {
                ////// UCSC
                if (config.dragObjects.length > 1)
                    config.dragObjects.css(config.onDropStyle);
                else ////// UCSC
                    jQuery(droppedRow).css(config.onDropStyle);
            }
            jQuery.tableDnD.dragObject   = null;
            if (config.onDrop) {
                // Call the onDrop method if there is one
                config.onDrop(jQuery.tableDnD.currentTable, droppedRow, config.dragStartIndex);
            }
            jQuery.tableDnD.currentTable = null; // let go of the table too
            config.dragObjects = [];  ////// UCSC
        }
    },

    serialize: function() {
        if (jQuery.tableDnD.currentTable) {
            return jQuery.tableDnD.serializeTable(jQuery.tableDnD.currentTable);
        } else {
            return "Error: No Table id set, you need to set an id on your table and every row";
        }
    },

    serializeTable: function(table) {
        var result = "";
        var tableId = table.id;
        var rows = table.rows;
        for (var i=0; i<rows.length; i++) {
            if (result.length > 0) result += "&";
            var rowId = rows[i].id;
            if (rowId && rowId && table.tableDnDConfig && table.tableDnDConfig.serializeRegexp) {
                rowId = rowId.match(table.tableDnDConfig.serializeRegexp)[0];
            }

            result += tableId + '[]=' + rowId;
        }
        return result;
    },

    serializeTables: function() {
        var result = "";
        this.each(function() {
            // this is now bound to each matching table
            result += jQuery.tableDnD.serializeTable(this);
        });
        return result;
    }

}

jQuery.fn.extend(
    {
        tableDnD : jQuery.tableDnD.build,
        tableDnDUpdate : jQuery.tableDnD.updateTables,
        tableDnDSerialize: jQuery.tableDnD.serializeTables
    }
);