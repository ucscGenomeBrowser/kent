/* cartReset - Reset cart. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"

static char const rcsid[] = "$Id: cartReset.c,v 1.4 2003/05/06 07:22:14 kate Exp $";


void doMiddle()
/* cartReset - Reset cart. */
{
cartResetInDb("hguid");
printf("Your settings are now reset to defaults.<BR>");
printf("You will be automatically redirected to the gateway page in 2 seconds,\n"
" or you can <BR> <A href=\"/cgi-bin/hgGateway\">click here to continue</A>.\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *headText = "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"2;URL=/cgi-bin/hgGateway\">";
htmShellWithHead("Reset Cart", headText, doMiddle, NULL);
return 0;
}
