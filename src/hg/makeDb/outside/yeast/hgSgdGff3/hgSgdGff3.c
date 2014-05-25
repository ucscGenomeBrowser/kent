/* hgSgdGff3 - Parse out SGD gff3 file into components. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSgdGff3 - Parse out SGD gff3 file into components\n"
  "usage:\n"
  "   hgSgdGff3 s_cerevisiae.gff3 outDir\n"
  "outDir will be filled with:\n"
  "   codingGenes.gff - protein coding genes\n"
  "   descriptions.gff - Descriptions of coding genes\n"
  "   otherFeatures.bed - other features in bed format\n"
  "   notes.txt - descriptions of other features: ID, type, note\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

FILE *openInDir(char *dir, char *file)
/* Open file to write in directory. */
{
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", dir, file);
return mustOpen(path, "w");
}

struct tenFields 
/* Hold the ten fields of a GFF. */
    {
    struct tenFields *next;
    int lineIx;
    char *fields[10];
    };

struct tenFields *parseTenFields(char *fileName)
/* Return list of partially parsed lines from GFF. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct tenFields *tfList = NULL, *tf;
char *words[11];
int wordCount, i;

while ((wordCount = lineFileChopNextTab(lf, words, ArraySize(words))) != 0)
    {
    if (wordCount != 10)
        errAbort("Expecting 10 tab separated fields got %d line %d of %s",
		wordCount, lf->lineIx, lf->fileName);
    AllocVar(tf);
    for (i=0; i<10; ++i)
	{
	char *s = words[i];
	if (i == 0 && sameString(s, "17")) /* 17 is a synonym for mitochondria */
	    s = cloneString("M");
	if (i != 6)
	    cgiDecode(s, s, strlen(s));
	tf->fields[i] = cloneString(s);
	}
    tf->lineIx = lf->lineIx;
    slAddHead(&tfList, tf);
    }
lineFileClose(&lf);
slReverse(&tfList);
return tfList;
}

boolean isCds(char *type)
/* Return TRUE if type is CDS-related. */
{
return startsWith("CDS", type) && !sameString("CDS:Pseudogene", type);
}

void noteIds(struct tenFields *tfList, char *inGff, 
	FILE *cdsFile, FILE *otherFile)
/* Look for cases where tenth field is of form
 * ID=XXX;Note=YYY.  In these cases move XXX to
 * the ninth field, and store the ID, the 
 * third (type) field, and the YYY in f */
{
struct tenFields *tf;
struct hash *uniqHash = newHash(19);
for (tf = tfList; tf != NULL; tf = tf->next)
    {
    char *s = tf->fields[9];
    if (startsWith("ID=", s))
        {
	char *id = s+3, *note = "";
	char *e = strchr(s, ';');
	if (!hashLookup(uniqHash, id))
	    {
	    hashAdd(uniqHash, id, NULL);
	    if (e != NULL)
		{
		*e++ = 0;
		s = stringIn("Note=", e);
		if (s != NULL)
		    {
		    note = s+5;
		    cgiDecode(note, note, strlen(note));
		    }
		}
	    }
	tf->fields[8] = id;
	}
    }
hashFree(&uniqHash);
}

void saveFeatures(struct tenFields *tfList, char *inGff,
	FILE *codingFile, FILE *descriptionFile, FILE *otherFile)
/* Save coding genes to coding file as GFF and rest to
 * otherFile as bed. */
{
struct tenFields *tf, *nextTf;
int i;

for (tf = tfList; tf != NULL; tf = nextTf)
    {
    char *type = tf->fields[2];
    nextTf = tf->next;
    if (isCds(type))
        {
	/* Write exons to coding output. */
	if (nextTf == NULL || !sameString("exon", nextTf->fields[2]))
	    errAbort("CDS not followed by exon line %d of %s",
	    	tf->lineIx, inGff);
	do  {
	    nextTf->fields[2] = "CDS";
	    fprintf(codingFile, "chr%s\t", nextTf->fields[0]);
	    for (i=1; i<8; ++i)
	        fprintf(codingFile, "%s\t", nextTf->fields[i]);
	    fprintf(codingFile, "%s\n", nextTf->fields[8]);
	    nextTf = nextTf->next;
	    }
	while (nextTf != NULL && sameString("exon", nextTf->fields[2]));
	}
    else if (sameString("Component", type) || sameString("exon", type))
        {
	/* Do nothing. */
	}
    else
        {
	/* Write BED. */
	int start = atoi(tf->fields[3]) - 1;
	int end = atoi(tf->fields[4]);
	char strand = tf->fields[6][0];
	if (strand != '+' && strand != '-')
	    strand = '.';
	if (!stringIn(";Note=", tf->fields[8]))	/* Fix Mike's typo. */
	    fprintf(otherFile, "chr%s\t%d\t%d\t%s\t0\t%c\t%s\n",
		    tf->fields[0], start, end, tf->fields[8], strand, type);
	}
    }
}

void hgSgdGff3(char *inGff, char *outDir)
/* hgSgdGff3 - Parse out SGD gff3 file into components. */
{
FILE *codingFile = openInDir(outDir, "codingGenes.gff");
FILE *otherFile = openInDir(outDir, "otherFeatures.bed");
FILE *descriptionFile = openInDir(outDir, "descriptions.txt");
FILE *noteFile = openInDir(outDir, "notes.txt");
struct tenFields *tfList = parseTenFields(inGff);

noteIds(tfList, inGff, descriptionFile, noteFile);
saveFeatures(tfList, inGff, codingFile, descriptionFile, otherFile);
carefulClose(&codingFile);
carefulClose(&otherFile);
carefulClose(&noteFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hgSgdGff3(argv[1], argv[2]);
return 0;
}
