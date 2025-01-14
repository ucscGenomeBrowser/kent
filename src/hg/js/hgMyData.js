/* jshint esnext: true */
var debugCartJson = true;

function prettyFileSize(num) {
    if (!num) {return "0B";}
    if (num < (1024 * 1024)) {
        return `${(num/1024).toFixed(1)}KB`;
    } else if (num < (1024 * 1024 * 1024)) {
        return `${((num/1024)/1024).toFixed(1)}MB`;
    } else {
        return `${(((num/1024)/1024)/1024).toFixed(1)}GB`;
    }
}

function generateApiKey() {
    let apiKeyInstr = document.getElementById("apiKeyInstructions");
    let apiKeyDiv = document.getElementById("apiKey");

    if (!document.getElementById("spinner")) {
        let spinner = document.createElement("i");
        spinner.id = "spinner";
        spinner.classList.add("fa", "fa-spinner", "fa-spin");
        document.getElementById("generateApiKey").after(spinner);
    }

    let handleSuccess = function(reqObj) {
        apiKeyDiv.textContent = reqObj.apiKey;
        apiKeyInstr.style.display = "block";
        let revokeDiv= document.getElementById("revokeDiv");
        revokeDiv.style.display = "block";
        document.getElementById("spinner").remove();

        // remove the word 'already' from the message if we have just re-generated a key
        let refreshSpan = document.getElementById("removeOnGenerate");
        if (refreshSpan) {
            refreshSpan.style.display = "none";
        }
    };

    let cartData = {generateApiKey: {}};
    cart.setCgi("hgHubConnect");
    cart.send(cartData, handleSuccess);
    cart.flush();
}

function revokeApiKeys() {
    let apiKeyInstr = document.getElementById("apiKeyInstructions");
    let apiKeyDiv = document.getElementById("apiKey");

    if (!document.getElementById("spinner")) {
        let spinner = document.createElement("i");
        spinner.id = "spinner";
        spinner.classList.add("fa", "fa-spinner", "fa-spin");
        document.getElementById("revokeApiKeys").after(spinner);
    }

    let handleSuccess = function(req) {
        apiKeyInstr.style.display = "none";
        document.getElementById("spinner").remove();
        let generateDiv = document.getElementById("generateDiv");
        generateDiv.style.display = "block";
        let revokeDiv = document.getElementById("revokeDiv");
        revokeDiv.style.display = "none";
    };

    let cartData = {revokeApiKey: {}};
    cart.setCgi("hgHubConnect");
    cart.send(cartData, handleSuccess);
    cart.flush();
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
                fileName: file.meta.name,
                fileSize: file.size,
                lastModified: file.data.lastModified,
            });
        }
        return doUpload;
    },
});

