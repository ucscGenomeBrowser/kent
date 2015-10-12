/* newPythonProg - Make a skeleton for a new python program. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
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

void newPythonProg(char *programName, char *usage)
/* newPythonProg - Make a skeleton for a new python program. */
{
FILE *programFile = mustOpen(programName, "w"); 
// Write the python skeleton
fprintf(programFile, "#!/usr/bin/env python2.7\n# %s\n\"\"\"%s\"\"\"\n", programName,  usage);
fprintf(programFile, "from __future__ import print_function\nimport sys, operator, fileinput, collections, string, os.path"
		   "\nimport re, argparse, subprocess, math, time, common\n\n");
fprintf(programFile, "def parseArgs(args):\n    \"\"\"\n    Parse the command line arguments.\n    \"\"\"\n    parser" 
		    "= argparse.ArgumentParser(description = __doc__)\n    parser.add_argument (\"inpu"
		    "tFile\",\n    help = \" The input file. \",\n    type = argparse.FileType(\"r\"))\n    ");
fprintf(programFile, "parser.add_argument (\"outputFile\",\n    help = \" The output file. \",\n    type =" 
		    "argparse.FileType(\"w\"))\n    options = parser.parse_args()\n    return options\n\n"); 
fprintf(programFile, "def main(args):\n    \"\"\"\n    Initialized options and calls other functions.\n    \"\"\"\n    "
		    "options = parseArgs(args)\n    ommon.cloneProgToPath(__file__)\n\nif __name__ == \""
		    "__main__\" : \n    sys.exit(main(sys.argv))"); 
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
