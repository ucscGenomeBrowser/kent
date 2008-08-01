/* Put up the configuration page. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"
#include "cart.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "web.h"
#include "hgVisiGene.h"
#include "configPage.h"

void configPage()
/* Put up configuration page. */
{
webStartWrapperDetailedNoArgs(cart, NULL, "", "VisiGene Configure",
        FALSE, FALSE, FALSE, TRUE);

printf("<FORM ACTION=\"%s\" METHOD=GET>\n",
	hgVisiGeneCgiName());

printf("<TABLE>");
printf("<TR>\n");
printf("<TD>");
cgiMakeCheckBox(hgpIncludeMutants, cartUsualBoolean(cart, hgpIncludeMutants, FALSE));
printf("</TD><TD>");
printf("include mutants");
printf("</TD>");
printf("</TR>");
printf("</TABLE>\n");

cgiMakeButton("submit", "submit");
printf("</FORM>");
webEnd();
}

