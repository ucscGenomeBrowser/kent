/* jshint esnext: true */
debugCartJson = true;

function prettyFileSize(num) {
    if (!num) {return "n/a";}
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
        hubList: [], // the hubs this user has created/uploaded, initially populated by server
                        // on page load, but gets updated as the user creates/deletes hubs
        userUrl: "", // the web accesible path where the uploads are stored for this user
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
                    if (req._req._xhr.readyState != XMLHttpRequest.DONE) {
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

        let onSuccess = function(req, metadata) {
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
            newReqObj = {
                "createTime": d.toJSON(),
                "fileName": metadata.fileName,
                "fileSize": metadata.fileSize,
                "fileType": metadata.fileType,
                "genome": metadata.genome,
                "hub": ""
            };
            addNewUploadedFileToTable(newReqObj);
        };

        let onError = function(err) {
            removeCancelAllButton();
            if (err.originalResponse !== null) {
                alert(err.originalResponse._xhr.responseText);
            } else {
                alert(err);
            }
        };

        let onProgress = function(bytesSent, bytesTotal) {
            this.updateProgress((bytesSent / bytesTotal) * 100);
        };

        for (let f in uiState.toUpload) {
            file = uiState.toUpload[f];
            if (useTus) {
                let progMeter = makeNewProgMeter(file.name);
                let metadata = {
                    fileName: file.name,
                    fileSize: file.size,
                    fileType: document.getElementById(`${file.name}#typeInput`).selectedOptions[0].value,
                    genome: document.getElementById(`${file.name}#genomeInput`).selectedOptions[0].value,
                    lastModified: file.lastModified,
                };
                let tusOptions = {
                    endpoint: tusdServer,
                    metadata: metadata,
                    onProgress: onProgress.bind(progMeter),
                    onBeforeRequest: onBeforeRequest,
                    onSuccess: onSuccess.bind(null, file, metadata),
                    onError: onError,
                    retryDelays: null,
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

    function makeGenomeSelect(formName, fileName) {
        let genomeInp = document.createElement("select");
        genomeInp.classList.add("genomePicker");
        genomeInp.name = `${fileName}#genomeInput`;
        genomeInp.id = `${fileName}#genomeInput`;
        genomeInp.form = formName;
        let labelChoice = document.createElement("option");
        labelChoice.label = "Choose Genome";
        labelChoice.value = "Choose Genome";
        labelChoice.selected = true;
        labelChoice.disabled = true;
        genomeInp.appendChild(labelChoice);
        let choices = ["Human hg38", "Human T2T", "Human hg19", "Mouse mm39", "Mouse mm10"];
        choices.forEach( (e) =>  {
            let choice = document.createElement("option");
            choice.id = e;
            choice.label = e;
            choice.value = e.split(" ")[1];
            genomeInp.appendChild(choice);
        });
        return genomeInp;
    }

    function makeTypeSelect(formName, fileName) {
        let typeInp = document.createElement("select");
        typeInp.classList.add("typePicker");
        typeInp.name = `${fileName}#typeInput`;
        typeInp.id = `${fileName}#typeInput`;
        typeInp.form = formName;
        let labelChoice = document.createElement("option");
        labelChoice.label = "Choose File Type";
        labelChoice.value = "Choose File Type";
        labelChoice.selected = true;
        labelChoice.disabled = true;
        typeInp.appendChild(labelChoice);
        let choices = ["hub.txt", "bigBed", "bam", "vcf", "bigWig"];
        choices.forEach( (e) =>  {
            let choice = document.createElement("option");
            choice.id = e;
            choice.label = e;
            choice.value = e;
            typeInp.appendChild(choice);
        });
        return typeInp;
    }


    function makeFormControlsForFile(li, formName, fileName) {
        typeInp = makeTypeSelect(formName, fileName);
        genomeInp = makeGenomeSelect(formName, fileName);
        li.append(typeInp);
        li.append(genomeInp);
    }

    function listPickedFiles() {
        // let the user choose files:
        if (uiState.input.files.length === 0) {
            console.log("not input");
            return;
        } else {
            let displayList;
            let displayListForm = document.getElementsByClassName("pickedFilesForm");
            if (displayListForm.length === 0) {
                displayListForm = document.createElement("form");
                displayListForm.id = "displayListForm";
                displayListForm.classList.add("pickedFilesForm");
                displayList = document.createElement("ul");
                displayList.classList.add("pickedFiles");
                displayListForm.appendChild(displayList);
                uiState.pickedList.appendChild(displayListForm);
            } else {
                displayList = displayListForm[0].firstChild;
            } 
            for (let file of uiState.input.files ) {
                if (file.name in uiState.toUpload) { continue; }
                // create a list for the user to see
                let li = document.createElement("li");
                li.classList.add("pickedFile");
                li.id = `${file.name}#li`;
                li.textContent = `File name: ${file.name}, file size: ${prettyFileSize(file.size)}`;
                // Add the form controls for this file:
                makeFormControlsForFile(li, "displayListForm", file.name);
                
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

    function deleteFileFromTable(fname) {
        // req is an object with properties of an uploaded file, make a new row
        // for it in the filesTable
        let table = $("#filesTable").DataTable();
        let row = table.row((idx, data) => data.fileName === fname);
        row.remove().draw();
    }

    function deleteFile(fname, fileType) {
        // Send an async request to hgHubConnect to delete the file
        // Note that repeated requests, like from a bot, will return 404 as a correct response
        console.log(`sending delete req for ${fname}`);
        cart.setCgi("hgHubConnect");
        cart.send({deleteFile: {fileNameList: [fname, fileType]}});
        cart.flush();
        deleteFileFromTable(fname);
    }

    function deleteFileList() {
    }

    function viewInGenomeBrowser(fname, genome) {
        // redirect to hgTracks with this track open in the hub
        if (typeof uiState.userUrl !== "undefined" && uiState.userUrl.length > 0) {
            bigBedExts = [".bb", ".bigBed", ".vcf.gz", ".vcf", ".bam", ".bw", ".bigWig"];
            let i;
            for (i = 0; i < bigBedExts.length; i++) {
                if (fname.toLowerCase().endsWith(bigBedExts[i].toLowerCase())) {
                    // TODO: tusd should return this location in it's response after
                    // uploading a file and then we can look it up somehow, the cgi can
                    // write the links directly into the html directly for prev uploaded files maybe?
                    window.location.assign("../cgi-bin/hgTracks?db=" + genome + "&hgt.customText=" + uiState.userUrl + fname);
                    return false;
                }
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

    function createHubSuccess(jqXhr, textStatus) {
        console.log(jqXhr);
        $("#newTrackHubDialog").dialog("close");
        addNewUploadedFileToTable({
            createTime: jqXhr.creationTime,
            fileType: "hub",
            fileName: jqXhr.hubName,
            genome: jqXhr.db,
            fileSize: null,
            hub: jqXhr.hubName
        });
    }

    function createHub(db, hubName) {
        // send a request to hgHubConnect to create a hub for this user
        cart.setCgi("hgHubConnect");
        cart.send({createHub: {db: db, name: hubName}}, createHubSuccess, null);
        cart.flush();
    }

    function startHubCreate() {
        // put up a dialog to walk a user through setting up a track hub
        console.log("create a hub button clicked!");
        $("#newTrackHubDialog").dialog({
            minWidth: $("#newTrackHubDialog").width(),
        });
        // attach the event handler to save this hub to this users hubspace
        let saveBtn = document.getElementById("doNewCollection");
        saveBtn.addEventListener("click", (e) => {
            let db = document.getElementById("db").value;
            let hubName = document.getElementById("hubName").value;
            // TODO: add a spinner while we wait for the request to complete
            createHub(db, hubName);
        });
        $("#newTrackHubDialog").dialog("open");
    }

    let tableInitOptions = {
        layout: {
            topStart: {
                buttons: [
                    {text: 'Create hub',
                     action: startHubCreate},
                ]
            }
        },
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
                    /* Return a node for rendering the actions column */
                    // all of our actions will be buttons in this div:
                    let container = document.createElement("div");

                    // click to call hgHubDelete file
                    let delBtn = document.createElement("button");
                    delBtn.textContent = "Delete";
                    delBtn.type = 'button';
                    delBtn.addEventListener("click", function() {
                        deleteFile(row.fileName, row.fileType);
                    });

                    // click to view hub/file in gb:
                    let viewBtn = document.createElement("button");
                    viewBtn.textContent = "View in Genome Browser";
                    viewBtn.type = 'button';
                    viewBtn.addEventListener("click", function() {
                        viewInGenomeBrowser(row.fileName, row.genome);
                    });

                    // click to rename file or hub:
                    let renameBtn = document.createElement("button");
                    renameBtn.textContent = "Rename";
                    renameBtn.type = 'button';
                    renameBtn.addEventListener("click", function() {
                        console.log("rename btn clicked!");
                    });

                    // click to associate this track to a hub
                    let addToHubBtn = document.createElement("button");
                    addToHubBtn.textContent = "Add to hub";
                    addToHubBtn.type = 'button';
                    addToHubBtn.addEventListener("click", function() {
                        console.log("add to hub button clicked!");
                    });

                    container.appendChild(delBtn);
                    container.appendChild(viewBtn);
                    container.appendChild(renameBtn);
                    container.appendChild(addToHubBtn);
                    
                    return container;
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
            {data: "fileName", title: "File name"},
            {data: "fileSize", title: "File size", render: dataTablePrintSize},
            {data: "fileType", title: "File type"},
            {data: "genome", title: "Genome"},
            {data: "hub", title: "Hubs"},
            {data: "createTime", title: "Creation Time"},
        ],
        order: [[6, 'desc']],
    };

    function showExistingFiles(d) {
        // Make the DataTable for each file
        // make buttons have the same style as other buttons
        $.fn.dataTable.Buttons.defaults.dom.button.className = 'button';
        tableInitOptions.data = d;
        let table = new DataTable("#filesTable", tableInitOptions);
        let toRemove = document.getElementById("welcomeDiv");
        if (d.length > 0 && toRemove !== null) {
            toRemove.remove();
        }
    }

    function showExistingHubs(d) {
        // Add the hubs to the files table
        let table = $("#filesTable").DataTable();
        d.forEach((hub) => {
            let hubName = hub.hubName;
            let db = hub.genome;
            let data = {
                fileName: hubName,
                fileSize: null,
                fileType: "hub",
                genome: db,
                hub: hubName,
                createTime: null,
            };
            table.row.add(data).draw();
        });
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
            let welcomeDiv = document.createElement("div");
            welcomeDiv.id = "welcomeDiv";
            welcomeDiv.textContent = "Once files are uploaded they will display here. Click \"Choose files\" above or \"Create Hub\" below to get started";
            let fileDiv = document.getElementById('filesDiv');
            fileDiv.insertBefore(welcomeDiv, fileDiv.firstChild);
            if (typeof userFiles !== 'undefined' && (userFiles.fileList.length > 0 || userFiles.hubList.length > 0)) { 
                uiState.fileList = userFiles.fileList;
                uiState.hubList = userFiles.hubList;
                uiState.userUrl = userFiles.userUrl;
            }
            showExistingFiles(uiState.fileList);
            showExistingHubs(uiState.hubList);
            inputBtn.addEventListener("click", (e) => uiState.input.click());
            //uiState.input.addEventListener("change", listPickedFiles);
            // TODO: add event handler for when file is succesful upload
            // TODO: add event handlers for editing defaults, grouping into hub
            // TODO: display quota somewhere
            // TODO: customize the li to remove the picked file
        }
        $("#newTrackHubDialog").dialog({
            modal: true,
            autoOpen: false,
            title: "Create new track hub",
            closeOnEscape: true,
            minWidth: 400,
            minHeight: 120
        });
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
