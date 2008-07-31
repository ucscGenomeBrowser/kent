/* ldHgGene - load a set of gene predictions from GFF or GTF file into
 * mySQL database. */

#include "common.h"
#include "options.h"
#include "hash.h"
#include "dystring.h"
#include "linefile.h"
#include "hdb.h"
#include "gff.h"
#include "jksql.h"
#include "genePred.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: ldHgGene.c,v 1.38.26.1 2008/07/31 02:24:39 markd Exp $";

char *exonType = "exon";	/* Type field that signifies exons. */
boolean requireCDS = FALSE;     /* should genes with CDS be dropped */
boolean useBin = TRUE;          /* add bin column */
char *outFile = NULL;	        /* Output file as alternative to database. */
unsigned gOptFields = 0;        /* optional fields from cmdline */
unsigned gCreateOpts = 0;       /* table create options from cmdline */
unsigned gGxfOptions = 0;       /* options for converting GTF/GFF */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"exon", OPTION_STRING},
    {"oldTable", OPTION_BOOLEAN},
    {"noncoding", OPTION_BOOLEAN},
    {"nonCoding", OPTION_BOOLEAN},
    {"gtf", OPTION_BOOLEAN},
    {"bin", OPTION_BOOLEAN},
    {"nobin", OPTION_BOOLEAN},
    {"predTab", OPTION_BOOLEAN},
    {"requireCDS", OPTION_BOOLEAN},
    {"genePredExt", OPTION_BOOLEAN},
    {"impliedStopAfterCds", OPTION_BOOLEAN},
    {"out", OPTION_STRING},
    {NULL, 0}
};

void usage()
{
errAbort(
    "ldHgGene - load database with gene predictions from a gff file.\n"
    "usage:\n"
    "     ldHgGene database table file(s).gff\n"
    "options:\n"
    "     -bin         Add bin column (now the default)\n"
    "     -nobin       don't add binning (you probably don't want this)\n"
    "     -exon=type   Sets type field for exons to specific value\n"
    "     -oldTable    Don't overwrite what's already in table\n"
    "     -noncoding   Forces whole prediction to be UTR\n"
    "     -gtf         input is GTF, stop codon is not in CDS\n"
    "     -predTab     input is already in genePredTab format\n"
    "     -requireCDS  discard genes that don't have CDS annotation\n"
    "     -out=gpfile  write output, in genePred format, instead of loading\n"
    "                  table. Database is ignored.\n"
    "     -genePredExt create a extended genePred, including frame\n"
    "                  information and gene name\n"
    "     -impliedStopAfterCds - implied stop codon in GFF/GTF after CDS\n");
}

void loadIntoDatabase(char *database, char *table, char *tabName,
                      bool appendTbl)
/* Load tabbed file into database table. Drop and create table. */
{
struct sqlConnection *conn = sqlConnect(database);

if (!appendTbl)
    {
    char *createSql = genePredGetCreateSql(table,  gOptFields, gCreateOpts,
                                           hGetMinIndexLength(database));
    sqlRemakeTable(conn, table, createSql);
    freeMem(createSql);
    }
sqlLoadTabFile(conn, tabName, table, SQL_TAB_FILE_WARN_ON_WARN);

/* add a comment to the history table and finish up connection */
hgHistoryComment(conn, "Add gene predictions to %s table %s frame info.", table,
                 ((gOptFields & genePredExonFramesFld) ? "with" : "w/o"));
sqlDisconnect(&conn);
}

char *convertSoftberryName(char *name)
/* Convert softberry name to simple form that is same as in
 * softberryPep table. */
{
static char *head = "gene_id S.";
char *s = strrchr(name, '.');

if (strstr(name, head) == NULL)
    errAbort("Unrecognized Softberry name %s, no %s", name, head);
return s+1;
}

