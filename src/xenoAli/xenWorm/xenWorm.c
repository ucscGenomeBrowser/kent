/* xenWorm.c - CGI script to align a piece of DNA from another species
 * to C. elegans. */
/* dynAlign.c - align using dynamic programming. */
#include "common.h"
#include "memalloc.h"
#include "portable.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nt4.h"
#include "fa.h"
#include "wormdna.h"
#include "crudeali.h"
#include "xenalign.h"

boolean isOnWeb;  /* True if run from Web rather than command line. */

struct nt4 **chrom;
int chromCount;

void dustData(char *in,char *out)
/* Clean up data by getting rid non-alphabet stuff. */
{
char c;
while ((c = *in++) != 0)
    if (isalpha(c)) *out++ = tolower(c);
*out++ = 0;
}

void doMiddle()
{
char *query = cgiString("sequence");
int qSize;
int maxQ = 8000;
long startTime, endTime;
long clock1000();

dustData(query, query);
printf("<PRE><TT>");
qSize = strlen(query);
printf("query size %d\n", qSize);
if ( qSize <= maxQ)
    {
    printf("This will take about %4.1f minutes to align on a vintage 1999 server\n",
        (double)qSize*14.5/5000.0 + 0.5);
    fflush(stdout);
    startTime = clock1000();
    xenAlignWorm(query, qSize, stdout, TRUE);
    endTime = clock1000();
    printf("%4.2f minutes to align\n", (double)(endTime-startTime)/(60*1000));
    }
else
    errAbort("%d bases in query - too big to handle.\n"
             "Maximum is %d bases in query.",
             qSize, maxQ);
}

int main(int argc, char *argv[])
{
dnaUtilOpen();
if ((isOnWeb = cgiIsOnWeb()) == TRUE)
    {
    htmShell("X to C. elegans Results", doMiddle, NULL);
    }
return 0;
}