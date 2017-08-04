// hgCollection.js - Interactive features for GTEX Body Map version of GTEx Gene track UI page

// Copyright (C) 2017 The Regents of the University of California


var collections = (function() {
    var names = []; // a list of names that have been used
    var selectedNode = "collections"; // keep track of id of selected row
    var $tracks;  // the #tracks object

    function errorHandler(request, textStatus) {
    }

    function updatePage(responseJson) {
        if (!responseJson) {
            return;
        }
        var message = responseJson.serverSays;
        if (message) {
            console.log('server says: ' + JSON.stringify(message));
        }
    }

    // make sure name is unique in track hub
    function getUniqueName(root) {
        if (!names.root) {
            names.root = true;
            return root;
        } else {
            var counter = 1;

            for(;;counter++) {
                var name  = root + counter;
                if (!names.name) {
                    names.name = true;
                    return name;
                }
            }
        }
    }

    function init() {
        // build the tracks tree
        $tracks = $("#tracks");
        $tracks.treetable({expandable:true});
        $tracks.show();
        //
        // record the names of all the tracks in the collections
        $("#tracks  .user").each(function() {
            var name = $(this).data("ttId");
            names[name] = 1;
        });

        // color pickers
        var viewOpt = {
            hideAfterPaletteSelect : true,
            color : $('#viewColorInput').val(),
            showPalette: true,
            showInput: true,
            preferredFormat: "hex",
            change: function() { var color = $("#viewColorPicker").spectrum("get"); $('#viewColorInput').val(color); },
            };
        $("#viewColorPicker").spectrum(viewOpt);

        var trackOpt = {
            hideAfterPaletteSelect : true,
            color : $('#trackColorInput').val(),
            showPalette: true,
            showInput: true,
            preferredFormat: "hex",
            change: function() { var color = $("#trackColorPicker").spectrum("get"); $('#trackColorInput').val(color); },
            };
        $("#trackColorPicker").spectrum(trackOpt);

        // Highlight selected row
        $("#tracks tbody").on("mousedown", "tr", function() {
            $("#tracks .selected").not(this).removeClass("selected");
            $(this).toggleClass("selected");
            selectedNode = this.getAttribute('data-tt-id');
            var name = this.cells[0].innerText;
            var description = this.cells[1].innerText;
            var visibility = $(this).data("visibility");

            if ($(this).hasClass("user")) {
                if ($(this).hasClass("view")) {
                    $("#viewOptions").show();
                    $("#CustomCompositeOptions").hide();
                    $("#CustomTrackOptions").hide();
                    $("#TrackDbOptions").hide();
                    $("#viewName").val(name);
                    $("#viewDescription").val(description);
                    $("#viewVis").val(visibility);
                } else if ($(this).hasClass("ui-droppable")) {
                    $("#CustomCompositeOptions").show();
                    $("#viewOptions").hide();
                    $("#CustomTrackOptions").hide();
                    $("#TrackDbOptions").hide();
                    $("#collectionName").val(name);
                    $("#collectionDescription").val(description);
                    $("#collectionVis").val(visibility);
                } else {
                    $("#CustomTrackOptions").show();
                    $("#CustomCompositeOptions").hide();
                    $("#TrackDbOptions").hide();
                    $("#viewOptions").hide();
                    $("#customName").val(name);
                    $("#customDescription").val(description);
                    $("#customVis").val(visibility);
                }
            } else {
                $("#CustomCompositeOptions").hide();
                $("#CustomTrackOptions").hide();
                $("#TrackDbOptions").show();
            }
        });

        // Drag & Drop 
        $("#tracks .file, #tracks .folder").draggable({
            helper: "clone",
            opacity: 0.75,
            refreshPositions: true,
            revert: "invalid",
            revertDuration: 300,
            scroll: true
        });

        $("#tracks .folder").each(function() {
            $(this).parents("#tracks tr").droppable({
                accept: ".file, .folder",
                drop: function(e, ui) {
                    var droppedEl = ui.draggable.parents("tr");
                    $("#tracks").treetable("move", droppedEl.data("ttId"), $(this).data("ttId"));
                    $(droppedEl).addClass("user");
                },
                hoverClass: "accept",
                over: function(e, ui) {
                    var droppedEl = ui.draggable.parents("tr");
                    if(this != droppedEl[0] && !$(this).is(".expanded")) {
                        $("#tracks").treetable("expandNode", $(this).data("ttId"));
                    }
                }
            });
        });

        $('body').on('click','#createView',function(){
            var composite = $("#tracks").treetable("node", selectedNode);
            var name = "View";
            var description = "Description of View";
            var thisNodeId = getUniqueName("View");
            var contents = "<tr data-tt-id=" + thisNodeId + " data-tt-parent-id=\"collections\"  class=\"user ui-droppable view\"><td> <span class='folder'>" + name + "</td><td>" + description +"</td></tr>";
            $("#tracks").treetable("loadBranch", composite, contents);
            var newNode =  $("#tracks").treetable("node", thisNodeId);
            $("#tracks").treetable("move", thisNodeId, selectedNode);
            var newNodeTr =  newNode.row;
            $(newNodeTr).data("visibility","squish");
            newNodeTr.droppable({
                accept: ".file, .folder",
                drop: function(e, ui) {
                    var droppedEl = ui.draggable.parents("tr");
                    $("#tracks").treetable("move", droppedEl.data("ttId"), $(this).data("ttId"));
                    $(droppedEl).addClass("user");
                },
                hoverClass: "accept",
                over: function(e, ui) {
                    var droppedEl = ui.draggable.parents("tr");
                    if(this != droppedEl[0] && !$(this).is(".expanded")) {
                        $("#tracks").treetable("expandNode", $(this).data("ttId"));
                    }
                }
            });
        });

        $("#newCollection").click ( function () {
            var collections = $("#tracks").treetable("node", "collections");
            var name = "Collection";
            var description = "Description of Collection";
            var thisNodeId = getUniqueName("collection");
            var contents = "<tr data-tt-id=" + thisNodeId + " data-tt-parent-id=\"collections\"  class=\"user ui-droppable\"><td><span class='folder'>" + name + "</td><td>" + description +"</td></tr>";
            $("#tracks").treetable("loadBranch", collections, contents);
            var newNode =  $("#tracks").treetable("node", thisNodeId);
            var newNodeTr =  newNode.row;
            $(newNodeTr).data("visibility","squish");
            newNodeTr.droppable({
                accept: ".file, .folder",
                drop: function(e, ui) {
                    var droppedEl = ui.draggable.parents("tr");
                    $("#tracks").treetable("move", droppedEl.data("ttId"), $(this).data("ttId"));
                    $(droppedEl).addClass("user");
                },
                hoverClass: "accept",
                over: function(e, ui) {
                    var droppedEl = ui.draggable.parents("tr");
                    if(this != droppedEl[0] && !$(this).is(".expanded")) {
                        $("#tracks").treetable("expandNode", $(this).data("ttId"));
                    }
                }
            });
        });

        $("#propsSave").click ( function () {
            window.location.reload();
        });

        $("#propsDiscard").click ( function () {
            window.location.reload();
        });

        $("#discardChanges").click ( function () {
            window.location.reload();
        });

        $("#saveCollections").click ( function () {
            var ii = 0;
            var values = [];
            $("#tracks  .user").each(function() {
                var jj = 0;
                var id = $(this).data("ttId");
                values[ii] = [];
                values[ii][jj] = $(this).data("ttParentId");

                jj++;
                $(this).find('td').each(function(){
                    values[ii][jj] = $(this).text();
                    jj++;
                });

                values[ii][jj] = $(this).data("ttId");
                jj++;

                values[ii][jj] = $(this).data("visibility");
                if (!values[ii][jj]) {
                    values[ii][jj] = "hide";
                }
                jj++;
                ii++;
            });
            requestData = 'jsonp=' + JSON.stringify(values);
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
        });
    }

    return {
            init: init
           };
}());
