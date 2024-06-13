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



// wrap setup in a function to be called only after  document is ready
function setupSteps()
{
    tour.addStep({
        title: 'Recommneded Track Sets',
        text: 'Some text about the recommended track sets',
        attachTo: {
            element: '#recTrackSetsMenuItem',
            on: 'bottom'
        },
        buttons: [tutorialButtons.next, tutorialButtons.end],
        id: 'navbar',
        classes: 'dark-background',
        when: {
            show: () => {
                const toolsMenu = document.querySelector('#tools2 > ul');
                toolsMenu.style.display = 'block';
                toolsMenu.style.visibility = 'visible';


                toolsMenu.addEventListener('mouseover', keepMenuVisible);
                toolsMenu.addEventListener('mouseout', keepMenuVisible);

                function keepMenuVisible() {
                    toolsMenu.style.display = 'block';
                    toolsMenu.style.visibility = 'visible';
                }
            },
            hide: () => {
                const toolsMenu = document.querySelector('#tools2 > ul');
                toolsMenu.style.display = 'none';
                toolsMenu.style.visibility = 'hidden';

                toolsMenu.removeEventListener('mouseover', keepMenuVisible);
                toolsMenu.removeEventListener('mouseout', keepMenuVisible);

                function keepMenuVisible() {
                    toolsMenu.style.display = 'block';
                    toolsMenu.style.visibility = 'visible';
                }

            }
        }
    });

    tour.addStep({
        title: '',
        text: 'Some test' +
              ' with more text',
        attachTo: {
            element: '#positionInput',
            on: 'bottom'
        },
        buttons: [tutorialButtons.back, tutorialButtons.next],
        id: 'search'
    });


    tour.addStep({
        title: 'Further Training and Contact Information',
        text: 'You can find other guides and training videos on the ' +
              '<a href="../training/" target="_blank">training page</a>. ' +
              'You can also search the ' +
              '<a href="https://groups.google.com/a/soe.ucsc.edu/g/genome" target="_blank">mailing list archive</a> ' +
              'to find previously answered questions for guidance. ' +
              '<br><br>' +
              'If you still have questions after searching the ' +
              '<a href="/FAQ/" target="_blank">FAQ page</a> or ' +
              '<a href="/goldenPath/help/hgTracksHelp.html" target="_blank">Genome Browser User Guide</a> ' +
              'pages, you can email the suitable mailing list for your inquiry from the ' +
              '<a href="../contacts.html">contact us</a> page. ' +
              '<br><br>' +
              'Follow these <a href="/cite.html" target="_blank">citation guidelines</a> when using ' +
              'the Genome Browser tool suite or data from the UCSC Genome Browser database in a ' +
              'research work that will be published in a journal or on the Internet. <br><br>' +
              'In addition to the <a href="/goldenPath/pubs.html" target="_blank">relevant paper</a>, '+
              'please include a reference to the Genome Browser website in your manuscript: '+
              '<i>http://genome.ucsc.edu</i>. ',
        attachTo: {
            element: '#help.menuparent',
            on: 'bottom'
        },
        buttons: [tutorialButtons.back, tutorialButtons.end],
        id: 'lastPopUp'
    });
}
