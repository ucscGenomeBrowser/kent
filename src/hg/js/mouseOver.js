
// One single top-level object variable to hold all other global variables
var mapData = {};
// mapData.spans{} - key name is track name, value is an array of
//                   objects: {x1, x2, value}
// mapData.visible - keep track of window visible or not, value: true|false
//                   shouldn't need to do this here, the window knows if it
//                   is visible or not, just ask it for status
// mapData.tracks{}  - tracks that came in with mapBoxes, key is track name
//                     value is the number of boxes (for debugging)
// =========================================================================
// intersect the point with the rectangle, where rect corners are x1,y1 x2,y2
// =========================================================================
// function intersectPointRectangle(x,y, rect) {
//   var answer = true;
//   if (x <= rect.x1 || x > rect.x2) { answer = false; }
//      else if (y <= rect.y1 || y > rect.y2) { answer = false; }
//   return answer;
// }

function dbgShowTracks() {
var dbgMsg = "<ol>";
var dbgCounted = 0;
if (typeof(hgTracks) !== "undefined") {
  if (typeof (hgTracks.trackDb) !== "undefined") {
    for (var trackName in hgTracks.trackDb) {
       var isWiggle = false;
       var tdName = "td_data_" + trackName;
       var tdId  = document.getElementById(tdName);
       var tdRect = tdId.getBoundingClientRect();
       var tdLeft = Math.floor(tdRect.left);
       var tdTop = Math.floor(tdRect.top);
       var imgName = "img_data_" + trackName;
       var imgElement  = document.getElementById(imgName);
       var jsonUrl = "no img element";
       if (imgElement) {
   var src = imgElement.src;
   jsonUrl = imgElement.src.replace("hgt/hgt_", "hgt/" + trackName + "_");
    jsonUrl = jsonUrl.replace(".png", ".json");
jsonUrl = jsonUrl.replace("https://hgwdev-hiram.gi.ucsc.edu", "");
       }
       var rec = hgTracks.trackDb[trackName];
       if (rec.type.includes("wig")) { isWiggle = true; }
       if (rec.type.includes("bigWig")) { isWiggle = true; }
       if (! isWiggle) { jsonUrl = "not wiggle"; }
       var viz = vis.enumOrder[hgTracks.trackDb[trackName].visibility];
       dbgMsg += "<li>" + trackName + ", " + rec.type + ", " + viz + ", " + tdLeft + "," + tdTop + ", " + jsonUrl + "</li>";
       dbgCounted += 1;
    }
  } else {
     dbgMsg += "<li>no hgTracks.trackDb</li>";
  }
} else {
     dbgMsg += "<li>no hgTracks</li>";
}
dbgMsg += "</ol>";
// if (hgTracks && hgTracks.trackDb) {
//   dbgMsg = ". hgTracks.trackDb exists with " + objKeyCount(hgTracks.trackDb) + "objKeyCount";
// }
$('#debugMsg').html(dbgMsg);
}

// called from: updateImgForId when it has updated a track in place
// need to refresh the event handlers and json data
function updateMouseOver(trackName) {
// alert("updateMouseOver '" + trackName + ")");
dbgShowTracks();
// var msg = ". . . updateMouseOver trackName: '" + trackName + "'";
// $('#debugMsg').html(msg);
  if (! mapData.tracks) {
 var msg = ". . . updateMouseOver mapData.tracks has disappeared: '" + trackName + "'";
 $('#debugMsg').html(msg);
return;
}
  if (mapData.tracks[trackName]) {
 // var msg = ". . . updateMouseOver trackName: '" + trackName + "'";
 // $('#debugMsg').html(msg);
    var tdName = "td_data_" + trackName;
    var tdElement  = document.getElementById(tdName);
    var id = tdElement.id;
    tdElement.addEventListener('mousemove', mouseInTrackImage);
    tdElement.addEventListener('mouseout', mouseLeftTrackImage);
    var imgName = "img_data_" + trackName;
    var imgElement  = document.getElementById(imgName);
    var src = imgElement.src;
    var jsonUrl = imgElement.src.replace("hgt/hgt_", "hgt/" + trackName + "_");
    jsonUrl = jsonUrl.replace(".png", ".json");
    fetchMapData(jsonUrl);
// alert("updateMouseOver id: '" + jsonUrl + "' valid track here");
  } else {
var msg = ". . . updateMouseOver NOT trackName: '" + trackName + "'";
$('#debugMsg').html(msg);
    return;     // not a track we are working on here
  }
}

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

