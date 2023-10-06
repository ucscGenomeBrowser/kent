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
        input: null, // the <input> element for picking files from the users machine
        pickedList: null, // the <div> for displaying files in toUpload
        pendingQueue: [], // our queue of pending [xmlhttprequests, file], kind of like the toUpload object
    };

    // We can use XMLHttpRequest if necessary or a mirror can't use tus
    var useTus = tus.isSupported && typeof tusdPort !== 'undefined' && typeof tusdBasePath !== 'undefined';

    function getTusdEndpoint() {
        // return the port and basepath of the tusd server
        // NOTE: the port and basepath are specified in hg.conf
        let currUrl = parseUrl(window.location.href);
        return "https://hgwdev-chmalee.gi.ucsc.edu" + ":" + tusdPort + "/" + tusdBasePath + "/";
    }

    function togglePickStateMessage(showMsg = false) {
        if (showMsg) {
            let para = document.createElement("p");
            para.textContent = "No files selected for upload";
            para.classList.add("noFiles");
            uiState.pickedList.prepend(para);
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

    function requestsPending() {
        /* Return true if requests are still pending, which means it needs to
         * have been sent(). aborted requests are considered done for this purpose */
        for (let [req, f] of uiState.pendingQueue) {
            if (req.readyState != XMLHttpRequest.DONE && req.readyState != XMLHttpRequest.UNSENT) {
                return true;
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
                let [q, f] = uiState.pendingQueue.pop();
                // we only need to abort requests that haven't finished yet
                if (q.readyState != XMLHttpRequest.DONE) {
                    q.abort();
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


    function sendFile(file) {
        // this function can go away once tus is implemented
        // this is mostly adapted from the mdn example:
        // https://developer.mozilla.org/en-US/docs/Web/API/File_API/Using_files_from_web_applications#example_uploading_a_user-selected_file
        // we will still need all the event handlers though
        const xhr = new XMLHttpRequest();
        this.progMeter = makeNewProgMeter(file.name);
        this.xhr = xhr;
        const endpoint = "../cgi-bin/hgHubConnect";
        const self = this; // why do we need this?
        const fd = new FormData();

        this.xhr.upload.addEventListener("progress", (e) => {
            if (e.lengthComputable) {
                const pct = Math.round(e.loaded * 100) / e.total;
                self.progMeter.updateProgress(pct);
            }
        }, false);

        // loadend handles abort/error or load events
        this.xhr.upload.addEventListener("loadend", (e) => {
            this.progMeter.updateProgress();
            const canvas = self.progMeter.ctx.canvas;
            canvas.parentNode.removeChild(canvas);
            delete uiState.toUpload[file.name];
            let li = liForFile(file);
            li.parentNode.removeChild(li);
            if (Object.keys(uiState.toUpload).length === 0) {
                removeCancelAllButton();
            }
        }, false);

        // for now just treat it like an abort/error
        this.xhr.upload.addEventListener("timeout", (e) => {
            progMeter.updateProgress();
            const canvas = self.progMeter.ctx.canvas;
            canvas.parentNode.removeChild(canvas);
        });

        this.xhr.upload.addEventListener("load", (e) => {
            // TODO:  on load populate the uploaded files side
            let uploadSection = document.getElementById("uploadedFilesSection");
            if (uploadSection.style.display === "none") {
                uploadSection.style.display = "";
            }
        });

        // on error keep the file name present and show the error somehow
        this.xhr.upload.addEventListener("error", (e) => {
        });
        this.xhr.upload.addEventListener("abort", (e) => {
            console.log("request aborted");
        });
        this.xhr.upload.addEventListener("timeout", (e) => {
        });

        // finally we can send the request
        this.xhr.open("POST", endpoint, true);
        fd.set("createHub", 1);
        fd.set("userFile", file);
        this.xhr.send(fd);
        uiState.pendingQueue.push([this.xhr,file]);

        addCancelButton(file, this.xhr);
    }

    function submitPickedFiles() {
        let tusdServer = getTusdEndpoint();
        for (let f in uiState.toUpload) {
            file = uiState.toUpload[f];
            if (useTus) {
                let tusOptions = {
                    endpoint: tusdServer,
                    metadata: {
                        filename: file.name,
                        fileType: file.type,
                        fileSize: file.size
                    }
                };
                // TODO: get the uploadUrl from the tusd server
                // use a pre-create hook to validate the user
                // and get an uploadUrl
                let tusUpload = new tus.Upload(file, tusOptions);
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
        uiState.input.value = "";
        uiState.toUpload = {};
        togglePickStateMessage(true);
    }

    function addClearSubmitButtons() {
        let firstBtn = document.getElementById("btnForInput");
        let btnRow = document.getElementById("chooseAndSendFilesRow");
        if (!document.getElementById("clearPicked")) {
            let newLabel = document.createElement("label");
            newLabel.classList.add("button");
            newLabel.id = "clearPicked";
            newLabel.textContent = "Clear";
            newLabel.addEventListener("click", clearPickedFiles);
            btnRow.append(newLabel);
        }
        if (!document.getElementById("submitPicked")) {
            newLabel = document.createElement("label");
            newLabel.id = "submitPicked";
            newLabel.classList.add("button");
            newLabel.textContent = "Submit";
            newLabel.addEventListener("click", submitPickedFiles);
            btnRow.append(newLabel);
        }
    }

    function removeClearSubmitButtons() {
        let btn = document.getElementById("clearPicked");
        btn.parentNode.removeChild(btn);
        btn = document.getElementById("submitPicked");
        btn.parentNode.removeChild(btn);
    }

    function listPickedFiles() {
        if (uiState.input.files.length === 0) {
            togglePickStateMessage(true);
            removeClearSubmitButtons();
        } else {
            let displayList = document.createElement("ul");
            displayList.classList.add("pickedFiles");
            uiState.pickedList.appendChild(displayList);
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
        let input = document.getElementById("uploadedFiles");
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
        if (input !== null) {
            uiState.input = input;
            uiState.input.style.float = "right";
            uiState.input.style.opacity = 0;
            uiState.input.value = "";
        } else {
            let parent = document.getElementById("chooseAndSendFilesRow");
            input = document.createElement("input");
            input.style.opacity = 0;
            input.style.float = "right";
            input.multiple = true;
            input.id = "uploadedFiles";
            input.type = "file";
            uiState.input = input;
            parent.appendChild(input);
        }

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
            input.addEventListener("change", listPickedFiles);
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
