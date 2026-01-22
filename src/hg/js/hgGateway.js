// hgGateway - autocomplete + graphical interface to select species, assembly & position.

// Copyright (C) 2016 The Regents of the University of California

// Several modules are defined in this file -- if some other code needs them someday,
// they can be moved out to lib files.

// function svgCreateEl: convenience function for creating new SVG elements, used by
// speciesTree and rainbow modules.

// speciesTree: module that exports draw() function for drawing a phylogenetic tree
// (and list of hubs above the tree, if any) in a pre-existing SVG element -- see
// hg/hgGateway/hgGateway.html.

// rainbow: module that exports draw() function and colors.  draw() adds stripes using
// a spectrum of colors that are associated to species groups.  The hgGateway view code
// uses coordinates of stripes within the tree image to create a corresponding "rainbow"
// slider bar to the left of the phylogenetic tree container.

// hgGateway: module of mostly view/controller code (model state comes directly from server).

// Globals (pragma for jshint):
/* globals dbDbTree, activeGenomes, surveyLink, surveyLabel, surveyLabelImage, cart */
/* globals autoCompleteCat */
/* globals calculateHgTracksWidth */ // function is defined in utils.js

window.hgsid = '';
window.activeGenomes = {};
window.surveyLink=null;
window.surveyLabel=null;
window.surveyLabelImage=null;

function setCopyLinks() {
  // add onclick to class 'copyLink' buttons, there could be more than one.
  addOnClick = function(){copyToClipboard(event);};
  copySpan = document.getElementsByClassName('copyLinkSpan');
  for (i = 0; i < copySpan.length; i++) {
    dataTarget = copySpan[i].getAttribute('data-target');
    hostName = window.location.hostname;
    targetSpan = document.getElementById(dataTarget);
    targetSpan.innerText = targetSpan.innerText.replace("http_host", hostName);
    aButton = document.createElement('button');
    aButton.type = "button";
    aButton.title = "Copy URL to clipboard";
    aButton.id = "copyIcon" + i;
    aButton.className = "copyLink";
    aButton.onclick = addOnClick;
    aButton.setAttribute('data-target', dataTarget);
    svgComment = document.createTextNode("<!--! Font Awesome Pro 6.1.1 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2022 Fonticons, Inc. -->");
    svgEl = document.createElementNS('http://www.w3.org/2000/svg','svg');
    svgEl.setAttributeNS(null, 'style', 'width:0.9em');
    svgEl.setAttributeNS('http://www.w3.org/2000/xmlns/', 'xmlns', 'http://www.w3.org/2000/svg');
    svgEl.setAttributeNS(null, 'viewBox', '0 0 512 512');
    pathEl = document.createElementNS("http://www.w3.org/2000/svg", "path");
    pathEl.setAttribute("d", 'M502.6 70.63l-61.25-61.25C435.4 3.371 427.2 0 418.7 0H255.1c-35.35 0-64 28.66-64 64l.0195 256C192 355.4 220.7 384 256 384h192c35.2 0 64-28.8 64-64V93.25C512 84.77 508.6 76.63 502.6 70.63zM464 320c0 8.836-7.164 16-16 16H255.1c-8.838 0-16-7.164-16-16L239.1 64.13c0-8.836 7.164-16 16-16h128L384 96c0 17.67 14.33 32 32 32h47.1V320zM272 448c0 8.836-7.164 16-16 16H63.1c-8.838 0-16-7.164-16-16L47.98 192.1c0-8.836 7.164-16 16-16H160V128H63.99c-35.35 0-64 28.65-64 64l.0098 256C.002 483.3 28.66 512 64 512h192c35.2 0 64-28.8 64-64v-32h-47.1L272 448z');
    svgEl.appendChild(svgComment);
    svgEl.appendChild(pathEl);
    buttonText = document.createTextNode(" Copy");
    aButton.append(svgEl);
    aButton.append(buttonText);
    copySpan[i].append(aButton);
  }     //      for (i = 0; i < copySpan.length; i++)
}       //      function setCopyLinks()

function svgCreateEl(type, config) {
    // Helper function for creating a new SVG element and initializing its
    // properties and attributes.  Type is something like 'rect', 'text', 'g', etc;
    // config is an object like { id: 'newThingie', x: 0, y: 10, title: 'blah blah' }.
    var svgns = 'http://www.w3.org/2000/svg';
    var xlinkns = 'http://www.w3.org/1999/xlink';
    var el = document.createElementNS(svgns, type);
    var title, titleEl;
    if (el) {
        _.forEach(config, function(value, setting) {
            if (setting === 'textContent') {
                // Text content (the text in a text element or title element) is a property:
                el.textContent = value;
            } else if (setting === 'href') {
                // href comes from a different namespace so must use setAttributeNS:
                el.setAttributeNS(xlinkns, 'href', value);
            } else if (setting === 'title') {
                title = value;
            } else if (setting === 'className') {
                el.setAttribute('class', value);
            } else {
                // Most of the time we're just setting an attribute:
                el.setAttribute(setting, value);
            }
        });
    }
    // Mouseover title actually requires creating a child element.
    // Strangely, if I did this in the above loop, the child element was lost if
    // props/attributes were set afterwards!!  So save title for last.
    if (title) {
        titleEl = document.createElementNS(svgns, 'title');
        titleEl.textContent = title;
        el.appendChild(titleEl);
    }
    return el;
}

///////////////////////////// Module: speciesTree /////////////////////////////

