/*
d3.dendrogram.js

Copyright (c) 2013, Ken-ichi Ueda
2015 UCSC Chris Eisenhart 

DOCUEMENTATION
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
				right-angle diagonal.
		  	skipTicks
				Skip the background lines for context. 
		  	skipBranchLengthScaling
				Make a dendrogram instead of a dendrogram.

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
				right-angle diagonal.
		  	skipTicks
				Skip the background lines for context. 
		  	skipBranchLengthScaling
				Make a dendrogram instead of a dendrogram.
	
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

// The color alphabet that was proposed in "A Colour Alphabet and the Limits of Colour Coding" by Paul Green-Armytage 10 August 2010 
// The color alphabet capitalizes the number of visibly unqiue colors. 
var colorAlphabet = ["#015eff", "#fc0a18","#0cc402", "#aea7a5", "#ff15ae", "#d99f07", "#11a5fe", "#037e43", "#ba4455", "#d10aff", "#9354a6", "#7b6d2b", "#08bbbb", "#95b42d", "#b54e04", "#ee74ff", "#2d7593", "#e19772","#fa7fbe", "#fe035b", "#aea0db", "#905e76", "#92b27a", "#03c262"];


if (!d3) { throw "d3 wasn't included!";}
(function(){
	
	
	d3.dendrogram = {}; 
	var colors=d3.scale.category20().range(colorAlphabet); 
	var legendRectSize=10;
	var legendSpacing=3;
	var currentLegend=-1;
	var radius=0; 
	
	d3.dendrogram.rightAngleDiagonal=function (){
		var projection = function(d) { return [d.y, d.x]; };
    
		var path = function(pathData) {
			return "M" + pathData[0] + ' ' + pathData[1] + " " + pathData[2];
			};
		
		function diagonal(diagonalPath, i) {
			var source = diagonalPath.source,
				target = diagonalPath.target,
				midpointX = (source.x + target.x) / 2,
				midpointY = (source.y + target.y) / 2,
				pathData = [source, {x: target.x, y: source.y}, target];
			pathData = pathData.map(projection);
			return path(pathData);
			}
		
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
						
	function componentToHex(c) {
		var hex = c.toString(16);
		return hex.length == 1 ? "0" + hex : hex;
		}

	function rgbToHex(r, g, b) {
		return "#" + componentToHex(r) + componentToHex(g) + componentToHex(b);
		}
	
	d3.dendrogram.leafColors = function(val, layer, shift,title, selector){
		
		var vis = d3.select(selector).select("svg").select("g").selectAll("g.leaf");
		var first = 1;
		var node = vis.selectAll("g."+ layer + "Leaf");
		//var node = vis.selectAll(layer + "Leaf");
		

		colors=d3.scale.category20().range(colorAlphabet); 

		node.selectAll("circle")
			.style("fill", function(d){
				// This block links the extra leaf json fields to a specific value (count). This value
				// is provided when the buttons are written in the main function. 
				if (d.name != " "){	
					if (val==1)
						{ 
						vis.selectAll("text."+layer+"LegendTitle").remove(); 
						vis.selectAll("text."+layer+"LegendTitleLabel").remove(); 
						return d3.rgb(d.colorGroup);
						}
					if (val==-1)
						{
						vis.selectAll("text."+layer+"LegendTitle").remove(); 
						vis.selectAll("text."+layer+"LegendTitleLabel").remove(); 
						return "none";
						}
					var count = 0;
					for (var key in d) {
						if (d.hasOwnProperty(key)){
							if (key != "x" && key!="y" && key!= "name" && key!="kids" && key!="length" && key!="colorGroup" && key!="parent" && key!="depth" && key!="rootDist"){
								if (count == val){
									var sub = d[key]; 
									if (typeof d[key] === 'string' && d[key].length > 20)
										{
										sub = d[key].substring(0,19)+"...";
										}
									count += 1;
									return colors(sub);
									}
								}
							count += 1;
							}
						}
					}
					else{return 'none';}
				})
		.style("stroke-width", ".1px") 
		.attr("transform","translate(" + shift + "," + 0 + ")");
		updateLegend(colors.domain(), layer, (d3.select(selector).select("svg.legendSvg").select("g")), title, shift);
		colors=d3.scale.category20();
		};
	
	d3.dendrogram.nodeColors = function(val, selector){
		var vis = d3.select(selector).selectAll("svg");
		var node=vis.selectAll("g.internalNode").selectAll("circle")
		.style("fill",function(d){
			if (d.name === " "){
				if (val==2) return d3.rgb(d.whiteToBlack);
				if (val==5) return d3.rgb(d.whiteToBlack);
				if (val==3) return d3.rgb(d.whiteToBlackSqrt);
				if (val==4) return d3.rgb(d.whiteToBlackQuad);
				}
			})
		.attr("r", function(d){
			if (d.name === " "){
				if (val ==5) return 1; 	
				else return 1 + Math.sqrt(d.kids);
				}
			}) 
		;
		};
	
	updateLegend = function(val, layer, vis, title, shift){
		// Remove the old legend, title and label.  
		vis.selectAll('g.' + layer + 'Legend').remove();
		vis.select("text." + layer+ "LegendTitle").remove(); 
		vis.select("text." + layer+ "LegendLabel").remove(); 
		
		// Make the new legend
		var legend=vis.selectAll('g.'+layer+'Legend').data(val); // Define the DOM elements
		var horz, vert;
		var first = 1; // Use this as a boolean to only draw the title and label one time. 
		var viewerWidth = $(document).width();	// Scale to fit width
		var viewerHeight = $(document).height();  // Scale to fit height
		
		legend.enter() // Fill out the DOM elements 
			.append('g')
				.attr('class',layer+ 'Legend')
				.attr('transform', function (d, i){
					var height=legendRectSize + legendSpacing;
					var offset= height * colors.domain().length / 2; 
					vert = (i * height - offset)+ (radius*(1/2)) + 80; // All legends are the same height, the width location varies	
					// Asign the legend width for each ring, scale to fit. 
					if (layer=="inner") horz= (viewerWidth*(1/35)); 
					if (layer=="middle") horz= (viewerWidth*(3/35)); 
					if (layer=="outer") horz= (viewerWidth*(5/35)); 
					if (first)
						{
						vis.append("text")// The layer, this is either 'inner', 'middle', or 'outer'.  
							.attr('class',layer+'LegendTitle')
							.style("font-size","16px")
							.style("font-weight","bold")
							.attr("transform", "translate(" + (horz-45) + "," + (vert-35)+ ")")
							.attr('x', legendRectSize + legendSpacing)
							.attr('y', legendRectSize - legendSpacing)
							.text(layer); 

						vis.append("text")// The title, this is mined from the meta data
							.attr('class',layer+'LegendLabel')
							.style("font-size","16px")
							.style("font-weight","bold")
							.attr("transform", "translate(" + (horz-45) + "," + (vert-15)+ ")")
							.attr('x', legendRectSize + legendSpacing)
							.attr('y', legendRectSize - legendSpacing)
						
							.text(function (){
								if (title.length > 15){
									return title.substring(0,14)+"...";
									} 
								return title; 
								});
						first =0;
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
  
	// The javascript buttons are created at runtime based on the .json file provided. This opens up several cans of worms.
	//
	// The addLeafButton function is used within this program to write buttons, the buttons themselves use the globally available 
	// functions d3.dendrogram.leafColors and d3.dendrogram.nodeColors to apply the colors.  Note that these buttons exist only when the 
	// program is being run, they are not present in the basic html.  
	addLeafButton = function (title, count, layer, shift, dropdownSelector, graph) {
		var leafButton = "<button onclick=\"d3.dendrogram.leafColors("+count+",\'"+layer+"\',"+shift+", \'"+title+"\', \'"+graph+"\')\">"+title+"</button>"; 
		d3.select(dropdownSelector).append("li")
		    .html(leafButton); 
	};
	
	addNodeButton = function (dropdownSelector, val, graph, title) {
		d3.select(dropdownSelector).append("li").html("<button onclick=\"d3.dendrogram.nodeColors("+val+",'"+graph+"')\">"+title+"</button>"); 
	};

	// This is the first function that is run, it creates the dropdown skeleton which is populated at a later point. 
	makeDropdownSkeleton = function (treeType){

		var dropdown = d3.select("#dropdown").append("ul").attr("style","list-style-type:none;display:inline-flex");
		d3.select("#dropdown").attr("style","position:fixed;z-index:2"); 		

		if (treeType == "Dendro"){
			var interiorNodeDropdown = dropdown.append("li").attr("class","dropdown");
				interiorNodeDropdown.append("button").attr("class","btn btn-default dropdown-toggle")
					.attr("type", "button").attr("data-toggle","dropdown").html("Inner "+treeType+" nodes").append("span").attr("class","caret"); 
				interiorNodeDropdown.append("ul").attr("class","dropdown-menu").html("<div id=interiorDendroNodes </div>"); 
			}
		
		var innerDropdown = dropdown.append("li").attr("class","dropdown");
			innerDropdown.append("button").attr("class","btn btn-default dropdown-toggle")
				.attr("type", "button").attr("data-toggle","dropdown").html("Inner "+treeType+" leaves").append("span").attr("class","caret"); 
			innerDropdown.append("ul").attr("class","dropdown-menu").html("<div id=inner"+treeType+"Leaves </div>"); 
		
		var middleDropdown = dropdown.append("li").attr("class","dropdown");
			middleDropdown.append("button").attr("class","btn btn-default dropdown-toggle")
				.attr("type", "button").attr("data-toggle","dropdown").html("Middle "+treeType+"  leaves").append("span").attr("class","caret"); 
			middleDropdown.append("ul").attr("class","dropdown-menu").html("<div id=middle"+treeType+"Leaves </div>"); 
		
		var outerDropdown = dropdown.append("li").attr("class","dropdown");
			outerDropdown.append("button").attr("class","btn btn-default dropdown-toggle")
				.attr("type", "button").attr("data-toggle","dropdown").html("Outer "+treeType+" leaves").append("span").attr("class","caret"); 
			outerDropdown.append("ul").attr("class","dropdown-menu").html("<div id=outer"+treeType+"Leaves </div>"); 
	
	
		};

	// This function generates the cartesian dendrogram (flat).  
	d3.dendrogram.makeCartesianDendrogram =function (selector, data, options) {
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

		var tree = options.tree || d3.layout.cluster()
		    .size([h, w])
			.sort(function(node) { return node.children ? parseInt(node.children.length) : -1; })
		    .children(options.children || function(node) {
				return node.children;
		  		});
		

		var diagonal = options.diagonal || d3.dendrogram.rightAngleDiagonal();
		
		var vis = options.vis || d3.select(selector)
			.append("svg")
				.attr("width", viewerWidth)
				.attr("height", viewerHeight)
				.attr("class", "overlay")
				.call(zoomListener); 
		
		d3.select(selector).select("svg").append ("rect")
			.attr("width",w).attr("height",h)
			.style("fill", "none")
			.style("stroke","black")
			.style("stroke-width","5")
			.style("opacity","0.5");
		  
		var svgGroup = vis.append("g")
				.attr("transform", "translate(20, 20)");
		
		d3.select(selector).select("svg").on("wheel.zoom",null); 
		d3.select(selector).select("svg").on("mousewheel.zoom",null); 

		var nodes = tree(data);
		var legendRectSize = 6; 
		var legendSpacing = 6; 
		var yscale; 	
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
											addLeafButton(key, count, 'inner', 0, '#innerPhyloLeaves', '#phylogram') ;
											addLeafButton(key, count, 'middle', 0, '#middlePhyloLeaves', '#phylogram'); 
											addLeafButton(key, count, 'outer', 0, '#outerPhyloLeaves', '#phylogram'); 
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
				.style("fil", 'none');
		
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
		function zoom() {
			svgGroup.attr("transform", "translate(" + d3.event.translate + ")scale(" + d3.event.scale + ")");
			}
			
		options = options || {};
		
		var viewerWidth = $(document).width(); // Scale to fit.
		var viewerHeight = $(document).height();  // Scale to fit. 
		
		var calc = Math.min(viewerWidth, viewerHeight);
		
		radius = (calc -50)/ 2; // Global for the legend placement. See function updateLegend.
		var tree = options.tree || d3.layout.cluster()
			.size([360, radius - 45]);
		
		
		var zoomListener = d3.behavior.zoom().scaleExtent([1, 6])
			.on("zoom", zoom); 
		
		var diagonal=d3.svg.diagonal.radial()
			.projection(function(d){return [d.y, d.x / 180 * Math.PI];});
		
		var width =  viewerWidth -25,
			height = viewerHeight - 50; 

		var metaHorz = (viewerWidth)/8,
			metaVert = 6*(viewerHeight)/8; 
		
		var vis = options.vis || d3.select(selector)
			.append("svg")
				.attr("width", 3*width/5 )
				.attr("height", height)
				.attr("class", "overlay")
				.call(zoomListener);
		
		var legendVis = options.vis || d3.select(selector)
			.append("svg")
			.attr("class", "legendSvg")
			.attr("width", 2*width/5)
			.attr("height", height); 

		vis.append ("rect")
			.attr("width", 3*width/5)
			.attr("height", height)
			.style("fill", "none")
			.style("stroke","black")
			.style("stroke-width","5")
			.style("opacity","0.5");
		  
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

		var tooltip = d3.select(selector)
			.append('div')
			.attr('class', 'tooltip')
			.style("opacity",0);

		var legendRectSize=20;
		var legendSpacing=4;
		var currentLegend=-1;
		var nodes=tree.nodes(data);
		
		var link=svgGroup.selectAll("path.link")
			.data(tree.links(nodes)).enter()
			.append("path")
				.attr("class", "link")
				.attr("fill", "none")
				.attr("stroke", "black")
				.attr("stroke-width", ".1px")
				.attr("d", diagonal); 	
		
		makeDropdownSkeleton("Dendro"); //Looks for the div 'dropdown' and generates the outer list and buttons
		// For the radial display the interior node color options are constant (calculated on the backend in the C code)
		addLeafButton("Remove ring", -1, 'inner', 10, '#innerDendroLeaves', '#dendrogram'); 
		addLeafButton("Remove ring", -1, 'middle', 15, '#middleDendroLeaves', '#dendrogram'); 
		addLeafButton("Remove ring", -1, 'outer', 20, '#outerDendroLeaves', '#dendrogram') ;
		addLeafButton("Distance", 1, 'inner', 10, '#innerDendroLeaves', '#dendrogram'); 
		addLeafButton("Distance", 1, 'middle', 15, '#middleDendroLeaves', '#dendrogram'); 
		addLeafButton("Distance", 1, 'outer', 20, '#outerDendroLeaves', '#dendrogram') ;
		
		addNodeButton("#interiorDendroNodes" ,2, selector, "Tpm distance"); 
		addNodeButton("#interiorDendroNodes" ,3, selector, "Square root tpm distance"); 
		addNodeButton("#interiorDendroNodes" ,4, selector, "Quad root tpm distance"); 
		addNodeButton("#interiorDendroNodes" ,5, selector, "Make nodes smaller"); 
					
		var first = 1; 


		// These are true nodes in the sense that they are actually calculated and defined, 
		// however this code is not repsonsible for the actual visuale representation. This
		// block only handles the backend DOM construction, the shapes and colors are assigned later. 
		var trueNodes = svgGroup.selectAll("g")
			.data(nodes).enter()
			.append("g")
				.attr("class", function(n) {
			  		if (n.children) {
				  		return "internalNode";
						}
					else{
						if (first){
							var count = 0;
							for (var key in n) {
								if (n.hasOwnProperty(key)) { 
								// Give each dropdown button a bunch of options. 
									if (key != "x" && key!="y" && key!= "name" && key!="kids" && key!="length" && key!="colorGroup" && key!="parent" && key!="depth" && key!="rootDist"){
										addLeafButton(key, count, 'inner', 10, '#innerDendroLeaves', '#dendrogram'); 
										addLeafButton(key, count, 'middle', 15, '#middleDendroLeaves', '#dendrogram'); 
										addLeafButton(key, count, 'outer', 20, '#outerDendroLeaves', '#dendrogram') ;
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
		var filled = 1; 
		var prevSel, prevSel2;
		var interiorNodes = svgGroup.selectAll("g.internalNode")
			.append("circle")
				.attr("r", function(d){ // Handle node size
					if (d.name==" "){
					return 1 + Math.sqrt(d.kids);}
					else{return 0;}
					})
				.style("fill", function(d){ // Handle node color 
					if (d.name==" "){return d.whiteToBlack;}
					else{return "none";}})
				.on("click", function(d) { // Tooltip stuffs
					d3.select(this).style("fill", "red"); 
					if (filled == 2) {
						if (prevSel2.name==" "){prevSel.style("fill", prevSel2.whiteToBlack);}
						else{prevSel.style("fill", "white");}
						} 
					tooltip.transition()
					.duration(200)
					.style("opacity", 0.9);
					result = "<table border=\"1\">"; // Make the tooltip a table, the first section is the basic meta data
					count = 1;
					result = result +"<tr><td colspan=\"4\" style=\"font-weight:bold;font-size:14px;text-align:center\">Node meta data</td>"; 
					for (var key in d) {
						if (d.hasOwnProperty(key)) {
							if (key == "number" || key == "rootDist" || key == "kids" || key == "tpmDistance" || key == "length" || key == "contGenes" || key == "normalizedDistance" || key == "depth")
								{
								if (count == 1){
									count = 2; 
									result = result +"<tr><td style=\"font-weight:bold;font-size:10px;\">" + key + "</td><td style=\"font-weight:bold;font-size:10px;\">" + d[key] + "</td>"; 
									}
								else{
									count = 1; 
									result = result +"<td style=\"font-weight:bold;font-size:10px;\">" + key + "</td><td style=\"font-weight:bold;font-size:10px;\">" + d[key] + "</td></tr>"; 
									}
								}
							}
						}
					count = 1;
					// This section handles the tooltip representation for the top contributing genes.  
					result = result +"<tr><td colspan=\"2\" style=\"font-weight:bold;font-size:14px\">Top 10 contributing genes </td><td colspan=\"2\" style=\"font-weight:bold;font-size:14px\">TPM value contributed</td>"; 
					for (var n in d.geneList){
						if (count == 1){
							count = 2; 
							result = result +"<tr><td style=\"font-weight:bold;font-size:10px\">" + n + "</td><td style=\"font-weight:bold;font-size:10px\">" + d.geneList[n] + "</td>"; 
							}
						else{
							count = 1; 
							result = result +"<td style=\"font-weight:bold;font-size:10px\">" + n + "</td><td style=\"font-weight:bold;font-size:10px\">" + d.geneList[n] + "</td></tr>"; 
							}
						}
					result = result + "</table"; 
					tooltip.html(result)
					.style("right", metaHorz + "px")
					.style("top", metaVert + "px"); 
					filled = 2;
					prevSel2 = d; 
					prevSel = d3.select(this); 
					})
				.style("stroke", "black")
				.style("stroke-width", ".1px"); 
		
		var leaves = svgGroup.selectAll("g.leaf");
		
		leaves
			.append("g")
			.attr("class", "trueLeaf")
			.append("circle")
			.attr("r", function(d){
				if (d.name !==" "){return 1;}})
			.style("fill", function(d){
				if (d.name ==" "){return "d.whiteToBlack";}
				else { return "white";}
				})
			.style("stroke", "black")
			.style("stroke-width", ".1px")
			.on("click", function(d) {
				d3.select(this).style("fill", "red"); 
				if (filled == 2)
					{
					if (prevSel2.name==" "){prevSel.style("fill", prevSel2.whiteToBlack);}
					else{prevSel.style("fill", "white");}
					}
				tooltip.transition()
				.duration(200)
				.style("opacity", 0.9);
				result = "<table border=\"1\">";  
				count = 1;
				result = result +"<tr><td colspan=\"4\" style=\"font-weight:bold;font-size:14px;text-align:center\">Leaf meta data</td>"; 
				for (var key in d) {
					if (d.hasOwnProperty(key)) {
						if (key != "x" && key!="y" && key!="kids" && key!="colorGroup" && key!="parent")
							{
							var sub1 = key, sub2 = d[key]; 
							if (typeof key === 'string' && key.length > 20)
								{
								sub1 = key.substring(0,19)+"...";
								}
							if (typeof d[key] === 'string' && d[key].length > 20)
								{
								sub2 = d[key].substring(0,19)+"...";
								}
							if (count == 1){
								count = 2; 
								result = result +"<tr><td style=\"font-weight:bold;font-size:10px;\">" + sub1 + "</td><td style=\"font-weight:bold;font-size:10px;\">" + sub2 + "</td>"; 
								}
							else{
								count = 1; 
								result = result +"<td style=\"font-weight:bold;font-size:10px;\">" + sub1 + "</td><td style=\"font-weight:bold;font-size:10px;\">" + sub2 + "</td></tr>"; 
								}
							}
						}
					}
				result = result + "</table"; 
				tooltip.html(result)
				.style("right", metaHorz + "px")
				.style("top", metaVert + "px"); 
				filled = 2;
				prevSel = d3.select(this); 
				prevSel2 = d; 
				});

		leaves
			.append("g")
			.attr("class", "outerLeaf")
			.append("circle")
				.attr("r", 2)
				.style("fill","none");
		
		leaves
			.append("g")
			.attr("class", "middleLeaf")
			.append("circle")
				.attr("r", 2)
				.style("fill","none");
		
		leaves
			.append("g")
			.attr("class", "innerLeaf")
			.append("circle")
				 .attr("r",2)
				 .style("fill","none");

		
		return {tree: tree, vis: svgGroup};
		};
})();
