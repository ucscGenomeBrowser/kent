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

function cgiEncode(value) {
    // copy of cheapgi.c:cgiEncode except we are explicitly leaving '/' characters:
    let splitVal = value.split('/');
    splitVal.forEach((ele, ix) => {
        splitVal[ix] = encodeURIComponent(ele);
    });
    return splitVal.join('/');
}

function cgiDecode(value) {
    // decode an encoded value
    return decodeURIComponent(value);
}

function setDbSelectFromAutocomplete(selectEle, item) {
    // this has been bound to the <select> we are going to add
    // a new child option to
    let newOpt = document.createElement("option");
    newOpt.value = item.genome;
    newOpt.label = item.label;
    newOpt.selected = true;
    selectEle.appendChild(newOpt);
    const event = new Event("change");
    selectEle.dispatchEvent(event);
}

function processFindGenome(result, term) {
    // process the hubApi/findGenome?q= result set into somthing
    // jquery-ui autocomplete can use
    let data = [];
    let apiSkipList = new Set(["downloadTime", "downloadTimeStamp", "availableAssemblies", "browser", "elapsedTimeMs", "itemCount", "q", "totalMatchCount"]);
    _.forEach(result, function(val, key) {
        if (!(apiSkipList.has(key))) {
            let d = {
                "genome": key,
                "label": `${val.commonName} (${key})`,
            };
            if (val.hubUrl !== null) {
                d.category = "UCSC GenArk - bulk annotated assemblies from NCBI GenBank / Refseq";
            } else {
                d.category = "UCSC Genome Browser assemblies - annotation tracks curated by UCSC";
            }
            data.push(d);
        }
    });
    return data;
}

let autocompletes = {};
function initAutocompleteForInput(inpIdStr, selectEle) {
    // we must set up the autocompleteCat for each input created, once per file chosen
    // override the autocompleteCat.js _renderMenu to get the menu on top
    // of the uppy widget.
    // Return true if we actually set up the autocomplete, false if we have already
    // set it up previously
    if ( !(inpIdStr in autocompletes) || autocompletes[inpIdStr] === false) {
        $.widget("custom.autocompleteCat",
                 $.ui.autocomplete,
                 {
                   _renderMenu: function(ul, items) {
                       var that = this;
                       var currentCategory = "";
                       // There's no this._super as shown in the doc, so I can't override
                       // _create as shown in the doc -- just do this every time we render...
                       this.widget().menu("option", "items", "> :not(.ui-autocomplete-category)");
                       $(ul).css("z-index", "99999999");
                       $.each(items,
                              function(index, item) {
                                  // Add a heading each time we see a new category:
                                  if (item.category && item.category !== currentCategory) {
                                      ul.append("<li class='ui-autocomplete-category'>" +
                                                item.category + "</li>" );
                                      currentCategory = item.category;
                                  }
                                  that._renderItem( ul, item );
                              });
                   },
                   _renderItem: function(ul, item) {
                     // In order to use HTML markup in the autocomplete, one has to overwrite
                     // autocomplete's _renderItem method using .html instead of .text.
                     // http://forum.jquery.com/topic/using-html-in-autocomplete
                       // Hits to assembly hub top level (not individial db names) have no item label,
                       // so use the value instead
                       return $("<li></li>")
                           .data("ui-autocomplete-item", item)
                           .append($("<a></a>").html((item.label !== null ? item.label : item.value)))
                           .appendTo(ul);
                   }
                 });
        let selectFunction = setDbSelectFromAutocomplete.bind(null, selectEle);
        autocompleteCat.init($("[id='"+inpIdStr+"']"), {
            baseUrl: "hubApi/findGenome?browser=mustExist&q=",
            //watermark: "Enter species name, common name, etc",
            onSelect: selectFunction,
            onServerReply: processFindGenome,
            enterSelectsIdentical: false
        });
        autocompletes[inpIdStr] = true;
        return true;
    }
    return false;
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
    cart.setCgiAndUrl(fileListEndpoint);
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
    cart.setCgiAndUrl(fileListEndpoint);
    cart.send(cartData, handleSuccess);
    cart.flush();
}

const fileNameRegex = /[0-9a-zA-Z._\-+]+/g; // allowed characters in file names
const parentDirRegex = /[0-9a-zA-Z._\-+]+/g; // allowed characters in hub names

