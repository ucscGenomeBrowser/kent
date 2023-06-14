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
    classes: 'class-1 class-2 dark-background',
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
            return this.complete();
        },
        classes: 'shepherd-button-secondary',
        text: 'Finish'
    }
};

tour.addStep({
    title: 'Welcome to the UCSC Genome Browser Tutorial',
    text: 'The blue navigation bar at the top of the page will allow you to access the ' +
          'tools, downloads, and help pages.' +
          '<br><br>' +
          'This tutorial is aimed for new users but may contain tips for even an ' +
          'experienced user. For example, the Genome Browser supports keyboard shortcuts, and ' +
          'you can press the \'<b>?</b>\' key on your keyboard for a full list of options. ',
    attachTo: {
        element: '#nice-menu-1',
        on: 'bottom'
    },
    buttons: [tutorialButtons['next'], tutorialButtons['end']],
    id: 'navbar',
    classes: 'dark-background'
});

/*
tour.addStep({
title: 'Multi-region',
text: 'The multi-region feature allows you to view two or ' +
        'more discontinuous regions of the genome on a virtual chromosome. The ' +
        'multi-region feature also enables you to view only genes or exons from the ' +
        'GENCODE genes track.',
    attachTo: {
        element: '#hgTracksConfigMultiRegionPage',
        on: 'bottom'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'multiRegion'
});
*/

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
    title: 'Quick Link to Change Track Settings',
    text: 'Clicking on the rectangle box next to a track is an easy way to quickly ' +
          'go to the track settings page for the track.' +
          '<br><br>' +
          '<a href="/goldenPath/help/hgTracksHelp.html#RIGHT_CLICK_NAV" ' +
          'target="_blank">Right-clicking</a> on the track will also bring up a menu ' +
          'to change the display mode, configure a track, or view a PNG image of the current ' +
          'window.' +
          '<img src="/images/right_click_example.png" width="350">' +
          '',
    attachTo: {
        element: '#td_btn_ruler',
        on: 'right'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'hgTrackUiLink'
});
/*
tour.addStep({
    title: 'Collapasing and Expanding Track Groups',
    text: 'Tracks are organized by the type of annotation the data represents. ' +
          'The groups will vary depending on the data available for the assembly. ' +
          '<br><br>' +
          'You can click on the minus button to hide a track group. If the track group is '+ 
          'hidden, click on the plus button to expand the group.',
    attachTo: {
        element: '#map_button',
        on: 'bottom'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'trackGroups'
});
*/

tour.addStep({
    title: 'Changing the Display Mode of a Track',
    text: 'After changing the display mode of a track, the change will not be applied ' +
          'until after you refresh the page. You could either refresh the page manually ' +
          'using your web browser or you can click the "refresh" button on any of the ' +
          'track groups.' +
          '<br><br>' +
          'More information about the four display modes can be found ' +
          '<a href="/goldenPath/help/hgTracksHelp.html#TRACK_CONT" target="_blank">here</a>.',
    attachTo: {
        element: function() {return $("input[name='hgt\.refresh']").slice(0)[0];},
        on: 'bottom'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'refresh'
});

tour.addStep({
    title: 'Searching for Tracks on the Genome Browser',
    text: 'Having trouble finding a dataset for your genome assembly? <br><br> The ' +
        '<a href="/cgi-bin/hgTracks?hgt_tSearch=track+search" target="_blank">Track Search</a> ' +
        'feature allows searching for terms in track names, descriptions, groups, and ENCODE ' +
        'metadata. If multiple terms are used, only tracks with all matching terms will be part ' +
        'of the results. The Track Search feature can also be accessed by hovering over the ' +
        '"Genome Browser" drop-down menu. '+
        '<br><br>' +
        'More information about the Track Search can be found on the following ' +
        '<a href="/goldenPath/help/trackSearch.html" target="_blank">help page</a>.',
    attachTo: {
        element: '#hgt_tSearch',
        on: 'top'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'shortCuts'
});

tour.addStep({
    title: 'Configure the Genome Browser Image',
    text: 'Use the <a href="/cgi-bin/hgTracks?hgTracksConfigPage=configure" ' +
          'target="_blank">configure</a> button to customize graphic font, size, gridlines, ' +
          'and more. This can be helpful when exporting an image for publication. ' +
          '<br><br>' +
          'You can also find a link to configure the browser image by hovering over the ' +
          '"Genome Browser" drop-down menu. ',
    attachTo: {
        element: '#hgTracksConfigPage',
        on: 'bottom'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['next']],
    id: 'configure'
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
          /*'<br><br>' +
          'Permission is granted for reuse of all graphics produced by the UCSC Genome Browser ' +
          'website.',*/
    attachTo: {
        element: '#help.menuparent',
        on: 'bottom'
    },
    buttons: [tutorialButtons['back'], tutorialButtons['end']],
    id: 'lastPopUp'
});
