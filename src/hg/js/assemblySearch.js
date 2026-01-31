// global variables:

/* jshint esnext: true */

var debug = false;
var measureTiming = false;
var urlParams;
var query = "";
var maxItemsOutput = 500;
var asmIdText = null;
// adjust default here and in assemblySearch.html
var browserExist = "mayExist";
var betterCommonName = null;
var comment = null;
var stateObject = {};	// maintain page state
var requestSubmitButton = null;
var completedAsmId = new Map();	// keep track of requests completed
				// so they won't be repeated
var maxLength = 1024;	// limit all incoming strings to this length

// This function is called on DOMContentLoaded as the initialization
//  procedure for first time page draw
document.addEventListener('DOMContentLoaded', function() {
    // allow semi colon separators as well as ampersand
    var urlArgList = window.location.search.replaceAll(";", "&");
    urlParams = new URLSearchParams(urlArgList);
    if (urlParams.has('level')) {
       let asmLevel = urlParams.get('level');
       document.getElementById('asmLevelAny').checked = true;  // default
       // only one of these four cases will be true
       if (asmLevel === "complete")
         document.getElementById('asmLevelComplete').checked = true;
       if (asmLevel === "chromosome")
         document.getElementById('asmLevelChromosome').checked = true;
       if (asmLevel === "scaffold")
         document.getElementById('asmLevelScaffold').checked = true;
       if (asmLevel === "contig")
         document.getElementById('asmLevelContig').checked = true;
    }
    if (urlParams.has('status')) {
       let asmStatus = urlParams.get('status');
       document.getElementById('statusAny').checked = true; // default
       // only one of these three cases will be true
       if (asmStatus === "latest")
          document.getElementById('statusLatest').checked = true;
       if (asmStatus === "replaced")
          document.getElementById('statusReplaced').checked = true;
       if (asmStatus === "suppressed")
          document.getElementById('statusSuppressed').checked = true;
    }
    if (urlParams.has('category')) {
       let refSeqCategory = urlParams.get('category');
       document.getElementById('refSeqAny').checked = true; // default
       // only one of these etwo cases will be true
       if (refSeqCategory === "reference")
          document.getElementById('refSeqReference').checked = true;
       if (refSeqCategory === "representative")
          document.getElementById('refSeqRepresentative').checked = true;
    }
    // default starts as hidden
    let copyIcon0 = document.getElementById('copyIcon0');
    stateObject.copyIcon0 = copyIcon0.innerHTML;
    document.getElementById('urlCopyLink').style.display = "none";
    stateObject.advancedSearchVisible = false;
    advancedSearchVisible(false);
    if (urlParams.has('advancedSearch')) {
        let advancedSearch = urlParams.get('advancedSearch');
        advancedSearchVisible(advancedSearch);
        stateObject.advancedSearchVisible = advancedSearch;
    }
    if (urlParams.has('measureTiming')) { // accepts no value or other string
       var measureValue = urlParams.get('measureTiming');
       if ("0" === measureValue | "off" === measureValue) {
         measureTiming = false;
       } else {			// any other string turns it on
         measureTiming = true;
       }
    }
    if (urlParams.has('browser')) {
       var browserValue = urlParams.get('browser');
       if ("mayExist" === browserValue) {
          browserExist = "mayExist";
          document.getElementById('mustExist').checked = true;
          document.getElementById('notExist').checked = true;
       } else if ("mustExist" === browserValue) {
          browserExist = "mustExist";
          document.getElementById('mustExist').checked = true;
          document.getElementById('notExist').checked = false;
       } else if ("notExist" === browserValue) {
          browserExist = "notExist";
          document.getElementById('mustExist').checked = false;
          document.getElementById('notExist').checked = true;
//       } else {
         // not going to worry about this here today, but there should be
         // a non-obtrusive dialog pop-up message about illegal arguments
//         alert("warning: illegal value for browser=... must be one of: mayExist, mustExist, notExist");
       }
    }
    if (urlParams.has('debug')) { // accepts no value or other string
       var debugValue = urlParams.get('debug');
       if ("0" === debugValue | "off" === debugValue) {
         debug = false;
       } else {			// any other string turns it on
         debug = true;
       }
    }

    // add extra element to the help text bullet list for API example
    if (debug) {
      var searchTipList = document.getElementById("searchTipList");
      // Create a new list item
      var li = document.createElement("li");
      li.innerHTML = "example API call: <span id=\"recentAjax\">n/a</span>";
      // Append the new list item to the ordered list
      searchTipList.appendChild(li);
    }

    var searchForm = document.getElementById('searchForm');
    var advancedSearchButton = document.getElementById('advancedSearchButton');
    var searchInput = document.getElementById('searchBox');
    var clearButton = document.getElementById('clearSearch');
    asmIdText = document.getElementById("formAsmId");
    asmIdText.textContent = asmIdText.textContent.substring(0,maxLength);
    betterCommonName = document.getElementById("betterCommonName");
    betterCommonName.value = betterCommonName.value.substring(0,maxLength);
    comment = document.getElementById("comment");
    comment.value = comment.value.substring(0,maxLength);
    requestSubmitButton = document.getElementById("submitButton");

    document.getElementById("modalFeedback").addEventListener("submit", checkForm, false);
    modalInit();
    var tableBody = document.getElementById('tableBody');
    tableBody.innerHTML = '<tr><td style="text-align:center;" colspan=8><b>(empty table)</b></td></tr>';

    clearButton.addEventListener('click', function() {
        searchInput.value = ''; // Clear the search input field
    });

    searchForm.addEventListener('submit', function(event) {
        event.preventDefault(); // Prevent form submission

        // the trim() removes stray white space before or after the string
        var searchTerm = document.getElementById('searchBox').value.trim();
        var resultCountLimit = document.getElementById('maxItemsOutput');
        var mustExist = document.getElementById('mustExist').checked;
        var notExist = document.getElementById('notExist').checked;
        browserExist = "mustExist";
        if (mustExist && notExist) {
           browserExist = "mayExist";
        } else if (notExist) {
           browserExist = "notExist";
        }
        makeRequest(searchTerm, browserExist, resultCountLimit.value);
    });

    advancedSearchButton.addEventListener('click', function() {
       document.getElementById('urlCopyLink').style.display = "none";
       let searchOptions = document.getElementById("advancedSearchOptions");
       // I don't know why it is false the first time ?
       if (! searchOptions.style.display || searchOptions.style.display === "none") {
          advancedSearchVisible(true);
       } else {
          advancedSearchVisible(false);
       }
    });

    // restore history on back button
    window.addEventListener('popstate', function(e) {
       var state = event.state;
       if (state) {
          stateObject.queryString = state.queryString;
          stateObject.advancedSearchVisible = state.advancedSearchVisible;
          stateObject.maxItemsOutput = state.maxItemsOutput;
          stateObject.browser = state.browser;
          stateObject.debug = state.debug;
          stateObject.measureTiming = state.measureTiming;
          stateObject.wordMatch = state.wordMatch;
          stateObject.asmStatus = state.asmStatus;
          stateObject.refSeqCategory = state.refSeqCategory;
          stateObject.asmLevel = state.asmLevel;
          stateObject.jsonData = state.jsonData;
          document.getElementById('mustExist').checked = false;
          document.getElementById('notExist').checked = false;
          if (stateObject.browser === "mustExist") {
             document.getElementById('mustExist').checked = true;
          }
          if (stateObject.browser === "notExist") {
             document.getElementById('notExist').checked = true;
          }
          if (stateObject.browser === "mayExist") {
             document.getElementById('mustExist').checked = true;
             document.getElementById('notExist').checked = true;
          }
          advancedSearchVisible(stateObject.advancedSearchVisible);
          if (stateObject.wordMatch === "allWords") {
             document.getElementById("allWords").checked = true;
          } else {
             document.getElementById("anyWord").checked = true;
          }
          // only one of these four cases will be true
          if (stateObject.asmStatus === "statusAny")
             document.getElementById('statusAny').checked = true;
          if (stateObject.asmStatus === "latest")
             document.getElementById('statusLatest').checked = true;
          if (stateObject.asmStatus === "replaced")
             document.getElementById('statusReplaced').checked = true;
          if (stateObject.asmStatus === "suppressed")
             document.getElementById('statusSuppressed').checked = true;

          // only one of these three cases will be true
          if (stateObject.refSeqCategory === "refSeqAny")
             document.getElementById('refSeqAny').checked = true;
          if (stateObject.refSeqCategory === "reference")
             document.getElementById('refSeqReference').checked = true;
          if (stateObject.refSeqCategory === "representative")
             document.getElementById('refSeqRepresentative').checked = true;

          // only one of these five cases will be true
          if (stateObject.asmLevel === "asmLevelAny")
             document.getElementById('asmLevelAny').checked = true;
          if (stateObject.asmLevel === "complete")
             document.getElementById('asmLevelComplete').checked = true;
          if (stateObject.asmLevel === "chromosome")
             document.getElementById('asmLevelChromosome').checked = true;
          if (stateObject.asmLevel === "scaffold")
             document.getElementById('asmLevelScaffold').checked = true;
          if (stateObject.asmLevel === "contig")
             document.getElementById('asmLevelContig').checked = true;

          document.getElementById('searchBox').value = stateObject.queryString;
	  populateTableAndInfo(JSON.parse(stateObject.jsonData));
//          alert("state: '" + JSON.stringify(stateObject) + "'");
       }
    });

    var tableHeader = document.getElementById('tableHeader');
    headerRefresh(tableHeader);

    if (urlParams.has('maxItemsOutput')) {
       maxItemsOutput = parseInt(urlParams.get('maxItemsOutput'), 10);
       if (maxItemsOutput < 1) {
         maxItemsOutput = 1;
       } else if (maxItemsOutput > 1000) {
         maxItemsOutput = 1000;
       }
       document.getElementById('maxItemsOutput').value = maxItemsOutput;
    }
    if (urlParams.has('q')) {
       query = urlParams.get('q').trim();
       if (query.length > 0) {
          searchInput.value = query;
          document.getElementById('submitSearch').click();
       }
    }
    document.getElementById("measureTiming").style.display = "none";
});

