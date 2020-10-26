
// One single top-level object variable to hold all other global variables
var mapData = {};
// mapData.spans{} - key name is track name, value is an array of
//                   objects: {x1, x2, value}
// mapData.visible - keep track of window visible or not, value: true|false
//                   shouldn't need to do this here, the window knows if it
//                   is visible or not, just ask it for status
// mapData.tracks[]  - list of tracks that came in with mapBoxes
// mapData.boxCount[]  - in same order as tracks[] number of boxes for track
//                       this boxCount is just for debugging information

// =========================================================================
// intersect the point with the rectangle, where rect corners are x1,y1 x2,y2
// =========================================================================
// function intersectPointRectangle(x,y, rect) {
//   var answer = true;
//   if (x <= rect.x1 || x > rect.x2) { answer = false; }
//      else if (y <= rect.y1 || y > rect.y2) { answer = false; }
//   return answer;
// }

// given an X coordinate: x, find the index idx
// in the rects[idx] array where rects[idx].x1 <= x < rects[idx].x2
// returning -1 when not found
// if we knew the array was sorted on x1 we could get out early
//   when x < x1
function findRange(x, rects) {
  var answer = -1;
  for ( var idx in rects ) {
     if ((rects[idx].x1 <= x) && (x < rects[idx].x2)) {
       answer = idx;
       break;
     }
  }
  return answer;
}

function mouseLeftTrackImage(evt) {
  if (mapData.visible) {
    var mouseOver = document.querySelector(".wigMouseOver");
    mouseOver.classList.toggle("showMouseOver");
    mapData.visible = false;
  }
}

function mouseInTrackImage(evt) {
  var trackName = evt.target.id.replace("img_data_", "");
  var firstRect = "";
  if (mapData.spans[trackName]) {
    var lastRect = mapData.spans[trackName].length - 1;
    firstRect = mapData.spans[trackName][lastRect].x1 + ".." + mapData.spans[trackName][lastRect].x2;
  }
  var evX = evt.x;
  var evY = evt.y;
  var oLeft = $(this).offset().left;
  var oTop = $(this).offset().top;
  var offLeft = Math.max(0, Math.floor(evt.x - $(this).offset().left));
  var windowUp = false;     // see if window is supposed to become visible
  var foundIdx = -1;
  var valP = "noX";
  if (mapData.spans[trackName]) {
     foundIdx = findRange(offLeft, mapData.spans[trackName]);
  }
  if (foundIdx > -1) {
    valP = mapData.spans[trackName][foundIdx].v;
    // value to display
    var msg = "&nbsp;" + mapData.spans[trackName][foundIdx].v + "&nbsp;";
    $('#mouseOverText').html(msg);
    var msgWidth = Math.ceil($('#mouseOverText').width());
    var msgHeight = Math.ceil($('#mouseOverText').height());
    var posLeft = (evt.x - msgWidth) + "px";
//    var posTop = (oTop + rect.y1) + "px";
    var posTop = oTop + "px";
    $('#mouseOverContainer').css('left',posLeft);
    $('#mouseOverContainer').css('top',posTop);
    windowUp = true;      // yes, window is to become visible
  }
  var offTop = Math.max(0, Math.floor(evt.y - $(this).offset().top));
  var msg = "<p>. . . mouse in target.id: " + evt.target.id + "(" + trackName + ")[" + foundIdx + "]='" + valP + "' ["  + firstRect + "] at " + offLeft + "," + offTop + ", evX,Y: " + evX + "," + evY + ", offL,T: " + oLeft + "," + oTop + "</p>";
  $('#eventRects').html(msg);
  if (windowUp) {     // the window should become visible
    if (! mapData.visible) {        // should *NOT* have to keep track !*!
      var contain = document.getElementById('mouseOverContainer');
      var mouseOver = document.querySelector(".wigMouseOver");
      mouseOver.classList.toggle("showMouseOver");
      mapData.visible = true;
    }
  } else {    // the window should disappear
    if (mapData.visible) {
      var mouseOver = document.querySelector(".wigMouseOver");
      mouseOver.classList.toggle("showMouseOver");
      mapData.visible = false;
    }
  } //      window visible/not visible
}

// =========================================================================
// receiveData() callback for successful JSON request, receives incoming JSON
//             data and gets it into global variables for use here.
//  The incoming 'arr' is a a set of objects where the object key name is
//      the track name, used here as an array reference: arr[trackName]
//        (currently only one object per json file, one file for each track,
//           this may change to have multiple tracks in one json file.)
//  The value associated with each track name
//  is an array of span definitions, where each element in the array is a
//     mapBox definition object:
//        {x1:n, x2:n, value:s}
//        where n is an integer in the range: 0..width,
//        and s is the value string to display
//     Will need to get them sorted on x1 for efficient searching as
//     they accumulate in the local data structure here.
// =========================================================================
function receiveData(arr) {
  if (typeof mapData.spans === 'undefined') {
    mapData.spans = {};         // get this object started first time
    mapData.tracks = [];
    mapData.boxCount = [];
  }
  mapData.visible = false;
  for (var trackName in arr) {
    mapData.tracks.push(trackName);
    mapData.spans[trackName] = [];      // start array
    // add a 'mousemove' event listener to each track display object
    var objectName = "td_data_" + trackName;
    var objectId  = document.getElementById(objectName);
    objectId.addEventListener('mousemove', mouseInTrackImage);
    objectId.addEventListener('mouseout', mouseLeftTrackImage);
    var itemCount = 0;	// just for monitoring purposes
    // save incoming x1,x2,v data into the mapData.spans[trackName][] array
    arr[trackName].forEach(function(box) {
      mapData.spans[trackName].push(box); ++itemCount});
    mapData.boxCount.push(itemCount);	// merely for debugging watch
  }
  var msg = "<ul>";
  for (var idx in mapData.tracks) {
      var trackName = mapData.tracks[idx];
      var imgData = "td_data_" + trackName;
      var imgMap  = document.getElementById(imgData);
      var imageRect = imgMap.getBoundingClientRect();
      var top = Math.floor(imageRect.top);
      var left = Math.floor(imageRect.left);
      var width = Math.floor(imageRect.width);
      var height = Math.floor(imageRect.height);
     msg += "<li>" + trackName + " at left,top (x,y)(w,h):" + left + "," + top + "(" + width + "," + height + ") has: " + mapData.boxCount[idx] + " mapBoxes</li>";
  }
  msg += "</ul>";
  $('#debugMsg').html(msg);
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
function mouseMoving(x, y) {
  var xyMouse = "<p>. . . mouse at x,y: " + Math.floor(x) + "," + Math.floor(y);
  $('#xyMouse').html(xyMouse);
}	//	function mouseMoving(x, y)

function getMouseOverData() {
  // there could be a number of these mouseOver class elements
  // there is one for each track that has this mouseOver data
  var x = document.getElementsByClassName("mouseOver");
  for (var i = 0; i < x.length; i++) {
     var trashUrl = x[i].getAttribute('trashFile');
     fetchMapData(trashUrl);
  }
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
    mouseMoving(Math.floor(mousePos.x), Math.floor(mousePos.y));
  }, false);
}

$('document').ready(addMouseMonitor());
