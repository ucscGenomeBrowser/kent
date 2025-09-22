// Helper functions for using igv.js with local files in the UCSC genome browser
//
// The UCSC browser does not use modules, so wrap code in a self-executing function to limit
// scope of variables to this file.

(function () {
    const indexExtensions = new Set(['bai', 'csi', 'tbi', 'idx', 'crai']);
    const requireIndex = new Set(['bam', 'cram']);

    // File scope variables
    const IGV_STORAGE_KEY = "igvSession";
    let filePicker = null;
    let isDragging = false;
    let sessionAutoSaveTimer = null;
    let igvBrowser = null;

    // Create a BroadcastChannel for communication between the UCSC browser page and the file picker page.
    const channel = new BroadcastChannel('igv_file_channel');

    // Message types for communication between browser page and file picker page
    const MSG = {
        SELECTED_FILES: 'selectedFiles',
        RESTORE_FILES: 'restoreFiles',
        REMOVED_TRACK: 'removedTrack',
        LOAD_URL: 'loadURL',
        FILE_PICKER_READY: 'filePickerReady',
        PING: 'ping',
        PONG: 'pong'
    };

    /**
     * A mock File object that wraps a real File object.  The purpose of this class is to provide a stable
     * identifier (id) for the file that can be used to restore the File object on page refresh. The object
     * looks like a File object to igv.js but has an extra "id" attribute.
     */
    class MockFile {

        constructor(id, file, name) {
            this.id = id;
            this.file = file;
            this.name = name || (file ? file.name : undefined);
            this.type = 'MockFile';
        }

        slice(start, end) {
            this.checkFile();
            return this.file.slice(start, end);
        }

        async text() {
            this.checkFile();
            return this.file.text();
        }

        async arrayBuffer() {
            this.checkFile();
            return this.file.arrayBuffer();
        }

        checkFile() {
            if (!this.file) {
                throw new Error(`Connection to file ${this.name} is not available.  Please re-select the file` +
                    ' in the IGV File Manager window.');
            }
        }
    }


    /**
     * Initialize igv.js, creating an igvBrowser if a session is found in local storage.   The existence of a
     * global "igvBrowser" variable indicates igv.js has already been initialized.
     *
     * @returns {Promise<void>}
     */
    async function initIgvUcsc() {

        if (igvBrowser) {
            // This can happend because initVars(), which calls initIgvUcsc(), is called from multiple places
            //console.log("igvBrowser already exists");
            return;
        }

        // Retrieve the igv session for this genome, if any,  from local storage.
        // TODO -- in the future this might come from the UCSC session (cart)
        const db = getDb();
        let sessionString = getSessionStorage()[db];

        if (sessionString) {

            const igvSession = JSON.parse(igv.uncompressSession(`blob:${sessionString}`));

            // Reconnect any file-based tracks to the actual File objects.
            if (igvSession.tracks) {

                const failed = await restoreTrackConfigurations(igvSession.tracks);

                if (failed.length > 0) {

                    const sendRestoreRequest = () => channel.postMessage({type: "restoreFiles", files: failed});

                    if (filePicker && !filePicker.closed) {
                        // Filepicker is set.  Its not clear how this happens, or if its even possible.
                        sendRestoreRequest();
                        return;
                    } else if (await pingFilePicker()) {

                        // A file picker is open, but (apparently) doesn't have references to the requested files.
                        // Send a request to restore the files, which will bring the file picker to the front and
                        // prompt the user to select the files. -- Prompt provided by igv.js error handler for now.
                        // showDialog(`Connections to ${failed.length} file(s) in the IGV session could not be ` +
                        //    `restored.  Please re-select the files in the IGV File Manager window.`);
                        sendRestoreRequest();

                    } else {
                        // Open a file picker and prompt user to select files to restore the connections.
                        filePicker = openFilePicker();
                        filePicker.onload = () => sendRestoreRequest();
                    }
                }
            }
            await createIGVBrowser(igvSession);
        }
    }

    /**
     * Given a list of files, return a list of track configurations.  Each configuration contains a url (MockFile) and
     * optionally an indexURL (MockFile).
     *
     * NOTE: igv uses the "url" and "indexURL" fiels for local files as well as URLs.
     *
     * @param files
     * @returns {*[]}
     */
    function getTrackConfigurations(files) {

        // Index look up table, key is the data file name, value is {id, file}
        const indexLUT = new Map();

        // Separate data files from index files
        const dataFiles = [];
        for (let {id, file} of files) {

            const name = file.name;
            const {dataPath, extension} = getExtension(name);

            if (indexExtensions.has(extension)) {
                // key is the data file name
                const key = dataPath;
                indexLUT.set(key, {id, file});
            } else {
                dataFiles.push({id, file});
            }
        }

        // Now create configurations, matching index files to corresponding data files when possible
        const configurations = [];
        for (let {id, file} of dataFiles) {

            const filename = file.name;
            const {extension} = getExtension(filename);

            if (indexLUT.has(filename)) {

                const indexURL = indexLUT.get(filename);

                configurations.push({
                    id: id,
                    url: new MockFile(id, file),
                    indexURL: new MockFile(indexURL.id, indexURL.file)
                });

            } else if (requireIndex.has(extension)) {
                throw new Error(`Unable to load track file ${filename} - you must select both ${filename} and its corresponding index file`);
            } else {
                configurations.push({url: new MockFile(id, file)});
            }

        }
        return configurations;
    }

    /**
     * Given a file name return the data path (file name without extension) and the extension.  If no extension
     * is present the extension is the empty string.
     *
     * @param name
     * @returns {{dataPath, extension: string}|{dataPath: string, extension: string}}
     */
    function getExtension(name) {
        const idx = name.lastIndexOf('.');

        if (idx > 0) {
            let dataPath = name.substring(0, idx);
            const extension = name.substring(idx + 1);

            // Special case for Picard  file convention
            if ('bai' === extension && !dataPath.endsWith('.bam')) {
                dataPath = dataPath + '.bam';
            } else if ('crai' === extension && !dataPath.endsWith('.cram')) {
                dataPath = dataPath + '.cram';
            }

            return {dataPath, extension};
        } else {
            return {
                dataPath: name,
                extension: ''
            };
        }
    }

    /**
     * Called upon a page reload to restore file references in the track configurations.  Returns a list of file
     * descriptors {id, name} for files that could not be restored.
     *
     * @param trackConfigurations
     * @returns {Promise<*[]>}
     */
    async function restoreTrackConfigurations(trackConfigurations) {
        const failed = [];
        for (let config of trackConfigurations) {

            if (config.url && 'MockFile' === config.url.type) {
                const {id, name} = config.url;
                const file = await restoreFile(id);
                if (!file) {
                    failed.push({id, name});
                }
                config.url = new MockFile(id, file, name);
            }
            if (config.indexURL && 'MockFile' === config.indexURL.type) {
                const {id, name} = config.indexURL;
                const file = await restoreFile(id);
                if (!file) {
                    failed.push({id, name});
                }
                config.indexURL = new MockFile(id, file, name);
            }
        }
        return failed;
    }

    /**
     * Attempt to restore a File object given its id by sending a "getFile" message on the BroadcastChannel and waiting for
     * a "file" message in response.  If no response is received within 1/2 a second undefined is returned.
     *
     * NOTE: the 1/2 second timeout is arbitrary, and could be reduced to improve responsiveness.
     *
     * @param id
     * @returns {Promise<unknown>}
     */
    async function restoreFile(id) {
        return new Promise((resolve) => {

            const fileListener = (event) => {
                const msg = event.data;
                if (msg.type === 'file' && msg.id === id) {
                    cleanup();
                    resolve(msg.data);
                }
            };

            const timeout = setTimeout(() => {
                cleanup();
                console.error(`Timeout waiting for file with id: ${id}`);
                resolve(undefined);
            }, 500);

            function cleanup() {
                channel.removeEventListener('message', fileListener);
                clearTimeout(timeout);
            }

            channel.addEventListener('message', fileListener);
            channel.postMessage({type: 'getFile', id});
        });
    }


    /**
     *  Update the igv.js browser to reflect a change in the UCSC browser start position.  This is called
     *  when the user drags a UCSC track image to a new position.  It is intended for small changes, and
     *  works by shifting the igv.js predrawn track image.  This will not work for large position
     *  changes, or for changes in resolution.
     *
     * @param newPortalStart
     */
    function updateIgvStartPosition(newPortalStart) {
        // TODO -- this is hacky, add new function to igv.js
        if (igvBrowser && !isDragging) {
            const rf = igvBrowser.referenceFrameList[0];
            const d = newPortalStart - 1 - rf.start;
            rf.shift(d);
            const allTracks = igvBrowser.findTracks(t => true);
            for (let track of allTracks) {
                const viewports = track.trackView.viewports;
                for (let vp of viewports) {
                    vp.shift();
                }
            }
        }
    }


    /**
     * Return the session storage object, which is a dictionary of {genomeID: sessionString}
     * @returns {any|{}}
     */
    function getSessionStorage() {
        // Retrieve igv session string from local storage.
        // TODO -- in the future this might come from the UCSC session (cart)
        const localStorageString = localStorage.getItem(IGV_STORAGE_KEY);
        return localStorageString ? JSON.parse(localStorageString) : {};
    }

    /**
     * Update the igv session in local storage.  This is called periodically and on adding tracks.  Ideally this
     * would be called on IGV state change, but we don't have a means to capture all state changes.
     */
    function updateSessionStorage() {
        if (igvBrowser) {
            //setCartVar("igvState", igvSession, null, false);
            const sessionDict = getSessionStorage();
            const db = getDb();
            sessionDict[db] = igvBrowser.compressedSession();
            localStorage.setItem(IGV_STORAGE_KEY, JSON.stringify(sessionDict));
        } else {
            localStorage.removeItem(IGV_STORAGE_KEY);
        }
    }

    /**
     * Start a timer to periodically save the igv session to local storage.  When / if we are able to
     * reliably capture IGV state changes we can eliminate this and just save on state change.
     *
     * @param intervalMs
     */
    function startSessionAutoSave(intervalMs = 1000) {
        if (sessionAutoSaveTimer !== null) return; // already running
        sessionAutoSaveTimer = setInterval(updateSessionStorage, intervalMs);
    }

    function stopSessionAutoSave() {
        if (sessionAutoSaveTimer !== null) {
            clearInterval(sessionAutoSaveTimer);
            sessionAutoSaveTimer = null;
        }
    }


    window.addEventListener("DOMContentLoaded", async () => {

        // The "Add IGV track" button handler.  The button opens the file picker window, unless
        // it is already open in which case it brings that window to the front.  Tracks are added
        // from the filePicker page by selecting track files.
        document.getElementById('hgtIgv').addEventListener('click', async function (e) {
            e.preventDefault(); // our
            if (filePicker && !filePicker.closed) {
                showDialog("To add tracks please use the IGV File Manager window.");
                filePicker.focus();
                return;
            } else {
                // A file picker may be open from a previous session. First ping it to see if it is still there,
                // if it responds the user should be alerted, if needed,  from a failed track load.
                const responded = await pingFilePicker();
                if (responded) {
                    showDialog("To add tracks please use the existing IGV File Manager window.");
                } else {
                    filePicker = openFilePicker();
                }
            }
        });

        initializeDialog();

    });

    // Initialize a jQuery UI dialog used for user messages.
    function initializeDialog() {

        // Inject a hidden dive for an alert dialog.  We use this to report errors.
        const alertDialog = document.createElement('div');
        alertDialog.id = 'igvAlertDialog';
        alertDialog.title = 'Alert!';
        alertDialog.style.display = 'none';
        document.body.appendChild(alertDialog);

        $("#igvAlertDialog").dialog({
            autoOpen: false,
            modal: true,
            position: {my: "center", at: "center", of: $("#imgTbl")}
        });
    }


    /**
     * Send a "ping" message to the file picker window and wait up to 100 msec for a "pong" response.  Used to
     * determine if a file picker window is already open.
     * @param channel
     * @returns {Promise<unknown>}
     */
    async function pingFilePicker() {
        return new Promise((resolve) => {

            const pongListener = (event) => {
                if (event.data && event.data.type === "pong") {
                    clearTimeout(timeout);
                    channel.removeEventListener('message', pongListener);
                    resolve(true);
                }
            };

            const timeout = setTimeout(() => {
                channel.removeEventListener('message', pongListener);
                resolve(false);
            }, 100);

            channel.addEventListener('message', pongListener);
            channel.postMessage({type: "ping"});
        });
    }


    /**
     * Open the file picker window.  If the window is already open it *should* be brought to the front.
     * @returns {WindowProxy}
     */
    function openFilePicker() {
        filePicker = window.open('../admin/filePicker.html', 'igvFilePicker', 'width=600,height=1000');
        if (!filePicker) {
            showDialog("Unable to open file picker window.  Please disable your popup blocker for this site and try again.");
        }
        return filePicker;
    }

    /**
     * Update the track names in the left hand column of the IGV row in the image table.
     */
    function updateTrackNames() {


        // Add track names to the left hand column.
        if (typeof igvBrowser === "undefined") return;

        const gearIconSize = 10;
        const leftSidebar = document.getElementById('igv_leftsidebar');
        const nameDiv = document.getElementById('igv_namediv');

        // Clear any existing content
        nameDiv.innerHTML = "";
        leftSidebar.innerHTML = "";

        const allTracks = igvBrowser.findTracks(t => t.type);  // ignore tracks with no type
        let top = 0;
        for (let track of allTracks) {

            const gearDiv = document.createElement('div');
            gearDiv.style.position = 'absolute';
            gearDiv.style.top = `${top + 5}px`; // Slightly offset from the top of the track
            gearDiv.style.left = '2px';
            gearDiv.style.width = `${gearIconSize}px`;
            gearDiv.style.height = `${gearIconSize}px`;
            gearDiv.style.maxWidth = `${gearIconSize}px`;
            gearDiv.style.maxHeight = `${gearIconSize}px`;
            gearDiv.style.overflow = 'hidden';
            gearDiv.style.cursor = 'pointer';

            const cog = igv.createIcon('cog', 'black');
            const svg = cog.tagName && cog.tagName.toLowerCase() === 'svg' ? cog : cog.querySelector('svg');
            svg.setAttribute('width', gearIconSize.toString());
            svg.setAttribute('height', gearIconSize.toString());
            svg.style.width = `${gearIconSize}px`;
            svg.style.height = `${gearIconSize}px`;
            gearDiv.appendChild(cog);
            gearDiv.setAttribute('data-track-id', track.id); // Set the track ID attribute
            document.getElementById('igv_leftsidebar').appendChild(gearDiv);


            gearDiv.addEventListener("click", (e) => {
                e.stopPropagation();
                e.preventDefault();
                const trackId = e.currentTarget.getAttribute('data-track-id');
                const matchingTracks = igvBrowser.findTracks(t => t.id === trackId);
                if (matchingTracks.length > 0) {
                    const trackView = matchingTracks[0].trackView;
                    trackView.trackGearPopup.presentMenuList(
                        trackView,
                        igvBrowser.menuUtils.trackMenuItemList(trackView),
                        igvBrowser.config);
                }
            });

            // Move the track gear popup location, and hide its true parent.  This is a bit hacky, but
            // moving the gear popover to a new parent has undesired side effects.
            const popover = track.trackView.trackGearPopup.popover;
            if (popover) {
                popover.parentElement.style.width = '0px';  // don't use display=none, that breaks the popup
                popover.parentElement.style.height = '0px';
                popover.style.left = "-100px";
            }
            if ('sequence' !== track.type) {
                const trackLabelDiv = document.createElement('div');
                trackLabelDiv.title = (typeof track.url === 'string') ? track.url : track.name;
                trackLabelDiv.innerHTML = track.name.replaceAll(".", " ");
                trackLabelDiv.style.position = 'absolute';
                trackLabelDiv.style.fontSize = '12px';
                trackLabelDiv.style.color = '#333';
                trackLabelDiv.style.top = `${top + 10}px`;
                trackLabelDiv.style.left = '5px';
                trackLabelDiv.style.right = '5px';
                trackLabelDiv.style.height = `{track.trackView.viewports[0].viewportElement.clientHeight}px`;
                trackLabelDiv.style.textAlign = 'left';
                trackLabelDiv.style.overflow = 'hidden';

                nameDiv.appendChild(trackLabelDiv);
            }
            top += track.trackView.viewports[0].viewportElement.clientHeight; // Adjust top for the next element
        }
    }

    /**
     * Insert a new row into the image table for the IGV browser.  The igv browser object is a super-track of
     * sorts, containing all igv.js tracks in the session.  In the future we might want to allocate a row
     * for each igv.js track, but this will require some refactoring of igv.js.
     *
     * @returns {HTMLTableRowElement}
     */

    function insertIGVRow() {
        const imgTbl = document.getElementById('imgTbl');
        const tbody = imgTbl.querySelector('tbody');
        const igvRow = document.createElement('tr');
        igvRow.id = "tr_igv";
        igvRow.classList.add('imgOrd', 'trDraggable');
        igvRow.style.position = 'relative'; // For positioning the overlay

        // First cell for drag handle and left sidebar
        const td1 = document.createElement('td');
        td1.className = 'dragHandle';
        td1.style.position = 'relative';
        igvRow.appendChild(td1);

        const leftSidebarDiv = document.createElement('div');
        leftSidebarDiv.id = 'igv_leftsidebar';
        leftSidebarDiv.style.position = "absolute";
        leftSidebarDiv.style.top = "0px";
        leftSidebarDiv.style.bottom = "0px";
        leftSidebarDiv.style.left = "0px";
        leftSidebarDiv.style.right = "0px";
        leftSidebarDiv.style.background = "rgb(193,193,193)";
        td1.appendChild(leftSidebarDiv);

        // Second cell for track names
        const td2 = document.createElement('td');
        td2.style.position = 'relative';
        td2.className = 'dragHandle tdLeft';
        igvRow.appendChild(td2);

        const nameDiv = document.createElement('div');
        nameDiv.id = 'igv_namediv';
        nameDiv.style.position = 'absolute';
        nameDiv.style.top = '0';
        nameDiv.style.bottom = '0';
        nameDiv.style.left = '0';
        nameDiv.style.right = '0';
        td2.appendChild(nameDiv);

        // Third cell for the IGV browser div
        const td3 = document.createElement('td');
        igvRow.appendChild(td3);

        const igvDiv = document.createElement('div');
        igvDiv.id = 'igv_div';
        igvDiv.style.width = 'auto';
        td3.appendChild(igvDiv);

        // Create a transparent overlay for the entire row --currently disabled
        // const overlay = document.createElement('div');
        // overlay.style.position = 'absolute';
        // overlay.style.top = '0';
        // overlay.style.left = '0';
        // overlay.style.width = '100%';
        // overlay.style.height = '100%';
        // overlay.style.backgroundColor = 'rgba(75, 255, 75, 0.2)';
        // overlay.style.display = 'none'; // Initially hidden
        // overlay.style.pointerEvents = 'none'; // Allow clicks to pass through
        // igvRow.appendChild(overlay);
        //
        // // Show/hide overlay on mouseover/mouseout of the left sidebar
        // leftSidebarDiv.addEventListener('mouseenter', () => {
        //     overlay.style.display = 'block';
        // });
        // leftSidebarDiv.addEventListener('mouseleave', () => {
        //     overlay.style.display = 'none';
        // });
        // nameDiv.addEventListener('mouseenter', () => {
        //     overlay.style.display = 'block';
        // });
        // nameDiv.addEventListener('mouseleave', () => {
        //     overlay.style.display = 'none';
        // });
        //

        tbody.appendChild(igvRow);
        return igvRow;
    }

    /**
     * Create an IGV browser instance and insert it into the image table in a new row. The IGV browser essentially becomes
     * a track in the UCSC browser context.  This function is called when the user adds the first IGV track, or
     * the igv session is restored on page reload.
     *
     * @param config -- The IGV browser configuration object.  Must include a reference genome, but might also include
     *                  an initial locus or tracks.
     * @returns {Promise<Browser>}
     */
    async function createIGVBrowser(config) {

        // Override locus  in the IGV session with the UCSC locus
        // TODO -- should we use genomePos here?
        const ucscImageWidth = document.getElementById("td_data_ruler").clientWidth;
        const resolution = (hgTracks.winEnd - hgTracks.winStart) / ucscImageWidth;
        config.locus = {chr: hgTracks.chromName, start: hgTracks.winStart, bpPerPixel: resolution};

        //console.log("Creating IGV browser with config: ", config);

        let igvRow = document.getElementById("tr_igv");
        if (!igvRow) {
            // No existing igv row, insert one
            igvRow = insertIGVRow();
        }


        // Ammend the igv config to remove most of the IGV widgets.  We only want the track display area.
        Object.assign(config, {
            showNavigation: false,
            showIdeogram: false,
            showRuler: false,
            //showSequence: false,   // Uncomment this to hide igv sequence track
            showAxis: false,
            showTrackDragHandles: false,
            showAxisColumn: false,
            gearColumnPosition: 'left',
            showGearColumn: false,
            showTrackLabels: false,   // Uncomment this to hide the "floating div" igv track labels
            formEmbedMode: true,  // works around hotkey issues affecting igv input elements
            disableZoom: true,   // disable zooming in igv, use UCSC zoom controls
            minimumBases: 0     // No lower limit on bases per pixel, this is controlled by UCSC zoom
        });

        const div = document.getElementById("igv_div");
        igvBrowser = await igv.createBrowser(div, config);
        updateTrackNames();

        // Add event handler to remove IGV row from table if all IGV tracks are removed.
        igvBrowser.on('trackremoved', function (track) {

            channel.postMessage({type: "removedTrack", config: track.config});

            const allTracks = igvBrowser.findTracks(t => "sequence" !== t.type);   // ignore sequence track
            if (allTracks.length === 0) {
                igvRow.remove();
                igvBrowser = null;
                updateSessionStorage();
                stopSessionAutoSave(); // stop auto save timer
                delete window.igvBrowser;
            }
            updateTrackNames();
        });

        // Add event handler to track igv.js track panning.  On the UCSC side this should be treated
        // as if the user had dragged a track image
        igvBrowser.on('trackdrag', e => {
                isDragging = true;
                const newStart = igvBrowser.referenceFrameList[0].start;
                igv.ucscTrackpan(newStart);
            }
        );

        // Notify UCSC browser that igv.js track panning has ended.
        igvBrowser.on('trackdragend', () => {
                isDragging = false;
                igv.ucscTrackpanEnd();
            }
        );

        // Repaint name panels as needed
        igvBrowser.on('trackheightchange', () => {
            updateTrackNames();
        });
        igvBrowser.on('tracknamechange', () => {
            updateTrackNames();
        });

        // Start the session auto-save timer.  This will periodically save the igv session to local storage.
        // When / if we can reliably capture IGV state changes we can eliminate this and just save on state change.
        startSessionAutoSave();

        window.igvBrowser = igvBrowser;  // Make available to hgTracks.js code

        return igvBrowser;
    }

    // Respond to messages from the filePicker window.
    channel.onmessage = async function (event) {
        const msg = event.data;
        if (!msg || !msg.type) return;

        switch (msg.type) {

            case MSG.SELECTED_FILES:

                //console.log("Received selected files: ", event.data.files);
                const configs = getTrackConfigurations(event.data.files);
                loadIGVTracks(configs);

                break;
            case MSG.LOAD_URL:
                loadIGVTracks([event.data.config]);
                break;

            // NOTE: The file picker ready message does not appear to be used.
            case MSG.FILE_PICKER_READY:
                const filesToRestore = JSON.parse(sessionStorage.getItem('filesToRestore'));
                if (filesToRestore) {
                    channel.postMessage({type: MSG.RESTORE_FILES, files: filesToRestore});
                    sessionStorage.removeItem('filesToRestore');
                }
                break;
        }
    };

    /**
     * Load one or more tracks into the igvBrowser.  If the igvBrowser does not exist it is created.
     *
     * @param configs
     * @returns {Promise<void>}
     */
    async function loadIGVTracks(configs) {

        if (configs.length > 0) {

            // Create igvBrowser if needed -- i.e. this is the first track being added.  State needs to be obtained
            // from the UCSC browser for genome and locus.
            if (typeof (igvBrowser) === 'undefined' || igvBrowser === null) {
                const defaultConfig = {
                    reference: await getMinimalReference(getDb()),
                    // locus: genomePos.get()
                };
                igvBrowser = await createIGVBrowser(defaultConfig);
            }

            // First search for existing tracks referencing the same files.  This is to handle the situation
            // of a user closing the file picker window, thus loosing file references, then reopening the file picker
            // to restore them.
            const newConfigs = [];

            for (let config of configs) {

                const matchingTracks = igvBrowser.findTracks(t => config.id === t.id);
                if (matchingTracks.length > 0) {
                    // Just select the first matching track, there should only be one.  Restore its file reference(s).
                    matchingTracks[0].config.url.file = config.url.file;
                    if (config.indexURL) {
                        matchingTracks[0].config.indexURL.file = config.indexURL.file;
                    }
                } else {
                    // This is a new track
                    newConfigs.push(config);
                }
            }

            await igvBrowser.loadTrackList(newConfigs);
            updateTrackNames();
            updateSessionStorage();
        }
    }

    /* get first line of text from URL */
    async function getLine(url, {timeoutMs = 10000} = {}) {
        const ctrl = new AbortController();
        const t = setTimeout(() => ctrl.abort(), timeoutMs);

        const res = await fetch(url, {
            headers: {Accept: "text/plain"},
            signal: ctrl.signal,
        }).catch(err => {
            // surface timeouts/aborts as regular errors
            throw new Error(`Request failed: ${err.message}`);
        });
        clearTimeout(t);

        if (!res.ok) throw new Error(`HTTP ${res.status} ${res.statusText}`);

        const text = await res.text();
        // Return a single line (trim and take first line)
        return text.trim().split(/\r?\n/, 1)[0];
    }

    /**
     * Return a minimal reference object for the given genomeID. We don't need or want default IGV tracks, only the
     * reference sequence.
     *
     * Eventually expand or reimplement this function to support all UCSC browser genomes.
     *
     * @param genomeID
     * @returns {{id: string, twoBitURL: string}}
     */
    async function getMinimalReference(genomeID) {
        const currentURL = window.location.href;
        const upOneDirURL = currentURL.substring(0, currentURL.lastIndexOf('/'));
        const apiUrl = upOneDirURL + `/hubApi?cmd=/list/files;genome=${genomeID};format=text;skipContext=1;fileType=2bit`;
        try {
            const twoBitURL = await getLine(apiUrl);
            return {
                "id": genomeID,
                "twoBitURL": twoBitURL,
            };
        } catch (e) {
            console.error(e);
            alert("Internal Error: Cannot get 2bit file from " + apiUrl);
            return null;
        }
    }

    /**
     * Opens a modal dialog with a message and custom buttons.
     * @param {string} message - The HTML message to display.
     * @param {Object} [buttons] - An object where keys are button labels and values are click handler functions.
     */
    function showDialog(message, buttons) {
        $("#igvAlertDialog").html(message);
        const buttonsToShow = buttons || {
            "OK": function () {
                $(this).dialog("close");
            }
        };
        $("#igvAlertDialog").dialog("option", "buttons", buttonsToShow).dialog("open");
    }


    /**
     * Parse a IGV / UCSC style locus string, e.g. "chr1:1000-2000" or "chr1:1000" or "chr1", and return
     * a locus object {chr, start, end}.  The start is 0-based, end is 1-based.
     *
     * @param locusString
     * @returns {{chr: *, start: (number|number), end: (number|*)}}
     */
    function parseLocusString(locusString) {
        const locusRegex = /^([^:]+)(?::(\d+)(?:-(\d+))?)?$/;
        const match = locusString.match(locusRegex);
        if (!match) {
            throw new Error(`Invalid locus string: ${locusString}`);
        }
        const chr = match[1];
        let start = match[2] ? parseInt(match[2].replace(",", ""), 10) - 1 : 0; // Convert to 0-based
        let end = match[3] ? parseInt(match[3].replace(",", ""), 10) : start + 100; // Default to 100bp if no end provided
        if (isNaN(start) || isNaN(end) || start < 0 || end <= start) {
            throw new Error(`Invalid start or end in locus string: ${locusString}`);
        }
        return {chr, start, end};
    }

// Attach helper functions to the igv object
    igv.initIgvUcsc = initIgvUcsc;
    igv.updateIgvStartPosition = updateIgvStartPosition;

})
();
