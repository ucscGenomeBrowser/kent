	/* jshint esnext: true */
	/* global Shepherd */

	//Creating an IIFE to prevent global variable conflicts
	(function() {
	    // Code runs immediately and does not pollute the global scope
	    const gatewayTour = new Shepherd.Tour({
		defaultStepOptions: {
		    cancelIcon: { enabled: true },
		    classes: 'class-1 class-2',
		    scrollTo: { behavior: 'smooth', block: 'center' }
		},
	      useModalOverlay: true
	    });
	    
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
	    
	    function keepMenuVisible(selector) {
		const menu = document.querySelector(selector);
		menu.style.display = 'block';
		menu.style.visibility = 'visible';
	    
		const makeVisible = () => {
		    menu.style.display = 'block';
		    menu.style.visibility = 'visible';
		};
	    
		const events = ['mouseover', 'mouseout', 'mouseenter', 'mouseleave', 'mousemove'];
		events.forEach(event => {
		    menu.addEventListener(event, makeVisible);
		});
	    
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
		showRecTrackSetsPopup(); // Call the function that generates the popup
	    }

	    // Function to close the popup
	    function closePopup() {
		$("#recTrackSetsPopup").dialog("close");
	    }

	    // Function to log finishing the clinical tutorial
	    function listenForBasicTutorial() {
		document.getElementById('basicTutorialLink').addEventListener('click', function() {
		    gatewayTour.complete();
		});
	    }

    function gatewaySteps() {
        gatewayTour.addStep({
            title: 'Setting up a gateway tutorial',
            text: 'Setting the interactive tutorial for the hgGateway page ',
            buttons: [tutorialButtons.next, tutorialButtons.end],
            id: 'intro',
            classes: 'dark-background'
        });
    }
	    
	    
	    if (typeof window.gatewayTour === 'undefined') {
		gatewaySteps();
        window.gatewayTour = gatewayTour;
    }
})();
