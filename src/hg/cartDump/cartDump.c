/* cartDump - Dump contents of cart. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cart.h"

void doMiddle(struct cart *cart)
/* cartDump - Dump contents of cart. */
{
printf("<TT><PRE>");
cartDump(cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartHtmlShell("Cart Dump", doMiddle, "hguid", NULL, NULL);
return 0;
}
