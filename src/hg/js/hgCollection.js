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
            color : $('#customColorInput').val(),
            showPalette: true,
            showInput: true,
            preferredFormat: "hex",
            change: function() { var color = $("#customColorPicker").spectrum("get"); $('#customColorInput').val(color); },
            };
        $("#customColorPicker").spectrum(trackOpt);

        // Highlight selected row
        $("#tracks tbody").on("mousedown", "tr", function() {
            $("#tracks .selected").not(this).removeClass("selected");
            $(this).toggleClass("selected");
            selectedNode = this.getAttribute('data-tt-id');
            var name = this.cells[0].innerText;
            var description = this.cells[1].innerText;
            var visibility = this.getAttribute("visibility");
            var color = this.getAttribute("color");

            if ($(this).hasClass("user")) {
                if ($(this).hasClass("view")) {
                    $("#viewOptions").show();
                    $("#CustomCompositeOptions").hide();
                    $("#CustomTrackOptions").hide();
                    $("#TrackDbOptions").hide();
                    $("#viewName").val(name.slice(1));
                    $("#viewDescription").val(description);
                    $("#viewVis").val(visibility);
                    $("#viewColorInput").val(color);
                    $("#viewColorPicker").spectrum("set", color);
                } else if ($(this).hasClass("ui-droppable")) {
                    $("#CustomCompositeOptions").show();
                    $("#viewOptions").hide();
                    $("#CustomTrackOptions").hide();
                    $("#TrackDbOptions").hide();
                    $("#collectionName").val(name.slice(1));
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
                    $("#customColorInput").val(color);
                    $("#customColorPicker").spectrum("set", color);
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

        $('body').on('click','#deleteTrack',function(){
            $("#tracks").treetable("removeNode", selectedNode);
        });

        $('body').on('click','#createView',function(){
            var composite = $("#tracks").treetable("node", selectedNode);
            var name = "View";
            var description = "Description of View";
            var thisNodeId = getUniqueName("View");
            var contents = "<tr color='#000000' data-tt-id=" + thisNodeId + " data-tt-parent-id=" + selectedNode + " class=\"user ui-droppable view\"><td> <span class='folder'>" + name + "</td><td>" + description +"</td></tr>";
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
            var collections = $("#tracks").treetable("node", "coll_collections");
            var name = "Collection";
            var description = "Description of Collection";
            var thisNodeId = getUniqueName("collection");
            var contents = "<tr color='#000000' data-tt-id=" + thisNodeId + " data-tt-parent-id=\"coll_collections\"  class=\"user ui-droppable\"><td><span class='folder'>" + name + "</td><td>" + description +"</td></tr>";
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

        $("#collectionApply").click ( function () {
            var view = $("#tracks").treetable("node", selectedNode);
            var name = $(view.row).find('td:first');
            name.html( name.html().replace(name.text().slice(1),$("#collectionName").val()));
            // resetting the html trashes the event handler.
            var target = view.treeCell;
            var handler = function(e) {
                $(this).parents("table").treetable("node", $(this).parents("tr").data("ttId")).toggle();
                return e.preventDefault();
            };
            target.off("click.treetable").on("click.treetable", handler);

            // now change the description
            var description = $(view.row).find('td:last');
            description.html(description.html().replace(description.text(),$("#collectionDescription").val()));

            view.row[0].setAttribute("color", $("#collectionColorInput").val());
            view.row[0].setAttribute("visibility", $("#collectionVis").val());
        });

        $("#collectionReset").click ( function () {
            var row = $("#tracks").treetable("node", selectedNode).row[0];
            var name = row.cells[0].innerText.slice(1);
            var description = row.cells[1].innerText;
            var color = row.getAttribute("color");
            var visibility = row.getAttribute("visibility");
            $("#collectionName").val(name);
            $("#collectionDescription").val(description);
            $("#collectionVis").val(visibility);
            $("#collectionColorInput").val(color);
            $("#collectionColorPicker").spectrum("set", color);
        });

        $("#customApply").click ( function () {
            var view = $("#tracks").treetable("node", selectedNode);
            var name = $(view.row).find('td:first');
            name.html( name.html().replace(name.text(),$("#customName").val()));

            // now change the description
            var description = $(view.row).find('td:last');
            description.html(description.html().replace(description.text(),$("#customDescription").val()));

            view.row[0].setAttribute("color", $("#customColorInput").val());
            view.row[0].setAttribute("visibility", $("#customVis").val());
        });

        $("#customReset").click ( function () {
            var row = $("#tracks").treetable("node", selectedNode).row[0];
            var name = row.cells[0].innerText.slice(1);
            var description = row.cells[1].innerText;
            var color = row.getAttribute("color");
            var visibility = row.getAttribute("visibility");
            $("#customName").val(name);
            $("#customDescription").val(description);
            $("#customVis").val(visibility);
            $("#customColorInput").val(color);
            $("#customColorPicker").spectrum("set", color);
        });

        $("#viewApply").click ( function () {
            var view = $("#tracks").treetable("node", selectedNode);
            var name = $(view.row).find('td:first');
            name.html( name.html().replace(name.text().slice(1),$("#viewName").val()));
            // resetting the html trashes the event handler.
            var target = view.treeCell;
            var handler = function(e) {
                $(this).parents("table").treetable("node", $(this).parents("tr").data("ttId")).toggle();
                return e.preventDefault();
            };
            target.off("click.treetable").on("click.treetable", handler);

            // now change the description
            var description = $(view.row).find('td:last');
            description.html(description.html().replace(description.text(),$("#viewDescription").val()));

            view.row[0].setAttribute("color", $("#viewColorInput").val());
            view.row[0].setAttribute("visibility", $("#viewVis").val());
        });

        $("#viewReset").click ( function () {
            var row = $("#tracks").treetable("node", selectedNode).row[0];
            var name = row.cells[0].innerText.slice(1);
            var description = row.cells[1].innerText;
            var color = row.getAttribute("color");
            var visibility = row.getAttribute("visibility");
            $("#viewName").val(name);
            $("#viewDescription").val(description);
            $("#viewVis").val(visibility);
            $("#viewColorInput").val(color);
            $("#viewColorPicker").spectrum("set", color);
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

                values[ii][jj] = this.getAttribute("visibility");
                if (!values[ii][jj]) {
                    values[ii][jj] = "hide";
                }
                jj++;

                values[ii][jj] = this.getAttribute("color");
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
