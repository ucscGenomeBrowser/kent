/* git-reports.c for creating git equivalent of cvs-reports. */

#include "common.h"
#include "options.h"
#include "dystring.h"
#include "errabort.h"
#include "hash.h"

static char const rcsid[] = "$Id: git-reports.c,v 1.1 2010/03/02 08:43:07 galt Exp $";

//struct hash *cidHash = NULL;
//struct dyString *dy = NULL;

char *startTag = NULL;
char *endTag = NULL;
char *startDate = NULL;
char *endDate = NULL;
char *title = NULL;
char *repoDir = NULL;
char *outDir = NULL;

char gitCmd[1024];

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "%s\n\n"
    "git-reports - produce source code reports useful for code-review on git repository \n"
    "\n"
    "Usage:\n"
    "    git-reports startTag endTag startDate endDate title repoDir outDir\n"
    "where "
    " startTag and endTag are repository tags marking the beginning and end of the git range\n"
    " startDate and endDate and title are just strings that get printed on the report\n"
    " repoDir is where the git repository\n"
    " outDir is the output directory.\n"
    "  --help - this help screen\n",
    msg);
}

static struct optionSpec options[] =
{
    {"-help", OPTION_BOOLEAN},
    {NULL, 0},
};


void getCommits()
/* get all commits from startTag to endTag */
{
safef(gitCmd,sizeof(gitCmd), ""
"GIT_DIR=%s/.git "
"git log origin/%s..origin/%s --name-status > commits.tmp"
, repoDir, startTag, endTag);
system(gitCmd);

}


void gitReports()
/* generate code-review reports from git repo */
{
getCommits();
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, options);
if (argc != 8)
    usage("wrong number of args");
if (optionExists("-help"))
    usage("help");
//if (optionExists("altHeader") && optionExists("autoBoundary"))
// altHeader  = optionVal("altHeader",altHeader);

startTag = argv[1];
endTag = argv[2];
startDate = argv[3];
endDate = argv[4];
title = argv[5];
repoDir = argv[6];
outDir = argv[7];

gitReports();

//cidHash = hashNew(5);
//dy = dyStringNew(0);


//hashFree(&cidHash);
//freeDyString(&dy);
printf("Done.\n");
return 0;
}

