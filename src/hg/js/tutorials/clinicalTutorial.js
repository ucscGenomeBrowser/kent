/* jshint esnext: true */
/* global Shepherd */

//Creating an IIFE to prevent global variable conflicts
(function() {
    const clinicalTour = new Shepherd.Tour({
        defaultStepOptions: {
            cancelIcon: { enabled: true },
            classes: 'class-1 class-2',
            scrollTo: { behavior: 'smooth', block: 'center' }
        },
      useModalOverlay: true
    });
    
    // log when a tutorial is started (commented out for now)
    clinicalTour.on('start', function() {
        writeToApacheLog("clinicalTutorial start " + getHgsid());
    });
    
    var tutorialButtons = {
        'back': {
            action() {
                return this.back();
            },
            classes: 'shepherd-button-secondary',
            text: 'Back'
        },
        'nextRTS': {
            action() {
                showPopup();
                return this.next();
            },
            text: 'Next'
        },
        'next': {
            action() {
                return this.next();
            },
            text: 'Next'
        },
        'end': {
            action() {
                // log when the tutorial is finished (commented out for now)
                writeToApacheLog("clinicalTutorial finish " + getHgsid());
                hideMenu('#help > ul');
                return this.complete();
            },
            classes: 'shepherd-button-secondary',
            text: 'Finish'
        }
    };
    
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

    
    // Function to show the popup
    function showPopup() {
        const rtsMenuItem = document.querySelector('#tools2 #recTrackSetsMenuItem');
        rtsMenuItem.click();
        const titleSpan = document.querySelector('#ui-dialog-title-recTrackSetsPopup');
        titleSpan.textContent = 'Recommended Track Sets';
    }
    
    // Function to close the popup
    function closePopup() {
        const rtsMenuClose = document.querySelector('.ui-dialog-titlebar-close');
        rtsMenuClose.click();
    }
    
    // Function to log finishing the clinical tutorial
    function listenForBasicTutorial() {
        document.getElementById('basicTutorialLink').addEventListener('click', function() {
            writeToApacheLog("clinicalTutorial finish " + getHgsid());
            clinicalTour.complete();
        });
    }

    // Function to add steps to the clinicalTour
    function clinicalSteps() {
        clinicalTour.addStep({
            title: 'Clinical Genetics in the UCSC Genome Browser',
            text: 'This brief tutorial will cover the primary resources used in standard variant '+
                  'interpretation available to clinical geneticists as per the ACMG/AMP guidelines.'+
                  '<br><br>'+
                  'A link to restart this tutorial is available below:'+
                  '<br>'+
                  '<em>'+
                  '<a href="/cgi-bin/hgTracks?startClinical=true" style="font-size: 14px" '+
                  'target="_blank">https://genome.ucsc.edu/cgi-bin/hgTracks?startClinical=true</a></em>',
            buttons: [tutorialButtons.next, tutorialButtons.end],
            id: 'intro',
            classes: 'dark-background'
        });
    
        clinicalTour.addStep({
            title: 'Searching for Variants and other Queries',
            text: 'The search bar allows you to navigate to a region on the genome using ' +
                  '<a href="http://varnomen.hgvs.org/" target="_blank">HGVS terms</a>, '+
                  '<a href="https://genome-blog.soe.ucsc.edu/blog/2016/12/12/the-ucsc-genome-browser-coordinate-counting-systems/"' +
                  'target="_blank">genome coordinates</a>, <a href="/FAQ/FAQgenes.html#genename" ' +
                  'target="_blank">gene symbols</a>, and specific annotation IDs such as NM '+
                  'identifiers, rsIDs, and more.'+
                  '<br><br>'+
                  'A few example queries are: ' +
                  '<ul>' +
                  '<li>rs2569190</li>' +
                  '<li>NM_198056.3</li>' +
                  '<li>NM_198056.3:c.1654G>T</li>' +
                  '</ul>',
            attachTo: {
                element: '#positionInput',
                on: 'bottom'
            },
            buttons: [tutorialButtons.back , tutorialButtons.next ],
            id: 'search',
            classes: 'dark-background'
        });
    
        clinicalTour.addStep({
            title: 'Accessing the Recommended Track Sets',
            text: 'Located under the "Genome Browser" drop-down menu, the <b>Recommended Track '+
                  'Sets</b> feature can assist in configuring the display with relevant '+
                  'annotations for variant interpretation. '+
                  '<a href="https://www.ncbi.nlm.nih.gov/pubmed/35088925" '+
                  'target="_blank">[publication]</a>'+
                  '<br><br>'+
                  'Selecting this option will launch a dialog box offering '+
                  'pre-configured track sets, enabling swapping from one view to another view '+
                  'without changing the current position.'+
                  '<br><br>'+
                  '<em>Currently only available on hg38 and hg19.</em>',
            attachTo: {
                element: '#recTrackSetsMenuItem',
                on: 'right'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.nextRTS ],
            id: 'rtsDropDown',
            classes: 'dark-background',
            when: {
                show: () => keepMenuVisible('#tools2 > ul'),
                hide: () => hideMenu('#tools2 > ul')
            }
        });
    
        clinicalTour.addStep({
            title: 'Display Annotations for a Specific Scenario',
            text: 
                  '<em>Note: Loading a track set may hide some tracks in your current view.</em>'+
                  '<br><br>'+
                  'Each available track set loads a display curated to the specific theme. '+
                  'The <b>Clinical SNVs</b> and <b>Clinical CNVs</b> track sets are '+
                  'routinely updated to include existing and new annotations available on the '+
                  'Genome Browser used in variant interpretation according to ACMG/AMP '+
                  'and ClinGen guidelines.'+
                  '<br><br>'+
                  'To use these track sets as your default view, bookmark these links:'+
                  '<ul>'+
                  '  <li>Clinical SNVs - <a target="_blank" '+
                  '   href="/cgi-bin/hgTracks?db=hg38&hgS_otherUserName=View&rtsLoad=Clinical_SNVs_hg38">hg38</a>,'+
                  '   <a target="_blank" href="/cgi-bin/hgTracks?db=hg19&hgS_otherUserName=View&rtsLoad=SNVs%20Clinical">hg19</a></li>'+
                  '  <li>Clinical CNVs - <a target="_blank" '+
                  '   href="/cgi-bin/hgTracks?db=hg38&hgS_otherUserName=View&rtsLoad=Clinical_CNVs_hg38">hg38</a>,'+ 
                  '   <a target="_blank" href="/cgi-bin/hgTracks?db=hg19&hgS_otherUserName=View&rtsLoad=CNVs%20Clinical">hg19</a></li>'+
                  '</ul>',
            attachTo: {
                element: '.ui-dialog[aria-labelledby="ui-dialog-title-recTrackSetsPopup"]',
                on: 'right'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            id: 'trackSets',
            classes: 'dark-background',
            when: {
                show: showPopup,
                hide: closePopup
            }
                
        });

        clinicalTour.addStep({
            title: 'Search for Available Datasets',
            text:
                  '<a href="/cgi-bin/hgTracks?hgt_tSearch=track+search" target="_blank">Track '+
                  'search</a> through all available annotations for any '+
                  'assembly. '+
                  'The track search feature provides users with two search options, "Search" and '+
                  '"Advanced."'+
                  '<br><br>'+
                  '<img src="/images/tutorialImages/trackSearch.png" width="350">' +
                  '<br><br>'+
                  'If multiple terms are entered, only tracks with all terms will be '+
                  'part of the results. We do not currently support either/or searching (e.g. a '+
                  'OR b).',
            attachTo: {
                element: '#trackSearchMenuLink',
                on: 'right'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            when: {
                show: () => keepMenuVisible('#tools2 > ul'),
                hide: () => hideMenu('#tools2 > ul')
            }
        });

        clinicalTour.addStep({
            title: 'Associate Annotations to Variant Calls',
            text:
                  '<a href="/cgi-bin/hgVai" target="_blank">Variant Annotation Integrator</a> '+
                  '(VAI) provides effect prediction and annotation associations for '+
                  '      variant calls.'+
                  '<br><br>'+
                  'VAI can optionally add several other types of relevant information: '+
                  '<ul>'+
                  '  <li>The dbSNP identifier when available</li>'+
                  '  <li>Protein damage scores for missense variants from the Database of '+
                  '      Non-synonymous Functional Predictions (dbNSFP)</li>'+
                  '  <li>Conservation scores computed from multi-species alignments.</li>'+
                  '</ul>'+
                  'VAI can also filter results to retain only specific functional effect '+
                  'categories, variant properties and multi-species conservation status.',
            attachTo: {
                element: '#vaiMenuLink',
                on: 'right'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            when: {
                show: () => keepMenuVisible('#tools3 > ul'),
                hide: () => hideMenu('#tools3 > ul')
            }
        });

        clinicalTour.addStep({
            title: 'Save and Share Snapshots of the Browser',
            text:
                  '<a href="/cgi-bin/hgSession" target="_blank">Sessions</a> allows you to '+
                  '      save your own pre-configured displays and create stable links. '+
                  '<br><br>'+
                  'Multiple sessions may be saved for future reference, for comparing different '+
                  'data sets, or for sharing with your colleagues. '+
                  '<br><br>'+
                  '<em>Saved sessions will not be '+
                  'expired, however we still recommend that you keep local back-ups of your '+
                  'session contents and any associated custom tracks.</em>',
            attachTo: {
                element: '#sessionsMenuLink',
                on: 'right'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            when: {
                show: () => keepMenuVisible('#myData > ul'),
                hide: () => hideMenu('#myData > ul')
            }
        });

        clinicalTour.addStep({
            title: 'View this Region in Another Genome (Convert)',
            text:
                  '<a href="/cgi-bin/hgConvert" target="_blank">'+
                  'The Genome Browser Convert utility</a> is useful for locating '+
                  'the position of a feature of interest in a different release of the same '+
                  'genome or (in some cases) in a genome assembly of another species.'+
                  '<br><br>'+
                  'When coordinate conversion is available for an assembly, '+
                  'you will be presented with a list of the '+
                  'genome/assembly conversion options available.'+
                  '<br><br>'+
                  'If the conversion is successful, the '+
                  'browser will return a list of regions in the new assembly, along with the '+
                  'percent of bases and span covered by that region. '+
                  'If the conversion is unsuccessful, the utility returns a failure message.',
            attachTo: {
                element: '#convertMenuLink',
                on: 'right'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            when: {
                show: () => keepMenuVisible('#view > ul'),
                hide: () => hideMenu('#view > ul')
            }
        });

        clinicalTour.addStep({
            title: 'Add Color to the Region',
            text:
                  'Our <a href="/goldenPath/help/hgTracksHelp.html#Highlight" '+
                  '      target="_blank">highlight</a> feature can accentuate specific regions '+
                  '      in your view. '+
                  'Highlight the current position with the keyboard shortcut "h then m".'+
                  '<br><br>'+
                  '<img src="/images/right_click_example.png" width="350">' +
                  '<br>'+
                  '<em>You can also right-click on an item to highlight a feature.</em>',
            attachTo: {
                element: '#highlightHereMenu',
                on: 'right'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.next ],
            when: {
                show: () => keepMenuVisible('#view > ul'),
                hide: () => hideMenu('#view > ul')
            }
        });

        clinicalTour.addStep({
            title: 'Additional Resources and Feedback',
            text: 'If you have any questions or suggestions including annotations '+
                  'that you feel are missing from a track set or a new track set theme, '+
                  '<a href="/contacts.html" target="_blank">please write to us</a>. '+
                  '<br><br>'+
                  'Also, if you are new to the '+
                  'UCSC Genome Browser, consider exploring our '+
                  '<a href="/cgi-bin/hgTracks?startTutorial=true" '+
                  'id="basicTutorialLink" '+
                  '>basic tutorial</a>.',
            attachTo: {
                element: '#contactUs',
                on: 'right'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.end ],
            when: {
                show: () => {
                    keepMenuVisible('#help > ul');
                    listenForBasicTutorial(); // listener to log if the basic tutorial was started
                },
                hide: () => hideMenu('#help > ul')
            }
        });
    }

    clinicalSteps();

    //Export the clinicalTour globalally
    window.clinicalTour = clinicalTour;
})();
