// "use strict";
// Don't complain about line break before '||' etc:
/* jshint -W014 */
/* jshint esversion: 8 */
function makeIframe(ev) {
    /* It's unusual to show script output in an iframe. But this solution has a few advantages:
     * - We can show a "waiting" message while the data loads
     * - The user knows where the results will appear, it looks like a dialog box and covers the page
     */
    ev.stopPropagation();
    var validateText = document.getElementById('validateHubUrl');
    validateText.value=$.trim(validateText.value);
    var hubUrl = $('#validateHubUrl').val();
    if(!validateUrl(hubUrl)) { 
        alert('Invalid hub URL');
        return;
    }

    hgsid = document.querySelector("input[name='hgsid']").value;
    var myUrl = window.location.href.split("#")[0].split("?")[0]; // strip off hgsid and tab-name
    var waitUrl =  myUrl + '?hgsid=' + hgsid + '&hgHub_do_hubCheck=1';
    var node = document.createElement('iframe'); 
    node.setAttribute('src', waitUrl);
    node.setAttribute('width', document.documentElement.clientWidth-100+'px');
    node.setAttribute('height', document.documentElement.clientHeight-100+'px');
    node.style.position = 'absolute'; 
    node.style.top = '50px'; 
    node.style.left = '50px'; 
    node.style.border = '3px solid darkgrey'; 
    node.id = 'checkerFrame';
    // first show the loading page
    document.body.appendChild(node);


    // when the waiting page has finished loading, load the hub checker page
    var finalUrl = waitUrl + '&validateHubUrl='+encodeURIComponent(hubUrl);
    var alreadyRun = false;
    node.addEventListener("load", function() {
        if (! alreadyRun)
            node.setAttribute('src', finalUrl);
        alreadyRun = true; // because 'load' fires again when finalUrl is loaded
        this.contentWindow.focus(); // activate keyboard event handlers of the iframe
    });
    return false;
}

function closeIframe() {
    var theFrame = window.parent.document.getElementById('checkerFrame');
    theFrame.parentNode.removeChild(theFrame);
}

function reloadIframe() {
    document.getElementById("content").innerHTML = "Re-loading hub...";
    window.parent.document.getElementById('checkerFrame').src += '';
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
      active: localStorage.getItem("hubTab") !== null ? localStorage.getItem("hubTab") : 0,
      activate: function(event, ui) {
          localStorage.setItem("hubTab", ui.newTab.index());
      },
  });
  // activate tabs if the current URL ends with the appropriate tab name
  var tabName = window.location.hash;
  if (tabName==="publicHubs")
      $("#tabs").tabs("option", "active", 0);
  if (tabName==="#conn" || tabName === "unlistedHubs")
      $("#tabs").tabs("option", "active", 1);
  if (tabName==="#dev" || tabName === "hubDeveloper")
      $("#tabs").tabs("option", "active", 2);
  if (tabName==="#hubUpload")
      $("#tabs").tabs("option", "active", 3);

  $("#tabs").tabs().on("tabsactivate", function(event, ui) {
    const  newHash = ui.newTab.find("a").attr("href");
    if (newHash) {
      history.replaceState(null, null, newHash);
    }
    if (newHash === "#hubUpload") {
        hubCreate.init();
    }
  });
});

// creates keyup event; listening for return key press
$(document).ready(function() {
    $('#loadSampleHub').bind('click', function(e) {
        $('#validateHubUrl').val("https://genome.ucsc.edu/goldenPath/help/examples/hubDirectory/hub.txt");

    });
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
    $('.pasteIcon').bind('click', function(e) {
        // The hgTracks link is in the <A> element two elements before the icon SVG:
        // <td>
        // <a class='hgTracksLink' href="hgTracks?hubUrl=https://hgwdev-kent.gi.ucsc.edu/~kent/t2t/hub/hub2.txt&amp;genome=hub_25068_GCA_009914755&amp;position=lastDbPos">GCA_009914755</a>
        // <input type="hidden" value="https://hgwdev-kent.gi.ucsc.edu/~kent/t2t/hub/hub2.txt">
        // <svg class="pasteIcon">...</svg>    <--- this is e.target of the click handler
        // </td>
        var inputEl = e.target.closest("svg").previousSibling;
        var connectUrl = inputEl.previousSibling.href;

        // the url is in the <input> element just before the SVG
        var oldVal = inputEl.value;
        // display:none does not work,
        // see https://stackoverflow.com/questions/31593297/using-execcommand-javascript-to-copy-hidden-text-to-clipboard
        inputEl.style = "position: absolute; left: -1000px; top: -1000px";
        inputEl.value = connectUrl;
        inputEl.type = 'text';
        inputEl.select();
        inputEl.setSelectionRange(0, 99999); /* For mobile devices */
        document.execCommand('copy');

        inputEl.type = 'hidden';
        inputEl.value = oldVal;
        alert("Copied Genome Browser hub connection URL to clipboard");
    });

    $('.shortPlus').bind('click', function(ev) {
        ev.target.parentElement.style.display = 'none';
        ev.target.parentElement.nextSibling.style.display = 'inline';
    });
    $('.fullMinus').bind('click', function(ev) {
        ev.target.parentElement.style.display = 'none';
        ev.target.parentElement.previousSibling.style.display = 'inline';
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
