#include "common.h"
#include "paraLib.h"


char paraSig[] = "0d2f070562685f29";  /* Mild security measure. */
int paraPort = 0x46DC;		      /* Our port */

char *getHost()
/* Return host name. */
{
static char *host = NULL;
if (host == NULL)
    {
    host = getenv("HOST");
    if (host == NULL)
	errAbort("Couldn't find HOST environment variable");
    }
return host;
}
