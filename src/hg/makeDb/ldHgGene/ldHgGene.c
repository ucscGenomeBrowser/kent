/* ldHgGene - load a set of gene predictions from GFF  or GTF file into
 * mySQL database. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "linefile.h"
#include "gff.h"
#include "jksql.h"
#include "genePred.h"

char *createString = 
"CREATE TABLE %s ( \n"
"   name varchar(255) not null,	# Name of gene \n"
"   chrom varchar(255) not null,	# Chromosome name \n"
"   strand char(1) not null,	# + or - for strand \n"
"   txStart int unsigned not null,	# Transcription start position \n"
"   txEnd int unsigned not null,	# Transcription end position \n"
"   cdsStart int unsigned not null,	# Coding region start \n"
"   cdsEnd int unsigned not null,	# Coding region end \n"
"   exonCount int unsigned not null,	# Number of exons \n"
"   exonStarts longblob not null,	# Exon start positions \n"
"   exonEnds longblob not null,	# Exon end positions \n"
          "   #Indices \n"
"   INDEX(name), \n"
"   INDEX(chrom(12),txStart), \n"
"   INDEX(chrom(12),txEnd) \n"
")";


void usage()
{
errAbort(
    "ldHgGene - load database with gene predictions from a gff file.\n"
    "usage:\n"
    "     ldHgGene database table file(s).gff");
}

void loadIntoDatabase(char *database, char *table, char *tabName)
/* Load tabbed file into database table. Drop and create table. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *ds = newDyString(2048);

dyStringPrintf(ds, createString, table);
sqlMaybeMakeTable(conn, table, ds->string);
dyStringClear(ds);
dyStringPrintf(ds, 
   "delete from %s", table);
sqlUpdate(conn, ds->string);
dyStringClear(ds);
dyStringPrintf(ds, 
   "LOAD data local infile '%s' into table %s", tabName, table);
sqlUpdate(conn, ds->string);
sqlDisconnect(&conn);
freeDyString(&ds);
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

void ldHgGene(char *database, char *table, int gtfCount, char *gtfNames[])
/* Load up database from a bunch of GTF files. */
{
struct gffFile *gff = gffFileNew("");
struct gffGroup *group;
int i;
char *fileName;
int lineCount;
struct genePred *gpList = NULL, *gp;
char *tabName = "genePred.tab";
FILE *f;
boolean isSoftberry = sameWord("softberryGene", table);
boolean isEnsembl = sameWord("ensGene", table);
boolean isSanger22 = sameWord("sanger22", table);

for (i=0; i<gtfCount; ++i)
    {
    fileName = gtfNames[i];
    printf("Reading %s\n", fileName);
    gffFileAdd(gff, fileName, 0);
    }
lineCount = slCount(gff->lineList);
printf("Read %d transcripts in %d lines in %d files\n", 
	slCount(gff->groupList), lineCount, gtfCount);
gffGroupLines(gff);
printf("  %d groups %d seqs %d sources %d feature types\n",
    slCount(gff->groupList), slCount(gff->seqList), slCount(gff->sourceList),
    slCount(gff->featureList));
printf("  %d ungrouped lines\n", slCount(gff->groupList));

/* Convert from gffGroup to genePred representation. */
for (group = gff->groupList; group != NULL; group = group->next)
    {
    char *name = group->name;
    if (isSoftberry)
        {
	name = convertSoftberryName(name);
	}
    gp = genePredFromGroupedGff(gff, group, name, "exon");
    if (gp != NULL)
	slAddHead(&gpList, gp);
    }
printf("%d gene predictions\n", slCount(gpList));
slSort(&gpList, genePredCmp);

/* Create tab-delimited file. */
f = mustOpen(tabName, "w");
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    genePredTabOut(gp, f);
    }
fclose(f);
loadIntoDatabase(database, table, tabName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
ldHgGene(argv[1], argv[2], argc-3, argv+3);
}
