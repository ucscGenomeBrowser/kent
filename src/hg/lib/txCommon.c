/* txCommon - contains routines shared by various programs in
 * the txXxx family (which is used to make the UCSC gene set */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "txCommon.h"

static char const rcsid[] = "$Id: txCommon.c,v 1.5 2008/09/17 18:10:14 kent Exp $";

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
if ((len < 8 && !startsWith("CCDS", s) ) || len > 75)
    {
    errAbort("Accession seems a little too long or short in %s", tempName);
    }
return s;
}

void txGeneAccFromId(int id, char acc[16])
/* Convert ID to accession in uc123ABC format. */
{
if (id >= 17576000)
    errAbort("Out of accessions!");
acc[8] = 0;
acc[7] = id%26 + 'a';
id /= 26;
acc[6] = id%26 + 'a';
id /= 26;
acc[5] = id%26 + 'a';
id /= 26;
acc[4] = id%10 + '0';
id /= 10;
acc[3] = id%10 + '0';
id /= 10;
acc[2] = id%10 + '0';
acc[1] = 'c';
acc[0] = 'u';
}

