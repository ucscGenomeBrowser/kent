/* newProg - make a new C source skeleton. */
#include "common.h"
#include "portable.h"
#include "dystring.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newProg - make a new C source skeleton.\n"
  "usage:\n"
  "   newProg progName description words\n"
  "This will make a directory 'progName' and a file in it 'progName.c'\n"
  "with a standard skeleton\n");
}

void newProg(char *name, char *description)
/* newProg - make a new C source skeleton. */
{
char progPath[512];
FILE *f;

makeDir(name);
sprintf(progPath, "%s/%s.c", name, name);
f = mustOpen(progPath, "w");

/* Make the usage routine. */
fprintf(f, "/* %s - %s. */\n", name, description);
fprintf(f, "#include \"common.h\"\n");
fprintf(f, "\n");
fprintf(f, "void usage()\n");
fprintf(f, "/* Explain usage and exit. */\n");
fprintf(f, "{\n");
fprintf(f, "errAbort(\n");
fprintf(f, "  \"%s - %s\\n\"\n", name, description);
fprintf(f, "  \"usage:\\n\"\n");
fprintf(f, "  \"   %s XXX\\n\");\n", name);
fprintf(f, "}\n");
fprintf(f, "\n");

/* Make the processing routine. */
fprintf(f, "void %s(char *XXX)\n", name);
fprintf(f, "/* %s - %s. */\n", name, description);
fprintf(f, "{\n");
fprintf(f, "}\n");
fprintf(f, "\n");

/* Make the main routine. */
fprintf(f, "int main(int argc, char *argv[])\n");
fprintf(f, "/* Process command line. */\n");
fprintf(f, "{\n");
fprintf(f, "if (argc != 2)\n");
fprintf(f, "    usage();\n");
fprintf(f, "%s(argv[1]);\n", name);
fprintf(f, "return 0;\n");
fprintf(f, "}\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct dyString *ds = newDyString(1024);
int i;

if (argc < 3)
     usage();
for (i=2; i<argc; ++i)
    {
    dyStringAppend(ds, argv[i]);
    if (i != argc-1)
       dyStringAppend(ds, " ");
    }
newProg(argv[1], ds->string);
}


