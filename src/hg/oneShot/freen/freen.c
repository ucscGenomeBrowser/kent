/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "bed.h"
#include "jksql.h"
#include "altGraphX.h"


struct altGraphX *altGraphXLoadFoo(char **row)
/* Load a altGraphX from row fetched with select * from altGraphX
 * from database.  Dispose of this with altGraphXFree(). */
{
struct altGraphX *ret;
int sizeOne,i;
char *s;

AllocVar(ret);
ret->vertexCount = sqlUnsigned(row[6]);
ret->edgeCount = sqlUnsigned(row[9]);
ret->mrnaRefCount = sqlSigned(row[14]);
ret->tName = cloneString(row[0]);
ret->tStart = sqlSigned(row[1]);
ret->tEnd = sqlSigned(row[2]);
ret->name = cloneString(row[3]);
ret->id = sqlUnsigned(row[4]);
strcpy(ret->strand, row[5]);
sqlUbyteDynamicArray(row[7], &ret->vTypes, &sizeOne);
assert(sizeOne == ret->vertexCount);
sqlSignedDynamicArray(row[8], &ret->vPositions, &sizeOne);
assert(sizeOne == ret->vertexCount);
sqlSignedDynamicArray(row[10], &ret->edgeStarts, &sizeOne);
assert(sizeOne == ret->edgeCount);
sqlSignedDynamicArray(row[11], &ret->edgeEnds, &sizeOne);
assert(sizeOne == ret->edgeCount);
#ifdef SOON
s = row[12];
for (i=0; i<ret->edgeCount; ++i)
    {
    s = sqlEatChar(s, '{');
    slSafeAddHead(&ret->evidence, evidenceCommaIn(&s, NULL));
    s = sqlEatChar(s, '}');
    s = sqlEatChar(s, ',');
    }
slReverse(&ret->evidence);
sqlSignedDynamicArray(row[13], &ret->edgeTypes, &sizeOne);
assert(sizeOne == ret->edgeCount);
sqlStringDynamicArray(row[15], &ret->mrnaRefs, &sizeOne);
assert(sizeOne == ret->mrnaRefCount);
sqlSignedDynamicArray(row[16], &ret->mrnaTissues, &sizeOne);
assert(sizeOne == ret->mrnaRefCount);
sqlSignedDynamicArray(row[17], &ret->mrnaLibs, &sizeOne);
assert(sizeOne == ret->mrnaRefCount);
#endif /* SOON */
return ret;
}

static char const rcsid[] = "$Id: freen.c,v 1.77 2007/09/03 15:31:48 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

void freen(char *track, char *item)
/* Test some hair-brained thing. */
{
struct sqlConnection *conn = sqlConnect("hg18");
struct sqlResult *sr = sqlGetResult(conn, "select * from sibTxGraph where name = 'NC_000020_1157'");
char **row = sqlNextRow(sr);
struct altGraphX *ag = altGraphXLoadFoo(row+1);
printf("%s has %d edges, %d vertices, %d mrnas\n", ag->tName, ag->edgeCount, ag->vertexCount,
    char *tName;	/* name of target sequence, often a chrom. */
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
