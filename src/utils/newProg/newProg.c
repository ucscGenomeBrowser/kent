/* newProg - make a new C source skeleton. */
#include "common.h"
#include "portable.h"
#include "dystring.h"
#include "options.h"

static char const rcsid[] = "$Id: newProg.c,v 1.26 2008/06/27 18:46:54 markd Exp $";

boolean jkhgap = FALSE;
boolean cgi = FALSE;
boolean cvs = FALSE;

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
  "   -jkhgap - include jkhgap.a and mysql libraries as well as jkweb.a archives \n"
  "   -cgi    - create shell of a CGI script for web\n"
  "   -cvs    - also check source into CVS, 'progName' must include full\n"
  "           - path in source repository starting with 'kent/'");
}

static struct optionSpec options[] = {
   {"jkhgap", OPTION_BOOLEAN},
   {"cgi", OPTION_BOOLEAN},
   {"cvs", OPTION_BOOLEAN},
   {NULL, 0},
};

void makeCgiBody(char *name, char *description, FILE *f)
/* Create most of the C file for a CGI. */
{
fprintf(f,
"/* Global Variables */\n"
"struct cart *cart;             /* CGI and other variables */\n"
"struct hash *oldVars = NULL;\n"
"\n"
);

fprintf(f,
"void doMiddle(struct cart *theCart)\n"
"/* Set up globals and make web page */\n"
"{\n"
"cart = theCart;\n"
"cartWebStart(cart, \"%s\");\n"
"printf(\"Your code goes here....\");\n"
"cartWebEnd();\n"
"}\n"
"\n"
, description
);

fprintf(f, 
"/* Null terminated list of CGI Variables we don't want to save\n"
" * permanently. */\n"  
"char *excludeVars[] = {\"Submit\", \"submit\", NULL,};\n"
"\n"
);

fprintf(f, 
"int main(int argc, char *argv[])\n"
"/* Process command line. */\n"
"{\n"
"cgiSpoof(&argc, argv);\n"
"cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);\n"
"return 0;\n"
"}\n"
);
}

void makeCommandLineBody(char *name, char *description, FILE *f)
/* Create most of the C file for a command line. */
{
/* Make the usage routine. */
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

/* Initialize options array to empty */
fprintf(f, "static struct optionSpec options[] = {\n");
fprintf(f, "   {NULL, 0},\n");
fprintf(f, "};\n");
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
fprintf(f, "optionInit(&argc, argv, options);\n");
fprintf(f, "if (argc != 2)\n");
fprintf(f, "    usage();\n");
fprintf(f, "%s(argv[1]);\n", name);
fprintf(f, "return 0;\n");
fprintf(f, "}\n");
fclose(f);
}

void makeC(char *name, char *description, char *progPath)
/* makeC - make a new C source skeleton. */
{
FILE *f = mustOpen(progPath, "w");

fprintf(f, "/* %s - %s. */\n", name, description);
fprintf(f, "#include \"common.h\"\n");
fprintf(f, "#include \"linefile.h\"\n");
fprintf(f, "#include \"hash.h\"\n");
fprintf(f, "#include \"options.h\"\n");
if (jkhgap || cgi)
    fprintf(f, "#include \"jksql.h\"\n");
if (cgi)
    {
    fprintf(f, "#include \"htmshell.h\"\n");
    fprintf(f, "#include \"web.h\"\n");
    fprintf(f, "#include \"cheapcgi.h\"\n");
    fprintf(f, "#include \"cart.h\"\n");
    fprintf(f, "#include \"hui.h\"\n");
    }
fprintf(f, "\n");
fprintf(f, "static char const rcsid[] = \"$Id: newProg.c,v 1.26 2008/06/27 18:46:54 markd Exp $\";\n");
fprintf(f, "\n");

if (cgi)
    makeCgiBody(name, description, f);
else
    makeCommandLineBody(name, description, f);

}

