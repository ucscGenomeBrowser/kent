/* jshint esnext: true */
var debugCartJson = true;
var hubNameDefault = "MyFirstHub";

function prettyFileSize(num) {
    if (!num) {return "n/a";}
    if (num < (1000 * 1024)) {
        return `${(num/1000).toFixed(1)}KB`;
    } else if (num < (1000 * 1000 * 1024)) {
        return `${((num/1000)/1000).toFixed(1)}MB`;
    } else {
        return `${(((num/1000)/1000)/1000).toFixed(1)}GB`;
    }
}

// make our Uppy instance:
const uppy = new Uppy.Uppy({
    debug: true,
    allowMultipleUploadBatches: false,
    onBeforeUpload: (files) => {
        // set all the fileTypes and genomes from their selects
        let doUpload = true;
        for (let [key, file] of Object.entries(files)) {
            if (!file.meta.genome) {
                uppy.info(`Error: No genome selected for file ${file.name}!`, 'error', 2000);
                doUpload = false;
                continue;
            } else if  (!file.meta.fileType) {
                uppy.info(`Error: File type not supported, please rename file: ${file.name}!`, 'error', 2000);
                doUpload = false;
                continue;
            }
            uppy.setFileMeta(file.id, {
                fileName: file.name,
                fileSize: file.size,
                lastModified: file.data.lastModified,
            });
        }
        return doUpload;
    },
});

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
        idStr = `${fileName}#fileName`;
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

    let extensionMap = {
        "bigBed": [".bb", ".bigbed"],
        "bam": [".bam"],
        "vcf": [".vcf"],
        "vcfTabix": [".vcf.gz", "vcf.bgz"],
        "bigWig": [".bw", ".bigwig"],
        "hic": [".hic"],
        "cram": [".cram"],
        "bigBarChart": [".bigbarchart"],
        "bigGenePred": [".bgp", ".biggenepred"],
        "bigMaf": [".bigmaf"],
        "bigInteract": [".biginteract"],
        "bigPsl": [".bigpsl"],
        "bigChain": [".bigchain"],
        "bamIndex": [".bam.bai", ".bai"],
        "tabixIndex": [".vcf.gz.tbi", "vcf.bgz.tbi"],
    };

    function detectFileType(fileName) {
        let fileLower = fileName.toLowerCase();
        for (let fileType in extensionMap) {
            for (let extIx in extensionMap[fileType]) {
                let ext = extensionMap[fileType][extIx];
                if (fileLower.endsWith(ext)) {
                    return fileType;
                }
            }
        }
        //we could alert here but instead just explicitly set the value to null
        //and let the backend reject it instead, forcing the user to rename their
        //file
        //alert(`file extension for ${fileName} not found, please explicitly select it`);
        return null;
    }

    /*
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
            let rowDivs = document.querySelectorAll("[id^='" + req.name+"']");
            let delIcon = rowDivs[0].previousElementSibling;
            rowDivs.forEach((div) => {div.remove();});
            delIcon.remove();

            // if nothing else we can close the dialog
            if (uiState.pendingQueue.length === 0) {
                // first remove the grid headers
                let headerEle = document.querySelectorAll(".fileListHeader");
                headerEle.forEach( (header) => {
                    if (header.style.display !== "none") {
                        header.style.display = "none";
                    }
                });
                //check if we can remove the batch change selects
                if (uiState.pendingQueue.length > 1) {
                    document.querySelectorAll("[id^=batchChangeSelect]").forEach( (select) => {
                        select.remove();
                    });
                }
                $("#filePickerModal").dialog("close");
            }
        };

        let onError = function(metadata, err) {
            console.log("failing metadata:");
            console.log(metadata);
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
                    fileType: detectFileType(file.name),
                    genome: document.getElementById(`${file.name}#genomeInput`).selectedOptions[0].value,
                    lastModified: file.lastModified,
                };
                let tusOptions = {
                    endpoint: tusdServer,
                    metadata: metadata,
                    onProgress: onProgress.bind(progMeter),
                    onBeforeRequest: onBeforeRequest,
                    onSuccess: onSuccess.bind(null, file, metadata),
                    onError: onError.bind(null, metadata),
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
    */

    /*
    function clearPickedFiles() {
        while (uiState.pickedList.firstChild) {
            uiState.pickedList.removeChild(uiState.pickedList.firstChild);
        }
        //uiState.input = createInput();
        //uiState.toUpload = {};
    }
    */

    function defaultFileType(file) {
        return detectFileType(file);
    }

    function defaultDb() {
        return cartDb.split(" ").slice(-1)[0];
    }

    function makeGenomeSelectOptions() {
        // Returns an array of options for genomes
        let ret = [];
        let choices = ["Human hg38", "Human T2T", "Human hg19", "Mouse mm39", "Mouse mm10"];
        let cartChoice = {};
        cartChoice.id = cartDb;
        cartChoice.label = cartDb;
        cartChoice.value = cartDb.split(" ").slice(-1)[0];
        if (cartChoice.value.startsWith("hub_")) {
            cartChoice.label = cartDb.split(" ").slice(0,-1).join(" "); // take off the actual db value
        }
        cartChoice.selected = true;
        ret.push(cartChoice);
        choices.forEach( (e) =>  {
            if (e === cartDb) {return;} // don't print the cart database twice
            let choice = {};
            choice.id = e;
            choice.label = e;
            choice.value = e.split(" ")[1];
            ret.push(choice);
        });
        return ret;
    }

    function makeTypeSelectOptions() {
        let ret = [];
        let autoChoice = {};
        autoChoice.label = "Auto-detect from extension";
        autoChoice.value = "Auto-detect from extension";
        autoChoice.selected = true;
        ret.push(autoChoice);
        let choices = ["bigBed", "bam", "vcf", "vcf (bgzip or gzip compressed)", "bigWig", "hic", "cram", "bigBarChart", "bigGenePred", "bigMaf", "bigInteract", "bigPsl", "bigChain"];
        choices.forEach( (e) =>  {
            let choice = {};
            choice.id = e;
            choice.label = e;
            choice.value = e;
            ret.push(choice);
        });
        return ret;
    }


    function createTypeAndDbDropdown(fileName) {
        typeInp = makeTypeSelect(fileName);
        genomeInp = makeGenomeSelectOptions(fileName);
        return [typeInp, genomeInp];
    }

    /*
    function deletePickedFile(eventInst) {
        // called when the trash icon has been clicked to remove a file
        // the sibling text content is the file name, which we use to
        // find all the other elements to delete
        let trashIconDiv = eventInst.currentTarget;
        fname = trashIconDiv.nextSibling.textContent;
        document.querySelectorAll("[id^='" + fname + "']").forEach( (sib) => {
            sib.remove();
        });
        trashIconDiv.remove();
        delete uiState.toUpload[fname];
        // if there are no file rows left (1 row for the header) in the picker hide the headers:
        let container = document.getElementById("fileList");
        if (getComputedStyle(container).getPropertyValue("grid-template-rows").split(" ").length === 1) {
            let headerEle = document.querySelectorAll(".fileListHeader");
            headerEle.forEach( (header) => {
                if (header.style.display !== "none") {
                    header.style.display = "none";
                }
            });
        }
        // if there is only one file picked hide the batch change selects, we may or may not have
        // hidden the headers yet, so we should have 3 or less rows
        if (getComputedStyle(container).getPropertyValue("grid-template-rows").split(" ").length <= 3) {
            let batchChange = document.querySelectorAll("[id^=batchChangeSelect]");
            batchChange.forEach( (select) => {
                select.remove();
            });
        }
    }
    */

    function listPickedFiles() {
        // displays the users chosen files in a grid:
        if (uiState.input.files.length === 0) {
            console.log("not input");
            return;
        } else {
            let displayList = document.getElementById("fileList");
            let deleteEle = document.createElement("div");
            deleteEle.classList.add("deleteFileIcon");
            deleteEle.innerHTML = "<svg xmlns='http://www.w3.org/2000/svg' height='0.8em' viewBox='0 0 448 512'><!--! Font Awesome Free 6.4.0 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --><path d='M135.2 17.7C140.6 6.8 151.7 0 163.8 0H284.2c12.1 0 23.2 6.8 28.6 17.7L320 32h96c17.7 0 32 14.3 32 32s-14.3 32-32 32H32C14.3 96 0 81.7 0 64S14.3 32 32 32h96l7.2-14.3zM32 128H416V448c0 35.3-28.7 64-64 64H96c-35.3 0-64-28.7-64-64V128zm96 64c-8.8 0-16 7.2-16 16V432c0 8.8 7.2 16 16 16s16-7.2 16-16V208c0-8.8-7.2-16-16-16zm96 0c-8.8 0-16 7.2-16 16V432c0 8.8 7.2 16 16 16s16-7.2 16-16V208c0-8.8-7.2-16-16-16zm96 0c-8.8 0-16 7.2-16 16V432c0 8.8 7.2 16 16 16s16-7.2 16-16V208c0-8.8-7.2-16-16-16z'/></svg>";
            for (let file of uiState.input.files ) {
                if (file.name in uiState.toUpload) {
                    continue;
                }
                // create a list for the user to see
                let nameCell = document.createElement("div");
                nameCell.classList.add("pickedFile");
                nameCell.id = `${file.name}#fileName`;
                nameCell.textContent = `${file.name}`;
                // Add the form controls for this file:
                let [typeCell, dbCell] = createTypeAndDbDropdown(file.name);
                let sizeCell = document.createElement("div");
                sizeCell.classList.add("pickedFile");
                sizeCell.id = `${file.name}#fileSize`;
                sizeCell.textContent = prettyFileSize(file.size);

                newDelIcon = deleteEle.cloneNode(true);
                newDelIcon.addEventListener("click", deletePickedFile);
                displayList.appendChild(newDelIcon);
                displayList.appendChild(nameCell);
                displayList.appendChild(typeCell);
                displayList.appendChild(dbCell);
                displayList.appendChild(sizeCell);

                // finally add it for us
                uiState.toUpload[file.name] = file;
            }
            let headerEle = document.querySelectorAll(".fileListHeader");
            headerEle.forEach( (header) => {
                if (header.style.display !== "block") {
                    header.style.display = "block";
                }
            });
            if (Object.keys(uiState.toUpload).length > 1) {
                // put up inputs to batch change all file inputs and dbs
                let batchType = document.createElement("div");
                batchType = makeTypeSelect("batchChangeSelectType");
                let batchDb = document.createElement("div");
                batchDb = makeGenomeSelectOptions("batchChangeSelectDb");

                // place into the grid in the right spot:
                batchType.classList.add('batchTypeSelect');
                batchDb.classList.add('batchDbSelect');

                // update each files select on change
                batchType.addEventListener("change", function(e) {
                    let newVal = e.currentTarget.selectedOptions[0].value;
                    document.querySelectorAll("[id$=typeInput]").forEach( (i) => {
                        if (i === e.currentTarget) {
                            return;
                        }
                        i.value = newVal;
                    });
                });
                batchDb.addEventListener("change", function(e) {
                    let newVal = e.currentTarget.selectedOptions[0].value;
                    document.querySelectorAll("[id$=genomeInput]").forEach( (i) => {
                        if (i === e.currentTarget) {
                            return;
                        }
                        i.value = newVal;
                    });
                });

                // append to the document
                displayList.appendChild(batchType);
                displayList.appendChild(batchDb);
            }
            document.querySelectorAll(".deleteFileIcon").forEach( (i) => {
            });
        }
        // always clear the input element
        uiState.input = createInput();
    }

    function dataTablePrintSize(data, type, row, meta) {
        return prettyFileSize(data);
    }

    function dataTablePrintGenome(data, type, row, meta) {
        if (data.startsWith("hub_"))
            return data.split("_").slice(2).join("_");
        return data;
    }

    function deleteFileFromTable(fname) {
        // req is an object with properties of an uploaded file, make a new row
        // for it in the filesTable
        let table = $("#filesTable").DataTable();
        let row = table.row((idx, data) => data.fileName === fname);
        row.remove().draw();
    }

    function deleteFile(fname, fileType, hubNameList) {
        // Send an async request to hgHubConnect to delete the file
        // Note that repeated requests, like from a bot, will return 404 as a correct response
        console.log(`sending delete req for ${fname}`);
        cart.setCgi("hgHubConnect");
        // a little complex, but the format is:
        // {commandToCgi: {arg: val, ...}
        // but we make val an object as well, becoming:
        // {commandToCgi: {fileList: [{propertyName1: propertyVal1, ...}, {propertName2: ...}]}}
        cart.send({deleteFile: {fileList: [{fileName: fname, fileType: fileType, hubNameList: hubNameList}]}});
        cart.flush();
        deleteFileFromTable(fname);
    }

    function deleteFileList() {
    }

    function addFileToHub(rowData) {
        // a file has been uploaded and a hub has been created, present a modal
        // to choose which hub to associate this track to
        // backend wise: move the file into the hub directory
        //               update the hubSpace row with the hub name
        // frontend wise: move the file row into a 'child' of the hub row
        console.log(`sending addToHub req for ${rowData.fileName} to `);
        cart.setCgi("hgHubConnect");
        cart.send({addToHub: {hubName: "", dataFile: ""}});
        cart.flush();
    }

    function viewInGenomeBrowser(fname, ftype, genome, hubName) {
        // redirect to hgTracks with this track open in the hub
        if (typeof uiState.userUrl !== "undefined" && uiState.userUrl.length > 0) {
            if (ftype in extensionMap) {
                // TODO: tusd should return this location in it's response after
                // uploading a file and then we can look it up somehow, the cgi can
                // write the links directly into the html directly for prev uploaded files maybe?
                let url = "../cgi-bin/hgTracks?hgsid=" + getHgsid() + "&db=" + genome + "&hubUrl=" + uiState.userUrl + hubName + "/hub.txt";
                window.location.assign(url);
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
            let newRow = table.row.add(req).order([8, 'asc']).draw().node();
            $(newRow).css('color','red').animate({color: 'black'}, 1000);
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

    /*
    function startUploadDialog() {
        // put up a dialog to walk a user through uploading data files and setting up a track hub
        console.log("create a hub button clicked!");
        hubUploadButtons = {
            "Start": function() {
                submitPickedFiles();
                let currBtns = $("#filePickerModal").dialog("option", "buttons");
                // add a cancel button to stop current uploads
                if (!("Cancel" in currBtns)) {
                    currBtns.Cancel = function() {
                        clearPickedFiles();
                        $(this).dialog("close");
                    };
                    $("#filePickerModal").dialog("option", "buttons", currBtns);
                }
            },
            "Cancel": function() {
                clearPickedFiles();
                $(this).dialog("close");
            },
            "Close": function() {
                // delete everything that isn't the headers, which we set to hide:
                let fileList = document.getElementById("fileList");
                let headers = document.querySelectorAll(".fileListHeader");
                fileList.replaceChildren(...headers);
                headers.forEach( (header) => {
                    header.style.display = "none";
                });
                uiState.input = createInput();
                $(this).dialog("close");
            }
        };
        $("#filePickerModal").dialog({
            modal: true,
            //buttons: hubUploadButtons,
            minWidth: $("#filePickerModal").width(),
            width: (window.innerWidth * 0.8),
            height: (window.innerHeight * 0.55),
            title: "Upload track data",
            open: function(e, ui) {
                $(e.target).parent().css("position", "fixed");
                $(e.target).parent().css("top", "10%");
            },
        });
        $("#filePickerModal").dialog("open");
    }
    */

    let tableInitOptions = {
        layout: {
            topStart: {
                buttons: [
                    {
                        text: 'Upload',
                        action: function() {return;},
                        className: 'uploadButton',
                        enabled: false, // disable by default in case user is not logged in
                    },
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
                        deleteFile(row.fileName, row.fileType, row.hub);
                    });

                    // click to view hub/file in gb:
                    let viewBtn = document.createElement("button");
                    viewBtn.textContent = "View in Genome Browser";
                    viewBtn.type = 'button';
                    viewBtn.addEventListener("click", function() {
                        viewInGenomeBrowser(row.fileName, row.fileType, row.genome, row.hub);
                    });

                    container.appendChild(delBtn);
                    container.appendChild(viewBtn);

                    return container;
                }
            },
            {
                targets: 3,
                render: function(data, type, row) {
                    return dataTablePrintSize(data);
                }
            },
            {
                targets: 5,
                render: function(data, type, row) {
                    return dataTablePrintGenome(data);
                }
            },
            {
                // The upload time column, not visible but we use it to sort on new uploads
                targets: 8,
                visible: false,
                searchable: false
            }
        ],
        columns: [
            {data: "", },
            {data: "", },
            {data: "fileName", title: "File name"},
            {data: "fileSize", title: "File size", render: dataTablePrintSize},
            {data: "fileType", title: "File type"},
            {data: "genome", title: "Genome", render: dataTablePrintGenome},
            {data: "hub", title: "Hubs"},
            {data: "lastModified", title: "File Last Modified"},
            {data: "uploadTime", title: "Upload Time"},
        ],
        order: [[6, 'desc']],
        drawCallback: function(settings) {
            if (isLoggedIn) {
                settings.api.buttons(0).enable();
            }
        }
    };

    function showExistingFiles(d) {
        // Make the DataTable for each file
        // make buttons have the same style as other buttons
        $.fn.dataTable.Buttons.defaults.dom.button.className = 'button';
        tableInitOptions.data = d;
        if (isLoggedIn) {
            tableInitOptions.language = {emptyTable: "Uploaded files will appear here. Click \"Upload\" to get started"};
        } else {
            tableInitOptions.language = {emptyTable: "You are not logged in, please navigate to \"My Data\" > \"My Sessions\" and log in or create an account to begin uploading files"};
        }
        let table = new DataTable("#filesTable", tableInitOptions);
    }

    function showExistingHubs(d) {
        // Add the hubs to the files table
        if (!d) {return;}
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
                pickedFiles.parentNode.appendChild(para);
            }
        } else {
            // TODO: graceful handle of leaving the page and coming back?
        }
        /*
        let parent = document.getElementById("chooseAndSendFilesRow");
        let input = createInput();
        uiState.input = input;
        inputBtn.parentNode.appendChild(input);
        */

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
            let fileDiv = document.getElementById('filesDiv');
            if (typeof userFiles !== 'undefined' && Object.keys(userFiles).length > 0) {
                uiState.fileList = userFiles.fileList;
                uiState.hubList = userFiles.hubList;
                uiState.userUrl = userFiles.userUrl;
            }
            showExistingFiles(uiState.fileList.filter((row) => row.fileType !== "hub"));
            //inputBtn.addEventListener("click", (e) => uiState.input.click());
            // TODO: add event handlers for editing defaults, grouping into hub
            // TODO: display quota somewhere
        }
        $("#newTrackHubDialog").dialog({
            modal: true,
            autoOpen: false,
            title: "Create new track hub",
            closeOnEscape: true,
            minWidth: 400,
            minHeight: 120
        });
        // create a custom uppy plugin to batch change the type and db fields
        class BatchChangePlugin extends Uppy.BasePlugin {
            constructor(uppy, opts) {
                super(uppy, opts);
                this.id = "BatchChangePlugin";
                this.type = "progressindicator";
                this.opts = opts;
            }

            createOptsForSelect(select, opts) {
                opts.forEach( (opt) => {
                    let option = document.createElement("option");
                    option.value = opt.value;
                    option.label = opt.label;
                    option.id = opt.id;
                    option.selected = typeof opt.selected !== 'undefined' ? opt.selected : false;
                    select.appendChild(option);
                });
            }

            addSelectsForFile(file) {
                /* create two selects for the file object, to include the db and type */
                const id = "uppy_" + file.id;
                let fileDiv = document.getElementById(id);
                // this might not exist yet depending on where we are in the render cycle
                if (fileDiv) {
                    let dbSelectId = "db_select_" + file.id;
                    if (!document.getElementById(dbSelectId)) {
                        let dbSelect = document.createElement("select");
                        dbSelect.id = dbSelectId;
                        let dbOpts = makeGenomeSelectOptions();
                        this.createOptsForSelect(dbSelect, dbOpts);
                        fileDiv.appendChild(dbSelect);
                    }
                    /*
                    let typeSelectId = "type_select_" + file.id;
                    if (!document.getElementById(typeSelectId)) {
                        let typeSelect = document.createElement("select");
                        typeSelect.id = typeSelectId;
                        let typeOpts = makeTypeSelectOptions();
                        this.createOptsForSelect(typeSelect, typeOpts);
                        fileDiv.appendChild(typeSelect);
                    }
                    */
                }
            }

            removeBatchSelectsFromDashboard() {
                let batchSelectDiv = document.getElementById("batch-selector-div");
                if (batchSelectDiv) {
                    batchSelectDiv.remove();
                }
            }

            addBatchSelectsToDashboard() {
                if (!document.getElementById("batch-selector-div")) {
                    let batchSelectDiv = document.createElement("div");
                    batchSelectDiv.id = "batch-selector-div";
                    batchSelectDiv.style.display = "grid";
                    batchSelectDiv.style.width = "80%";
                    // the grid syntax is 2 columns, 3 rows
                    batchSelectDiv.style.gridTemplateColumns = "50% 50%";
                    batchSelectDiv.style.gridTemplateRows = "25px 25px 25px";
                    batchSelectDiv.style.margin = "10px auto"; // centers this div
                    if (window.matchMedia("(prefers-color-scheme: dark)").matches) {
                        batchSelectDiv.style.color = "#eaeaea";
                    }

                    // first just explanatory text:
                    let batchSelectText = document.createElement("div");
                    batchSelectText.textContent = "Change options for all files:";
                    // syntax here is rowStart / columnStart / rowEnd / columnEnd
                    batchSelectText.style.gridArea = "1 / 1 / 1 / 2";

                    // the batch change db select
                    let batchDbSelect = document.createElement("select");
                    this.createOptsForSelect(batchDbSelect, makeGenomeSelectOptions());
                    batchDbSelect.id = "batchDbSelect";
                    batchDbSelect.style.gridArea = "2 / 2 / 2 / 2";
                    batchDbSelect.style.margin = "1px 1px auto";
                    let batchDbLabel = document.createElement("label");
                    batchDbLabel.textContent = "Genome";
                    batchDbLabel.for = "batchDbSelect";
                    batchDbLabel.style.gridArea = "2 / 1 / 2 / 1";

                    // the batch change hub name
                    let batchParentDirInput = document.createElement("input");
                    batchParentDirInput.id = "batchParentDir";
                    batchParentDirInput.value = hubNameDefault;
                    batchParentDirInput.style.gridArea = "3 / 2 / 3 / 2";
                    batchParentDirInput.style.margin= "1px 1px auto";
                    let batchParentDirLabel = document.createElement("label");
                    batchParentDirLabel.textContent = "Hub Name";
                    batchParentDirLabel.for = "batchParentDir";
                    batchParentDirLabel.style.gridArea = "3 / 1 / 3 / 1";

                    // add event handlers to change metadata, use an arrow function
                    // because otherwise 'this' keyword will be the element instead of
                    // our class
                    batchDbSelect.addEventListener("change", (ev) => {
                        let files = this.uppy.getFiles();
                        let val = ev.target.value;
                        for (let [key, file] of Object.entries(files)) {
                            this.uppy.setFileMeta(file.id, {genome: val});
                        }
                    });
                    batchParentDirInput.addEventListener("change", (ev) => {
                        let files = this.uppy.getFiles();
                        let val = ev.target.value;
                        for (let [key, file] of Object.entries(files)) {
                            this.uppy.setFileMeta(file.id, {parentDir: val});
                        }
                    });

                    batchSelectDiv.appendChild(batchSelectText);
                    batchSelectDiv.appendChild(batchDbLabel);
                    batchSelectDiv.appendChild(batchDbSelect);
                    batchSelectDiv.appendChild(batchParentDirLabel);
                    batchSelectDiv.appendChild(batchParentDirInput);

                    // append the batch changes to the bottom of the file list, for some reason
                    // I can't append to the actual Dashboard-files, it must be getting emptied
                    // and re-rendered or something
                    let uppyFilesDiv = document.querySelector(".uppy-Dashboard-progressindicators");
                    if (uppyFilesDiv) {
                        uppyFilesDiv.insertBefore(batchSelectDiv, uppyFilesDiv.firstChild);
                    }
                }
            }

            install() {
                this.uppy.on("file-added", (file) => {
                    // add default meta data for genome and fileType
                    console.log("file-added");
                    this.uppy.setFileMeta(file.id, {"genome": defaultDb(), "fileType": defaultFileType(file.name), "parentDir": hubNameDefault});
                    if (this.uppy.getFiles().length > 1) {
                        this.addBatchSelectsToDashboard();
                    } else {
                        // only open the file editor when there is one file
                        const dash = uppy.getPlugin("Dashboard");
                        dash.toggleFileCard(true, file.id);
                    }
                });
                this.uppy.on("file-removed", (file) => {
                    // remove the batch change selects if now <2 files present
                    if (this.uppy.getFiles().length < 2) {
                        this.removeBatchSelectsFromDashboard();
                    }
                });

                this.uppy.on("dashboard:modal-open", () => {
                    // check if there were already files chosen from before:
                    if (this.uppy.getFiles().length > 2) {
                        this.addBatchSelectsToDashboard();
                    }
                    if (this.uppy.getFiles().length < 2) {
                        this.removeBatchSelectsFromDashboard();
                    }
                });
                this.uppy.on("dashboard:modal-close", () => {
                    if (this.uppy.getFiles().length < 2) {
                        this.removeBatchSelectsFromDashboard();
                    }
                });
            }
            uninstall() {
                // not really used because we aren't ever uninstalling the uppy instance
                this.uppy.off("file-added");
            }
        }
        let uppyOptions = {
            //target: "#filePickerModal", // this seems nice but then the jquery css interferes with
                                          // the uppy css
            trigger: ".uploadButton",
            showProgressDetails: true,
            note: "Example text in the note field",
            meta: {"genome": null, "fileType": null},
            metaFields: (file) => {
                const fields = [{
                    id: 'name',
                    name: 'File name',
                    render: ({value, onChange, required, form}, h) => {
                        return h('input',
                            {type: "text",
                            value: value,
                            onChange: e => {
                                onChange(e.target.value);
                                file.meta.fileType = detectFileType(e.target.value);
                            },
                            required: required,
                            form: form,
                            }
                        );
                    },
                },
                {
                    id: 'genome',
                    name: 'Genome',
                    render: ({value, onChange}, h) => {
                        return h('select', {
                            onChange: e => {
                                onChange(e.target.value);
                                file.meta.genome = e.target.value;
                            }
                            },
                            makeGenomeSelectOptions().map( (genomeObj) => {
                                return h('option', {
                                    value: genomeObj.value,
                                    label: genomeObj.label,
                                    selected: file.meta.genome !== null ? genomeObj.value === file.meta.genome : genomeObj.value === defaultDb()
                                });
                            })
                        );
                    },
                },
                {
                    id: 'parentDir',
                    name: 'Hub Name',
                    render: ({value, onChange, required, form}, h) => {
                        return h('input',
                            {type: 'text',
                             value: value,
                             onChange: e => {
                                onChange(e.target.value);
                             },
                             required: required,
                             form: form,
                            }
                        );
                    },
                }];
                return fields;
            },
            restricted: {requiredMetaFields: ["genome"]},
            closeModalOnClickOutside: true,
            closeAfterFinish: true,
            theme: 'auto',
        };
        let tusOptions = {
            endpoint: getTusdEndpoint(),
            withCredentials: true,
            retryDelays: null,
        };
        uppy.use(Uppy.Dashboard, uppyOptions);
        uppy.use(Uppy.Tus, tusOptions);
        uppy.use(BatchChangePlugin, {target: Uppy.Dashboard});
        uppy.on('upload-success', (file, response) => {
            const metadata = file.meta;
            const d = new Date(metadata.lastModified);
            newReqObj = {
                "uploadTime": Date.now(),
                "lastModified": d.toJSON(),
                "fileName": metadata.fileName,
                "fileSize": metadata.fileSize,
                "fileType": metadata.fileType,
                "genome": metadata.genome,
                "parentDir": metadata.parentDir,
            };
            addNewUploadedFileToTable(newReqObj);
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
