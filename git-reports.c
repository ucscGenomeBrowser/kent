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
char *outPrefix = NULL;

char gitCmd[1024];


struct files
    {
    struct files *next;
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
    "    git-reports startTag endTag startDate endDate title repoDir outDir outPrefix\n"
    "where "
    " startTag and endTag are repository tags marking the beginning and end of the git range\n"
    " startDate and endDate and title are just strings that get printed on the report\n"
    " title is usually the branch number, e.g. v225\n"
    " repoDir is where the git repository (use absolute path)\n"
    " outDir is the output directory (use absolute path).\n"
    " outPrefix is typically \"branch\" or \"review\" directory.\n"
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
"git log origin/%s..origin/%s --name-status > commits.tmp"
, startTag, endTag);
system(gitCmd);
// TODO error handling
struct lineFile *lf = lineFileOpen("commits.tmp", TRUE);
int lineSize;
char *line;
struct commit *commits = NULL, *commit = NULL;
struct files *files = NULL, *f = NULL;
char *sep = "";
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
    sep = "";
    files = NULL;
    while (lineFileNext(lf, &line, &lineSize))
	{
	if (sameString("", line))
	    break;
	w = skipLeadingSpaces(line);
	dyStringPrintf(dy, "%s%s", w, sep);
	sep = "\n";
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

    verbose(2, 
 "commitId: %s\n"
 "author: %s\n"
 "date: %s\n"
 "comment: [%s]\n"
 "file(s): \n"
, commit->commitId
, commit->author
, commit->date
, commit->comment);

    for (f=commit->files; f; f = f->next)
    	verbose(2, "%s\n", f->path);
    verbose(2, "------------\n");

    }
lineFileClose(&lf);
slReverse(&commits);


unlink("commits.tmp");
return commits;
}

void doUser(char *u, struct commit *commits)
/* process one user */
{
struct commit *c = NULL;
struct files *f = NULL;
for(c = commits; c; c = c->next)
    {
    if (sameString(c->author, u))
	{
	for(f = c->files; f; f = f->next)
	    {
	    char path[1024];
	    char *r = strrchr(f->path, '/');
	    if (r)
		{
		*r = 0;
		/* make internal levels of subdirs */
		safef(path, sizeof(path), "mkdir -p %s/%s/%s/%s/%s/%s", outDir, outPrefix, "user", u, "context", f->path);
		uglyf("path=%s\n", path);
		system(path);
		safef(path, sizeof(path), "mkdir -p %s/%s/%s/%s/%s/%s", outDir, outPrefix, "user", u, "full", f->path);
		uglyf("path=%s\n", path);
		system(path);
		*r = '/';
		}

            // context unified
	    safef(path, sizeof(path), "%s/%s/%s/%s/%s/%s.diff", outDir, outPrefix, "user", u, "context", f->path);
	    uglyf("path=%s\n", path);

	    safef(gitCmd,sizeof(gitCmd), ""
	    "git show %s %s > %s"
	    , c->commitId, f->path, path);
	    uglyf("gitCmd=%s\n", gitCmd);
	    system(gitCmd);
	    // TODO error handling

            // full text (up to 10,000 lines)
	    safef(path, sizeof(path), "%s/%s/%s/%s/%s/%s.diff", outDir, outPrefix, "user", u, "full", f->path);
	    uglyf("path=%s\n", path);

	    safef(gitCmd,sizeof(gitCmd), ""
	    "git show --unified=10000 %s %s > %s"
	    , c->commitId, f->path, path);
	    uglyf("gitCmd=%s\n", gitCmd);
	    system(gitCmd);
	    // TODO error handling

	    //git show --unified=10000 11a20b6cd113d75d84549eb642b7f2ac7a2594fe src/utils/qa/weeklybld/buildEnv.csh


	    }
	}
    }
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
slNameSort(&users);

/* create prefix dir */
char path[256];
safef(path, sizeof(path), "%s/%s", outDir, outPrefix);
if (!fileExists(path) && mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
    errnoAbort("unable to mkdir %s", path);

/* create file dir */
safef(path, sizeof(path), "%s/%s/%s", outDir, outPrefix, "file");
if (!fileExists(path) && mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
    errnoAbort("unable to mkdir %s", path);

/* create user dir */
safef(path, sizeof(path), "%s/%s/%s", outDir, outPrefix, "user");
if (!fileExists(path) && mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
    errnoAbort("unable to mkdir %s", path);

struct slName*u;
for(u = users; u; u = u->next)
    {
    printf("user: %s\n", u->name);

    /* create user/name dir */
    safef(path, sizeof(path), "%s/%s/%s/%s", outDir, outPrefix, "user", u->name);
    if (!fileExists(path) && mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
	errnoAbort("unable to mkdir %s", path);

    /* create user/name/context dir */
    safef(path, sizeof(path), "%s/%s/%s/%s/%s", outDir, outPrefix, "user", u->name, "context");
    if (!fileExists(path) && mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
	errnoAbort("unable to mkdir %s", path);

    /* create user/name/full dir */
    safef(path, sizeof(path), "%s/%s/%s/%s/%s", outDir, outPrefix, "user", u->name, "full");
    if (!fileExists(path) && mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
	errnoAbort("unable to mkdir %s", path);

    /* make user's reports */
    doUser(u->name, commits);    

    }

}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, options);
if (argc != 9)
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
outPrefix = argv[8];

userHash = hashNew(5);

chdir(repoDir);

gitReports();

hashFree(&userHash);
printf("Done.\n");
return 0;
}

