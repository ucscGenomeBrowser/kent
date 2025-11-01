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
            text:
                  'In this tutorial, we '+
                  'will explore how to configure custom tracks '+
                  'in the various formats available for a track.<br><br>'+
                  'For a stable and permanent visualization, please consider '+
                  'creating a track hub. ',
            buttons: [tutorialButtons.quit, tutorialButtons.next],
            id: 'intro'
        });
        customTrackTour.addStep({
            title: 'Selecting the genome assembly',
            text:
                  'By default, your most recently viewed assembly is selected. '+
                  'Alter the drop-down menus to change the genome assembly.',
            buttons: [tutorialButtons.back, tutorialButtons.next],
            id: 'genome-select'
        });
        customTrackTour.addStep({
            title: 'Examples of custom track data Pt. 1', 
            text:
                  'The simpliest way to view a custom track is to paste the '+
                  'the track and data lines in the dialog box. '+
                  'A custom track consists of three items:'+
                  '<ol>'+
                  '    <li>browser lines (optional)</li>'+
                  '    <li>track line</li>'+
                  '    <li>data lines</li>'+
                  '</ol>'+
                  'Alternatively, you can store the custom track data in a file on a web server, '+
                  'and paste the URL to the file to load the custom track. ',
            buttons: [tutorialButtons.back, tutorialButtons.next],
            id: 'dialog-box'
        });
        customTrackTour.addStep({
            title: 'Examples of custom track data Pt. 2', 
            text:
                  'You can use the <em>bigDataUrl</em> parameter in the track line ' +
                  'to upload a bigBed, bigWig, or other binary indexed file',
            buttons: [tutorialButtons.back, tutorialButtons.next],
            id: 'bigCustom-tracks'
        });
        customTrackTour.addStep({
            title: '(Optional) Upload track documentation',
            text:
                  ''+
                  '',
            buttons: [tutorialButtons.back, tutorialButtons.next],
            id: 'documentation-box'
        });
    
    }

    if (typeof window.customTrackTour === 'undefined') {
        customTrackSteps();
        //Export the customTrackTour globalally
        window.customTrackTour = customTrackTour;
    }
})();
