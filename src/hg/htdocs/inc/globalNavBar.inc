<div id="main-menu-whole">

		<div id="main-menu"><div id="block-nice_menus-1" class="clear-block block block-nice_menus">

	<div class="upcorner"></div>
	<div class="blockbg">
		<div class="blockcontent">
						<div class="content">

<ul class="nice-menu nice-menu-down sf-js-enabled" id="nice-menu-1">

<li class="first even" id="home"><a href="/index.html" ><span id="home-link">Home</span></a></li> 

<li id="genomes"><a href="/cgi-bin/hgGateway">Genomes</a></li>
<li id="genomeBrowser"><a href="/cgi-bin/hgTracks">Genome Browser</a></li>

<li class="menuparent" id="tools"><span>Tools</span>
	<ul style="display: none; visibility: hidden;">
	<li class="first"><a href="../cgi-bin/hgBlat?command=start">Blat</a></li>
	<li><a href="../cgi-bin/hgTables">Table Browser</a></li>
	<li><a href="../cgi-bin/hgNear">Gene Sorter</a></li>
	<li><a href="../cgi-bin/hgGenome">Genome Graphs</a></li>
	<li><a href="../cgi-bin/hgPcr">In-Silico PCR</a></li>
	<li><a href="../cgi-bin/hgLiftOver">LiftOver</a></li>
	<li><a href="../cgi-bin/hgVisiGene">VisiGene</a></li>
	<li class="last"><a href="../util.html">Other Utilities</a></li>
	</ul>
</li>
<li class="menuparent" id="mirrors"><span>Mirrors</span>
	<ul style="display: none; visibility: hidden;">
	<li class="first"><a href="../mirror.html">Third Party Mirrors</a></li>
	<li><a href="../admin/mirror.html">Mirroring Instructions</a></li>

        <!-- OPTIONAL_GEO_MIRROR_ITEMS -->
	<li id="geoMirrorMenu" class="noHighlight"><hr></li>
        <li id="serverEuroLink">
            <img alt="X" width="16" height="16" style="float: left;" src="../images/greenChecksm.png">
            <a href="http://genome-euro.ucsc.edu/cgi-bin/hgGateway">Use Europe Server</a></li>
        <li class="last" id="serverUSLink">
            <img alt="X" width="16" height="16" style="float: left;" src="../images/greenChecksm.png">
            <a href="http://genome.ucsc.edu/cgi-bin/hgGateway">Use US Server</a></li>
	</ul>
</li>
<li class="menuparent" id="downloads"><span>Downloads</span>
	<ul style="display: none; visibility: hidden;">
	<li class="first"><a href="http://hgdownload.soe.ucsc.edu/downloads.html">Genome Data</a></li>
	<li><a href="http://hgdownload.soe.ucsc.edu/downloads.html#source_downloads">Source Code</a></li>
	<li><a href="http://hgdownload.soe.ucsc.edu/downloads.html#utilities_downloads">Utilities</a></li>
	<li class="last"><a href="../goldenPath/help/ftp.html">FTP</a></li>
	<li class="last"><a href="../goldenPath/help/mysql.html">MySQL Access</a></li>
	</ul>
</li>
<li class="menuparent" id="myData"><span>My Data</span>
	<ul style="display: none; visibility: hidden;">
	<li><a href="../cgi-bin/hgSession?hgS_doMainPage=1">Sessions</a></li>
	<li><a href="../cgi-bin/hgHubConnect">Track Hubs</a></li>
	<li class="last"><a href="../cgi-bin/hgCustom">Custom Tracks</a></li>
	</ul>
