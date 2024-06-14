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
        title: 'Introduction for Clinicians',
        text: 'This tutorial is for clinicians and is no where near being complete in terms of '+
              'content but should be fully functioning without any bugs (hopefully). ',
        buttons: [tutorialButtons.next, tutorialButtons.end],
        id: 'intro',
        classes: 'dark-background'
    });
    tour.addStep({
        title: 'Recommended Track Sets',
        text: 'Some text about the recommended track sets',
        attachTo: {
            element: '#recTrackSetsMenuItem',
            on: 'bottom'
        },
        buttons: [ tutorialButtons.back,
           {
                text: 'Next',
                action: function() {
                    const rtsMenuItem = document.querySelector('#tools2 #recTrackSetsMenuItem');
                    rtsMenuItem.click();
                    tour.next();
                }
            }
        ],
        id: 'navbar',
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
        title: 'Clinical SNVs',
        text: 'Assess potential disease contributions of single nucleotide variants in coding regions.',
        attachTo: {
            element: '#recTrackSetsPopup ul li:nth-child(1)',
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
            tutorialButtons.next
        ],
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
        buttons: [
            tutorialButtons.back,
            tutorialButtons.next
        ],
        when: {
            show: showPopup
        }
    });

    tour.addStep({
        title: 'Non-coding SNVs',
        text: 'Investigate functional aspects of non-coding variants.',
        attachTo: {
            element: '#recTrackSetsPopup ul li:nth-child(3)',
            on: 'right'
        },
        buttons: [
            tutorialButtons.back,
            tutorialButtons.next
        ],
        when: {
            show: showPopup
        }
    });

    tour.addStep({
        title: 'Problematic Regions',
        text: 'Evaluate if annotations are on potential low confidence regions due to high homology or other reported concerns.',
        attachTo: {
            element: '#recTrackSetsPopup ul li:nth-child(4)',
            on: 'right'
        },
        buttons: [
            tutorialButtons.back,
            {
                text: 'Finish',
                action: () => {
                    tour.complete();
                    closePopup();
                }
            }
        ],
        when: {
            show: showPopup
        }
    });
}
