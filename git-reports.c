/* git-reports.c for creating git equivalent of cvs-reports. */

#include "common.h"
#include "options.h"
#include "dystring.h"
#include "errabort.h"
#include "hash.h"
#include "linefile.h"
#include "htmshell.h"

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
    int linesChanged;
    };

struct commit 
    {
    struct commit *next;
    int commitNumber;    // used for sorting fileviews
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
int numCommits = 0;
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
    commit->commitNumber = ++numCommits;

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
    commit->date = cloneString(line);

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

int makeHtml(char *diffPath, char *htmlPath, char *path, char *commitId)
/* Make a color-coded html diff 
 * Return the number of lines changed */
{
int linesChanged = 0;

FILE *h = mustOpen(htmlPath, "w");
struct lineFile *lf = lineFileOpen(diffPath, TRUE);
int lineSize;
char *line;
char *xline = NULL;
boolean inBody = FALSE;
boolean inBlock = TRUE;
int blockP = 0, blockN = 0;
fprintf(h, "<html>\n<head>\n<title>%s %s</title>\n</head>\n</body>\n<pre>\n", path, commitId);
while (lineFileNext(lf, &line, &lineSize))
    {
    xline = htmlEncode(line);	
    if (line[0] == '-')
	{
	fprintf(h, "<span style=\"background-color:#FF9999\">%s</span>\n", xline);
	if (inBody)
	    {
	    inBlock = TRUE;
	    ++blockN;
	    }
	}
    else if (line[0] == '+')
	{
	fprintf(h, "<span style=\"background-color:#99FF99\">%s</span>\n", xline);
	if (inBody)
	    {
	    inBlock = TRUE;
	    ++blockP;
	    }
	}
    else
	{
	if (line[0] == '@')
	    fprintf(h, "<span style=\"background-color:#FFFF99\">%s</span>\n", xline);
	else
	    fprintf(h, "%s\n", xline);
	if (inBody)
	    {
	    if (inBlock)
		{
		inBlock = FALSE;
		if (blockP >= blockN)
		    linesChanged += blockP;
		else
		    linesChanged += blockN;
		blockP = 0;
		blockN = 0;
		}
	    }
	}
	
    if (line[0] == '@')
	inBody = TRUE;

    freeMem(xline);

    }
// what if there is no last trailing line to end the last block?
if (inBody)
    {
    if (inBlock)
	{
	inBlock = FALSE;
	if (blockP >= blockN)
	    linesChanged += blockP;
	else
	    linesChanged += blockN;
	blockP = 0;
	blockN = 0;
	}
    }

lineFileClose(&lf);
fprintf(h, "</pre>\n</body>\n</html>\n");
fclose(h);
return linesChanged;
}


void doUser(char *u, struct commit *commits, int *saveUlc, int *saveUfc)
/* process one user */
{


char userPath[1024];
safef(userPath, sizeof(userPath), "%s/%s/%s/%s/index.html", outDir, outPrefix, "user", u);

FILE *h = mustOpen(userPath, "w");
fprintf(h, "<html>\n<head>\n<title>%s Commits View</title>\n</head>\n</body>\n", u);
fprintf(h, "<h2>Commits for %s</h2>\n", u);

//switch to grouped by file view, user index
fprintf(h, "<h2>%s to %s (%s to %s) %s</h2>\n", startTag, endTag, startDate, endDate, title);

fprintf(h, "<pre>\n");


int userLinesChanged = 0;
int userFileCount = 0;   // TODO do we want to not count the same file twice?

char *cDiff = NULL, *cHtml = NULL, *fDiff = NULL, *fHtml = NULL;
char *relativePath = NULL;
char *commonPath = NULL;

struct commit *c = NULL;
struct files *f = NULL;
for(c = commits; c; c = c->next)
    {
    if (sameString(c->author, u))
	{
	fprintf(h, "%s\n", c->commitId);
	fprintf(h, "%s\n", c->date);
	fprintf(h, "%s\n", c->comment);
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
	    safef(path, sizeof(path), "%s/%s%s", "context", f->path, c->commitId);
	    relativePath = cloneString(path);

	    safef(path, sizeof(path), "%s/%s/%s/%s/%s", outDir, outPrefix, "user", u, relativePath);
	    commonPath = cloneString(path);

	    safef(path, sizeof(path), "%s.html", commonPath);
	    cHtml = cloneString(path);

	    safef(path, sizeof(path), "%s.diff", commonPath);
	    cDiff = cloneString(path);

	    uglyf("path=%s\n", path);
	    safef(gitCmd,sizeof(gitCmd), 
		"git show %s %s > %s"
		, c->commitId, f->path, cDiff);
	    uglyf("gitCmd=%s\n", gitCmd);
	    system(gitCmd);
	    // TODO error handling

	    
	    // we need a lame work-around with this version of git
            // because there is odd and varying unwanted context text after @@ --- @@ in diff output
	    safef(gitCmd,sizeof(gitCmd), ""
		"sed -i -e 's/\\(^@@ .* @@\\).*/\\1/' %s",
		cDiff);
	    uglyf("sedCmd=%s\n", gitCmd);
	    system(gitCmd);
	    // TODO error handling
	    


	    // make context html page
	    f->linesChanged = makeHtml(cDiff, cHtml, f->path, c->commitId);
	    userLinesChanged += f->linesChanged;
	    ++userFileCount;   // TODO do we want to not count the same file twice?

	    freeMem(cDiff);
	    freeMem(cHtml);
	    safef(path, sizeof(path), "%s.html", relativePath);
	    cHtml = cloneString(path);
	    safef(path, sizeof(path), "%s.diff", relativePath);
	    cDiff = cloneString(path);



            // full text (up to 10,000 lines)
	    freeMem(relativePath);
	    safef(path, sizeof(path), "%s/%s%s", "full", f->path, c->commitId);
	    relativePath = cloneString(path);

	    safef(path, sizeof(path), "%s/%s/%s/%s/%s", outDir, outPrefix, "user", u, relativePath);
	    freeMem(commonPath);
	    commonPath = cloneString(path);

	    safef(path, sizeof(path), "%s.html", commonPath);
	    fHtml = cloneString(path);

	    safef(path, sizeof(path), "%s.diff", commonPath);
	    fDiff = cloneString(path);

	    safef(gitCmd,sizeof(gitCmd), ""
	    "git show --unified=10000 %s %s > %s"
	    , c->commitId, f->path, fDiff);
	    uglyf("gitCmd=%s\n", gitCmd);
	    system(gitCmd);
	    // TODO error handling

	    //git show --unified=10000 11a20b6cd113d75d84549eb642b7f2ac7a2594fe src/utils/qa/weeklybld/buildEnv.csh

	    // we need a lame work-around with this version of git
            // because there is odd and varying unwanted context text after @@ --- @@ in diff output
	    safef(gitCmd,sizeof(gitCmd), ""
		"sed -i -e 's/\\(^@@ .* @@\\).*/\\1/' %s",
		fDiff);
	    uglyf("sedCmd=%s\n", gitCmd);
	    system(gitCmd);
	    // TODO error handling

	    // make full html page
	    makeHtml(fDiff, fHtml, f->path, c->commitId);

	    freeMem(fDiff);
	    freeMem(fHtml);
	    safef(path, sizeof(path), "%s.html", relativePath);
	    fHtml = cloneString(path);
	    safef(path, sizeof(path), "%s.diff", relativePath);
	    fDiff = cloneString(path);

	    // make file view links
	    fprintf(h, "  %s - lines changed %d, "
		"context: <A href=\"%s\">html</A>, <A href=\"%s\">text</A>, "
		"full: <A href=\"%s\">html</A>, <A href=\"%s\">text</A>\n"
		, f->path, f->linesChanged
		, cHtml, cDiff, fHtml, fDiff);

	    freeMem(relativePath);
	    freeMem(commonPath);
	    freeMem(cDiff);
	    freeMem(cHtml);
	    freeMem(fDiff);
	    freeMem(fHtml);

	    }
	fprintf(h, "\n");
	}
    }
fprintf(h, "</pre>\n</body>\n</html>\n");
fclose(h);
*saveUlc = userLinesChanged;
*saveUfc = userFileCount;
}