</li>
<li class="menuparent" id="aboutUs"><span>About Us</span>
	<ul style="display: none; visibility: hidden;">
	<li class="first odd"><a href="../goldenPath/newsarch.html">News</a></li>
	<li class="even"><a href="../goldenPath/pubs.html">Publications</a></li>
	<li class="odd"><a href="../cite.html">Cite Us</a></li>
	<li class="even"><a href="../goldenPath/credits.html">Credits</a></li>
	<li class="odd"><a href="../goldenPath/releaseLog.html">Release Log</a></li>
	<li class="even"><a href="../staff.html">Staff</a></li>
	<li class="odd"><a href="../contacts.html">Contact Us</a></li>
	<li class="even"><a href="../conditions.html">Conditions of Use</a></li>
	<li class="odd"><a href="http://www.cbse.ucsc.edu/jobs" TARGET="_blank">Jobs</a></li>
	<li class="even last"><a href="../license/">Licenses</a></li>
</ul>
</li>

<!-- OPTIONAL_VIEW_MENU -->

<li class="menuparent last" id="help"><span>Help</span><ul style="display: none; visibility: hidden;">
<!-- CONTEXT_SPECIFIC_HELP -->
<li><a href="../FAQ/">FAQs</a></li>
<li><a href="../goldenPath/help/hgTracksHelp.html">Browser Documentation</a></li>
<li><a href="../training.html">Training</a></li>
<li><a href="../contacts.html">Mailing Lists</a></li>
<li class="last"><a href="http://genomewiki.ucsc.edu/" target="_blank">GenomeWiki</a></li>
</ul>
</li>
</ul>
</div>
		</div>
	</div>
	<div class="downcorner"></div>

</div>
</div><!-- end main-menu -->
</div><!-- end main-menu-whole -->

<script type="text/javascript">
<!--//--><![CDATA[//><!--
jQuery.extend(Drupal.settings, { "basePath": "/", "nice_menus_options": { "delay": 800, "speed": 1 }, "thickbox": { "close": "Close", "next": "Next \x3e", "prev": "\x3c Prev", "esc_key": "or Esc Key", "next_close": "Next / Close on last", "image_count": "Image !current of !total" }, "custom_search": { "form_target": "_self", "solr": 0 } });
//--><!]]>

/* geographic server (mirror) support

   Customize 'Mirrors' drop-down menu based on current server.  If the server
   is a UCSC-sponsored site (USA/Ca or European), show this in drop-down menu
   with a checkmark, and allow user to change server.  Based on design notes here:
        http://genomewiki.ucsc.edu/genecats/index.php/Euronode

   NOTE: More generalized implementation would use hgcentral.gbNode table
   to populate menu items and locate redirects.  This implementation uses the
   spec from above wiki page:
        browser.node=1 -> US server (genome.ucsc.edu)
        browser.node=2 -> Europe server (genome-euro.ucsc.edu)

   NOTE:  if further JS is needed for nav bar, this should be moved
   into a new globalNavBar.js file

*/

$(function() {
    var hostName = window.location.hostname;

    function hideGeoServers() {
        // remove geo servers and separator from dropdown menu
        $('#serverEuroLink').hide();
        $('#serverUSLink').hide();
        $('#mirrors hr').hide();
    }

    function setGeoServerEuro(isEuro) {
        // show checkmark on active server and disable selecting it
        if (isEuro) {
            $('#serverEuroLink').addClass('noHighlight');
            $('#serverEuroLink a').removeAttr('href');
            $('#serverUSLink img').hide();
        } else {
            $('#serverUSLink').addClass('noHighlight');
            $('#serverUSLink a').removeAttr('href');
            $('#serverEuroLink img').hide();
        }
    }
    if (typeof GB_geoMirror === 'undefined') {
        // development servers and 3rd party mirrors
        hideGeoServers();
        return;
    }

    if (GB_geoMirror == 1) {
        // US server - disable highlight and link for US, hide checkmark for Euro
        setGeoServerEuro(false);
    } else if (GB_geoMirror === 2) {
        // Europe server - disable highlight and link for Europe, hide checkmark for US
        setGeoServerEuro(true);
    } else {
        hideGeoServers();
    }
});

</script>