void ldHgGenePred(char *database, char *table, int gCount, char *gNames[])
/* Load up database from a bunch of genePred files. */
{
char *tabName = "genePred.tab";
FILE *f;
struct genePred *gpList = NULL, *gp;
int i;

for (i=0; i<gCount; ++i)
    {
    verbose(1, "Reading %s\n", gNames[i]);
    gpList = slCat(genePredExtLoadAll(gNames[i]), gpList);
    }
verbose(1, "%d gene predictions\n", slCount(gpList));
slSort(&gpList, genePredCmp);

/* Create tab-delimited file. */
if (outFile != NULL)
    f = mustOpen(outFile, "w");
else
    f = mustOpen(tabName, "w");
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    if (useBin)
        fprintf(f, "%d\t", hFindBin(gp->txStart, gp->txEnd));
    genePredTabOut(gp, f);
    }
carefulClose(&f);

if (outFile == NULL)
    loadIntoDatabase(database, table, tabName, optionExists("oldTable"));
}

void ldHgGene(char *database, char *table, int gtfCount, char *gtfNames[])
/* Load up database from a bunch of GTF files. */
{
struct gffFile *gff = gffFileNew("");
struct gffGroup *group;
int i;
int lineCount;
struct genePred *gpList = NULL, *gp;
char *tabName = "genePred.tab";
FILE *f;
boolean nonCoding = optionExists("noncoding") || optionExists("nonCoding");
boolean isGtf = optionExists("gtf");

boolean isSoftberry = sameWord("softberryGene", table);

for (i=0; i<gtfCount; ++i)
    {
    verbose(1, "Reading %s\n", gtfNames[i]);
    gffFileAdd(gff, gtfNames[i], 0);
    }
lineCount = slCount(gff->lineList);
verbose(1, "Read %d transcripts in %d lines in %d files\n", 
	slCount(gff->groupList), lineCount, gtfCount);
gffGroupLines(gff);
verbose(1, "  %d groups %d seqs %d sources %d feature types\n",
    slCount(gff->groupList), slCount(gff->seqList), slCount(gff->sourceList),
    slCount(gff->featureList));

/* Convert from gffGroup to genePred representation. */
for (group = gff->groupList; group != NULL; group = group->next)
    {
    char *name = group->name;
    if (isSoftberry)
        {
	name = convertSoftberryName(name);
	}
    if (isGtf)
        gp = genePredFromGroupedGtf(gff, group, name, gOptFields, gGxfOptions);
    else
        gp = genePredFromGroupedGff(gff, group, name, exonType, gOptFields, gGxfOptions);
    if (gp != NULL)
	{
	if (nonCoding)
	    gp->cdsStart = gp->cdsEnd = 0;
        if (requireCDS && (gp->cdsStart == gp->cdsEnd))
            genePredFree(&gp);
        else
            slAddHead(&gpList, gp);
	}
    }
verbose(1, "%d gene predictions\n", slCount(gpList));
slSort(&gpList, genePredCmp);

/* Create tab-delimited file. */
if (outFile != NULL)
    f = mustOpen(outFile, "w");
else
    f = mustOpen(tabName, "w");
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    if (useBin)
        fprintf(f, "%d\t", hFindBin(gp->txStart, gp->txEnd));
    genePredTabOut(gp, f);
    }
carefulClose(&f);

if (outFile == NULL)
    loadIntoDatabase(database, table, tabName, optionExists("oldTable"));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage();
if (optionExists("exon") && optionExists("gtf"))
    errAbort("can't specify -exon= with -gtf");
exonType = optionVal("exon", exonType);
outFile = optionVal("out", NULL);
requireCDS = optionExists("requireCDS");
if (optionExists("bin") && optionExists("nobin"))
    errAbort("can't specify both -bin and -nobin");
useBin = !optionExists("nobin");
if (optionExists("genePredExt"))
    gOptFields |= genePredAllFlds;
if (useBin)
    gCreateOpts |= genePredWithBin;
if (optionExists("impliedStopAfterCds"))
    gGxfOptions |= genePredGxfImpliedStopAfterCds;
if (optionExists("predTab"))
    ldHgGenePred(argv[1], argv[2], argc-3, argv+3);
else
    ldHgGene(argv[1], argv[2], argc-3, argv+3);
return 0;
}
