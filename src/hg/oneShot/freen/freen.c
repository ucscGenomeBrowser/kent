/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "rbTree.h"
#include "rangeTree.h"

static char const rcsid[] = "$Id: freen.c,v 1.73 2007/02/16 16:48:29 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

void freen(char *inFile)
/* Test some hair-brained thing. */
{
struct rbTree *tree = rangeTreeNew();
rangeTreeAdd(tree,141674678,141674708);
rangeTreeAdd(tree,141674754,141675090);
rangeTreeAdd(tree,141674797,141675035);
rangeTreeAdd(tree,141674797,141675090);
rangeTreeAdd(tree,141675130,141675136);
rangeTreeAdd(tree,141695673,141695703);
rangeTreeAdd(tree,141695792,141696080);
rangeTreeAdd(tree,141749297,141749327);
rangeTreeAdd(tree,141749366,141749706);
rangeTreeAdd(tree,141749418,141749706);
rangeTreeAdd(tree,141760401,141760452);
rangeTreeAdd(tree,141760584,141760877);
rangeTreeAdd(tree,141776136,141776202);
rangeTreeAdd(tree,141776303,141776590);
rangeTreeAdd(tree,141776497,141776505);
rangeTreeAdd(tree,141813060,141813071);
rangeTreeAdd(tree,141819477,141819526);
rangeTreeAdd(tree,141819618,141819906);
rangeTreeAdd(tree,141819907,141819906);
rangeTreeAdd(tree,141819990,141819994);
rangeTreeAdd(tree,141838008,141838101);
rangeTreeAdd(tree,141838190,141838480);
rangeTreeAdd(tree,141838428,141838478);
rangeTreeAdd(tree,141890129,141890146);
rangeTreeAdd(tree,141932851,141932873);
rangeTreeAdd(tree,141932979,141933271);
rangeTreeAdd(tree,141933272,141933271);
rangeTreeAdd(tree,141933317,141933322);
rangeTreeAdd(tree,141933380,141933386);
rangeTreeAdd(tree,141937028,141937034);
rangeTreeAdd(tree,141949662,141949669);
rangeTreeAdd(tree,141951595,141951602);
rangeTreeAdd(tree,142058171,142058220);
rangeTreeAdd(tree,142058344,142058638);
rangeTreeAdd(tree,142058633,142058637);
struct range *r = rangeTreeFindEnclosing(tree,141819907,141819906);
printf("r is %p\n", r);
struct range t;
t.start = 141819907;
t.end = 141819906;
r = rbTreeFind(tree, &t);
printf("r now is %p\n", r);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000*1024L);
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
