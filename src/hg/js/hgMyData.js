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

const fileNameRegex = /[0-9a-zA-Z._\-+]+/g; // allowed characters in file names
const parentDirRegex = /[0-9a-zA-Z._\-+ ]+/g; // allowed characters in hub names, spaces allowed
// make our Uppy instance:
const uppy = new Uppy.Uppy({
    debug: true,
    allowMultipleUploadBatches: false,
    onBeforeUpload: (files) => {
        // set all the fileTypes and genomes from their selects
        let doUpload = true;
        let thisQuota = 0;
        for (let [key, file] of Object.entries(files)) {
            let fileNameMatch = file.meta.name.match(fileNameRegex);
            let parentDirMatch = file.meta.parentDir.match(parentDirRegex);
            if (!fileNameMatch || fileNameMatch[0] !== file.meta.name) {
                uppy.info(`Error: File name has special characters, please rename file: ${file.meta.name} to only include alpha-numeric characters, period, dash, underscore or plus.`, 'error', 2000);
                doUpload = false;
                continue;
            }
            if (!parentDirMatch || parentDirMatch[0] !== file.meta.parentDir) {
                uppy.info(`Error: Hub name has special characters, please rename file: ${file.meta.parentDir} to only include alpha-numeric characters, period, dash, underscore, plus or space.`, 'error', 2000);
                doUpload = false;
                continue;
            }
            if (!file.meta.genome) {
                uppy.info(`Error: No genome selected for file ${file.meta.name}!`, 'error', 2000);
                doUpload = false;
                continue;
            } else if  (!file.meta.fileType) {
                uppy.info(`Error: File type not supported, file: ${file.meta.name}!`, 'error', 2000);
                doUpload = false;
                continue;
            }
            uppy.setFileMeta(file.id, {
                fileName: file.meta.name,
                fileSize: file.size,
                lastModified: file.data.lastModified,
            });
            thisQuota += file.size;
        }
        if (thisQuota + hubCreate.uiState.userQuota > hubCreate.uiState.maxQuota) {
            uppy.info(`Error: this file batch exceeds your quota. Please delete some files to make space or email genome-www@soe.ucsc.edu if you feel you need more space.`);
            doUpload = false;
        }
        return doUpload;
    },
});