// refresh the thead columns in the specified table
function headerRefresh(tableHead) {
  // clear existing content
  tableHead.innerHTML = '';
  // re-populate header row - the sortable system added a class to
  //  the last sorted column, need to rebuild the headerRow to get the
  //  header back to pristine condition for the next sort
  var headerRow = '<tr>';
  let circleQuestion = '<svg width="24" height="24"> <circle cx="12" cy="12" r="10" fill="#4444ff" /> <text x="50%" y="50%" text-anchor="middle" fill="white" font-size="13px" font-family="Verdana" dy=".3em">?</text>?</svg>';
  headerRow += '<th><div class=tooltip>View/<br>request&nbsp;&#9432;<span onclick="event.stopPropagation()" class="tooltiptext"><b>view</b> opens the genome browser for an existing assembly, <b>request</b> opens an assembly request form.</span></div></th>';
  headerRow += '<th><div class="tooltip">English common name&nbsp;&#9432;<span onclick="event.stopPropagation()" class="tooltiptext">English common name.  This field may also include a unique identifier and the year the assembly was released.</span></div></th>';
  headerRow += '<th><div class="tooltip">Scientific name&nbsp;&#9432;<span onclick="event.stopPropagation()" class="tooltiptext">The Scientific name is the standardized, Latin-based designation used in biological taxonomy to formally classify and identify a species.  Obtained from NCBI taxonomy.</span></div></th>';
  headerRow += '<th><div class="tooltip">NCBI Assembly&nbsp;&#9432;<span onclick="event.stopPropagation()" class="tooltiptext">The GC accession number for the assembly, if available, links to the corresponding NCBI resource record or UCSC download options for local UCSC assemblies.</span></div></th>';
  headerRow += '<th><div class="tooltip">Year&nbsp;&#9432;<span onclick="event.stopPropagation()" class="tooltiptextright">Year assembly was released.</span></div></th>';
  headerRow += '<th><div class="tooltip"><em>GenArk</em> clade&nbsp;&#9432;<span onclick="event.stopPropagation()" class="tooltiptextright">Clade specification as found in the <a href="https://hgdownload.soe.ucsc.edu/hubs/index.html" target=_blank>GenArk</a> system, this is not a strict taxonomy category, merely a division of assemblies into several categories.</span></div></th>';
  headerRow += '<th><div class="tooltip">Description&nbsp;&#9432;<span onclick="event.stopPropagation()" class="tooltiptextright">Description may include other names, the <b>taxId</b>, year of assembly release and assembly center.</span></div></th>';
  headerRow += '<th><div class="tooltip">Status&nbsp;&#9432;<span onclick="event.stopPropagation()" class="tooltiptextright">When specified, status will show the <b>Assembly status</b> the <b>RefSeq category</b> and the <b>Assembly level</b>.</span></div></th>';
  headerRow += '</tr>';
  tableHead.innerHTML = headerRow;
}

