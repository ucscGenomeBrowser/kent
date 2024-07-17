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
   // clinicalTour.on('start', function() {
  //      writeToApacheLog("clinical start " + getHgsid());
  //  });
    
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
    // Function to keep the menu visible
    function helpMenuVisible() {
        const helpMenu = document.querySelector('#help > ul');
        helpMenu.style.display = 'block';
        helpMenu.style.visibility = 'visible';
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
            buttons: [tutorialButtons['back'], tutorialButtons['next']],
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
            buttons: [ tutorialButtons.back, {
                         text: 'Next',
                         action: function() {
                             const rtsMenuItem = document.querySelector('#tools2 #recTrackSetsMenuItem');
                             rtsMenuItem.click();
                             clinicalTour.next();
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
    
        clinicalTour.addStep({
            title: 'Display Annotations for a Specific Scenario',
            text: 
                  '<em>Note: Loading a track set may hide some tracks in your current view.</em>'+
                  '<br><br>'+
                  'Each available track set loads a display curated to the specific theme. '+
                  'The <b>Clinical SNVs</b> and <b>Clinical CNVs</b> track sets are '+
                  'routinely updated to include existing and new annotations available on the '+
                  'Genome Browser used in variant interpretation according to ACMG/AMP guidelines.'+
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
            buttons: [
                {
                    text: 'Back',
                    action: () => {
                        clinicalTour.back();
                        closePopup();
                    }
                },
                {
                    text: 'Next',
                    action: () => {
                        clinicalTour.next();
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
    
        clinicalTour.addStep({
            title: 'Additional Resources and Feedback',
            text: 'If you have any questions or suggestions including annotations '+
                  'that you feel are missing from a track set or a new track set theme, '+
                  '<a href="/contacts.html" target="_blank">please write to us</a>. '+
                  'Also, if you are new to the '+
                  'UCSC Genome Browser, consider exploring our '+
                  '<a href="/cgi-bin/hgTracks?startTutorial=true" '+
                  'target="_blank">basic tutorial</a>.'+
                  '<br><br>'+
                  'Lastly, the following features may also be helpful in variant interpretation:'+
                  '<ul>'+
                  '  <li><a href="/cgi-bin/hgVai" target="_blank">Variant Annotation Integrator</a>:'+
                  '       Provides effect prediction and annotation associations for variant calls.</li>'+
                  '  <li>Tracks display <a href="/goldenPath/help/hgTracksHelp.html#Highlight" '+
                  '      target="_blank">highlighting</a> can accent specific regions in your '+
                  '      view.</li>'+
                  '  <li><a href="/cgi-bin/hgSession" target="_blank">Sessions</a>: Allows you to '+
                  '      save your own pre-configured displays and create stable links.</li>'+
                  '  <li><a href="/cgi-bin/hgTracks?hgt_tSearch=track+search" target="_blank">Track'+
                  '      search</a>: Search through all available annotations for an '+
                  '      assembly. This is the best way to check if the Genome Browser has '+
                  '      specific datasets.</li>'+
                  '</ul>',
            attachTo: {
                element: '#help ul li:nth-child(4)',
                on: 'right'
            },
            buttons: [ tutorialButtons.back, {
                           text: 'Finish',
                           action: () => {
                               const helpMenu = document.querySelector('#help > ul');
                               helpMenu.style.display = 'none';
                               helpMenu.style.visibility = 'hidden';
                               clinicalTour.complete();
                           }
                     }],
            when: {
                show: () => {
                    const helpMenu = document.querySelector('#help > ul');
                    helpMenu.style.display = 'block';
                    helpMenu.style.visibility = 'visible';
    
                    helpMenu.addEventListener('mouseover', helpMenuVisible);
                    helpMenu.addEventListener('mouseout', helpMenuVisible);
                    helpMenu.addEventListener('mouseleave', helpMenuVisible);
                    helpMenu.addEventListener('mousemove', helpMenuVisible);
                    helpMenu.querySelectorAll('li').forEach(function(item) {
                        item.addEventListener('mouseover', helpMenuVisible);
                        item.addEventListener('mouseout', helpMenuVisible);
                        item.addEventListener('mouseleave', helpMenuVisible);
                        item.addEventListener('mousemove', helpMenuVisible);
                    });
                },
                hide: () => {
                    const helpMenu = document.querySelector('#help > ul');
                    helpMenu.style.display = 'none';
                    helpMenu.style.visibility = 'hidden';
    
                    helpMenu.removeEventListener('mouseover', helpMenuVisible);
                    helpMenu.removeEventListener('mouseout', helpMenuVisible);
                    helpMenu.removeEventListener('mouseleave', helpMenuVisible);
                    helpMenu.removeEventListener('mousemove', helpMenuVisible);
                    helpMenu.querySelectorAll('li').forEach(function(item) {
                        item.removeEventListener('mouseover', helpMenuVisible);
                        item.removeEventListener('mouseout', helpMenuVisible);
                        item.removeEventListener('mouseleave', helpMenuVisible);
                        item.removeEventListener('mousemove', helpMenuVisible);
                    });
                }
            }
        });
    }
    
    clinicalSteps();
    
    //Export the clinicalTour globalally
    window.clinicalTour = clinicalTour;
})();
