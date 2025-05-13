/* jshint esnext: true */
/* global Shepherd */

//Creating an IIFE to prevent global variable conflicts
(function() {
    const tableBrowserTour = new Shepherd.Tour({
        defaultStepOptions: {
            cancelIcon: { enabled: true },
            classes: 'class-1 class-2',
            scrollTo: { behavior: 'smooth', block: 'center' }
        },
      useModalOverlay: true,
      canClickTarget: false
    });
    
    // log when a tutorial is started (commented out for now)
    tableBrowserTour.on('start', function() {
        writeToApacheLog("tableBrowserTutorial start " + getHgsid());
    });
    
    var tutorialButtons = {
        'back': {
            action() {
                return this.back();
            },
            classes: 'shepherd-button-secondary',
            text: 'Back'
        },
        'next': {
            action() {
                return this.next();
            },
            text: 'Next'
        },
        'quit': {
            action() {
                return this.complete();
            },
            classes: 'shepherd-button-secondary',
            text: 'Exit'
        },
        'end': {
            action() {
                hideMenu('#help > ul');
                return this.complete();
            },
            //classes: 'shepherd-button-secondary',
            text: 'Finish'
        }
    };
    // Function to disable drop-downs, button clicks, and text inputs
    function toggleSelects(containerId, enabled) {
        const container = document.getElementById(containerId);

        const selectors = ['select', 'button', 'input[type="radio"]',
                           'input[type="submit"]', 'input[type="checkbox"]',
                           'input[type="text"]'
                          ];
        container.querySelectorAll(selectors).forEach(sel => {
            if (enabled) {
                sel.style.pointerEvents = '';
                sel.style.opacity = '';
                sel.tabIndex = 0;
            } else {
                sel.style.pointerEvents = 'none'; // blocks mouse interaction
                sel.style.opacity = '1';          // keep visual styling normal
                sel.tabIndex = -1;                // skip from keyboard nav
            }
        });

    }

    // Function to keep menus visible using a selector
    function keepMenuVisible(selector) {
        const menu = document.querySelector(selector);
        //Make sure the drop-down is visibile
        menu.style.display = 'block';
        menu.style.visibility = 'visible';
        // function to keep the menu visibile
        const makeVisible = () => {
            menu.style.display = 'block';
            menu.style.visibility = 'visible';
        };
        const events = ['mouseover', 'mouseout', 'mouseenter', 'mouseleave', 'mousemove'];
        // Add event listeners to keep the menu open
        events.forEach(event => {
            menu.addEventListener(event, makeVisible);
        });
        // Add event listeners to the elements of the menu list
        menu.querySelectorAll('li').forEach(function(item) {
            events.forEach(event => {
                item.addEventListener(event, makeVisible);
            });
        });
    }
    
    // Function to hide the menu
    function hideMenu(selector) {
        const menu = document.querySelector(selector);
        menu.style.display = 'none';
        menu.style.visibility = 'hidden';

        const hideVisible = () => {
            menu.style.display = 'none';
            menu.style.visibility = 'hidden';
        };

        const events = ['mouseover', 'mouseout', 'mouseenter', 'mouseleave', 'mousemove'];
        // Remove event listeners to keep the menu open
        events.forEach(event => {
             menu.removeEventListener(event, hideVisible);
        });
        menu.querySelectorAll('li').forEach(function(item) {
            events.forEach(event => {
                item.removeEventListener(event, hideVisible);
            });
        });
    }

    
    // Function to add steps to the tableBrowserTour
    function tableBrowserSteps() {
        tableBrowserTour.addStep({
            title: 'Navigating the Table Browser',
            text: 'Welcome to an introductory tutorial to the Table Browser. '+
                  '<br><br>'+
                  'In this tutorial, we '+
                  'will explore how to configure the Table Browser, and output your results '+
                  'in the various formats available for a track.',
            buttons: [tutorialButtons.quit, tutorialButtons.next],
            id: 'intro'
        });
    
        tableBrowserTour.addStep({
            title: '<b>Step 1:</b> &nbsp;Select your assembly',
            text: 'To begin, select the genome you want to use on the Table Browser. By default, '+
                  'your most recently used assembly will be selected.'+
                  '<br><br>'+
                  'If you wish to change the assembly, start by altering the:'+
                  '<ul>'+
                  '<li><b>Clade</b> to select another domain of life</li>'+
                  '<li><b>Genome</b> to change the organism</li>'+
                  '<li><b>Assembly</b> to specify the assembly version</li>'+
                  '</ul>',
            attachTo: {
                element: '#genome-select',
                on: 'bottom'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            id: 'genome_select',
            when: {
                show: () => toggleSelects('genome-select', false),
                hide: () => toggleSelects('genome-select', true)
            }
        });
    
        tableBrowserTour.addStep({
            title: '<b>Step 2:</b> &nbsp; Select the track for your assembly',
            text: 'The Table Browser automatically uses your most recently viewed track. '+
                  'However, you can change the track by changing these drop-down menus. '+
                  'Tracks are grouped the same way as they are on the main Genome Browser page.'+
                  '<br><br>'+
                  'If you are having a hard time finding your track, there is an "All Tracks" '+
                  'and an "All Tables" option under the Group drop-down menu.',
            attachTo: {
                element: '#track-select',
                on: 'bottom-start'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            id: 'track_select',
            when: {
                show: () => toggleSelects('track-select', false),
                hide: () => toggleSelects('track-select', true)
            }
        });

        tableBrowserTour.addStep({
            title: '<b>Step 3:</b> &nbsp; Select the primary or related table',
            text: 'Finally, select the table associated with the track to extract the data. '+
                  'Depending on the track, there may be more than one table associated with the '+
                  'track. '+
                  '<br><br>'+
                  'For more information about the table, use the '+
                  '<button>Data format description</button> '+
                  'to learn more about the table format, connected tables, and joining fields.',
            attachTo: {
                element: '#table-select',
                on: 'bottom-start'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            id: 'track_select',
            when: {
                show: () => toggleSelects('table-select', false),
                hide: () => toggleSelects('table-select', true)
            }
        });

        tableBrowserTour.addStep({
            title: '<b>Step 4:</b> &nbsp; Select your region of interest',
            text: 'You can limit the output to a specific position in the genome assembly, or '+
                  'retrieve output for the entire genome. <em>Please note, due to some data '+
                  'restrictions, output for the entire genome may not be available '+
                  'for a given a track.</em>'+
                  '<br><br>'+
                  'When entering a position in the text box, you can use coordinates or an '+
                  'identifier (i.e. gene symbol) to select your region.'+
                  'If you are using an identifier, use the <button>Lookup</button> to update the ' +
                  'position.'+
                  '<br><br>'+
                  'The <button>Define regions</button> button allows you to paste/upload a set '+
                  'of coordinates or BED lines.',
            attachTo: {
                element: '#position-controls',
                on: 'bottom'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            id: 'region',
            when: {
                show: () => toggleSelects('position-controls', false),
                hide: () => toggleSelects('position-controls', true)
            }
        });

        tableBrowserTour.addStep({
            title: '<b>Optional step:</b> &nbsp; Filter for items using identifiers',
            text: 'Use this setting if you have a set of IDs, such as gene symbols, that you ' +
                  'want to extract from a dataset. These IDs <b>must</b> match the <em>name</em> '+
                  'field in the selected table.'+
                  '<br><br>'+
                  '<em>'+
                  'Please note: use the "Genome" option for the region if your '+
                  'IDs are on different chromosomes, otherwise the output will be limited to the '+
                  'postion selcted in the previous step.'+
                  '</em>',
            attachTo: {
                element: '#identifiers-controls',
                on: 'bottom-start'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            id: 'identifiers',
            when: {
                show: () => toggleSelects('identifiers-controls', false),
                hide: () => toggleSelects('identifiers-controls', true)
            }
        });

        tableBrowserTour.addStep({
            title: '<b>Step 5:</b> &nbsp; Select the output format',
            text: 'By default, all fields from the table will be returned. If you are only '+
                  'interested in a few columns, converting to a file format, or creating a '+
                  'custom track from the data, alter this drop-down option.'+
                  '<br><br>'+
                  '<img src="/images/tutorialImages/tableBrowserOutputDropDown.png" width="350">' +
                  '<br>'+
                  'Alternatively, you can also send the Table Browser output to Galaxy or GREAT. ',
            attachTo: {
                element: '#output-select',
                on: 'bottom-start'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            id: 'output_selection',
            when: {
                show: () => toggleSelects('output-select', false),
                hide: () => toggleSelects('output-select', true)
            }
        });

        tableBrowserTour.addStep({
            title: '<b>Optional step:</b> &nbsp; Save output to a file',
            text: 'By default, output is printed to the screen in your web browser. Enter a '+
                  'filename to this dialogue box to have the output save to a '+
                  'file.',
            attachTo: {
                element: '#filename-select',
                on: 'bottom-start'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            id: 'filename_selection',
            when: {
                show: () => toggleSelects('filename-select', false),
                hide: () => toggleSelects('filename-select', true)
            }
        });

        tableBrowserTour.addStep({
            title: '<b>Step 6:</b> &nbsp; Submit your selections to get the output',
            text: 'Finally, to submit your selections and retrieve your output, click the '+
                  '<button>Get output</button> button.'+
                  '<br><br>'+
                  'There is also a <button>Summary/statistics</button> button that will give you '+
                  'information such as item count, smallest/biggest item, bases in region, '+
                  'load/calculation time, and more.',
            attachTo: {
                element: '#submit-select',
                on: 'bottom-start'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            id: 'get_output',
            when: {
                show: () => toggleSelects('submit-select', false),
                hide: () => toggleSelects('submit-select', true)
            }
        });

        tableBrowserTour.addStep({
            title: 'Additional resources and documentation',
            text: 'For further examples of using the ' +
                  'Table Browser, please read the <a href="/goldenPath/help/hgTablesHelp.html" '+
                  'target="_blank">Table Browser user guide</a>. You can find examples of batch '+
                  'queries, filtering on fields from tables, video examples, and more. '+
                  '<br><br>'+
                  'You can also <a href="/contacts.html" target="_blank">contact us</a> if you '+
                  'have any questions or issues using a dataset '+
                  'on the Table Browser.',
            attachTo: {
                element: '#hgTablesHelp',
                on: 'left-start'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.end ],
            id: 'additionalHelp',
            when: {
                show: () => {
                    keepMenuVisible('#help > ul');
                    writeToApacheLog("tableBrowserTutorial finish " + getHgsid());
                },
                hide: () => hideMenu('#help > ul')
            }
        });
    }

    if (typeof window.tableBrowserTour === 'undefined') {
        tableBrowserSteps();
        //Export the tableBrowserTour globalally
        window.tableBrowserTour = tableBrowserTour;
    }
})();
