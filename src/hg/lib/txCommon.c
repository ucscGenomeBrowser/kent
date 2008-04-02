/* txCommon - contains routines shared by various programs in
 * the txXxx family (which is used to make the UCSC gene set */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "txCommon.h"

char *txAccFromTempName(char *tempName)
/* Given name in this.that.acc.version format, return
 * just acc.version. */
{
char *s = tempName + strlen(tempName);
int periodCount = 0;
while (s > tempName && periodCount < 2)
    {
    char c = *(--s);
    if (c == '.')
        ++periodCount;
    }
if (periodCount < 2)
    errAbort("Can't extract accession.ver from %s, expecting at least two periods",
    	tempName); 
s += 1;
int len = strlen(s);
if ((len < 8 && !startsWith("CCDS", s) ) || len > 18)
    {
    errAbort("Accession seems a little too long or short in %s", tempName);
    }
return s;
}
