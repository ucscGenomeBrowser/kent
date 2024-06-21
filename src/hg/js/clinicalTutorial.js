/* jshint esnext: true */
/*jslint unused: false */
/* global Shepherd */



const tour = new Shepherd.Tour({
  defaultStepOptions: {
    cancelIcon: {
      enabled: true
    },
    classes: 'class-1 class-2',
    scrollTo: { behavior: 'smooth', block: 'center' }
  },
  useModalOverlay: true
});

// log when a tutorial is started (commented out for now)
//tour.on('start', function() {
//    writeToApacheLog("clinical start " + getHgsid());
//});

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
            // log when the tutorial is finished (commented out for now)
            //writeToApacheLog("clinical finish " + getHgsid());
            //localStorage.setItem("hgTracks_hideTutorial", "1");
            return this.complete();
        },
        classes: 'shepherd-button-secondary',
        text: 'Finish'
    }
};

// Function to keep the menu visible
function keepMenuVisible() {
    const toolsMenu = document.querySelector('#tools2 > ul');
    toolsMenu.style.display = 'block';
    toolsMenu.style.visibility = 'visible';
}

// Function to show the popup
function showPopup() {
    const recTrackSetsPopup = document.querySelector('.ui-dialog[aria-labelledby="ui-dialog-title-recTrackSetsPopup"]');
    if (recTrackSetsPopup) recTrackSetsPopup.style.display = 'block';
}

// Function to close the popup
function closePopup() {
  const popupDialog = document.querySelector('.ui-dialog[aria-labelledby="ui-dialog-title-recTrackSetsPopup"]');
  if (popupDialog) popupDialog.style.display = 'none';
}

// Function to add steps to the tour
function setupSteps() {
    tour.addStep({
        title: 'A Brief Introduciton for Clinical Genetists',
        text: 'This tutorial is for clinicians and is no where near being complete in terms of '+
              'content but should be fully functioning without any bugs (hopefully). ',
        buttons: [tutorialButtons.next, tutorialButtons.end],
        id: 'intro',
        classes: 'dark-background'
    });

    tour.addStep({
        title: 'Browsing the Genome',
        text: 'The search bar allows you to navigate to a region on the genome using ' +
              '<a href="https://genome-blog.soe.ucsc.edu/blog/2016/12/12/the-ucsc-genome-browser-coordinate-counting-systems/"' +
              'target="_blank">genome coordinates</a>, <a href="/FAQ/FAQgenes.html#genename" ' +
              'target="_blank">gene symbols</a>, <a href="https://www.ncbi.nlm.nih.gov/snp/docs/RefSNP_about/#what-is-a-reference-snp" ' +
              'target="_blank">rsIDs</a>, <a href="http://varnomen.hgvs.org/" ' +
              'target="_blank">HGVS</a> terms.<br><br> '+
              'A few example queries are: ' +
              '<ul>' +
              '<li>chr1:127140001-127140001</li>' +
              '<li>SOD1</li>' +
              '<li>rs2569190</li>' +
              '<li>NM_198056.3:c.1654G>T</li>' +
              '</ul>' +
              'The <a href="/goldenPath/help/query.html" target="_blank">Querying the Genome '+
              'Browser</a> help page contains other search term examples that are available on '+
              'the UCSC Genome Browser.',
        attachTo: {
            element: '#positionInput',
            on: 'bottom'
        },
        buttons: [tutorialButtons['back'], tutorialButtons['next']],
        id: 'search',
        classes: 'dark-background'
    });

    tour.addStep({
        title: 'Accessing the Recommended Track Sets',
        text: 'The <b>Recommended Track Sets</b> feature is available under the "Genome Browser" '+
              'drop-down menu. <br><br>'+
              'Selecting this option will launch a dialog box offering '+
              'pre-configured track sets, enabling swapping from one view to another view without '+
              'changing the current position.'+
              '<br><br><em>Currently only available on hg38 and hg19.</em>',
        attachTo: {
            element: '#recTrackSetsMenuItem',
            on: 'right'
        },
        buttons: [ tutorialButtons.back, {
                     text: 'Next',
                     action: function() {
                         const rtsMenuItem = document.querySelector('#tools2 #recTrackSetsMenuItem');
                         rtsMenuItem.click();
                         tour.next();
                         }
                  }],
        id: 'rtsDropDown',
        classes: 'dark-background',
        when: {
            show: () => {
                const toolsMenu = document.querySelector('#tools2 > ul');
                toolsMenu.style.display = 'block';
                toolsMenu.style.visibility = 'visible';

                toolsMenu.addEventListener('mouseover', keepMenuVisible);
                toolsMenu.addEventListener('mouseout', keepMenuVisible);
            },
            hide: () => {
                const toolsMenu = document.querySelector('#tools2 > ul');
                toolsMenu.style.display = 'none';
                toolsMenu.style.visibility = 'hidden';

                toolsMenu.removeEventListener('mouseover', keepMenuVisible);
                toolsMenu.removeEventListener('mouseout', keepMenuVisible);
            }
        }
    });

    tour.addStep({
        title: 'Pre-configured Track Sets',
        text: 'Track Sets allow a user to quickly swap out the on-screen annotations they may '+
              'be looking at for a different set of tracks relevant to specific medical scenarios.'+
              '<br><br>Track sets are updated  XXXXXXXXXX'+
              '<br><br><em>This tool is for research use only.</em>',
        attachTo: {
            element: '.ui-dialog[aria-labelledby="ui-dialog-title-recTrackSetsPopup"]',
            on: 'right'
        },
        buttons: [
            {
                text: 'Back',
                action: () => {
                    tour.back();
                    closePopup();
                }
            },
            {
                text: 'Next',
                action: () => {
                    tour.next();
                    closePopup();
                }
            }
        ],
        id: 'trackSets',
        classes: 'dark-background',
        when: {
            show: showPopup
        }
            
    });
    /*
    tour.addStep({
        title: 'Clinical SNVs',
        text: 'Assess potential disease contributions of single nucleotide variants in coding regions.',
        attachTo: {
            element: '#recTrackSetsPopup ul li:nth-child(1)',
            on: 'right'
        },
        buttons: [ tutorialButtons.back, tutorialButtons.next ],
        id: 'SNVs',
        classes: 'dark-background',
        when: {
            show: showPopup
        }
    });

    tour.addStep({
        title: 'Clinical CNVs',
        text: 'Assess potential disease contributions of structural variants in coding regions.',
        attachTo: {
            element: '#recTrackSetsPopup ul li:nth-child(2)',
            on: 'right'
        },
        buttons: [ tutorialButtons.back,
            {
                text: 'Next',
                action: () => {
                    tour.next();
                    closePopup();
                }
            }
        ],
        id: 'CNVs',
        classes: 'dark-background',
        when: {
            show: showPopup
        }
    });
    */

    tour.addStep({
        title: 'The final step',
        text: 'Some text for the final step.',
        buttons: [
            tutorialButtons.back,
            {
                text: 'Finish',
                action: () => {
                    closePopup();
                    tour.complete();
                }
            }
        ],
        when: {
            show: showPopup
        }
    });
}