void gitReports()
/* generate code-review reports from git repo */
{
int totalChangedLines = 0;
int totalChangedFiles = 0;

int userChangedLines = 0;
int userChangedFiles = 0;

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


char usersPath[1024];
safef(usersPath, sizeof(usersPath), "%s/%s/%s/index.html", outDir, outPrefix, "user");

FILE *h = mustOpen(usersPath, "w");
fprintf(h, "<html>\n<head>\n<title>Git Changes By User</title>\n</head>\n</body>\n");
fprintf(h, "<h2>Git Changes By User</h2>\n");

fprintf(h, "<h2>%s to %s (%s to %s) %s</h2>\n", startTag, endTag, startDate, endDate, title);

fprintf(h, "<pre>\n");



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

    userChangedLines = 0;
    userChangedFiles = 0;

    // DEBUG REMOVE
    if (sameString(u->name, "galt"))
    /* make user's reports */
    doUser(u->name, commits, &userChangedLines, &userChangedFiles);

    char relPath[1024];
    safef(relPath, sizeof(relPath), "%s/index.html", u->name);
    fprintf(h, "  <A href=\"%s\">%s</A> - changed lines: %d, files: %d\n", relPath, u->name, userChangedLines, userChangedFiles);

    totalChangedLines += userChangedLines;
    totalChangedFiles += userChangedFiles;  
    // TODO do we have to worry about counting the same file more than once?

    }

fprintf(h, "\n  lines changed: %d\n  files changed: %d\n", totalChangedLines, totalChangedFiles);

fprintf(h, "</pre>\n</body>\n</html>\n");
fclose(h);
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

