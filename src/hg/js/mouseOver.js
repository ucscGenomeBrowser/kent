
// One single top-level object variable to hold all other global variables
var mapData = {};
// mapData.rects[] - an array of the rectangles, where each rect is
//                   {x, y, w, h, v} == x,y width,height, value
// mapData.visible - keep track of window visible or not, value: true|false
// mapData.tracks[]  - list of tracks with mapBoxes

function intersectPointRectangle(x,y, rect) {
  var answer = true;
  if (x <= rect.x1 || x > rect.x2) { answer = false; }
     else if (y <= rect.y1 || y > rect.y2) { answer = false; }
  return answer;
}

// function addOneBox(box) {
//    var rect = { x:box.x, y:box.y, w:box.w, h:box.h, v:box.v };
//    alert("x:" + rect.x + ", y:" + rect.y + ", w:" + rect.w + ", h:" + rect.h + ", v:" + rect.v);
//    mapData.rects.push(box);
//    mapData.rects.push(rect);
//}

// =========================================================================
// receiveData() callback for successful JSON request, receives incoming JSON
//             data and gets it into global variables for use here
// =========================================================================
function receiveData(arr) {
  if (typeof mapData.rects === 'undefined') {
    mapData.rects = [];	// get this array started first time
    mapData.tracks = [];
  }
  mapData.visible = false;
  for (var mapId in arr) {
    mapData.tracks.push(mapId);
    // save all the incoming mapBoxes into the mapData.rects[] array
    arr[mapId].forEach(function(box) {
      mapData.rects.push(box)});
  }
//  alert(mapData.rects.length + " total map boxes after '" + mapId + "'");
//  alert(mapData.tracks.length + " total track names after '" + mapId + "'");
//  mapData.rects.forEach(function(rect) {
//    alert("x:" + rect.x + ", y:" + rect.y + ", w:" + rect.w + ", h:" + rect.h + ", v:" + rect.v);
//   });
//  for (var i = 0; i < mapData.tracks.length; i++) {
//     var trackName = mapData.tracks[i];
//     var imgData = "img_data_" + trackName;
//     var imgMap  = document.getElementById(imgData);
//     var rect = imgMap.getBoundingClientRect();
//     var msg = ". . . rect[" + i + "] for " + imgData + ":" + Math.floor(rect.left) + "," + Math.floor(rect.right);
// alert(msg);
//     $('#mouseOverData').text(msg);
//  }
}

// =========================================================================
// fetchMapData() sends JSON request, callback to receiveData() upon return
// =========================================================================
function fetchMapData(url) {
  var xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
    if (4 === this.readyState && 200 === this.status) {
      var mapData = JSON.parse(this.responseText);
      receiveData(mapData);
    }
  };
  xmlhttp.open("GET", url, true);
  xmlhttp.send();  // sends request and exits this function
                   // the onreadystatechange callback above will trigger
                   // when the data has safely arrived
}	// function fetchMapData()

// Mouse x,y positions arrive as fractions when the
// WEB page is zoomed into to make the pixels larger.  Hence the Math.floor
// to keep them as integers.
function showMousePos(x, y) {
    var msg = ". . . mouse: x,y: " + x + "," + y;
    $('#mouseXY').text(msg);
  for (var i = 0; i < mapData.tracks.length; i++) {
     var trackName = mapData.tracks[i];
     var imgData = "img_data_" + trackName;
     var imgMap  = document.getElementById(imgData);
     var imageRect = imgMap.getBoundingClientRect();
     msg = ". . . imageRect[" + i + "] for " + imgData + ":" + Math.floor(imageRect.left) + "," + Math.floor(imageRect.top);
// alert(msg);
     $('#mouseOverData').text(msg);
     var imageTop = imageRect.top;
  var x1 = x - Math.floor(imageRect.left);
  var y1 = y - Math.floor(imageRect.top);
  if (x1 >= 0 && y1 >= 0 && x1 < Math.floor(imageRect.width) && y1 < Math.floor(imageRect.height)) {
  }
  if (typeof mapData.rects !== 'undefined') { // if there are rectangles
    var windowUp = false;
    for (var rect of mapData.rects) {
      if (intersectPointRectangle(x1,y1, rect)) {
        var msg = "&nbsp;" + rect.v + "&nbsp;";
        $('#mouseOverText').html(msg);
        var msgWidth = Math.ceil($('#mouseOverText').width());
        var msgHeight = Math.ceil($('#mouseOverText').height());
//        $('#hitBox').text(msg);
//        $('#hitBox').text(msgWidth);
        var posLeft = (x - msgWidth) + "px";
        var posTop = (imageTop + rect.y1) + "px";
        $('#mouseOverContainer').css('left',posLeft);
        $('#mouseOverContainer').css('top',posTop);
        windowUp = true;
        break;
      }
    }
    if (windowUp) {
        if (! mapData.visible) {
            var contain = document.getElementById('mouseOverContainer');
            var text = document.getElementById('mouseOverText');
            text.style.backgroundColor = "#ff69b4";
            var mouseOver = document.querySelector(".wigMouseOver");
            mouseOver.classList.toggle("showMouseOver");
            mapData.visible = true;
        }
    } else {
        if (mapData.visible) {
            var mouseOver = document.querySelector(".wigMouseOver");
            mouseOver.classList.toggle("showMouseOver");
            mapData.visible = false;
        }
    }
  }
  }
}

function getMouseOverData() {
  var x = document.getElementsByClassName("mouseOver");
  var trackCount = x.length;
  var msg = ". . . https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/" + x[0].getAttribute('trashFile');
//  msg = ". . . track count: " + trackCount;
  $('#trashFile').text(msg);
  for (var i = 0; i < x.length; i++) {
     var trashUrl = x[i].getAttribute('trashFile');
//     alert(" looking for track name trash file: '" + trashUrl + "'");
     fetchMapData(trashUrl);
  }
//  fetchMapData(x[0].getAttribute('trashFile'));
}

function getMousePos(evt) {
  return { x: evt.clientX, y: evt.clientY };
}

function addMouseMonitor() {
  window.addEventListener('load', function(evt) {
    getMouseOverData();
  }, false);
  window.addEventListener('mousemove', function(evt) {
    var mousePos = getMousePos(evt);
    showMousePos(Math.floor(mousePos.x), Math.floor(mousePos.y));
  }, false);
}

$('document').ready(addMouseMonitor());
