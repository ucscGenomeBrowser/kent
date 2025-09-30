/* jshint esnext: true */
/* global Shepherd */

//Creating an IIFE to prevent global variable conflicts
(function() {
    const customTrackTour = new Shepherd.Tour({
        defaultStepOptions: {
            cancelIcon: { enabled: true },
            classes: 'class-1 class-2',
            scrollTo: { behavior: 'smooth', block: 'center' }
        },
      useModalOverlay: true,
      canClickTarget: false
    });
    
    // log when a tutorial is started (commented out for now)
    customTrackTour.on('start', function() {
        writeToApacheLog("customTrackTutorial start " + getHgsid());
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

    
    // Function to add steps to the customTrackTour
    function customTrackSteps() {
        customTrackTour.addStep({
            title: 'Adding custom data to the Genome Browser',
            text: 'In this tutorial, we '+
                  'will explore how to configure custom tracks '+
                  'in the various formats available for a track.',
            buttons: [tutorialButtons.quit, tutorialButtons.next],
            id: 'intro'
        });
    
    }

    if (typeof window.customTrackTour === 'undefined') {
        customTrackSteps();
        //Export the customTrackTour globalally
        window.customTrackTour = customTrackTour;
    }
})();