var hubCreate = (function() {
    let uiState = { // our object for keeping track of the current UI and what to do
        userUrl: "", // the web accesible path where the uploads are stored for this user
    };

    function getTusdEndpoint() {
        // return the port and basepath of the tusd server
        // NOTE: the port and basepath are specified in hg.conf
        //let currUrl = parseUrl(window.location.href);
        return "https://hgwdev-hubspace.gi.ucsc.edu/files";
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

    function viewInGenomeBrowser(fname, ftype, genome, hubName) {
        // redirect to hgTracks with this track open in the hub
        if (typeof uiState.userUrl !== "undefined" && uiState.userUrl.length > 0) {
            if (ftype in extensionMap) {
                // TODO: tusd should return this location in it's response after
                // uploading a file and then we can look it up somehow, the cgi can
                // write the links directly into the html directly for prev uploaded files maybe?
                let url = "../cgi-bin/hgTracks?hgsid=" + getHgsid() + "&db=" + genome + "&hubUrl=" + uiState.userUrl + hubName + "/hub.txt&" + trackHubFixName(fname) + "=pack";
                window.location.assign(url);
                return false;
            }
        }
    }

    const regex = /[^A-Za-z0-9_-]+/g;
    function trackHubFixName(trackName) {
        // replace everything but alphanumeric, underscore and dash with underscore
        return trackName.replaceAll(regex, "_");
    }

    // helper object so we don't need to use an AbortController to update
    // the data this function is using
    let selectedData = {};
    function viewAllInGenomeBrowser(ev) {
        // redirect to hgTracks with these tracks/hubs open
        let data = selectedData;
        if (typeof uiState.userUrl !== "undefined" && uiState.userUrl.length > 0) {
            let url = "../cgi-bin/hgTracks?hgsid=" + getHgsid();
            let genome; // may be multiple genomes in list, just redirect to the first one
                        // TODO: this should probably raise an alert to click through
            let hubsAdded = {};
            _.forEach(data, (d) => {
                if (!genome) {
                    genome = d.genome;
                    url += "&db=" + genome;
                }
                if (d.fileType in extensionMap) {
                    // TODO: tusd should return this location in it's response after
                    // uploading a file and then we can look it up somehow, the cgi can
                    // write the links directly into the html directly for prev uploaded files maybe?
                    if (!(d.parentDir in hubsAdded)) {
                        // NOTE: hubUrls get added regardless of whether they are on this assembly
                        // or not, because multiple genomes may have been requested. If this user
                        // switches to another genome we want this hub to be connected already
                        url += "&hubUrl=" + uiState.userUrl + d.parentDir;
                        if (d.parentDir.endsWith("/")) {
                            url += "hub.txt";
                        } else {
                            url += "/hub.txt";
                        }
                    }
                    hubsAdded[d.parentDir] = true;
                    if (d.genome == genome) {
                        // turn the track on if its for this db
                        url += "&" + trackHubFixName(d.fileName) + "=pack";
                    }
                } else if (d.fileType === "hub.txt") {
                    url += "&hubUrl=" + uiState.userUrl + d.fullPath;
                } 
            });
            window.location.assign(url);
            return false;
        }
    }

    function deleteFileSuccess(jqXhr, textStatus) {
        deleteFileFromTable(jqXhr.deletedList);
        updateSelectedFileDiv(null);
    }

    function deleteFileList(ev) {
        // same as deleteFile() but acts on the selectedData variable
        let data = selectedData;
        let cartData = {deleteFile: {fileList: []}};
        cart.setCgi("hgHubConnect");
        _.forEach(data, (d) => {
            cartData.deleteFile.fileList.push({
                fileName: d.fileName,
                fileType: d.fileType,
                parentDir: d.parentDir,
                genome: d.genome,
                fullPath: d.fullPath,
            });
        });
        cart.send(cartData, deleteFileSuccess);
        cart.flush();
    }

    function updateSelectedFileDiv(data) {
        // update the div that shows how many files are selected
        let numSelected = data !== null ? Object.entries(data).length : 0;
        let infoDiv = document.getElementById("selectedFileInfo");
        let span = document.getElementById("numberSelectedFiles");
        let spanParentDiv = span.parentElement;
        span.textContent = `${numSelected} ${numSelected > 1 ? "files" : "file"}`;
        if (numSelected > 0) {
            // (re) set up the handlers for the selected file info div:
            let viewBtn = document.getElementById("viewSelectedFiles");
            selectedData = data;
            viewBtn.addEventListener("click", viewAllInGenomeBrowser);
            viewBtn.textContent = numSelected === 1 ? "View selected file in Genome Browser" : "View all selected files in Genome Browser";
            let deleteBtn = document.getElementById("deleteSelectedFiles");
            deleteBtn.addEventListener("click", deleteFileList);
            deleteBtn.textContent = numSelected === 1 ? "Delete selected file" : "Delete selected files";
            
        }

        // set the visibility of the placeholder text and info text
        spanParentDiv.style.display = numSelected === 0 ? "none": "block";
        let placeholder = document.getElementById("placeHolderInfo");
        placeholder.style.display = numSelected === 0 ? "block" : "none";
    }

    function handleCheckboxSelect(ev, table, row) {
        // depending on the state of the checkbox, we will be adding information
        // to the div, or removing information. We will also be potentially checking/unchecking
        // all of the checkboxes if the selectAll box was clicked. The data variable
        // will hold all the information we want to keep visible in the info div
        let data = {};

        // get all of the currently selected rows (may be more than just the one that
        // was most recently clicked)
        table.rows({selected: true}).data().each(function(row, ix) {
            data[ix] = row;
        });
        updateSelectedFileDiv(data);
    }

    function dataTablePrintSize(data, type, row, meta) {
        return prettyFileSize(data);
    }

    function dataTablePrintGenome(data, type, row, meta) {
        if (data.startsWith("hub_"))
            return data.split("_").slice(2).join("_");
        return data;
    }

    function dataTablePrintAction(rowData) {
        /* Return a node for rendering the actions column */
        if (rowData.fileType === "dir") {
            let folderIcon = document.createElement("i");
            folderIcon.style.display = "inline-block";
            folderIcon.style.backgroundImage = "url(\"../images/folderC.png\")";
            folderIcon.style.backgroundPosition = "left center";
            folderIcon.style.backgroundRepeat = "no-repeat";
            folderIcon.style.width = "24px";
            folderIcon.style.height = "24px";
            folderIcon.classList.add("folderIcon");
            folderIcon.addEventListener("dblclick", function(e) {
                e.stopPropagation();
                console.log("dblclick");
                let table = $("#filesTable").DataTable();
                let trow = $(e.target).closest("tr");
                let row = table.row(trow);
                if (row.child.isShown()) {
                    row.child.hide();
                } else {
                    row.child.show();
                }
            });
            folderIcon.addEventListener("click", (e) => {
                e.stopPropagation();
                console.log("click");
            });
            return folderIcon;
        } else {
            let container = document.createElement("div");
            // click to view hub.txt or track file in gb:
            let viewBtn = document.createElement("button");
            viewBtn.textContent = "View in Genome Browser";
            viewBtn.type = 'button';
            viewBtn.addEventListener("click", function() {
                viewInGenomeBrowser(rowData.fileName, rowData.fileType, rowData.genome, rowData.parentDir);
            });
            container.appendChild(viewBtn);
            return container;
        }
    }

    function deleteFileFromTable(pathList) {
        // req is an object with properties of an uploaded file, make a new row
        // for it in the filesTable
        let table = $("#filesTable").DataTable();
        let rows = table.rows((idx, data) => pathList.includes(data.fullPath));
        rows.remove().draw();
        let toKeep = (elem) => !pathList.includes(elem.fullPath);
        uiState.fileList = uiState.fileList.filter(toKeep);
        history.replaceState(uiState, "", document.location.href);
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


    // hash of file paths to their objects, starts as userFiles
    let filesHash = {};
    function addNewUploadedFileToTable(req) {
        // req is an object with properties of an uploaded file, make a new row
        // for it in the filesTable
        if (!(req.fullPath in filesHash)) {
            if ($.fn.dataTable.isDataTable("#filesTable")) {
                let table = $("#filesTable").DataTable();
                let rowObj = table.row.add(req);
                rowVis[req.fullPath] = true;
                table.search.fixed("defaultView", function(searchStr, data, rowIx) {
                    return rowVis[data.fileName] || rowVis[data.fullPath];
                }).draw();
                indentActionButton(rowObj);
                let newRow = rowObj.node();
                $(newRow).css('color','red').animate({color: 'black'}, 1500);
            } else {
                showExistingFiles([req]);
            }
            filesHash[req.fullPath] = req;
            uiState.fileList.push(req);
        }
    }

    function doRowSelect(ev, table, indexes) {
        let row = table.row(indexes);
        let rowTr = row.node();
        let rowCheckbox = rowTr.childNodes[0].firstChild;
        if (rowTr.classList.contains("parentRow")) {
            // we need to manually check the children
            table.rows((idx,rowData) => rowData.fullPath.startsWith(row.data().fullPath) && rowData.parentDir === row.data().fileName).every(function(rowIdx, tableLoop, rowLoop) {
                if (ev.type === "select") {
                    this.select();
                } else {
                    this.deselect();
                }
            });
        }
        rowCheckbox.checked = ev.type === "select";
        handleCheckboxSelect(ev, table, rowTr);
    }

    let rowVis = {}; // heirarchy of files and their row visibility
    function makeFileHeirarchy(table) {
        function rowHeirarchy(data) {
            if (rowVis[data.fullPath]) {return;}

            // first check for the top levels which are always open by default
            if (!data.parentDir) {
                rowVis[data.fileName] = true;
                return;
            }

            // get the parent row and check it's status:
            let parentName = data.parentDir[-1] === "/" ? data.parentDir.slice(0,-1) : data.parentDir;
            let parentData = table.row((idx, rowData) => rowData.fileName === parentName).data();
            if (!(parentName in rowVis) && !(parentData.fullPath in rowVis)) {
                // get the data for the parent and recurse "up":
                rowHeirarchy(parentData);
            }
            parentVis = rowVis[parentName] || rowVis[parentData.fullPath];
            rowVis[data.fullPath] = parentVis;
        }

        table.rows().every(function(rowIdx, tableLoop, rowLoop) {
            let data = this.data();
            rowHeirarchy(data);
        });
    }

    function indentActionButton(rowObj) {
        let data = rowObj.data();
        let numIndents = data.parentDir !== "" ? data.fullPath.split('/').length - 1: 0;
        rowObj.node().childNodes[1].style.textIndent = (numIndents * 10) + "px";
    }

    let tableInitOptions = {
        select: {
            items: 'row',
            style: 'multi', // default to a single click is all that's needed
        },
        pageLength: 25,
        scrollY: 600,
        scrollCollapse: true, // when less than scrollY height is needed, make the table shorter
        layout: {
            topStart: {
                buttons: [
                    {
                        text: 'Upload',
                        action: function() {return;},
                        className: 'uploadButton',
                        enabled: false, // disable by default in case user is not logged in
                    },
                ],
                quota: null,
            }
        },
        columnDefs: [
            {
                orderable: false, targets: 0,
                render: DataTable.render.select(),
            },
            {
                orderable: false, targets: 1,
                data: "action", title: "",
                render: function(data, type, row) {
                    if (type === "display") {
                        return dataTablePrintAction(row);
                    }
                    return '';
                }
            },
            {
                targets: 3,
                render: function(data, type, row) {
                    if (type === "display") {
                         dataTablePrintSize(data);
                    }
                    return data;
                }
            },
            {
                targets: 5,
                render: function(data, type, row) {
                    if (type === "display") {
                        return dataTablePrintGenome(data);
                    }
                    return data;
                }
            },
            {
                // The upload time column, not visible but we use it to sort on new uploads
                targets: 8,
                visible: false,
                searchable: false,
                orderable: true,
            },
            {
                targets: 9,
                visible: false,
                searchable: false,
                orderable: true,
            }
        ],
        columns: [
            {data: "", },
            {data: "", },
            {data: "fileName", title: "File name"},
            {data: "fileSize", title: "File size"},
            {data: "fileType", title: "File type"},
            {data: "genome", title: "Genome"},
            {data: "parentDir", title: "Hubs"},
            {data: "lastModified", title: "File Last Modified"},
            {data: "uploadTime", title: "Upload Time", name: "uploadTime"},
            {data: "fullPath", title: "fullPath", name: "fullPath"},
        ],
        order: [{name: "fullPath", dir: "asc"},{name: "uploadTime", dir: "asc"}],
        drawCallback: function(settings) {
            console.log("table draw");
            if (isLoggedIn) {
                settings.api.buttons(0).enable();
            }
        },
        rowCallback: function(row, data, displayNum, displayIndex, dataIndex) {
            // a row can represent one of three things:
            // a 'folder', with no parents, but with children
            // a folder with parents and children (can only come from hubtools
            // a 'file' with no children, but with parentDir
            // we assign the appropriate classes which are used later to
            // collapse/expand and select rows for viewing or deletion
            if (!data.parentDir) {
                row.className = "topLevelRow";
            } else {
                row.className = "childRow";
            }
            if (data.fileType === "dir") {
                row.className += " parentRow";
            }
        },
        initComplete: function(settings, json) {
            console.log("data loaded, hiding directories");
            let table = new $.fn.dataTable.Api(settings);
            makeFileHeirarchy(table);
            table.rows().every(function(rowIdx, rowLoop, tableLoop) {
                indentActionButton(this);
            });
            table.order.fixed({pre: [{name: "fullPath", dir: "asc"}, {name: "uploadTime", dir: "asc"}]});
            // only show the top level and one layer of children by default
            table.search.fixed("defaultView", function(searchStr, data, rowIx) {
                return rowVis[data.fileName] || rowVis[data.fullPath];
            }).draw();
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
        DataTable.feature.register('quota', function(settings, opts) {
            let options = Object.assign({option1: false, option2: false}, opts);
            let container = document.createElement("div");
            if (isLoggedIn) {
                container.textContent = `Using ${prettyFileSize(userQuota)} of ${prettyFileSize(maxQuota)}`;
            }
            return container;
        });
        let table = new DataTable("#filesTable", tableInitOptions);
        table.on("select", function(e, dt, type, indexes) {
            doRowSelect(e, dt, indexes);
        });
        table.on("deselect", function(e, dt, type, indexes) {
            doRowSelect(e, dt, indexes);
        });
        _.each(d, function(f) {
            filesHash[f.fullPath] = f;
        });
        return table;
    }

    function init() {
        cart.setCgi('hgMyData');
        cart.debug(debugCartJson);
        // TODO: write functions for
        //     creating default trackDbs
        //     editing trackDbs
        let fileDiv = document.getElementById('filesDiv');
        // get the state from the history stack if it exists
        if (history.state) {
            uiState = history.state;
        } else if (typeof userFiles !== 'undefined' && Object.keys(userFiles).length > 0) {
            uiState.fileList = userFiles.fileList;
            uiState.hubList = userFiles.hubList;
            uiState.userUrl = userFiles.userUrl;
        }
        // first add the top level directories/files
        let table = showExistingFiles(uiState.fileList);
        // TODO: add event handlers for editing defaults, grouping into hub
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
                this.uppy.on("dashboard:modal-closed", () => {
                    if (this.uppy.getFiles().length < 2) {
                        this.removeBatchSelectsFromDashboard();
                    }
                    let allFiles = this.uppy.getFiles();
                    let completeFiles = this.uppy.getFiles().filter((f) => f.progress.uploadComplete === true);
                    if (allFiles.length === completeFiles.length) {
                        this.uppy.clear();
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
            restricted: {requiredMetaFields: ["genome"]},
            closeModalOnClickOutside: true,
            closeAfterFinish: true,
            theme: 'auto',
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
            doneButtonHandler: function() {
                uppy.clear();
            },
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
            const now = new Date(Date.now());
            newReqObj = {
                "fileName": metadata.fileName,
                "fileSize": metadata.fileSize,
                "fileType": metadata.fileType,
                "genome": metadata.genome,
                "parentDir": metadata.parentDir,
                "lastModified": d.toLocaleString(),
                "uploadTime": now.toLocaleString(),
                "fullPath": metadata.parentDir + "/" + metadata.fileName,
            };
            // from what I can tell, any response we would create in the pre-finish hook
            // is completely ignored for some reason, so we have to fake the other files
            // we would have created with this one file and add them to the table if they
            // weren't already there:
            hubTxtObj = {
                "uploadTime": now.toLocaleString(),
                "lastModified": d.toLocaleString(),
                "fileName": "hub.txt",
                "fileSize": 0,
                "fileType": "hub.txt",
                "genome": metadata.genome,
                "parentDir": metadata.parentDir,
                "fullPath": metadata.parentDir + "/hub.txt",
            };
            parentDirObj = {
                "uploadTime": now.toLocaleString(),
                "lastModified": d.toLocaleString(),
                "fileName": metadata.parentDir,
                "fileSize": 0,
                "fileType": "dir",
                "genome": metadata.genome,
                "parentDir": "",
                "fullPath": metadata.parentDir + "/",
            };
            addNewUploadedFileToTable(parentDirObj);
            addNewUploadedFileToTable(hubTxtObj);
            addNewUploadedFileToTable(newReqObj);
        });
        uppy.on('complete', (result) => {
            history.replaceState(uiState, "", document.location.href);
            console.log("replace history with uiState");
        });
    }
    return { init: init,
             uiState: uiState,
           };
}());
