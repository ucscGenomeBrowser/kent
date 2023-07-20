function displaySelectedOption(selectId) {
  const selectElement = document.getElementById(selectId);
  const selectedOption = selectElement.options[selectElement.selectedIndex].text;
  return selectedOption;
}


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

/* Wrapping the tutorial steps and fuctions to only execute after the page loads*/
window.onload = function() {
  function selectMiddleButton() {
    var hgTracksTable = document.getElementById('imgTbl');
    var rowCount = hgTracksTable.rows.length;
    var middleIndex = Math.floor(rowCount / 2);

    var middleRow = hgTracksTable.rows[middleIndex];
    var firstId = middleRow.querySelector('td');
    var middleTrackId = firstId.id;

    return '#' + middleTrackId;
  }

tour.addStep({
    title: 'Welcome to the UCSC Genome Browser Tutorial',
    text: 'The blue navigation bar at the top of the page will allow you to access the ' +
          'tools, downloads, and help pages. There are four main drop-downs that are useful ' +
          'for most users: ' +
          '<ul>' +
          '<li><b>Genomes</b> - switch between the many genomes available</li> ' +
          '<li><b>Genome Browser</b> - configure, search for tracks, and reset the ' +
           'Genome Browser back to the default settings.</li>' +
          '<li><b>Tools</b> - access to features such as <a target="_blank" ' +
           'href="/cgi-bin/hgBlat">BLAT</a>, <a target="_blank" href="/cgi-bin/hgPcr">isPCR</a>, '+
           'and <a target="_blank" href="/cgi-bin/hgLiftOver">LiftOver</a>. The <a target="_blank" '+
           'href="/cgi-bin/hgTables">Table Browser</a> can also be used to export track data in ' +
           'various file formats.</li>' +
          '<li><b>My Data</b> - create stable short links (<a target="_blank" '+
           'href="/cgi-bin/hgSession">Sessions</a>), and visualize '+
           'your own data via <a target="_blank" href="/cgi-bin/hgCustom">custom tracks</a> or ' +
           '<a target="_blank" href="/cgi-bin/hgHubConnect">track hubs</a>.</li>'+
          '<li><b>Help</b> - access contact information, FAQs, and Browser Documentation.</li>' +
          '</ul>',
    attachTo: {
        element: '#nice-menu-1',
        on: 'bottom'
    },
    buttons: [tutorialButtons['next'], tutorialButtons['end']],
    id: 'navbar',
    classes: 'dark-background'
});


tour.addStep({
    title: 'Browsing the Genome',
    text: 'The search bar allows you to navigate to a region on the genome using ' +
          '<a href="https://genome-blog.soe.ucsc.edu/blog/2016/12/12/the-ucsc-genome-browser-coordinate-counting-systems/"' +
          'target="_blank">genome coordinates</a>, <a href="/FAQ/FAQgenes.html#genename" ' +
          'target="_blank">gene symbols</a>, <a href="https://www.ncbi.nlm.nih.gov/snp/docs/RefSNP_about/#what-is-a-reference-snp" ' +
          'target="_blank">rsIDs</a>, <a href="http://varnomen.hgvs.org/" ' +
          'target="_blank">HGVS</a> terms, or DNA sequence. You can even search documentation' +
          'and FAQ pages using this search bar. A few example queries are: ' +
          '<ul>' +
          '<li>chr1:127140001-127140001</li>' +
          '<li>SOD1</li>' +
          '<li>rs2569190</li>' +
          '<li>NM_198056.3:c.1654G>T</li>' +
          '<li>CCTTCCTATAGTCCGGAATACGCC<br>' +
          'AATGGCGCGGCCGGCCTGGACC<br>' +
          'ACTCCCATTACGGGGGTGTCCC<br>' +
          'GGGCAGCGGGGCCGGAGGCTTA<br>' +
          'ATGCAAAGGC</li></ul>' +
          'Please note, <a href="/goldenPath/help/hgTracksHelp.html#BLATAlign" target="_blank">BLAT</a> '+
          'is used if your search term is a DNA sequence. For the best ' +
          'results, make sure your sequence is long enough to meet BLAT specifications. The ' +
          '<a href="/goldenPath/help/query.html" target="_blank">examples</a> link next to ' +
          'the search bar contains even more search queries.' ,
    attachTo: {
        element: '#positionInput',
        on: 'bottom'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'search'
});

tour.addStep({
    title: 'Drag-and-Select the Genome Browser Image',
    text: 'Dragging the Genome Browser image performs different tasks depeneding on where and ' +
          'how you click the image. <br><br> '+
          'Click-and-Drag the ruler at the top of the image will bring up a menu to zoom into '+
          'or highlight the region. Click-and-Drag anywhere else on the Genome Browser image '+
          'will allow you to scroll to the left or right.' +
          '<br><br>' +
          'Alternatively, you can: '+
          '<ul>'+
          '<li>Hold <b>Alt+drag</b> or <b>Option+drag</b> to highlight</li>'+
          '<li>Hold <b>Ctrl+drag</b> or <b>Cmd+drag</b> to zoom</li>'+
          '</ul>',
    attachTo: {
        element: '#td_data_ruler',
        on: 'bottom',
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'highlight'
});

tour.addStep({
    title: 'Quick Link to Change Track Settings',
    text: 'Clicking on the rectangle box next to a track is an easy way to quickly ' +
          'go to the track settings page for that track.' +
          '<br><br>' +
          '<a href="/goldenPath/help/hgTracksHelp.html#RIGHT_CLICK_NAV" ' +
          'target="_blank">Right-clicking</a> on the track will also bring up a menu ' +
          'to change the display mode, configure a track, or view a PNG image of the current ' +
          'window.' +
          '<img src="/images/right_click_example.png" width="350">' +
          '',
    attachTo: {
        element: selectMiddleButton(),
        on: 'right',
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'hgTrackUiLink'
});

tour.addStep({
    title: 'Changing the Display Mode of a Track',
    text: 'Annotation tracks can be entirely hidden or shown in four different ways that take ' +
          'an increasing amount of vertical space: ' +
          '<a href="/goldenPath/help/hgTracksHelp.html#TRACK_CONT" target="_blank">dense, squish, '+
          'pack, and full</a>.'+
          '<br><br>' +
          'After changing the display mode of a track, the change will not be applied ' +
          'until after you refresh the page. You could either refresh the page manually ' +
          'using your web browser or you can click <button>refresh</button> on any of the ' +
          'track groups.',
    attachTo: {
        element: function() {return $("input[name='hgt\.refresh']").slice(0)[0];},
        on: 'bottom'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'refresh'
});

tour.addStep({
    title: 'Searching for Tracks on the Genome Browser',
    text: 'Having trouble finding a dataset for your genome assembly? The ' +
        '<a href="/cgi-bin/hgTracks?hgt_tSearch=track+search" target="_blank">Track Search</a> ' +
        'feature allows searching for terms in track names, descriptions, groups, and ENCODE ' +
        'metadata. <br><br>'+
        'More information about <button>track search</button> can be found on the following ' +
        '<a href="/goldenPath/help/trackSearch.html" target="_blank">help page</a>. '+
        'The Track Search feature can also be accessed by hovering over the ' +
        '"Genome Browser" drop-down menu.',
    attachTo: {
        element: '#hgt_tSearch',
        on: 'top'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'shortCuts',
});

tour.addStep({
    title: 'Configure the Genome Browser Image',
    text: 'Use the <button>configure</button> button to customize graphic font, size, gridlines, ' +
          'and more. This can be helpful when exporting an image for publication. ' +
          '<br><br>' +
          'You can also find a link to configure the browser image by hovering over the ' +
          '"Genome Browser" drop-down menu.',
    attachTo: {
        element: '#hgTracksConfigPage',
        on: 'bottom'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'configure'
});

tour.addStep({
title: 'Flip the Strand Orientation',
text: 'By default, the UCSC Genome Browser displays the forward strand (5\' to 3\'), but ' +
      'it can be configured to display the negative strand (3\' to 5\'). <br><br>' +
      'To reverse the genome orientation, click the <button>reverse</button> button and the Genome Browser image '+
      'will flip to show either the negative or positive strand.',
    attachTo: {
        element: document.getElementById('hgt.toggleRevCmplDisp'),
        on: 'bottom'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'reverse'
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
    buttons: [tutorialButtons['back'], tutorialButtons['end']],
    id: 'lastPopUp'
});
}
