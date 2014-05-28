/* hgSanger22 - Load up database with Sanger 22 annotations. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "obscure.h"
#include "jksql.h"
#include "sanger22extra.h"
#include "gff.h"
#include "genePred.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSanger22 - Load up database with Sanger 22 annotations\n"
  "usage:\n"
  "   hgSanger22 database transcript.gff cds.gff mrna.fa aa.fa shortArmSize\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

char *createGenePred = 
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

char *createExtra = 
"CREATE TABLE %s (\n"
"    name varchar(255) not null,	# Transcript name\n"
"    locus varchar(255) not null,	# Possibly biological short name\n"
"    description longblob not null,	# Description from Sanger gene GFFs\n"
"    geneType varchar(255) not null,	# Type field from Sanger gene GFFs\n"
"    cdsType varchar(255) not null,	# Type field from Sanger CDS GFFs\n"
"              #Indices\n"
"    PRIMARY KEY(name)\n"
")\n";

void loadIntoDatabase(char *database, char *createString, char *table, char *tabName)
/* Load tabbed file into database table. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *ds = newDyString(2048);
sqlDyStringPrintf(ds, createString, table);
sqlRemakeTable(conn, table, ds->string);
dyStringClear(ds);
sqlDyStringPrintf(ds, 
   "LOAD data local infile '%s' into table %s", tabName, table);
sqlUpdate(conn, ds->string);
sqlDisconnect(&conn);
freeDyString(&ds);
}

boolean lineToGffFields(struct lineFile *lf, char *line, char *fields[9])
/* Convert first eight space-separated fields to first 8
 * gff items, and stick rest of line in last item. */
{
char *s, *word;
int i;

/* Pretty much ignore comments and blank lines. */
s = skipLeadingSpaces(line);
if (s[0] == '#' || s[0] == '0')
    return FALSE;

for (i=0; i<8; ++i)
    {
    word = nextWord(&s);
    if (word == NULL)
	errAbort("Expecting at least 8 words line %d of %s", lf->lineIx, lf->fileName);
    fields[i] = word;
    }
fields[8] = skipLeadingSpaces(s);
return TRUE;
}

void writeGffLine(FILE *f, char *fields[9])
/* Output tab-separated line. */
{
int i;
for (i=0; i<8; ++i)
   fprintf(f, "%s\t", fields[i]);
fprintf(f, "%s\n", fields[8]);
}

char *findVal(struct lineFile *lf, char *group, char *key)
/* Return value that matches key in group or NULL. */
{
char *s, *var, *val;
static char buf[512];

if (strlen(group) >= sizeof(buf))
    errAbort("Line too long line %d of %s", lf->lineIx, lf->fileName);
strcpy(buf, group);
s = buf;
for (;;)
    {
    var = nextWord(&s);
    if (var == NULL)
        return "";
    s = skipLeadingSpaces(s);
    if (s == NULL || s[0] == 0)
        errAbort("Unmatched key/val pair in group line %d of %s", lf->lineIx, lf->fileName);
    val = s;
    if (s[0] == '\'' || s[0] == '"')
	{
        if (!parseQuotedString(val, val, &s))
	    errAbort("Unmatched quote line %d of %s", lf->lineIx, lf->fileName);
	}
    else
        {
	int end;
	val = nextWord(&s);
	end = strlen(val) - 1;
	if (val[end] == ';')
	     val[end] = 0;
	}
    s = skipLeadingSpaces(s);
    if (s != NULL && s[0] == ';')
        s += 1;
    if (sameString(key, var))
	{
	subChar(val, '\t', ' ');
        return val;
	}
    }
}

struct sanger22extra *sanger22extraNew(char *name)
/* Allocate a new extra.  Don't try to free this though. */
{
struct sanger22extra *extra;
AllocVar(extra);
extra->name = cloneString(name);
extra->geneType = extra->cdsType = extra->locus =  extra->description = "";
return extra;
}

void processOneGff(char *gffName, FILE *f, char *exonType, struct hash *extraHash,
	struct sanger22extra **pExtraList, boolean isCds)