var speciesTree = (function() {
    // SVG phylogenetic tree of species in the browser (with connected assembly hubs above tree)

    // Layout parameters/configuration (object passed into draw can override these defaults):
    var cfg = { labelRightX: 230,
                labelStartY: 18,
                speciesLineOffsetX: 8,
                branchLength: 5,
                halfTextHeight: 4,
                hubLineOffset: 100,
                labelLineHeight: 18,
                paddingRight: 5,
                paddingBottom: 5,
                branchPadding: 2,
                trackHubsUrl: '',
                containerWidth: 370
              };

    function checkTree(node) {
        // Return true if node and its descendants are of the form
        // Array[ label:String, taxId:Number, sciName:String, Array[ [node]...] ]
        // e.g. ['root', taxId0, sciName0,
        //       [ ['leaf1', taxId1, sciName1, []],
        //         ['leaf2', taxId2, sciName2, []] ]
        //      ]
        if (! _.isString(node[0])) {
            console.log('label is not a string', node);
            return false;
        }
        if (! _.isNumber(node[1])) {
            console.log('taxId is not a number', node);
            return false;
        }
        if (! (node[2] === null || _.isString(node[2]))) {
            console.log('sciName is not null or string', node);
            return false;
        }
        return _.every(node[3], checkTree);
    }

    function addDepth(node) {
        // Each node is of the form [ label, taxId, sciName, node[] ]
        // e.g. ['root', taxId0, sciName0,
        //       [ ['leaf1', taxId1, sciName1, []],
        //         ['leaf2', taxId2, sciName2, []] ]
        //      ]
        // Add a fifth property to node: depth, i.e. the maximum number of
        // branching nodes along any path from node to a leaf.
        // Returns depth.
        var kids = node[3];
        var depth, deepestKid;
        if (!kids || kids.length === 0) {
            // Leaf: depth is 0
            depth = 0;
        } else if (kids.length === 1) {
            // Node with one child: pass-through, depth is child's depth
            depth = addDepth(kids[0]);
        } else {
            // Node with multiple children: depth is 1 + max child depth
            deepestKid = _.max(kids, addDepth);
            depth = 1 + deepestKid[4];
        }
        node[4] = depth;
        return depth;
    }

    function addSpeciesLabel(svg, label, taxId, sciName, y) {
        // Add a species label to svg at y offset
        var text = svgCreateEl('text', { x: cfg.labelRightX, y: y,
                                         id: 'textEl_' + taxId,
                                         name: 'textEl_' + taxId,
                                         title: sciName,
                                         textContent: label
			       	});
	// CSP2 will not allow setAttribute with events like onclick,
        // no matter whether the value is string or function.
        text.onclick = function(){hgGateway.onClickSpeciesLabel(taxId);};

        svg.appendChild(text);
    }

    function addLine(svg, x1, y1, x2, y2, mouseover) {
        // Add <line x1=x1, y1=y1, x2=x2, y2=y2 /> optionally with mouseover title
        var config = { x1: x1, y1: y1,
                       x2: x2, y2: y2,
                       title: mouseover };
        var line;
        if (x1 === x2) {
            // vertical lines get special styling
            config.className = 'vert';
        }
        line = svgCreateEl('line', config);
        svg.appendChild(line);
    }

    function calcBranchX(kidRetList, depth, parentDepth, parentNodeDepth) {
        // Return the x offset used for drawing lines from children and a vertical line
        // connecting them.
        // If the branch from parent is longer than the minimum length, we can scale myX
        // to reflect how many nodes are skipped in branches to the right vs. to the left,
        // ensuring the minimum branch length on either side.  Without this adjustment,
        // we sometimes get very long branches that skip only one node to the right of short
        // branches that skip many nodes.
        var myX;
        var kidMaxX = _.max(kidRetList, 'x').x;
        var branchSteps = parentDepth - depth;
        var kidMinNodeDepth, ratioSkipped, stepsX;
        if (branchSteps > 1) {
            kidMinNodeDepth = _.min(kidRetList, 'nodeDepth').nodeDepth;
            ratioSkipped = kidMinNodeDepth / (kidMinNodeDepth + parentNodeDepth);
            stepsX = (branchSteps+1) * ratioSkipped;
            // Make sure there is at least one step (min branch width) before and after
            // stepsX.
            if (stepsX < 1) {
                stepsX = 1;
            } else if (stepsX > branchSteps) {
                stepsX = branchSteps;
            }
            myX = kidMaxX + cfg.branchLength * stepsX;
        } else {
            // Only one change in depth level, so short branch:
            myX = kidMaxX + cfg.branchLength;
        }
        return myX;
    }

    function drawNode(svg, node, leafY, leafTops, parentDepth, parentNodeDepth) {
        // Each node is of the form [ label, taxId, sciName, node[], depth ]
        // e.g. ['root', taxId0, sciName0,
        //       [ ['leaf1', taxId1, sciName1, [], 0],
        //         ['leaf2', taxId2, sciName2, [], 0] ],
        //       1,
        //      ]
        // depth is the max number of branching nodes under this node.
        // Recursively draw the tree by generating new SVG elements and adding them to svg.
        // Returns  {x:, y:, leafY:} where x and y are the endpoint for the parent's branch
        // to this node and leafY is the Y offset for the next leaf label.
        // For leaf nodes, draw labels, store y in leafTops, and return label coordinates.
        // Nodes with a single child don't draw anything, they simply return the
        // child's info.
        // Nodes with multiple children draw a horizontal line to each child and
        // a vertical line connecting the horizontal lines.  They return the {X,Y} of
        // the midpoint of the vertical line.
        // optional arg parentDepth is the depth of the parent node as defined above.
        // optional arg parentNodeDepth is 1 plus the number of single-child nodes that were
        // skipped on the way from the last branching ancestor.
        // For internal use, the return object also includes nodeDepth: 1 + number of
        // nodes skipped between this node and the next branching descendant or leaf
        // and mouseover: horizontal line labels showing child label plus any skipped
        // nodes' labels.
        var label = node[0], taxId = node[1], sciName = node[2], kids = node[3], depth = node[4];
        var myX, myY;
        var kidRet, kidRetList, kidMinY, kidMaxY, extraSpace;
        parentDepth = parentDepth || 1;
        parentNodeDepth = parentNodeDepth || 1;
        if (!kids || kids.length === 0) {
            // leaf node: draw species label, store myY in leafTops
            addSpeciesLabel(svg, label, taxId, sciName, leafY);
            myX = cfg.labelRightX + cfg.speciesLineOffsetX;
            myY = leafY - cfg.halfTextHeight;
            leafTops[label] = leafY - cfg.labelLineHeight;
            return { x: myX, y: myY, leafY: leafY + cfg.labelLineHeight, nodeDepth: 1 };
        } else if (kids.length === 1) {
            // Single child: don't draw anything (keep the rendering compact),
            // but make a note that we skipped a node so we can use it for mouseover text
            kidRet = drawNode(svg, kids[0], leafY, leafTops, parentDepth, parentNodeDepth + 1);
            kidRet.nodeDepth++;
            if (kidRet.mouseover) {
                kidRet.mouseover += ' - ' + label;
            } else {
                kidRet.mouseover = label;
            }
            return kidRet;
        } else {
            // Multiple children.  First pass to draw kids and gather their coords,
            // second pass to draw lines connecting kids.
            extraSpace = (depth - 1) * cfg.branchPadding;
            kidRetList = _.map(kids, function(kid) {
                var kidRet = drawNode(svg, kid, leafY, leafTops, depth, 1);
                if (! kidRet.mouseover) {
                    // If nothing was skipped, use kid's sciName / label (same as kid's vert line)
                    var kidLabel = kid[0], kidSciName = kid[2];
                    kidRet.mouseover = kidSciName ? kidSciName : kidLabel;
                }
                leafY = kidRet.leafY + extraSpace;
                return kidRet;
            });
            myX = calcBranchX(kidRetList, depth, parentDepth, parentNodeDepth);
            // Draw horizontal lines from kids
            _.forEach(kidRetList, function(kidRet) {
                addLine(svg, kidRet.x, kidRet.y, myX, kidRet.y, kidRet.mouseover);
            });
            // Draw vertical line connecting kids
            kidMinY = kidRetList[0].y;
            kidMaxY = kidRetList[kids.length-1].y;
            addLine(svg, myX, kidMinY, myX, kidMaxY, label);
            myY = (kidMinY + kidMaxY) / 2;
            return { x: myX, y: myY, leafY: leafY, nodeDepth: 1, mouseover: label };
        }
    }

    function addTrackHubsLink(svg, x, y) {
        // Add a label with link to hgHubConnect at the given position.
        var a = svgCreateEl('a', { 'href': cfg.hgHubConnectUrl });
        var text = svgCreateEl('text', { x: x, y: y,
                                         textContent: 'Hub Genomes',
                                         className: 'trackHubsLink' });
        a.appendChild(text);
        svg.appendChild(a);
    }

    function doubleQuote(string) {
        return '"' + string + '"';
    }

    function addHubLabel(svg, hub, y) {
        // Add a track hub label to svg at y offset
        var label = hub.shortLabel + ' (' + hub.assemblyCount + ')';
        // There are a bunch of hub properties to pass to the onClick handler;
        // too bad we can't pass a bound function but instead must build a string:
        var text = svgCreateEl('text', { x: cfg.labelRightX, y: y,
                                         textContent: label,
                                         id: 'textEl_' + hub.name,
                                         name: 'textEl_' + hub.name,
                                         title: hub.longLabel
				     });
	text.onclick = function() {hgGateway.onClickHubName(hub.hubUrl, hub.taxId, hub.defaultDb, hub.name);};
        svg.appendChild(text);
    }

    function drawHubs(svg, hubList, yIn) {
        // Add a label for each hub in hubList, with a separator line below and
        // "Hub Genomes" link instead of a tree.
        var y = yIn;
        var hub, i, textX, textY, lineX1, lineY, lineX2;
        var countNonCurated = 0;
        if (hubList && hubList.length) {
            for (i = 0;  i < hubList.length;  i++) {
                hub = hubList[i];
                // is this a curated assembly hub? If so, don't list it
                if (!(hub.hubUrl.startsWith("/gbdb") && ! hub.hubUrl.startsWith("/gbdb/genark"))) {
                    addHubLabel(svg, hub, y);
                    y += cfg.labelLineHeight;
                    countNonCurated++;
                }
            }
            if (countNonCurated) {
                textX = cfg.labelRightX + cfg.speciesLineOffsetX;
                textY = (yIn + y - cfg.labelLineHeight) / 2;
                addTrackHubsLink(svg, textX, textY);
                lineX1 = cfg.hubLineOffset;
                lineY = y - cfg.halfTextHeight;
                lineX2 = cfg.containerWidth - cfg.speciesLineOffsetX;
                addLine(svg, lineX1, lineY, lineX2, lineY);
                y += cfg.labelLineHeight;
            }
        }
        return y;
    }

    function draw(svg, dbDbTree, hubList, cfgOverrides) {
        // dbDbTree is the root node of a phylogenetic tree that we render in svg.
        // cfgOverrides should be used to provide (names of) meaningful onclick functions
        // and track hub URL.
        // Return the width and height of the tree, the top offset of the tree (below hubs
        // if any), and an object with the top coords of each label/leaf.
        // Instead of tacking a bunch of children directly onto svg, make a <g> (group)
        // and append that to svg when done.
        // First see if there's already something there that we will replace:
        var oldG = svg.getElementById('hubsAndTree');
        var newG = svgCreateEl('g', { id: 'hubsAndTree' });
        // y offsets of tops of species labels (leaves of dbDbTree), filled in by drawNode.
        var leafTops = {};
        var hubBottomY, treeInfo, width, height;
        if (! checkTree(dbDbTree)) {
            console.error('dbDbTree in wrong format', dbDbTree);
            return;
        }
        _.assign(cfg, cfgOverrides);
        hubBottomY = drawHubs(newG, hubList, cfg.labelStartY);
        addDepth(dbDbTree);
        treeInfo = drawNode(newG, dbDbTree, hubBottomY, leafTops);
        width = treeInfo.x + cfg.paddingRight;
        if (width < cfg.containerWidth) {
            width = cfg.containerWidth;
        }
        height = treeInfo.leafY - cfg.labelLineHeight + cfg.paddingBottom;
        if (oldG) {
            svg.removeChild(oldG);
        }
        svg.appendChild(newG);
        return { width: width, height: height,
                 yTree: hubBottomY - cfg.labelLineHeight,
                 leafTops: leafTops };
    }

    return { draw: draw };

}()); // speciesTree


///////////////////////////// Module: rainbow /////////////////////////////

