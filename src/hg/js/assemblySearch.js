document.addEventListener('DOMContentLoaded', function() {
    var searchForm = document.getElementById('searchForm');
    var searchInput = document.getElementById('searchBox');
    var clearButton = document.getElementById('clearSearch');

    clearButton.addEventListener('click', function() {
        searchInput.value = ''; // Clear the search input field
    });

    searchForm.addEventListener('submit', function(event) {
        event.preventDefault(); // Prevent form submission

        var searchTerm = document.getElementById('searchBox').value;
        var resultCountLimit = document.getElementById('limitResultCount');
        var browserExist = document.querySelector('input[name="browserExist"]:checked').value;
        var wordMatch = document.querySelector('input[name="wordMatch"]:checked').value;
        makeRequest(searchTerm, browserExist, resultCountLimit.value, wordMatch);
    });
});

// Function to generate the table and extra information
function populateTableAndInfo(jsonData) {
    var tableBody = document.getElementById('tableBody');
    var metaData = document.getElementById('metaData');
    document.getElementById('searchString').innerHTML = "";
    document.getElementById('matchCounts').innerHTML = "0";
    document.getElementById('availableAssemblies').innerHTML = "0";
    document.getElementById('elapsedTime').innerHTML = "0";

    // Clear existing table content
    tableBody.innerHTML = '';
    metaData.innerHTML = '';

    // Extract the genomic entries and the extra info
    const genomicEntries = {};
    const extraInfo = {};

    for (const key in jsonData) {
        if (jsonData[key].scientificName) {
            genomicEntries[key] = jsonData[key];
        } else {
            extraInfo[key] = jsonData[key];
        }
    }

    var count = 0;
    for (const id in genomicEntries) {
        var dataRow = '<tr>';
        dataRow += "<th>" + ++count + "</th>";
        var urlReference = id;
        if (genomicEntries[id].browserExists) {
          if (id.startsWith("GC")) {
            urlReference = "<a href='/h/" + id + "?position=lastDbPos' target=_blank>" + id + "</a>";
          } else {
            urlReference = "<a href='/cgi-bin/hgTracks?db=" + id + "' target=_blank>" + id + "</a>";
          }
        }
        dataRow += "<th>" + urlReference + "</th>";
        dataRow += "<td>" + genomicEntries[id].scientificName + "</td>";
        dataRow += "<td>" + genomicEntries[id].commonName + "</td>";
        dataRow += "<td>" + genomicEntries[id].clade + "</td>";
        dataRow += "<td>" + genomicEntries[id].description + "</td>";
        dataRow += '</tr>';
        tableBody.innerHTML += dataRow;
    }
    var dataTable = document.getElementById('dataTable');
    sorttable.makeSortable(dataTable);

    document.getElementById('searchString').innerHTML = extraInfo['genomeSearch'];
    document.getElementById('matchCounts').innerHTML = extraInfo['totalMatchCount'].toLocaleString();
    document.getElementById('availableAssemblies').innerHTML = extraInfo['availableAssemblies'].toLocaleString();
    var etMs = extraInfo['elapsedTimeMs'];
    var elapsedTime = etMs.toLocaleString() + " milliseconds";
    if ( etMs > 1000 ) {
       var etSec = etMs/1000;
       elapsedTime = etSec.toFixed(2) + " seconds";
    }
    document.getElementById('elapsedTime').innerHTML = elapsedTime.toLocaleString();
}	//	function populateTableAndInfo(jsonData)

function enableButtons() {
    document.getElementById('submitButton').disabled = false;
    document.getElementById('clearSearch').disabled = false;
}

function disableButtons() {
    document.getElementById('submitButton').disabled = true;
    document.getElementById('clearSearch').disabled = true;
}

function makeRequest(query, browserExist, resultLimit, wordMatch) {
    // Disable the submit button
    disableButtons();
    var queryString = query;
    // for allWords, place + sign in front of each word if not already there
    if (wordMatch === "allWords") {
      const words = query.split(/\s+/);
      if (words.length > 1) {	// not needed on only one word
        var queryPlus = "";	// compose new query string
        words.forEach(word => {
          if (word.startsWith("+")) {
            queryPlus += " " + word;	// space separated each word
          } else {
            queryPlus += " +" + word;
          }
        });
      queryString = queryPlus.trimStart();	// remove first space character
      }
    }

    // Show the wait spinner
    document.querySelector(".submitContainer").classList.add("loading");
    document.getElementById("loadingSpinner").style.display = "block";

    var xhr = new XMLHttpRequest();
    var url = "https://genome-test.gi.ucsc.edu/cgi-bin/hubApi/findGenome?genomeSearch=" + encodeURIComponent(queryString);
    url += ";browser=" + browserExist;
    url += ";maxItemsOutput=" + resultLimit;

    xhr.open('GET', url, true);

    xhr.onload = function() {
        if (xhr.status === 200) {
            // Hide the wait spinner once the AJAX request is complete
            document.querySelector(".submitContainer").classList.remove("loading");
            document.getElementById("loadingSpinner").style.display = "none";
            enableButtons();
            var data = JSON.parse(xhr.responseText);
	    populateTableAndInfo(data);
        } else {
            // Hide the wait spinner once the AJAX request is complete
            document.querySelector(".submitContainer").classList.remove("loading");
            document.getElementById("loadingSpinner").style.display = "none";
            enableButtons();
	    var tableBody = document.getElementById('tableBody');
            var metaData = document.getElementById('metaData');
            tableBody.innerHTML = '';
            metaData.innerHTML = '';
            metaData.innerHTML = "<b>no results found for query: '" + queryString + "'</b>";
            document.getElementById('searchString').innerHTML = queryString;
            document.getElementById('matchCounts').innerHTML = "0";
            document.getElementById('availableAssemblies').innerHTML = "0";
            document.getElementById('elapsedTime').innerHTML = "0";
        }
    };

    xhr.onerror = function() {
        alert('console.log("Request failed")');
        console.log('Request failed');
    };

    xhr.send();
}