void makeMakefile(char *progName, char *makeName)
/* Make makefile. */
{
char *upLevel;
char *L;
char *myLibs;
FILE *f = mustOpen(makeName, "w");

if (fileExists("../inc/common.mk"))
	upLevel = cloneString("..");
else if (fileExists("../../inc/common.mk"))
	upLevel = cloneString("../..");
else if (fileExists("../../../inc/common.mk"))
	upLevel = cloneString("../../..");
else if (fileExists("../../../../inc/common.mk"))
	upLevel = cloneString("../../../..");
else if (fileExists("../../../../../inc/common.mk"))
	upLevel = cloneString("../../../../..");
else
    {
    warn("WARNING: can not find inc/common.mk 1 to 4 directories up, fix the makefile");
    upLevel = cloneString("../../../../..");
    }

if (jkhgap || cgi)
    {
    L = cloneString("L = $(MYSQLLIBS) -lm");
    myLibs = cloneString("MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a");
    }
else
    {
    L = cloneString("L = -lm");
    myLibs = cloneString("MYLIBS =  ${MYLIBDIR}/jkweb.a");
    }

fprintf(f, 
"include %s/inc/common.mk\n"
"\n"
"%s\n"
"MYLIBDIR = %s/lib/${MACHTYPE}\n"
"%s\n"
"\n"
"O = %s.o\n"
"\n"
, upLevel, L, upLevel, myLibs, progName);

if (cgi)
    {
    fprintf(f, 
    "A = %s\n"
    "\n"
    "include %s/inc/cgi_build_rules.mk\n"
    "\n"
    , progName, upLevel);
    fprintf(f,
    "compile: $O \n"
    "\t${CC} $O ${MYLIBS} ${L}\n"
    "\tmv ${AOUT} $A${EXE}\n"
    "\n");
    }
else
    {
    fprintf(f, 
    "%s: $O ${MYLIBS}\n"
    "\t${CC} ${COPT} -o ${BINDIR}/%s $O ${MYLIBS} $L\n"
    "\t${STRIP} ${BINDIR}/%s${EXE}\n"
    "\n"
    "clean:\n"
    "\trm -f $O\n"
    , progName, progName, progName);
    }


fclose(f);
}

void newProg(char *module, char *description)
/* newProg - make a new C source skeleton. */
{
char fileName[512];
char dirName[512];
char fileOnly[128];
char command[512];

if (cvs)
    {
    char *homeDir = getenv("HOME");
    if (homeDir == NULL)
        errAbort("Can't find environment variable 'HOME'");
    if (!startsWith("kent", module))
        errAbort("Need to include full module name with cvs option, not just relative path");
    safef(dirName, sizeof(dirName), "%s/kent%s", homeDir, module+strlen("kent"));
    }
else
    safef(dirName, sizeof(dirName), "%s", module);
makeDir(dirName);
splitPath(dirName, NULL, fileOnly, NULL);
safef(fileName, sizeof(fileName), "%s/%s.c", dirName, fileOnly);
makeC(fileOnly, description, fileName);

/* makefile is now constructed properly with ../.. paths */
setCurrentDir(dirName);
makeMakefile(fileOnly, "makefile");

if (cvs)
    {
    /* Set current directory.  Return FALSE if it fails. */
    printf("Adding %s to CVS\n", module);
    setCurrentDir("..");
    safef(command, sizeof(command), "cvs add %s", fileOnly);
    if (system(command) != 0)
        errAbort("system call '%s' returned non-zero", command);
    safef(command, sizeof(command), "cvs commit -m \"%s\" %s", description, fileOnly);
    if (system(command) != 0)
        errAbort("system call '%s' returned non-zero", command);
    setCurrentDir(dirName);
    safef(command, sizeof(command), "cvs add %s.c makefile", fileOnly);
    if (system(command) != 0)
        errAbort("system call '%s' returned non-zero", command);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct dyString *ds = newDyString(1024);
int i;

optionInit(&argc, argv, options);
cgi = optionExists("cgi");
cvs = optionExists("cvs");
jkhgap = optionExists("jkhgap");
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