var rainbow = (function() {
    // Add rainbow stripes with cute species icons on the left of an SVG element that
    // already has a phylogenetic tree drawn by speciesTree.

    // Layout parameters/configuration (object passed into draw can override these defaults):
    var cfg = { stripeWidth: 69,
                iconX: 2,
                iconYOffset: -14,
                iconWidth: 65,
                iconHeight: 65,
                iconSpriteUrl: '../images/jWestIconsAlpha65px.png',
                iconSpriteWidth: 325,
                iconSpriteHeight: 325
                };

    // Color spectrum for slider and tree display:
    var stripeColors = [ '#7E1F16',
                         '#A12321',
                         '#C15026',
                         '#DF6933',
                         '#EB8734',
                         '#B57E2A',
                         '#CD9C2A',
                         '#CFB32B',
                         '#959E38',
                         '#3A8349',
                         '#216D6D',
                         '#4C749B',
                         '#31469A',
                         '#7E4475',
                         '#231F1F' ];

    // Hubs go above the species rainbow -- give them their own color of stripe:
    var hubColor = '#60180C';

    // Taxonomy IDs for assigning rainbow stripes to species groups:
    var stripeTaxIds = [ 9443,   // Primates
                         314146, // Euarchontoglires
                         9362,   // Insectivora
                         91561,  // Cetartiodactyla
                         314145, // Laurasiatheria
                         9397,   // Chiroptera
                         9347,   // Eutheria
                         40674,  // Mammalia
                         32523,  // Tetrapoda
                         7742,   // Vertebrata
                         33511,  // Deuterostomia
                         33392,  // Endopterygota
                         6072,   // Eumetazoa
                         2759,   // Eukaryota
                         1 ];    // root

    // Cute species icons placed along the stripe also help to orient.
    // This maps leaf labels to icon names:
    var iconSpeciesToName = { Human: 'Human',
                              Mouse: 'Mouse',
                              'D. melanogaster': 'Fly',
                              'C. elegans': 'Worm',
                              'S. cerevisiae': 'Yeast',
                              'Rhesus': 'Monkey',
                              Hedgehog: 'Hedgehog',
                              // Pig: 'Pig',
                              Cow: 'Cow',
                              'Killer whale': 'Orca',
                              Horse: 'Horse',
                              Dog: 'Dog',
                              'Pacific walrus': 'Walrus',
                              'Megabat': 'Bat',
                              Elephant: 'Elephant',
                              Manatee: 'Manatee',
                              Armadillo: 'Armadillo',
                              'Wallaby': 'Kangaroo',
                              'Zebra finch': 'Bird',
                              Lizard: 'Lizard',
                              'X. tropicalis': 'Frog',
                              'Fugu': 'Fish',
                              'Zaire ebolavirus': 'Ebola',  // on hgwdev April 2016
                              'Ebola virus': 'Ebola'        // on RR April 2016
                            };

    // The icon sprite image has 5 rows and 5 columns:
    var spriteRowCol = { Human: [0,0],
                         Mouse: [0,1],
                         Fly: [0,3],
                         Worm: [0,4],
                         Yeast: [1,0],
                         Monkey: [1,1],
                         Hedgehog: [1,2],
                         Pig: [1,3],
                         Cow: [1,4],
                         Orca: [2,0],
                         Horse: [2,1],
                         Dog: [2,2],
                         Walrus: [2,3],
                         Bat: [2,4],
                         Elephant: [3,0],
                         Manatee: [3,1],
                         Armadillo: [3,2],
                         Kangaroo: [3,3],
                         Bird: [3,4],
                         Lizard: [4,0],
                         Frog: [4,1],
                         Fish: [4,2],
                         Ebola: [4,3]
                       };

    // Some icon drawings are shorter than others, and some need to be moved up to make space
    // for close neighbors.
    var iconFudgeY = { Human: 20,
                         Mouse: 0,
                         Fly: 0,
                         Worm: 0,
                         Monkey: 0,
                         Hedgehog: -18,
                         Pig: -5,
                         Cow: 0,
                         Orca: 0,
                         Horse: -8,
                         Dog: 15,
                         Walrus: 40,
                         Bat: 0,
                         Elephant: -20,
                         Manatee: 20,
                         Armadillo: 0,
                         Kangaroo: 0,
                         Bird: 0,
                         Lizard: -20,
                         Frog: -10,
                         Fish: 0,
                         Yeast: -15,
                         Ebola: 0
                       };

    function findStripeTops(node, parentStripeIx, leafTops, stripeTops, yPrev) {
        // Each node is of the form [ label, taxId, sciName, node[], ... ]
        // Recursively find the top coordinate of each stripe in stripeTaxIds,
        // using node taxId.  Modifies stripeTops.  Returns the y of the top of the
        // last leaf visited.
        var label = node[0], taxId = node[1], kids = node[3];
        var stripeIx = stripeTaxIds.indexOf(taxId);
        var i;
        // Inherit parent stripe unless this node is found in stripeTaxIds:
        if (stripeIx < 0) {
            stripeIx = parentStripeIx;
        }
        if (!kids || kids.length === 0) {
            // leaf node: if this stripe's top coord has not yet been assigned,
            // assign it.
            if (stripeTops[stripeIx] === undefined) {
                stripeTops[stripeIx] = (leafTops[label] + yPrev) / 2;
            }
            yPrev = leafTops[label];
        } else {
            // descend to children
            for (i = 0;  i < kids.length;  i++) {
                yPrev = findStripeTops(kids[i], stripeIx, leafTops, stripeTops, yPrev);
            }
        }
        return yPrev;
    }

    function addRectFill(svg, x, y, width, height, color) {
        // Add filled rectangle to svg
        var rect = svgCreateEl('rect', { x: x, y: y,
                                         width: width, height: height,
                                         style: 'fill:' + color + '; stroke:' + color });
        svg.appendChild(rect);
    }

    function drawStripes(svg, dbDbTree, yTop, height, leafTops) {
        var stripeCount = stripeColors.length;
        var lastStripeIx = stripeCount - 1;
        var stripeTops = [];
        var i, stripeHeight;
        findStripeTops(dbDbTree, lastStripeIx, leafTops, stripeTops, yTop);
        // Add an extra "stripe" coord so we have the coord for the bottom of the last stripe:
        stripeTops[stripeCount] = height;
        // Initialize missing stripes to 0-height (top = next stripe's top), if any:
        for (i = stripeCount - 1;  i >= 0;  i--) {
            if (stripeTops[i] === undefined) {
                stripeTops[i] = stripeTops[i+1];
            }
        }
        for (i = 0;  i < stripeCount;  i++) {
            stripeHeight = stripeTops[i+1] - stripeTops[i];
            addRectFill(svg, 0, stripeTops[i], cfg.stripeWidth, stripeHeight, stripeColors[i]);
        }
        // Add stripe for hubs, if any:
        if (yTop > 0) {
            addRectFill(svg, 0, 0, cfg.stripeWidth, yTop, hubColor);
        }
        return stripeTops;
    }

    function drawOneIcon(svg, name, y) {
        // Create an image, offset so that the icon is positioned where we need it,
        // and use a clip-path to limit display to just that icon, not the whole sprite image.
        var iconY = y + cfg.iconYOffset + iconFudgeY[name];
        var rowCol = spriteRowCol[name];
        var row = rowCol[0], column = rowCol[1];
        var clipPathId = 'clip' + name;
        var img = svgCreateEl('image', { x: cfg.iconX - (column * cfg.iconWidth),
                                         y: iconY - (row * cfg.iconHeight),
                                         width: cfg.iconSpriteWidth,
                                         height: cfg.iconSpriteHeight,
                                         style: 'clip-path: url(#' + clipPathId + ')',
                                         href: cfg.iconSpriteUrl });
        // Set the y of the pre-existing clip path:
        var rect = $('#' + clipPathId + ' rect')[0];
        rect.setAttribute('y', iconY);
        svg.appendChild(img);
    }

    function drawIcons(svg, leafTops) {
        // For each icon listed in iconSpeciesToName, look up the species' y offset in
        // the tree and add the icon to svg.
        _.forEach(iconSpeciesToName, function (name, species) {
            var y = leafTops[species];
            if (y >= 0) {
                drawOneIcon(svg, name, y);
            }
        });
    }

    function draw(svg, dbDbTree, yTree, height, leafTops) {
        // Draw stripes with colors corresponding to species groups and cute-species icons.
        // Return y offsets of stripes so that a slider widget can be drawn accordingly.
        // Instead of tacking a bunch of children directly onto svg, make a <g> (group)
        // and append that to svg when done.
        // First see if there's already something there that we will replace:
        var oldG = svg.getElementById('stripesAndIcons');
        var newG = svgCreateEl('g', { id: 'stripesAndIcons' });
        var stripeTops = drawStripes(newG, dbDbTree, yTree, height, leafTops);
        drawIcons(newG, leafTops);
        if (oldG) {
            svg.removeChild(oldG);
        }
        svg.appendChild(newG);
        return stripeTops;
    }

    return { draw: draw,
             colors: stripeColors,
             hubColor: hubColor
             };
}()); // rainbow


///////////////////////////// Module: hgGateway /////////////////////////////