/* Parse through a GFF and store it in f, hash, and list. */
{
char *line, *name, *group, *fields[9];
struct lineFile *lf = lineFileOpen(gffName, TRUE);
struct sanger22extra *extra;

while (lineFileNext(lf, &line, NULL))
    {
    if (lineToGffFields(lf, line, fields))
	{
	fields[0] = "chr22";
	group = fields[8];
	fields[8] = name = findVal(lf, group, "Sequence");
	if (name == NULL)
	    continue;
	if (endsWith(name, ".mRNA"))
	    chopSuffix(name);
	if (sameString(fields[2], "exon"))
	    {
	    fields[2] = exonType;
	    writeGffLine(f, fields);
	    if ((extra = hashFindVal(extraHash, name)) == NULL)
		{
		extra = sanger22extraNew(name);
		hashAdd(extraHash, name, extra);
		slAddHead(pExtraList, extra);
		}
	    if (isCds)
		extra->cdsType = cloneString(fields[1]);
	    else
		extra->geneType = cloneString(fields[1]);
	    }
	else if (sameString(fields[2], "Sequence"))
	    {
	    if ((extra = hashFindVal(extraHash, name)) == NULL)
		{
		extra = sanger22extraNew(name);
		hashAdd(extraHash, name, extra);
		slAddHead(pExtraList, extra);
		}
	    if (extra->description[0] == 0)
		extra->description = cloneString(findVal(lf, group, "Description"));
	    if (extra->locus[0] == 0)
	        extra->locus = cloneString(findVal(lf, group, "Locus"));
	    }
	}
    }
lineFileClose(&lf);
}

struct sanger22extra *makeFixedGffAndReadExtra(char *txGff, char *cdsGff, 
	char *fixedGff, struct hash *extraHash)
/* Combine txGff and cdsGff into something our regular GFF to
 * genePred routine can handle. */
{
FILE *f = mustOpen(fixedGff, "w");
struct sanger22extra *extraList = NULL;

processOneGff(txGff, f, "exon", extraHash, &extraList, FALSE);
processOneGff(cdsGff, f, "CDS", extraHash, &extraList, TRUE);
carefulClose(&f);
slReverse(&extraList);
return extraList;
}

void saveExtras(char *fileName, struct sanger22extra *extraList)
/* Save out extras to tab-separated file. */
{
struct sanger22extra *extra;
FILE *f = mustOpen(fileName, "w");
for (extra = extraList; extra != NULL; extra = extra->next)
    sanger22extraTabOut(extra, f);
carefulClose(&f);
}

void genePredOffset(struct genePred *gp, int offset)
/* Add fixed offset to gene prediction. */
{
int i;
gp->txStart += offset;
gp->txEnd += offset;
gp->cdsStart += offset;
gp->cdsEnd += offset;
for (i=0; i<gp->exonCount; ++i)
    {
    gp->exonStarts[i] += offset;
    gp->exonEnds[i] += offset;
    }
}

void gffIntoDatabase(char *database, char *fileName, char *table, int offset)
/* Load a gff file into database. */
{
struct gffFile *gff = gffFileNew("");
struct gffGroup *group;
struct genePred *gpList = NULL, *gp;
FILE *f;
char *tabName = "genePred.tab";

/* Load fixed gff and convert it to genePred. */
gffFileAdd(gff, fileName, 0);
gffGroupLines(gff);
for (group = gff->groupList; group != NULL; group = group->next)
    {
    gp = genePredFromGroupedGff(gff, group, group->name, "exon", 
                                genePredCdsStatFld|genePredExonFramesFld,
                                genePredGxfDefaults);
    if (gp != NULL)
	{
	slAddHead(&gpList, gp);
	genePredOffset(gp, offset);
	}
    }
slSort(&gpList, genePredCmp);

/* Create tab-delimited file. */
f = mustOpen(tabName, "w");
for (gp = gpList; gp != NULL; gp = gp->next)
    genePredTabOut(gp, f);
carefulClose(&f);

/* Load into database. */
loadIntoDatabase(database, createGenePred, "sanger22", tabName);
}

void hgSanger22(char *database, char *txGff, char *cdsGff, char *mrnaFa, char *aaFa,
	int shortArmSize)
/* hgSanger22 - Load up database with Sanger 22 annotations. */
{
char *fixedGff = "fixed.gff";
char *extraBed = "extras.bed";
struct hash *extraHash = newHash(0);
struct sanger22extra *extraList;

extraList = makeFixedGffAndReadExtra(txGff, cdsGff, fixedGff, extraHash);
saveExtras(extraBed, extraList);
loadIntoDatabase(database, sanger22extraCreate, "sanger22extra", extraBed);
gffIntoDatabase(database, fixedGff, "sanger22", shortArmSize);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 7)
    usage();
hgSanger22(argv[1], argv[2], argv[3], argv[4], argv[5], atoi(argv[6]));
return 0;
}