// call with visible true to make visible, false to hide
function advancedSearchVisible(visible) {
  var advancedSearchButton = document.getElementById("advancedSearchButton");
  var searchOptions = document.getElementById("advancedSearchOptions");
  if (visible) {
    searchOptions.style.display = "flex";
    advancedSearchButton.textContent = "Hide search options";
    stateObject.advancedSearchVisible = true;
  } else {
    searchOptions.style.display = "none";
    advancedSearchButton.textContent = "Show search options";
    stateObject.advancedSearchVisible = false;
  }
}

// Function to highlight words in the result that match the words
//  in the queryString.  Work through each item of the rowData object,
//  for each string, find out if it matches any of the words in the
//  queryString.  This is tricky, there are extra characters besides
//  just [a-zA-Z] that are getting in the way of these matches, where
//  the MySQL could match, for example: checking:
//      'HG02257' =? '(HG02257.pat'
//      'HG02257' =? 'HG02257.alt.pat.f1_v2'
//  doesn't match here, but MySQL match did

function highlightMatch(queryString, rowData) {
    // fixup the queryString words to get rid of the special characters
    var words = queryString.split(/\s+/);
    var wholeWord = [];	// going to be words that match completely
    var prefix = [];	// going to be words that match prefix
    for (let word of words) {
       var noPrefix = word.replace(/^[-+]/, '');	// remove + - beginning
       if (noPrefix.endsWith("*")) {
         let subPrefix = noPrefix.replace(/\*$/, '').match(/(\w+)/g);
         if (subPrefix.length > 1) {
           let i = 0;
           for ( ; i < subPrefix.length - 1; i++) {
              wholeWord.push(subPrefix[i]);
           }
           prefix.push(subPrefix[i].replace(/\*$/, ''));
         } else {
           prefix.push(noPrefix.replace(/\*$/, ''));
         }
       } else {
         wholeWord.push(noPrefix);
       }
    }
    if (wholeWord.length > 0) {
      for (let word of wholeWord) {
        for (let key in rowData) {
           if (rowData.hasOwnProperty(key)) {
              if (typeof rowData[key] === 'string') {
                 let wholeSubs = word.match(/(\w+)/g);
                 if (wholeSubs && wholeSubs.length > 0) {
                   for (let whole of wholeSubs) {
                     let newString = "";
                     let value = rowData[key];
                     let subWords = value.match(/(\w+)|(\W+)/g);
                     for (let subWord of subWords) {
                       if ( whole.toLowerCase() === subWord.toLowerCase() ) {
                          newString += "<span class='highlight'>" + subWord + "</span>";
                       } else {
                          newString += subWord;
                       }
                     }
                     newString = newString.trim();
                     if (newString !== rowData[key])
                        rowData[key] = newString;
                   }	//	for (let whole of wholeSubs)
                 }	//	if (wholeSubs.length > 0)
              }	//	if (typeof rowData[key] === 'string')
           }	//	if (rowData.hasOwnProperty(key))
        }	//	for (let key in rowData)
      }	//	for (let word of wholeWord)
    }	//	if (wholeWord.length > 0)
    if (prefix.length > 0) {
      for (let word of prefix) {
        for (let key in rowData) {
           if (rowData.hasOwnProperty(key)) {
              if (typeof rowData[key] === 'string') {
                 let value = rowData[key];
                 let subWords = value.match(/(\w+)|(\W+)/g);
                 let newString = "";
                 for (let subWord of subWords) {
                   if ( subWord.toLowerCase().startsWith(word.toLowerCase())) {
                      newString += "<span class='highlight'>" + subWord + "</span>";
                   } else {
                      newString += subWord;
                   }
                 }
                 newString = newString.trim();
                 if (newString !== rowData[key])
                    rowData[key] = newString;
              }
           }
        }
      }
    }
}

