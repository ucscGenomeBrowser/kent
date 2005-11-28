/* xmlToSql - Convert XML dump into a fairly normalized relational database.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dtdParse.h"
#include "portable.h"

static char const rcsid[] = "$Id: xmlToSql.c,v 1.1 2005/11/28 20:34:06 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "xmlToSql - Convert XML dump into a fairly normalized relational database.\n"
  "usage:\n"
  "   xmlToSql in.xml in.dtd in.stats outDir\n"
  "options:\n"
  "   -prefix=name - A name to prefix all tables with\n"
  "   -textField=name - Name to use for text field (default 'text')\n"
  );
}

char *globalPrefix = "";
char *textField = "text";

static struct optionSpec options[] = {
   {"prefix", OPTION_STRING},
   {"textField", OPTION_STRING},
   {NULL, 0},
};

struct attStat
/* Statistics on an xml attribute. */
   {
   struct attStat *next;
   char *name;		/* Attribute name. */
   int maxLen;		/* Maximum length. */
   char *type;		/* int/float/string/none */
   int count;		/* Number of times attribute seen. */
   int uniqCount;	/* Number of unique values of attribute. */
   };

struct elStat
/* Statistics on an xml element. */
   {
   struct elStat *next;	/* Next in list. */
   char *name;		/* Element name. */
   int count;		/* Number of times element seen. */
   struct attStat *attList;	/* List of attributes. */
   };

struct elStat *elStatLoadAll(char *fileName)
/* Read all elStats from file. */
{
struct elStat *list = NULL, *el = NULL;
struct attStat *att;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *row[6];
int wordCount;

for (;;)
   {
   wordCount = lineFileChop(lf, row);
   if (wordCount == 2)
       {
       AllocVar(el);
       el->name = cloneString(row[0]);
       el->count = lineFileNeedNum(lf, row, 1);
       slAddHead(&list, el);
       }
    else if (wordCount == 5)
       {
       if (el == NULL)
           errAbort("%s doesn't start with an element", fileName);
       AllocVar(att);
       att->name = cloneString(row[0]);
       att->maxLen = lineFileNeedNum(lf, row, 1);
       att->type = cloneString(row[2]);
       att->count = lineFileNeedNum(lf, row, 3);
       att->uniqCount = lineFileNeedNum(lf, row, 4);
       slAddTail(&el->attList, att);
       }
    else if (wordCount == 0)
       break;
    else
       errAbort("Don't understand line %d of %s", lf->lineIx, lf->fileName);
    }
slReverse(&list);
return list;
}

void xmlToSql(char *xmlFileName, char *dtdFileName, char *statsFileName,
	char *outDir)
/* xmlToSql - Convert XML dump into a fairly normalized relational database. */
{
struct elStat *elStatList = elStatLoadAll(statsFileName);
struct dtdElement *dtdList;
struct hash *dtdHash;

verbose(1, "%d elements in %s\n", slCount(elStatList), statsFileName);
dtdParse(dtdFileName, globalPrefix, textField,
	&dtdList, &dtdHash);
verbose(1, "%d elements in %s\n", dtdHash->elCount, dtdFileName);
makeDir(outDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
globalPrefix = optionVal("prefix", globalPrefix);
textField = optionVal("textField", textField);
if (argc != 5)
    usage();
xmlToSql(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
