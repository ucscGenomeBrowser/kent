/* jshint esnext: true */
/* global Shepherd */

//Creating an IIFE to prevent global variable conflicts
(function() {
    //Inject CSS directly to make popup bigger
    const style = document.createElement('style');
    style.innerHTML = `
      .shepherd-element.large-popup {
        max-width: 600px;
        width: 600px;
      }
    `;
    document.head.appendChild(style);

    // Code runs immediately and does not pollute the global scope
    const gatewayTour = new Shepherd.Tour({
	defaultStepOptions: {
	    cancelIcon: { enabled: true },
            classes: 'class-1 class-2 dark-background large-popup',
	    scrollTo: { behavior: 'smooth', block: 'center' }
	},
      useModalOverlay: true
    });

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



    // log when a tutorial is started
    gatewayTour.on('start', function() {
        writeToApacheLog("gatewayTutorial start " + getHgsid());
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
        'end': {
            action() {
                hideMenu('#help > ul');
                return this.complete();
            },
            classes: 'shepherd-button-secondary',
            text: 'Finish'
        }
    };


    function gatewaySteps() {
        gatewayTour.addStep({
            title: 'Welcome to the UCSC Genome Browser Gateway tutorial',
            text: 'The UCSC Genome Browser Gateway page is a tool for finding and accessing genome '+
		  'assemblies. This tutorial will guide you through various features of the Gateway '+
		  'page, including: '+
		  '<ul>'+
                  '<li>Finding a genome browser using the Popular Species option </li>'+
                  '<li>Exploring the UCSC Species Tree</li>'+
		  '<li>Searching for an assembly using different identifiers</li>'+
	          '<li>Viewing sequences</li>'+
                  '<li>Using search terms to jump to a specific genome location</li>'+
                  '</ul>',
            buttons: [tutorialButtons.next, tutorialButtons.end],
            id: 'intro',
        });

        gatewayTour.addStep({
            title: 'Using the Popular Species Option',
            text: 'The <b>Popular Species</b> section lists commonly used model organisms, allowing '+
		  'for quick selection of their genome browsers.<br><br>'+
                  '<img src="/images/popular_species.png" width="350"><br>'+
		  'Clicking on a species will display '+
		  'the default assembly version for that organism.',
            attachTo: {
                element: '#popSpeciesTitle',
                on: 'bottom'
            },
            buttons: [tutorialButtons.back , tutorialButtons.next ],
            id: 'PopularSpecies1',
        });

        gatewayTour.addStep({
            title: 'Using the Popular Species Option',
            text: 'To change the assembly version, click the <b>Assembly</b> option under '+
		  '<b>Find Position</b>.<br><br>'+
                  '<img src="/images/assembly_version.png" width="350">',
            attachTo: {
                element: '#selectAssembly',
                on: 'bottom'
            },
            buttons: [tutorialButtons.back , tutorialButtons.next ],
            id: 'selectAssemblyVersion',
        });

        gatewayTour.addStep({
            title: 'Using the search box',
            text: 'The search box allows users to find genome assemblies by entering different '+
                  'types of queries:'+
                  '<ul>'+
                  '<li>Searching by <b>species name</b>: Ovis aries</li>'+
                  '<li>Searching by <b>common name</b>: dog</li>'+
                  '<li>Searching by <b>GC accession number</b>: GCF_016699485</li>'+
		  '</ul>',
            attachTo: {
                element: '#speciesSearch',
                on: 'bottom'
            },
            buttons: [tutorialButtons.back , tutorialButtons.next ],
            id: 'speciesSearchBox',
        });

        gatewayTour.addStep({
            title: 'Exploring the UCSC Species Tree',
            text: 'The <b>Species Tree</b> option displays a phylogenetic tree that can be navigated '+
                  'using scrolling or by clicking different parts of the tree.  Hovering over '+
                  'a branch will reveal the lineage branch name.<br><br>'+
		  '<b>The Species Tree does not include all available Genome Browser assemblies.</b> '+
		  'The process of adding Genome Browsers has been streamlined, enabling rapid releases but '+
		  'omitting certain previous features, such as inclusion in the Species Tree. To find a '+
		  'specific assembly, use the assembly search box.',
            attachTo: {
                element: '#representedSpeciesTitle',
                on: 'top'
            },
            buttons: [tutorialButtons.back , tutorialButtons.next ],
            id: 'speciesTree',
        });

        gatewayTour.addStep({
            title: 'Requesting an Assembly',
            text: 'If a desired assembly is not listed, you can request it by clicking '+
		  '<a href="/assemblySearch.html" target="_blank">Unable to find a genome? Send us a '+
		  'request.</a> This will direct you to the <b>Genome Assembly Search and Request '+
		  'page</b>.<br><br>Steps to request an assembly: '+
		  '<ol>'+
		  '<li>Enter the <b>species name, common name,</b> or <b>GC accession number</b> of the assembly.</li>'+
		  '<img src="/images/assembly_request.png" height="200" width="500">'+
		  '<li>Click <button>request</button>.</li>'+
		  '<li>Fill out the required information on the submission page.</li>'+
	  	  '<img src="/images/request_page.png" width="200">',
            attachTo: {
                element: '#assemblySearchLink',
                on: 'right'
            },
            buttons: [tutorialButtons.back , tutorialButtons.next ],
            id: 'requestingAssembly',
        });

        gatewayTour.addStep({
            title: 'Using View sequences',
            text: 'Clicking the <b>View sequences</b> link directs users to the '+
		  '<b>Assembly Browser Sequences</b> page.'+
                  '<img src="/images/sequences_list.png" width="575">'+
                  'This page displays information about chromosomes, sequences, and '+
		  'contigs for the selected assembly.<br><br>'+
		  'The third column displays alias sequence names, while subsequent columns show '+
		  'alternate naming schemes. These include:'+
                  '<ul>'+
                  '<li><b>assembly</b> - Names from NCBI&apos;s assembly_report.txt file.</li>'+
                  '<li><b>genbank</b> - INSDC names.</li>'+
                  '<li><b>refseq</b> - Names from RefSeq annotations.</li>'+
                  '</ul>',

            attachTo: {
                element: '#chromInfoLink',
                on: 'left'
            },
            buttons: [tutorialButtons.back , tutorialButtons.next ],
            id: 'viewSequences',
        });

        gatewayTour.addStep({
            title: 'Assembly details on the Gateway Page',
            text: 'The <b>Gateway Page</b> provides various details about an assembly, including:'+
                  '<ul>'+
                  '<li>UCSC Genome Browser assembly ID</li>'+
                  '<li>Common name</li>'+
                  '<li>Taxonomic name</li>'+
                  '<li>Sequencing/Assembly provider ID</li>'+
                  '<li>Assembly date</li>'+
                  '<li>Assembly type</li>'+
                  '<li>Assembly level</li>'+
                  '<li>Biosample</li>'+
                  '<li>Assembly accession</li>'+
                  '<li>NCBI Genome ID</li>'+
                  '<li>NCBI Assembly ID</li>'+
                  '<li>BioProject ID</li>'+
                  '</ul>'+
		  'The Gateway page also offers <b>download links</b> for data files related '+
		  'to the genome assembly. ',

            attachTo: {
                element: '#descriptionText',
                on: 'left'
            },
            buttons: [tutorialButtons.back , tutorialButtons.next ],
            id: 'assemblyDetails',
        });

        gatewayTour.addStep({
            title: 'Jumping to a Specific Genome Location',
            text: 'Once an assembly is selected, users can search for specific genome locations '+
		  'using:'+
                  '<ul>'+
                  '<li>Genome positions (e.g., chr1:1000000-2000000)</li>'+
                  '<li>Gene names (e.g., BRCA1)</li>'+
                  '</ul>'+
                  'For more details on valid position queries, visit the '+
		  '<a href="/goldenPath/help/query.html" target="_blank">Querying the Genome Browser</a> page.',

            attachTo: {
                element: '#positionInput',
                on: 'bottom'
            },
            buttons: [tutorialButtons.back , tutorialButtons.next ],
            id: 'searchLocations',
        });

        gatewayTour.addStep({
            title: 'Additional resources and feedback',
            text: 'If you have any questions or suggestions, '+
                  '<a href="/contacts.html" target="_blank">please write to us</a>. '+
                  '<br><br>'+
                  'Also, if you are new to the '+
                  'UCSC Genome Browser, consider exploring our '+
                  '<a href="/cgi-bin/hgTracks?startTutorial=true" '+
                  'id="basicTutorialLink" '+
                  'target="_blank">basic tutorial</a>.',
            attachTo: {
                element: '#contactUs',
                on: 'right'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.end ],
            when: {
                show: () => {
                    keepMenuVisible('#help > ul');
                    // log when the tutorial is finished (commented out for now)
                    writeToApacheLog("gatewayTutorial finish " + getHgsid());
                },
                hide: () => hideMenu('#help > ul')
            }
        });
    }
	    
    if (typeof window.gatewayTour === 'undefined') {
        gatewaySteps();
        window.gatewayTour = gatewayTour;
    }
})();

