/* hgSanger20 - Load extra info from Sanger Chromosome 20 annotations.. */

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
  "hgSanger20 - Load extra info from Sanger Chromosome 20 annotations.\n"
  "usage:\n"
  "   hgSanger20 database sanger20.gff sanger20.info\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void finishRecord(struct sanger22extra *sx, struct dyString *dy)
/* Finish off record. */
{
if (sx != NULL)
    {
    sx->description = cloneString(dy->string);
    dyStringClear(dy);
    }
}

struct hash *readInfoFile(char *fileName)
/* Read in 'info' file into hash keyed by gene field and
 * containing a partial sanger22extra. */
{
struct hash *infoHash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *type, *val;
struct dyString *dy = newDyString(512);
struct sanger22extra *sx = NULL;

while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    if (line[0] == 0)
        {
	finishRecord(sx, dy);
	sx = NULL;
	}
    else 
        {
	if (sx == NULL)
	    AllocVar(sx);
	type = nextWord(&line);
	val = strchr(line, '"');
	if (val == NULL)
	   errAbort("No quoted value line %d of %s\n", lf->lineIx, lf->fileName);
	parseQuotedString(val, val, &line);
	if (sameString("Gene", type))
	    {
	    sx->name = cloneString(val);
	    hashAdd(infoHash, val, sx);
	    }
	else if (sameString("Remark", type))
	    {
	    dyStringAppend(dy, val);
	    dyStringAppend(dy, ". ");
	    }
	}
    }
finishRecord(sx, dy);
lineFileClose(&lf);
return infoHash;
}

char *needNextWord(char **pLine, struct lineFile *lf)
/* Get next word or die. */
{
char *res = nextWord(pLine);
if (res == NULL)
    errAbort("Bad line %d in %s\n", lf->lineIx, lf->fileName);
return res;
}

void chopSemi(char *s)
/* Remove trailing semicolon. */
{
int len = strlen(s);
if (s[len-1] == ';')
   s[len-1] = 0;
}

struct sanger22extra *processGff(char *fileName, struct hash *infoHash)
/* Process gff file and return a list of sanger22 extras. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
struct sanger22extra *sxList = NULL, *sx, *info;
char *line, *source, *type, *word;

while (lineFileNext(lf, &line, NULL))
    {
    char *gene = NULL, *transcript = NULL;
    line = skipLeadingSpaces(line);
    if (line[0] == 0 || line[0] == '#')
        continue;
    word = needNextWord(&line, lf);	/* Skip chr20 */
    source = needNextWord(&line, lf);	
    type = needNextWord(&line, lf);
    if (sameString(type, "CDS") || sameString(type, "Exon"))
	{
	while (gene == NULL || transcript == NULL)
	    {
	    word = needNextWord(&line, lf);
	    if (sameString(word, "gene_id"))
		{
		gene = needNextWord(&line, lf);
		chopSemi(gene);
		}
	    else if (sameString(word, "transcript_id"))
		{
		transcript = needNextWord(&line, lf);
		chopSemi(transcript);
		}
	    }
	sx = hashFindVal(hash, transcript);
	if (sx == NULL)
	    {
	    AllocVar(sx);
	    sx->name = cloneString(transcript);
	    sx->locus = cloneString(gene);
	    sx->geneType = cloneString(source);
	    sx->cdsType = cloneString("");
	    if ((info = hashFindVal(infoHash, gene)) == NULL)
	       sx->description = "";
	    else
	       sx->description = cloneString(info->description);
	    hashAdd(hash, transcript, sx);
	    slAddHead(&sxList, sx);
	    }
	}
    }
hashFree(&hash);
lineFileClose(&lf);
slReverse(&sxList);
return sxList;
}

void loadIntoDatabase(char *database, char *createString, char *table, char *tabName)
/* Load tabbed file into database table. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *ds = newDyString(2048);
dyStringPrintf(ds, createString, table);
sqlRemakeTable(conn, table, ds->string);
dyStringClear(ds);
sqlDyStringPrintf(ds, 
   "LOAD data local infile '%s' into table %s", tabName, table);
sqlUpdate(conn, ds->string);
sqlDisconnect(&conn);
freeDyString(&ds);
}

void hgSanger20(char *database, char *gffFile, char *infoFile)
/* Process chromosome 20 gff file. */
{
struct hash *infoHash = readInfoFile(infoFile);
struct sanger22extra *sxList = processGff(gffFile, infoHash), *sx;
char *tabName = "extra.tab";
FILE *f = mustOpen(tabName, "w");
for (sx = sxList; sx != NULL; sx = sx->next)
    sanger22extraTabOut(sx, f);
carefulClose(&f);
loadIntoDatabase(database, sanger22extraCreate, "sanger20extra", tabName);
}


int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
hgSanger20(argv[1], argv[2], argv[3]);
return 0;
}