var hubCreate = (function() {
    let uiState = { // our object for keeping track of the current UI and what to do
        userUrl: "", // the web accesible path where the uploads are stored for this user
        hubNameDefault: "",
        isLoggedIn: "",
        maxQuota: 0,
        userQuota: 0,
        userFiles: {},
    };

    function getTusdEndpoint() {
        // this variable is set by hgHubConnect and comes from hg.conf value
        return tusdEndpoint;
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
        "hub.txt": ["hub.txt"],
        "text": [".txt", ".text"],
    };

    function detectFileType(fileName) {
        let fileLower = fileName.toLowerCase();
        for (let fileType in extensionMap) {
            for (let ext of extensionMap[fileType]) {
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

    const regex = /[^A-Za-z0-9_-]/g;
    function trackHubFixName(trackName) {
        // replace everything but alphanumeric, underscore and dash with underscore
        return encodeURIComponent(trackName).replaceAll(regex, "_");
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

    function createOneCrumb(table, dirName, dirFullPath) {
        // make a new span that can be clicked to nav through the table
        let newSpan = document.createElement("span");
        newSpan.id = dirName;
        newSpan.textContent = dirName;
        newSpan.classList.add("breadcrumb");
        newSpan.addEventListener("click", function(e) {
            dataTableShowDir(table, dirName, dirFullPath);
            dataTableCustomOrder(table, {"fullPath": dirFullPath});
            table.draw();
        });
        return newSpan;
    }

    function dataTableEmptyBreadcrumb(table) {
        let currBreadcrumb = document.getElementById("breadcrumb");
        currBreadcrumb.replaceChildren(currBreadcrumb.firstChild);
        currBreadcrumb.firstChild.style.cursor = "unset";
        currBreadcrumb.firstChild.style.textDecoration = "unset";
    }

    function dataTableCreateBreadcrumb(table, dirName, dirFullPath) {
        // Re-create the breadcrumb nav to move back through directories
        let currBreadcrumb = document.getElementById("breadcrumb");
        // empty the node but leave the first "My Data" span
        if (currBreadcrumb.children.length > 1) {
            currBreadcrumb.replaceChildren(currBreadcrumb.firstChild);
        }
        currBreadcrumb.firstChild.style.cursor = "pointer";
        currBreadcrumb.firstChild.style.textDecoration = "underline";
        let components = dirFullPath.split("/");
        components.forEach(function(dirName, dirNameIx) {
            if (!dirName) {
                return;
            }
            let path = components.slice(0, dirNameIx+1);
            componentFullPath = path.join('/');
            componentFullPath += "/"; // need a final '/' on there
            let newSpan = createOneCrumb(table, dirName, componentFullPath);
            currBreadcrumb.appendChild(document.createTextNode(" > "));
            currBreadcrumb.appendChild(newSpan);
        });
    }

    // search related functions:
    function clearSearch(table) {
        // clear any fixed searches so we can apply a new one
        let currSearches = table.search.fixed().toArray();
        currSearches.forEach((name) => table.search.fixed(name, null));
    }

    function dataTableShowTopLevel(table) {
        // show all the "root" files, which are files (probably mostly directories)
        // with no parentDir
        clearSearch(table);
        table.search.fixed("showRoot", function(searchStr, rowData, rowIx) {
            return !rowData.parentDir;
        });
    }

    function dataTableShowDir(table, dirName, dirFullPath) {
        // show the directory and all immediate children of the directory
        clearSearch(table);
        table.search.fixed("oneHub", function(searchStr, rowData, rowIx) {
            return rowData.parentDir === dirName ||  rowData.fullPath === dirFullPath;
        });
        dataTableCreateBreadcrumb(table, dirName, dirFullPath);
    }

    // when we move into a new directory, we remove the row from the table
    // and add it's html into the header, keep the row object around so
    // we can add it back in later
    let oldRowData = null;
    function dataTableCustomOrder(table, dirData) {
        // figure out the order the rows of the table should be in
        // if dirData is null, sort on  uploadTime first
        // if dirData exists, that is the first row, followed by everything else
        // in uploadTime order
        if (!dirData) {
            // make sure the old row can show up again in the table
            let thead = document.querySelector(".dt-scroll-headInner > table:nth-child(1) > thead:nth-child(1)");
            if (thead.childNodes.length > 1) {
                let old = thead.removeChild(thead.lastChild);
                if (oldRowData) {
                    table.row.add(oldRowData);
                    oldRowData = null;
                }
            }
            table.order([{name: "uploadTime", dir: "desc"}]);
        } else {
            // move the dirName row into the header, then the other files can
            // sort normally
            let row = table.row((idx,data) => data.fullPath === dirData.fullPath);
            let rowNode = row.node();
            if (oldRowData) {
                // restore the previous row, which will be not displayed by the search anyways:
                table.row.add(oldRowData);
                oldRowData = null;
            }
            if (!rowNode) {
                // if we are using the breadcrumb to jump back 2 directories, we won't
                // have a rowNode because the row will not have been rendered yet
                table.draw();
                rowNode = row.node();
            }
            oldRowData = row.data();
            // put the data in the header:
            let rowClone = rowNode.cloneNode(true);
            // match the background color of the normal rows:
            rowNode.style.backgroundColor = "#f9f9f9";
            let thead = document.querySelector(".dt-scroll-headInner > table:nth-child(1) > thead:nth-child(1)");
            if (thead.childNodes.length === 1) {
                thead.appendChild(rowClone);
            } else {
                thead.replaceChild(rowClone, thead.lastChild);
            }
            // remove the row
            row.remove();
            // now do a regular order
            table.order([{name: "uploadTime", dir: "desc"}]);
        }
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
            folderIcon.addEventListener("click", function(e) {
                e.stopPropagation();
                console.log("folder click");
                let table = $("#filesTable").DataTable();
                let trow = $(e.target).closest("tr");
                let row = table.row(trow);
                dataTableShowDir(table, rowData.fileName, rowData.fullPath);
                dataTableCustomOrder(table, rowData);
                table.draw();
            });
            return folderIcon;
        } else {
            // only offer the button if this is a track file
            if (rowData.fileType !== "hub.txt" && rowData.fileType !== "text" && rowData.fileType in extensionMap) {
                let container = document.createElement("div");
                let viewBtn = document.createElement("button");
                viewBtn.textContent = "View in Genome Browser";
                viewBtn.type = 'button';
                viewBtn.addEventListener("click", function(e) {
                    e.stopPropagation();
                    viewInGenomeBrowser(rowData.fileName, rowData.fileType, rowData.genome, rowData.parentDir);
                });
                container.appendChild(viewBtn);
                return container;
            } else {
                return null;
            }
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


    // hash of file paths to their objects, starts as uiState.userFiles
    let filesHash = {};
    function addNewUploadedHubToTable(hub) {
        // hub is a list of objects representing the file just uploaded, the associated
        // hub.txt, and directory. Make a new row for each in the filesTable, except for
        // maybe the hub directory row and hub.txt which we may have already seen before
        let table = $("#filesTable").DataTable();
        let justUploaded = {}; // hash of contents of hub but keyed by fullPath
        let hubDirData = {}; // the data for the parentDir of the uploaded file
        for (let obj of hub) {
            if (!obj.parentDir) {
                hubDirData = obj;
            }
            let rowObj;
            if (!(obj.fullPath in filesHash)) {
                justUploaded[obj.fullPath] = obj;
                rowObj = table.row.add(obj);
                filesHash[obj.fullPath] = obj;
                uiState.fileList.push(obj);
            }
        }

        // show all the new rows we just added, note the double draw, we need
        // to have the new rows rendered to do the order because the order
        // will copy the actual DOM node
        dataTableShowDir(table, hubDirData.fileName, hubDirData.fullPath);
        table.draw();
        dataTableCustomOrder(table, hubDirData);
        table.draw();
    }

    function doRowSelect(ev, table, indexes) {
        let row = table.row(indexes);
        let rowTr = row.node();
        if (rowTr) {
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
    }

    function indentActionButton(rowTr, rowData) {
        let numIndents = "0px"; //data.parentDir !== "" ? data.fullPath.split('/').length - 1: 0;
        if (rowData.fileType !== "dir") {
            numIndents = "10px";
        }
        rowTr.childNodes[1].style.textIndent = numIndents;
    }

    let tableInitOptions = {
        select: {
            items: 'row',
            style: 'multi+shift', // default to a single click is all that's needed
        },
        pageLength: 25,
        scrollY: 600,
        scrollCollapse: true, // when less than scrollY height is needed, make the table shorter
        deferRender: true, // only draw into the DOM the nodes we need for each page
        orderCellsTop: true, // when viewing a subdirectory, the directory becomes a part of
                             // the header, this option prevents those cells from being used to
                             // sort the table
        layout: {
            top2Start: {
                div: {
                    className: "",
                    id: "breadcrumb",
                    html: "<span id=\"rootBreadcrumb\">My Data</span>",
                }
            },
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
            },
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
                         return dataTablePrintSize(data);
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
                // The upload time column
                targets: 8,
                visible: true,
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
        drawCallback: function(settings) {
            console.log("table draw");
        },
        rowCallback: function(row, data, displayNum, displayIndex, dataIndex) {
            // row is a tr element, data is the td values
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
            indentActionButton(row, data);
        },
        initComplete: function(settings, json) {
            console.log("data loaded, only showing directories");
            let table = new $.fn.dataTable.Api(settings);
            dataTableShowTopLevel(table);
            dataTableCustomOrder(table);
            table.draw();
        }
    };

    function showExistingFiles(d) {
        // Make the DataTable for each file
        // make buttons have the same style as other buttons
        $.fn.dataTable.Buttons.defaults.dom.button.className = 'button';
        tableInitOptions.data = d;
        if (uiState.isLoggedIn) {
            tableInitOptions.language = {emptyTable: "Uploaded files will appear here. Click \"Upload\" to get started"};
        } else {
            tableInitOptions.language = {emptyTable: "You are not logged in, please navigate to \"My Data\" > \"My Sessions\" and log in or create an account to begin uploading files"};
        }
        DataTable.feature.register('quota', function(settings, opts) {
            let options = Object.assign({option1: false, option2: false}, opts);
            let container = document.createElement("div");
            if (uiState.isLoggedIn) {
                container.textContent = `Using ${prettyFileSize(uiState.userQuota)} of ${prettyFileSize(uiState.maxQuota)}`;
            }
            return container;
        });
        let table = new DataTable("#filesTable", tableInitOptions);
        if (uiState.isLoggedIn) {
            table.buttons(".uploadButton").enable();
            document.getElementById("rootBreadcrumb").addEventListener("click", function(e) {
                dataTableShowTopLevel(table);
                dataTableCustomOrder(table);
                dataTableEmptyBreadcrumb(table);
                table.draw();
            });
        } else {
            table.buttons(".uploadButton").disable();
        }
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
        // get the state from the history stack if it exists
        if (typeof uiData !== 'undefined' && typeof uiState.userFiles !== 'undefined') {
            _.assign(uiState, uiData.userFiles);
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
                    batchParentDirInput.value = uiState.hubNameDefault;
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
                    this.uppy.setFileMeta(file.id, {"genome": defaultDb(), "fileType": detectFileType(file.name), "parentDir": uiState.hubNameDefault});
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
            trigger: ".uploadButton",
            showProgressDetails: true,
            note: "The UCSC Genome Browser is not a HIPAA compliant data store. Do not upload patient information or other sensitive data files here, as anyone with the URL can view them.",
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
            // package the three objects together as one "hub" and display it
            let hub = [parentDirObj, hubTxtObj, newReqObj];
            addNewUploadedHubToTable(hub);
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
