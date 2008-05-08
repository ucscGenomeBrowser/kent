#include "common.h"
#include "hgConfig.h"
#include "hPrint.h"
#include "googleAnalytics.h"

static char const rcsid[] = "$Id: googleAnalytics.c,v 1.2 2008/05/08 21:42:07 hiram Exp $";

void googleAnalytics()
{
static boolean done = FALSE;
char *analyticsHosts = cfgOption("analyticsHostList");
char *analyticsKey = cfgOption("analyticsKey");

/*	if either is missing, nothing happens here	*/
if (done || (NULL == analyticsHosts) || (NULL == analyticsKey))
    return;

done = TRUE;	/*	do not repeat this by mistake	*/

char *thisHost = getenv("HTTP_HOST");
if (NULL == thisHost)
    return;

char *hostList[256];
int hostCount = 0;
hostCount = chopByChar(cloneString(analyticsHosts), ',', hostList,
    ArraySize(hostList));
boolean validHost = FALSE;
if (hostCount > 0)
    {
    validHost = FALSE;
    int i;
    for (i = 0; i < hostCount; ++i)
	{
	if (startsWith(hostList[i], thisHost))
	    {
	    validHost = TRUE;
	    break;
	    }
	}
    }

if (validHost)
    {
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
}
