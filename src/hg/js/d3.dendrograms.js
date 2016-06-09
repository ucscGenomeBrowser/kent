/*
d3.dendrogram.js

Copyright (c) 2013, Ken-ichi Ueda
2015 UCSC Chris Eisenhart 

DOCUEMENTATION
Currently the radial dendrogram function is much more flushed out than the cartesian
dendrograms. 

d3.dendrogram.makeRadialDendrogram(selector, nodes, options)
	Generates a radial (circular) dendrogram from a set of .json data. The dendrogram displays
	internal node size based on the number of children nodes and intenal node color based of the
	.json field "whiteToBlack" which is expected to be a rgb color.  Buttons are automatically
	generated to control internal node color, these buttons correspond to the .json fields
	[whiteToBlack, whiteToBlackSqrt, whiteToBlackQuad]. Additionally buttons are automatically 
	generated for every .json field, in the leaf nodes only, not in this list, [x, y, name, kids, 
	length, colorGroup, parent, depth, rootDist]. 
	Arguments:
		selector: selector of an element that will contain the SVG (a div id). 
		nodes: A Javascript object of nodes, most commonly a .json file parsed into javascript. 
		options:
		  	radius
				The radius of the tree, it will auto scale to fit the screen unless
				the value is specified here. 
			vis
				Pre-constructed d3 vis.
		  	tree
				Pre-constructed d3 tree layout.
		  	children
				Function for retrieving an array of children given a node. Default is
				to assume each node has an attribute called "children" that contains the
				nodes children. 
		  	diagonal
				Function that creates the d attribute for an svg:path. Defaults to a
				right-angle diagonal. To make the links smooth set diagonal to 
				d3.svg.diagonal.radial().projection(function(d){return [d.y, d.x / 180 * Math.PI];})

d3.dendrogram.makeCartesianDendrogram(selector, nodes, options)
	Generates a cartesian (flat) dendrogram from a set of .json data.  The branch lengths are
	scaled acording to the 'length' field in the .json file, buttons are automatically generated 
	for every .json field, in the leaf nodes only, not in this list, [x, y, name, kids, length, 
	colorGroup, parent, depth, rootDist]. 
	Arguments:
		selector: selector of an element that will contain the SVG (a div id). 
		nodes: A Javascript object of nodes, most commonly a .json file parsed into javascript. 
		options:
			width       
				Width of the vis, will attempt to set a default based on the width of
				the container.
		  	height
				Height of the vis, will attempt to set a default based on the height
				of the container.
		  	vis
				Pre-constructed d3 vis.
		  	tree
				Pre-constructed d3 tree layout.
		  	children
				Function for retrieving an array of children given a node. Default is
				to assume each node has an attribute called "children" that contains the
				nodes children. 
		  	diagonal
				Function that creates the d attribute for an svg:path. Defaults to a
				right-angle diagonal. To make the links smooth set diagonal to 
				d3.svg.diagonal.radial().projection(function(d){return [d.y, d.x / 180 * Math.PI];})
		  	skipTicks
				Skip the background lines for context. 
	
d3.dendrogram.rightAngleDiagonal()
	Computes the angles for the graph, the user can override this if needed. 

d3.dendrogram.leafColors(val, layer, shift, selector)
	This function is used by the buttons to color the leaf nodes. It should not be 
	modified.

d3.dendrogram.nodeColors(val, selector)
	This function is used by the buttons in the radial dendrogram to color the 
	internal nodes.  It should not be modified.
	
Internal functions:
	updateLegend(val, layer, vis, shift)
		Update the legend to correspond to the current set of meta data.  
	scaleBranchLengths(nodes, w)
		Scale the branch lengths to correspond to the 'length' field in the json data. 
	addLeafButton(title, count, layer, shift, dropdownSelector, graph) 
		Adds a leaf button to the dropdown skeleton. 
	addNodeButton(dropdownSelector, val, graph, title)
		Adds a node button to the dropdown skeleton.
	makeDropdownSkeleton(treeType)
		Builds a bootstrap dropdown menu skeleton.  

*/