// Function to generate the table and extra information
function populateTableAndInfo(jsonData) {
    var tableHeader = document.getElementById('tableHeader');
    var tableBody = document.getElementById('tableBody');
    var metaData = document.getElementById('metaData');
    document.getElementById('resultCounts').innerHTML = "";
    document.getElementById('elapsedTime').innerHTML = "0";

    // Clear existing table content
    tableHeader.innerHTML = '';
    tableBody.innerHTML = '';
    metaData.innerHTML = '';

    // Extract the genomic entries and the extra info
    var genomicEntries = {};
    var extraInfo = {};

    for (var key in jsonData) {
        if (jsonData[key] && jsonData[key].scientificName) {
            genomicEntries[key] = jsonData[key];
        } else {
            extraInfo[key] = jsonData[key];
        }
    }

    headerRefresh(tableHeader);

    var count = 0;
    for (var id in genomicEntries) {
        highlightMatch(extraInfo.q.trim(), genomicEntries[id]);
        var dataRow = '<tr>';
        var browserUrl = id;
        var asmInfoUrl = id;
        if (genomicEntries[id].browserExists) {
          if (id.startsWith("GC")) {
            browserUrl = "<a href='/h/" + id + "?position=lastDbPos' target=_blank>view</a>";
            asmInfoUrl = "<a href='https://www.ncbi.nlm.nih.gov/assembly/" + id + "' target=_blank>" + id + "</a>";
          } else {
            browserUrl = "<a href='/cgi-bin/hgTracks?db=" + id + "' target=_blank>view</a>";
            asmInfoUrl = "<a href='https://hgdownload.soe.ucsc.edu/goldenPath/" + id + "/bigZips/' target=_blank>" + id + "</a>";
          }
          dataRow += "<th>" + browserUrl + "</th>";
        } else {
          dataRow += "<th><button type=button' onclick='asmOpenModal(this)' name=" + id + ">request</button></th>";
        }
        dataRow += "<td>" + genomicEntries[id].commonName + "</td>";
        dataRow += "<td>" + genomicEntries[id].scientificName + "</td>";
        dataRow += "<th>" + asmInfoUrl + "</th>";
        dataRow += "<td>" + genomicEntries[id].year + "</td>";
        dataRow += "<td>" + genomicEntries[id].clade + "</td>";
        dataRow += "<td>" + genomicEntries[id].description + "</td>";
        var status =  "<td>";
        var breakSpace = "";	// first one does not need the break
        if (genomicEntries[id].versionStatus) {
           status += breakSpace + genomicEntries[id].versionStatus;
           breakSpace = "<br>";	// subsequent words will have the break
        }
        if (genomicEntries[id].refSeqCategory) {
           status += breakSpace + genomicEntries[id].refSeqCategory;
           breakSpace = "<br>";	// subsequent words will have the break
        }
        if (genomicEntries[id].assemblyLevel) {
           status += breakSpace + genomicEntries[id].assemblyLevel;
        }
        status += "</td>";
        dataRow += status;
        dataRow += '</tr>';
        tableBody.innerHTML += dataRow;
    }
    var dataTable = document.getElementById('dataTable');
    sorttable.makeSortable(dataTable);

    var itemCount = parseInt(extraInfo.itemCount, 10);
    var totalMatchCount = parseInt(extraInfo.totalMatchCount, 10);
    var availableAssemblies = parseInt(extraInfo.availableAssemblies, 10);

    var resultCounts = "<em>Query: </em><b>'" + extraInfo.q.trim() + "'</b>, ";
    if ( itemCount === totalMatchCount ) {
      resultCounts += "<em>Matches </em><b>" + itemCount.toLocaleString() + "</b> <em>assemblies, </em>";
    } else {
      resultCounts += "<em>Matches </em><b>" + totalMatchCount.toLocaleString() + "</b> <em>assemblies, </em> ";
      resultCounts += "<em>limited display here of </em><b>" + itemCount.toLocaleString() + "</b> <em>matching assemblies, </em> ";
    }
    resultCounts += "<em>from a total of </em><b>" + availableAssemblies.toLocaleString() + "</b>.";
    document.getElementById('resultCounts').innerHTML = resultCounts;
    if (measureTiming) {
      var etMs = extraInfo.elapsedTimeMs;
      var elapsedTime = "<b>" + etMs.toLocaleString() + "</b> <em>milliseconds</em>";
      if ( etMs > 1000 ) {
         var etSec = etMs/1000;
         elapsedTime = "<b>" + etSec.toFixed(2) + "</b> <em>seconds</em>";
      }
      document.getElementById('elapsedTime').innerHTML = elapsedTime.toLocaleString();
      document.getElementById("measureTiming").style.display = "inline";
    } else {
      document.getElementById("measureTiming").style.display = "none";
    }
    document.getElementById('urlCopyLink').style.display = "inline";
}	//	function populateTableAndInfo(jsonData)

