/* cartReset - Reset cart. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"


void doMiddle()
/* cartReset - Reset cart. */
{
cartResetInDb("hguid");
printf("Your settings are now reset to defaults.");
}

int main(int argc, char *argv[])
/* Process command line. */
{
htmShell("Reset Cart", doMiddle, NULL);
return 0;
}
