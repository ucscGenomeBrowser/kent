/* newProg - make a new C source skeleton. */
#include "common.h"
#include "portable.h"
#include "dystring.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newProg - make a new C source skeleton.\n"
  "usage:\n"
  "   newProg progName description words\n"
  "This will make a directory 'progName' and a file in it 'progName.c'\n"
  "with a standard skeleton\n"
  "\n"
  "Options:\n"
  "   -cvs\n"
  "This will also check it into CVS.  'progName' should include full path\n"
  "in source repository\n");
}

void makeC(char *name, char *description, char *progPath)
/* makeC - make a new C source skeleton. */
{
FILE *f = mustOpen(progPath, "w");

/* Make the usage routine. */
fprintf(f, "/* %s - %s. */\n", name, description);
fprintf(f, "#include \"common.h\"\n");
fprintf(f, "#include \"linefile.h\"\n");
fprintf(f, "#include \"hash.h\"\n");
fprintf(f, "#include \"cheapcgi.h\"\n");
fprintf(f, "\n");
fprintf(f, "void usage()\n");
fprintf(f, "/* Explain usage and exit. */\n");
fprintf(f, "{\n");
fprintf(f, "errAbort(\n");
fprintf(f, "  \"%s - %s\\n\"\n", name, description);
fprintf(f, "  \"usage:\\n\"\n");
fprintf(f, "  \"   %s XXX\\n\"\n", name);
fprintf(f, "  \"options:\\n\"\n");
fprintf(f, "  \"   -xxx=XXX\\n\"\n");
fprintf(f, "  );\n");
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
fprintf(f, "cgiSpoof(&argc, argv);\n");
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
"\tgcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c\n"
"\n"
"L = -lm\n"
"MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)\n"
"MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a\n"
"\n"
"O = %s.o\n"
"\n"
"%s: $O $(MYLIBS)\n"
"\tgcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/%s $O $(MYLIBS) $L\n"
"\tstrip $(HOME)/bin/$(MACHTYPE)/%s\n",
	progName, progName, progName, progName);



fclose(f);
}

void newProg(char *module, char *description)
/* newProg - make a new C source skeleton. */
{
char fileName[512];
char dirName[512];
char fileOnly[128];
char command[512];
boolean doCvs = cgiBoolean("cvs");

if (doCvs)
    {
    char *homeDir = getenv("HOME");
    if (homeDir == NULL)
        errAbort("Can't find environment variable 'HOME'");
    if (!startsWith("kent", module))
        errAbort("Need to include full module name with cvs option, not just relative path");
    sprintf(dirName, "%s%s", homeDir, module+strlen("kent"));
    }
else
    sprintf(dirName, "%s", module);
makeDir(dirName);
splitPath(dirName, NULL, fileOnly, NULL);
sprintf(fileName, "%s/%s.c", dirName, fileOnly);
makeC(fileOnly, description, fileName);

sprintf(fileName, "%s/makefile", dirName);
makeMakefile(fileOnly, fileName);

if (doCvs)
    {

    /* Set current directory.  Return FALSE if it fails. */
    printf("Adding %s to CVS\n", module);
    if (!setCurrentDir(dirName))
        errAbort("Couldn't change dir to %s", dirName);
    sprintf(command, "cvs import -m \"%s\" %s kent start", description, module);
    if (system(command) != 0)
        errAbort("system call '%s' returned non-zero", command);
    if (!setCurrentDir(".."))
        errAbort("Couldn't change dir to ..");
    sprintf(command, "cvs checkout -d %s %s", fileOnly, module);
    if (system(command) != 0)
        errAbort("system call '%s' returned non-zero", command);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct dyString *ds = newDyString(1024);
int i;
boolean doCvs = FALSE;

cgiSpoof(&argc, argv);
if (argc < 3)
     usage();
for (i=2; i<argc; ++i)
    {
    dyStringAppend(ds, argv[i]);
    if (i != argc-1)
       dyStringAppend(ds, " ");
    }
newProg(argv[1], ds->string);
return 0;
}


