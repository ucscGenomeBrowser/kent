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
        let igvBrowser = null;
        let igvInitialized = false;
        let isDragging = false;

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
                    throw new Error(`Connection to file ${this.name} is not available.  Please re-select the file.`);
                }
            }
        }


        /**
         * Given a list of files, return a list of track configurations.  Each configuration contains a url (MockFile) and
         * optionally an indexURL (MockFile).
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

            // Now create configurations, matching index files when possible
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
         * a "file" message in response.  If no response is received within 1 second undefined is returned.
         *
         * @param id
         * @returns {Promise<unknown>}
         */
        async function restoreFile(id) {
            return new Promise((resolve, reject) => {

                const previousOnMessage = channel.onmessage;
                const timeoutId = setTimeout(() => {
                    cleanup();
                    console.error(`Timeout waiting for file with id: ${id}`);
                    resolve(undefined);
                }, 1000);

                function cleanup() {
                    channel.onmessage = previousOnMessage;
                    clearTimeout(timeoutId);
                }

                channel.onmessage = function (event) {
                    try {
                        const msg = event.data;
                        if (msg.type === 'file') {
                            cleanup();
                            resolve(msg.data);
                        }
                    } catch (error) {
                        cleanup();
                        console.error(error);
                        resolve(undefined);
                    }
                };

                channel.postMessage({type: 'getFile', id});
            });
        }


        /**
         *  Update the igv.js browser to reflect a change in the UCSC browser start position.  This is called
         *  when the user drags a track image to a new position.  It is intended for small changes, and
         *  works by shifting the igv.js predrawn track image.  This will not work for large position
         *  changes.
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

        function openFilePicker() {
            return window.open('../admin/filePicker.html', 'filePicker' + Date.now(), 'width=600,height=1000');
        }

        // Initialize the embedded IGV browser, restoring state from local storage.
        async function initIgvUcsc() {

            console.log("invoking initIgvUcsc");

            if (window.igvBrowser) {
                console.log("igvBrowser already exists");
                return;
            }

            if (igvInitialized) {
                // Already initialized, do nothing
                return;
            }

            // Retrieve igv session string from local storage.
            // TODO -- in the future this might come from the UCSC session (cart)


            const db = getDb();
            let sessionString = getSessionStorage()[db];

            if (sessionString) {

                // Restore the previously saved igv session, if any.
                const igvSession = JSON.parse(igv.uncompressSession(`blob:${sessionString}`));

                // Reconnect any file-based tracks to the actual File objects.
                if (igvSession.tracks) {

                    const failed = await restoreTrackConfigurations(igvSession.tracks);

                    if (failed.length > 0) {

                        const sendRestoreRequest = () => channel.postMessage({type: "restoreFiles", files: failed});

                        if (filePicker && !filePicker.closed) {
                            sendRestoreRequest();
                            return;
                        }
                        if (filePicker) {
                            // Unexpected: filePicker reference exists but window is closed.
                            alert(
                                `The following file connections could not be restored:\n<ul>${
                                    failed.map(f => `<li>${f.name}</li>`).join('')
                                }</ul>\nTo restore the connection select 'Choose Files' and select the files.`
                            );
                            sendRestoreRequest();
                        } else {

                            // No filePicker, open one
                            filePicker = openFilePicker();
                            filePicker.onload = () => {
                                channel.postMessage({type: "restoreFiles", files: failed});
                                //alert(
                                //    `The following file connections could not be restored:\n<ul>${
                                //        failed.map(f => `<li>${f}</li>`).join('')
                                //    }</ul>\nTo restore the connection select 'Choose Files' and select the files.`
                                //)
                            };
                        }
                    }
                }
                await createIGVBrowser(igvSession);
            }
        };

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
            }
        }

        // Periodically update the igv session in local storage.
        setInterval(updateSessionStorage, 1);

        // Detect a page refresh (visibility change to hidden) and save the session to local storage.  This is meant to
        // simulate  UCSC browser session handling.
        // XX TODO - not enough time for sending an HTTP request to update cart - need a better system!
        // document.onvisibilitychange = () => {
        //     if (document.visibilityState === "hidden") {
        //         if (igvBrowser) {
        //             //setCartVar("igvState", igvSession, null, false);
        //             //const igvSession = igvBrowser.compressedSession();
        //             //localStorage.setItem("igvSession", igvSession);
        //         }
        //     }
        // };

        // The "Add IGV track" button handler.  The button opens the file picker window, unless
        // it is already open in which case it brings that window to the front.  Tracks are added
        // from the filePicker page by selecting track files.
        window.addEventListener("DOMContentLoaded", () => {
            document.getElementById('hgtIgv').addEventListener('click', async function (e) {
                e.preventDefault(); // our
                if (filePicker && !filePicker.closed) {
                    filePicker.focus();
                    return;
                } else {

                    // A filePicker might be open from a previous instance of this page.  We can detect this by sending
                    // a message on the channel and waiting briefly for a response, but we cannot get a reference to the window
                    // so we ask the user to bring it to the front.

                    const responded = await pingFilePicker(channel);
                    if (responded) {
                        alert("File picker is already open. Please switch to that window.");
                    } else {
                        // No filePicker found, open a new one.
                        filePicker = openFilePicker();
                    }
                }
            });
        });


        /**
         * Update the track names in the left hand column of the IGV row in the image table.
         */
        function updateTrackNames() {
            // Add track names to the left hand column.
            const allTracks = igvBrowser.findTracks(t => t.type);
            let top = 0;
            document.getElementById('igv_namediv').innerHTML = ""; // Clear any existing content
            for (let track of allTracks) {
                const trackLabelDiv = document.createElement('div');
                trackLabelDiv.setAttribute('data-track-id', track.id); // Set the track ID attribute
                trackLabelDiv.textContent = track.name; // Set the track name as the label
                trackLabelDiv.style.marginBottom = '5px'; // Optional: Add spacing between labels
                trackLabelDiv.style.position = 'absolute'; // Use absolute positioning
                trackLabelDiv.style.top = `${top}px`; // Position the element at the current value of "top"
                trackLabelDiv.style.right = '5px'; // Set a fixed width for the label div
                trackLabelDiv.style.textAlign = 'right'; // Right-justify the text
                document.getElementById('igv_namediv').appendChild(trackLabelDiv);

                trackLabelDiv.addEventListener('contextmenu', (e) => {
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

                top += track.trackView.viewports[0].viewportElement.clientHeight; // Adjust top for the next element
            }
        }

        function insertIGVRow() {
            // Insert the IGV row into the image table.
            const imgTbl = document.getElementById('imgTbl');
            const tbody = imgTbl.querySelector('tbody');
            const igvRow = document.createElement('tr');
            igvRow.id = "tr_igv";
            igvRow.innerHTML = `
            <td style="background: grey">
                <div style="width:13px"></div>
            </td>
            <td style="position: relative">
                <div id = "igv_namediv" style="width: 140px;position: absolute;top: 0; bottom: 0;"></div>
            </td>
            <td>
                <div id="igv_div" style="width: auto"></div>
            </td>
             `;
            tbody.appendChild(igvRow);
            return igvRow;
        }

        /**
         * Create an IGV browser instance and insert it into the image table as a new row. The IGV browser essentially becomes
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

            console.log("Creating IGV browser with config: ", config);

            if (document.getElementById("tr_igv")) {
                console.warn("IGV track row already exists ???");   // TODO -- how can this happen?
                return;
            }
            const igvRow = insertIGVRow();

            // Ammend the config to remove most of the IGV widgets.  We only want the track display area.
            Object.assign(config, {
                showNavigation: false,
                showIdeogram: false,
                showRuler: false,
                //showSequence: false,
                showAxis: false,
                showTrackDragHandles: false,
                showAxisColumn: false,
                gearColumnPosition: 'left',
                showGearColumn: false,
                showTrackLabels: false,
                formEmbedMode: true,  // triggers key capture in input dialogs
                disableZoom: true,
                minimumBases: 0
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

            window.igvBrowser = igvBrowser;
            return igvBrowser;
        }


        // Respond to messages from the filePicker window.
        channel.onmessage = async function (event) {
            const msg = event.data;
            if (!msg || !msg.type) return;

            switch (msg.type) {

                case MSG.SELECTED_FILES:

                    console.log("Received selected files: ", event.data.files);
                    const configs = getTrackConfigurations(event.data.files);
                    loadIGVTracks(configs);

                    break;
                case MSG.LOAD_URL:
                    loadIGVTracks([event.data.config]);
                    break;

                case MSG.FILE_PICKER_READY:
                    const filesToRestore = JSON.parse(sessionStorage.getItem('filesToRestore'));
                    if (filesToRestore) {
                        channel.postMessage({type: MSG.RESTORE_FILES, files: filesToRestore});
                        sessionStorage.removeItem('filesToRestore');
                    }
                    break;
            }
        };

        async function loadIGVTracks(configs) {


            if (configs.length > 0) {

                // Create igvBrowser if needed -- i.e. this is the first track being added.  State needs to be obtained
                // from the UCSC browser for genome and locus.
                if (typeof (window.igvBrowser) === 'undefined' || window.igvBrowser === null) {
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

        /**
         * Send a "ping" message to the file picker window and wait up to 100 msec for a "pong" response.  Used to
         * determine if a file picker window is already open.
         * @param channel
         * @returns {Promise<unknown>}
         */
        async function pingFilePicker(channel) {
            const waitForResponse = new Promise((resolve) => {
                const originalOnMessage = channel.onmessage;
                channel.onmessage = (event) => {
                    if (event.data && event.data.type === "pong") {
                        channel.onmessage = originalOnMessage;
                        resolve(true);
                    }
                };
                setTimeout(() => {
                    channel.onmessage = originalOnMessage;
                    resolve(false);
                }, 100);
            });

            channel.postMessage({type: "ping"});

            const responded = await waitForResponse;
            return responded;
        }


	/* get first line of text from URL */
        async function getLine(url, { timeoutMs = 10000 } = {}) {
	  const ctrl = new AbortController();
	  const t = setTimeout(() => ctrl.abort(), timeoutMs);

	  const res = await fetch(url, {
	      headers: { Accept: "text/plain" },
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
            const apiUrl = upOneDirURL+`/hubApi?cmd=/list/files;genome=${genomeID};format=text;skipContext=1;fileType=2bit`;
            try {
                const twoBitURL = await getLine(apiUrl);
                return {
                    "id": genomeID,
                    "twoBitURL": twoBitURL,
                };
            } catch (e) {
                console.error(e);
                alert("Internal Error: Cannot get 2bit file from "+ apiUrl);
                return null;
            }
        }

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

    }
)();
