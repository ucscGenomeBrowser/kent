const tour = new Shepherd.Tour({
  defaultStepOptions: {
    cancelIcon: {
      enabled: true
    },
    classes: 'class-1 class-2',
    scrollTo: { behavior: 'smooth', block: 'center' }
  }
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
            localStorage.setItem("hgTracks_hideTutorial", "1");
            return this.complete();
        },
        classes: 'shepherd-button-secondary',
        text: 'Finish'
    }
};

tour.addStep({
title: 'Using the UCSC Genome Browser',
text: 'This is the first step in the tutorial, it is at ' +
        'the \'bottom\' of the multi-region button. Click "Next" button to continue ' +
        'to the next step or the "X" or "Finish" to stop the tutorial',
    attachTo: {
        element: '#hgTracksConfigMultiRegionPage',
        on: 'bottom'
    },
    buttons: [tutorialButtons['next'], tutorialButtons['end']],
    id: 'creating'
});

tour.addStep({
title: 'Jairo\'s test edit'
text: 'This will highlight the blue navigation bar' +
        'and hopefully works',
    attachTo: {
        element: '#hgTracks-main-menu-whole',
        on: 'bottom'
    },
    buttons: [tutorialButtons['next'], tutorialButtons['end']],
    id: 'test'
});

tour.addStep({
    title: 'Second step',
    text: 'This is the second step in the tutorial, to the left of the \'go\' button ' +
        'yes the tutorial will restart every time you refresh the page since this is a ' +
        'proof of concept',
    attachTo: {
        element: '#goButton',
        on: 'left'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'second'
});

tour.addStep({
    title: 'Third step',
    text: 'This is the last step in the tutorial, to the right of the last refresh button',
    attachTo: {
        element: function() {return $("input[name='hgt\.refresh']").slice(-1)[0];},
        on: 'right'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['end']],
    id: 'third'
});
