/* pslDiff - compare psl files */
#include "common.h"
#include "psl.h"
#include "pslTbl.h"
#include "pslSets.h"
#include "options.h"

/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"details", OPTION_STRING},
    {"setNames", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean gHaveSetNames = FALSE;  /* set names in args */
boolean gNumAligns = FALSE;     /* compare by number of alignments */

void usage(char *msg, ...)
/* usage msg and exit */
{
va_list ap;
va_start(ap, msg);
vfprintf(stderr, msg, ap);
errAbort("\n%s",
         "pslDiff [options] psl1 psl2 ...\n"
         "pslDiff [options] -setNames setName1 pslFile1 setName2 pslFile2 ...\n"
         "\n"
         "Compare queries in two or more psl files \n"
         "\n"
         "   -setNames - commmand line specifies name to use for a set of alignments\n"
         "    found in a psl file.  If this is not specified, the set names are the\n"
         "    base nmaes of the psl files\n"
         "   -details=file - write details of psls that differ to this file\n"
         );
}

struct pslSets *createPslSets(int numPslSpecs, char **pslSpecs)
/* create pslSets object from the specified psl files.  */
{
struct pslSets *ps = pslSetsNew(numPslSpecs/(gHaveSetNames ? 2 : 1));
int i;
if (gHaveSetNames)
    {
    for (i = 0; i < numPslSpecs; i += 2)
        pslSetsLoadSet(ps, i/2, pslSpecs[i+1], pslSpecs[i]);
    }
else
    {
    char setName[PATH_LEN];
    for (i = 0; i < numPslSpecs; i++)
        {
        splitPath(pslSpecs[i], NULL, setName, NULL);
        pslSetsLoadSet(ps, i, pslSpecs[i], setName);
        }
    }
return ps;
}

boolean pslSame(struct psl *psl1, struct psl *psl2)
/* determine if two psls (with same query and target) are the same */
{
int iBlk;
if (psl1->blockCount != psl2->blockCount)
    return FALSE;
for(iBlk = 0; iBlk < psl1->blockCount; iBlk++)
    {
    if ((psl1->qStarts[iBlk] != psl2->qStarts[iBlk])
        || (psl1->tStarts[iBlk] != psl2->tStarts[iBlk])
        || (psl1->blockSizes[iBlk] != psl2->blockSizes[iBlk]))
        return FALSE;
    }
return TRUE;
}

boolean allMatchesSame(struct pslMatches *matches)
/* determine if all sets have  matches and are same */
{
int iSet;
if (matches->psls[0] == NULL)
    return FALSE;  /* first set doesn't have a match */
for (iSet = 1; iSet < matches->numSets; iSet++)
    if ((matches->psls[iSet] == NULL)
        || !pslSame(matches->psls[0], matches->psls[iSet]))
        return FALSE;
return TRUE;
}

char getSetCategory(struct pslMatches *matches, int iSet, char *categories)
/* get category letter for a set */
{
int iSet2;
char lastCat = '\0';
if (matches->psls[iSet] == NULL)
    return '-';  /* missing */


/* search for a matching psl, stopping when the first unassigned set is hit */
for (iSet2 = 0; (iSet2 < matches->numSets) && (categories[iSet2] != '\0'); iSet2++)
    {
    if (pslSame(matches->psls[iSet2], matches->psls[iSet]))
        return categories[iSet2];
    else
        lastCat = max(categories[iSet2], lastCat);
    }

if (lastCat == '\0')
    return 'A';  /* first category assigned */
else
    return lastCat+1;
}


void categorizePsls(struct pslMatches *matches, char *categories)
/* generate categories labels for each psl.  All psls that are
 * the same are assigned the same category letter */
{
int iSet;

/* zero so we know which we have set */
memset(categories, 0, matches->numSets);

/* fill in categories */
for (iSet = 0; iSet < matches->numSets; iSet++)
    categories[iSet] = getSetCategory(matches, iSet, categories);
}

void prDiffMatches(FILE *outFh, struct pslSets *ps, char *qName,
                   struct pslMatches *matches)
/* print information about matched psls that are known to have differences */
{
static char *categories = NULL;
int iSet;

/* categories is an array of 1-character values, but add a terminating
 * zero to make it easy to display in a debugger */
if (categories == NULL)
    categories = needMem(ps->numSets+1);
categorizePsls(matches, categories);

fprintf(outFh, "%s\t%s\t%d\t%d", qName,
        matches->tName, matches->tStart, matches->tEnd);
for (iSet = 0; iSet < ps->numSets; iSet++)
    fprintf(outFh, "\t%c", categories[iSet]);
fprintf(outFh, "\n");
}

void diffQuery(FILE *outFh, FILE *detailsFh, struct pslSets *ps, char *qName)
/* diff one query */
{
struct pslMatches *matches;
pslSetsMatchQuery(ps, qName);
for (matches = ps->matches; matches != NULL; matches = matches->next)
    if (!allMatchesSame(matches))
        prDiffMatches(outFh, ps, qName, matches);
}

void pslDiff(int numPslSpecs, char **pslSpecs, char *detailsFile)
/* compare psl files */
{
struct pslSets *ps = createPslSets(numPslSpecs, pslSpecs);
struct slName *queries = pslSetsQueryNames(ps);
FILE *detailsFh = (detailsFile != NULL) ? mustOpen(detailsFile, "w") : NULL;
struct slName *query;
for (query = queries; query != NULL; query = query->next)
    diffQuery(stdout, detailsFh, ps, query->name);

slFreeList(&queries);
pslSetsFree(&ps);
carefulClose(&detailsFh);
}

int main(int argc, char *argv[])
/* Process command line */
{
char *detailsFile;
optionInit(&argc, argv, optionSpecs);
gHaveSetNames = optionExists("setNames");
if (((gHaveSetNames) && (argc < 5))
    || ((!gHaveSetNames) && (argc < 3)))
    usage("wrong # of args:");

if (gHaveSetNames && ((argc-1)&1)) /* must have even number */
    usage("-setNames requires pairs of setName and pslFile");

detailsFile = optionVal("details", NULL);
pslDiff(argc-1, argv+1, detailsFile);
return 0;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

