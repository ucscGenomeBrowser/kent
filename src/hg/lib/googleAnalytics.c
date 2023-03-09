/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hgConfig.h"
#include "hPrint.h"
#include "googleAnalytics.h"
#include "htmshell.h"

void googleAnalyticsSetGa4Key()
/* if the google analytics key is a GA4 key, set the variable in htmlshell */
{
char *analyticsKey = cfgOption("analyticsKey");
if (analyticsKey && startsWith("G-", analyticsKey))
    htmlSetGa4Key(analyticsKey);
}

void googleAnalytics()
/* check for analytics configuration item and output google hooks if OK */
{
static boolean done = FALSE;

if (done)
    return;

done = TRUE;	/*	do not repeat this by mistake	*/

char *analyticsKey = cfgOption("analyticsKey");

/*	if config is missing or empty, nothing happens here	*/
if (isEmpty(analyticsKey))
    return;

char* scriptName = getenv("SCRIPT_NAME");
if (!scriptName)
    return;
char* cgiName = basename(scriptName);

// new GA4 tags start with G- and should be used with the entirely new system called Google Tag Manager,
// which is a different Javascript, new functions, totally different features.
if (startsWith("G-", analyticsKey))
    {
    jsInlineF(
        "// Google tag load (gtag.js)\n"
        "   window.dataLayer = window.dataLayer || [];\n"
        "   function gtag(){dataLayer.push(arguments);}\n"
        "   gtag('js', new Date()); gtag('config', '%s');\n"
        "// Google tag load end\n", analyticsKey);

    if (sameWord(cgiName, "hgTracks"))
        jsInlineF(
        "  $(document).ready(function() {\n"
        "          if (gtag) {\n" // When using an Adblocker, the ga object does not exist
        "              /* send db to ga4 as an event on page load */\n"
        "              gtag('event', 'hgTracksLoad', {'db': getDb()})\n"
        "          };\n"
        "  });");
    return;
    }

// link tracking broke in Firefox once. In case this ever happens again, we can
// now switch off outlink tracking quickly
boolean trackClicks = cfgOptionBooleanDefault("analytics.trackClicks", TRUE);
boolean trackButtons = cfgOptionBooleanDefault("analytics.trackButtons", TRUE);

// replace analytics.js below with analytics_debug.js to activate ga debugging
// It will log all events and all data that is sent to the javascript console, very handy
jsInlineF(
"  (function(i,s,o,g,r,a,m){i['GoogleAnalyticsObject']=r;i[r]=i[r]||function(){\n"
"  (i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();a=s.createElement(o),\n"
"  m=s.getElementsByTagName(o)[0];a.async=1;a.src=g;m.parentNode.insertBefore(a,m)\n"
"  })(window,document,'script','//www.google-analytics.com/analytics.js','ga');\n"
"  ga('create', '%s', 'auto');\n"
"  ga('require', 'displayfeatures');\n"
"  ga('set', 'transport', 'beacon');\n" // default to beacons. Old browser cannot use them, so ga will use an alternative.
"  ga('send', 'pageview');\n"
"\n"
, analyticsKey);

if (sameWord(cgiName, "hgc") || sameWord(cgiName, "hgTracks") || 
        sameWord(cgiName, "hgGene") || sameWord(cgiName, "hgTrackUi"))
{
    // see https://support.google.com/analytics/answer/1136920?hl=en
    jsInlineF(
    "  function onExtAnchorClick(ev) {\n"
    "  /* user clicked an anchor: if external link, send event to Google analytics and navigate to the link */\n"
    "      var url = ev.currentTarget.href;\n"
    "      var hostname = null;\n"
    "      if (typeof URL===undefined) {\n"
    "          hostname = url.split('//')[1].split('/')[0];\n" // for MSIE
    "      } else {\n"
    "          var urlObj = new URL(url);\n"
    "          hostname = urlObj.hostname;\n"
    "          if (hostname.indexOf('.ncbi.')!==-1)\n" // for NCBI, we keep the first part of the pathname, as all DBs share the base URL
    "              hostname = hostname+'/'+urlObj.pathname.split('/')[1];\n"
    "      }\n"
    "      ga('send', 'event', 'outlink', hostname, url);\n" // beacon is default, see above.
    "\n"
    "      window.open(url);\n" // This may stop the previous ga event that is still running, but beacons should get through
    "      return false;\n"
    "  }\n"
    "  function gaTrackDb(db) {\n"
    "          if (!window.ga || ga.loaded)\n" // When using an Adblocker, the ga object does not exist
    "              return;\n"
    "          /* send db to ga as an event */\n"
    "          ga('send', 'event', 'hgTracks', 'load', getDb());\n"
    "}\n"
    );
    if (trackClicks)
        jsInlineF(
        "\n"
        "  $(document).ready(function() {\n"
        "      if (!window.ga || ga.loaded)\n" // When using an Adblocker, the ga object does not exist
        "          return;\n"
        "      var anchors = document.getElementsByTagName('a');\n"
        "      for (var i = 0; i < anchors.length; i++) { \n"
        "           var a = anchors[i];\n"
        "           var isExternal = (a.target.toLowerCase()==='_blank');\n"
        "           if (a.attributes.href && a.attributes.href.value!=='#' && isExternal)\n" // do not run on javascript links
        "               a.onclick = onExtAnchorClick;" // addEventHandler would not work here, the default click stops propagation.
        "      };\n"
        "      // on hgTracks: send an event with the current db, so we can report popularity of assemblies to NIH\n"
        "      if (typeof hgTracks !== 'undefined')\n"
        "          gaTrackDb(getDb());\n"
        "  });"
        );
    if (trackButtons)
        jsInlineF(
        "  $(document).ready(function() {\n"
        "      gaTrackButtons();\n" // see utils.js
        "  });");
}
}
