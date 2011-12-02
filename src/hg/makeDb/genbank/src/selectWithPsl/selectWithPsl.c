#include "common.h"
struct sqlConnection;
#include "options.h"
#include "linefile.h"
#include "errabort.h"
#include "hash.h"
#include "psl.h"
#include "estOrientInfo.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"fmt", OPTION_STRING},
    {"startOff", OPTION_LONG_LONG},
    {"endOff", OPTION_LONG_LONG},
    {NULL, 0}
};

#define ROW_KEY_BUFSZ 512 /* buffer size to use for a joined row key */

void usage()
/* Explain usage and exit */
{
errAbort("selectWithPsl -fmt=fmt selectPsl in out\n\n"
         "Select and sort a file based on the same query and target\n"
         "ranges occuring in a PSL file.  This can sort orientInfo\n"
         "files produced by polyInfo or other psl files.  This is used\n"
         "to produce a matching set of rows when the select PSL\n"
         "file have been filtered.  A byte subrange of the PSL file maybe\n"
         "specified.\n\n"
         "Options:\n"
         "   -fmt=fmt - format is 'psl' or 'oi'.\n"
         "   -startOff=off - start reading PSLs at this byte offset.\n"
         "   -endOff=off - start reading PSLs at this byte offset.\n");
}

void makeRowKey(char *chrom, unsigned start, unsigned end, char *name,
                char *key)
/* generate a row key, this combines chrom coords and name to form
 * a name that can be hashed */
{
safef(key, ROW_KEY_BUFSZ, "%s:%u-%u %s", chrom, start, end, name);
}

boolean isFirstSelected(struct hash *keyHash, char *chrom,
                        unsigned start, unsigned end, char *name)
/* determine if an alignment is selected and this is the first occurance */
{
char key[ROW_KEY_BUFSZ];
struct hashEl* hel;
makeRowKey(chrom, start, end, name, key);
hel = hashLookup(keyHash, key);
if ((hel != NULL) && (((size_t)(hel->val)++) == 0))
    return TRUE;
else
    return FALSE;
}

struct hash *loadPslRowKeys(char* pslFile, long long startOff,
                            long long endOff)
/* load a psl file into a hash table of row keys (no value) */
{
char key[ROW_KEY_BUFSZ];
struct hash *keyHash = hashNew(20);
struct lineFile *lf = lineFileOpen(pslFile, TRUE);
struct psl *psl;

if (startOff > 0)
    lineFileSeek(lf, startOff, SEEK_SET);

while (((endOff < 0) || (lineFileTell(lf) < endOff))
       && ((psl = pslNext(lf)) != NULL))
    {
    makeRowKey(psl->tName, psl->tStart, psl->tEnd, psl->qName, key);
    hashAdd(keyHash, key, (void*)0);
    pslFree(&psl);
    }
lineFileClose(&lf);
return keyHash;
}

struct estOrientInfo *loadOrientInfo(char *oiFile, struct hash *keyHash)
/* load an orientInfo file, filtering by the keyHash */
{
struct estOrientInfo *eoiList = NULL;
struct lineFile *lf = lineFileOpen(oiFile, TRUE);
char *row[EST_ORIENT_INFO_NUM_COLS];

while (lineFileNextRowTab(lf, row, ArraySize(row)))
    {
    struct estOrientInfo *eoi = estOrientInfoLoad(row);
    if (isFirstSelected(keyHash, eoi->chrom, eoi->chromStart, eoi->chromEnd,
                        eoi->name))
        slAddHead(&eoiList, eoi);
    else
        estOrientInfoFree(&eoi);
    }
lineFileClose(&lf);
return eoiList;
}

int estOrientInfoCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct estOrientInfo *a = *((struct estOrientInfo **)va);
const struct estOrientInfo *b = *((struct estOrientInfo **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
if (dif == 0)
    dif = a->chromEnd - b->chromEnd;
if (dif == 0)
    dif = strcmp(a->name, b->name);
return dif;
}

void selectOrientInfo(struct hash *keyHash, char *inOi, char *outOi)
/* select and sort oriention info files */
{
struct estOrientInfo *eoiList = loadOrientInfo(inOi, keyHash);
struct estOrientInfo *eoi;
FILE *outFh;

slSort(&eoiList, estOrientInfoCmp);

outFh = mustOpen(outOi, "w");
for (eoi = eoiList; eoi != NULL; eoi = eoi->next)
    {
    estOrientInfoTabOut(eoi, outFh);
    if (ferror(outFh))
        errnoAbort("write failed: %s", outOi);
    }
carefulClose(&outFh);
}

struct psl *loadPsl(char *pslFile, struct hash *keyHash)
/* load a psl file, filtering by the keyHash */
{
struct psl *pslList = NULL;
struct psl *psl;
struct lineFile *lf = lineFileOpen(pslFile, TRUE);

while ((psl = pslNext(lf)) != NULL)
    {
    if (isFirstSelected(keyHash, psl->tName, psl->tStart, psl->tEnd,
                        psl->qName))
        slAddHead(&pslList, psl);
    else
        pslFree(&psl);
    }
lineFileClose(&lf);
return pslList;
}

void selectPsl(struct hash *keyHash, char *inPsl, char *outPsl)
/* select and sort oriention info files */
{
struct psl *pslList = loadPsl(inPsl, keyHash);
struct psl *psl;
FILE *outFh;

slSort(&pslList, pslCmpTarget);

outFh = mustOpen(outPsl, "w");
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    pslTabOut(psl, outFh);
    if (ferror(outFh))
        errnoAbort("write failed: %s", outPsl);
    }
carefulClose(&outFh);
}

void selectWithPsl(char *fmt, char *selectPslFile, long long startOff,
                   long long endOff, char *in, char *out)
/* select and sort files */
{
/* we don't bother freeing anything after initial load */
struct hash *keyHash = loadPslRowKeys(selectPslFile, startOff, endOff);
if (sameString(fmt, "oi"))
    selectOrientInfo(keyHash, in, out);
else
    selectPsl(keyHash, in, out);
}

int main(int argc, char **argv)
/* command line parsing */
{
char *fmt;
int numOffOpts = 0;
long long startOff, endOff;
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
fmt = optionVal("fmt", NULL);
if (fmt == NULL)
    errAbort("must specify -fmt");
if (optionExists("startOff"))
    numOffOpts++;
if (optionExists("endOff"))
    numOffOpts++;
if (numOffOpts == 1)
    errAbort("must specify neither or both of -startOff and -endOff");
startOff = optionLongLong("startOff", -1);
endOff = optionLongLong("endOff", -1);
if (!(sameString(fmt, "oi") || sameString(fmt, "psl")))
    errAbort("invalid value for -fmt, expect either \"oi\" or \"psl\"");

selectWithPsl(fmt, argv[1], startOff, endOff, argv[2], argv[3]);

return 0;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

