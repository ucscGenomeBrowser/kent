/* jshint esnext: true */
debugCartJson = true;

function prettyFileSize(num) {
    if (num < (1000 * 1024)) {
        return `${(num/1000).toFixed(1)}kb`;
    } else if (num < (1000 * 1000 * 1024)) {
        return `${((num/1000)/1000).toFixed(1)}mb`;
    } else {
        return `${(((num/1000)/1000)/1000).toFixed(1)}gb`;
    }
}

var hubCreate = (function() {
    let uiState = { // our object for keeping track of the current UI and what to do
        toUpload: {}, // set of file objects keyed by name
        input: null, // the hidden input element
        pickedList: null, // the <div> for displaying files in toUpload
        pendingQueue: [], // our queue of pending [tus.Upload, file], kind of like the toUpload object
        fileList: [], // the files this user has uploaded, initially populated by the server
                        // on page load, but gets updated as the user uploades/deletes files
    };

    // We can use XMLHttpRequest if necessary or a mirror can't use tus
    var useTus = tus.isSupported && true;

    function getTusdEndpoint() {
        // return the port and basepath of the tusd server
        // NOTE: the port and basepath are specified in hg.conf
        //let currUrl = parseUrl(window.location.href);
        return "https://hgwdev-hubspace.gi.ucsc.edu/files";
    }

    function togglePickStateMessage(showMsg = false) {
        if (showMsg) {
            let para = document.createElement("p");
            para.textContent = "No files selected for upload";
            para.classList.add("noFiles");
            uiState.pickedList.prepend(para);
            removeClearSubmitButtons();
        } else {
            let msg = document.querySelector(".noFiles");
            if (msg) {
                msg.parentNode.removeChild(msg);
            }
        }
    }

    function liForFile(file) {
        let liId = `${file.name}#li`;
        let li = document.getElementById(liId);
        return li;
    }

    function newButton(text) {
        /* Creates a new button with some text as the label */
        let newBtn = document.createElement("label");
        newBtn.classList.add("button");
        newBtn.textContent = text;
        return newBtn;
    }

    function createInput() {
        /* Create a new input element for a file picker */
        let input = document.createElement("input");
        //input.style.opacity = 0;
        //input.style.float = "right";
        input.multiple = true;
        input.type = "file";
        input.id = "hiddenFileInput";
        input.style = "display: none";
        input.addEventListener("change", listPickedFiles);
        return input;
    }

    function requestsPending() {
        /* Return true if requests are still pending, which means it needs to
         * have been sent(). aborted requests are considered done for this purpose */
        for (let [req, f] of uiState.pendingQueue) {
            if (req._req !== null) {
                xreq = req._req._xhr;
                if (xreq.readyState != XMLHttpRequest.DONE && xreq.readyState != XMLHttpRequest.UNSENT) {
                    return true;
                }
            }
        }
        return false;
    }

    function addCancelButton(file, req) {
        /* Add a button that cancels the request req */
        let li = liForFile(file);
        let newBtn = newButton("Cancel upload");
        newBtn.addEventListener("click", (e) => {
            req.abort();
            li.removeChild(newBtn);
            // TODO: make this remove the cancel all button if it's the last pending
            // request
            stillPending = requestsPending();
            if (!stillPending) {
                let btnRow = document.getElementById("chooseAndSendFilesRow");
                cAllBtn = btnRow.lastChild;
                btnRow.removeChild(cAllBtn);
            }
        });
        li.append(newBtn);
    }

    function removeCancelAllButton() {
        let btnRow = document.getElementById("chooseAndSendFilesRow");
        if (btnRow.lastChild.textContent === "Cancel all") {
            btnRow.removeChild(btnRow.lastChild);
        }
        togglePickStateMessage(true);
    }

    function addCancelAllButton() {
        let btnRow = document.getElementById("chooseAndSendFilesRow");
        let newBtn = newButton("Cancel all");
        newBtn.addEventListener("click", (e) => {
            while (uiState.pendingQueue.length > 0) {
                let [req, f] = uiState.pendingQueue.pop();
                // we only need to abort requests that haven't finished yet
                if (req._req !== null) {
                    if (_req._xhr.readyState != XMLHttpRequest.DONE) {
                        req.abort(true);
                    }
                }
                let li = liForFile(f);
                if (li !== null) {
                    // the xhr event handlers should handle this but just in case
                    li.removeChild(li.lastChild);
                }
            }
        });
        btnRow.appendChild(newBtn);
    }

    function makeNewProgMeter(fileName) {
        // create a progress meter for this filename
        const progMeterWidth = 128;
        const progMeterHeight = 12;
        const progMeter = document.createElement("canvas");
        progMeter.classList.add("upload-progress");
        progMeter.setAttribute("width", progMeterWidth);
        progMeter.setAttribute("height", progMeterHeight);
        idStr = `${fileName}#li`;
        ele = document.getElementById(idStr);
        ele.appendChild(progMeter);
        progMeter.ctx = progMeter.getContext('2d');
        progMeter.ctx.fillStyle = 'orange';
        progMeter.updateProgress = (percent) => {
            // update the progress meter for this elem
            if (percent === 100) {
                progMeter.ctx.fillStyle = 'green';
            }
            progMeter.ctx.fillRect(0, 0, (progMeterWidth * percent) / 100, progMeterHeight);
        };
        progMeter.updateProgress(0);
        return progMeter;
    }

    function submitPickedFiles() {
        let tusdServer = getTusdEndpoint();

        let onBeforeRequest = function(req) {
            let xhr = req.getUnderlyingObject(req);
            xhr.withCredentials = true;
        };

        let onSuccess = function(req) {
            // remove the selected file from the input element and the ul list
            // FileList is a read only setting, so we have to make
            // a new one without this req
            delete uiState.toUpload[req.name];
            let i, newReqObj = {};
            for (i = 0; i < uiState.pendingQueue.length; i++) {
                fname = uiState.pendingQueue[i][1].name;
                if (fname === req.name) {
                    // remove the successful tusUpload off
                    uiState.pendingQueue.splice(i, 1);
                }
            }
            // remove the file from the list the user can see
            let li = document.getElementById(req.name+"#li");
            li.parentNode.removeChild(li);
            if (uiState.pendingQueue.length === 0) {
                removeCancelAllButton();
            }
            const d = new Date(req.lastModified);
            newReqObj = {"createTime": d.toJSON(), "name": req.name, "size": req.size};
            addNewUploadedFileToTable(newReqObj);
        };

        let onError = function(err) {
            removeCancelAllButton();
            alert(err.originalResponse._xhr.responseText);
        };

        for (let f in uiState.toUpload) {
            file = uiState.toUpload[f];
            if (useTus) {
                let tusOptions = {
                    endpoint: tusdServer,
                    metadata: {
                        filename: file.name,
                        fileType: file.type,
                        fileSize: file.size
                    },
                    onBeforeRequest: onBeforeRequest,
                    onSuccess: onSuccess.bind(null, file),
                    onError: onError,
                    retryDelays: [1000],
                };
                // TODO: get the uploadUrl from the tusd server
                // use a pre-create hook to validate the user
                // and get an uploadUrl
                let tusUpload = new tus.Upload(file, tusOptions);
                uiState.pendingQueue.push([tusUpload, file]);
                tusUpload.start();
            } else {
                // make a new XMLHttpRequest for each file, if tusd-tusclient not supported
                new sendFile(file);
            }
        }
        addCancelAllButton();
        return;
    }

    function clearPickedFiles() {
        while (uiState.pickedList.firstChild) {
            uiState.pickedList.removeChild(uiState.pickedList.firstChild);
        }
        uiState.input = createInput();
        uiState.toUpload = {};
        togglePickStateMessage(true);
    }

    function addClearSubmitButtons() {
        let firstBtn = document.getElementById("btnForInput");
        let btnRow = document.getElementById("chooseAndSendFilesRow");
        if (!document.getElementById("clearPicked")) {
            let clearBtn = document.createElement("button");
            clearBtn.classList.add("button");
            clearBtn.id = "clearPicked";
            clearBtn.textContent = "Clear";
            clearBtn.addEventListener("click", clearPickedFiles);
            btnRow.append(clearBtn);
        }
        if (!document.getElementById("submitPicked")) {
            submitBtn = document.createElement("button");
            submitBtn.id = "submitPicked";
            submitBtn.classList.add("button");
            submitBtn.textContent = "Submit";
            submitBtn.addEventListener("click", submitPickedFiles);
            btnRow.append(submitBtn);
        }
    }

    function removeClearSubmitButtons() {
        let btn = document.getElementById("clearPicked");
        btn.parentNode.removeChild(btn);
        btn = document.getElementById("submitPicked");
        btn.parentNode.removeChild(btn);
    }

    function listPickedFiles() {
        //uiState.input.click(); // let the user choose files:
        if (uiState.input.files.length === 0) {
            console.log("not input");
            return;
            //togglePickStateMessage(true);
            //removeClearSubmitButtons();
        } else {
            let displayList = document.getElementsByClassName("pickedFiles");
            if (displayList.length === 0) {
                displayList = document.createElement("ul");
                displayList.classList.add("pickedFiles");
                uiState.pickedList.appendChild(displayList);
            } else {
                displayList = displayList[0];
            } 
            for (let file of uiState.input.files ) {
                if (file.name in uiState.toUpload) { continue; }
                // create a list for the user to see
                let li = document.createElement("li");
                li.classList.add("pickedFile");
                li.id = `${file.name}#li`;
                li.textContent = `File name: ${file.name}, file size: ${prettyFileSize(file.size)}`;
                displayList.appendChild(li);

                // finally add it for us
                uiState.toUpload[file.name] = file;
            }
            togglePickStateMessage(false);
            addClearSubmitButtons();
        }
        // always clear the input element
        uiState.input = createInput();
    }

    function dataTablePrintSize(data, type, row, meta) {
        return prettyFileSize(data);
    }

    function deleteFileFromTable(rowIx, fname) {
        // req is an object with properties of an uploaded file, make a new row
        // for it in the filesTable
        let table = $("#filesTable").DataTable();
        let row = table.row((idx, data) => data.name === fname);
        row.remove().draw();
    }

    let pendingDeletes = {};
    function deleteFile(rowIx, fname) {
        // Send an async request to hgHubConnect to delete the file
        // Note that repeated requests, like from a bot, will return 404 as a correct response
        console.log(`sending delete req for ${fname}`);
        const endpoint = "../cgi-bin/hgHubConnect?deleteFile=" + fname;
        if (!(endpoint in pendingDeletes)) {
            const xhr = new XMLHttpRequest();
            pendingDeletes[endpoint] = xhr;
            this.xhr = xhr;
            this.xhr.open("DELETE", endpoint, true);
            this.xhr.send();
            deleteFileFromTable(rowIx, fname);
            delete pendingDeletes[endpoint];
        }
    }

    function viewInGenomeBrowser(rowIx, fname) {
        // redirect to hgTracks with this track as a custom track
        bigBedExts = [".bb", ".bigBed"];
        let i;
        for (i = 0; i < bigBedExts.length; i++) {
            if (fname.toLowerCase().endsWith(bigBedExts[i])) {
                // TODO: tusd should return this location in it's response after
                // uploading a file and then we can look it up somehow, the cgi can
                // write the links directly into the html directly for prev uploaded files maybe?
                window.location.assign("../cgi-bin/hgTracks?db=hg38&hgt.customText=" + "/hive/users/chmalee/tmp/userDataDir/4f/chmalee/" + fname);
                return false;
            }
        }
    }

    function addNewUploadedFileToTable(req) {
        // req is an object with properties of an uploaded file, make a new row
        // for it in the filesTable
        let table = null;
        if ($.fn.dataTable.isDataTable("#filesTable")) {
            table = $("#filesTable").DataTable();
            let newRow = table.row.add(req).draw();
        } else {
            showExistingFiles([req]);
        }
    }

    let tableInitOptions = {
        //columnDefs: [{orderable: false, targets: [0,1]}],
        columnDefs: [
            {
                orderable: false, targets: 0,
                title: "<input type=\"checkbox\"></input>",
                render: function(data, type, row) {
                    return "<input type=\"checkbox\"></input>";
                }
            },
            {
                orderable: false, targets: 1,
                data: "action", title: "Action",
                render: function(data, type, row) {
                    // TODO: add event handler on delete button
                    // click to call hgHubDelete file
                    return "<button class='deleteFileBtn'>Delete</button><button class='viewInBtn'>View In GB</button>";
                }
            },
            {
                targets: 3,
                render: function(data, type, row) {
                    return dataTablePrintSize(data);
                }
            }
        ],
        columns: [
            {data: "", },
            {data: "", },
            {data: "name", title: "File name"},
            {data: "size", title: "File size", render: dataTablePrintSize},
            {data: "createTime", title: "Creation Time"},
        ],
        order: [[4, 'desc']],
        drawCallback: function(settings) {
            let btns = document.querySelectorAll('.deleteFileBtn');
            let i;
            for (i = 0; i < btns.length; i++) {
                let fnameNode = btns[i].parentNode.nextElementSibling.childNodes[0];
                if (fnameNode.nodeName !== "#text") {continue;}
                let fname = fnameNode.nodeValue;
                btns[i].addEventListener("click", (e) => {
                    deleteFile(i, fname);
                });
            }
            btns = document.querySelectorAll('.viewInBtn');
            for (i = 0; i < btns.length; i++) {
                let fnameNode = btns[i].parentNode.nextElementSibling.childNodes[0];
                if (fnameNode.nodeName !== "#text") {continue;}
                let fname = fnameNode.nodeValue;
                btns[i].addEventListener("click", (e) => {
                    viewInGenomeBrowser(i, fname);
                });
            }
        },
    };

    function showExistingFiles(d) {
        // Make the DataTable for each file
        //$(document).on("draw.dt", function() {alert("table redrawn");});
        tableInitOptions.data = d;
        let table = $("#filesTable").DataTable(tableInitOptions);
    }

    function checkJsonData(jsonData, callerName) {
        // Return true if jsonData isn't empty and doesn't contain an error;
        // otherwise complain on behalf of caller.
        if (! jsonData) {
            alert(callerName + ': empty response from server');
        } else if (jsonData.error) {
            console.error(jsonData.error);
            alert(callerName + ': error from server: ' + jsonData.error);
        } else if (jsonData.warning) {
            alert("Warning: " + jsonData.warning);
            return true;
        } else {
            if (debugCartJson) {
                console.log('from server:\n', jsonData);
            }
            return true;
        }
        return false;
    }

    function updateStateAndPage(jsonData, doSaveHistory) {
        // Update uiState with new values and update the page.
        _.assign(uiState, jsonData);
        /*
        urlVars = {"db": db, "search": uiState.search, "showSearchResults": ""};
        // changing the url allows the history to be associated to a specific url
        var urlParts = changeUrl(urlVars);
        if (doSaveHistory)
            saveHistory(uiState, urlParts);
        changeSearchResultsLabel();
        */
    }

    function handleRefreshState(jsonData) {
        if (checkJsonData(jsonData, 'handleRefreshState')) {
            updateStateAndPage(jsonData, true);
        }
        $("#spinner").remove();
    }

    function init() {
        cart.setCgi('hgMyData');
        cart.debug(debugCartJson);
        if (!useTus) {
            console.log("tus is not supported, falling back to XMLHttpRequest");
        }
        let pickedFiles = document.getElementById("fileList");
        let inputBtn = document.getElementById("btnForInput");
        if (pickedFiles !== null) {
            // this element should be an empty div upon loading the page
            uiState.pickedList = pickedFiles;
            if (pickedFiles.children.length === 0) {
                let para = document.createElement("p");
                para.textContent = "No files chosen yet";
                para.classList.add("noFiles");
                pickedFiles.appendChild(para);
            }
        } else {
            // TODO: graceful handle of leaving the page and coming back?
        }
        let parent = document.getElementById("chooseAndSendFilesRow");
        let input = createInput();
        uiState.input = input;
        inputBtn.parentNode.appendChild(input);

        if (typeof cartJson !== "undefined") {
            if (typeof cartJson.warning !== "undefined") {
                alert("Warning: " + cartJson.warning);
            }
            var urlParts = {};
            if (debugCartJson) {
                console.log('from server:\n', cartJson);
            }
            _.assign(uiState,cartJson);
            saveHistory(cartJson, urlParts, true);
        } else {
            // no cartJson object means we are coming to the page for the first time:
            //cart.send({ getUiState: {} }, handleRefreshState);
            //cart.flush();
            // TODO: initialize buttons, check if there are already files
            // TODO: write functions for
            //     after picking files
            //     choosing file types
            //     creating default trackDbs
            //     editing trackDbs
            // TODO: make hgHubConnect respond to requests
            // TODO: initialize tus-client
            // TODO: get user name
            // TODO: send a request with username
            // TODO: have tusd respond on server
            let uploadSection = document.getElementById("chosenFilesSection");
            if (uploadSection.style.display === "none") {
                uploadSection.style.display = "";
            }
            if (typeof userFiles !== 'undefined' && typeof userFiles.fileList !== 'undefined' &&
                    userFiles.fileList.length > 0) { 
                uiState.fileList= userFiles.fileList;
                showExistingFiles(uiState.fileList);
            }
            inputBtn.addEventListener("click", (e) => uiState.input.click());
            //uiState.input.addEventListener("change", listPickedFiles);
            // TODO: add event handler for when file is succesful upload
            // TODO: add event handlers for editing defaults, grouping into hub
            // TODO: display quota somewhere
            // TODO: customize the li to remove the picked file
        }
    }
    return { init: init,
             uiState: uiState,
           };

}());


// when a user reaches this page from the back button we can display our saved state
// instead of sending another network request
window.onpopstate = function(event) {
    event.preventDefault();
    hubCreate.updateStateAndPage(event.state, false);
};
