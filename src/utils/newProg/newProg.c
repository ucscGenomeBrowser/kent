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

void makeC(char *name, char *description, char *progPath)
/* makeC - make a new C source skeleton. */
{
FILE *f = mustOpen(progPath, "w");

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
fclose(f);
}

void makeMakefile(char *progName, char *makeName)
/* Make makefile. */
{
FILE *f = mustOpen(makeName, "w");

fprintf(f, 

".c.o:\n"
"\tgcc -ggdb -O -Wimplicit -I../inc -I../../inc -I../../../inc -c $*.c\n"
"\n"
"L = -lm\n"
"MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)\n"
"MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a\n"
"\n"
"O = %s.o\n"
"\n"
"%s: $O $(MYLIBS)\n"
"\tgcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/%s $O $(MYLIBS) $L\n",
	progName, progName, progName);


fclose(f);
}

void newProg(char *name, char *description)
/* newProg - make a new C source skeleton. */
{
char fileName[512];

makeDir(name);
sprintf(fileName, "%s/%s.c", name, name);
makeC(name, description, fileName);

sprintf(fileName, "%s/makefile", name);
makeMakefile(name, fileName);
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


