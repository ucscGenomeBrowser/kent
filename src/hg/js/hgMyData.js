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
    // copy of cheapgi.c:cgiEncode except we are explicitly leaving '/' characters, and
    // space becomes '+':
    let splitVal = value.split('/');
    splitVal.forEach((ele, ix) => {
        if (ele == " ") {
            splitVal[ix] = '+';
        } else {
            splitVal[ix] = encodeURIComponent(ele);
        }
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
    if (item.disabled || !item.genome) return;
    let newOpt = document.createElement("option");
    newOpt.value = item.genome;
    newOpt.label = item.label;
    newOpt.selected = true;
    selectEle.appendChild(newOpt);
    const event = new Event("change");
    selectEle.dispatchEvent(event);
}

function onSearchError(jqXHR, textStatus, errorThrown, term) {
    return [{label: 'No genomes found', value: '', genome: '', disabled: true}];
}

let autocompletes = {};
function initAutocompleteForInput(inpIdStr, selectEle) {
    // we must set up the autocompleteCat for each input created, once per file chosen
    // override the autocompleteCat.js _renderMenu to get the menu on top
    // of the uppy widget.
    // Return true if we actually set up the autocomplete, false if we have already
    // set it up previously
    if ( !(inpIdStr in autocompletes) || autocompletes[inpIdStr] === false) {
        let selectFunction = setDbSelectFromAutocomplete.bind(null, selectEle);
        initSpeciesAutoCompleteDropdown(inpIdStr, selectFunction, null, null, null, onSearchError);
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

const fileNameRegex = /[0-9a-zA-Z._]+/g; // allowed characters in file names
const fileNameFixRegex = /[^0-9a-zA-Z_]+/g; // '.' get replaced to underbars in trackHub.c. Also any files uploaded from hubtools that may have weird chars need to be escaped
const parentDirRegex = /[0-9a-zA-Z._]+/g; // allowed characters in hub names

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
                // 2bit files name a new assembly hub (editable). Other files
                // with genomeLocked are pinned by a hub-defining sibling or
                // the hub they were drilled into.
                let isTwoBit = file.meta.fileType === "2bit";
                let isLocked = !!file.meta.genomeLocked;
                if (isTwoBit || isLocked) {
                    let editable2bit = isTwoBit && !isLocked;
                    let label;
                    if (editable2bit) {
                        label = "Genome name for your assembly hub:";
                    } else if (isTwoBit) {
                        label = "Genome (set by hub.txt in this batch):";
                    } else {
                        label = "Genome (set by the assembly hub):";
                    }
                    return h('div', {
                            class: "uppy-Dashboard-FileCard-label",
                            style: "display: inline-block; width: 78%"
                            },
                        label,
                        h('input', {
                            id: `${file.meta.name}AsmHubInput`,
                            type: 'text',
                            class: "uppy-u-reset uppy-c-textInput uppy-Dashboard-FileCard-input",
                            style: "margin-left: 5px",
                            value: file.meta.genome || "",
                            disabled: !editable2bit,
                            onChange: e => {
                                let v = hubCreate.sanitizeGenomeName(e.target.value);
                                onChange(v);
                                file.meta.genome = v;
                                file.meta.genomeLabel = v;
                            }
                        })
                    );
                }
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
                            // If the user picked one of their own assembly hubs,
                            // flip hubType and align parentDir so the upload
                            // targets that existing hub rather than creating a
                            // new stub. If they picked a UCSC db, flip back
                            // and reset parentDir to a fresh default so they
                            // don't accidentally upload into an assembly hub.
                            let hub = hubCreate.assemblyHubByGenome(e.target.value);
                            if (hub) {
                                file.meta.hubType = "assemblyHub";
                                file.meta.parentDir = hub.fileName;
                            } else {
                                file.meta.hubType = "trackHub";
                                file.meta.parentDir = hubCreate.uiState.hubNameDefault;
                            }
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
        let filesToOverwrite = []; // collect files that will overwrite existing ones

        // If a 2bit is in the batch, propagate its genome/hubType to siblings.
        // Fall back to sanitized filename because setFileMeta is async from
        // our point of view - captured meta may not be populated yet.
        let batchTwoBit = Object.values(files).find(looksLikeTwoBit);
        if (batchTwoBit) {
            let asmGenome = batchTwoBit.meta.genome || hubCreate.sanitizeGenomeName(batchTwoBit.name);
            for (let f of Object.values(files)) {
                f.meta.genome = asmGenome;
                f.meta.genomeLabel = asmGenome;
                f.meta.hubType = "assemblyHub";
                // fileType may also be stale; recompute from filename if missing
                if (!f.meta.fileType) {
                    f.meta.fileType = hubCreate.detectFileType(f.name);
                }
            }
        }

        // Tag every file so pre-finish knows a user hub.txt is coming in
        // the same batch and can skip synthesizing its own.
        let hasHubTxt = Object.values(files).some(looksLikeHubTxt);
        for (let f of Object.values(files)) {
            f.meta.batchHasHubTxt = hasHubTxt ? "true" : "false";
        }

        for (let [key, file] of Object.entries(files)) {
            let fileNameMatch = file.meta.name.match(fileNameRegex);
            let parentDirMatch = file.meta.parentDir.match(parentDirRegex);
            if (!fileNameMatch || fileNameMatch[0] !== file.meta.name) {
                uppy.info(`Error: File name has special characters, please rename file: ${file.meta.name} to only include alpha-numeric characters, period, or underscore.`, 'error', 5000);
                doUpload = false;
                continue;
            }
            if (!parentDirMatch || parentDirMatch[0] !== file.meta.parentDir) {
                uppy.info(`Error: Hub name has special characters, please rename hub: ${file.meta.parentDir} for file: ${file.meta.name} to only include alpha-numeric characters, period, or underscore.`, 'error', 5000);
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
                let existing = hubCreate.uiState.filesHash[file.meta.parentDir];
                // If the existing hub is an assembly hub, adopt its genome
                // automatically rather than erroring - the UI hid the picker
                // for this case, so the mismatch is just stale metadata.
                if (existing.hubType === "assemblyHub") {
                    file.meta.genome = existing.genome;
                    file.meta.genomeLabel = existing.genome;
                    file.meta.hubType = "assemblyHub";
                } else {
                    uppy.info(`Error: the hub ${file.meta.parentDir} already exists and is for genome "${existing.genome}". Please select the correct genome, a different hub or make a new hub.`);
                    doUpload = false;
                    continue;
                }
            }
            // check if the user is uploading a file that already exists in this hub
            if (file.meta.parentDir in hubCreate.uiState.filesHash) {
                let hubFiles = hubCreate.uiState.filesHash[file.meta.parentDir].children;
                for (let j = 0; j < hubFiles.length; j++) {
                    if (hubFiles[j].fileName === file.meta.name) {
                        filesToOverwrite.push(file);
                        break;
                    }
                }
            }

            // Set metadata directly on the file object since we're returning a modified files object
            // (using setFileMeta would be overwritten when we return the files object)
            file.meta.fileName = file.meta.name;
            file.meta.fileSize = file.size;
            file.meta.lastModified = file.data.lastModified;
            thisQuota += file.size;

        }
        // If any files will overwrite existing ones, show a single confirmation dialog
        if (filesToOverwrite.length > 0) {
            let fileNames = filesToOverwrite.map(f => f.meta.name).join("\n  ");
            if (!confirm(`The following file(s) already exist and will be overwritten:\n  ${fileNames}\n\nContinue?`)) {
                doUpload = false;
            } else {
                // Set metadata flag to allow overwrite on backend for each file
                filesToOverwrite.forEach(f => f.meta.allowOverwrite = "true");
            }
        }
        if (thisQuota + hubCreate.uiState.userQuota > hubCreate.uiState.maxQuota) {
            uppy.info(`Error: this file batch exceeds your quota. Please delete some files to make space or email genome-www@soe.ucsc.edu if you feel you need more space.`);
            doUpload = false;
        }
        return doUpload ? files : false;
    },
});

