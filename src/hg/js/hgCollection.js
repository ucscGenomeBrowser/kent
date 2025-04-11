// hgCollection.js - custom collection builder

// Copyright (C) 2018 The Regents of the University of California

var hgCollection = (function() {
    var selectedNode;
    var selectedTree;
    var $tracks;  // the #tracks object
    var trees = [];
    var isDirty = false;
    var goTracks = false;
    var loadFinished = false;
    var doAjaxAsync = true;
    var emptyCollectionText;
    var addWithoutCollectionText;

    function currentTrackItems(node) {
        // populate the menu for the currentCollection tree
        var items = {
            addItem: { // The "add" menu item
                label: "Add",
                action: function () {
                    if (selectedNode === undefined) {
                        alert(addWithoutCollectionText);
                        return;
                    }
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
                    checkEmpty(parentId);
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
            deleteItem: { // The "delete" menu item
                label: "Delete",
                action: function () {
                    var nodes = $(selectedTree).jstree( "get_selected");
                    var parentNode = $(selectedTree).jstree("get_node", node.parent);
                    isDirty = true;
                    if ((parentNode.id !== '#') && (parentNode.children.length === nodes.length) ){
                        $(selectedTree).jstree("create_node", node.parent, emptyCollectionText);
                        parentNode.li_attr.class = "folder empty";
                    }
                    $(selectedTree).jstree( "delete_node", nodes);
                    if (parentNode.id === '#') {
                        var firstChild = $(selectedTree).find("li").first();
                        $(selectedTree).jstree("select_node", $(firstChild).attr("id"));
                    } else {
                        $(selectedTree).jstree( "select_node", parentNode.id);
                    }
                }
            }
        };

        return items;
        }

    function changeCollection() {
        var newName = $("#customName").val().trim();
        if (!validateLabel(newName))
            return;

        var newDescription = $("#customDescription").val().trim();
        if (!validateLabel(newDescription))
            return;
        $( "#newCollectionDialog" ).dialog("close");
        selectedNode.li_attr.class = "folder";
        selectedNode.li_attr.title = collectionTitle;
        selectedNode.li_attr.shortlabel = newName;
        selectedNode.li_attr.longlabel = newDescription;
        selectedNode.li_attr.visibility = $("#customVis").val();
        selectedNode.li_attr.color = $("#customColorInput").val();
        selectedNode.li_attr.missingmethod = $("input:radio[name ='missingData']:checked").val();
        selectedNode.li_attr.viewfunc = $("#viewFunc").val();
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
        if (type == 'collection')  {
            $( "#newCollectionDialog" ).dialog( 'option', 'title', 'Edit Collection');
            $('#collectionDialogHelp').show();
            $('#trackDialogHelp').hide();
        } else {
            $( "#newCollectionDialog" ).dialog( 'option', 'title', 'Edit Track');
            $('#collectionDialogHelp').hide();
            $('#trackDialogHelp').show();
        }

        $( "#newCollectionDialog" ).dialog("open");
    }

    function moveNode(evt, data) {
        // called when a node is moved
        checkEmpty(data.parent);
        var oldParentNode = $(selectedTree).jstree('get_node', data.old_parent);
        if (oldParentNode.children.length === 0) {
            oldParentNode.li_attr.class = "folder empty";
            $(selectedTree).jstree("create_node", data.old_parent, emptyCollectionText);
        }
    }

    function selectNode(tree, node) {
        // called when a node in the currentCollection tree is selected
        selectedNode = node;
        selectedTree = tree;
        $(selectedTree).jstree("open_node", selectedNode);
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
            if ((node.parent != '#') && (node_parent.parent === '#')) {
                if (node.icon === true) // empty stub
                    return false;
                return true;
            }
            return false;
        }
        return true;
    }


    function dialogCollection() {
        $("#doNewCollection").off ( "click" );
        $("#doNewCollection").click ( newCollection );
        $("#viewFuncDiv").show();

        var collectionLabel = getUniqueLabel();
        $("#customName").val(collectionLabel);
        $("#customDescription").val(collectionLabel + " description");
        $("#customVis").val("full");
        $("#customColorInput").val("#0");
        $("#customColorPicker").spectrum("set", "#0");
        $("#viewFunc").val("show all");
        $( "#customName" ).select();
        $('#collectionDialogHelp').show();
        $('#trackDialogHelp').hide();
        $( "#newCollectionDialog" ).dialog( 'option', 'title', 'Create New Collection');
        $( "#newCollectionDialog" ).dialog("open");
    } 

    function newCollection() {
        var newName = $("#customName").val().trim();
        if (!validateLabel(newName))
            return;

        var newDescription = $("#customDescription").val().trim();
        if (!validateLabel(newDescription))
            return;
        var ourCollectionName = getUniqueName();
        var parent = $(selectedTree).find("li").first();
        $( "#newCollectionDialog" ).dialog("close");

        var newId = $(selectedTree).jstree("create_node", "#", newName + " (" + newDescription + ")");
        var newId2 = $(selectedTree).jstree("create_node", newId, emptyCollectionText);
        var newNode = $(selectedTree).jstree("get_node", newId);
        isDirty = true;
        newNode.li_attr.class = "folder empty";
        newNode.li_attr.name = ourCollectionName;
        newNode.li_attr.shortlabel = newName;
        newNode.li_attr.longlabel = newDescription;
        newNode.li_attr.visibility = $("#customVis").val();
        newNode.li_attr.color = $("#customColorInput").val();
        newNode.li_attr.missingmethod = $("input:radio[name ='missingData']:checked").val();
        newNode.li_attr.viewfunc = $("#viewFunc").val();
        newNode.li_attr.viewtype = "collection";
        $(selectedTree).jstree("set_icon", newNode, '../images/folderC.png');
        $(selectedTree).jstree("deselect_node", selectedNode);
        $(selectedTree).jstree("select_node", newNode.id);
        $(selectedTree).on("move.jstree", moveNode);
        rebuildLabel();
    }

    function addCollection(trees, list) {
        // called when outputting JSON of all the collectionList
        var collectTree = trees[list.id];
        var v = collectTree.jstree(true).get_json('#', {flat:true, no_data:true, no_state:true, no_a_attr:true});
        var mytext = JSON.stringify(v);
        return mytext;
    }

    function finishSaving() {
        // save the collection tree to the server
       var json = "[";
        var v = $(selectedTree).jstree(true).get_json('#', {flat:true, no_data:true, no_state:true, no_a_attr:true});
        var children;
        var parents = {};
        for(ii=0; ii < v.length; ii++) {
            if (v[ii].parent === '#') {
                parents[v[ii].id] = v[ii].text;
            }
        }
        for(ii=0; ii < v.length; ii++) { 
            if (v[ii].li_attr.name === undefined) {
                alert(parents[v[ii].parent] + " does not have any wiggles.  Not saved.");
            }
        }
        json += JSON.stringify(v);
        json += ']';
        json = encodeURIComponent(json);  // encodes , / ? : @ & = + $ # and special characters.
        var requestData = 'jsonp=' + json + '&hgsid=' + getHgsid();
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

    function saveCollections(trees) {
        // called when the "Save" button is pressed
        $("#workScreen").css("display","block");
        finishSaving();
    }

    function rebuildLabel() {
        // rebuild the label for tree item
        var newText = selectedNode.li_attr.shortlabel + "   (" + selectedNode.li_attr.longlabel + ")";
        $(selectedTree).jstree('rename_node', selectedNode, newText);
        isDirty = true;
    }

    function colorChange() {
        // change the color for a track
        isDirty = true;
        var color = $("#customColorPicker").spectrum("get"); 
        $('#customColorInput').val(color);
    }

    function isDraggable(nodes) {
        // only children can be dragged
        var ii;
        for (ii=0; ii < nodes.length; ii++)
            if (nodes[ii].children.length !== 0)
                return false;
        return true;
    }

    function checkEmpty(parentId) {
        // add or remove the "empty collection" stub
        if ($('#'+parentId).hasClass('empty')) {
            var parentNode = $(selectedTree).jstree('get_node', parentId);
            var stub;
            for (i = 0; i < parentNode.children.length; i++) {
                stub = $(selectedTree).jstree('get_node', parentNode.children[i]);
                if (stub.icon === true)
                    break;
            }

            if (i === parentNode.children.length)
                return;

            $(selectedTree).jstree('delete_node', stub);
            $('#'+parentId).removeClass('empty');
            parentNode.li_attr.class = 'folder';
        }
    }

    function findCollection(parentNode) {
        while(parentNode.parent !== '#') {
            parentNode = $(selectedTree).jstree("get_node", parentNode.parent);
        }

        return parentNode.id;
    }
    
    function plusHit(event, data) {
        // called with the plus icon is hit
        var treeObject = $(event.currentTarget).parent().parent();
        var id = treeObject.attr('id');
        var node = treeObject.jstree("get_node", id);

        if (node.icon !== 'fa fa-plus')  // if this isn't a leaf, then return
            return;

        if (selectedNode === undefined) {   // if there are no collections
            alert(addWithoutCollectionText);
            return;
        }

        var parentId = $(selectedNode).attr('id');
        parentId = findCollection(selectedNode);
        checkEmpty(parentId);
        isDirty = true;
        $(selectedTree).jstree("copy_node", node, parentId,'last');
    }

    function minusHit (event, data) {
        // called with the minus icon is hit
        var treeObject = $(event.currentTarget).parent().parent();
        var id = treeObject.attr('id');
        var node = treeObject.jstree("get_node", id);
        if (node.parent !== '#') {
            isDirty = true;
            var parentNode = treeObject.jstree("get_node", node.parent);
            if (parentNode.children.length === 1) {
                treeObject.jstree("create_node", node.parent, emptyCollectionText);
                parentNode.li_attr.class = "folder empty";
            }
            $(selectedTree).jstree( "delete_node", node);
        }
    }

    function buildCollections(node, cb) {
        // called when jstree wants data to open a node for the collection tree
        cb.call(this, collectionData[node.id]);
    }

    function buildTracks(node, cb) {
        // called when jstree wants data to open a node for the tracks tree
        cb.call(this, trackData[node.id]);
    }

    function trackTreeReady() {
        // called when the track tree has been loaded
        var firstChild = $(this).find("li").first();
        $(this).jstree("select_node", $(firstChild).attr("id"));
    }

    function collectionTreeReady() {
        // called when the collection tree has been loaded
        //$(selectedTree).on("load_all.jstree", collectionLoadFinished);
        var rootNode = $(selectedTree).jstree("get_node", '#');
        $(selectedTree).jstree('load_all', rootNode, collectionLoadFinished);
        var firstChild = $(this).find("li").first();
        $(this).jstree("select_node", $(firstChild).attr("id"));
    }

    function colorInputChange () {
        // called when the color text edit box changes
        $("#customColorPicker").spectrum("set",$("#customColorInput").val());
    }

    function collectionLoadFinished() {
        loadFinished = true;
        $("#workScreen").css("display","none");
    }

    function init() {
        // called at initialization time
        $body = $("body");
        $("#customColorInput").change(colorInputChange);

        emptyCollectionText = $('#emptyCollectionText').text();
        addWithoutCollectionText = $('#addWithoutCollectionText').text();

        // block user input when ajax is running
        $(document).on({
            ajaxStart: function() { $body.addClass("loading");    },
            ajaxStop: function() { $body.removeClass("loading"); }    
        });

        $('.gbButtonGoContainer').click(submitForm);
       
        window.addEventListener("beforeunload", function (e) {
            if (isDirty) {
                doAjaxAsync = false;
                if (loadFinished)
                    saveCollections(trees);
                else
                    e.returnValue = "ask to leave";
            }
        });

        $("#saveCollections").click ( function() { saveCollections(trees); } );
        $("#discardChanges").click ( function () { isDirty = false; window.location.reload(); });

        $( "#newCollectionDialog" ).dialog({ modal: true, 
            width: "50%", 
            autoOpen: false,
            dialogClass: 'myTitleClass'
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
        var addedOne = false;
        if ( $("#currentCollection ul").length !== 0) {
            addedOne = true;
        }

        var newTree=$('#currentCollection');
        newTree.on("ready.jstree", collectionTreeReady);
        newTree.jstree({
           'plugins' : ['dnd', 'conditionalselect', 'contextmenu'],
           //'plugins' : [ 'conditionalselect', 'contextmenu'],
           'contextmenu': { "items" : currentCollectionItems},
           'core': {
               "data" : buildCollections,
               "dblclick_toggle" : false,
            },
           'dnd': {
                "check_callback" : checkCallback,
            }
        });
        $(newTree).on("select_node.jstree", selectTreeNode);
        $(newTree).on("dblclick.jstree", doubleClickTreeNode);
        $(newTree).on("move_node.jstree", moveNode);

        $(newTree).on("copy_node.jstree", function (evt, data)  {
            $(evt.target).jstree("open_node", data.parent);
            $(evt.target).jstree("set_icon", data.node, 'fa fa-minus-square');
            data.node.li_attr.title = collectionTitle;
            $(evt.target).jstree("redraw", "true");
        });
        $(newTree).on('click', '.jstree-themeicon ', minusHit);

        selectedTree = newTree;

        treeDiv=$('#tracks');
        treeDiv.jstree({
               'plugins' : ['conditionalselect', 'contextmenu'],
               'contextmenu': { "items" : currentTrackItems},
               'dnd': {
                    "check_callback" : checkCallback,
                   'always_copy' : true,
                    is_draggable: isDraggable,
               },
               'core' :  {
                   "data" : buildTracks,
                   "check_callback" : checkCallback,
                   "dblclick_toggle" : false,
                },
        });
        treeDiv.on("ready.jstree", trackTreeReady);
        treeDiv.on("select_node.jstree", function (evt, data)  {
            $(evt.target).jstree("open_node", data.node);
        });
        treeDiv.on('click', '.jstree-themeicon ', plusHit);
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

    function getUniqueLabel() {
        var root = "New Collection";
        if (!collectionLabels[root]) {
            collectionLabels[root] = 1;
            return root;
        } else {
            var counter = 1;

            for(; ; counter++) {
                var label  = root + ' (' + counter + ')';
                if (!collectionLabels[label]) {
                    collectionLabels[label] = 1;
                    return label;
                }
            }
        }
    }

    function getUniqueName() {
        // make sure name is unique in track hub
        var releaseDateInSeconds = 1520631071;
        var seconds =  Math.floor( Date.now() / 1000 ) - releaseDateInSeconds;
        var root = "coll" + seconds;
        if (!collectionNames[root]) {
            collectionNames[root] = 1;
            return root;
        } else {
            var counter = 1;

            for(; ; counter++) {
                var name  = root + counter;
                if (!collectionNames[name]) {
                    collectionNames[name] = 1;
                    return name;
                }
            }
        }
    }

    return {
        init: init
    };
}());
