/* hgBatch - Use to do batch queries of genome database over web, 
 * now it sends user to Table Browser which has incorporated its functions. */
#include "common.h"
#include "options.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "web.h"
#include "cart.h"
#include "hui.h"
#include "hCommon.h"



static struct cart *cart = NULL;

void doMiddle(struct cart *theCart)
/* Set up a few preliminaries and dispatch to a
 * particular form. */
{
cart = theCart;
char headerText[256];
int redirDelay = 5;
safef(headerText, sizeof(headerText),
      "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"%d;URL=%s\">",
      redirDelay, hgTextName());
webStartHeader(cart, headerText, "hgBatch: replaced by hgText");
puts("The Table Browser now supports batch queries, so this page "
     "has been retired.  \n"
     "You will be automatically redirected to the Table Browser in ");
printf("%d seconds, or you can <BR>\n"
       "<A HREF=\"%s\">click here to continue</A>.\n",
       redirDelay, hgTextName());
webEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", 
	"hgb.userKeys", 
	"hgb.pasteKeys", "hgb.uploadKeys", "hgb.showPasteResults",
	"hgb.showUploadResults"};


int main(int argc, char *argv[])
/* Process command line. */
{
struct cart *theCart;
struct hash *oldVars = hashNew(8);
cgiSpoof(&argc, argv);
// Sometimes we output HTML and sometimes plain text; let each outputter 
// take care of headers instead of using a fixed cart*Shell().
theCart = cartAndCookieWithHtml(hUserCookie(), excludeVars, oldVars, FALSE);
doMiddle(theCart);
cartCheckout(&theCart);
return 0;
}