function enableButtons() {
    document.getElementById('submitSearch').disabled = false;
    document.getElementById('clearSearch').disabled = false;
}

function disableButtons() {
    document.getElementById('submitSearch').disabled = true;
    document.getElementById('clearSearch').disabled = true;
}

function parentTable(e) {
  while (e) {
      e = e.parentNode;
      if (e.tagName.toLowerCase() === 'table') {
          return e;
      }
  }
  return undefined;
}

function whichRow(e) {
  while (e) {
    if (e.rowIndex) {
      return e.rowIndex;
    }
    e = e.parentNode;
  }
  return undefined;
}

function closeModal(e)
{
  document.getElementById("modalWrapper").className = "";
  if (e.preventDefault) {
    e.preventDefault();
  } else {
    e.returnValue = false;
  }
}

function clickHandler(e) {
  if(!e.target) e.target = e.srcElement;
    if(e.target.tagName === "DIV") {
      if(e.target.id != "modalWindow") closeModal(e);
  }
}

function keyHandler(e) {
  if(e.keyCode === 27) closeModal(e);
}

function modalInit() {
  if(document.addEventListener) {
    document.getElementById("modalClose").addEventListener("click", closeModal, false);
    document.addEventListener("click", clickHandler, false);
    document.addEventListener("keydown", keyHandler, false);
  } else {
    document.getElementById("modalClose").attachEvent("onclick", closeModal);
    document.attachEvent("onclick", clickHandler);
    document.attachEvent("onkeydown", keyHandler);
  }
}

