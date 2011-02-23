/* gtfToGenePred - convert a GTF file to a genePred. */
#include "common.h"
#include "linefile.h"
#include "gff.h"
#include "genePred.h"
#include "errCatch.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gtfToGenePred - convert a GTF file to a genePred\n"
  "usage:\n"
  "   gtfToGenePred gtf genePred\n"
  "\n"
  "options:\n"
  "     -genePredExt - create a extended genePred, including frame\n"
  "      information and gene name\n"
  "     -allErrors - skip groups with errors rather than aborting.\n"
  "      Useful for getting infomation about as many errors as possible.\n"
  "     -infoOut=file - write a file with information on each transcript\n"
  "     -sourcePrefix=pre - only process entries where the source name has the\n"
  "      specified prefix.  May be repeated.\n"
  "     -impliedStopAfterCds - implied stop codon in after CDS\n"
  "     -simple    - just check column validity, not hierarchy, resulting genePred may be damaged\n"
  "     -geneNameAsName2 - if specified, use gene_name for the name2 field\n"
  "      instead of gene_id.\n");
}

static struct optionSpec options[] = {
    {"simple", OPTION_BOOLEAN},
    {"genePredExt", OPTION_BOOLEAN},
    {"allErrors", OPTION_BOOLEAN},
    {"infoOut", OPTION_STRING},
    {"sourcePrefix", OPTION_STRING|OPTION_MULTI},
    {"impliedStopAfterCds", OPTION_BOOLEAN},
    {"geneNameAsName2", OPTION_BOOLEAN},
    {NULL, 0},
};
boolean clGenePredExt = FALSE;  /* include frame and geneName */
boolean clAllErrors = FALSE;    /* report as many errors as possible */
struct slName *clSourcePrefixes; /* list of source prefixes to match */
unsigned clGxfOptions = 0;       /* options for converting GTF/GFF */
boolean doSimple = FALSE;      /* only check column validity */

int badGroupCount = 0;  /* count of inconsistent groups found */


/* header for info file */
static char *infoHeader = "#transId\tgeneId\tsource\tchrom\tstart\tend\tstrand\tproteinId\tgeneName\ttranscriptName\n";

static void saveName(char **name, char *newName)
/* if name references NULL, and newName is not NULL, update name */
{
if ((*name == NULL) && (newName != NULL))
    *name = newName;
}
static void writeInfo(FILE *infoFh, struct gffGroup *group)
/* write a row for a GTF group from the info file */
{

// scan lineList for group and protein ids
struct gffLine *ll;
char *geneId = NULL, *proteinId = NULL, *geneName = NULL, *transcriptName = NULL;
for (ll = group->lineList; ll != NULL; ll = ll->next)
    {
    saveName(&geneId, ll->geneId);
    saveName(&proteinId, ll->proteinId);
    saveName(&geneName, ll->geneName);
    saveName(&transcriptName, ll->transcriptName);
    }

fprintf(infoFh, "%s\t%s\t%s\t%s\t%d\t%d\t%c\t%s\t%s\t%s\n",
        group->name, emptyForNull(geneId), group->source,
        group->seq, group->start, group->end, group->strand,
        emptyForNull(proteinId), emptyForNull(geneName),
        emptyForNull(transcriptName));
}

static void gtfGroupToGenePred(struct gffFile *gtf, struct gffGroup *group, FILE *gpFh,
                               FILE *infoFh)
/* convert one gtf group to a genePred */
{
unsigned optFields = (clGenePredExt ? genePredAllFlds : 0);
struct errCatch *errCatch = errCatchNew();

if (errCatchStart(errCatch))
    {
    struct genePred *gp = genePredFromGroupedGtf(gtf, group, group->name, optFields, clGxfOptions);
    if (gp == NULL)
        {
        if (clAllErrors)
            fprintf(stderr,"no exons defined for %s\n", group->name);
        else
            errAbort("no exons defined for %s", group->name);
        badGroupCount++;
        }
    else
        {
        genePredTabOut(gp, gpFh);
        genePredFree(&gp);
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    // drop trailing newline in caught message
    int l = strlen(errCatch->message->string);
    if ((l > 0) && (errCatch->message->string[l-1] == '\n'))
        errCatch->message->string[l-1] = '\0';
    if (clAllErrors)
        fprintf(stderr, "%s\n", errCatch->message->string);
    else
        errAbort("%s", errCatch->message->string);
    badGroupCount++;
    }
else
    {
    if (infoFh != NULL)
        writeInfo(infoFh, group);
    }
errCatchFree(&errCatch); 
}

static bool sourceMatches(struct gffGroup *group)
/* see if the source matches on on the list */
{
struct slName *pre = NULL;
for (pre = clSourcePrefixes; pre != NULL; pre = pre->next)
    if (startsWith(pre->name, group->source))
        return TRUE;
return FALSE;
}
        

static bool inclGroup(struct gffGroup *group)
/* check if a group should be included in the output */
{
if (clSourcePrefixes != NULL)
    {
    if (!sourceMatches(group))
        return FALSE;
    }
return TRUE;
}

static void gtfToGenePred(char *gtfFile, char *gpFile, char *infoFile)
/* gtfToGenePred -  convert a GTF file to a genePred.. */
{
struct gffFile *gtf = gffRead(gtfFile);
FILE *gpFh, *infoFh = NULL;
struct gffGroup *group;

if (!gtf->isGtf)
    errAbort("%s doesn't appear to be a GTF file (GFF not supported by this program)", gtfFile);
gffGroupLines(gtf);
gpFh = mustOpen(gpFile, "w");
if (infoFile != NULL)
    {
    infoFh = mustOpen(infoFile, "w");
    fputs(infoHeader, infoFh);
    }

if (!doSimple)
    for (group = gtf->groupList; group != NULL; group = group->next)
	if (inclGroup(group))
	    gtfGroupToGenePred(gtf, group, gpFh, infoFh);

carefulClose(&gpFh);
gffFileFree(&gtf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clGenePredExt = optionExists("genePredExt");
doSimple = optionExists("simple");
clAllErrors = optionExists("allErrors");
clSourcePrefixes = optionMultiVal("sourcePrefix", NULL);
if (optionExists("impliedStopAfterCds"))
    clGxfOptions |= genePredGxfImpliedStopAfterCds;
if (optionExists("geneNameAsName2"))
    clGxfOptions |= genePredGxfGeneNameAsName2;

gtfToGenePred(argv[1], argv[2], optionVal("infoOut", NULL));
if (badGroupCount > 0)
    errAbort("%d errors", badGroupCount);
return 0;
}
