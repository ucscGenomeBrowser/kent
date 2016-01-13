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

if (!d3) { throw "d3 wasn't included!";}
(function(){
	d3.dendrogram = {}; 
  
	var colors=d3.scale.category20();
	var legendRectSize=20;
	var legendSpacing=4;
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
	
	d3.dendrogram.leafColors = function(val, layer, shift, selector){
		colors = d3.scale.category20();
		var vis = d3.select(selector).select("svg");
		var first = 1;
		var node = vis.selectAll("g."+ layer + "Leaf");

		node.selectAll("circle")
			.style("fill", function(d){
				// This block links the extra leaf json fields to a specific value (count). This value
				// is provided when the buttons are written in the main function. 
				if (d.name != " "){	
					var count = 0;
					for (var key in d) {
						if (d.hasOwnProperty(key)){
							if (key != "x" && key!="y" && key!= "name" && key!="kids" && key!="length" && key!="colorGroup" && key!="parent" && key!="depth" && key!="rootDist"){
								if (count == val){
									count += 1;
									return colors(d[key]); 
									}
								}
							count += 1;
							}
						}
					}
					else{return 'none';}
				})
		.attr("transform","translate(" + shift + "," + 0 + ")");
		
		var legend=vis.selectAll('g.' + layer + 'Legend').remove();
		updateLegend(colors.domain(), layer, vis, shift);
		colors=d3.scale.category20();
		};
	
	d3.dendrogram.nodeColors = function(val, selector){
		var vis = d3.select(selector).selectAll("svg");
		var node=vis.selectAll("g.internalNode").selectAll("circle")
		.style("fill",function(d){
			if (d.name === " "){
				if (val==1) return d3.rgb(d.colorGroup);
				if (val==2) return d3.rgb(d.whiteToBlack);
				if (val==3) return d3.rgb(d.whiteToBlackSqrt);
				if (val==4) return d3.rgb(d.whiteToBlackQuad);
				}
			});
		};
	
	updateLegend = function(val, layer, vis, shift){
		var legend=vis.selectAll('g.'+layer+'Legend').data(val);
		
		legend.enter() 
			.append('g')
				.attr('class',layer+ 'Legend')
				.attr('transform', function (d, i){
					var height=legendRectSize + legendSpacing;
					var offset= height * colors.domain().length / 2;
					var horz, vert;
					if (layer=="inner"){
						horz=(-2 * legendRectSize) + radius;
						vert=-(i * height - offset) + (radius*(1/2));
					}
					if (layer=="middle"){
						horz=(-2 * legendRectSize) + radius + 75;
						vert=-(i * height - offset) + (radius*(1/2));
					}
					if (layer=="outer"){
						horz=(-2 * legendRectSize) + radius + 150;
						vert=-(i * height - offset)+ (radius*(1/2));
					}
					return 'translate(' + horz + ',' + vert + ')';
					});
			
		legend.append('rect')
			.attr('width', legendRectSize)
			.attr('height', legendRectSize)
			.style('fill', colors)
			.style('stroke', colors);
		
		legend.append('text')
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
  
	addLeafButton = function (title, count, layer, shift, dropdownSelector, graph) {
		d3.select(dropdownSelector).append("li").html("<button onclick=\"d3.dendrogram.leafColors("+count+",\'"+layer+"\',"+shift+", \'"+graph+"\')\">"+title+"</button>"); 
	};
	
	addNodeButton = function (dropdownSelector, val, graph, title) {
		d3.select(dropdownSelector).append("li").html("<button onclick=\"d3.dendrogram.nodeColors("+val+",'"+graph+"')\">"+title+"</button>"); 
	};

	makeDropdownSkeleton = function (treeType){
		var dropdown = d3.select("#dropdown").append("ul").attr("style","list-style-type:none;display:inline-flex");
		
		if (treeType == "Dendro"){
			var interiorNodeDropdown = dropdown.append("li").attr("class","dropdown");
				interiorNodeDropdown.append("button").attr("class","btn btn-default dropdown-toggle")
					.attr("type", "button").attr("data-toggle","dropdown").html("Inner nodes").append("span").attr("class","caret"); 
				interiorNodeDropdown.append("ul").attr("class","dropdown-menu").html("<div id=interiorDendroNodes </div>"); 
			}
		
		var innerDropdown = dropdown.append("li").attr("class","dropdown");
			innerDropdown.append("button").attr("class","btn btn-default dropdown-toggle")
				.attr("type", "button").attr("data-toggle","dropdown").html("Inner nodes").append("span").attr("class","caret"); 
			innerDropdown.append("ul").attr("class","dropdown-menu").html("<div id=inner"+treeType+"Leaves </div>"); 
		
		var middleDropdown = dropdown.append("li").attr("class","dropdown");
			middleDropdown.append("button").attr("class","btn btn-default dropdown-toggle")
				.attr("type", "button").attr("data-toggle","dropdown").html("Middle nodes").append("span").attr("class","caret"); 
			middleDropdown.append("ul").attr("class","dropdown-menu").html("<div id=middle"+treeType+"Leaves </div>"); 
		
		var outerDropdown = dropdown.append("li").attr("class","dropdown");
			outerDropdown.append("button").attr("class","btn btn-default dropdown-toggle")
				.attr("type", "button").attr("data-toggle","dropdown").html("Outer nodes").append("span").attr("class","caret"); 
			outerDropdown.append("ul").attr("class","dropdown-menu").html("<div id=outer"+treeType+"Leaves </div>"); 
		};


	d3.dendrogram.makeCartesianDendrogram =function (selector, data, options) {
		options = options || {};
		var w = options.width || d3.select(selector).style('width') || d3.select(selector).attr('width'),
			h = options.height || d3.select(selector).style('height') || d3.select(selector).attr('height');
		
		var tree = options.tree || d3.layout.cluster()
		    .size([h, w])
		    .sort(function(node) { return node.children ? parseInt(node.children.length) : -1; })
		    .children(options.children || function(node) {
				return node.children;
		  		});

		var diagonal = options.diagonal || d3.dendrogram.rightAngleDiagonal();
		
		var vis = options.vis || d3.select(selector).append("svg")
			.attr("width", w + 300)
			.attr("height", h + 30)
		  .append("g")
			.attr("transform", "translate(20, 20)");
		
		var nodes = tree(data);
		
		var legendRectSize = 10; 
		var legendSpacing = 10; 
		
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
		  	vis.selectAll('line')
				.data(yscale.ticks(10))
				.enter().append('svg:line')
			  		.attr('y1', 0)
			  		.attr('y2', h)
			  		.attr('x1', yscale)
			  		.attr('x2', yscale)
			  		.attr("stroke", "#ddd");

		  	vis.selectAll("text.rule")
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
			
		var link = vis.selectAll("path.link")
			.data(tree.links(nodes))
		  	.enter().append("svg:path")
				.attr("class", "link")
				.attr("d", diagonal)
				.attr("fill", "none")
				.attr("stroke", "#aaa")
				.attr("stroke-width", "4px");
			
		var first = 1; 
		
		makeDropdownSkeleton("Phylo");
		
		var innerNodes = vis.selectAll("g.innerNode")
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
		
		var innerLeaves = vis.selectAll("g.innerLeaf")
			.append("circle")
				.attr("r", '3') 
				.style("fil", 'none');
		
		var middleLeaves = vis.selectAll("g.middleLeaf")
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
			
		vis.selectAll("g.middleLeaf")
			.attr("transform", function(d) { return "translate(" + (d.y +10 ) + "," + d.x + ")"; })
			.append("circle")
				.attr("r", '3')
				.style("fil", 'none'); 
		
		
		var outerLeaves = vis.selectAll("g.outerLeaf")
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
			
		vis.selectAll("g.outerLeaf")
			.append("svg:circle")
				.attr("r", '3')
				.attr('stroke',  'black')
				.style("stroke-width", ".25px")
				.attr('stroke-width',  '.25px')
				.attr('fill', 'greenYellow')
				.attr('fill', 'black')
				.attr('stroke-width', '2px');
		
		vis.selectAll('g.root.node')
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
		options = options || {};
		radius = (options.radius || 800) / 2; // Global for the legend placement. See function updateLegend.
		var cluster = d3.layout.cluster().size([360, radius - 40]);
		var tree = options.tree || d3.layout.cluster()
			.size([360, radius - 40]);
		
		var diagonal=d3.svg.diagonal.radial()
			.projection(function(d){return [d.y, d.x / 180 * Math.PI];});
		
		var vis=d3.select(selector)
			.append("svg")
				.attr("width", 200+(radius * 2))
				.attr("height", radius * 2) 
				.append("g") 
					.attr("transform","translate(" + (radius + 150) + "," + radius + ")");

		var legendRectSize=20;
		var legendSpacing=4;
		var currentLegend=-1;
		var nodes=cluster.nodes(data);
		
		var link=vis.selectAll("path.link")
			.data(cluster.links(nodes)).enter()
			.append("path")
				.attr("class", "link")
				.attr("fill", "none")
				.attr("stroke", "#aaa")
				.attr("stroke-width", "4px")
				.attr("d", diagonal); 	
		
		makeDropdownSkeleton("Dendro");//Looks for the div 'dropdown' and generates the outer list and buttons
		// For the radial display the interior node color options are constant (calculated on the backend in the C code)
		addNodeButton("#interiorDendroNodes" ,2, selector, "Tpm distance"); 
		addNodeButton("#interiorDendroNodes" ,3, selector, "Square root tpm distance"); 
		addNodeButton("#interiorDendroNodes" ,4, selector, "Quad root tpm distance"); 
					
		var first = 1; 

		var innerNodes = vis.selectAll("g")
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
									if (key != "x" && key!="y" && key!= "name" && key!="kids" && key!="length" && key!="colorGroup" && key!="parent" && key!="depth" && key!="rootDist"){
										addLeafButton(key, count, 'inner', 0, '#innerDendroLeaves', '#dendrogram'); 
										addLeafButton(key, count, 'middle', 15, '#middleDendroLeaves', '#dendrogram'); 
										addLeafButton(key, count, 'outer', 30, '#outerDendroLeaves', '#dendrogram') ;
										}
									}
								count += 1;
								}
							first = 0;
							}
						return "g.innerLeaf";
						}
					})
				.attr("transform", function(d){return "rotate(" + (d.x - 90) + ")translate(" + d.y + ")";});
			
		var interiorNodes = vis.selectAll("g.internalNode")
			.append("circle")
				.attr("r", function(d){
					if (d.name==" "){return 1 + Math.sqrt(d.kids);}
					else{return 0;}
					})
				.style("fill", function(d){
					if (d.name==" "){return d.whiteToBlack;}
					else{return "none";}})
				.style("stroke", "steelblue")
				.style("stroke-width", "1.25px");

		var innerLeaves = vis.selectAll("g.innerLeaf")
			.data(nodes).enter()
			.append("g")
				.attr("class", "innerLeaf")
				.attr("transform",function(d){return "rotate(" + (d.x - 90) + ")translate(" + d.y + ")";});

		innerLeaves.append("circle")
			.attr("r", function(d){
				if (d.name !==" "){return 5;}})
			.style("fill", function(d){
				if (d.name !==" "){return "d.whiteToBlack";}
				});

		var middleLeaves = vis.selectAll("g.middleLeaf")
			.data(nodes).enter()
			.append("g")
				.attr("class", "middleLeaf")
				.attr("transform",function(d){return "rotate(" + (d.x - 90) + ")translate(" + d.y + ")";});
		
		middleLeaves.append("circle")
			.attr("r", 5)
			.style("fill","none")
			.attr("transform","translate(" + 15 + "," + 0 + ")");

		var outerLeaves = vis.selectAll("g.outerLeaf")
			.data(nodes).enter()
			.append("g")
				.attr("class", "outerLeaf")
				.attr("transform",function(d){return "rotate(" + (d.x - 90) + ")translate(" + d.y + ")";})
				.append("circle")
					.attr("r", 5)
					.style("fill","none");
		
		return {tree: tree, vis: vis};
		};
})();
