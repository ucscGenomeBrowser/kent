// make div1 visible and hide div2, used to show two different intro texts
// in the public/my hubs tab and the hub develop tab
function toggleTwoDivs(div1, div2) {
    div1.style.display = "block";
    div2.style.display = "none";
}

// hover effect to highlight table rows
$(function() {
    $(".hubList tr").hover(

    function() {
        $(this).addClass("hoverRow");
    }, function() {
        $(this).removeClass("hoverRow");
    });
});


// initializes the tabs - with cookie option
// cookie option requires jquery.cookie.js
$(function() {
    $("#tabs").tabs({
        cookie: {
            name: 'hubTab_cookie',
            expires: 30
        }
    });
});


// creates keyup event; listening for return key press
$(document).ready(function() {
    $('#hubUrl').bind('keypress', function(e) {  // binds listener to url field
        if (e.which === 13) {  // listens for return key
             e.preventDefault();   // prevents return from also submitting whole form
             if (validateUrl($('#hubUrl').val()))
                 $('input[name="hubAddButton"]').focus().click(); // clicks AddHub button
        }
    });
    $('#validateHubUrl').bind('keypress', function(e) {  // binds listener to url field
        if (e.which === 13) {  // listens for return key
             e.preventDefault();   // prevents return from also submitting whole form
             if (validateUrl($('#validateHubUrl').val()))
                 $('input[name="hubValidateButton"]').focus().click(); // clicks Validate Url button
        }
    });
    $('#hubSearchTerms').bind('keypress', function(e) {  // binds listener to text field
        if (e.which === 13) {  // listens for return key
            e.preventDefault();   // prevents return from also submitting whole form
            $('input[name="hubSearchButton"]').focus().click(); // clicks search button
        }
    });
    $('#hubDbFilter').bind('keypress', function(e) {  // binds listener to text field
        if (e.which === 13) {  // listens for return key
            e.preventDefault();   // prevents return from also submitting whole form
            $('input[name="hubSearchButton"]').focus().click(); // clicks db filter button
        }
    });
});

var hubSearchTree = (function() {
    var treeDiv;        // Points to div we live in

    function hubSearchTreeContextMenuHandler (node, callback) {
        var nodeType = node.li_attr.nodetype;
        if (nodeType == 'track') {
            callback({
                'openConfig': {
                    'label' : 'Configure this track',
                    'action' : function () {
                        window.open(node.li_attr.configlink, '_blank');
                    }
                }
            });
        }
        else if (nodeType == 'assembly') {
            callback({
                'openConfig': {
                    'label' : 'Open this assembly',
                    'action' : function () {
                        window.open(node.li_attr.assemblylink, '_blank');
                    }
                }
            });
        }
    }

    function buildTracks(node, cb) {
        // called when jstree wants data to open a node for the tracks tree
        cb.call(this, trackData[node.id]);
    }

    function init(searching) {
        $.jstree.defaults.core.themes.icons = false;
        $.jstree.defaults.core.themes.dots = true;
        $.jstree.defaults.contextmenu.show_at_node = false;
        if (searching === true) {
            $.jstree.defaults.contextmenu.items = hubSearchTreeContextMenuHandler;

            $('div[id^="tracks"]').each(function(i, obj) {
                treeDiv = obj;
                var hubId = treeDiv.id.slice(6);
                arrId = '#_' + hubId;
                $(treeDiv).jstree({
                    'plugins' : ['contextmenu'],
                    'core' : {
                        'data': function(node, cb) {
                            if (node.id === '#') {
                                cb([{"text" : "Search details ...", "id": arrId, "children": true}]);
                            } else {
                                cb(trackData[""+node.id]);
                            }
                        },
                        'dbclick_toggle': false
                    }
                });
                $(treeDiv).on("select_node.jstree", function (e, data)  {
                    $(e.target).jstree("open_node", data.node);
                }); // jstree
            }); // each div
        } else { // validating hub, no contextmenu and easier tree building
            treeDiv = $('#validateHubResult');
            treeDiv.jstree({
                'core' : {
                    'data' : buildTracks,
                    'dbclick_toggle': false
                }
            });
            treeDiv.on('select_node.jstree', function(e, data) {
                    $(e.target).jstree("open_node", data.node);
            });
        }
    } // init
    return {
        init: init
    };
}());