function failedRequest(url) {
  requestSubmitButton.value = "request failed";
  requestSubmitButton.disabled = true;
//      garStatus.innerHTML = "FAILED: '" + url + "'";
}

function sendRequest(name, email, asmId, betterName, comment) {
    var urlComponents = encodeURIComponent(name) + "&email=" + encodeURIComponent(email) + "&asmId=" + encodeURIComponent(asmId) + "&betterName=" + encodeURIComponent(betterName) + "&comment=" + encodeURIComponent(comment);

    var url = "/cgi-bin/asr?name=" + urlComponents;
// information about escaping characters:
// https://stackoverflow.com/questions/10772066/escaping-special-character-in-a-url/10772079
// encodeURI() will not encode: ~!@#$&*()=:/,;?+'
// encodeURIComponent() will not encode: ~!*()'

//    var encoded = encodeURIComponent(url);
//    encoded = encoded.replace("'","&rsquo;");
//    var encoded = encodeURI(cleaner);
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function() {
         if (4 === this.readyState && 200 === this.status) {
            requestSubmitButton.value = "request completed";
         } else {
            if (4 === this.readyState && 404 === this.status) {
               failedRequest(url);
            }
         }
       };
    xmlhttp.open("GET", url, true);
    xmlhttp.send();

}  //      sendRequest: function(name, email. asmId)

// borrowed this code from utils.js
function copyToClipboard(ev) {
    /* copy a piece of text to clipboard. event.target is some DIV or SVG that is an icon.
     * The attribute data-target of this element is the ID of the element that contains the text to copy.
     * The text is either in the attribute data-copy or the innerText.
     * see C function printCopyToClipboardButton(iconId, targetId);
     * */

    ev.preventDefault();

    var buttonEl = ev.target.closest("button"); // user can click SVG or BUTTON element

    var targetId = buttonEl.getAttribute("data-target");
    if (targetId===null)
        targetId = ev.target.parentNode.getAttribute("data-target");
    var textEl = document.getElementById(targetId);
    var text = textEl.getAttribute("data-copy");
    if (text===null)
        text = textEl.innerText;

    var textArea = document.createElement("textarea");
    textArea.value = text;
    // Avoid scrolling to bottom
    textArea.style.top = "0";
    textArea.style.left = "0";
    textArea.style.position = "fixed";
    document.body.appendChild(textArea);
    textArea.focus();
    textArea.select();
    document.execCommand('copy');
    document.body.removeChild(textArea);
    buttonEl.innerHTML = 'Copied';
    ev.preventDefault();
}

// do not allow both checkboxes to go off
function atLeastOneCheckBoxOn(e) {
  var mustExist = document.getElementById('mustExist').checked;
  var notExist = document.getElementById('notExist').checked;
  if (! mustExist && ! notExist ) {  // turn on the other one when both off
     if (e.name === "mustExist") {
       document.getElementById('notExist').checked = true;
     } else {
       document.getElementById('mustExist').checked = true;
     }
  }
}

