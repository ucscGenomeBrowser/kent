// hgCollection.js - Interactive features for GTEX Body Map version of GTEx Gene track UI page

// Copyright (C) 2017 The Regents of the University of California

var collections = (function() {
    var names = []; // a list of names that have been used
    var selectedNode = "collections"; // keep track of id of selected row
    var selectedTree = "collections"; // keep track of id of selected row
    var $tracks;  // the #tracks object
    var trees = [];

    function customApply() {
        // called when the apply button on the track settings dialog is pressed
        selectedNode.li_attr.shortlabel = $("#customName").val();
        selectedNode.li_attr.longlabel = $("#customDescription").val();
        selectedNode.li_attr.visibility = $("#customVis").val();
        selectedNode.li_attr.color = $("#customColorInput").val();
    }

    function hideAllAttributes() {
        // hide all the "set attribute" dialogs
        $("#viewOptions").hide();
        $("#CustomCompositeOptions").hide();
        $("#CustomTrackOptions").hide();
        $("#TrackDbOptions").hide();
    }

    function selectTreeNode(evt, data) {
        // called when a node in the collections tree is selected
        var color = data.node.li_attr.color;
        var name =  data.node.li_attr.shortlabel;
        var description = data.node.li_attr.longlabel;
        var visibility = data.node.li_attr.visibility;
        var type = data.node.li_attr.viewtype;
        selectedNode = data.node;
        selectedTree = evt.target;

        if (!type) {
                hideAllAttributes();
                $("#viewOptions").show();
                $("#viewName").val(name);
                $("#viewDescription").val(description);
                $("#viewVis").val(visibility);
                $("#viewColorInput").val(color);
                $("#viewColorPicker").spectrum("set", color);
        } else if (type == 'collection') {
                hideAllAttributes();
                $("#CustomCompositeOptions").show();
                $("#collectionName").val(name);
                $("#collectionDescription").val(description);
                $("#collectionVis").val(visibility);
        } else  if (type == 'track') {
            hideAllAttributes();
            $("#CustomTrackOptions").show();
            $("#customName").val(name);
            $("#customDescription").val(description);
            $("#customVis").val(visibility);
            $("#customColorInput").val(color);
            $("#customColorPicker").spectrum("set", color);
        } else {
            hideAllAttributes();
            $("#TrackDbOptions").show();
        }
   }           

    function checkCallback( operation, node, node_parent, node_position, more) {
        // called during a drag and drop action to see if the target is droppable
        if (operation === "move_node") {
            if ($(node_parent).hasClass('nodrop'))
                return false;
            if ($(node_parent).attr('parent') !== '#')
                return false;
        }
        return true;
    }

    function newCollection() {
        // called when the "New Collection" button is pressed
        var ourCollectionName = getUniqueName("coll");
        var ourTreeName = getUniqueName("tree");
        var newName = "A New Collection";
        var newDescription = "Description of New Collection";
        var attributes = "shortLabel='" +  newName + "' ";
        attributes += "longLabel='" +  newDescription + "' ";
        attributes += "color='" + "#0" + "' ";
        attributes += "viewType='" + "collection" + "' ";
        attributes += "visibility='" + "full" + "' ";
        attributes += "name='" +  ourCollectionName + "' ";

        $('#collections').append("<li " + attributes +  "id='"+ourCollectionName+"'>A New Collection</li>");
        $('#collection').append("<div id='"+ourTreeName+"'><ul><li " + attributes+ ">A New Collection</li><ul></div>");
        var newTree = $('#collection div:last');
        trees[ourCollectionName] = newTree;
        $(newTree).jstree({
               "core" : {
                     "check_callback" : checkCallback
                         },
               'plugins' : ['dnd', 'conditionalselect', 'contextmenu'],
               'check_callback' : checkCallback,
               'dnd': {check_while_dragging: true}
        });
       $(newTree).on("select_node.jstree", selectTreeNode);
    }

    function hideAllTrees() {
        // hide all the trees in the Collected Tracks window
        for(var key in trees)
            trees[key].hide();
    }

    function selectCollection(event, ui ) {
        // called with a collection is selected
        var id = ui.selected.id;
        $('#collectedTracksTitle').text(ui.selected.innerText);
        hideAllTrees();
        trees[id].show();
        trees[id].jstree('select_node', trees[id].find("li").first());

    }

    function addCollection(trees, list) {
        // called when outputting JSON of all the collections
        var collectTree = trees[list.id];
        var v = collectTree.jstree(true).get_json('#', {flat:true, no_data:true, no_state:true, no_a_attr:true});
        var mytext = JSON.stringify(v);
        return mytext;
    }

    function saveCollections(trees) {
       // called when the "Save" button is pressed
       var json = "[";
       $('#collections li').each(function() {
            json += addCollection(trees, this ) + ',';
        });
        json = json.slice(0, -1);
        json += ']';
        console.log(json);
        var requestData = 'jsonp=' + json;
        $.ajax({
            data:  requestData ,
            async: false,
            dataType: "JSON",
            type: "PUT",
            url: "hgCollection?cmd=saveCollection",
            trueSuccess: updatePage,
            success: catchErrorOrDispatch,
            error: errorHandler,
        });
    }

    function rebuildLabel() {
        var newText = selectedNode.li_attr.shortlabel + "   " + selectedNode.li_attr.longlabel;
        $(selectedTree).jstree('rename_node', selectedNode, newText);
    }

    function descriptionChange() {
        selectedNode.li_attr.longlabel = $("#customDescription").val();
        rebuildLabel();
    }

    function nameChange() {
        selectedNode.li_attr.shortlabel = $("#customName").val();
        rebuildLabel();
        if (selectedNode.parent === '#') {
            $("#collections .ui-selected").text($("#customName").val());

        }
    }

    function colorChange() {
        var color = $("#customColorPicker").spectrum("get"); $('#customColorInput').val(color);
        selectedNode.li_attr.color = $("#customColorInput").val();
    }

    function visChange() {
        selectedNode.li_attr.visibility = $("#customVis").val();
    }

    function isDraggable(nodes) {
        var ii;
        for (ii=0; ii < nodes.length; ii++)
            if (nodes[ii].children.length !== 0)
                return false;
        return true;
    }

    function init() {
        // called at initialization time
        $("#customName").change(nameChange);
        $("#customDescription").change(descriptionChange);
        $("#customVis").change(visChange);
        //$("#customColorInput").change(colorChange);
        $("#saveCollections").click ( function() {saveCollections(trees);} );
        $("#discardChanges").click ( function () { window.location.reload(); });

        $("#newCollection").click ( newCollection );
        $("#collectionApply").click ( collectionApply );
        $("#customApply").click ( customApply );
        $('#collections').selectable({selected : selectCollection});

        var trackOpt = {
            hideAfterPaletteSelect : true,
            color : $('#customColorInput').val(),
            showPalette: true,
            showInput: true,
            preferredFormat: "hex",
            change: colorChange,
//            change: function() { var color = $("#customColorPicker").spectrum("get"); $('#customColorInput').val(color); },
        };

        $("#customColorPicker").spectrum(trackOpt);
        //$.jstree.defaults.core.themes.icons = false;
        //$.jstree.defaults.core.check_callback = true;
        $.jstree.defaults.core.themes.dots = true;
        $.jstree.defaults.contextmenu.show_at_node = false;
        $("#collection div").each(function(index) {
            //$("#collection").append($(this).clone());
            var newTree = this;

            $(newTree).jstree({
               'plugins' : ['dnd', 'conditionalselect', 'contextmenu'],
               'core': {
                "check_callback" : checkCallback,
                }
            });
            trees[this.id] = $(newTree);
           $(newTree).on("select_node.jstree", selectTreeNode);
        });
        $("#collection  li").each(function() {
            names[this.getAttribute("name")] = 1;
        });
        treeDiv=$('#tracks');
        treeDiv.jstree({
               'plugins' : ['dnd', 'conditionalselect', 'contextmenu'],
               'dnd': {
                "check_callback" : checkCallback,
               'always_copy' : true,
                is_draggable: isDraggable,
               },
               'core' :  {
                "check_callback" : checkCallback
            }
        });

        $("#collections li").first().addClass( "ui-selected" );
        hideAllTrees();
        var id = $("#collections li").first().attr('id');
        trees[id].show();
        selectedNode = trees[id].find("li").first();
        selectedTree = trees[id];
        trees[id].jstree('select_node', selectedNode);
        $('#collectedTracksTitle').text(trees[id].attr('shortlabel'));
    }

    function updatePage(responseJson) {
        // called after AJAX call
        if (!responseJson) {
            return;
        }
        var message = responseJson.serverSays;
        if (message) {
            console.log('server says: ' + JSON.stringify(message));
        }
    }

    function getUniqueName(root) {
        // make sure name is unique in track hub
        if (!names[root]) {
            names[root] = true;
            return root;
        } else {
            var counter = 1;

            for(;;counter++) {
                var name  = root + counter;
                if (!names[name]) {
                    names[name] = true;
                    return name;
                }
            }
        }
    }

    return {
        init: init
    };
}());
