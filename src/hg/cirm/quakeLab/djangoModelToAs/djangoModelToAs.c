/* djangoModelToAs - Convert django model to as file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "asParse.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "djangoModelToAs - Convert django model to as file\n"
  "usage:\n"
  "   djangoModelToAs input.py output.as\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *tripleQuote = "\"\"\"";

boolean isModelLine(char *line)
/* Return TRUE if line seems to be defining a model */
{
return (startsWith("class", line) && stringIn("models.Model", line) != NULL);
}

void doOneClass(char *line, struct lineFile *lf, FILE *f)
/* Parse until end of class and writ out result */
{
char *modelName = stringBetween("class ", "(", line);
if (modelName == NULL)
    errAbort("Couldn't parse model name line %d of %s\n", lf->lineIx, lf->fileName);
fprintf(f, "table %s\n", modelName);
lineFileNeedNext(lf, &line, NULL);
char *openComment = stringIn(tripleQuote, line);
if (openComment == NULL)
     {
     fprintf(f, "\"%s needs documentation\"\n", modelName);
     lineFileReuse(lf);
     }
else
    {
    fprintf(f, "\"%s\"\n", skipLeadingSpaces(openComment + strlen(tripleQuote)));
    lineFileNeedNext(lf, &line, NULL);
    if (!stringIn(tripleQuote, line))
	errAbort("Expecting single line triple quoted comment line %d of %s", 
		lf->lineIx, lf->fileName);
    }
fprintf(f, "    (\n");
for (;;)
    {
    if (!lineFileNextReal(lf, &line))
         break;
    char *equals = strchr(line, '=');
    char *models = stringIn("models.", line);
    if (equals != NULL && models != NULL)
         {
	 boolean isFk = FALSE;
	 char *label = cloneStringZ(line, equals - line);
	 label = trimSpaces(label);
	 models += strlen("models.");
	 char *type = "string";
	 if (startsWith("FloatField(", models))
	     type = "double";
	 else if (startsWith("IntegerField(", models))
	     type = "int";
	 else if (startsWith("ForeignKey(", models))
	     {
	     type = "uint";
	     isFk = TRUE;
	     }
	 char *commentString = stringBetween("(", ")", models);
	 if (commentString != NULL)
	     {
	     if (isFk)
		 {
		 char *comma = strchr(commentString, ',');
		 if (comma != NULL)
		     *comma = 0;
		 commentString = trimSpaces(commentString);
		 }
	     else
		 commentString = stringBetween("'", "'", commentString);
	     }
	 if (commentString == NULL)
	     commentString = "needs comment";
	 if (isFk)
	     fprintf(f, "    %s %s; \"Foreign key in %s table\"\n", type, label, commentString);
	 else
	     fprintf(f, "    %s %s; \"%s\"\n", type, label, commentString);
	 }
    else if (isModelLine(line))
         {
	 lineFileReuse(lf);
	 break;
	 }
    }
fprintf(f, "    )\n\n");
}

void djangoModelToAs(char *inFile, char *outFile)
/* djangoModelToAs - Convert django model to as file. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");

for (;;)
    {
    char *line;
    if (!lineFileNextReal(lf, &line))
        break;
    line = trimSpaces(line);
    if (isModelLine(line))
         doOneClass(line, lf, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
djangoModelToAs(argv[1], argv[2]);
return 0;
}