var hgGateway = (function() {
    // Interactive parts of the new gateway page: species autocomplete,
    // graphical species-picker, db select, and position autocomplete.

    // Constants
    var speciesWatermark = 'Enter species, common name or assembly ID';
    var positionWatermark = 'Enter position, gene symbol or search terms';
    // Shortcuts to popular species:
    var favIconTaxId = [ ['Human', 9606],
                         ['Mouse', 10090],
                         ['Rat', 10116],
                         ['Zebrafish', 7955],
                         ['Fly', 7227],
                         ['Worm', 6239],
                         ['Yeast', 559292] ];
    // Aliases for species autocomplete:
    var commonToSciNames = { bats: 'Chiroptera',
                             bees: 'Apoidea',
                             birds: 'Aves',
                             fish: 'Actinopterygii',
                             fly: 'Diptera',
                             flies: 'Diptera',
                             frogs: 'Anura',
                             fruitfly: 'Drosophila',
                             'fruit fly': 'Drosophila',
                             honeybees: 'Apinae',
                             'honey bees': 'Apinae',
                             monkeys: 'Simiiformes',
                             mosquitos: 'Culicidae',
                             worms: 'Nematoda',
                             yeast: 'Ascomycota' };

    var getBetterBrowserMessage = '<P style="padding-left: 10px;">' +
                                  'Our website has detected that you are using ' +
                                  'an outdated browser that will prevent you from ' +
                                  'accessing certain features. An update is not ' +
                                  'required, but it is strongly recommended to ' +
                                  'improve your browsing experience. ' +
                                  'Please use the following links to upgrade your ' +
                                  'existing browser to ' +
                                  '<A HREF="https://www.mozilla.org/en-US/firefox/new/">' +
                                  ' FireFox</A> or ' +
                                  '<A HREF="https://www.google.com/chrome/browser/">' +
                                  'Chrome</A>.' +
                                  '</P>';

    // Globals (within this function scope)
    // Set this to true to see server requests and responses in the console.
    var debugCartJson = false;
    // This is a global (within wrapper function scope) so event handlers can use it
    // without needing to bind functions.
    var scrollbarWidth = 0;
    // This holds everything we need to know to draw the page: taxId, db, hubs, description etc.
    var uiState = {};
    // This is used to check whether a taxId is found in activeGenomes:
    var activeTaxIds = [];  // gets set in init() now;
    // This is dbDbTree after pruning -- null if dbDbTree has no children left
    var prunedDbDbTree = null;
    // This keeps track of which gene the user has selected most recently from autocomplete.
    var selectedGene = null;

    function setupFavIcons() {
        // Set up onclick handlers for shortcut buttons and labels
        var haveIcon = false;
        var i, name, taxId, onClick;
        for (i = 0;  i < favIconTaxId.length;  i++) {
            name = favIconTaxId[i][0];
            taxId = favIconTaxId[i][1];
            if (activeTaxIds[taxId]) {
                // When user clicks on icon, set the taxId (default database);
                // scroll the image to that species and clear the species autocomplete input.
                onClick = setTaxId.bind(null, taxId, null, null, true, true);
                // Onclick for both the icon and its sibling label:
                $('.jwIconSprite' + name).parent().children().on("click", onClick);
                haveIcon = true;
            } else {
                // Inactive on this site -- hide it
                $('.jwIconSprite' + name).parent().hide();
            }
        }
        if (! haveIcon) {
            $('#popSpeciesTitle').text('Species Search');
        }
    }

    function addCategory(cat, item) {
        // Clone item, add category: cat to it and return it (helper function, see below).
        var clone = {};
        _.assign(clone, item, { category: cat });
        return clone;
    }

    function autocompleteFromNode(node) {
        // Traverse dbDbTree to make autocomplete result lists for all non-leaf node labels.
        // Returns an object mapping each label of node and descendants to a list of
        // result objects with the same structure that we'd get from a server request.
        if (! node) {
            return;
        }
        var searchObj = {};
        var myResults = [];
        var label = node[0], taxId = node[1], kids = node[3];
        var addMyLabel;
        if (!kids || kids.length === 0) {
            // leaf node: return autocomplete result for species
            myResults = [ { genome: label,
                            label: label,
                            taxId: taxId } ];
        } else {
            // Accumulate the list of all children's result lists;
            // keep each's child searchObj mappings unless the child is a leaf
            // (which would be redundant with server autocomplete results).
            addMyLabel = addCategory.bind(null, label);
            myResults = _.flatten(
                _.map(kids, function(kid) {
                    var kidLabel = kid[0], kidKids = kid[3];
                    var kidObj = autocompleteFromNode(kid);
                    // Clone kid's result list and add own label as category:
                    var kidResults = _.map(kidObj[kidLabel], addMyLabel);
                    // Add kid's mappings to searchObj only if kid is not a leaf.
                    if (kidKids && kidKids.length > 0) {
                        _.assign(searchObj, kidObj);
                    }
                    return kidResults;
                })
            );
        }
        // Exclude some overly broad categories:
        if (label !== 'root' && label !== 'cellular organisms') {
            searchObj[label] = myResults;
        }
        return searchObj;
    }

    function autocompleteFromTree(node, searchObj) {
        // Traverse dbDbTree to make autocomplete result lists for all non-leaf node labels.
        // searchObj is extended to map each label of node and descendants to a list of
        // result objects with the same structure that we'd get from a server request.
        _.assign(searchObj, autocompleteFromNode(node));
        // Add aliases for some common names that map to scientific names in the tree.
        _.forEach(commonToSciNames, function(sciName, commonName) {
            var label, addMyLabel;
            if (searchObj[sciName]) {
                label = sciName + ' (' + commonName + ')';
                addMyLabel = addCategory.bind(null, label);
                searchObj[commonName] = _.map(searchObj[sciName], addMyLabel);
            }
        });
    }

    function makeStripe(id, color, stripeHeight, scrollTop, onClickStripe) {
        // Return an empty div with specified background color and height
        var $stripe = $('<div class="jwRainbowStripe">');
        $stripe.attr('id', 'rainbowStripe' + id);
        $stripe.attr('title', 'Click to scroll the tree display');
        $stripe.css('background-color', color);
        $stripe.height(stripeHeight);
        $stripe.on("click", onClickStripe.bind(null, scrollTop));
        return $stripe;
    }

    function makeRainbowSliderStripes($slider, onClickStripe, svgHeight, stripeColors, stripeTops) {
        // Set up the rainbow slider bar for the speciesPicker.
        // The stripeColors array determines the number of stripes and their colors.
        // stripeTops contains the pixel y coordinates of the top of each stripe;
        // we convert these to pixel heights of HTML div stripes.
        // onClickStripe is bound to the normalized top coord of each stripe
        // (the ratio of stripe's top coord to slider bar height, in the range
        // 0.0 to (1.0 - 1/stripeColors.length)).
        var i, $stripe, scrollTop, stripeHeight, svgStripeHeight;
        var sliderHeight = $('#speciesPicker').outerHeight();
        $slider.empty();
        if (stripeTops[0] > 0) {
            // Add a placeholder stripe for hubs at the top
            stripeHeight = sliderHeight * stripeTops[0] / svgHeight;
            $stripe = makeStripe('Hub', rainbow.hubColor, stripeHeight, 0, onClickStripe);
            $slider.append($stripe);
        }
        for (i = 0;  i < stripeColors.length;  i++) {
            svgStripeHeight = stripeTops[i+1] - stripeTops[i];
            stripeHeight = sliderHeight * svgStripeHeight / svgHeight;
            scrollTop = stripeTops[i] / svgHeight;
            $stripe = makeStripe(i, stripeColors[i], stripeHeight, scrollTop, onClickStripe);
            $slider.append($stripe);
        }
    }

    function resizeSliderIcon($sliderIcon, svgHeight, sliderBarHeight) {
        // Make the slider icon's height cover the same portion of the slider bar
        // as the visible part of the tree is compared to the entire tree image height.
        var visiblePortion = _.min([1, sliderBarHeight / svgHeight]);
        var iconHeight = visiblePortion * sliderBarHeight;
        // Set the icon rectangle's height and triangle vertical offset.
        var svg = document.getElementById('sliderSvg');
        var rect = document.getElementById('sliderRectangle');
        var tri = document.getElementById('sliderTriangle');
        var strokeWidth = 3, triangleHeight = 6;
        svg.setAttribute('height', iconHeight);
        rect.setAttribute('height', iconHeight - strokeWidth);
        tri.setAttribute('d', 'm 2.5,' + ((iconHeight - triangleHeight) / 2) + ' 0,6 4,-3 z');
        $sliderIcon.height(iconHeight);
    }

    function initRainbowSlider(svgHeight, stripeColors, stripeTops) {
        // Once we know the height of the hubs & tree image, initialize the rainbow slider
        // widget for coordinated scrolling.  Dragging the slider causes the image to scroll.
        // Scrolling the image causes the slider to move.  Clicking on a stripe causes the
        // image to scroll and the slider to move.
        var $speciesPicker = $('#speciesPicker');
        var $sliderBar = $('#rainbowSlider');
        var sliderBarTop = $sliderBar.offset().top;
        var sliderBarHeight = $speciesPicker.outerHeight();
        var $sliderIcon = $('#sliderSvgContainer');
        var sliderIconLeft = $sliderIcon.offset().left;
        var $speciesTree = $('#speciesTree');
        // When the user moves the slider, causing the image to scroll, don't do the
        // image onscroll action (do that only when the user scrolls the image).
        var inhibitImageOnScroll = false;
        // Don't let the slider hang off the bottom when the user clicks the bottom stripe:
        var maxNormalizedTop = 1 - (sliderBarHeight / svgHeight);

        // Define several helper functions within this function scope so they can use
        // the variables defined above.
        var scrollImage = function(normalizedTop) {
            // Scroll the hubs+tree image to a normalized top coord scaled by svgHeight.
            $speciesPicker.scrollTop(svgHeight * normalizedTop);
        };

        var moveSlider = function(normalizedTop) {
            // Move the slider icon to a normalized top coord scaled by sliderBarHeight.
            $sliderIcon.offset({ top: sliderBarTop + (normalizedTop * sliderBarHeight),
                                 left: sliderIconLeft.left });
        };

        var onClickStripe = function(normalizedTop) {
            // The user clicked a stripe; move the slider to the top of that stripe and
            // scroll the tree image to the top of the corresponding stripe in the image.
            inhibitImageOnScroll = true;
            if (normalizedTop > maxNormalizedTop) {
                normalizedTop = maxNormalizedTop;
            }
            scrollImage(normalizedTop);
            moveSlider(normalizedTop);
        };

        var onDragSlider = function(event, ui) {
            // The user dragged the slider; scroll the tree image to the corresponding
            // position.
            var sliderTop = ui.offset.top - sliderBarTop;
            var normalizedTop = sliderTop / sliderBarHeight;
            inhibitImageOnScroll = true;
            scrollImage(normalizedTop);
        };

        var onScrollImage = function() {
            // The user scrolled the image -- or the user did something else which caused
            // the image to scroll, in which case we don't need to do anything more.
            var imageTop, normalizedTop;
            if (inhibitImageOnScroll) {
                inhibitImageOnScroll = false;
                return;
            }
            imageTop = -$speciesTree.offset().top + sliderBarTop + 1;
            normalizedTop = imageTop / svgHeight;
            moveSlider(normalizedTop);
        };

        // This might be called before the species image has been created; if so, do nothing.
        if (! $speciesTree || speciesTree.length === 0) {
            return;
        }

        makeRainbowSliderStripes($sliderBar, onClickStripe, svgHeight, stripeColors, stripeTops);
        resizeSliderIcon($sliderIcon, svgHeight, sliderBarHeight);
        $sliderIcon.draggable({ axis: 'y',
                                containment: '#speciesGraphic',
                                drag: onDragSlider
                                });
        $speciesPicker.on("scroll", onScrollImage);
    }

    function findScrollbarWidth() {
        var widthPlain = $("#sbTestContainerDPlain").width();
        var widthInsideScroll = $("#sbTestContainerDInsideScroll").width();
        $('#sbTestContainer').hide();
        return widthPlain - widthInsideScroll;
    }

    function updateGoButtonPosition() {
        // If there's enough room for the Go button to be to the right of the inputs,
        // set its height to the midpoint of theirs.
        var $goButton = $('.jwGoButtonContainer');
        var goOffset = $goButton.offset();
        var menuOffset = $('#selectAssembly').offset();
        var inputOffset = $('#positionInput').offset();
        var verticalMidpoint = (menuOffset.top + inputOffset.top) / 2;
        if (goOffset.left > inputOffset.left) {
            $goButton.offset({top: verticalMidpoint });
        } else {
            // If the window shrinks and there's no longer room for the button, undo the above.
            $goButton.css('top', 0);
        }
    }

    function setRightColumnWidth() {
        // Adjust the width of the "Find Position" section so it fits to the right of the
        // "Browse/Select Species" section.
        var ieFudge = scrollbarWidth ? scrollbarWidth + 4 : 0;
        var extraFudge = 4;
        var $contents = $('#findPositionContents');
        var sectionContentsPadding = (_.parseInt($contents.css("padding-left")) +
                                      _.parseInt($contents.css("padding-right")));
        var rightColumnWidth = ($('#pageContent').width() -
                                $('#selectSpeciesSection').width() -
                                ieFudge - extraFudge);
        if (rightColumnWidth >= 400) {
            $('#findPositionSection').width(rightColumnWidth);
        }
        updateGoButtonPosition();
        $('#findPositionTitle').outerWidth(rightColumnWidth + extraFudge);
        $('#descriptionTitle').outerWidth(rightColumnWidth - sectionContentsPadding);
    }

    function setSpeciesPickerSizes(svgWidth, svgHeight) {
        // Adjust widths and heights of elements in #speciesPicker according to svg size.
        $('#speciesTree').width(svgWidth);
        $('#speciesTree').height(svgHeight);
        $('#speciesTreeContainer').height(svgHeight);
        // Make #speciesTreeContainer skinnier if a scrollbar is taking up space
        // in #speciesPicker.
        var leftover = ($("#speciesPicker").width() - scrollbarWidth);
        $("#speciesTreeContainer").width(leftover);
    }

    function highlightLabel(selectedName, scrollToItem) {
        // Highlight the selected species.
        // jQuery (at least our old version of it) can find the SVG text elements but can't
        // directly manipulate their class, so do it manually.
        var $sp = $('#speciesPicker');
        var y = $sp.scrollTop();
        $('svg text').each(function(ix, el) {
            var elName = el.getAttribute('name');
            var elClass = el.getAttribute('class');
            if (!elClass || elClass.indexOf('trackHubsLabel') < 0) {
                if (elName === selectedName) {
                    el.setAttribute('class', 'selected');
                    y = el.getAttribute('y');
                } else if (elClass === 'selected') {
                    el.setAttribute('class', '');
                }
            }
        });
        if (scrollToItem) {
            $sp.scrollTop(y - 100);
        }
    }

    function hubNameFromDb(db) {
        var matches = db ? db.match(/^(hub_[0-9]+)_/) : null;
        if (matches) {
            return matches[1];
        } else {
            return null;
        }
    }

    function highlightLabelForDb(db, taxId) {
        var hubName = hubNameFromDb(db);
        if (hubName) {
            highlightLabel('textEl_' + hubName, true);
        } else {
            highlightLabel('textEl_' + taxId, true);
        }
    }

    function pruneInactive(node, activeGenomes, activeTaxIds) {
        // Return true if some leaf descendant of node is in activeGenomes or activeTaxIds.
        // Remove any child that returns false.
        // If one of {genome, taxId} matches but not the other, tweak the other to match dbDb,
        // Since we'll be using the hgwdev dbDbTree on the RR which may have been tweaked.
        var genome = node[0], taxId = node[1], kids = node[3];
        var hasActiveLeaf = false, i, dbDbTaxId, dbDbGenome;
        if (!kids || kids.length === 0) {
            // leaf node: is it active?
            dbDbTaxId = activeGenomes[genome];
            if (dbDbTaxId) {
                hasActiveLeaf = true;
                node[1] = dbDbTaxId;
            }
            // Yet another special case for Baboon having one genome with two species...
            // maybe we should just change dbDb?
            else if (_.startsWith(genome, 'Baboon ') && (taxId === 9555 || taxId === 9562) &&
                     activeGenomes.Baboon) {
                hasActiveLeaf = true;
            } else {
                dbDbGenome = activeTaxIds[taxId];
                if (dbDbGenome) {
                    hasActiveLeaf = true;
                    node[0] = dbDbGenome;
                }
            }
        } else {
            // parent node: splice out any child nodes with no active leaves
            for (i = kids.length - 1;  i >= 0;  i--) {
                if (pruneInactive(kids[i], activeGenomes, activeTaxIds)) {
                    hasActiveLeaf = true;
                } else {
                    kids.splice(i, 1);
                }
            }
        }
        return hasActiveLeaf;
    }

    function drawSpeciesPicker(dbDbTree) {
        // If dbDbTree is nonempty and SVG is supported, draw the tree; if SVG is not supported,
        // use the space to suggest that the user install a better browser.
        // If dbDbTree doesn't exist, leave the "Represented Species" section hidden.
        var svg, spTree, stripeTops;
        if (dbDbTree) {
            if (document.createElementNS) {
                // Draw the phylogenetic tree and do layout adjustments
                svg = document.getElementById('speciesTree');
                spTree = speciesTree.draw(svg, dbDbTree, uiState.hubs,
                                          { hgHubConnectUrl: 'hgHubConnect?hgsid=' + window.hgsid,
                                            containerWidth: $('#speciesPicker').width()
                                            });
                setSpeciesPickerSizes(spTree.width, spTree.height);
                stripeTops = rainbow.draw(svg, dbDbTree,
                                          spTree.yTree, spTree.height, spTree.leafTops);
            } else {
                $('#speciesTreeContainer').html(getBetterBrowserMessage);
            }
            $('#representedSpeciesTitle').show();
            $('#speciesGraphic').show();
            if (dbDbTree && document.createElementNS) {
                // These need to be done after things are visible because heights are 0 when hidden.
                highlightLabelForDb(uiState.db, uiState.taxId);
                initRainbowSlider(spTree.height, rainbow.colors, stripeTops);
            }
        }
    }

    function addCommasToPosition(pos) {
        // Return seqName:start-end pos with commas inserted in start and end as necessary.
        var posComma = pos;
        var fourDigits = /(^.*:.*[0-9])([0-9]{3}\b.*)/;
        var matches = fourDigits.exec(posComma);
        while (matches) {
            posComma = matches[1] + ',' + matches[2];
            matches = fourDigits.exec(posComma);
        }
        return posComma;
    }

    function onSelectGene(item) {
        // Set the position from an autocomplete result;
        // set hgFindMatches and make sure suggestTrack is in pack mode for highlighting the match.
        var newPos = item.id;
        var newPosComma = addCommasToPosition(newPos);
        var settings;
        $('#positionDisplay').text(newPosComma);
        function onSuccess(jqXHR, textStatus) {
            goToHgTracks();
        }
        function onFail(jqXHR, textStatus) {
            cart.defaultErrorCallback(jqXHR, textStatus);
        }
        if (uiState.suggestTrack) {
            settings = { 'hgFind.matches': item.internalId };
            settings[uiState.suggestTrack] = 'pack';
            cart.send({ cgiVar: settings }, onSuccess , onFail);
            cart.flush();
        }
        function overwriteWithGene() {
            $('#positionInput').val(item.geneSymbol);
        }
        if (item.geneSymbol) {
            selectedGene = item.geneSymbol;
            // Overwrite item's long value with symbol after the autocomplete plugin is done:
            window.setTimeout(overwriteWithGene, 0);
        } else {
            selectedGene = item.value;
        }
    }

    function onSelectOther(item) {
        function overwriteWithGene() {
            $('#positionInput').val(item.geneSymbol);
        }
        if (item.geneSymbol) {
            selectedGene = item.geneSymbol;
            // Overwrite item's long value with symbol after the autocomplete plugin is done:
            window.setTimeout(overwriteWithGene, 0);
        } else {
            selectedGene = item.value;
        }
        window.location.assign(item.id);
    }

    function onSelect(item) {
        // Switch out to the right type of select function for the type of category
        // match: track hits (including genes), help page, etc
        if (["helpDocs", "publicHubs", "trackDb"].includes(item.type) ||
                item.id.startsWith("hgc")) {
            onSelectOther(item);
        } else {
            onSelectGene(item);
        }
    }

    function setAssemblyOptions(uiState) {
        var assemblySelectLabel = 'Assembly';
        if (uiState.dbOptions) {
            var html = '', option, i, selected;
            for (i = 0;  i < uiState.dbOptions.length;  i++) {
                option = uiState.dbOptions[i];
                selected = (option.value === uiState.db) ? 'selected ' : '';
                html += '<option ' + selected + 'value="' + option.value + '">' +
                        option.label + '</option>';
            }
            $('#selectAssembly').html(html);
        }
        if (uiState.genomeLabel) {
            if (uiState.hubUrl && uiState.genomeLabel.indexOf('Hub') < 0) {
                assemblySelectLabel = uiState.genomeLabel + ' Hub Assembly';
            } else {
                assemblySelectLabel = uiState.genomeLabel + ' Assembly';
            }
        }
        $('#selectAssemblyLabel').text(assemblySelectLabel);
    }

    function setAssemblyDescriptionTitle(db, genome) {
        $('#descriptionGenome').html(trackHubSkipHubName(genome));
        $('#descriptionDb').html(trackHubSkipHubName(db));
    }

    function tweakDescriptionPhotoWidth() {
        // Our description.html files assume a pretty wide display area, but now we're
        // squeezed to the right of the 'Select Species' section.  If there's a large
        // image, scale it down.  The enclosing table is usually sized to leave a lot
        // of space to the left of the image, so shrink that too.
        // This must be called *after* #descriptionText is updated with the new content.
        var width, scaleFactor, newWidth;
        var $table = $('#descriptionText table').first();
        var $img = $('#descriptionText table img').first();
        if ($img.length) {
            width = $img.width();
            if (width > 175) {
                // Scale to 150px wide, preserving aspect ratio
                newWidth = 150;
                scaleFactor = newWidth / width;
                $img.width(newWidth)
                    .height($img.height() * scaleFactor);
                width = newWidth;
            }
            if ($table.width() - width > 20) {
                $table.width(width + 10);
            }
            // hg19's description.html sets a height for its table that pushes the
            // links section down; unneeded & unwanted here, so remove height if set:
            $table.removeAttr('height');
        }
    }

    function updateDescription(description) {
        // We got the contents of a db's description.html -- tweak its format to fit
        // the new design.
        $('#descriptionText').html(description);
        tweakDescriptionPhotoWidth();
        // Apply JWest formatting to all anchors in description.
        // We can't simply style all <a> tags that way because autocomplete uses <a>'s.
        $('#descriptionText a').addClass('jwAnchor');
        // Apply square bullet style to all ul's in description.
        $('#descriptionText ul').addClass('jwNoBullet');
        $('#descriptionText li').addClass('jwSquareBullet');
        setCopyLinks();
    }

    function initFindPositionContents() {
        // Unhide contents of Find Position section and adjust layout.
        $('#findPositionContents').show();
        // Set assembly menu's width to same as position input.
        var posWidth = $('#positionInput').outerWidth();
        var $select = $('#selectAssembly');
        $select.outerWidth(posWidth);
        // For some reason, it doesn't set it to posWidth, it sets it to posWidth-2...
        // detect and adjust.
        var weirdDiff = posWidth - $select.outerWidth();
        if (weirdDiff) {
            $select.outerWidth(posWidth + weirdDiff);
        }
        updateGoButtonPosition();
    }

    function processHgSuggestResults(results, term) {
        // Make matching part of the gene symbol bold
        _.each(results, function(item) {
            if (_.startsWith(item.value.toUpperCase(), term.toUpperCase())) {
                item.label = '<b>' + item.value.substring(0, term.length) + '</b>' +
                             item.value.substring(term.length);
            }
        });
        return results;
    }

    function updateFindPositionSection(uiState) {
        // Update the assembly menu, positionInput and description.
        var suggestUrl = null;
        if (uiState.suggestTrack) {
            suggestUrl = 'hgSuggest?db=' + uiState.db + '&prefix=';
        }
        setAssemblyOptions(uiState);
        if (uiState.position) {
            $('#positionDisplay').text(addCommasToPosition(uiState.position));
        }
        autocompleteCat.init($('#positionInput'),
                             { baseUrl: suggestUrl,
                               watermark: positionWatermark,
                               onServerReply: processHgSuggestResults,
                               onSelect: onSelect,
                               enterSelectsIdentical: true,
                               onEnterTerm: goToHgTracks });
        selectedGene = null;
        setAssemblyDescriptionTitle(uiState.db, uiState.genome);
        updateDescription(uiState.description);
        $('#positionInput').on("focus", function() {$(this).autocompleteCat("search", "");});
        if (uiState.db && $('#findPositionContents').css('display') === 'none') {
            initFindPositionContents();
        }
    }

    function removeDups(inList, isDup) {
        // Return a list with only unique items from inList, using isDup(a, b) -> true if a =~ b
        var inLength = inList.length;
        // inListDups is an array of boolean flags for marking duplicates, parallel to inList.
        var inListDups = [];
        var outList = [];
        var i, j;
        for (i = 0;  i < inLength;  i++) {
            // If something has already been marked as a duplicate, skip it.
            if (! inListDups[i]) {
                // the first time we see a value, add it to outList.
                outList.push(inList[i]);
                for (j = i+1;  j < inLength;  j++) {
                    // Now scan the rest of inList to find duplicates of inList[i].
                    // We can skip items previously marked as duplicates.
                    if (!inListDups[j] && isDup(inList[i], inList[j])) {
                        inListDups[j] = true;
                    }
                }
            }
        }
        return outList;
    }

    function speciesResultsEquiv(a, b) {
        // For autocompleteCat's option isDuplicate: return true if species search results
        // a and b would be redundant (and hence one should be removed).
        if (a.db !== b.db) {
            return false;
        } else if (a.genome === b.genome) {
            return true;
        }
        return false;
    }

    function searchByKeyNoCase(searchObj, term) {
        // Return a concatenation of searchObj list values whose keys start with term
        // (case-insensitive).
        var termUpCase = term.toUpperCase();
        var searchObjResults = [];
        _.forEach(searchObj, function(results, key) {
            if (_.startsWith(key.toUpperCase(), termUpCase)) {
                searchObjResults = searchObjResults.concat(results);
            }
        });
        return searchObjResults;
    }

    function processSpeciesAutocompleteItems(searchObj, results, term) {
        // This (bound to searchObj) is passed into autocompleteCat as options.onServerReply.
        // The server sends a list of items that may include duplicates and can have
        // results from dbDb and/or assembly hubs.
        // Remove duplicates and return the processed results which will then
        // be used to render the autocomplete menu only.
        var processedResults = removeDups(results, speciesResultsEquiv);
        return processedResults;
    }

    // Server response event handlers

    function checkJsonData(jsonData, callerName) {
        // Return true if jsonData isn't empty and doesn't contain an error;
        // otherwise complain on behalf of caller.
        if (! jsonData) {
            alert(callerName + ': empty response from server');
        } else if (jsonData.error) {
            console.error(jsonData.error);
            alert(callerName + ': error from server: ' + jsonData.error);
        } else {
            if (debugCartJson) {
                console.log('from server:\n', jsonData);
            }
            return true;
        }
        return false;
    }

    function updateStateAndPage(jsonData) {
        // Update uiState with new values and update the page.
        var hubsChanged = !_.isEqual(jsonData.hubs, uiState.hubs);
        // In rare cases, there can be a genome (e.g. Baboon) with multiple species/taxIds
        // (e.g. Papio anubis for papAnu1 vs. Papio hamadryas for papHam1).  Changing the
        // db can result in changing the taxId too.  In that case, update the highlighted
        // species in the tree image.
        if (jsonData.taxId !== uiState.taxId) {
            highlightLabel('textEl_' + jsonData.taxId, false);
        }
        _.assign(uiState, jsonData);
        updateFindPositionSection(uiState);
        if (hubsChanged) {
            drawSpeciesPicker(prunedDbDbTree);
        }
    }

    function handleRefreshState(jsonData) {
        if (checkJsonData(jsonData, 'handleRefreshState')) {
            updateStateAndPage(jsonData);
        }
    }

    function handleSetDb(jsonData) {
        // Handle the server's response to cartJson command setDb or setHubDb
        if (checkJsonData(jsonData, 'handleSetDb') &&
            trackHubSkipHubName(jsonData.db) === trackHubSkipHubName(uiState.db)) {
            updateStateAndPage(jsonData);
        } else {
            console.log('handleSetDb ignoring: ' + trackHubSkipHubName(jsonData.db) +
                        ' !== ' + trackHubSkipHubName(uiState.db));
        }
    }

    function handleSetTaxId(jsonData) {
        // Handle the server's response to the setTaxId cartJson command.
        if (checkJsonData(jsonData, 'handleSetTaxId') && jsonData.taxId === uiState.taxId) {
            // Update uiState with new values and update the page:
            _.assign(uiState, jsonData);
            updateFindPositionSection(uiState);
        } else {
            console.log('handleSetTaxId ignoring: ' + jsonData.taxId +
                        ' !== ' + uiState.taxId);
        }
    }

    // UI Event Handlers

    function clearWatermarkInput($input, watermark) {
        // Note: it is not necessary to re-.Watermark if we upgrade the plugin to version >= 3.1
        $input.css('color', 'black');
        $input.val('').Watermark(watermark ,'#686868');
    }

    function clearSpeciesInput() {
        // Replace anything typed into the species input with the watermark.
        clearWatermarkInput($('#speciesSearch'), speciesWatermark);
    }

    function clearPositionInput() {
        // Replace anything typed into the position input with the watermark.
        clearWatermarkInput($('#positionInput'), positionWatermark);
    }

    function setTaxId(taxId, db, org, doScrollToItem, doClearSpeciesInput) {
        // The user has selected a species (and possibly even a particular database) --
        // if we're not already using it, change to it.
        var cmd;
        if (uiState.hubUrl !== null || taxId !== uiState.taxId || (db && db !== uiState.db)) {
            uiState.taxId = taxId;
            uiState.hubUrl = null;
            cmd = { setTaxId: { taxId: '' + taxId, org: org } };
            if (db) {
                uiState.db = db;
                cmd.setTaxId.db = db;
            }
            cart.send(cmd, handleSetTaxId);
            cart.flush();
            clearPositionInput();
        }
        highlightLabel('textEl_' + taxId, doScrollToItem);
        if (doClearSpeciesInput) {
            clearSpeciesInput();
        }
  }

    function setHubDb(hubUrl, taxId, db, hubName, org, isAutocomplete) {
        // User clicked on a hub name (switch to its default genome) or selected an
        // assembly hub from autocomplete (switch to that assembly hub db).
        var cmd;
        if (hubUrl !== uiState.hubUrl ||
            (isAutocomplete && db !== uiState.db)) {
            uiState.hubUrl = hubUrl;
            uiState.taxId = taxId;
            uiState.db = trackHubSkipHubName(db);
            // Use cart variables to connect to the selected hub and switch to db
            // (hubConnectLoadHubs, called by cartNew)
            cmd = { cgiVar: { hubUrl: hubUrl,
                              genome: trackHubSkipHubName(db) },
                    setHubDb: { hubUrl: hubUrl,
                                taxId: '' + taxId }
                    };
            cart.send(cmd, handleSetDb);
            cart.flush();
            clearPositionInput();
        }
        highlightLabel('textEl_' + hubName, isAutocomplete);
        if (! isAutocomplete) {
            clearSpeciesInput();
        }
    }


    function setDbFromAutocomplete(item) {
        // The user has selected a result from the species-search autocomplete.
        // It might be a taxId and/or db from dbDb, or it might be a hub db,
        // or a taxon-only result (like "Human") that shows an assembly dropdown.
        var taxId = item.taxId || -1;
        var db = item.db;
        var org = item.org || item.value || item.label;
        var cmd;
        var genome = item.genome || '';

        // Check if db is a valid assembly name (not an organism name)
        // Valid db names are typically lowercase alphanumeric like "hg38", "mm10"
        // Organism names are capitalized like "Human", "Mouse"
        var isValidDb = db && /^[a-z]/.test(db) && db !== org;

        // Detect GenArk by category OR by genome name pattern (GCA_/GCF_)
        var isGenArk = (typeof item.category !== "undefined" && item.category.startsWith("UCSC GenArk")) ||
                       (item.hubUrl && (genome.startsWith('GCA_') || genome.startsWith('GCF_')));

        if (isGenArk) {
            db = item.genome;
            setHubDb(item.hubUrl, taxId, db, "GenArk", item.scientificName || org, true);
        } else if (item.hubUrl && item.hubName) {
            // Public hub - the autocomplete sends the hub database from hubPublic.dbList,
            // without the hub prefix -- restore the prefix here.
            db = item.hubName + '_' + item.db;
            setHubDb(item.hubUrl, taxId, db, item.hubName, org, true);
        } else if (taxId > 0) {
            // Native db with valid taxId - pass db only if it's a valid assembly name
            setTaxId(taxId, isValidDb ? db : null, org, true, false);
        } else if (isValidDb) {
            // Native db without taxId - use setDb command directly
            cmd = { setDb: { db: db, position: "lastDbPos" } };
            cart.send(cmd, handleSetDb);
            cart.flush();
            uiState.db = db;
            clearPositionInput();
            clearSpeciesInput();
        }

        // Refresh the recent genomes panel (autocompleteCat.js handles saving to localStorage)
        displayRecentGenomesInPanel();
    }

    function onClickSpeciesLabel(taxId) {
        // When user clicks on a label, use that taxId (default db);
        // don't scroll to the label because if they clicked on it they can see it already;
        // do clear the autocomplete input.
        setTaxId(taxId, null, null, false, true);
    }

    function onClickHubName(hubUrl, taxId, db, hubName) {
        // This is just a wrapper -- the draw module has to know all about the contents
        // of each hub object in hubList anyway.
        setHubDb(hubUrl, taxId, db, hubName, false);
    }

    function onChangeDbMenu() {
        // The user selected a different db for this genome; get db info from server.
        var db = $('#selectAssembly').val();
        var cmd;
        if (db !== uiState.db) {
            setAssemblyDescriptionTitle(db, uiState.genome);
            cmd = { setDb: { db: db, position: "lastDbPos" } };
            if (uiState.hubUrl) {
                cmd.setDb.hubUrl = uiState.hubUrl;
            }
            cart.send(cmd, handleSetDb);
            cart.flush();
            uiState.db = db;
            clearPositionInput();
        }
    }

    function onClickCopyPosition() {
        // Copy the displayed position into the position input:
        var posDisplay = $('#positionDisplay').text();
        $('#positionInput').val(posDisplay).trigger("focus");
    }

    function goToHgTracks() {
        // Create and submit a form for hgTracks with hidden inputs for org, db and position.
        var goDirectlyToHgTracks = false;
        var position = $('#positionInput').val();
        position = position.replace(/[\u200b-\u200d\u2060\uFEFF]/g,''); // remove 0-width chars
        var searchTerm = encodeURIComponent(position.replace(/^[\s]*/,'').replace(/[\s]*$/,''));
        var posDisplay = $('#positionDisplay').text();
        var pix = uiState.pix || calculateHgTracksWidth();
        var oldCgi = cart.cgi();
        cart.setCgi('hgSearch');
        if (! position || position === '' || position === positionWatermark ||
            position === selectedGene) {
            position = posDisplay;
            goDirectlyToHgTracks = true;
        } else {
            position = position.replace(/\u2013|\u2014/g, "-");  // replace en-dash and em-dash with hyphen
        }
        var $form;
        $form = $('<form action="hgTracks" method=GET id="mainForm">' +
                  '<input type=hidden name="hgsid" value="' + window.hgsid + '">' +
                  '<input type=hidden name="org" value="' + uiState.genome + '">' +
                  '<input type=hidden name="db" value="' + uiState.db + '">' +
                  '<input type=hidden name="position" value="' + position + '">' +
                  '<input type=hidden name="pix" value="' + pix + '">' +
                  '</form>');
        // helper functions for checking whether a plain chrom name was searched for
        function onSuccess(jqXHR, textStatus) {
            if (jqXHR.chromName !== null) {
                $('body').append($form);
                $form.trigger("submit");
            } else  {
                window.location.assign("../cgi-bin/hgSearch?search=" + searchTerm  + "&hgsid="+ window.hgsid );
            }
        }
        function onFail(jqXHR, textStatus) {
            window.location.assign("../cgi-bin/hgSearch?search=" + searchTerm  + "&hgsid="+ window.hgsid );
        }
        var canonMatch = position.match(canonicalRangeExp);
        var gbrowserMatch = position.match(gbrowserRangeExp);
        var lengthMatch = position.match(lengthRangeExp);
        var bedMatch = position.match(bedRangeExp);
        var sqlMatch = position.match(sqlRangeExp);
        var singleMatch = position.match(singleBaseExp);
        var gnomadRangeMatch = searchTerm.match(gnomadRangeExp);
        var gnomadVarMatch = searchTerm.match(gnomadVarExp);
        var positionMatch = canonMatch || gbrowserMatch || lengthMatch || bedMatch || sqlMatch || singleMatch || gnomadRangeMatch || gnomadVarMatch;
        if (positionMatch !== null || goDirectlyToHgTracks) {
            // We already have a position from either selecting a suggestion or the user just typed a regular
            // old position, so go to hgTracks at that location
            // Show a spinner -- sometimes it takes a while for hgTracks to start displaying.
            $('.jwGoIcon').removeClass('fa-play').addClass('fa-spinner fa-spin');
            // Make a form and submit it.  In order for this to work in IE, the form
            // must be appended to the body.
            $('body').append($form);
            $form.trigger("submit");
        } else {
            // User has entered a search term with no suggestion, go to the disambiguation
            // page so the user can choose a position
            // redirect to hgBlat if the input looks like a DNA sequence
            // minimum length=19 so we do not accidentally redirect to hgBlat for a gene identifier 
            // like ATG5
            var dnaRe = new RegExp("^(>[^\n\r ]+[\n\r ]+)?(\\s*[actgnACTGN \n\r]{19,}\\s*)$");
            if (dnaRe.test(searchTerm)) {
                var blatUrl = "hgBlat?type=BLAT%27s+guess&userSeq="+searchTerm;
                window.location.href = blatUrl;
                return false;
            }

            // also check if just a plain chromosome name was entered:
            $('.jwGoIcon').removeClass('fa-play').addClass('fa-spinner fa-spin');
            cmd = {getChromName: {'searchTerm': searchTerm, 'db': uiState.db}};
            cart.send(cmd, onSuccess, onFail);
            cart.flush();
        }
        cart.setCgi(oldCgi);
    }

    function replaceHgsidInLinks() {
        // Substitute '$hgsid' with real hgsid in <a> href's.
        $('a').each(function(ix, aEl) {
            var href = aEl.getAttribute('href');
            if (href && href.indexOf('$hgsid') >= 0) {
                aEl.setAttribute('href', href.replace('$hgsid', window.hgsid));
            }
        });
    }

    function displaySurvey() {
        // If hg.conf specifies a survey link, then hgGateway.c has set corresponding global vars.
        // Use those to display a labeled link (possibly an <img>) in the otherwise empty
        // #surveyContainer.
        var label;
        if (surveyLink && (surveyLabel || surveyLabelImage)) {
            if (surveyLabelImage) {
                label = '<img src="' + surveyLabelImage + '" alt="' + surveyLabel + '">';
            } else {
                label = surveyLabel;
            }
            $('#surveyContainer').html('<a href="' + surveyLink + '" target=_blank>' +
                                       label + '</a>');
        }
    }
    function createTutorialLink() {
        // allow the user to bring the tutorials popup via a new help menu button
        var tutorialLinks = document.createElement("li");
        tutorialLinks.id = "hgGatewayHelpTutorialLinks";
        tutorialLinks.innerHTML = "<a id='hgGatewayHelpTutorialLinks' href='#showTutorialPopup'>" +
            "Interactive Tutorials</a>";
        $("#help > ul")[0].appendChild(tutorialLinks);
        $("#hgGatewayHelpTutorialLinks").on("click", function () {
            // Check to see if the tutorial popup has been generated already
            var tutorialPopupExists = document.getElementById ("tutorialContainer");
            if (!tutorialPopupExists) {
                // Create the tutorial popup if it doesn't exist
                createTutorialPopup();
            } else {
                //otherwise use jquery-ui to open the popup
                $("#tutorialContainer").dialog("open");
            }
        });
    }

    // Recent Genomes Panel functions (Option C layout)

    function renderRecentGenomesPanel(genomes) {
        // Render recent genomes as vertical scrollable cards
        var $panel = $('#recentGenomesList');
        $panel.empty();

        if (!genomes || genomes.length === 0) {
            $panel.html('<div class="recentGenomesEmpty">Search for a genome above, ' +
                        'or click a popular species icon</div>');
            return;
        }

        // Render each genome as a card (vertical layout)
        genomes.forEach(function(item) {
            var $card = $('<div class="recentGenomeCard"></div>');
            var label = item.label || item.value || item.genome || item.commonName;
            var genome = item.genome || item.db || '';

            $card.append('<div class="recentGenomeLabel">' + escapeHtml(label) + '</div>');
            if (genome && label.indexOf(genome) < 0) {
                $card.append('<div class="recentGenomeDb">' + escapeHtml(genome) + '</div>');
            }

            // Add category as small label
            if (item.category) {
                var shortCategory = item.category;
                if (shortCategory.indexOf('UCSC Genome Browser') >= 0) {
                    shortCategory = 'UCSC';
                } else if (shortCategory.indexOf('GenArk') >= 0) {
                    shortCategory = 'GenArk';
                } else if (shortCategory.indexOf('Assembly Hub') >= 0) {
                    shortCategory = 'Hub';
                }
                $card.append('<div class="recentGenomeCategory">' + escapeHtml(shortCategory) + '</div>');
            }

            // Store item data for click handler
            $card.data('item', item);
            $card.on('click', function() {
                var clickedItem = $(this).data('item');
                setDbFromAutocomplete(clickedItem);
                // Highlight selected card
                $('.recentGenomeCard').removeClass('selected');
                $(this).addClass('selected');
            });

            $panel.append($card);
        });
    }

    function displayRecentGenomesInPanel() {
        // Display recent genomes in the panel on page load and after genome selection
        var recentGenomes = getRecentGenomes();
        // Show the section (hidden by default in HTML)
        $('#recentGenomesTitle').show();
        $('#recentGenomesSection').show();
        renderRecentGenomesPanel(recentGenomes);
    }

    function escapeHtml(text) {
        // Simple HTML escape for display
        if (!text) return '';
        return String(text)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;');
    }

    function init() {
        // Boot up the page; initialize elements and install event handlers.
        var searchObj = {};
        // We need a bound function to pass into autocompleteCat.init below.
        var processSpeciesResults = processSpeciesAutocompleteItems.bind(null, searchObj);
        cart.setCgi('hgGateway');
        cart.debug(debugCartJson);
        // Get state from cart
        cart.send({ getUiState: {} }, handleRefreshState);
        cart.flush();
        activeTaxIds = _.invert(activeGenomes);
        // Note: Tree pruning kept for potential future use, but tree is no longer displayed.
        if (window.dbDbTree) {
            prunedDbDbTree = dbDbTree;
            if (! pruneInactive(dbDbTree, activeGenomes, activeTaxIds)) {
                prunedDbDbTree = null;
            }
        }

        // When page has loaded, do layout adjustments and initialize event handlers.
        $(function() {
            scrollbarWidth = findScrollbarWidth();
            setRightColumnWidth();
            setupFavIcons();
            autocompleteCat.init($('#speciesSearch'),
                                 { baseUrl: 'hgGateway?hggw_term=',
                                   watermark: speciesWatermark,
                                   onSelect: setDbFromAutocomplete,
                                   onServerReply: processSpeciesResults,
                                   showRecentGenomes: true,
                                   enterSelectsIdentical: false });
            $('#selectAssembly').on("change", onChangeDbMenu);
            $('#positionDisplay').on("click", onClickCopyPosition);
            $('#copyPosition').on("click", onClickCopyPosition);
            $('.jwGoButtonContainer').on("click", goToHgTracks);
            $(window).on("resize", setRightColumnWidth.bind(null, scrollbarWidth));
            displaySurvey();
            replaceHgsidInLinks();

            // Display recent genomes in the left panel on page load
            displayRecentGenomesInPanel();

            // Gateway tutorial
            if (typeof gatewayTour !== 'undefined') {
                if (typeof startGatewayOnLoad !== 'undefined' && startGatewayOnLoad) {
                    gatewayTour.start();
                }
            }

        });
        createTutorialLink();
    }

    return { init: init,
             // For use by speciesTree.draw SVG (text-only onclick):
             onClickSpeciesLabel: onClickSpeciesLabel,
             onClickHubName: onClickHubName
           };

}()); // hgGateway

