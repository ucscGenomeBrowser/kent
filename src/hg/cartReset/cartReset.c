/* cartReset - Reset cart. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hui.h"
#include "cart.h"



static char *defaultDestination = "../cgi-bin/hgGateway";

void doMiddle()
/* cartReset - Reset cart. */
{
char *destination = cgiUsualString("destination", defaultDestination);

cartResetInDb(hUserCookie());
printf("Your settings are now reset to defaults.<BR>");
printf("You will be automatically redirected to the gateway page in 2 seconds,\n"
" or you can <BR> <A href=\"%s\">click here to continue</A>.\n",
       destination);
}

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
struct dyString *headText = newDyString(512);
char *destination = cgiUsualString("destination", defaultDestination);

dyStringPrintf(headText,
	       "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"2;URL=%s\">"
	       "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">"
	       "<META HTTP-EQUIV=\"Expires\" CONTENT=\"-1\">"
	       ,destination);
htmShellWithHead("Reset Cart", headText->string, doMiddle, NULL);
dyStringFree(&headText);
cgiExitTime("cartReset", enteredMainTime);
return 0;
}
