/* git-reports.c for creating git equivalent of cvs-reports. */

#include "common.h"
#include "options.h"
#include "dystring.h"
#include "errabort.h"
#include "hash.h"
#include "linefile.h"

static char const rcsid[] = "$Id: git-reports.c,v 1.1 2010/03/02 08:43:07 galt Exp $";


struct hash *userHash = NULL;
struct slName *users = NULL;

char *startTag = NULL;
char *endTag = NULL;
char *startDate = NULL;
char *endDate = NULL;
char *title = NULL;
char *repoDir = NULL;
char *outDir = NULL;

char gitCmd[1024];


struct files
    {
    char type;
    char *path;
    };

struct commit 
    {
    struct commit *next;
    char *commitId;
    char *author;
    char *date;
    char *comment;
    struct files *files;
    };

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


struct commit* getCommits()
/* get all commits from startTag to endTag */
{
safef(gitCmd,sizeof(gitCmd), ""
"GIT_DIR=%s/.git "
"git log origin/%s..origin/%s --name-status > commits.tmp"
, repoDir, startTag, endTag);
system(gitCmd);
// TODO error handling
struct lineFile *lf = lineFileOpen("commits.tmp", TRUE);
int lineSize;
char *line;
struct commit *commits = NULL, *commit = NULL;
struct files *files = NULL, *f = NULL;
while (lineFileNext(lf, &line, &lineSize))
    {
    boolean isMerge = FALSE;
    char *w = nextWord(&line);
    AllocVar(commit);
    if (!sameString("commit", w))
	errAbort("expected keyword commit parsing commits.tmp\n");
    commit->commitId = cloneString(nextWord(&line));

    lineFileNext(lf, &line, &lineSize);
    w = nextWord(&line);
    if (sameString("Merge:", w))
	{
	isMerge = TRUE;
	lineFileNext(lf, &line, &lineSize);
	w = nextWord(&line);
	}
    if (!sameString("Author:", w))
	errAbort("expected keyword Author: parsing commits.tmp\n");
    commit->author = cloneString(nextWord(&line));

    lineFileNext(lf, &line, &lineSize);
    w = nextWord(&line);
    if (!sameString("Date:", w))
	errAbort("expected keyword Date: parsing commits.tmp\n");
    commit->date = cloneString(nextWord(&line));

    lineFileNext(lf, &line, &lineSize);
    if (!sameString("", line))
	errAbort("expected blank line parsing commits.tmp\n");

    /* collect the comment-lines */
    struct dyString *dy = NULL;
    dy = dyStringNew(0);
    while (lineFileNext(lf, &line, &lineSize))
	{
	if (sameString("", line))
	    break;
	w = skipLeadingSpaces(line);
	dyStringPrintf(dy, "%s\n", w);
	}
    commit->comment = cloneString(dy->string);
    freeDyString(&dy);

    if (!isMerge)
	{
	/* collect the files-list */
	while (lineFileNext(lf, &line, &lineSize))
	    {
	    if (sameString("", line))
		break;
	    AllocVar(f);
	    w = nextWord(&line);
	    f->type = w[0];
	    f->path = cloneString(line);
	    slAddHead(&files, f);
	    }
	slReverse(&files);
	}

    commit->files = files;

    slAddHead(&commits, commit);
    slReverse(&commits);
    }
lineFileClose(&lf);


unlink("commits.tmp");
return commits;
}


void gitReports()
/* generate code-review reports from git repo */
{
/* read the commits */
struct commit *commits = getCommits(), *c = NULL;
/* make the user list */
for(c = commits; c; c = c->next)
    {
    
    if (!hashLookup(userHash, c->author))
	{
	hashStore(userHash, c->author);
	struct slName *name = newSlName(c->author);
	slAddHead(&users, name);
	}
    }
slReverse(&users);

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

userHash = hashNew(5);

gitReports();

hashFree(&userHash);
printf("Done.\n");
return 0;
}

