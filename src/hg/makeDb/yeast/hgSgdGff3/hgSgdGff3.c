/* hgSgdGff3 - Parse out SGD gff3 file into components. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"

static char const rcsid[] = "$Id: hgSgdGff3.c,v 1.1 2003/11/25 19:29:22 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSgdGff3 - Parse out SGD gff3 file into components\n"
  "usage:\n"
  "   hgSgdGff3 s_cerevisiae.gff3 outDir\n"
  "outDir will be filled with:\n"
  "   codingGenes.gff - protein coding genes\n"
  "   otherFeatures.bed - other features in bed format\n"
  "   notes.txt - three columns: ID, type, note\n"
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
         tf->fields[i] = cloneString(words[i]);
    tf->lineIx = lf->lineIx;
    slAddHead(&tfList, tf);
    }
lineFileClose(&lf);
slReverse(&tfList);
return tfList;
}


void noteIds(struct tenFields *tfList, char *inGff, FILE *f)
/* Look for cases where tenth field is of form
 * ID=XXX;Note=YYY.  In these cases move XXX to
 * the ninth field, and store the ID, the 
 * third (type) field, and the YYY in f */
{
struct tenFields *tf;
for (tf = tfList; tf != NULL; tf = tf->next)
    {
    char *s = tf->fields[9];
    if (startsWith("ID=", s))
        {
	char *id = s+3, *note = "";
	char *e = strchr(s, ';');
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
	fprintf(f, "%s\t%s\t%s\n", id, tf->fields[2], note);
	tf->fields[8] = id;
	}
    }
}

void hgSgdGff3(char *inGff, char *outDir)
/* hgSgdGff3 - Parse out SGD gff3 file into components. */
{
FILE *codingFile = openInDir(outDir, "codingGenes.gff");
FILE *otherFile = openInDir(outDir, "otherFeatures.bed");
FILE *noteFile = openInDir(outDir, "notes.txt");
struct tenFields *tf, *tfList = parseTenFields(inGff);

uglyf("Got %d lines\n", slCount(tfList));
noteIds(tfList, inGff, noteFile);
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