function checkForm(e) {
  if (requestSubmitButton.value === "request completed") {
     if (e.preventDefault) {
       e.preventDefault();
     } else {
       e.returnValue = false;
     }
     closeModal(e);
     return;
  }
  var form = (e.target) ? e.target : e.srcElement;
  if(form.name.value === "") {
    alert("Please enter your Name");
    form.name.focus();
    if (e.preventDefault) {
      e.preventDefault();
    } else {
      e.returnValue = false;
    }
    return;
  }
  form.name.value = form.name.value.substring(0,maxLength);
  if(form.email.value === "") {
    alert("Please enter a valid Email address");
    form.email.focus();
    if (e.preventDefault) {
      e.preventDefault();
    } else {
      e.returnValue = false;
    }
    return;
  }
  form.email.value = form.email.value.substring(0,maxLength);
// validation regex from:
//      https://www.w3resource.com/javascript/form/email-validation.php
// another example from
//      https://www.simplilearn.com/tutorials/javascript-tutorial/email-validation-in-javascript
//   var validRegex = /^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9-]+(?:\.[a-zA-Z0-9-]+)*$/;
// another example from
//      https://ui.dev/validate-email-address-javascript/
//      return /\S+@\S+\.\S+/.test(email)
//      return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)
//      var re = /^[^\s@]+@[^\s@]+$/;
//  if (re.test(email)) { OK }

//    var validEmail = /^\w+([\.-]?\w+)*@\w+([\.-]?\w+)*(\.\w{2,3})+$/;
  var validEmail = /^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9-]+(?:\.[a-zA-Z0-9-]+)*$/;
  if(! validEmail.test(form.email.value)) {
    alert("You have entered an invalid email address!");
    form.email.focus();
    if (e.preventDefault) {
      e.preventDefault();
    } else {
      e.returnValue = false;
    }
    return;
  }
  sendRequest(form.name.value, form.email.value, asmIdText.textContent, betterCommonName.value, comment.value);
  if (e.preventDefault) {
    e.preventDefault();
  } else {
    e.returnValue = false;
  }
}    //      checkForm: function(e)

function asmOpenModal(e) {
  if (e.name) {
    var modalWindow = document.getElementById("modalWindow");
    var pTable = parentTable(e);
    var thisRow = whichRow(e);
    var colGroup = document.getElementById('colDefinitions');
    var comName = "n/a";
    var sciName = "n/a";
    var descr = "n/a";
    var i = 0;
    for (i = 0; i < colGroup.children.length; i++) {
      if (colGroup.children[i].id === "comName") {
        comName = pTable.rows[thisRow].cells[i].innerText;
      } else if (colGroup.children[i].id === "sciName") {
        sciName = pTable.rows[thisRow].cells[i].innerText;
      } else if (colGroup.children[i].id === "description") {
        descr = pTable.rows[thisRow].cells[i].innerText;
      }
    }
    document.getElementById("commonName").textContent = comName;
    document.getElementById("formSciName").textContent = sciName;
    document.getElementById("formAsmId").textContent = e.name;
    document.getElementById("comment").value = descr;
    if (completedAsmId.has(e.name)) {
      requestSubmitButton.value = "request completed";
      requestSubmitButton.disabled = false;
      document.getElementById("modalWrapper").className = "overlay";
      return;
    } else {
      completedAsmId.set(e.name, true);
      requestSubmitButton.value = "Submit request";
    }
    document.getElementById("modalWrapper").className = "overlay";
    requestSubmitButton.disabled = false;
    var overflow = modalWindow.offsetHeight - document.documentElement.clientHeight;
    if (overflow > 0) {
        modalWindow.style.maxHeight = (parseInt(window.getComputedStyle(modalWindow).height) - overflow) + "px";
    }
    modalWindow.style.marginTop = (-modalWindow.offsetHeight)/2 + "px";
    modalWindow.style.marginLeft = (-modalWindow.offsetWidth)/2 + "px";
  }
  if (e.preventDefault) {
    e.preventDefault();
  } else {
    e.returnValue = false;
  }
}

function optionsChange(e) {
  // options are changing, share URL is no longer viable, eliminate it
  document.getElementById('copyIcon0').innerHTML = stateObject.copyIcon0;
  document.getElementById('urlCopyLink').style.display = "none";
}