function getTusdEndpoint() {
    // this variable is set by hgHubConnect and comes from hg.conf value
    return tusdEndpoint;
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
                    class: "uppy-u-reset uppy-c-textInput uppy-Dashboard-FileCard-input",
                    onChange: e => {
                        onChange(e.target.value);
                        file.meta.fileType = hubCreate.detectFileType(e.target.value);
                        file.meta.name = e.target.value;
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
                // keep these as a variable so we can init the autocompleteCat
                // code only after the elements have actually been rendered
                // there are multiple rendering passes and only eventually
                // do the elements actually make it into the DOM
                let ret = h('div', {
                        class: "uppy-Dashboard-FileCard-label",
                        style: "display: inline-block; width: 78%"
                        },
                    // first child of div
                    "Select from popular assemblies:",
                    // second div child
                    h('select', {
                        id: `${file.meta.name}DbSelect`,
                        style: "margin-left: 5px",
                        onChange: e => {
                            onChange(e.target.value);
                            file.meta.genome = e.target.value;
                            file.meta.genomeLabel = e.target.selectedOptions[0].label;
                        }
                        },
                        hubCreate.makeGenomeSelectOptions(file.meta.genome, file.meta.genomeLabel).map( (genomeObj) => {
                            return h('option', {
                                value: genomeObj.value,
                                label: genomeObj.label,
                                selected: file.meta.genome !== null ? genomeObj.value === file.meta.genome : genomeObj.value === hubCreate.defaultDb()
                            });
                        })
                    ),
                    h('p', {
                        class: "uppy-Dashboard-FileCard-label",
                        style: "display: block; width: 78%",
                        }, "or search for your genome:"),
                    // third div child
                    h('input', {
                        id: `${file.meta.name}DbInput`,
                        type: 'text',
                        class: "uppy-u-reset uppy-c-textInput uppy-Dashboard-FileCard-input",
                    }),
                    h('input', {
                        id: `${file.meta.name}DbSearchButton`,
                        type: 'button',
                        value: 'search',
                        style: "margin-left: 5px",
                    })
                );
            let selectToChange = document.getElementById(`${file.meta.name}DbSelect`);
            if (selectToChange) {
                let justInitted = initAutocompleteForInput(`${file.meta.name}DbInput`, selectToChange);
                if (justInitted) {
                    // only do this once per file
                    document.getElementById(`${file.meta.name}DbSearchButton`)
                            .addEventListener("click", (e) => {
                                let inp = document.getElementById(`${file.meta.name}DbInput`).value;
                                let selector = `[id='${file.meta.name}DbInput']`;
                                $(selector).autocompleteCat("search", inp);
                            });
                }
            }
            return ret;
            }
        },
        {
            id: 'parentDir',
            name: 'Hub Name',
        }];
        return fields;
    },
    doneButtonHandler: function() {
        uppy.clear();
    },
};

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
                uppy.info(`Error: File name has special characters, please rename file: ${file.meta.name} to only include alpha-numeric characters, period, dash, underscore or plus.`, 'error', 5000);
                doUpload = false;
                continue;
            }
            if (!parentDirMatch || parentDirMatch[0] !== file.meta.parentDir) {
                uppy.info(`Error: Hub name has special characters, please rename hub: ${file.meta.parentDir} for file: ${file.meta.name} to only include alpha-numeric characters, period, dash, underscore, or plus.`, 'error', 5000);
                doUpload = false;
                continue;
            }
            if (!file.meta.genome) {
                uppy.info(`Error: No genome selected for file ${file.meta.name}!`, 'error', 5000);
                doUpload = false;
                continue;
            }
            if  (!file.meta.fileType) {
                uppy.info(`Error: File type not supported, file: ${file.meta.name}!`, 'error', 5000);
                doUpload = false;
                continue;
            }
            // check if this hub already exists and the genome is different from what was
            // just selected, if so, make the user create a new hub
            if (file.meta.parentDir in hubCreate.uiState.filesHash && hubCreate.uiState.filesHash[file.meta.parentDir].genome !== file.meta.genome) {
                genome = hubCreate.uiState.filesHash[file.meta.parentDir].genome;
                uppy.info(`Error: the hub ${file.meta.parentDir} already exists and is for genome "${genome}". Please select the correct genome, a different hub or make a new hub.`);
                doUpload = false;
                continue;
            }
            // check if the user is uploading a hub.txt into a hub that already has a hub.txt
            if (file.meta.parentDir in hubCreate.uiState.filesHash) {
                let hubFiles = hubCreate.uiState.filesHash[file.meta.parentDir].children;
                if (file.meta.fileType === "hub.txt" && hubFiles.filter((f) => f.fileType === "hub.txt").length !== 0) {
                    uppy.info(`Error: the hub definition file (ex: hub.txt) already exists, create a new hub if you want to upload this hub definition file`);
                    doUpload = false;
                    continue;
                }
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
                let dbOpts = hubCreate.makeGenomeSelectOptions();
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
            batchSelectDiv.style.gridTemplateColumns = "max-content minmax(0, 200px) max-content 1fr min-content";
            batchSelectDiv.style.gridTemplateRows = "repest(3, auto)";
            batchSelectDiv.style.margin = "10px auto"; // centers this div
            batchSelectDiv.style.fontSize = "14px";
            batchSelectDiv.style.gap = "8px";
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
            this.createOptsForSelect(batchDbSelect, hubCreate.makeGenomeSelectOptions());
            batchDbSelect.id = "batchDbSelect";
            batchDbSelect.style.gridArea = "2 / 2 / 2 / 2";
            batchDbSelect.style.margin = "2px";
            let batchDbLabel = document.createElement("label");
            batchDbLabel.textContent = "Genome";
            batchDbLabel.for = "batchDbSelect";
            batchDbLabel.style.gridArea = "2 / 1 / 2 / 1";

            // the search bar for db selection
            let batchDbSearchBarLabel= document.createElement("label");
            batchDbSearchBarLabel.textContent = "or search for your genome:";
            batchDbSearchBarLabel.style.gridArea = "2 / 3 /2 / 3";
            batchDbSearchBarLabel.style.margin = "auto";

            let batchDbGenomeSearchBar = document.createElement("input");
            batchDbGenomeSearchBar.classList.add("uppy-u-reset", "uppy-c-textInput");
            batchDbGenomeSearchBar.type = "text";
            batchDbGenomeSearchBar.id = "batchDbSearchBar";
            batchDbGenomeSearchBar.style.gridArea = "2 / 4 / 2 / 4";
            let batchDbGenomeSearchButton = document.createElement("input");
            batchDbGenomeSearchButton.type = "button";
            batchDbGenomeSearchButton.value = "search";
            batchDbGenomeSearchButton.id = "batchDbSearchBarButton";
            batchDbGenomeSearchButton.style.gridArea = "2 / 5 / 2 / 5";

            // the batch change hub name
            let batchParentDirLabel = document.createElement("label");
            batchParentDirLabel.textContent = "Hub Name";
            batchParentDirLabel.for = "batchParentDir";
            batchParentDirLabel.style.gridArea = "3 / 1 / 3 / 1";

            let batchParentDirInput = document.createElement("input");
            batchParentDirInput.id = "batchParentDir";
            batchParentDirInput.value = hubCreate.uiState.hubNameDefault;
            batchParentDirInput.style.gridArea = "3 / 2 / 3 / 2";
            batchParentDirInput.style.margin= "1px 1px auto";
            batchParentDirInput.classList.add("uppy-u-reset", "uppy-c-textInput");

            // add event handlers to change metadata, use an arrow function
            // because otherwise 'this' keyword will be the element instead of
            // our class
            batchDbSelect.addEventListener("change", (ev) => {
                let files = this.uppy.getFiles();
                let val = ev.target.value;
                for (let [key, file] of Object.entries(files)) {
                    this.uppy.setFileMeta(file.id, {genome: val});
                    this.uppy.setFileMeta(file.id, {genomeLabel: ev.target.selectedOptions[0].label});
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
            batchSelectDiv.appendChild(batchDbSearchBarLabel);
            batchSelectDiv.appendChild(batchDbGenomeSearchBar);
            batchSelectDiv.appendChild(batchDbGenomeSearchButton);
            batchSelectDiv.appendChild(batchParentDirLabel);
            batchSelectDiv.appendChild(batchParentDirInput);

            // append the batch changes to the bottom of the file list, for some reason
            // I can't append to the actual Dashboard-files, it must be getting emptied
            // and re-rendered or something
            let uppyFilesDiv = document.querySelector(".uppy-Dashboard-progressindicators");
            if (uppyFilesDiv) {
                uppyFilesDiv.insertBefore(batchSelectDiv, uppyFilesDiv.firstChild);
            }

            // everything has to exist already for autocompleteCat to initialize
            let justInitted = initAutocompleteForInput(batchDbGenomeSearchBar.id, batchDbSelect);
            if (justInitted) {
                // only do this once per batch setup
                batchDbGenomeSearchButton.addEventListener("click", (e) => {
                    let inp = document.getElementById(batchDbSearchBar.id).value;
                    let selector = "[id='"+batchDbGenomeSearchBar.id+"']";
                    $(selector).autocompleteCat("search", inp);
                });
            }
        }
    }

    install() {
        this.uppy.on("file-added", (file) => {
            // add default meta data for genome and fileType
            console.log("file-added");
            this.uppy.setFileMeta(file.id, {"genome": hubCreate.defaultDb(), "fileType": hubCreate.detectFileType(file.name), "parentDir": hubCreate.uiState.hubNameDefault});
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
        this.uppy.on("dashboard:file-edit-start", (file) => {
            autocompletes[`${file.name}DbInput`] = false;
        });

        this.uppy.on("dashboard:file-edit-complete", (file) => {
            // check the filename and hubname metadata and warn the user
            // to edit them if they are wrong. unfortunately I cannot
            // figure out how to force the file card to re-toggle
            // and jump back into the editor from here
            if (file) {
                let fileNameMatch = file.meta.name.match(fileNameRegex);
                let parentDirMatch = file.meta.parentDir.match(parentDirRegex);
                const dash = uppy.getPlugin("Dashboard");
                if (!fileNameMatch || fileNameMatch[0] !== file.meta.name) {
                    uppy.info(`Error: File name has special characters, please rename file: '${file.meta.name}' to only include alpha-numeric characters, period, dash, underscore or plus.`, 'error', 5000);
                }
                if (!parentDirMatch || parentDirMatch[0] !== file.meta.parentDir) {
                    uppy.info(`Error: Hub name has special characters, please rename hub: '${file.meta.parentDir}' to only include alpha-numeric characters, period, dash, underscore, or plus.`, 'error', 5000);
                }
            }
        });
    }
    uninstall() {
        // not really used because we aren't ever uninstalling the uppy instance
        this.uppy.off("file-added");
    }
}

var hubCreate = (function() {
    let uiState = { // our object for keeping track of the current UI and what to do
        userUrl: "", // the web accesible path where the uploads are stored for this user
        hubNameDefault: "",
        isLoggedIn: "",
        maxQuota: 0,
        userQuota: 0,
        userFiles: {}, // same as uiData.userFiles on page load
        filesHash: {}, // for each file, userFiles.fullPath is the key, and then the userFiles.fileList data as the value, with an extra key for the child fullPaths if the file is a directory
    };

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

    let defaultGenomeChoices = {
        "Human hg38": {value: "hg38", label: "Human hg38"},
        "Human T2T": {value: "hs1", label: "Human T2T"},
        "Human hg19": {value: "hg19", label: "Human hg19"},
        "Mouse mm39": {value: "mm39", label: "Mouse mm39"},
        "Mouse mm10": {value: "mm10", label: "Mouse mm10"}
    };

    function makeGenomeSelectOptions(value, label) {
        // Returns an array of options for genomes, if value and label exist, add that
        // as an additional option
        let ret = [];
        let cartChoice = {};
        cartChoice.id = cartDb;
        cartChoice.label = cartDb;
        cartChoice.value = cartDb.split(" ").slice(-1)[0];
        if (cartChoice.value.startsWith("hub_")) {
            cartChoice.label = cartDb.split(" ").slice(0,-1).join(" "); // take off the actual db value
        }
        cartChoice.selected = value && label ? false: true;
        defaultGenomeChoices[cartChoice.label] = cartChoice;

        // next time around our value/label pair will be a default. this time around we
        // want it selected because it was explicitly asked for, but it may not be next time
        ret = Object.values(defaultGenomeChoices);
        if (value && label && !(label in defaultGenomeChoices)) {
            defaultGenomeChoices[label] = {value: value, label: label, selected: true};
        }
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
                let url = "../cgi-bin/hgTracks?hgsid=" + getHgsid() + "&db=" + genome + "&hubUrl=" + uiState.userUrl + cgiEncode(hubName) + "/hub.txt&" + trackHubFixName(fname) + "=pack";
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
                if (d.fileType === "hub.txt") {
                    url += "&hubUrl=" + uiState.userUrl + cgiEncode(d.fullPath);
                }
                else if (d.fileType in extensionMap) {
                    // TODO: tusd should return this location in it's response after
                    // uploading a file and then we can look it up somehow, the cgi can
                    // write the links directly into the html directly for prev uploaded files maybe?
                    if (!(d.parentDir in hubsAdded)) {
                        // NOTE: hubUrls get added regardless of whether they are on this assembly
                        // or not, because multiple genomes may have been requested. If this user
                        // switches to another genome we want this hub to be connected already
                        url += "&hubUrl=" + uiState.userUrl + cgiEncode(d.parentDir);
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
        cart.setCgiAndUrl(fileListEndpoint);
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

    function updateSelectedFileDiv(data, isFolderSelect = false) {
        // update the div that shows how many files are selected
        let numSelected = data !== null ? data.length : 0;
        // if a hub.txt file is in data, disable the delete button
        let disableDelete = false;
        if (data) {
            disableDelete = data.filter((obj) => obj.fileType === "hub.txt").length > 0;
        }
        let infoDiv = document.getElementById("selectedFileInfo");
        let span = document.getElementById("numberSelectedFiles");
        let spanParentDiv = span.parentElement;
        if (numSelected > 0) {
            if (isFolderSelect || span.textContent.endsWith("hub") || span.textContent.endsWith("hubs")) {
                span.textContent = `${numSelected} ${numSelected > 1 ? "hubs" : "hub"}`;
            } else {
                span.textContent = `${numSelected} ${numSelected > 1 ? "files" : "file"}`;
            }
            // (re) set up the handlers for the selected file info div:
            let viewBtn = document.getElementById("viewSelectedFiles");
            viewBtn.addEventListener("click", viewAllInGenomeBrowser);
            viewBtn.textContent = "View selected";
            if (!disableDelete) {
                let deleteBtn = document.getElementById("deleteSelectedFiles");
                deleteBtn.style.display = "inline-block";
                deleteBtn.addEventListener("click", deleteFileList);
                deleteBtn.textContent = "Delete selected";
            } else {
                // delete the old button:
                let deleteBtn = document.getElementById("deleteSelectedFiles");
                deleteBtn.style.display = "none";
            }
        } else {
            span.textContent = "";
        }

        // set the visibility of the placeholder text and info text
        spanParentDiv.style.display = numSelected === 0 ? "none": "block";
        let placeholder = document.getElementById("placeHolderInfo");
        placeholder.style.display = numSelected === 0 ? "block" : "none";
    }

    function handleCheckboxSelect(evtype, table, selectedRow) {
        // depending on the state of the checkbox, we will be adding information
        // to the div, or removing information. We also potentially checked/unchecked
        // all of the checkboxes if the selectAll box was clicked.

        // The data variable will hold all the information we want to keep visible in the info div
        let data = [];
        // The selectedData global holds the actual information needed for the view/delete buttons
        // to work, so data plus any child rows
        selectedData = {};

        // get all of the currently selected rows (may be more than just the one that
        // was most recently clicked)
        table.rows({selected: true}).data().each(function(row, ix) {
            data.push(row);
            selectedData[row.fullPath] = row;
            // add any newly checked rows children to the selectedData structure for the view/delete
            if (row.children) {
                row.children.forEach(function(child) {
                    selectedData[child.fullPath] = child;
                });
            }
        });
        updateSelectedFileDiv(data, selectedRow.data().fileType === "dir");
    }

    function createOneCrumb(table, dirName, dirFullPath, doAddEvent) {
        // make a new span that can be clicked to nav through the table
        let newSpan = document.createElement("span");
        newSpan.id = dirName;
        newSpan.textContent = decodeURIComponent(dirName);
        newSpan.classList.add("breadcrumb");
        if (doAddEvent) {
            newSpan.addEventListener("click", function(e) {
                dataTableShowDir(table, dirName, dirFullPath);
                table.draw();
                dataTableCustomOrder(table, {"fullPath": dirFullPath});
                table.draw();
            });
        } else {
            // can't click the final crumb so don't underline it
            newSpan.style.textDecoration = "unset";
        }
        return newSpan;
    }

    function dataTableEmptyBreadcrumb(table) {
        let currBreadcrumb = document.getElementById("breadcrumb");
        currBreadcrumb.replaceChildren(currBreadcrumb.firstChild);
    }

    function dataTableCreateBreadcrumb(table, dirName, dirFullPath) {
        // Re-create the breadcrumb nav to move back through directories
        let currBreadcrumb = document.getElementById("breadcrumb");
        // empty the node but leave the first "My Data" span
        if (currBreadcrumb.children.length > 1) {
            currBreadcrumb.replaceChildren(currBreadcrumb.firstChild);
        }
        let components = dirFullPath.split("/");
        let numComponents = components.length;
        components.forEach(function(dirName, dirNameIx) {
            if (!dirName) {
                return;
            }
            let doAddEvent = dirNameIx !== (numComponents - 1);
            let path = components.slice(0, dirNameIx+1);
            componentFullPath = path.join('/');
            let newSpan = createOneCrumb(table, dirName, componentFullPath, doAddEvent);
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
        // deselect any selected rows like Finder et al when moving into/upto a directory
        table.rows({selected: true}).deselect();
        table.search.fixed("showRoot", function(searchStr, rowData, rowIx) {
            return !rowData.parentDir;
        });
    }

    function dataTableShowDir(table, dirName, dirFullPath) {
        // show the directory and all immediate children of the directory
        clearSearch(table);
        // deselect any selected rows like Finder et al when moving into/upto a directory
        table.rows({selected: true}).deselect();
        table.draw();
        // NOTE that the below does not actually render until the next table.draw() call
        table.search.fixed("oneHub", function(searchStr, rowData, rowIx) {
            // calculate the fullPath of this rows parentDir in case the dirName passed
            // to this function has the same name as a parentDir further up in the
            // listing. For example, consider a test/test/tmp.txt layout, where "test"
            // is the parentDir of tmp.txt and the test subdirectory
            let parentDirFull = rowData.fullPath.split("/").slice(0,-1).join("/");
            if (rowData.parentDir === dirName && parentDirFull === dirFullPath) {
                return true;
            } else if (rowData.fullPath === dirFullPath) {
                // also return the directory itself
                return true;
            } else {
                return false;
            }
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
                // if we are using the breadcrumb to jump back 2 directories or doing an upload
                // while a subdirectory is opened, we won't have a rowNode because the row will
                // not have been rendered yet. So draw the table with the oldRowData restored
                table.draw();
                // and now we can try again
                row = table.row((idx,data) => data.fullPath === dirData.fullPath);
                rowNode = row.node();
            }
            oldRowData = row.data();
            // put the data in the header:
            let rowClone = rowNode.cloneNode(true);
            // match the background color of the normal rows:
            rowClone.style.backgroundColor = "#fff9d2";
            let thead = document.querySelector(".dt-scroll-headInner > table:nth-child(1) > thead:nth-child(1)");
            // remove the checkbox because it doesn't do anything, and replace it
            // with a back arrow 'button'
            let btn = document.createElement("button");
            btn.id = "backButton";
            $(btn).button({icon: "ui-icon-triangle-1-w"});
            btn.addEventListener("click", (e) => {
                let parentDir = dirData.parentDir;
                let parentDirPath = dirData.fullPath.slice(0,-dirData.fullPath.length);
                if (parentDirPath.length) {
                    dataTableShowDir(table, parentDir, parentDirPath);
                } else {
                    dataTableShowTopLevel(table);
                    dataTableCustomOrder(table);
                    dataTableEmptyBreadcrumb(table);
                }
                table.draw();
            });
            let tdBtn = document.createElement("td");
            tdBtn.appendChild(btn);
            rowClone.replaceChild(tdBtn, rowClone.childNodes[0]);
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

    function parseFileListIntoHash(fileList) {
        // Hash the uiState fileList by the fullPath, and also store the children
        // for each directory
        // first go through and copy all of the data and make the empty
        // children array for each directory
        fileList.forEach(function(fileData) {
            uiState.filesHash[fileData.fullPath] = fileData;
            if (fileData.fileType === "dir") {
                uiState.filesHash[fileData.fullPath].children = [];
            }
        });
        // use a second pass to go through and set the children
        // since we may not have encountered them yet in the above loop
        fileList.forEach(function(fileData) {
            if (fileData.fileType !== "dir" || fileData.parentDir !== "") {
                // compute the key from the fullPath:
                let parts = fileData.fullPath.split("/");
                let keyName = parts.slice(0,-1).join("/");
                if (keyName in uiState.filesHash) {
                    uiState.filesHash[keyName].children.push(fileData);
                }
            }
        });
    }

    function getChildRows(dirFullPath, childRowArray) {
        // Recursively return all of the child rows for a given path
        let childRows = uiState.filesHash[dirFullPath].children;
        childRows.forEach(function(rowData) {
            if (rowData.fileType !== "dir") {
                childRowArray.push(rowData);
            } else {
                childRowArray.concat(getChildRows(rowData.fullPath, childRowArray));
            }
        });
    }

    function dataTablePrintSize(data, type, row, meta) {
        if (row.fileType !== "dir") {
            return prettyFileSize(data);
        } else {
            let childRows = [];
            getChildRows(row.fullPath, childRows);
            let sum = childRows.reduce( (accumulator, currentValue) => {
                return accumulator + currentValue.fileSize;
            }, 0);
            return prettyFileSize(sum);
        }
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
            if (rowData.fileType !== "hub.txt" && rowData.fileType !== "text" && rowData.fileType !== "tabixIndex" && rowData.fileType !== "bamIndex" && rowData.fileType in extensionMap) {
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
        pathList.forEach((f) => {
            updateQuota(-uiState.filesHash[f].fileSize);
        });
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
        cart.setCgiAndUrl(fileListEndpoint);
        cart.send({addToHub: {hubName: "", dataFile: ""}});
        cart.flush();
    }

    function updateQuota(newFileSize) {
        // Change the quota displayed to the user, pass in newFileSize as a negative number
        // when deleting files
        let container = document.getElementById("quotaDiv");
        uiState.userQuota += newFileSize;
        container.textContent = `Using ${prettyFileSize(uiState.userQuota)} of ${prettyFileSize(uiState.maxQuota)}`;
    }

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
            if (!(obj.fullPath in uiState.filesHash)) {
                justUploaded[obj.fullPath] = obj;
                rowObj = table.row.add(obj);
                uiState.fileList.push(obj);
                // NOTE: we don't add the obj to the filesHash until after we're done
                // so we don't need to reparse all files each time we add one
            }
        }

        // show all the new rows we just added, note the double draw, we need
        // to have the new rows rendered to do the order because the order
        // will copy the actual DOM node
        parseFileListIntoHash(uiState.fileList);
        dataTableShowDir(table, hubDirData.fileName, hubDirData.fullPath);
        table.draw();
        dataTableCustomOrder(table, hubDirData);
        table.columns.adjust().draw();
    }

    function doRowSelect(evtype, table, indexes) {
        let selectedRow = table.row(indexes);
        let rowTr = selectedRow.node();
        if (rowTr) {
            handleCheckboxSelect(evtype, table, selectedRow);
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
            selector: 'td:first-child',
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
                    html: "<span id=\"rootBreadcrumb\" class=\"breadcrumb\">My Data</span>",
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
                targets: 2,
                render: function(data, type, row, meta) {
                    return decodeURIComponent(data);
                }
            },
            {
                targets: 3,
                render: function(data, type, row, meta) {
                    if (type === "display") {
                         return dataTablePrintSize(data, type, row, meta);
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
                targets: 6,
                render: function(data, type, row) {
                    if (type === "display") {
                        return cgiDecode(data);
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
        if ($.fn.dataTable.isDataTable("#filesTable")) {
            return $("#filesTable").DataTable();
        }

        $.fn.dataTable.Buttons.defaults.dom.button.className = 'button';
        tableInitOptions.data = d;
        if (uiState.isLoggedIn) {
            tableInitOptions.language = {emptyTable: "Uploaded files will appear here. Click \"Upload\" to get started"};
        } else {
            tableInitOptions.language = {emptyTable: "You are not logged in, please <a href=\"../cgi-bin/hgSession\">log in or create an account</a> to begin uploading files"};
        }
        DataTable.feature.register('quota', function(settings, opts) {
            let options = Object.assign({option1: false, option2: false}, opts);
            let container = document.createElement("div");
            container.id = "quotaDiv";
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
            indexes.forEach(function(i) {
                doRowSelect(e.type, dt, i);
            });
        });
        table.on("deselect", function(e, dt, type, indexes) {
            indexes.forEach(function(i) {
                doRowSelect(e.type, dt, i);
            });
        });
        table.on("click", function(e) {
            if (e.target.className !== "dt-select-checkbox") {
                e.stopPropagation();
                // we've clicked somewhere not on the checkbox itself, we need to:
                // 1. open the directory if the clicked row is a directory
                // 2. select the file if the clicked row is a regular file
                let row = table.row(e.target);
                let data = row.data();
                if (data.children && data.children.length > 0) {
                    dataTableShowDir(table, data.fileName, data.fullPath);
                    dataTableCustomOrder(table, {"fullPath": data.fullPath});
                    table.draw();
                } else {
                    if (row.selected()) {
                        row.deselect();
                        doRowSelect("deselect", table, row.index());
                    } else {
                        row.select();
                        doRowSelect("select", table, row.index());
                    }
                }
            }
        });
        return table;
    }

    function handleGetFileList(jsonData, textStatus) {
        _.assign(uiState, jsonData.userFiles);
        if (uiState.fileList) {
            parseFileListIntoHash(uiState.fileList);
        }

        // first add the top level directories/files
        let table = showExistingFiles(uiState.fileList);
        table.columns.adjust().draw();

        uppy.use(Uppy.Dashboard, uppyOptions);

        // define this in init so globals are available at runtime
        let tusOptions = {
            endpoint: getTusdEndpoint(),
            withCredentials: true,
            retryDelays: null,
        };

        uppy.use(Uppy.Tus, tusOptions);
        uppy.use(BatchChangePlugin, {target: Uppy.Dashboard});
        uppy.on('upload-success', (file, response) => {
            const metadata = file.meta;
            const d = new Date(metadata.lastModified);
            const pad = (num) => String(num).padStart(2, '0');
            const dFormatted = `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
            const now = new Date(Date.now());
            const nowFormatted = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())} ${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
            let newReqObj, hubTxtObj, parentDirObj;
            newReqObj = {
                "fileName": cgiEncode(metadata.fileName),
                "fileSize": metadata.fileSize,
                "fileType": metadata.fileType,
                "genome": metadata.genome,
                "parentDir": cgiEncode(metadata.parentDir),
                "lastModified": dFormatted,
                "uploadTime": nowFormatted,
                "fullPath": cgiEncode(metadata.parentDir) + "/" + cgiEncode(metadata.fileName),
            };
            // from what I can tell, any response we would create in the pre-finish hook
            // is completely ignored for some reason, so we have to fake the other files
            // we would have created with this one file and add them to the table if they
            // weren't already there:
            if (metadata.fileType !== "hub.txt") {
                // if the user uploaded a hub.txt don't make a second fake object for it
                hubTxtObj = {
                    "uploadTime": nowFormatted,
                    "lastModified": dFormatted,
                    "fileName": "hub.txt",
                    "fileSize": 0,
                    "fileType": "hub.txt",
                    "genome": metadata.genome,
                    "parentDir": cgiEncode(metadata.parentDir),
                    "fullPath": cgiEncode(metadata.parentDir) + "/hub.txt",
                };
            }
            parentDirObj = {
                "uploadTime": nowFormatted,
                "lastModified": dFormatted,
                "fileName": cgiEncode(metadata.parentDir),
                "fileSize": 0,
                "fileType": "dir",
                "genome": metadata.genome,
                "parentDir": "",
                "fullPath": cgiEncode(metadata.parentDir),
            };
            // package the three objects together as one "hub" and display it
            let hub = [parentDirObj, newReqObj];
            if (hubTxtObj) {
                hub.push(hubTxtObj);
            }
            addNewUploadedHubToTable(hub);
            updateQuota(metadata.fileSize);
        });
        uppy.on('complete', (result) => {
            history.replaceState(uiState, "", document.location.href);
            console.log("replace history with uiState");
        });
        inited = true;
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

    function handleRefreshState(jsonData, textStatus) {
        if (checkJsonData(jsonData, 'handleRefreshState')) {
            handleGetFileList(jsonData, true);
        }
    }

    function handleErrorState(jqXHR, textStatus) {
        cart.defaultErrorCallback(jqXHR, textStatus);
    }

    let inited = false; // keep track of first init for tab switching purposes
    function init() {
        cart.setCgiAndUrl(fileListEndpoint);
        cart.debug(debugCartJson);
        // get the file list immediately upon page load
        let activeTab = $("#tabs").tabs( "option", "active" );
        if (activeTab === 3) {
            let url = new URL(window.location.href);
            if (url.protocol === "http:") {
                warn(`The hub upload feature is only available over HTTPS. Please load the HTTPS version of ` +
                        `our site: <a href="https:${url.host}${url.pathname}${url.search}">https:${url.host}${url.pathname}${url.search}</a>`);
            } else if (!inited) {
                cart.send({ getHubSpaceUIState: {}}, handleRefreshState, handleErrorState);
                cart.flush();
            }
        }
    }

    return { init: init,
             uiState: uiState,
             defaultDb: defaultDb,
             makeGenomeSelectOptions: makeGenomeSelectOptions,
             detectFileType: detectFileType,
           };
}());
