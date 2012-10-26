// Used by trackDbDoc.html and friends

function jumpTo(obj) {
    // jumps to an anchor based upon text of obj
    //alert("url:"+window.location+"#"+obj.text);
    obj.href="#"+obj.text;
    return true;
}

var tdbDoc = {

    sortNoCase: function (x,y) {
      // case insensitive sort routine
      // perhaps move this to utils.js
      // NOTE: localeCompare() is not supported by all browsers.
      var a = String(x).toUpperCase();
      var b = String(y).toUpperCase();
      if (a > b) return  1
      if (a < b) return -1
                 return  0;
    },

    library: { // separate namespace for library handling

        checkClasses: function (classes) {
            // trackDbTestBlurb.html only
            // Walks through the classes and ensures they are unique

            //warn("class count:"+classes.length);
            $(classes).each(function (ix) {

                var divs = $('div#library').find( 'div.'+this );
                if (divs.length > 1) {
                    warn("Found more than one DIV of class='"+this+"' in library.");
                    return false;
                }
            });
            return true;
        },

        checkBlurb: function (div) {
            // trackDbTestBlurb.html only
            // check that a single div is conforming and return classes
            var classes = $(div).attr("class").split(" ");
            if (classes.length > 1) {
                warn('DIV with more than one class: '+ classes);
            } else if (classes.length === 1) {
                if (classes[0].indexOf('_intro') === -1) {
                    var types = $(div).find('span.types');
                    if (types.length === 0)
                        warn('DIV '+classes[0]+' missing "types" span.');
                    else if (types.length > 1)
                        warn('DIV '+classes[0]+' more than one "types" span:'+types.length);

                    var format = $(div).find('div.format');
                    if (format.length === 0)
                        warn('DIV '+classes[0]+' missing "format" DIV.');
                    else if (format.length > 1)
                        warn('DIV '+classes[0]+' more than one "format" DIV:'+format.length);
                }
            }
            return classes;
        },

        checkup: function () {
            // trackDbTestBlurb.html only
            // This routine runs the library through unique class check
            var allClasses = [];
            $('div#library').find('div').not('.format,.hintBox').each(function (ix) {
                var classes = tdbDoc.library.checkBlurb(this);
                if ( classes.length === 0 )
                    return true;
                allClasses.push(classes);
            });
            return tdbDoc.library.checkClasses(allClasses);
        },

        lookup: function (id,verbose) {
            // Standard lookup for library blurbs
            var blurb = $('div#library').find('div.'+id);
            if (blurb.length > 1 && verbose) {
                warn("Non unique ID found in library: "+ id);
            } else if (blurb.length === 0) {
                // There can be several references to the same details blurb in
                // the document.  So not finding it may mean it has already been
                // moved from the library.  Find an clone that one.
                blurb = $("div."+id).first();
                if (blurb.length === 0 && verbose)
                    warn("Missing document blurb for ID: "+ id);
                else {
                    var format = $(blurb).parent('td').find('.format');
                    blurb =  $(blurb).clone();
                    $(blurb).attr('id','');  // cloned blurb cannot have unique ID
                    if (format.length === 1) {
                        format =  $(format).clone();
                        $(blurb).append(format);
                    }
                }
            }
            return blurb;
        }
    },

    toc: { // separate namespace for self-assembling table of contents

        blurbCells: function () {
            // returns list of cells that are to be filled with blurbs
            return $('table.settingsTable').find('td:has(div.format)');
        },

        blurblessCells: function () {
            // returns list of setting cells that are not in within library
            var tbls = $('table.settingsTable').not('#alphabetical,#toc,:has(div.format)');
            if (tbls.length === 0)
                return [];
            return $(tbls).find("td:has(A)");
        },

        classesFromObjects: function (objs,onlyOne) {
            // converts objs array to classes array.  Warns if obj does not have exactly 1 class
            var allClasses = [];
            $(objs).each(function (ix) {
                var classes = $(this).attr("class").split(" ");
                if (classes.length > 1 && onlyOne)
                    warn('obj with more than one class: '+ classes);
                else if (classes.length === 0 && onlyOne)
                    warn('obj with no classes.');
                else
                    allClasses.push( classes );   // classes string array is [ [ str1], [str2] ]
            });
            return allClasses;
        },

        namesFromContainedAnchors: function (cells) {
            // Special case code for getting hidden settings that are only documented within
            // another settings blob (example viewLimitsMax)
            // returns list of named anchors within cells which should detect these hidden settings
            var allNames = [];
            $(cells).each(function (ix) {
                var cell = this;
                var anchors = $(cell).children("A[name!='']");
                if (anchors.length > 1) { // one is expected and not interesting
                    var aClass = String( $(cell).attr('class') );   // NEVER name var 'class' !!!!!
                    $(anchors).each(function (ix) {
                        var name = String( $(this).attr('name') );
                        if (name !== aClass)
                            allNames.push( [ name ] ); // string array best as [ [ str1], [str2] ]
                    });
                }
            });
            return allNames;
        },

        classesExcludeSuffixes: function (classes, suffixes) {
            // Since class list will include "_example" classes...
            // returns classes after those with unwanted suffixes are removed
            var cleanClasses = [];
            while (classes.length > 0) {      // unless string array is [ [ str1], [str2] ]
                var aClass = classes.shift(); //        shift/pop gets lost!
                for(var ix=0;ix<suffixes.length;ix++) {
                    if (aClass[0].indexOf( suffixes[ix] ) !== -1)
                        break;
                }
                if (ix === suffixes.length)
                    cleanClasses.push(aClass);
            }
            return cleanClasses;
        },

        makeRow: function (aClass) {
            // Puts together a single row into a self assembling table of contents

            // Find types
            var types = "";
            var div = tdbDoc.library.lookup(aClass,false);
            if (div.length === 1) {
                var spanner = $(div).find('span.types');
                if (spanner.length === 1) {
                    var classes = $(spanner).attr("class").split(" ");
                    classes = aryRemove(classes,['types']);
                    $(classes).each(function (ix) {
                        types += " <A onclick='return jumpTo(this);' HREF='#'>"+this+"</a>";
                    });
                }
            }

            // Where documented (what table)?
            // TODO: Should rewrite classes array to also carry the table.
            var best = '';
            var td = $('td.'+aClass);
            var tbl;
            if (td.length > 0) // Always chooses first
                tbl = $(td[0]).parents('table.settingsTable');
            else
                tbl = $('table.settingsTable').has("a[name='"+aClass+"']");

            if (tbl != undefined && tbl.length === 1) {
                var id = $(tbl[0]).attr('id');
                if (id.length > 0) {
                    best = "<A HREF='#"+id+"'>"+id.replace(/_/g," ")+"</a>"
                    //if (td.length > 1)
                    //    best += " found "+td.length;
                }
            }

            return "<tr><td><A onclick='return jumpTo(this);' HREF='#'>"+aClass+"</a></td><td>"+
                        types+"</td><td>"+best+"</td></tr>";
        },

        excludeClasses: [],

        assemble: function () {
            // assembles (or extends) a table of contents if on is found (Launched by timer)
            var tocTable = $('table#toc');
            if (tocTable.length === 1) {
                var cells = tdbDoc.toc.blurbCells(); // Most documented settings are found here
                var names = tdbDoc.toc.namesFromContainedAnchors(cells);// settings in others' cells
                cells = $(cells).add( tdbDoc.toc.blurblessCells() );    // undocumented: losers
                var classes = tdbDoc.toc.classesFromObjects(cells,true);// classes are setting names
                classes = classes.concat( names );                      // hidden names wanted to
                if (classes.length === 0)
                    return;

                // clean up and sort
                classes = tdbDoc.toc.classesExcludeSuffixes(classes,['_intro','_example']);
                if (classes.length === 0)
                    return;
                if (tdbDoc.toc.excludeClasses.length > 0) {
                    aryRemove( classes, tdbDoc.toc.excludeClasses );
                    tdbDoc.toc.excludeClasses = [];
                }
                classes.sort( tdbDoc.sortNoCase );

                // Should now have a full set of settings to add to TOC
                if ($(tocTable).find('thead').length === 0)
                    $(tocTable).prepend( "<THEAD><TR><TD colspan='3'>"+
                               "<H3>Table of Contents</H3></TD></TR></THEAD>" );
                if ($(tocTable).find('th').length === 0)
                    $(tocTable).append( "<TR VALIGN=TOP><TH WIDTH=100>Setting</TH>"+
                                        "<TH WIDTH='25%'>For Types</TH><TH>Documented</TH></TR>" );
                var lastClass = '';
                $(classes).each(function (ix) {
                    if (lastClass !== String(this)) {// skip duplicates
                        lastClass = String(this);
                        $(tocTable).append( tdbDoc.toc.makeRow(this) );
                    }
                });
                // TODO: Nice to do: allow for seeding toc with user defined rows.
                // Currently they would be before the rows assembled, but we would want to sort!
            }
        }
    },

    tocAssemble: function (timeout,excludeClasses) {
        // Launches tocAssembly on timer
        if (excludeClasses != undefined && excludeClasses.length > 0)
            tdbDoc.toc.excludeClasses = excludeClasses;
        else
            tdbDoc.toc.excludeClasses = [];
        if (timeout > 0)
            setTimeout('tdbDoc.toc.assemble();',timeout);
        else
            tdbDoc.toc.assemble();
    },

    polishButton: function (obj,openUp) {
        // Called to polish the toggle buttons after an action
        if (openUp) {
            obj.src='/images/remove_sm.gif';
            obj.alt='-';
            obj.title='hide details';
        } else {
            obj.src='/images/add_sm.gif';
            obj.alt='+';
            obj.title='show details';
        }
    },

    loadIntro: function (div) {
        // Called at startup to load each track type intro from the library
        var id = $(div).attr('id');
        var intro = tdbDoc.library.lookup(id,true);
        if (intro.length === 1) {
            $(intro).addClass("intro");
            $(intro).detach();
            $(div).replaceWith(intro);
        }
    },

    findDetails: function (td) {
        // Gets the setting description out of the library and puts it in place
        var id = $(td).attr('class');
        var details = tdbDoc.library.lookup(id,true);
        if (details.length === 1) {
            //var detail =  $(details).clone();
            var detail =  $(details).detach();
            //$(detail).attr('id','');  // keep unique ID
            $(detail).addClass("details");
            var format = $(detail).find("div.format");
            if (format.length === 1) {
                $(format).detach();
                $(td).find("div.format").replaceWith(format);
            }
            //$(details).clone().appendTo(td);
            //$(details).detatch();
            $(td).append(detail);
        }
        return $(td).find('div.details');
    },

    toggleDetails: function (obj,openUp) {
        // Called to toggle open or closed a single detail div or a related set
        var td = $(obj).parents('td')[0];
        var details = $(td).find('div.details');
        if (details == undefined || details.length !== 1) {
            details = tdbDoc.findDetails(td);
        }
        if (details == undefined || details.length > 1) {
            warn("Can't find details for id:" + td.id);
            return false;
        }
        details = details[0];

        // determine recursion
        var tr = $(obj).parents('tr')[0];
        var set = $(tr).hasClass('related') || $(tr).hasClass('related1st');
        if (openUp == undefined)
            openUp = ( obj.alt === '+' );
        else
            set = false; // called for set only if openOp is undefined

        // Handle recursion
        if (set) {
            var setClass = $( obj ).attr("class").replace(/ /g,".");

            var buttons = $(tr).parents('table').find("img." + setClass);
            $(buttons).each(function (ix) {
                tdbDoc.toggleDetails(this,openUp);
            });
            return true;
        }

        // Just one
        if (openUp)
            $(details).show();
        else
            $(details).hide();
        tdbDoc.polishButton(obj,openUp);
        return true;
    },

    toggleTable: function (obj,openUp) {
        // Called to toggle open or closed details in a table
        if (openUp == undefined)
            openUp = ( obj.alt === '+' );

        var buttons = $(obj).parents('table').find('img.toggle.detail');
        $(buttons).each(function (ix) {
            tdbDoc.toggleDetails(this,openUp);
        });
        tdbDoc.polishButton(obj,openUp);
        return true;
    },

    _toggleAll: function (obj,openUp) {
        // Called to toggle open or closed details in a table
        if (openUp == undefined)
            openUp = ( obj.alt === '+' );

        var buttons = $('img.toggle.oneSection');
        $(buttons).each(function (ix) {
            tdbDoc.toggleTable(this,openUp);
        });
        tdbDoc.polishButton(obj,openUp);
        return true;
    },

    toggleAll: function (obj,openUp) {
        waitOnFunction( tdbDoc._toggleAll, obj, openUp);
    },

    documentLoad: function () {
        // Called at $(document).ready() to load a trackDb document page
        var divIntros = $("div.intro").each( function (ix) {
            tdbDoc.loadIntro(this);
        });
        $('div.details').hide();
        $('img.toggle').each(function (ix) {
            var obj = this;
            tdbDoc.polishButton(obj,false);
            if ($(obj).hasClass('all')) {
                $(obj).click(function () {
                    return tdbDoc.toggleAll(obj);
                });
            } else if ($(obj).hasClass('oneSection')) {
                $(obj).click(function () {
                    return tdbDoc.toggleTable(obj);
                });
            } else {
                $(obj).click(function () {
                    return tdbDoc.toggleDetails(obj);
                });
            }
        });
        if (document.URL.search('#allOpen') !== -1)
            tdbDoc.toggleAll($('img.toggle.all')[0],true);
    }
}