if (!d3) { throw "d3 wasn't included!";}
(function(){
	
 	d3.dendrogram = {}; 
	
	// The color alphabet that was proposed in "A Colour Alphabet and the Limits of Colour Coding" by Paul Green-Armytage 10 August 2010 
	// The color alphabet capitalizes the number of visibly unqiue colors. 
	var innerColorAlphabet = ["#015eff", "#fc0a18","#0cc402", "#aea7a5", "#ff15ae", "#d99f07", 
			"#11a5fe", "#037e43","#ba4455", "#d10aff", "#9354a6", "#7b6d2b", "#08bbbb", "#95b42d", 
			"#b54e04", "#ee74ff","#2d7593","#e19772","#fa7fbe", "#fe035b", "#aea0db", "#905e76",
			"#92b27a", "#03c262"];

	var middleColorAlphabet = ["#d10aff","#03c262","#d99f07","#7b6d2b","#2d7593","#7b6d2b", 
			"#fe035b", "#aea0db","#ba4455","#9354a6","#08bbbb", "#95b42d", "#b54e04","#905e76", 
			"#92b27a", "#ee74ff","#015eff", "#fc0a18","#0cc402", "#aea7a5", "#ff15ae", "#fa7fbe", 
			"#11a5fe", "#037e43", "#e19772"];

	var outerColorAlphabet = ["#e19772","#2d7593","#fa7fbe", "#fe035b", "#aea0db", "#905e76", 
			"#92b27a", "#03c262","#015eff", "#fc0a18","#0cc402", "#aea7a5", "#ff15ae", "#d99f07",
			"#11a5fe", "#037e43","#ba4455", "#d10aff", "#9354a6", "#7b6d2b", "#08bbbb", "#95b42d",
			"#b54e04", "#ee74ff"];

	// These are used for the distance legend, same legend used in the UCSC genome browser FAQ
	var distanceRange = ["#e2e2e2","#c6c6c6","#aaaaaa","#8d8d8d","#717171", "#555555", "#383838",
			"#1c1c1c", "#000000"];

	var distanceDomain = ["< 12.5% max distance", "12.5-25%", "25-37.5%","37.5-50%","50-62.5%", 
			"62.5-75%", "75-87.5%", " > 87.5 % max distance"];

	var colors;  // The color dictionary. 
	var legendRectSize=10;	// The size of legend color squares.
	var legendSpacing=3;	// The number of pixels between each legend element.
	var radius; // The radius of a radial dendrogram.  
	
	// These are used by the leafColors function to keep track of the last button (in each dropdown)
	// that was pressed so that the color can be updated. 
	var prevInBut, prevMidBut, prevOutBut; 
	

	capitalizeFirstLetter = function(string) {
		// Capitalize the first character in a string. 
		return string.charAt(0).toUpperCase() + string.slice(1);
	}; 
	
	rightAngleDiagonal = function (){
		// Calculate the right angle connections. 
		var projection = function(d) { return [d.y, d.x]; }; 
		var path = function(pathData) {
			return "M" + pathData[0] + ' ' + pathData[1] + " " + pathData[2];
			};
		
		diagonal = function(diagonalPath, i) {
			var source = diagonalPath.source,
				target = diagonalPath.target,
				midpointX = (source.x + target.x) / 2,
				midpointY = (source.y + target.y) / 2,
				pathData = [source, {x: target.x, y: source.y}, target];
			pathData = pathData.map(projection);
			return path(pathData);
			}; 
			
		diagonal.projection = function(x) {
			if (!arguments.length) return projection;
			projection = x;
			return diagonal;
			};
			
		diagonal.path = function(x) {
			if (!arguments.length) return path;
			path = x;
			return diagonal;
			};	
		return diagonal;
	};
	
	radialRightAngleDiagonal = function() {
		// Calculate the right angle connections for a radial display.
		return rightAngleDiagonal()
			.path(function(pathData) {
				var src = pathData[0],
					mid = pathData[1],
					dst = pathData[2],
					radius = Math.sqrt(src[0]*src[0] + src[1]*src[1]),
					srcAngle = coordinateToAngle(src, radius),
					midAngle = coordinateToAngle(mid, radius),
					clockwise = Math.abs(midAngle - srcAngle) > Math.PI ? midAngle <= srcAngle : midAngle > srcAngle,
					rotation = 0,
					largeArc = 0,
					sweep = clockwise ? 0 : 1;
				return 'M' + src + ' ' +
					"A" + [radius,radius] + ' ' + rotation + ' ' + largeArc+','+sweep + ' ' + mid +
					'L' + dst;
			})
			.projection(function(d) {
				var r = d.y, a = (d.x - 90) / 180 * Math.PI;
				return [r * Math.cos(a), r * Math.sin(a)];
			});
	}; 

	coordinateToAngle = function(coord, radius) {
		// Tranform a coordinate and radius into an angle. 
		var wholeAngle = 2 * Math.PI, quarterAngle = wholeAngle / 4; 
		var coordQuad = coord[0] >= 0 ? (coord[1] >= 0 ? 1 : 2) : (coord[1] >= 0 ? 4 : 3),
			coordBaseAngle = Math.abs(Math.asin(coord[1] / radius));
		// Since this is just based on the angle of the right triangle formed
		// by the coordinate and the origin, each quad will have different 
		// offsets
		switch (coordQuad) {
			case 1:
				coordAngle = quarterAngle - coordBaseAngle;
				break;
			case 2:
				coordAngle = quarterAngle + coordBaseAngle;
				break;
			case 3:
				coordAngle = 2*quarterAngle + quarterAngle - coordBaseAngle;
				break;
			case 4:
				coordAngle = 3*quarterAngle + coordBaseAngle;
		}
		return coordAngle;
	};
	
	updateLegend = function(val, layer, vis, title, shift){
		// Generate/remove/update a legend. This handles all three rings
		
		// Remove the old legend, title and label.  
		vis.selectAll('g.' + layer + 'Legend').remove();
		vis.select("text." + layer+ "LegendTitle").remove(); 
		vis.select("text." + layer+ "LegendLabel").remove(); 
		
		// Make the new legend
		var legend=vis.selectAll('g.'+layer+'Legend').data(val); // Define the DOM elements
		var horz, vert;
		var first = 1; // Use this as a boolean to only draw the title and label one time. 
		var viewerWidth = $(document).width()/3;	// Scale to fit width
		var viewerHeight = $(document).height();  // Scale to fit height
		
		legend.enter() // Fill out the DOM elements 
			.append('g')
				.attr('class',layer+ 'Legend')
				.attr('transform', function (d, i){
					var height=legendRectSize + legendSpacing;
					var offset= height * colors.domain().length / 2; 
					vert = (i * height - offset) + (radius*(1/2)) ; // All legends are the same height, the width location varies	
					// Asign the legend width for each ring, scale to fit. 
					if (layer=="inner") horz= 20 ;  
					else if (layer=="middle") horz= 40 + viewerWidth/3; 
					else if (layer=="outer") horz= 60 + 2*viewerWidth/3; 
					if (first)
						{
						vis.append("text")// The layer, this is either 'inner', 'middle', or 'outer'.  
							.attr('class',layer+'LegendTitle')
							.style("font-size","16px")
							.style("font-weight","bold")
							.attr("transform", "translate(" + (horz-15) + "," + (vert-35)+ ")")
							.attr('x', legendRectSize + legendSpacing)
							.attr('y', legendRectSize - legendSpacing)
							.text(capitalizeFirstLetter(layer)); 

						vis.append("text")// The title, this is mined from the meta data
							.attr('class',layer+'LegendLabel')
							.style("font-size","16px")
							.style("font-weight","bold")
							.attr("transform", "translate(" + (horz-15) + "," + (vert-15)+ ")")
							.attr('x', legendRectSize + legendSpacing)
							.attr('y', legendRectSize - legendSpacing)				
							.text(function (){
								if (title.length > 15){
									return title.substring(0,14)+"...";
									} 
								return title; 
								});
						first = 0;// 1 is true, 0 is false; 
						}
					return 'translate(' + horz + ',' + vert + ')'; // Actually move the legend. 
					});

		// Apply the colored rectangles.
		legend.append('rect')
			.attr('width', legendRectSize)
			.attr('height', legendRectSize)
			.style('fill', colors)
			.style('stroke', colors);
		
		// Give the colored rectangles associated name tags.  
		legend.append('text')
			.style("font-size","12px")
			.style("font-weight","bold")
			.attr('x', legendRectSize + legendSpacing)
			.attr('y', legendRectSize - legendSpacing)
			.text(function (d){return d;});
		};
	

	scaleBranchLengths = function (nodes, w) {
	// Used for the cartesian dendrogram to scale the branch lengths making
	// them proportional to the 'length' variable. 
	// Visit all nodes and adjust y pos width distance metric
		var visitPreOrder = function(root, callback) {
			callback(root);
			if (root.children) {
				for (var i = parseInt(root.children.length) - 1; i >= 0; i--){
					visitPreOrder(root.children[i], callback);
					}
				}
			};

		visitPreOrder(nodes[0], function(node) {
			node.rootDist = (node.parent ? node.parent.rootDist : 0) + (parseInt(node.length) || 0);
			});

		var rootDists = nodes.map(function(n) { return n.rootDist; });
		var yscale = d3.scale.linear()
			.domain([0, d3.max(rootDists)])
			.range([0, w]);
		visitPreOrder(nodes[0], function(node) {
			node.y = yscale(node.rootDist);
			});
		return yscale;
		};
	
	// This function creates the dropdown skeleton which is populated at a later point. 
	// The javascript buttons are created at runtime based on the .json file provided. 
	// This allows the .html code to be greatly simplified, the work is made up here with javascript. 
	makeDropdownSkeleton = function (treeType){
		// Generate the html buttons, use a base anchor '#dropdown' and append everything to that. 
		var dropdown = d3.select("#dropdown").append("ul")	// Make a list of the dropdown buttons. 
			.attr("style","list-style-type:none;display:inline-flex");
		
		d3.select("#dropdown").attr("style","position:fixed;z-index:2"); 		

		if (treeType == "Dendro"){
			var interiorNodeDropdown = dropdown.append("li")// Add an element to the list of dropdown buttons 
				.attr("class","dropdown");
				
			interiorNodeDropdown.append("button")	// The element is also a button 
					.attr("class","btn btn-default dropdown-toggle") // and a dropdown list  
					.attr("type", "button")	// of buttons. 
					.attr("data-toggle","dropdown")	
					.html("General control panel")	// Name the button 
					.append("span").attr("class","caret"); // Give the button a carot. 
				
			interiorNodeDropdown.append("ul")
				.attr("class","dropdown-menu")
				.html("<div id=interiorDendroNodes </div>"); // Identify the button. 
			}
		
		// Setup the inner ring dropdown button. This button opens up a list of buttons, and itself
		// is a item in a list (a list of buttons). 
		var innerDropdown = dropdown.append("li").attr("class","dropdown");
			
		innerDropdown.append("button")
			.attr("id", "innerDropdown")	//Identify a different part of the button so that it can be renamed
			.attr("class","btn btn-default dropdown-toggle")
			.attr("type", "button")
			.attr("data-toggle","dropdown")
			.html("Inner ring")
			.append("span")
				.attr("class","caret"); 
			
		innerDropdown.append("ul")
			.attr("class","dropdown-menu")
			.html("<div id=inner"+treeType+"Leaves </div>"); 
		
		// Setup the middle ring dropdown button 
		var middleDropdown = dropdown.append("li")
			.attr("class","dropdown");
		
		middleDropdown.append("button")
			.attr("id", "middleDropdown")
			.attr("class","btn btn-default dropdown-toggle")
			.attr("type", "button")
			.attr("data-toggle","dropdown")
			.html("Middle ring")
			.append("span")
				.attr("class","caret"); 
		
		middleDropdown.append("ul")
			.attr("class","dropdown-menu")
			.html("<div id=middle"+treeType+"Leaves </div>"); 
		
		// Setup the outer ring dropdown button 
		var outerDropdown = dropdown.append("li").attr("class","dropdown");
			
		outerDropdown.append("button")
			.attr("id", "outerDropdown")
			.attr("class","btn btn-default dropdown-toggle")
			.attr("type", "button")
			.attr("data-toggle","dropdown")
			.html("Outer ring")
			.append("span")
				.attr("class","caret"); 
			
		outerDropdown.append("ul")
			.attr("class","dropdown-menu")
			.html("<div id=outer"+treeType+"Leaves </div>"); 
		};
	
	d3.dendrogram.nodeColors = function(val, selector){
		var vis = d3.select(selector).selectAll("svg");
		var node=vis.selectAll("g.internalNode").selectAll("circle");
		if (val < 8)
			{
			node.attr("r", function(d){
				if (d.name === " "){
					if (val == 2) return Math.sqrt(d.kids-1);
					else if (val == 3) return 1; //small	
					else if (val == 4) return 3; //normal
					else if (val == 5) return 5; //large
					else if (val == 6) return 0.5; //tiny
					else if (val == 7) return 10; //huge
					}
				});
			}
		else
			{
			var link = vis.selectAll("path.link")
				.attr("stroke-width", function(d){
					if (val == 8) return ".07px"; // small
					else if (val ==9) return ".2px"; // normal
					else if (val == 10) return ".5px"; // large
					}); 
			}
		}; 

	// The javascript buttons are created at runtime based on the .json file provided. This allows the .html code to be greatly
	// simplified at the expense of some rather confusing javascript.  All functions with the prefix d3.dendrograms.js are
	// accessible from outside this module. All functions without the prefix can only be used within the module. 
	//
	// The addLeafButton and addNodeButton functions are used within this program to write buttons, the buttons themselves use the globally available 
	// functions d3.dendrogram.leafColors and d3.dendrogram.nodeColors to apply the colors.  This is necesseary because the addLeafButton and
	// addNodeButton functions are writing new elements to the DOM, namely buttons. These buttons have an on click function that points to
	// the corresponding function in this module (d3.dendrogram.leafColors/d3.dendrogram.nodeColors), so the prefix is necessary to identify
	// where the function resides. 
	//
	// A cool side effect/result of this is there is no buttons in the .html (and the user doesn't have to fus with them). 
	// The buttons are all created at runtime by the javascript! 
	
	addNodeButton = function (dropdownSelector, val, graph, title) {
		// Add a node button to the corresponding dropdown list. 
		d3.select(dropdownSelector).append("li").html("<button onclick=\"d3.dendrogram.nodeColors("+val+",'"+graph+"')\">"+title+"</button>"); 
	};
	
	addLeafButton = function (title, count, layer, shift, dropdownSelector, graph, offset) {
		// Add a leaf button to the corresponding dropdown list.  Shorten the name if it is too long
		// and append '...' to the end. 
		var nameTag;
		if (typeof title === 'string' && title.length > 20) nameTag =  capitalizeFirstLetter(title.substring(0,19)+"...");
		else nameTag = capitalizeFirstLetter(title); 
		// Give the button a div tag. This lets it be colored in after the user clicks on it. 
		var divId = layer+capitalizeFirstLetter(title).replace("_","");  // Divs break with underscores, probably other things too... 
		var options = count+",\'"+layer+"\',"+shift+", \'"+title+"\',\'"+graph+ "\',"+ offset; 
		var leafButton = "<div id=\""+divId+"Button\"</div><button onclick=\"d3.dendrogram.leafColors("+options+")\">"+nameTag+"</button>"; 
			d3.select(dropdownSelector).append("li")
				.html(leafButton); 
	};
	
	d3.dendrogram.leafColors = function(val, layer, shift, title, selector, offset){
		// Apply/change/remove colors to a leaf ring.  The colors are assigned based on
		// .json fields. This function calls updateLegend to update the corresponding legend.
		// An interesting note is that updateLegend doesn't need to have a d3.dendrograms.js prefix
		// since it is being called by a function in the module.  
		var vis = d3.select(selector).select("svg").select("g").selectAll("g.leaf"); //Select the the leaf nodes. 
		var first = 1;
		var node = vis.selectAll("g."+ layer + "Leaf");
		// Change the color based on the ring, look for a previously clicked button and update the color. 
		if (layer == "inner")
			{
			colors = d3.scale.category20().range(innerColorAlphabet);
			if (prevInBut) prevInBut.style("background", "rgb(239,239,239)");
			} 
		if (layer == "middle")
			{
			colors = d3.scale.category20().range(middleColorAlphabet);
			if (prevMidBut) prevMidBut.style("background", "rgb(239,239,239)");
			} 
		if (layer == "outer")
			{
			colors = d3.scale.category20().range(outerColorAlphabet);
			if (prevOutBut) prevOutBut.style("background", "rgb(239,239,239)");
			}
		
		var ringName; // Format the ring title. 
		if (title.length > 20) ringName = capitalizeFirstLetter(title).substring(0,19)+"...";
		else ringName = capitalizeFirstLetter(title); 

		// Update the dropdown title to reflect the button that was clicked, use the formatted name. 
		d3.select("#"+layer+"Dropdown").html(capitalizeFirstLetter(layer)+" ring: " + ringName);

		// Reset if the user clicks the Remove button. 
		if (val == -1)  d3.select("#"+layer+"Dropdown").html(capitalizeFirstLetter(layer) + " ring"); 
		
		// If there is no remove button and this function is being called then generate a remove button function. 
		if (!d3.select("#"+layer+"RemoveButton")[0][0]){
			addLeafButton("Remove", -1, layer, 10, '#'+layer+'DendroLeaves', selector, offset);
			}

		// Keep track of the currently selected button so its color can be reverted when another button is clicked. 
		if (layer == "inner") prevInBut = d3.select("#"+layer+capitalizeFirstLetter(title).replace("_","")+"Button").selectAll("button");
		if (layer == "middle") prevMidBut = d3.select("#"+layer+capitalizeFirstLetter(title).replace("_","")+"Button").selectAll("button");
		if (layer == "outer") prevOutBut = d3.select("#"+layer+capitalizeFirstLetter(title).replace("_","")+"Button").selectAll("button");
	
		if (val == -1)
			{
			vis.selectAll("text."+layer+"LegendTitle").remove(); 
			vis.selectAll("text."+layer+"LegendTitleLabel").remove(); 
			d3.select("#"+layer+"RemoveButton").remove();
			}

		// Color the current button. 
		d3.select("#"+layer+capitalizeFirstLetter(title).replace("_", "")+"Button").selectAll("button")	
			.style("background","cornflowerblue"); 

		// Update the ring color. Start by selecting all the polygons.  
		node.selectAll("polygon")
			.style("fill", function(d){
				// This block links the extra leaf json fields to a specific value (count). This value
				// is provided when the buttons are written in the main function. 
				if (val==1) // A val of 1 indicates a distance ring, update the colors dictionary so the legend displays properly.  
					{ 
					colors = d3.scale.category20().range(distanceRange).domain(distanceDomain); 
					return d3.rgb(d.colorGroup);
					}
				else if (val==-1) // A val of -1 indicates a remove ring call. Kill everything. 
					{
					return "none";
					}
				var count = 0;
				// Go through the json fields in each leaf node and find the corresponding field using the val variable
				// once found give it a new color with the d3 colors dictionary/function/object.  
				for (var key in d) {
					if (d.hasOwnProperty(key)){ // Ignore several json fields that don't need meta data rings.  
						if (key != "x" && key!="y" && key!= "name" && key!="kids" && key!="length" && key!="colorGroup" && key!="parent" && key!="depth" && key!="rootDist"){
							if (count == val){
								var sub = d[key]; // Format the json field. 
								if (typeof d[key] === 'string' && d[key].length > 20)
									{
									sub = d[key].substring(0,19)+"...";
									}
								count += 1;
								return colors(sub); // Pass the field into the color dictionary. This allows the legend to display properly. 
								}
							}
						count += 1;
						}
					}
				})
		.style("stroke-width", ".2px") 
		.attr("transform", "rotate(270)translate("+(-offset)+","+shift+")"); 
		// Move the ring out for each layer, rotate it 1/2 the size of a polygon so that the polygons are centered. 
		
		// Update the legend. 
		updateLegend(colors.domain(), layer, (d3.select(selector).select("svg.legendSvg").select("g")), title, shift);
		// Reset the colors so there is no overlap. 
		colors=d3.scale.category20();
		};

	d3.dendrogram.makeCartesianDendrogram =function (selector, data, options) {
		// This function generates the cartesian dendrogram (flat).  Right now I consider it 
		// depricated. Use it at your own disgression! If there is interest it will be updated
		// with all the fancy new features (tooltips and such). Otherwise it may be removed 
		// entirely. 

		function zoom() {
			svgGroup.attr("transform", "translate(" + d3.event.translate + ")scale(" + d3.event.scale + ")");
			}

		options = options || {};
		// define the zoomListener which calls the zoom function on the "zoom" event constrained within the scaleExtents
		var zoomListener = d3.behavior.zoom().scaleExtent([0.01, 10])
				.on("zoom", zoom);	
		var w = options.width || d3.select(selector).style('width') || d3.select(selector).attr('width'),
			h = options.height || d3.select(selector).style('height') || d3.select(selector).attr('height');
		var viewerWidth = $(document).width();	//Scale to fit. 
		var viewerHeight = $(document).height();	//Scale to fit. 
		var diagonal = options.diagonal || d3.dendrogram.rightAngleDiagonal();
		var nodes = tree(data);
		var legendRectSize = 6; 
		var legendSpacing = 6; 
		var yscale; 	
		var tree = options.tree || d3.layout.cluster()
		    .size([h, w])
			.sort(function(node) { return node.children ? parseInt(node.children.length) : -1; })
		    .children(options.children || function(node) {
				return node.children;
		  		});	
		var vis = options.vis || d3.select(selector)
			.append("svg")
				.attr("width", viewerWidth)
				.attr("height", viewerHeight)
				.attr("class", "overlay")
				.call(zoomListener); 
		var svgGroup = vis.append("g")
				.attr("transform", "translate(20, 20)");
		
		d3.select(selector).select("svg").append ("rect")
			.attr("width",w).attr("height",h)
			.style("fill", "none")
			.style("stroke","black")
			.style("stroke-width","5")
			.style("opacity","0.5");
		  
		
		d3.select(selector).select("svg").on("wheel.zoom",null); 
		d3.select(selector).select("svg").on("mousewheel.zoom",null); 

		if (options.skipBranchLengthScaling) {
		  	yscale = d3.scale.linear()
				.domain([0, w])
				.range([0, w]);
			}
		else{
			yscale = scaleBranchLengths(nodes, w);
			}
		
		if (!options.skipTicks) {
		  	svgGroup.selectAll('line')
				.data(yscale.ticks(10))
				.enter().append('svg:line')
			  		.attr('y1', 0)
			  		.attr('y2', h)
			  		.attr('x1', yscale)
			  		.attr('x2', yscale)
			  		.attr("stroke", "#ddd")
					.attr("stroke-width","3px");

		  	svgGroup.selectAll("text.rule")
				.data(yscale.ticks(10))
				.enter().append("svg:text")
			  		.attr("class", "rule")
			  		.attr("x", yscale)
			  		.attr("y", 0)
			  		.attr("dy", -3)
			  		.attr("text-anchor", "middle")
			  		.attr('font-size', '8px')
			  		.attr('fill', '#ccc')
			  		.text(function(d) { return Math.round(d*100) / 100; });
			}
			
		var link = svgGroup.selectAll("path.link")
			.data(tree.links(nodes))
		  	.enter().append("svg:path")
				.attr("class", "link")
				.attr("d", diagonal)
				.attr("fill", "none")
				.attr("stroke", "#aaa")
				.attr("stroke-width", "1px");
			
		var first = 1; 
		
		makeDropdownSkeleton("Phylo");
		
		var innerNodes = svgGroup.selectAll("g.innerNode")
			.data(nodes)
		  	.enter().append("g")
				.attr("class", function(n) {
				  	if (n.children) {
						if (n.depth === 0) {
					  		return "root node";
						} else {
					  		return "inner node";
							}
				  		}
					else{
						if (first){
							var count = 0;
								for (var key in n) {
									if (n.hasOwnProperty(key)) 
										{
										if (key != "x" && key!="y" && key!= "name" && key!="kids" && key!="length" && key!="colorGroup" && key!="parent" && key!="depth" && key!="rootDist")
											{
											addLeafButton(key, count, 'inner', 0, '#innerPhyloLeaves', '#phylogram', 0) ;
											addLeafButton(key, count, 'middle', 0, '#middlePhyloLeaves', '#phylogram', 0); 
											addLeafButton(key, count, 'outer', 0, '#outerPhyloLeaves', '#phylogram', 0); 
											}
										}
										count += 1;
								}
							first = 0;
							}
						return "innerLeaf";
						}
					})
				.attr("transform", function(d) { return "translate(" + (d.y) + "," + (d.x) + ")"; });
		
		var innerLeaves = svgGroup.selectAll("g.innerLeaf")
			.append("circle")
				.attr("r", '3') 
				.style("fill", 'none');
		
		var middleLeaves = svgGroup.selectAll("g.middleLeaf")
			.data(nodes)
		 	.enter().append("svg:g")
				.attr("class", function(n) {
			  		if (n.children) {
						return "nill";
			  			}
						else{
							return "middleLeaf";
			  			}
					});
			
		svgGroup.selectAll("g.middleLeaf")
			.attr("transform", function(d) { return "translate(" + (d.y +10 ) + "," + d.x + ")"; })
			.append("circle")
				.attr("r", '3')
				.style("fil", 'none'); 
		
		
		var outerLeaves = svgGroup.selectAll("g.outerLeaf")
			.data(nodes)
			.enter().append("svg:g")
			.attr("class", function(n) {
			  	if (n.children) {
					return "nill";
					}
			   	else{
					return "outerLeaf";
			  		}
				})
			.attr("transform", function(d) { return "translate(" + (d.y+20 ) + "," + d.x + ")"; });
			
		svgGroup.selectAll("g.outerLeaf")
			.append("svg:circle")
				.attr("r", '3')
				.attr('stroke',  'black')
				.style("stroke-width", ".25px")
				.attr('fill', 'greenYellow')
				.attr('fill', 'black'); 
		
		svgGroup.selectAll('g.root.node')
		 	.append('svg:circle')
				.attr("r", 10)
				.style("fill", function(d){
					if (d.name !==" "){return "d.whiteToBlack";}
					})
				.attr('stroke', '#369')
				.attr('stroke-width', '2px');
		
		if (!options.skipLabels){
		  	vis.selectAll('g.inner.node')
				.append("svg:text")
			  		.attr("dx", -6)
			  		.attr("dy", -6)
			  		.attr("text-anchor", 'end')
			  		.attr('font-size', '8px')
			  		.attr('fill', '#ccc')
			  		.text(function(d) { return d.length; });

			vis.selectAll('g.outerLeaf')
				.append("svg:text")
					.attr("dx", 8)
					.attr("dy", 3)
					.attr("text-anchor", "start")
					.attr('font-family', 'Helvetica Neue, Helvetica, sans-serif')
					.attr('font-size', '6px')
					.attr('fill', 'black')
					.text(function(d) { return d.name + ' ('+d.length+')'; });
			}
		
		return {tree: tree, vis: vis};
		};
  

	d3.dendrogram.makeRadialDendrogram = function(selector, data, options) {
		// Generate the radial dendrogram and all corresponding boxes/buttons. 
		// This includes a dropdown hover menu that is 'stickied' to the top of the page. 
		// The page dimensions are determined and 60% is specified for the dendrogram while
		// 40% is saved for the control panel (where legends and tooltips are shown). The leaf nodes are
		// cast to be polygons. The polygons are scaled based on the radius to fit perfectly as a circle. 
		// The 'scaling' option should be used with smaller submissions to account for the more angled
		// leaf ring. A value of 4/3 is recommended for submissions with < 100 leaves, and 5/6 for < 200. 
		
		options = options || {}; // Read in the options. set it to null otherwise. 

		function zoom() { // Function for enabling zoom. 
			svgGroup.attr("transform", "translate(" + d3.event.translate + ")scale(" + d3.event.scale + ")");
			}
		
		function makePoints(n, k)
			// Calculate the points for the polygon's. The Y value is automatically cast to 5, ie it will be 5
			// pixels long. K is assumed to be the big end, while n is assumed to be the little end.  
			{
			var point1, point2, point4; 
			point1 = (String((k-n)/2) + ",0").replace(" ", "");
			point2 = (String((k+n)/2) + ",0").replace(" ", ""); 
			point4 = (String(k) + ",7").replace(" ", ""); 
			return(point2  + " " + point1 + " 0,7 "  + point4); 
			}
		
		var viewerWidth = $(document).width(); // Scale to fit.
		var viewerHeight = $(document).height();  // Scale to fit. 
		
		var calc = Math.min(viewerWidth, viewerHeight);  // Make the radius depend on the minimum of height and weight.
		
		radius = (calc -50)/ 2; // Override the global, this is used for the legend placement. See function updateLegend.

		var tree = options.tree || d3.layout.cluster()
			.size([360, radius - 45])
			.separation(function separation(a, b) {
				return (a.parent == b.parent ? 1 : 1) ; // Evenly place leaf nodes.  
				});
		
		var zoomListener = d3.behavior.zoom().scaleExtent([1, 10]) // Let the user zoom up to 10x 
			.on("zoom", zoom); 
		
		var diagonal = options.diagonal || radialRightAngleDiagonal(); // Makes links right angled. 

		var scaling = options.scaling || 1; 
		var width =  viewerWidth -25,
			height = viewerHeight - 50; 

		var metaHorz = 50,
			metaVert = (viewerHeight)/2; 
		
		// Setup the svg for the radial display. 
		var vis = options.vis || d3.select(selector) 
			.attr("style", "display:inline-flex") 
			.append("svg")
				.attr("width", 3*width/5 )
				.attr("height", height)
				.attr("class", "overlay")
				.call(zoomListener);
		
		// Setup the svg for the control display. 
		var legendVis = options.vis || d3.select(selector)
			.append("svg")
			.style("background","rgb(239,239,239)")
			.attr("class", "legendSvg")
			.attr("width", 2*width/5)
			.attr("height", height); 
		
		// Append a rectangle to the page for the dendrogram, let it take up 60% the width
		vis.append ("rect")
			.attr("width", 3*width/5)
			.attr("height", height)
			.style("fill", "none")
			.style("stroke","black")
			.style("stroke-width","5")
			.style("opacity","0.5");
		
		// Append a rectangle to the page for the legends, let it take up 40% the width
		legendVis.append ("rect")
			.attr("width", 2*width/5)
			.attr("height", height)
			.style("fill", "none")
			.style("stroke","black")
			.style("stroke-width","5")
			.style("opacity","0.5");

		legendVis.append("g"); 

		var svgGroup = vis.append("g")
				.attr("transform","translate(" + (radius) + "," + radius + ")");
				
		d3.select("svg").on("wheel.zoom",null); 
		d3.select("svg").on("mousewheel.zoom",null); 

		var tooltip = d3.select(selector) // Some infastructure for the tooltips. 
			.append('div')
			.attr('class', 'tooltip')
			.style("opacity",0);

		var nodes=tree.nodes(data);
		
		var link=svgGroup.selectAll("path.link")
			.data(tree.links(nodes)).enter()
			.append("path")
				.attr("class", "link")
				.attr("fill", "none")
				.attr("stroke", "black")
				.attr("stroke-width", ".2px")
				.attr("d", diagonal); 	
	
		var first = 1; // Essentially a boolean. 
		var leafCount; // The total number of leaves, will be used to calculate the polygon width. 
		var innerOffset, middleOffset, outerOffset, rawOffset; // The offsets are used to center the polygons.  
		var inR=6, midR=13, outR=20; // This is the pixel offset for each ring.  
		var filled = 1; // Essentially a boolean for the first tooltip trigger.   
		var prevSelLeaf, prevSelLeafNode; // Keep track of the previously selected node so that its color can be reverted when another node is clicked. 
		
		makeDropdownSkeleton("Dendro"); //Looks for the div 'dropdown' and generates the outer list and buttons
		
		// Add a bunch of the default options for to the general commands dropdown. 
		addNodeButton("#interiorDendroNodes" ,2, selector, "Node size: sqrt(kids)"); 
		addNodeButton("#interiorDendroNodes" ,6, selector, "Node size: tiny"); 
		addNodeButton("#interiorDendroNodes" ,3, selector, "Node size: small"); 
		addNodeButton("#interiorDendroNodes" ,4, selector, "Node size: normal"); 
		addNodeButton("#interiorDendroNodes" ,5, selector, "Node size: large"); 
		addNodeButton("#interiorDendroNodes" ,7, selector, "Node size: huge"); 
		addNodeButton("#interiorDendroNodes" ,8, selector, "Link width: thin"); 
		addNodeButton("#interiorDendroNodes" ,9, selector, "Link width: normal"); 
		addNodeButton("#interiorDendroNodes" ,10, selector, "Link width: thick"); 

		// These are true nodes in the sense that they are actually calculated and defined, 
		// however this code is not repsonsible for the actual visuale representation. This
		// block only handles the backend DOM construction, the shapes and colors are assigned later.

		var trueNodes = svgGroup.selectAll("g")
			.data(nodes).enter()
			.append("g")
				.attr("class", function(n) {
					if (n.depth===0){ // This is the root, do a bunch of one off things. 
						leafCount = n.kids; 
						rawOffset = Math.PI*2*(radius)/(parseInt(leafCount)*3); 
						innerOffset =  Math.PI*2*(radius+inR)/(parseInt((leafCount*(scaling))*2)); 
						middleOffset =  Math.PI*2*(radius+midR)/(parseInt((leafCount*(scaling))*2)); 
						outerOffset =   Math.PI*2*(radius+outR)/(parseInt((leafCount*(scaling))*2)); 
						addLeafButton("Distance", 1, 'inner', inR, '#innerDendroLeaves', '#dendrogram', innerOffset); 
						addLeafButton("Distance", 1, 'middle', midR, '#middleDendroLeaves', '#dendrogram', middleOffset); 
						addLeafButton("Distance", 1, 'outer', outR, '#outerDendroLeaves', '#dendrogram', outerOffset) ;
						}
					if (n.children) { // Any node with children is an internal node. 
				  		return "internalNode";
						}
					else{
						if (first){
							var count = 0;
							for (var key in n) {
								if (n.hasOwnProperty(key)) { 
								// Give each dropdown button a bunch of options. 
									if (key != "x" && key!="y" && key!= "name" && key!="kids" && key!="length" && key!="colorGroup" && key!="parent" && key!="depth" && key!="rootDist"){
										addLeafButton(capitalizeFirstLetter(key), count, 'inner', inR, '#innerDendroLeaves', '#dendrogram', innerOffset); 
										addLeafButton(capitalizeFirstLetter(key), count, 'middle', midR, '#middleDendroLeaves', '#dendrogram', middleOffset); 
										addLeafButton(capitalizeFirstLetter(key), count, 'outer', outR, '#outerDendroLeaves', '#dendrogram', outerOffset);
										}
									}
								count += 1;
								}
							first = 0;
							}
						return "leaf";
						}
					})
				.attr("transform", function(d){return "rotate(" + (d.x - 90) + ")translate(" + d.y + ")";});
		
		
		// This section handles the internal node visual representation, including the node initial size/color
		// and the tooltips (on click visualizations) 
		var interiorNodes = svgGroup.selectAll("g.internalNode")
			.append("circle")
				.attr("r", function(d){ // Handle node size
					if (d.name==" "){
					return 3;}
					else{return 0;}
					})
				.style("fill", function(d){ // Handle node color 
					if (d.name==" "){return d.whiteToBlack;}
					else{return "none";}})
				.on("click", function(d) { // Tooltip stuffs 
					d3.select(this).style("fill", "red"); // Color this node red 
					if (filled == 2) { // Another node was previously clicked, its color needs to be reverted. 
						if (d3.select(this) != prevSelLeaf && d3.select(this) != prevSelLeafNode && d != prevSelLeaf && d != prevSelLeafNode)
							{
							if (prevSelLeafNode.name==" "){prevSelLeaf.style("fill", prevSelLeafNode.whiteToBlack);}
							else{prevSelLeaf.style("fill", "white");}
							}
						} 
					tooltip.transition()
					.duration(200)
					.style("opacity", 0.9);
					result = "<table border=\"1\">"; // Make the tooltip a table, the first section is the basic meta data
					count = 1;
					result = result +"<tr><td colspan=\"4\" style=\"font-weight:bold;font-size:16px;text-align:center\">Node meta data</td>"; 
					for (var key in d) {
						if (d.hasOwnProperty(key)) {
							if (key == "number" || key == "rootDist" || key == "kids" || key == "tpmDistance" || key == "length" || key == "contGenes" || key == "normalizedDistance" || key == "depth")
								{
								if (count == 1){
									count = 2; 
									result = result +"<tr><td style=\"font-weight:bold;font-size:12px;\">" + (key)+ "</td><td style=\"font-weight:bold;font-size:12px;\">" + (d[key]) + "</td>"; 
									}
								else{
									count = 1; 
									result = result +"<td style=\"font-weight:bold;font-size:12px;\">" + (key) + "</td><td style=\"font-weight:bold;font-size:12px;\">" + (d[key]) + "</td></tr>"; 
									}
								}
							}
						}
					count = 1;
					// This section handles the tooltip representation for the top contributing genes.  
					result = result +"<tr><td colspan=\"2\" style=\"font-weight:bold;font-size:16px\">Top 10 contributing genes </td><td colspan=\"2\" style=\"font-weight:bold;font-size:16px\">TPM value contributed</td>"; 
					for (var n in d.geneList){
						if (count == 1){
							count = 2; 
							result = result +"<tr><td style=\"font-weight:bold;font-size:12px\">" + n + "</td><td style=\"font-weight:bold;font-size:12px\">" + d.geneList[n] + "</td>"; 
							}
						else{
							count = 1; 
							result = result +"<td style=\"font-weight:bold;font-size:12px\">" + n + "</td><td style=\"font-weight:bold;font-size:12px\">" + d.geneList[n] + "</td></tr>"; 
							}
						}
					result = result + "</table"; 
					tooltip.html(result)
					.style("right", metaHorz + "px")
					.style("top", metaVert + "px"); 
					filled = 2;
					prevSelLeafNode = d; 
					prevSelLeaf = d3.select(this); 
					})
				.style("stroke", "black")
				.style("stroke-width", ".1px"); 
		
		
		var leaves = svgGroup.selectAll("g.leaf"); // All leaf like things reside in here. 
		
		// These are considered 'trueLeafs' which means they are generated all the time (ie not generated by leafColors). 
		// They are turned red on click. 
		leaves
			.append("g")
			.attr("class", "trueLeaf")
			.append("polygon")
				.style("fill","none")
				.attr("points", makePoints(Math.PI*2*radius/(leafCount*(3/2)), Math.PI*2*(radius+5)/(leafCount*(3/2))))
				.style("fill", "white")
				.style("stroke", "black")
				.style("stroke-width", ".1px")
				// Leaves tooltip 	
				.on("click", function(d) {
					d3.select(this).style("fill", "red"); // Change the clicked node to be red. 
					if (filled == 2) // A node was clicked after a previous node was clicked! Need to revert the previous nodes color.  
						{
						if (d3.select(this) != prevSelLeaf && d3.select(this) != prevSelLeafNode && d != prevSelLeaf && d != prevSelLeafNode)
							{
							if (prevSelLeafNode.name==" ")
								{
								prevSelLeaf.style("fill", prevSelLeafNode.whiteToBlack);
								}
							else
								{
								prevSelLeaf.style("fill", "white");
								}
							}
						}
					tooltip.transition()
					.duration(200)
					.style("opacity", 0.9);
					result = "<table border=\"1\">";  
					count = 1;
					// Create a .html table to display on click.
					result = result +"<tr><td colspan=\"4\" style=\"font-weight:bold;font-size:16px;text-align:center\">Leaf meta data</td>"; 
					for (var key in d) {
						if (d.hasOwnProperty(key)) {
							if (key != "x" && key!="y" && key!="kids" && key!="colorGroup" && key!="parent")
								{
								var sub1 = key, sub2 = d[key]; 
								if (typeof key === 'string' && key.length > 12)
									{
									sub1 = capitalizeFirstLetter(key.substring(0,10)+"...");
									}
								if (typeof d[key] === 'string' && d[key].length > 20)
									{
									sub2 = capitalizeFirstLetter(d[key].substring(0,19)+"...");
									}
								if (count == 1){
									count = 2; 
									result = result +"<tr><td style=\"font-weight:bold;font-size:12px;\">" + sub1 + "</td><td style=\"font-weight:bold;font-size:12px;\">" + sub2 + "</td>"; 
									}
								else{
									count = 1; 
									result = result +"<td style=\"font-weight:bold;font-size:12px;\">" + sub1 + "</td><td style=\"font-weight:bold;font-size:12px;\">" + sub2 + "</td></tr>"; 
									}
								}
							}
						}
					result = result + "</table"; 
					tooltip.html(result)
					.style("right", metaHorz + "px")
					.style("top", metaVert + "px"); 
					filled = 2;
					prevSelLeaf = d3.select(this); 
					prevSelLeafNode = d; 
					})
				.attr("transform", "rotate(270)translate("+(-rawOffset)+",1)"); 

		// This is the inner most leaf ring. 
		leaves
			.append("g")
			.attr("class", "innerLeaf")
			.attr("width","3px")
			.attr("height","3px")
			.append("polygon") // Give it a polygon, scale the polygon to fit. 
				.style("fill","none")
				.attr("points", makePoints(Math.PI*2*(radius+inR)/(leafCount*(scaling)), Math.PI*2*(radius+midR)/(leafCount*(scaling)) ));
		
		// This is the middle most leaf ring. 
		leaves
			.append("g")
			.attr("class", "middleLeaf")
			.attr("width","3px")
			.attr("height","3px")
			.append("polygon")
				.style("fill","none")
				.attr("points", makePoints(Math.PI*2*(radius+midR)/(leafCount*(scaling)), Math.PI*2*(radius+outR)/(leafCount*(scaling))));
		
		// This is the outer most leaf ring. 
		leaves
			.append("g")
			.attr("class", "outerLeaf")
			.attr("width","3px")
			.attr("height","3px")
			.append("polygon")
				.style("fill","none")
				.attr("points", makePoints(Math.PI*2*(radius+outR)/(leafCount*(scaling)), Math.PI*2*(radius+(outR + (outR -1)/3))/(leafCount*(scaling))));
		
		return {tree: tree, vis: svgGroup};
		};
})();