function looksLikeTwoBit(f) {
    return (f.name || "").toLowerCase().endsWith(".2bit");
}

function looksLikeHubTxt(f) {
    // Accept exact "hub.txt" or any "*.hub.txt" (e.g. "araTha1.hub.txt").
    let n = (f.name || "").toLowerCase();
    return n === "hub.txt" || n.endsWith(".hub.txt");
}

function propagateAssemblyHubMeta(uppyInstance) {
    // When a batch contains a 2bit (and/or an assembly-hub hub.txt), mirror the
    // custom genome name onto every file sharing that parentDir and mark every
    // file hubType=assemblyHub. hub.txt wins over the 2bit's default.
    //
    // We detect the hub-defining files by filename rather than by meta.fileType,
    // because setFileMeta updates Uppy's state immutably - file objects captured
    // from getFiles() earlier in this event may still carry old meta.
    let files = uppyInstance.getFiles();
    let twoBit = files.find(looksLikeTwoBit);
    let hubTxt = files.find(looksLikeHubTxt);
    if (!twoBit && !hubTxt) return;

    function applyGenomeToSiblings(genome, alsoLockHubDefiners) {
        // Set genome/hubType on every file in the batch. Non-hub-defining
        // files (i.e. the sibling tracks) are always locked to this genome so
        // the user can't drift them. The hub-defining files (2bit, hub.txt)
        // are locked only when alsoLockHubDefiners is true - used by the
        // hub.txt path to pin the 2bit's editable field too.
        if (!genome) return;
        // All files in this batch belong to one new hub, so they must share
        // one parentDir. Take it from the hub-defining file - its parentDir
        // came from getDefaultHubName(), while a track that was added first
        // may have been pointed at an existing assembly hub.
        let hubDefiner = hubTxt || twoBit;
        let syncParentDir = hubDefiner && hubDefiner.meta && hubDefiner.meta.parentDir;
        for (let f of uppyInstance.getFiles()) {
            let isHubDefining = looksLikeTwoBit(f) || looksLikeHubTxt(f);
            let meta = {
                genome: genome,
                genomeLabel: genome,
                hubType: "assemblyHub",
                genomeLocked: !isHubDefining || alsoLockHubDefiners,
            };
            if (syncParentDir) meta.parentDir = syncParentDir;
            uppyInstance.setFileMeta(f.id, meta);
        }
    }

    if (hubTxt) {
        hubCreate.readFileAsText(hubTxt.data).then((text) => {
            let parsed = hubCreate.parseHubTxt(text);
            // hub.txt is authoritative: lock the genome field so the user
            // can't edit it and drift the stored db away from what hub.txt
            // says. The user can always edit the hub.txt itself if they
            // want a different name.
            if (parsed.isAssemblyHub && parsed.genome) {
                applyGenomeToSiblings(parsed.genome, true);
                uppyInstance.info(`Using genome "${parsed.genome}" from hub.txt`, "info", 4000);
            } else if (parsed.genome && twoBit) {
                let twoBitGenome = twoBit.meta.genome || hubCreate.sanitizeGenomeName(twoBit.name);
                if (parsed.genome !== twoBitGenome) {
                    applyGenomeToSiblings(parsed.genome, true);
                    uppyInstance.info(`Using genome "${parsed.genome}" from hub.txt (overrides 2bit default)`, "warning", 5000);
                }
            }
        }).catch((err) => {
            console.warn("Could not read hub.txt for genome detection:", err);
        });
        return;
    }

    let asmGenome = twoBit.meta.genome || hubCreate.sanitizeGenomeName(twoBit.name);
    applyGenomeToSiblings(asmGenome, false);
}

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
            // If the batch contains a 2bit, the UCSC genome picker makes no
            // sense - show the custom genome name read-only instead. Detect by
            // filename rather than meta.hubType because setFileMeta updates
            // Uppy state immutably and the meta may not be visible on file
            // objects captured from getFiles() earlier in this event.
            let assemblyHubGenome = null;
            for (let f of this.uppy.getFiles()) {
                if (looksLikeTwoBit(f)) {
                    assemblyHubGenome = f.meta.genome || hubCreate.sanitizeGenomeName(f.name);
                    break;
                }
            }

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

            let batchDbLabel = document.createElement("label");
            batchDbLabel.textContent = "Genome";
            batchDbLabel.style.gridArea = "2 / 1 / 2 / 1";

            let batchDbSelect = null;
            let batchDbGenomeSearchBar = null;
            let batchDbGenomeSearchButton = null;
            let batchDbSearchBarLabel = null;

            if (assemblyHubGenome) {
                // Assembly hub: show the custom genome name as a locked text
                // field, no UCSC picker or search.
                let locked = document.createElement("input");
                locked.type = "text";
                locked.id = "batchAsmHubGenome";
                locked.value = assemblyHubGenome;
                locked.disabled = true;
                locked.classList.add("uppy-u-reset", "uppy-c-textInput");
                locked.style.gridArea = "2 / 2 / 2 / 2";
                locked.style.margin = "2px";
                batchDbLabel.for = "batchAsmHubGenome";

                let note = document.createElement("div");
                note.textContent = "(assembly hub - genome is set by the 2bit in this batch)";
                note.style.gridArea = "2 / 3 / 2 / 5";
                note.style.margin = "auto 0";
                note.style.fontStyle = "italic";

                batchSelectDiv.appendChild(batchSelectText);
                batchSelectDiv.appendChild(batchDbLabel);
                batchSelectDiv.appendChild(locked);
                batchSelectDiv.appendChild(note);
            } else {
                // Track hub: the usual UCSC picker + autocomplete.
                batchDbSelect = document.createElement("select");
                this.createOptsForSelect(batchDbSelect, hubCreate.makeGenomeSelectOptions());
                batchDbSelect.id = "batchDbSelect";
                batchDbSelect.style.gridArea = "2 / 2 / 2 / 2";
                batchDbSelect.style.margin = "2px";
                batchDbLabel.for = "batchDbSelect";

                batchDbSearchBarLabel = document.createElement("label");
                batchDbSearchBarLabel.textContent = "or search for your genome:";
                batchDbSearchBarLabel.style.gridArea = "2 / 3 /2 / 3";
                batchDbSearchBarLabel.style.margin = "auto";

                batchDbGenomeSearchBar = document.createElement("input");
                batchDbGenomeSearchBar.classList.add("uppy-u-reset", "uppy-c-textInput");
                batchDbGenomeSearchBar.type = "text";
                batchDbGenomeSearchBar.id = "batchDbSearchBar";
                batchDbGenomeSearchBar.style.gridArea = "2 / 4 / 2 / 4";
                batchDbGenomeSearchButton = document.createElement("input");
                batchDbGenomeSearchButton.type = "button";
                batchDbGenomeSearchButton.value = "search";
                batchDbGenomeSearchButton.id = "batchDbSearchBarButton";
                batchDbGenomeSearchButton.style.gridArea = "2 / 5 / 2 / 5";

                batchDbSelect.addEventListener("change", (ev) => {
                    let files = this.uppy.getFiles();
                    let val = ev.target.value;
                    let label = ev.target.selectedOptions[0].label;
                    let hub = hubCreate.assemblyHubByGenome(val);
                    for (let [key, file] of Object.entries(files)) {
                        let meta = {
                            genome: val,
                            genomeLabel: label,
                            hubType: hub ? "assemblyHub" : "trackHub",
                            parentDir: hub ? hub.fileName : hubCreate.uiState.hubNameDefault,
                        };
                        this.uppy.setFileMeta(file.id, meta);
                    }
                });

                batchSelectDiv.appendChild(batchSelectText);
                batchSelectDiv.appendChild(batchDbLabel);
                batchSelectDiv.appendChild(batchDbSelect);
                batchSelectDiv.appendChild(batchDbSearchBarLabel);
                batchSelectDiv.appendChild(batchDbGenomeSearchBar);
                batchSelectDiv.appendChild(batchDbGenomeSearchButton);
            }

            // the batch change hub name (shown in both modes)
            let batchParentDirLabel = document.createElement("label");
            batchParentDirLabel.textContent = "Hub Name";
            batchParentDirLabel.for = "batchParentDir";
            batchParentDirLabel.style.gridArea = "3 / 1 / 3 / 1";

            let batchParentDirInput = document.createElement("input");
            batchParentDirInput.id = "batchParentDir";
            batchParentDirInput.value = hubCreate.getDefaultHubName();
            batchParentDirInput.style.gridArea = "3 / 2 / 3 / 2";
            batchParentDirInput.style.margin= "1px 1px auto";
            batchParentDirInput.classList.add("uppy-u-reset", "uppy-c-textInput");

            batchParentDirInput.addEventListener("change", (ev) => {
                let files = this.uppy.getFiles();
                let val = ev.target.value;
                for (let [key, file] of Object.entries(files)) {
                    this.uppy.setFileMeta(file.id, {parentDir: val});
                }
            });

            batchSelectDiv.appendChild(batchParentDirLabel);
            batchSelectDiv.appendChild(batchParentDirInput);

            // append the batch changes to the bottom of the file list, for some reason
            // I can't append to the actual Dashboard-files, it must be getting emptied
            // and re-rendered or something
            let uppyFilesDiv = document.querySelector(".uppy-Dashboard-progressindicators");
            if (uppyFilesDiv) {
                uppyFilesDiv.insertBefore(batchSelectDiv, uppyFilesDiv.firstChild);
            }

            // autocomplete only applies in the track-hub path
            if (batchDbSelect && batchDbGenomeSearchBar && batchDbGenomeSearchButton) {
                let justInitted = initAutocompleteForInput(batchDbGenomeSearchBar.id, batchDbSelect);
                if (justInitted) {
                    batchDbGenomeSearchButton.addEventListener("click", (e) => {
                        let inp = document.getElementById(batchDbGenomeSearchBar.id).value;
                        let selector = "[id='"+batchDbGenomeSearchBar.id+"']";
                        $(selector).autocompleteCat("search", inp);
                    });
                }
            }
        }
    }

    install() {
        this.uppy.on("file-added", (file) => {
            // add default meta data for genome and fileType
            let ftype = hubCreate.detectFileType(file.name);
            let defaultMeta = {
                "genome": hubCreate.defaultDb(),
                "fileType": ftype,
                "parentDir": hubCreate.getDefaultHubName(),
                "hubType": "trackHub",
            };
            if (ftype === "2bit") {
                // This file defines an assembly hub. Default the genome to the
                // sanitized filename stem; the user can edit it in the file card.
                defaultMeta.genome = hubCreate.sanitizeGenomeName(file.name);
                defaultMeta.genomeLabel = defaultMeta.genome;
                defaultMeta.hubType = "assemblyHub";
            }
            this.uppy.setFileMeta(file.id, defaultMeta);

            // When drilled into an assembly hub, inherit and lock its genome.
            let drilledIntoAsmHub = false;
            if (hubCreate.uiState.currentHub &&
                hubCreate.uiState.currentHub === defaultMeta.parentDir) {
                let existing = hubCreate.uiState.filesHash[defaultMeta.parentDir];
                if (existing && existing.hubType === "assemblyHub") {
                    this.uppy.setFileMeta(file.id, {
                        genome: existing.genome,
                        genomeLabel: existing.genome,
                        hubType: "assemblyHub",
                        genomeLocked: true,
                    });
                    drilledIntoAsmHub = true;
                }
            }

            // Top-level with no drilled-in hub: if the user already has an
            // assembly hub, default this file to target it so they don't have
            // to re-enter the genome + hub name. The user can still switch via
            // the dropdown. Skip this when the file itself is hub-defining
            // (2bit or hub.txt) or when any file in the batch is - in that
            // case the batch is creating a *new* hub, not adding to an
            // existing one, so the defaults should not point at the old hub.
            let fileIsHubDefining = ftype === "2bit" || ftype === "hub.txt";
            let batchHasHubDefining = this.uppy.getFiles().some(f =>
                looksLikeTwoBit(f) || looksLikeHubTxt(f));
            if (!drilledIntoAsmHub && !fileIsHubDefining && !batchHasHubDefining) {
                let firstHub = hubCreate.firstAssemblyHub();
                if (firstHub) {
                    this.uppy.setFileMeta(file.id, {
                        genome: firstHub.genome,
                        genomeLabel: firstHub.genome,
                        parentDir: firstHub.fileName,
                        hubType: "assemblyHub",
                        // NOT genomeLocked - user may want a different hub/genome
                    });
                }
            }

            // If a 2bit is in the batch, every sibling file in the same parentDir
            // adopts its genome and gets hubType=assemblyHub. Also handle hub.txt:
            // parse it client-side and, if it declares an assembly hub, mirror
            // those values onto every file (hub.txt wins).
            propagateAssemblyHubMeta(this.uppy);

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
                    uppy.info(`Error: File name has special characters, please rename file: '${file.meta.name}' to only include alpha-numeric characters, period, or underscore.`, 'error', 5000);
                }
                if (!parentDirMatch || parentDirMatch[0] !== file.meta.parentDir) {
                    uppy.info(`Error: Hub name has special characters, please rename hub: '${file.meta.parentDir}' to only include alpha-numeric characters, period, or underscore.`, 'error', 5000);
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
        currentHub: "", // if the user has a hub dir open, set the name here and use it as the default
                        // hub name when uploading a new file with the dir open, otherwise hubNameDefault
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
        "2bit": [".2bit"],
        "text": [".txt", ".text"],
    };

    function getDefaultHubName() {
        return uiState.currentHub.length > 0 ? uiState.currentHub : uiState.hubNameDefault;
    }

    function sanitizeGenomeName(name) {
        // Strip .2bit, replace non-alphanumeric/_/- with _, drop hub_ prefix.
        // Returns empty string if nothing usable is left.
        if (!name) return "";
        let stem = name.replace(/\.2bit$/i, "");
        stem = stem.replace(/[^A-Za-z0-9_-]/g, "_");
        stem = stem.replace(/^hub_/, "");
        return stem;
    }

    function hubTxtPathForHub(hubName) {
        // Return the fullPath of the hub.txt file inside hubName as recorded in
        // hubSpace, falling back to "hubName/hub.txt" if there's no row yet.
        // The user may have uploaded their own "araTha1.hub.txt" - use its
        // actual filename rather than assuming "hub.txt".
        let dir = uiState.filesHash[hubName];
        if (dir && dir.children) {
            for (let child of dir.children) {
                if (child.fileType === "hub.txt") return child.fullPath;
            }
        }
        return hubName + "/hub.txt";
    }

    function assemblyHubByGenome(genome) {
        // Return the dir row of the user's assembly hub whose genome matches,
        // or null. If genome is falsy, return the first assembly hub found.
        for (let fullPath in uiState.filesHash) {
            let fd = uiState.filesHash[fullPath];
            if (fd.fileType !== "dir" || fd.hubType !== "assemblyHub" || !fd.genome) continue;
            if (!genome || fd.genome === genome) return fd;
        }
        return null;
    }

    function firstAssemblyHub() { return assemblyHubByGenome(null); }
    function genomeIsAssemblyHub(genome) { return !!genome && !!assemblyHubByGenome(genome); }

    function parseHubTxt(text) {
        // Very small hub.txt parser: extracts `genome` and `twoBitPath` lines.
        // Returns {genome, twoBitPath, isAssemblyHub} (values may be null/false).
        let ret = {genome: null, twoBitPath: null, isAssemblyHub: false};
        if (!text) return ret;
        let lines = text.split(/\r?\n/);
        for (let line of lines) {
            let trimmed = line.replace(/^\s+/, "");
            if (trimmed.startsWith("genome ") && !ret.genome) {
                ret.genome = trimmed.substring(7).trim();
            } else if (trimmed.startsWith("twoBitPath ")) {
                ret.twoBitPath = trimmed.substring(11).trim();
                ret.isAssemblyHub = true;
            }
        }
        return ret;
    }

    function readFileAsText(fileObj) {
        // Return a Promise resolving to the file contents as text.
        return new Promise((resolve, reject) => {
            let reader = new FileReader();
            reader.onload = () => resolve(reader.result);
            reader.onerror = () => reject(reader.error);
            reader.readAsText(fileObj);
        });
    }

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

        // Include the user's uploaded assembly hubs as options. One entry per
        // assembly hub (dedupe by genome name), taken from the dir row in
        // filesHash. This lets users picking a dropdown genome target a hub
        // they already created.
        let seenAsmHub = {};
        for (let fullPath in uiState.filesHash) {
            let fd = uiState.filesHash[fullPath];
            if (fd.fileType === "dir" && fd.hubType === "assemblyHub" &&
                fd.genome && !seenAsmHub[fd.genome]) {
                seenAsmHub[fd.genome] = true;
                ret.push({
                    value: fd.genome,
                    label: `${fd.genome} (your assembly hub)`,
                });
            }
        }

        // Cache the value/label pair so it's a default next time - but skip
        // assembly-hub genomes, those are added by the loop above with the
        // "(your assembly hub)" suffix and would otherwise show up twice.
        if (value && label && !(label in defaultGenomeChoices) &&
            !genomeIsAssemblyHub(value)) {
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

    function viewInGenomeBrowser(fname, ftype, genome, hubName, hubType) {
        // redirect to hgTracks with this track open in the hub
        if (typeof uiState.userUrl !== "undefined" && uiState.userUrl.length > 0) {
            if (ftype in extensionMap) {
                // TODO: tusd should return this location in it's response after
                // uploading a file and then we can look it up somehow, the cgi can
                // write the links directly into the html directly for prev uploaded files maybe?
                let hubUrl = uiState.userUrl + cgiEncode(hubTxtPathForHub(hubName));
                // Assembly hubs use the user-defined genome name, which isn't a
                // UCSC db - hgTracks needs 'genome=' (resolves via the hub)
                // rather than 'db=' (looks up a native assembly).
                let dbParam = hubType === "assemblyHub" ? "genome" : "db";
                let url = "../cgi-bin/hgTracks?hgsid=" + getHgsid() + "&" + dbParam + "=" + genome + "&hubUrl=" + encodeURIComponent(hubUrl) + "&" + trackHubFixName(fname) + "=pack";
                window.location.assign(url);
                return false;
            }
        }
    }

    function trackHubFixName(trackName) {
        // replace everything but alphanumeric and underscore with underscore
        return encodeURIComponent(trackName.replaceAll(fileNameFixRegex, "_"));
    }

    // helper object so we don't need to use an AbortController to update
    // the data this function is using
    let selectedData = {};
    // track which items the user directly selected (vs children of selected directories)
    let directlySelected = {};
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
                    // Assembly hubs use 'genome=' (hub-resolved name), native
                    // UCSC assemblies use 'db='.
                    let dbParam = d.hubType === "assemblyHub" ? "genome" : "db";
                    url += "&" + dbParam + "=" + genome;
                }
                if (d.fileType === "hub.txt") {
                    url += "&hubUrl=" + encodeURIComponent(uiState.userUrl + cgiEncode(d.fullPath));
                }
                else if (d.fileType in extensionMap) {
                    // TODO: tusd should return this location in it's response after
                    // uploading a file and then we can look it up somehow, the cgi can
                    // write the links directly into the html directly for prev uploaded files maybe?
                    if (!(d.parentDir in hubsAdded)) {
                        // NOTE: hubUrls get added regardless of whether they are on this assembly
                        // or not, because multiple genomes may have been requested. If this user
                        // switches to another genome we want this hub to be connected already
                        // Resolve the actual hub.txt filename - user may have
                        // uploaded "<prefix>.hub.txt" rather than literal hub.txt.
                        let hubDir = d.parentDir.replace(/\/$/, "");
                        url += "&hubUrl=" + encodeURIComponent(uiState.userUrl + cgiEncode(hubTxtPathForHub(hubDir)));
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
        // Only warn about hub.txt deletion if the user directly selected the hub.txt file,
        // not if it's being deleted as part of selecting a whole hub/directory
        let hasDirectlySelectedHubTxt = Object.values(directlySelected).some(d => d.fileType === "hub.txt");
        if (hasDirectlySelectedHubTxt) {
            if (!confirm("Warning: Deleting a hub.txt file will remove your hub and its shareable URL. Are you sure?")) {
                return;
            }
        }
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
            let deleteBtn = document.getElementById("deleteSelectedFiles");
            deleteBtn.style.display = "inline-block";
            deleteBtn.addEventListener("click", deleteFileList);
            deleteBtn.textContent = "Delete selected";
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
        // Track only the rows the user directly selected (not children)
        directlySelected = {};

        // get all of the currently selected rows (may be more than just the one that
        // was most recently clicked)
        table.rows({selected: true}).data().each(function(row, ix) {
            data.push(row);
            selectedData[row.fullPath] = row;
            directlySelected[row.fullPath] = row;
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
        uiState.currentHub = "";
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
        uiState.currentHub = dirName;
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
            if (rowData.fileType !== "hub.txt" && rowData.fileType !== "text" && rowData.fileType !== "tabixIndex" && rowData.fileType !== "bamIndex" && rowData.fileType !== "2bit" && rowData.fileType in extensionMap) {
                let container = document.createElement("div");
                let viewBtn = document.createElement("button");
                viewBtn.textContent = "View in Genome Browser";
                viewBtn.type = 'button';
                viewBtn.addEventListener("click", function(e) {
                    e.stopPropagation();
                    viewInGenomeBrowser(rowData.fileName, rowData.fileType, rowData.genome, rowData.parentDir, rowData.hubType);
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
        // Rebuild filesHash from remaining fileList to remove stale entries
        uiState.filesHash = {};
        parseFileListIntoHash(uiState.fileList);
        // If the currently viewed hub directory was deleted (its data is in oldRowData
        // because dataTableCustomOrder moved it to the header), clean up that stale state
        if (oldRowData && pathList.includes(oldRowData.fullPath)) {
            let thead = document.querySelector(
                ".dt-scroll-headInner > table:nth-child(1) > thead:nth-child(1)");
            if (thead && thead.childNodes.length > 1) {
                thead.removeChild(thead.lastChild);
            }
            oldRowData = null;
            dataTableShowTopLevel(table);
            dataTableEmptyBreadcrumb(table);
            table.order([{name: "uploadTime", dir: "desc"}]);
            table.draw();
        }
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
            } else {
                // File already exists - update the existing row with new data (for overwrites)
                let existingObj = uiState.filesHash[obj.fullPath];
                existingObj.fileSize = obj.fileSize;
                existingObj.lastModified = obj.lastModified;
                existingObj.uploadTime = obj.uploadTime;
                // Find and invalidate the row in DataTable to refresh display
                let allRows = table.rows().indexes();
                for (let j = 0; j < allRows.length; j++) {
                    let rowData = table.row(allRows[j]).data();
                    if (rowData.fullPath === obj.fullPath) {
                        table.row(allRows[j]).invalidate();
                        break;
                    }
                }
            }
        }

        // show all the new rows we just added, note the double draw, we need
        // to have the new rows rendered to do the order because the order
        // will copy the actual DOM node
        parseFileListIntoHash(uiState.fileList);
        dataTableShowDir(table, hubDirData.fileName, hubDirData.fullPath);
        table.draw();
        dataTableCustomOrder(table, hubDirData);
        // Flush dataTableCustomOrder's row.remove() so DataTables internal state is clean,
        // then defer columns.adjust() to allow browser reflow after header DOM manipulation
        table.draw();
        setTimeout(function() {
            table.columns.adjust();
        }, 0);
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
                    let decodedName = decodeURIComponent(data);
                    if (type !== "display" || row.fileType === "dir") {
                        return decodedName;
                    }
                    if (typeof uiState.userUrl === "undefined" || uiState.userUrl.length === 0) {
                        return decodedName;
                    }
                    let fileUrl = uiState.userUrl + cgiEncode(row.fullPath);
                    let copyIcon = '<svg class="copyLinkIcon" title="Copy file URL to clipboard" data-url="' + fileUrl + '" style="margin-left: 6px; cursor: pointer; vertical-align:baseline; width:0.8em" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512"><path d="M502.6 70.63l-61.25-61.25C435.4 3.371 427.2 0 418.7 0H255.1c-35.35 0-64 28.66-64 64l.0195 256C192 355.4 220.7 384 256 384h192c35.2 0 64-28.8 64-64V93.25C512 84.77 508.6 76.63 502.6 70.63zM464 320c0 8.836-7.164 16-16 16H255.1c-8.838 0-16-7.164-16-16L239.1 64.13c0-8.836 7.164-16 16-16h128L384 96c0 17.67 14.33 32 32 32h47.1V320zM272 448c0 8.836-7.164 16-16 16H63.1c-8.838 0-16-7.164-16-16L47.98 192.1c0-8.836 7.164-16 16-16H160V128H63.99c-35.35 0-64 28.65-64 64l.0098 256C.002 483.3 28.66 512 64 512h192c35.2 0 64-28.8 64-64v-32h-47.1L272 448z"/></svg>';
                    return '<a class="fileLink" href="' + fileUrl + '" target="_blank" rel="noopener">' + decodedName + '</a>' + copyIcon;
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
            let copyIcon = e.target.closest ? e.target.closest(".copyLinkIcon") : null;
            if (copyIcon) {
                e.stopPropagation();
                e.preventDefault();
                let url = copyIcon.getAttribute("data-url");
                navigator.clipboard.writeText(url).then(function() {
                    let feedback = document.createElement("span");
                    feedback.textContent = "copied";
                    feedback.style.marginLeft = "6px";
                    feedback.style.fontSize = "0.85em";
                    feedback.style.color = "#080";
                    copyIcon.parentNode.replaceChild(feedback, copyIcon);
                    setTimeout(function() {
                        if (feedback.parentNode) {
                            feedback.parentNode.replaceChild(copyIcon, feedback);
                        }
                    }, 1500);
                }, function() {
                    alert("Failed to copy URL: " + url);
                });
                return;
            }
            if (e.target.closest && e.target.closest(".fileLink")) {
                e.stopPropagation();
                return;
            }
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
            removeFingerprintOnSuccess: true, // clean up localStorage after successful upload
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
            let hubType = metadata.hubType || "trackHub";
            newReqObj = {
                "fileName": cgiEncode(metadata.fileName),
                "fileSize": metadata.fileSize,
                "fileType": metadata.fileType,
                "genome": metadata.genome,
                "parentDir": cgiEncode(metadata.parentDir),
                "lastModified": dFormatted,
                "uploadTime": nowFormatted,
                "fullPath": cgiEncode(metadata.parentDir) + "/" + cgiEncode(metadata.fileName),
                "hubType": hubType,
            };
            // from what I can tell, any response we would create in the pre-finish hook
            // is completely ignored for some reason, so we have to fake the other files
            // we would have created with this one file and add them to the table if they
            // weren't already there:
            // Only fabricate a hub.txt row when the backend actually synthesized
            // one. Skip if the user supplied their own *.hub.txt (either already
            // in filesHash from a prior upload, or coming in this same batch -
            // upload-success order is arbitrary so the hub.txt row may not be
            // in filesHash yet when a sibling's upload-success fires).
            let dirHash = uiState.filesHash[cgiEncode(metadata.parentDir)];
            let hubTxtExists = !!(dirHash && dirHash.children &&
                dirHash.children.some(c => c.fileType === "hub.txt"));
            let batchHasHubTxt = metadata.batchHasHubTxt === "true";
            if (metadata.fileType !== "hub.txt" && !hubTxtExists && !batchHasHubTxt) {
                hubTxtObj = {
                    "uploadTime": nowFormatted,
                    "lastModified": dFormatted,
                    "fileName": "hub.txt",
                    "fileSize": 0,
                    "fileType": "hub.txt",
                    "genome": metadata.genome,
                    "parentDir": cgiEncode(metadata.parentDir),
                    "fullPath": cgiEncode(metadata.parentDir) + "/hub.txt",
                    "hubType": hubType,
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
                "hubType": hubType,
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
            } else if ((url.protocol + "//" + url.host) !== loginHost) {
                warn(`The hub upload feature is only avaiable on our US based public site (<a href="${loginHost}">${loginHost}</a>) for speed purposes. Please go there to upload your hubs, copy the links to the hub.txt files, then use the Connected Hubs tab here to view your files.`);
            } else if (!inited && isLoggedIn) {
                cart.send({ getHubSpaceUIState: {}}, handleRefreshState, handleErrorState);
                cart.flush();
            } else {
                showExistingFiles([]);
            }
        }
    }

    return { init: init,
             uiState: uiState,
             defaultDb: defaultDb,
             makeGenomeSelectOptions: makeGenomeSelectOptions,
             getDefaultHubName: getDefaultHubName,
             detectFileType: detectFileType,
             sanitizeGenomeName: sanitizeGenomeName,
             readFileAsText: readFileAsText,
             parseHubTxt: parseHubTxt,
             firstAssemblyHub: firstAssemblyHub,
             genomeIsAssemblyHub: genomeIsAssemblyHub,
             assemblyHubByGenome: assemblyHubByGenome,
           };
}());
