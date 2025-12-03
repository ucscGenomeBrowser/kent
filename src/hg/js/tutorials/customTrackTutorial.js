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
        'load_bigWig': {
            text: 'Load a bigWig example',
            classes: 'shepherd-button-optional',
            action (event) {
              // If button was clicked already, do nothing
              if (event.currentTarget.textContent === 'bigWig example added'){
                  return;
              }
              // Change button color when clicked
              if (event && event.currentTarget) {
                  event.currentTarget.style.backgroundColor = '#87CEEB'; // Light blue
                  event.currentTarget.textContent = 'bigWig example added';
              }

              // Add text to the textarea
              const textarea = document.querySelector('textarea[name="hgct_customText"]');
              if (textarea.value.trim() === '') {
                  textarea.value = 'browser position chr21:33031596-33041570\n';
              }
              textarea.value += 'track type=bigWig name="bigWig variableStep" '+
                                'description="bigWig variableStep format" visibility=full '+
                                'autoScale=off viewLimits=5.0:25.0 color=255,200,0 '+
                                'yLineMark=11.76 yLineOnOff=on '+
                                'bigDataUrl="http://genome-test.gi.ucsc.edu/~hiram/ctDb/wigVariableStep.bw"\n';
              
              // Optional: trigger any change events if the page listens for them
              textarea.dispatchEvent(new Event('change', { bubbles: true }));
              textarea.dispatchEvent(new Event('input', { bubbles: true }));
            }
        },
        'load_bigBed': {
            text: 'Load a bigBed example',
            classes: 'shepherd-button-optional',
            action (event) {
              // If button was clicked already, do nothing
              if (event.currentTarget.textContent === 'bigBed example added'){
                  return;
              }
              // Change button color when clicked
              if (event && event.currentTarget) {
                  event.currentTarget.style.backgroundColor = '#87CEEB'; // Light blue
                  event.currentTarget.textContent = 'bigBed example added';
              }

              // Add text to the textarea
              const textarea = document.querySelector('textarea[name="hgct_customText"]');
              if (textarea.value.trim() === '') {
                  textarea.value = 'browser position chr21:33031596-33041570\n';
              }
              textarea.value += 'track type=bigBed db="hg19" name="bigBed12" type="bigBed 12" '+
                                'bigDataUrl="http://genome-test.gi.ucsc.edu/~hiram/ctDb/bigBedType12.bb "'+
                                'visibility=pack itemRgb="On"\n';
              
              // Optional: trigger any change events if the page listens for them
              textarea.dispatchEvent(new Event('change', { bubbles: true }));
              textarea.dispatchEvent(new Event('input', { bubbles: true }));
            }
        },
        'load_bed': {
            text: 'Load a BED example',
            classes: 'shepherd-button-optional',
            action (event) {
              // If button was clicked already, do nothing
              if (event.currentTarget.textContent === 'BED example added'){
                  return;
              }
              // Change button color when clicked
              if (event && event.currentTarget) {
                  event.currentTarget.style.backgroundColor = '#87CEEB'; // Light blue
                  event.currentTarget.textContent = 'BED example added';
              }

              // Add text to the textarea
              const textarea = document.querySelector('textarea[name="hgct_customText"]');
              if (textarea.value.trim() === '') {
                  textarea.value = 'browser position chr21:33031596-33041570\n';
              }
              textarea.value += 'track name="BED 6 example" priority="40" visibility=pack\n'+
                                'chr21	33031596	33033258	item_0	0	-\n'+
                                'chr21	33033258	33034920	item_1	100	+\n'+
                                'chr21	33034920	33036582	item_2	200	-\n'+
                                'chr21	33036582	33038244	item_3	300	+\n'+
                                'chr21	33038244	33039906	item_4	400	-\n'+
                                'chr21	33039906	33041570	item_5	500	+\n';
              
              // Optional: trigger any change events if the page listens for them
              textarea.dispatchEvent(new Event('change', { bubbles: true }));
              textarea.dispatchEvent(new Event('input', { bubbles: true }));
            }
        },
        'load_wig': {
            text: 'Load a WIG example',
            classes: 'shepherd-button-optional',
            action (event) {
              // If button was clicked already, do nothing
              if (event.currentTarget.textContent === 'WIG example added'){
                  return;
              }
              // Change button color when clicked
              if (event && event.currentTarget) {
                  event.currentTarget.style.backgroundColor = '#87CEEB'; // Light blue
                  event.currentTarget.textContent = 'WIG example added';
              }

              // Add text to the textarea
              const textarea = document.querySelector('textarea[name="hgct_customText"]');
              if (textarea.value.trim() === '') {
                  textarea.value = 'browser position chr21:33031596-33041570\n';
              }
              textarea.value += 'track type=wiggle_0 priority="110" name="Wiggle variableStep" '+
                                'visibility=full autoScale=off viewLimits=5.0:25.0 color=255,200,0 '+
                                'yLineMark=11.76 yLineOnOff=on\n'+
                                'variableStep chrom=chr21 span=1108\n'+
                                '33031597	10.0\n'+
                                '33032705	12.5\n'+
                                '33033813	15.0\n'+
                                '33034921	17.5\n'+
                                '33036029	20.0\n'+
                                '33037137	17.5\n'+
                                '33038245	15.0\n'+
                                '33039353	12.5\n'+
                                '33040461	10.0\n';
              
              // Optional: trigger any change events if the page listens for them
              textarea.dispatchEvent(new Event('change', { bubbles: true }));
              textarea.dispatchEvent(new Event('input', { bubbles: true }));
            }
        },
        'load_html': {
            text: 'Load HTML example',
            classes: 'shepherd-button-optional',
            action (event) {
              // If button was clicked already, do nothing
              if (event.currentTarget.textContent === 'HTML example added'){
                  return;
              }
              // Change button color when clicked
              if (event && event.currentTarget) {
                  event.currentTarget.style.backgroundColor = '#87CEEB'; // Light blue
                  event.currentTarget.textContent = 'HTML example added';
              }

              // Add text to the textarea
              const textarea = document.querySelector('textarea[name="hgct_docText"]');
              textarea.value = '<h2>Description</h2>\n' +
                               '<p>\n'+
                               'Replace this text with a summary describing the concepts or \n'+
                               'analysis represented by your data.</p>\n'+
                               '\n'+
                               '<h2>Methods</h2>\n'+
                               '<p>\n'+
                               'Replace this text with a description of the methods used to \n'+
                               'generate and analyze the data.</p>\n'+
                               '\n'+
                               '<h2>Verification</h2>\n'+
                               '<p>\n'+
                               'Replace this text with a description of the methods used to \n'+
                               'verify the data.</p>\n'+
                               '\n'+
                               '<h2>Credits</h2>\n'+
                               '<p>\n'+
                               'Replace this text with a list of the individuals and/or \n'+
                               'organizations who contributed to the collection and analysis \n'+
                               'of the data.</p>\n'+
                               '\n'+
                               '<h2>References</h2>\n'+
                               '<p>\n'+
                               'Replace this text with a list of relevant literature references \n'+
                               'and/or websites that provide background or supporting \n'+
                               'information about the data.</p>';

              // Optional: trigger any change events if the page listens for them
              textarea.dispatchEvent(new Event('change', { bubbles: true }));
              textarea.dispatchEvent(new Event('input', { bubbles: true }));
            }
        },
        'submit': {
            text: 'Submit & continue',
            action() {
              // Save the tour so it continues after page reload
              sessionStorage.setItem('customTrackTourActive', 'true');
              sessionStorage.setItem('customTrackTourStep', 'after-submit');
              // Click the submit button
              const submitButton = document.getElementById('Submit');
              submitButton.click();
            }
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
    }

    
    // Function to add steps to the customTrackTour
    function customTrackSteps() {
        customTrackTour.addStep({
            title: 'Adding custom data to the Genome Browser',
            text:
                  'In this tutorial, we '+
                  'will explore how to configure custom tracks '+
                  'in the <a href="/FAQ/FAQformat.html" target="_blank">various formats</a> '+
                  'available for a track.<br><br>'+
                  'For a stable and permanent visualization, please consider '+
                  'creating a <a href="/goldenPath/help/hgTrackHubHelp.html" '+
                  'target="blank">track hub</a>. If you require hosting space, '+
                  'the UCSC Genome Browser now provides 10Gb of '+
                  '<a href="hgHubConnect?#hubUpload" target="_blank">free hosting space</a> '+
                  'for each user.', 
            buttons: [tutorialButtons.quit, tutorialButtons.next],
            id: 'intro'
        });
        customTrackTour.addStep({
            title: 'Selecting the genome assembly',
            text:
                  'By default, your most recently viewed assembly is selected. '+
                  'Alter the drop-down menus to change the genome assembly.',
            buttons: [tutorialButtons.back, tutorialButtons.next],
            attachTo: 
                {
                element: '#genome-selection-table',
                on: 'bottom'
                },
            id: 'genome-select'
        });
        customTrackTour.addStep({
            title: 'Text-based custom tracks',
            text:
                  'The simplest way to view a custom track is to paste '+
                  'the track and data lines in the dialog box. '+
                  'A custom track consists of three items:'+
                  '<ol>'+
                  '    <li><a href="/goldenPath/help/hgTracksHelp.html#BROWSER" '+
                  '        target="_blank"><b>browser line</b></a>: (optional)<br>'+
                  '        Control aspects of the overall '+
                  '        display window</li>'+
                  '    <li><a href="/goldenPath/help/hgTracksHelp.html#TRACK" '+
                  '        target="_blank"><b>track line</b></a>:<br> '+
                  '        Defines the attributes for '+
                  '        the specific data set</li>'+
                  '    <li><b>data lines</b></li>'+
                  '</ol>'+
                  'Alternatively, you can store the custom track data in a file on a web server, '+
                  'paste the file URL, or upload the file to load the custom track. ',
            buttons: [tutorialButtons.back, tutorialButtons.load_bed,
                      tutorialButtons.load_wig, tutorialButtons.next],
            attachTo: 
                {
                element: '#data-input',
                on: 'right'
                },
            id: 'dialog-box'
        });
        customTrackTour.addStep({
            title: 'Binary-indexed custom tracks',
            text:
                  'To view a bigBed, bigWig, or another binary-indexed file, the file must be '+
                  '<a href="/goldenPath/help/hgTrackHubHelp.html#Hosting" target="_blank">hosted '+
                  'on an external server</a> that allows byte-range requests. '+
                  'Binary-indexed custom tracks only need a <em>track</em> '+
                  'line to define the custom track as it uses the '+
                  '<a href="/goldenPath/help/trackDb/trackDbHub.html#bigDataUrl" '+
                  'target="_blank"><em>bigDataUrl</em></a> setting to fetch the annotations.'+
                  '<br><br>'+
                  'Alternatively, you can paste the URL directly without a "track line" to '+
                  'quickly view the file. The benefit in using a track line with a '+
                  '<em>bigDataUrl</em> parameter is ' +
                  'to add other configuration options for the big* file.',
            buttons: [tutorialButtons.back, tutorialButtons.load_bigBed,
                      tutorialButtons.load_bigWig, tutorialButtons.next],
            attachTo: 
                {
                element: '#data-input',
                on: 'right'
                },
            id: 'bigCustom-tracks'
        });
        customTrackTour.addStep({
            title: 'Upload track documentation &nbsp;<em>(Optional)</em>',
            text:
                  'In this dialoge box, you can add HTML that will be shown on the custom '+
                  'track\'s description page. This is not required but highly recommended.'+
                  '<br><br>'+
                  'An <a href="/goldenPath/help/ct_description.txt" '+
                  'target="_blank">example HTML</a> text is provided, and can '+
                  'be edited to fit your needs. ',
            buttons: [tutorialButtons.back, tutorialButtons.load_html, tutorialButtons.next],
            attachTo: 
                {
                element: '#description-input',
                on: 'right'
                },
            id: 'documentation-box'
        });
    
        customTrackTour.addStep({
            title: 'Submit the custom track',
            text:
                  'Once you paste/upload your custom track data, you can use the '+
                  '<em><b>Submit</b></em> button to send the data to the UCSC Genome Browser. ',
            buttons: [tutorialButtons.back, tutorialButtons.submit],
            when: {
                show: function() {
                    const textarea = document.querySelector('textarea[name="hgct_customText"]');
                    const isEmpty = textarea.value.trim() === '';
                    
                    // If the textbox is empty, load a BED track
                    if (isEmpty) {
                        textarea.value = 'browser position chr21:33031596-33041570\n';
                        textarea.value += 'track type=bigWig name="bigWig variableStep" '+
                                          'description="bigWig variableStep format" visibility=full '+
                                          'autoScale=off viewLimits=5.0:25.0 color=255,200,0 '+
                                          'yLineMark=11.76 yLineOnOff=on '+
                                          'bigDataUrl="http://genome-test.gi.ucsc.edu/~hiram/ctDb/wigVariableStep.bw"\n';
              
                        // Optional: trigger any change events if the page listens for them
                        textarea.dispatchEvent(new Event('change', { bubbles: true }));
                        textarea.dispatchEvent(new Event('input', { bubbles: true }));
                    }
                }
            },
            attachTo: 
                {
                element: '#Submit',
                on: 'top'
                },
            id: 'submit'
        });
    }

    function customTrackResultSteps() {
        customTrackTour.addStep({
            title: 'View your uploaded custom tracks',
            text:
                  'This table shows the custom tracks that you uploaded to the UCSC Genome Browser '+
                  'from the previous page.<br><br>'+
                  'Using this table, you can delete any unwanted custom '+
                  'tracks. You can also click onto the track names to edit the settings or data.',
            buttons: [tutorialButtons.next],
            attachTo: 
                {
                element: '#resultsTable',
                on: 'bottom'
                },
            id: 'after-submit'
        });

        customTrackTour.addStep({
            title: '"View in" drop-down menu',
            text:
                  'By default, the Genome Browser will take you to view your custom track '+
                  'on the main Genome Browser image. <br><br>'+
                  'Altering this drop-down will let you send '+
                  'the data to the Table Browser, Variant Annotation Integrator, or the '+
                  'Data Integrator.',
            buttons: [tutorialButtons.back, tutorialButtons.next],
            attachTo: 
                {
                element: '#navSelect',
                on: 'bottom'
                },
            id: 'navigation-drop-down'
        });

        customTrackTour.addStep({
            title: 'Add more tracks to the Genome Browser',
            text:
                  'Click this button to return to the previous page to add more custom tracks.',
            buttons: [tutorialButtons.back, tutorialButtons.next],
            attachTo: 
                {
                element: '#addTracksButton',
                on: 'bottom'
                },
            id: 'add-tracks'
        });

        customTrackTour.addStep({
            title: 'Additional resources and documentation',
            text: 'For further examples of using ' +
                  'custom tracks, please read the <a href="/goldenPath/help/customTrack.html" '+
                  'target="_blank">Custom Track user guide</a>. You can find examples of simple '+
                  'annotation files, BED custom tracks with muliple blocks, loading custom tracks '+
                  'via the URL, and more. '+
                  '<br><br>'+
                  'You can also <a href="/contacts.html" target="_blank">contact us</a> if you '+
                  'have any questions or issues uploading a custom track. ',
            attachTo: {
                element: '#hgCustomHelp',
                on: 'left-start'
            },
            buttons: [ tutorialButtons.back, tutorialButtons.end ],
            when: {
                show: () => {
                    keepMenuVisible('#help > ul');
                },
                hide: () => hideMenu('#help > ul')
            },
            id: 'additional-help'
        });
    
    }

    if (typeof window.customTrackTour === 'undefined') {
        // Unique ID to verify that you are on the results page
        customTrackSteps();
        //Export the customTrackTour globalally
        window.customTrackTour = customTrackTour;
    }
    // Condition to continue the tutorial once you submit the custom track
    if (sessionStorage.getItem('customTrackTourActive') === 'true') {
        const stepToShow = sessionStorage.getItem('customTrackTourStep');
        
        // Clear the session storage
        sessionStorage.removeItem('customTrackTourActive');
        sessionStorage.removeItem('customTrackTourStep');
        
        // Resume tour at the saved step
        if (stepToShow) {
            customTrackResultSteps();
            customTrackTour.show(stepToShow);
        }
    }
})();
