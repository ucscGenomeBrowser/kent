/* stringify - Convert file to C strings. */
#include "common.h"
#include "linefile.h"
#include "options.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"var", OPTION_STRING},
    {"static", OPTION_BOOLEAN},
    {NULL, 0}
};
/* command line options */
static char *varName = NULL;
static boolean staticVar = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "stringify - Convert file to C strings\n"
  "usage:\n"
  "   stringify [options] in.txt\n"
  "A stringified version of in.txt  will be printed to standard output.\n"
  "\n"
  "Options:\n"
  "  -var=varname - create a variable with the specified name containing\n"
  "                 the string.\n"
  "  -static - create the variable as a string array.\n"
  "\n"
  );
}

void stringify(char *fileName, FILE *f)
/* stringify - Convert file to C strings. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, c;

if (varName != NULL)
    fprintf(f, "%schar *%s =\n", (staticVar ? "static " : ""),
            varName);

while (lineFileNext(lf, &line, NULL))
    {
    fputc('"', f);
    while ((c = *line++) != 0)
        {
        if (c == '"' || c == '\\')
            fputc('\\', f);
        fputc(c, f);
        }
    fputc('\\', f);
    fputc('n', f);
    fputc('"', f);
    fputc('\n', f);
    }
if (varName != NULL)
    fputs(";\n", f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 2)
    usage();
varName = optionVal("var", NULL); 
staticVar = optionExists("static");
stringify(argv[1], stdout);
return 0;
}

