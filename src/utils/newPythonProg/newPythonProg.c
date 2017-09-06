/* newPythonProg - Make a skeleton for a new python program. */
#include "common.h"
#include "portable.h"
#include "dystring.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newPythonProg - Make a skeleton for a new python program\n"
  "usage:\n"
  "   newPythonProg programName \"The usage statement\"\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}


/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void makeMakefile(char *progName, char *makeName)
/* Make makefile. */
{
FILE *f = mustOpen(makeName, "w");

fprintf(f, 
"all::\n\t cp -p %s ${HOME}/bin/scripts\n"
"test::\n\t@if test -d tests -a -s tests/makefile; then (cd tests && ${MAKE} test); \\ " 
"\n\telse echo \"# no tests directory (or perhaps no tests/makefile) in $(CURDIR)\"; fi"
, progName);

fclose(f);
}

void writeProgram(char *fileName, char *programName, char *usage)
{
FILE *programFile = mustOpen(fileName, "w"); 
// Write the python skeleton
fprintf(programFile, "#!/usr/bin/env python2.7\n# %s\n\"\"\"%s\"\"\"\n", programName,  usage);
fprintf(programFile, "import os\nimport sys\nimport argparse\n\n");
fprintf(programFile, "def parseArgs(args):\n\t\"\"\"\n\tParse the command line arguments.\n\t\"\"\"\n\tparser" 
		    "= argparse.ArgumentParser(description = __doc__)\n\tparser.add_argument (\"inpu"
		    "tFile\",\n\t\thelp = \" The input file. \",\n\t\ttype = argparse.FileType(\"r\"))\n\t");
fprintf(programFile, "parser.add_argument (\"outputFile\",\n\t\thelp = \" The output file. \",\n\t\ttype =" 
		    "argparse.FileType(\"w\"))\n\n\tif (len(sys.argv) == 1):\n\t\tparser.print_help()\n\t"
		    "\texit(1)\n\toptions = parser.parse_args()\n\treturn options\n\n"); 
fprintf(programFile, "def main(args):\n    \"\"\"\n    Initialized options and calls other functions.\n    \"\"\"\n    "
		    "options = parseArgs(args)\n\nif __name__ == \""
		    "__main__\" : \n    sys.exit(main(sys.argv))"); 
}



void newPythonProg(char *programName, char *usage)
/* newPythonProg - Make a skeleton for a new python program. */
{
char fileName[512];
char dirName[512];
//char fileOnly[128];
safef(dirName, sizeof(dirName), "%s", programName);
makeDir(dirName);
// Stolen code from newProg
safef(fileName, sizeof(fileName), "%s/%s", programName, programName);
writeProgram(fileName, programName, usage);
setCurrentDir(dirName);
makeMakefile(programName, "makefile");
// Change file permissions    
char cmd[1024]; 
safef(cmd, 1024, "chmod 755 %s",programName);  
mustSystem(cmd); 
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
newPythonProg(argv[1], argv[2]);
return 0;
}