function makeRequest(query, browserExist, resultLimit) {
    // Disable the submit button
    disableButtons();
    var wordMatch = document.querySelector('input[name="wordMatch"]:checked').value;
    var asmStatus = document.querySelector('input[name="asmStatus"]:checked').value;
    var refSeqCategory = document.querySelector('input[name="refSeqCategory"]:checked').value;
    var asmLevel = document.querySelector('input[name="asmLevel"]:checked').value;

    // start with what the user requested:
    var queryString = query;

    // for allWords, place + sign in front of each word if not already there
    if (wordMatch === "allWords") {
      var words = queryString.split(/\s+/);
      if (words.length > 1) {	// not needed on only one word
        var queryPlus = "";	// compose new query string
        let inQuote = false;
        words.forEach(function(word) {
          if (word.match(/^[-+]?"/)) {
             if (/^[-+]/.test(word))
                queryPlus += " " + word; // do not add + to - or + already there
             else
                queryPlus += " +" + word;
             inQuote = true;
          } else if (inQuote) {
             queryPlus += " " + word; // space separates each word
             if (word.endsWith('"'))
                inQuote = false;
          } else if (/^[-+]/.test(word)) {
            queryPlus += " " + word; // do not add + to - or + already there
          } else {
            queryPlus += " +" + word;
          }
        });
        queryString = queryPlus.trim();
      }
    }
    // remove stray white space from beginning or end
    queryString = queryString.trim();

    // Show the wait spinner
    document.querySelector(".submitContainer").classList.add("loading");
    document.getElementById("loadingSpinner").style.display = "block";

    var xhr = new XMLHttpRequest();
    var urlPrefix = "/cgi-bin/hubApi";
    var historyUrl = "?q=" + encodeURIComponent(queryString);
    historyUrl += ";browser=" + browserExist;
    historyUrl += ";maxItemsOutput=" + resultLimit;
    if (asmStatus !== "statusAny")	// default is any assembly status
       historyUrl += ";status=" + asmStatus;	// something specific is being requested
    if (refSeqCategory !== "refSeqAny")	// default is any RefSeq category
       historyUrl += ";category=" + refSeqCategory;	// something specific
    if (asmLevel !== "asmLevelAny")	// default is any level of assembly
       historyUrl += ";level=" + asmLevel;	// something specific
    if (debug)
       historyUrl += ";debug=1";
    if (measureTiming)
       historyUrl += ";measureTiming=1";

    var url = "/findGenome" + historyUrl;

    if (debug) {
      var apiUrl = "<a href='" + urlPrefix + url + "' target=_blank>" + url + "</a>";
      document.getElementById("recentAjax").innerHTML = apiUrl;
    }
    stateObject.queryString = queryString;
    var searchOptions = document.getElementById("advancedSearchOptions");
    if (searchOptions.style.display === "flex") {
        stateObject.advancedSearchVisible = true;
        historyUrl += ";advancedSearch=true";
    } else {
        stateObject.advancedSearchVisible = false;
    }
    stateObject.maxItemsOutput = maxItemsOutput;
    stateObject.browser = browserExist;
    stateObject.debug = debug;
    stateObject.measureTiming = measureTiming;
    stateObject.wordMatch = wordMatch;
    stateObject.asmStatus = asmStatus;
    stateObject.refSeqCategory = refSeqCategory;
    stateObject.asmLevel = asmLevel;
    let urlText0 = document.getElementById('urlText0');
    let hostName = window.location.hostname;
    urlText0.innerHTML = "https://" + hostName + "/assemblySearch.html" + historyUrl;

    xhr.open('GET', urlPrefix + url, true);

    xhr.onload = function() {
        if (xhr.status === 200) {
            // Hide the wait spinner once the AJAX request is complete
            document.querySelector(".submitContainer").classList.remove("loading");
            document.getElementById("loadingSpinner").style.display = "none";
            document.getElementById('copyIcon0').innerHTML = stateObject.copyIcon0;
            enableButtons();

            stateObject.jsonData = xhr.responseText;
            history.pushState(stateObject, '', historyUrl);

            var data = JSON.parse(xhr.responseText);
	    populateTableAndInfo(data);
        } else {
            // Hide the wait spinner once the AJAX request is complete
            document.querySelector(".submitContainer").classList.remove("loading");
            document.getElementById("loadingSpinner").style.display = "none";
            document.getElementById('copyIcon0').innerHTML = stateObject.copyIcon0;
            document.getElementById('urlCopyLink').style.display = "none";
            enableButtons();
	    var tableBody = document.getElementById('tableBody');
            tableBody.innerHTML = "<tr><td style='text-align:center;' colspan=8><b>no results found for query: <em>'" + queryString + "'</em></b></td></tr>";
            var metaData = document.getElementById('metaData');
            metaData.innerHTML = '';
//            metaData.innerHTML = "<b>no results found for query: '" + queryString + "'</b>";
            document.getElementById('resultCounts').innerHTML = "";
            document.getElementById('elapsedTime').innerHTML = "0";
        }
    };

    xhr.onerror = function() {
        alert('console.log("Request failed")');
        console.log('Request failed');
    };

    xhr.send();
}