function popUpDisappear() {
  if (mapData.visible) {        // should *NOT* have to keep track !*!
    var mouseOver = document.querySelector(".wigMouseOver");
    mouseOver.classList.toggle("showMouseOver");
    mapData.visible = false;
  }
}

function popUpVisible() {
  if (! mapData.visible) {        // should *NOT* have to keep track !*!
    var contain = document.getElementById('mouseOverContainer');
    var mouseOver = document.querySelector(".wigMouseOver");
    mouseOver.classList.toggle("showMouseOver");
    mapData.visible = true;
  }
}

function mouseLeftTrackImage(evt) {
  popUpDisappear();
}

// the evt.target.id is the img_data_<trackName> element of the track graphic
function mouseInTrackImage(evt) {
//  $('#msgDebug').html(". . . debug message");
  // the center label also events here, can't use that
  //  plus there is a one pixel line under the center label that has no
  //   id name at all
  if (! evt.target.id.includes("img_data_")) { return; }
  var trackName = evt.target.id.replace("img_data_", "");
  if (trackName.length < 1) { return; }
  // find location of this <td> slice in the image, this is the track
  //   image in the graphic, including left margin and center label
  //   This location follows the window scrolling
  var tdName = "td_data_" + trackName;
  var tdId  = document.getElementById(tdName);
  var tdRect = tdId.getBoundingClientRect();
  var tdLeft = Math.floor(tdRect.left);
  var tdTop = Math.floor(tdRect.top);
  // find the location of the image itself, this could be the single complete
  //  graphic image of all the tracks, or possibly the single image of the
  //  track itself.  This location also follows the window scrolling and can
  //  even go negative when the web browser scrolls a window that is larger
  //  than the width of the web browser.
  var imageId = document.getElementById(evt.target.id);
  var imageRect = imageId.getBoundingClientRect();
  var imageLeft = Math.floor(imageRect.left);
  var imageTop = Math.floor(imageRect.top);
  var srcUrl = evt.target.src;
  var evX = evt.x;      // location of mouse on the web browser screen
  var evY = evt.y;
var msg = ". . . mouse x,y: " + evX + "," + evY";
$('#xyMouse').html(msg);
  var offLeft = Math.max(0, Math.floor(evt.x - tdLeft));
  var windowUp = false;     // see if window is supposed to become visible
  var foundIdx = -1;
  var valP = "noX";
  var lengthSpansArray = -1;
  if (mapData.spans[trackName]) {
     foundIdx = findRange(offLeft, mapData.spans[trackName]);
     lengthSpansArray = mapData.spans[trackName].length;
  }
  if (foundIdx > -1) {
    valP = mapData.spans[trackName][foundIdx].v;
    // value to display
    var msg = "&nbsp;" + mapData.spans[trackName][foundIdx].v + "&nbsp;";
    $('#mouseOverText').html(msg);
    var msgWidth = Math.ceil($('#mouseOverText').width());
    var msgHeight = Math.ceil($('#mouseOverText').height());
    var posLeft = evt.x - msgWidth + "px";
    var posTop = tdTop + "px";
    $('#mouseOverContainer').css('left',posLeft);
    $('#mouseOverContainer').css('top',posTop);
    windowUp = true;      // yes, window is to become visible
  }
//  var offTop = Math.max(0, Math.floor(evt.y - $(this).offset().top));
//  var msg = "<p>. . . mouse in target.id: " + evt.target.id + "(" + trackName + ")[" + foundIdx + "]='" + valP + "' spansLength: " + lengthSpansArray + " at " + offLeft + "," + offTop + ", evX,Y: " + evX + "," + evY + "</p>";
//  $('#eventRects').html(msg);
  if (windowUp) {     // the window should become visible
    popUpVisible();
  } else {    // the window should disappear
    popUpDisappear();
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
    mapData.tracks = {};
  }
  mapData.visible = false;
  for (var trackName in arr) {
// alert("receiving data for " + trackName);
    mapData.spans[trackName] = [];      // start array
    // add a 'mousemove' and 'mouseout' event listener to each track
    //     display object
    var objectName = "td_data_" + trackName;
    var objectId  = document.getElementById(objectName);
    if (! objectId) { return; } // not sure why objects are not found
    objectId.addEventListener('mousemove', mouseInTrackImage);
    objectId.addEventListener('mouseout', mouseLeftTrackImage);
    // would be nice to know when the window is scrolling in the browser so
    // the text box could disappear.  These do not appear to work.
    // Beware, onscroll event is continuous while scrolling.
//    objectId.addEventListener('onscroll', mouseLeftTrackImage);
//    window.addEventListener('onscroll', mouseLeftTrackImage);
    var itemCount = 0;	// just for monitoring purposes
    // save incoming x1,x2,v data into the mapData.spans[trackName][] array
    arr[trackName].forEach(function(box) {
      mapData.spans[trackName].push(box); ++itemCount});
    mapData.tracks[trackName] = itemCount;	// merely for debugging watch
  }
// dbgShowTracks();
//  var msg = "<ul>";
//  for (var trackName in mapData.tracks) {
//      var imgData = "td_data_" + trackName;
//      var imgMap  = document.getElementById(imgData);
//      var imageRect = imgMap.getBoundingClientRect();
//      var top = Math.floor(imageRect.top);
//      var left = Math.floor(imageRect.left);
//      var width = Math.floor(imageRect.width);
//      var height = Math.floor(imageRect.height);
//     msg += "<li>" + trackName + " at left,top (x,y)(w,h):" + left + "," + top + "(" + width + "," + height + ") has: " + mapData.tracks[trackName] + " mapBoxes</li>";
//  }
//  msg += "</ul>";
//  $('#debugMsg').html(msg);
}

