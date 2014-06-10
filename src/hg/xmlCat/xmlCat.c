/* xmlCat - Concatenate xml files together, stuffing all records inside a single outer tag. . */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "xmlEscape.h"
#include "xp.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "xmlCat - Concatenate xml files together, stuffing all records inside a single outer tag. \n"
  "usage:\n"
  "   xmlCat XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

boolean showFirst, showLast;
int level = 0;

void atStart(void *userData, char *name, char **atts)
/* Handle start tag. */
{
if (level != 0 || showFirst)
    {
    char *att;
    spaceOut(stdout, level*2);
    printf("<%s", name);
    while ((att = *atts++) != NULL)
	{
	printf(" %s=\"", att);
	xmlEscapeStringToFile(*atts++, stdout);
	fputc('"', stdout);
	}
    printf(">\n");
    }
level += 1;
}

void atEnd(void *userData, char *name, char *text)
/* Handle end tag. */
{
level -= 1;
if (level != 0 || showLast)
    {
    text = trimSpaces(text);
    if (text[0] != 0)
	{
	xmlEscapeStringToFile(text, stdout);
	fputc('\n', stdout);
	}
    spaceOut(stdout, level*2);
    printf("</%s>\n", name);
    }
}

struct xp *xpNew(void *userData, 
   void (*atStartTag)(void *userData, char *name, char **atts),
   void (*atEndTag)(void *userData, char *name, char *text),
   int (*read)(void *userData, char *buf, int bufSize),
   char *fileName);

void xmlCat(int fileCount, char *fileNames[])
/* xmlCat - Concatenate xml files together, stuffing all records inside 
 * a single outer tag. . */
{
int i;
for (i=0; i<fileCount; ++i)
    {
    char *fileName = fileNames[i];
    FILE *f = mustOpen(fileName, "r");
    struct xp *xp = xpNew(f, atStart, atEnd, xpReadFromFile, fileName);
    showFirst = (i == 0);
    showLast = (i == fileCount-1);
    xpParse(xp);
    carefulClose(&f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
xmlCat(argc-1, argv+1);
return 0;
}
