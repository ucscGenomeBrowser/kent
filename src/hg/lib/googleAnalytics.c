#include "common.h"
#include "hgConfig.h"
#include "hPrint.h"
#include "googleAnalytics.h"

static char const rcsid[] = "$Id: googleAnalytics.c,v 1.4 2008/05/16 19:04:40 hiram Exp $";

void googleAnalytics()
/* check for analytics configuration item and output google hooks if OK */
{
static boolean done = FALSE;

if (done)
    return;

done = TRUE;	/*	do not repeat this by mistake	*/

char *analyticsKey = cfgOption("analyticsKey");

/*	if config is missing, nothing happens here	*/
if (NULL == analyticsKey)
    return;


hPrintf("\n<script type=\"text/javascript\">\n"
"var gaJsHost = ((\"https:\" == document.location.protocol) ? \"https://ssl.\" : \"http://www.\");\n"
"document.write(unescape(\"%%3Cscript src='\" + gaJsHost + \"google-analytics.com/ga.js' type='text/javascript'%%3E%%3C/script%%3E\"));\n"
"</script>\n"
"<script type=\"text/javascript\">\n"
"var pageTracker = _gat._getTracker(\"%s\");\n"
"pageTracker._initData();\n"
"pageTracker._trackPageview();\n"
"</script>\n", analyticsKey);

}
