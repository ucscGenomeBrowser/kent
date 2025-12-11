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
            // Fill in tag 'level' if any from calling HTML doc
            var spec = $('div#specification').find('td.'+id);
            if (spec.length === 0) {
                return blurb;
            }
            var code = $(spec).find('code');
            if (code.length === 0) {
                return blurb;
            }
            var level = $(code).attr('class');
            if (level && level.length !== 0) {
                var start = $(blurb).find('p').first();
                if ($(start).attr('class') !== 'level') {
                    $(start).before('<p class="level">Support level: ' + '<span class=' +
                                    level + '>' + level.replace('level-','') + '</span></p>');
                }
                $(blurb).find('code').addClass(level);
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
                var classesTemp = this.getAttribute("class");
                var classes = classesTemp !== null ? classesTemp.split(" ") : [""];
                if (classes.length > 1 && onlyOne)
                    warn('obj with more than one class: '+ classes);
                else if (classes.length === 0 && onlyOne)
                    warn('obj ' + this.name + ' with no classes.');
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

        makePlainRow: function (aClass) {
            var level = 'level-new';
            var div = tdbDoc.library.lookup(aClass,false);
            if (div.length === 1) {
                level = $(div).find('code').attr('class');
                if (!level) {
                    level = 'level-new';
                }
            }
            return aClass + '\t' + level.replace('level-','') + '\n';
        },

        makeRow: function (aClass) {
            // Puts together a single row into a self assembling table of contents

            // Find types
            var types = "";
            var div = tdbDoc.library.lookup(aClass, false);
            var level = null;
            if (div.length === 1) {
                level = $(div).find('code').attr('class');
                if (!level) {
                    level = null;
                }
                var spanner = $(div).find('span.types');
                if (spanner.length === 1) {
                    var classes = $(spanner).attr("class").split(" ");
                    classes = aryRemove(classes,['types']);
                    $(classes).each(function (ix) {
                        types += " <A onclick='return jumpTo(this);' HREF='#'>" + this + "</a>";
                    });
                }
            }
            // Where documented (what table)?
            // TODO: Should rewrite classes array to also carry the table.
            var best = '';
            var td = $('td.' + aClass);
            var tbl;
            if (td.length > 0) // Always chooses first
                tbl = $(td[0]).parents('table.settingsTable');
            else
                tbl = $('table.settingsTable').has("a[name='"+aClass+"']");

            if (tbl != undefined && tbl.length === 1) {
                var id = $(tbl[0]).attr('id');
                if (id.length > 0) {
                    best = "<A HREF='#" + id + "'>" +id.replace(/_/g," ") + "</a>"
                    //if (td.length > 1)
                    //    best += " found "+td.length;
                }
            }
            var row = "<tr><td><A onclick='return jumpTo(this);' HREF='#'>" + aClass + "</a></td>";
            if (tdbDoc.isHubDoc()) {
                // hide superfluous 'Required' paragraphs
                $('p.isRequired').hide();

                //include level in the table
                if (level === null) {
                    // settings w/o a 'blurb' can still have a level (e.g. deprecated settings)
                    level = $(td[0]).find('code').attr('class');
                    if (typeof level === 'undefined') {
                        var span = $('span.' + aClass);
                            level = $(span[0]).find('code').attr('class');
                            if (typeof level === 'undefined') {
                                level = 'level-';
                        }
                    }
                }
                row += "<td class=" + level + ">" +level.replace('level-','') + "</span></td>";
            }
            row += "<td>" + types + "</td><td>" + best + "</td></tr>";
            return row;

        },

        excludeClasses: [],

        assemble: function () {
            // assembles (or extends) a table of contents if on is found (Launched by timer)
            var tocTable = $('table#toc');
            var plainTable = $('#plainToc');
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
                var cols = tdbDoc.isHubDoc() ? 4 : 3;
                if ($(tocTable).find('thead').length === 0) {
                    $(tocTable).prepend( "<THEAD><TR><TD colspan='" + cols + "'>"+
                               "<H3>Table of Contents</H3></TD></TR></THEAD>" );
                }
                $(tocTable).append("<tbody>");
                if ($(tocTable).find('th').length === 0) {
                    var th = "<TR VALIGN=TOP><TH WIDTH=100>Setting</TH>";
                    if (tdbDoc.isHubDoc()) {
                        th += "<TH>Level</TH>";
                    }
                    th += "<TH WIDTH='25%'>For Types</TH><TH>Documented</TH></TR>";
                    $(tocTable).append(th);
                }
                var lastClass = '';
                $(classes).each(function (ix) {
                    if (lastClass !== String(this)) {// skip duplicates
                        lastClass = String(this);
                        $(tocTable).append(tdbDoc.toc.makeRow(this));
                        $(plainTable).append(tdbDoc.toc.makePlainRow(this));
                    }
                });
                $(tocTable).append("</tbody>");
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
            setTimeout(tdbDoc.toc.assemble, timeout);
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


    isHubDoc: function () {
       return $('#trackDbHub_version').length !== 0;
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

        document.addEventListener("keydown", e => {
            if (e.key === "F3") {
                e.preventDefault();
                jumpToResult(e.shiftKey ? -1 : 1);
            }
        });


        // add the tdb setting to the img toggle classList
        let toggles = document.querySelectorAll("IMG.toggle.detail");
        toggles.forEach( (toggle) => {
            toggle.classList.add(toggle.parentNode.className + "_imgToggle");
        });
        let inp = document.getElementById("tdbSearch");
        inp.addEventListener("input", (e) => {
            let term = inp.value.trim();
            if (term.length >= 2) {
                runSearch(term);
            } else if (term.length === 1) {
                // Show helpful message for single character
                runSearch(term);
            } else if (term.length === 0) {
                // Clear search when input is empty
                clearSearchHighlights();
                hideSearchStatus();
                currentResults = [];
                currentIndex = -1;
            }
        });

        // Add keyboard shortcuts for search navigation
        inp.addEventListener("keydown", (e) => {
            if (e.key === "Enter") {
                e.preventDefault();
                if (currentResults.length > 0) {
                    jumpToResult(e.shiftKey ? -1 : 1);
                }
            } else if (e.key === "Escape") {
                e.preventDefault();
                clearSearch();
            }
        });

        // Add clear search button functionality
        let clearButton = document.getElementById("clearSearch");
        if (clearButton) {
            clearButton.addEventListener("click", () => {
                clearSearch();
            });
        }

        // Add jump to top functionality with smooth scrolling
        let jumpToTopButton = document.getElementById("jumpToTop");
        if (jumpToTopButton) {
            jumpToTopButton.addEventListener("click", (e) => {
                e.preventDefault();
                // Smooth scroll to top
                window.scrollTo({
                    top: 0,
                    behavior: "smooth"
                });
                // Also focus the search input for easy continued searching
                setTimeout(() => {
                    inp.focus();
                }, 500);
            });
        }
    }
}

function searchHidden(term) {
    // Reset previous search
    clearSearchHighlights();

    if (!term || term.length < 2) {
        return [];
    }

    let results = [];
    let escapedTerm = escapeRegexSpecialChars(term);

    // Strategy: Search ALL content and sort results by document position

    // First, collect all searchable elements
    let allElements = [];

    // 1. Add visible format divs (which contain code blocks with setting names)
    let formatDivs = document.querySelectorAll('div.format');
    formatDivs.forEach(element => {
        // Skip if this element is inside the library (which is hidden) or already in details
        if (!element.closest('#library') && !element.closest('div.details')) {
            allElements.push(element);
        }
    });

    // 2. Add standalone code and pre elements (not inside format divs or details)
    let codeElements = document.querySelectorAll('code, pre');
    codeElements.forEach(element => {
        // Skip if inside library, details, or format divs (already searched above)
        if (!element.closest('#library') && !element.closest('div.details') && !element.closest('div.format')) {
            allElements.push(element);
        }
    });

    // 3. Add other table content that might contain searchable text
    let tableCells = document.querySelectorAll('table.settingsTable td');
    tableCells.forEach(element => {
        // Skip if inside library or if it contains details/format (avoid duplication)
        if (!element.closest('#library') && !element.querySelector('div.details') && !element.querySelector('div.format')) {
            allElements.push(element);
        }
    });

    // 4. Add all already-opened details sections
    let existingDetails = document.querySelectorAll('div.details');
    existingDetails.forEach(div => {
        allElements.push(div);
    });

    // Sort elements by their position in the document (top to bottom)
    allElements.sort((a, b) => {
        let position = a.compareDocumentPosition(b);
        if (position & Node.DOCUMENT_POSITION_FOLLOWING) {
            return -1; // a comes before b
        } else if (position & Node.DOCUMENT_POSITION_PRECEDING) {
            return 1; // a comes after b
        }
        return 0; // same position
    });

    // Now search through elements in document order
    allElements.forEach(element => {
        let foundMatches = searchAndHighlightInElement(element, escapedTerm);
        if (foundMatches.length > 0) {
            results.push(...foundMatches);
        }
    });

    // 5. Find all toggle elements that might have hidden content not yet opened
    let toggles = [...document.querySelectorAll("[class]")].filter(el =>
        [...el.classList].some(c => c.endsWith("_imgToggle")));

    // Sort toggles by document position too
    toggles.sort((a, b) => {
        let position = a.compareDocumentPosition(b);
        if (position & Node.DOCUMENT_POSITION_FOLLOWING) {
            return -1;
        } else if (position & Node.DOCUMENT_POSITION_PRECEDING) {
            return 1;
        }
        return 0;
    });

    toggles.forEach((toggle) => {
        let detailsId = toggle.parentNode.className;
        if (!detailsId)
            return;

        let td = toggle.parentNode;
        let existingDetails = $(td).find('div.details');

        // Only search library content if details are not already visible
        if (existingDetails.length === 0) {
            let blurb = tdbDoc.library.lookup(detailsId, false);
            if (blurb && blurb.length > 0) {
                // Check if library content contains the search term
                let hasMatch = false;
                blurb.each((ix, div) => {
                    if (div.textContent && div.textContent.toLowerCase().includes(term.toLowerCase())) {
                        hasMatch = true;
                        return false; // break out of jQuery each
                    }
                });

                if (hasMatch) {
                    // Show the details to move content from library to document
                    tdbDoc.toggleDetails(toggle, true);

                    // Now search the newly visible content and insert results in correct position
                    let newDetails = $(td).find('div.details');
                    let newMatches = [];
                    newDetails.each((ix, docDiv) => {
                        let docMatches = searchAndHighlightInElement(docDiv, escapedTerm);
                        newMatches.push(...docMatches);
                    });

                    // Insert new matches in the correct position relative to existing results
                    if (newMatches.length > 0) {
                        // Find where to insert these results to maintain document order
                        let insertIndex = results.length;
                        for (let i = 0; i < results.length; i++) {
                            let position = newMatches[0].compareDocumentPosition(results[i]);
                            if (position & Node.DOCUMENT_POSITION_FOLLOWING) {
                                insertIndex = i;
                                break;
                            }
                        }
                        results.splice(insertIndex, 0, ...newMatches);
                    }
                }
            }
        }
    });

    return results;
}

function escapeRegexSpecialChars(str) {
    // Escape special regex characters to treat search term as literal text
    return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function clearSearchHighlights() {
    // More robust cleanup of previous search highlights
    document.querySelectorAll("mark").forEach(mark => {
        let parent = mark.parentNode;
        if (parent) {
            // Replace mark with its text content
            let textNode = document.createTextNode(mark.textContent);
            parent.replaceChild(textNode, mark);

            // Normalize to merge adjacent text nodes
            parent.normalize();
        }
    });
}

function clearHighlightsInElement(element) {
    // Clear highlights only within a specific element
    if (!element) return;

    let marks = element.querySelectorAll("mark");
    marks.forEach(mark => {
        let parent = mark.parentNode;
        if (parent) {
            let textNode = document.createTextNode(mark.textContent);
            parent.replaceChild(textNode, mark);
            parent.normalize();
        }
    });
}

function searchAndHighlightInElement(element, escapedTerm) {
    let results = [];
    let regex = new RegExp(`(${escapedTerm})`, "gi");

    // Create a tree walker to traverse only text nodes
    let walker = document.createTreeWalker(
        element,
        NodeFilter.SHOW_TEXT,
        null,
        false
    );

    let textNodes = [];
    let node;

    // Collect all text nodes first to avoid modifying tree during traversal
    while (node = walker.nextNode()) {
        if (node.textContent.trim().length > 0) {
            textNodes.push(node);
        }
    }

    // Process each text node
    textNodes.forEach(textNode => {
        let text = textNode.textContent;
        if (regex.test(text)) {
            // Create a document fragment to hold the new nodes
            let fragment = document.createDocumentFragment();
            let lastIndex = 0;
            let match;

            // Reset regex lastIndex for global search
            regex.lastIndex = 0;

            while ((match = regex.exec(text)) !== null) {
                // Add text before match
                if (match.index > lastIndex) {
                    fragment.appendChild(
                        document.createTextNode(text.substring(lastIndex, match.index))
                    );
                }

                // Add highlighted match
                let mark = document.createElement('mark');
                mark.textContent = match[1];
                fragment.appendChild(mark);
                results.push(mark);

                lastIndex = regex.lastIndex;

                // Prevent infinite loop on zero-length matches
                if (match.index === regex.lastIndex) {
                    regex.lastIndex++;
                }
            }

            // Add remaining text after last match
            if (lastIndex < text.length) {
                fragment.appendChild(
                    document.createTextNode(text.substring(lastIndex))
                );
            }

            // Replace the original text node with the fragment
            textNode.parentNode.replaceChild(fragment, textNode);
        }
    });

    return results;
}

let currentIndex = -1;
let currentResults = [];

function jumpToResult(direction) {
    if (currentResults.length === 0) {
        return;
    }

    currentIndex += direction;
    if (currentIndex >= currentResults.length) {
        currentIndex = 0;
    }
    if (currentIndex < 0) {
        currentIndex = currentResults.length - 1;
    }


    // Clear previous active results
    currentResults.forEach( (r) => {
        if (r && r.classList) {
            r.classList.remove("active-result");
        }
    });

    let target = currentResults[currentIndex];
    if (target && target.classList) {
        target.classList.add("active-result");

        // Ensure the element is visible and scroll to it
        scrollToResult(target);
    } else {
        console.log("Target result element not found or invalid");
    }
}

function scrollToResult(target) {
    if (!target) return;

    // Function to attempt scrolling with retries
    function attemptScroll(retries = 3) {
        if (retries <= 0) {
            console.log("Failed to scroll to result after retries");
            return;
        }

        // Check if the target is visible in the DOM
        if (target.offsetParent !== null) {
            console.log("Scrolling to result");
            target.scrollIntoView({behavior: "smooth", block: "center"});
        } else {
            // Target might be hidden, try to find its visible container
            let container = target.closest('div.details');
            if (container && container.offsetParent !== null) {
                console.log("Scrolling to result container");
                container.scrollIntoView({behavior: "smooth", block: "start"});
            } else {
                // Wait a bit and try again
                console.log(`Retrying scroll (${retries} retries left)`);
                setTimeout(() => attemptScroll(retries - 1), 50);
            }
        }
    }

    attemptScroll();
}

function runSearch(term) {

    // Clear previous results
    currentResults = [];
    currentIndex = -1;

    if (!term || term.length < 2) {
        // Show message for short terms
        if (term.length === 1) {
            showSearchStatus(term, 0);
        } else {
            hideSearchStatus();
        }
        return;
    }

    currentResults = searchHidden(term);

    // Always show search results count for valid searches
    showSearchStatus(term, currentResults.length);

    if (currentResults.length > 0) {
        // Small delay to ensure any new details sections are fully opened and rendered
        setTimeout(() => {
            jumpToResult(1);
        }, 150);
    }
}

function clearSearch() {
    let inp = document.getElementById("tdbSearch");
    if (inp) {
        inp.value = "";
    }
    clearSearchHighlights();
    hideSearchStatus();
    currentResults = [];
    currentIndex = -1;
    if (inp) {
        inp.blur();
    }
}

function showSearchStatus(term, count) {

    let statusDiv = document.getElementById('searchStatus');
    if (!statusDiv) {
        statusDiv = document.createElement('div');
        statusDiv.id = 'searchStatus';
        statusDiv.style.cssText = `
            margin: 5px 0;
            padding: 5px 10px;
            background-color: #e8f4f8;
            border: 1px solid #bee5eb;
            border-radius: 3px;
            font-size: 12px;
            color: #0c5460;
            display: block;
        `;

        let searchDiv = document.getElementById('searchDiv');
        if (searchDiv) {
            searchDiv.appendChild(statusDiv);
        } else {
            console.log('ERROR: Could not find searchDiv to append status');
        }
    }

    // Ensure the status div is visible
    statusDiv.style.display = 'block';

    if (count === 0) {
        if (term.length === 1) {
            statusDiv.textContent = `Type at least 2 characters to search`;
            statusDiv.style.backgroundColor = '#fff3cd';
            statusDiv.style.borderColor = '#ffeaa7';
            statusDiv.style.color = '#856404';
        } else {
            statusDiv.textContent = `No results found for "${term}"`;
            statusDiv.style.backgroundColor = '#f8d7da';
            statusDiv.style.borderColor = '#f5c6cb';
            statusDiv.style.color = '#721c24';
        }
    } else {
        statusDiv.textContent = `Found ${count} result${count !== 1 ? 's' : ''} for "${term}" - Use F3/Enter (Shift+F3/Shift+Enter) to navigate`;
        statusDiv.style.backgroundColor = '#e8f4f8';
        statusDiv.style.borderColor = '#bee5eb';
        statusDiv.style.color = '#0c5460';
    }
}

function hideSearchStatus() {
    let statusDiv = document.getElementById('searchStatus');
    if (statusDiv) {
        statusDiv.style.display = 'none';
    }
}

