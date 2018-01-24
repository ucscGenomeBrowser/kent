// hgCollection.js - Interactive features for GTEX Body Map version of GTEx Gene track UI page

// Copyright (C) 2017 The Regents of the University of California

var hgCollection = (function() {
    var names = []; // a list of names that have been used
    var selectedNode = "collectionList"; // keep track of id of selected row
    var selectedTree = "collectionList"; // keep track of id of selected row
    var $tracks;  // the #tracks object
    var trees = [];
    var isDirty = false;
    var goTracks = false;
    var doAjaxAsync = true;

    function currentTrackItems(node) {
        // populate the menu for the currentCollection tree
        var items = {
            addItem: { // The "add" menu item
                label: "Add",
                action: function () {
                    var nodeIds = $("#tracks").jstree( "get_selected");
                    isDirty = true;
                    var nodes = [];
                    var node;
                    for(ii=0; ii < nodeIds.length; ii++) {
                        node = $("#tracks").jstree('get_node', nodeIds[ii]);
                        if (node.children.length === 0)
                            nodes.push(node);
                    }
                    var parentId = $(selectedNode).attr('id');
                    $(selectedTree).jstree("copy_node", nodes, parentId,'last');
                }
            }
        };

        if ($(node).attr('children').length > 0)
            delete items.addItem;

        return items;
    }

    function currentCollectionItems(node) {
        // populate the menu for the currentCollection tree
        var items = {
            duplicateItem: { // The "duplicate" menu item
                label: "Duplicate",
                action: function () { 
                }
            },
            deleteItem: { // The "delete" menu item
                label: "Delete",
                action: function () {
                    var nodes = $(selectedTree).jstree( "get_selected");
                    isDirty = true;
                    $(selectedTree).jstree( "delete_node", nodes);
                }
            }
        };

        // can't delete root
        //if ($(node).attr('parent') === '#')
            //delete items.deleteItem;

        return items;
        }

    function changeCollection() {
        $( "#newCollectionDialog" ).dialog("close");
        selectedNode.li_attr.class = "folder";
        selectedNode.li_attr.shortlabel = $("#customName").val();
        selectedNode.li_attr.longlabel = $("#customDescription").val();
        selectedNode.li_attr.visibility = $("#customVis").val();
        selectedNode.li_attr.color = $("#customColorInput").val();
        selectedNode.li_attr.missingmethod = $("input:radio[name ='missingData']:checked").val();
        selectedNode.li_attr.viewfunc = $("#viewFunc").val();
        //newNode.li_attr.viewtype = "view";
        rebuildLabel();
    }

    function doubleClickNode(tree) {
        var node = $(selectedTree).jstree("get_node", tree.id);
        var color = node.li_attr.color;
        var name =  node.li_attr.shortlabel;
        var description = node.li_attr.longlabel;
        var visibility = node.li_attr.visibility;
        var type = node.li_attr.viewtype;
        var viewFunc = node.li_attr.viewfunc;

        if (type == 'collection') 
            $("#viewFuncDiv").show();
        else
            $("#viewFuncDiv").hide();

        $("#CustomTrackOptions").show();
        $("#viewFunc").val(viewFunc);
        $("#customName").val(name);
        $("#customDescription").val(description);
        $("#customVis").val(visibility);
        $("#customColorInput").val(color);
        $("#customColorPicker").spectrum("set", color);
        if ( node.li_attr.missingmethod === 'zero') {
            $("input[name='missingData'][value='zero']").prop("checked",true);
            $("input[name='missingData'][value='missing']").prop("checked",false);
        } else {
            $("input[name='missingData'][value='zero']").prop("checked",false);
            $("input[name='missingData'][value='missing']").prop("checked",true);
        }

        $("#doNewCollection").off ( "click" );
        $("#doNewCollection").click ( changeCollection );
        $( "#newCollectionDialog" ).dialog("open");
    }

    function selectNode(tree, node) {
        // called when a node in the currentCollection tree is selected
        selectedNode = node;
        selectedTree = tree;
        $(selectedTree).jstree("toggle_node", selectedNode);
   }

    function doubleClickTreeNode(evt, data)             {
        doubleClickNode(evt.target);
    }

    function selectTreeNode(evt, data)             {
        selectNode(evt.target, data.node);
    }

    function checkCallback( operation, node, node_parent, node_position, more) {
        // called during a drag and drop action to see if the target is droppable
        if ((operation === "copy_node") ||  (operation === "move_node")) {
            if (node.li_attr.class === "folder") {
                if (node_parent.id !== '#') {
                    return false;
                }
            }
            else if (node_parent.li_attr.class !== "folder") {
                return false;
            }
        }
        return true;
    }

    function dialogCollection() {
        $("#doNewCollection").off ( "click" );
        $("#doNewCollection").click ( newCollection );
        $("#viewFuncDiv").show();
        $("#customName").val("A New Collection");
        $("#customDescription").val("A New Collection Description");
        $("#customVis").val("full");
        $("#customColorInput").val("#0");
        //$("input:radio[name ='missingData']:checked").val();
        $("#viewFunc").val("show all");
        $( "#customName" ).select();
        $( "#newCollectionDialog" ).dialog("open");
    } 

    function newCollection() {
        $( "#newCollectionDialog" ).dialog("close");
        var ourCollectionName = getUniqueName("coll");
        var newName = $("#customName").val();
        var newDescription = $("#customDescription").val();
        var parent = $(selectedTree).find("li").first();

        var newId = $(selectedTree).jstree("create_node", "#", newName + " (" + newDescription + ")");
        var newNode = $(selectedTree).jstree("get_node", newId);
        isDirty = true;
        newNode.li_attr.class = "folder";
        newNode.li_attr.name = ourCollectionName;
        newNode.li_attr.shortlabel = $("#customName").val();
        newNode.li_attr.longlabel = $("#customDescription").val();
        newNode.li_attr.visibility = $("#customVis").val();
        newNode.li_attr.color = $("#customColorInput").val();
        newNode.li_attr.missingmethod = $("input:radio[name ='missingData']:checked").val();
        newNode.li_attr.viewfunc = $("#viewFunc").val();
        newNode.li_attr.viewtype = "collection";
        //selectedNode = newNode;
        $(selectedTree).jstree("set_icon", newNode, '../images/folderC.png');
        $(selectedTree).jstree("deselect_node", selectedNode);
        $(selectedTree).jstree("select_node", newNode.id);
        rebuildLabel();
    }

    function addCollection(trees, list) {
        // called when outputting JSON of all the collectionList
        var collectTree = trees[list.id];
        var v = collectTree.jstree(true).get_json('#', {flat:true, no_data:true, no_state:true, no_a_attr:true});
        var mytext = JSON.stringify(v);
        return mytext;
    }

    function saveCollections(trees) {
       // called when the "Save" button is pressed
       var json = "[";
        var v = $(selectedTree).jstree(true).get_json('#', {flat:true, no_data:true, no_state:true, no_a_attr:true});
        var children;
        var parents = {};
        for(ii=0; ii < v.length; ii++) {
            if (v[ii].parent !== '#') {
                parents[v[ii].parent] = 1;
            }
        }
        for(ii=0; ii < v.length; ii++) { 
            if (v[ii].li_attr.class === "folder") {
                if (parents[v[ii].id] !== 1)
                    alert(v[ii].text + " does not have any wiggles.  Not saved");
            }
        }
        json += JSON.stringify(v);
        json += ']';
        var requestData = 'jsonp=' + json;
        $.ajax({
            data:  requestData ,
            async: doAjaxAsync,
            dataType: "JSON",
            type: "PUT",
            url: "hgCollection?cmd=saveCollection",
            trueSuccess: updatePage,
            success: catchErrorOrDispatch,
            error: errorHandler,
        });
    }

    function rebuildLabel() {
        // rebuild the label for tree item
        var newText = selectedNode.li_attr.shortlabel + "   (" + selectedNode.li_attr.longlabel + ")";
        $(selectedTree).jstree('rename_node', selectedNode, newText);
    }

    function colorChange() {
        // change the color for a track
        isDirty = true;
        var color = $("#customColorPicker").spectrum("get"); $('#customColorInput').val(color);
    }

    function isDraggable(nodes) {
        // only children can be dragged
        var ii;
        for (ii=0; ii < nodes.length; ii++)
            if (nodes[ii].children.length !== 0)
                return false;
        return true;
    }

    function recordNames(tree) {
        // keep an accounting of track names that have been used
        var v = $(tree).jstree(true).get_json('#', {'flat': true});
        for (i = 0; i < v.length; i++) {
            var z = v[i];
            names[z.li_attr.name] = 1;
        }
    }

    function plusHit(event, data) {
        // called with the plus icon is hit
        var treeObject = $(event.currentTarget).parent().parent();
        var id = treeObject.attr('id');
        var node = treeObject.jstree("get_node", id);
        if (node.children.length === 0) {
            var parentId = $(selectedNode).attr('id');
            isDirty = true;
            $(selectedTree).jstree("copy_node", node, parentId,'last');
        }
    }

    function minusHit (event, data) {
        // called with the minus icon is hit
        var treeObject = $(event.currentTarget).parent().parent();
        var id = treeObject.attr('id');
        var node = treeObject.jstree("get_node", id);
        //if (node.children.length === 0) {
        if (node.li_attr.class !== "folder") {
            isDirty = true;
            $(selectedTree).jstree( "delete_node", node);
        }
    }

    function init() {
        // called at initialization time
        $body = $("body");

        // block user input when ajax is running
        $(document).on({
            ajaxStart: function() { $body.addClass("loading");    },
            ajaxStop: function() { $body.removeClass("loading"); }    
        });

        $('.gbButtonGoContainer').click(submitForm);
       
        window.addEventListener("beforeunload", function (e) {
            if (isDirty) {
                doAjaxAsync = false;
                saveCollections(trees);
            }

            return undefined;
        });

        $("#saveCollections").click ( function() { saveCollections(trees); } );
        $("#discardChanges").click ( function () { isDirty = false; window.location.reload(); });

        $( "#newCollectionDialog" ).dialog({ modal: true, 
            width: "50%", 
            autoOpen: false,
            });
        $("#newCollection").click ( dialogCollection );
        $("#doNewCollection").click ( newCollection );

        var trackOpt = {
            hideAfterPaletteSelect : true,
            color : $('#customColorInput').val(),
            showPalette: true,
            showInput: true,
            preferredFormat: "hex",
            change: colorChange,
        };

        $("#customColorPicker").spectrum(trackOpt);
        $.jstree.defaults.core.check_callback = checkCallback;
        $.jstree.defaults.core.themes.dots = true;
        $.jstree.defaults.contextmenu.show_at_node = false;
        var addedOne = true;
        if ( $("#currentCollection ul").length === 0) {
            addedOne = false;
        }
        $("#currentCollection div").each(function(index) {
            var newTree = this;

            $(newTree).jstree({
               'plugins' : ['dnd', 'conditionalselect', 'contextmenu'],
               'contextmenu': { "items" : currentCollectionItems},
               'dnd': {
                "check_callback" : checkCallback,
                }
            });
            recordNames(newTree);
            trees[this.id] = $(newTree);
            $(newTree).on("select_node.jstree", selectTreeNode);
            $(newTree).on("dblclick.jstree", doubleClickTreeNode);

            $(newTree).on("copy_node.jstree", function (evt, data)  {
                $(evt.target).jstree("open_node", data.parent);
                $(evt.target).jstree("set_icon", data.node, 'fa fa-minus-square');
            });
            $(newTree).on('click', '.jstree-themeicon ', minusHit);

            // select the first cild
            var firstChild = $(newTree).find("li").first();
            $(newTree).jstree("select_node", $(firstChild).attr("id"));
            selectedTree = newTree;
        });


        treeDiv=$('#tracks');
        treeDiv.jstree({
               'plugins' : ['dnd', 'conditionalselect', 'contextmenu'],
               'contextmenu': { "items" : currentTrackItems},
               'dnd': {
                "check_callback" : checkCallback,
               'always_copy' : true,
                is_draggable: isDraggable,
               },
               'core' :  {
                "check_callback" : checkCallback
            }
        });
        treeDiv.on("select_node.jstree", function (evt, data)  {
            $(evt.target).jstree("toggle_node", data.node);
        });
        treeDiv.on('click', '.jstree-themeicon ', plusHit);

        //var firstElement = $("#collectionList li").first();
        //selectElements($("#collectionList"), firstElement) ;
        if (!addedOne) {
            dialogCollection();
        }
    }

   function submitForm() {
    // Submit the form (from GO button -- as in hgGateway.js)
    // Show a spinner -- sometimes it takes a while for hgTracks to start displaying.
        $('.gbIconGo').removeClass('fa-play').addClass('fa-spinner fa-spin');
        goTracks = true;
        saveCollections(trees);
    }

    function updatePage(responseJson) {
        // called after AJAX call
        isDirty = false;
        if (!responseJson) {
            return;
        }

        if (goTracks) {
            // we go straight to hgTracks after save
            $form = $('#redirectForm');
            $form.submit();
        }
    }

    function getUniqueName(root) {
        // make sure name is unique in track hub
        if (!names[root]) {
            names[root] = true;
            return root;
        } else {
            var counter = 1;

            for(; ; counter++) {
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
