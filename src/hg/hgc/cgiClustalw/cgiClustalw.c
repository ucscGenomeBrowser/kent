/* cgiClustalw - run clustalw on a given sequence */

#include "common.h"
#include "obscure.h"
#include "hCommon.h"
#include "hash.h"
#include "hdb.h"
#include "fa.h"
#include "cheapcgi.h"
#include "portable.h"
#include "jalview.h"
#include "web.h"

static char const rcsid[] = "$Id: cgiClustalw.c,v 1.5 2006/03/06 18:42:33 angie Exp $";

int main(int argc, char *argv[])
{
char *faIn=cgiOptionalString("fa");
struct tempName alnTn;
struct lineFile *lf;
char *line = NULL;
char clustal[512];
int ret;

cgiSpoof(&argc,argv);

if (faIn == NULL)
    webAbort("Error", "Requires input fa file");

printf("\n");
makeTempName(&alnTn, "clustal", ".aln");
safef(clustal, sizeof(clustal), \
    "/usr/local/bin/clustalw -infile=%s -outfile=%s -quicktree > /dev/null ", \
    faIn, alnTn.forCgi);
ret = system(clustal);    
if (ret != 0)
    errAbort("Error in clustalW");
lf = lineFileOpen(alnTn.forCgi,TRUE);
while (lineFileNext(lf, &line, NULL))
    printf("%s\n",line);
lineFileClose(&lf);
return 0;
}