// =========================================================================
// fetchMapData() sends JSON request, callback to receiveData() upon return
// =========================================================================
function fetchMapData(url) {
//  alert("fetchMapData(" + url + ")");
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
// function mouseMoving(x, y) {
//   var xyMouse = "<p>. . . mouse at x,y: " + Math.floor(x) + "," + Math.floor(y);
//   $('#xyMouse').html(xyMouse);
// }	//	function mouseMoving(x, y)

function getMouseOverData() {
  if (typeof(hgTracks) !== "undefined") {
    if (typeof (hgTracks.trackDb) !== "undefined") {
      for (var trackName in hgTracks.trackDb) {
       var isWiggle = false;
       var rec = hgTracks.trackDb[trackName];
       if (rec.type.includes("wig")) { isWiggle = true; }
       if (rec.type.includes("bigWig")) { isWiggle = true; }
       if (! isWiggle) { continue; }
       var imgName = "img_data_" + trackName;
       var imgElement  = document.getElementById(imgName);
       if (imgElement) {
         var src = imgElement.src;
         var jsonUrl = src.replace("hgt/hgt_", "hgt/" + trackName + "_");
         jsonUrl = jsonUrl.replace(".png", ".json");
         jsonUrl = jsonUrl.replace("https://hgwdev-hiram.gi.ucsc.edu", "");
         fetchMapData(jsonUrl);
       }
      }
    }
  }
dbgShowTracks();
  // there could be a number of these mouseOver class elements
  // there is one for each track that has this mouseOver data
//  var x = document.getElementsByClassName("mouseOver");
//  for (var i = 0; i < x.length; i++) {
//     var trashUrl = x[i].getAttribute('trashFile');
//     fetchMapData(trashUrl);
//  }
}

// function getMousePos(evt) {
//   return { x: evt.clientX, y: evt.clientY };
// }

// function addMouseMonitor() {
//   window.addEventListener('load', function(evt) {
//     getMouseOverData();
//   }, false);
//  window.addEventListener('mousemove', function(evt) {
//    var mousePos = getMousePos(evt);
//    mouseMoving(Math.floor(mousePos.x), Math.floor(mousePos.y));
//  }, false);
// }

// $('document').ready(addMouseMonitor());
