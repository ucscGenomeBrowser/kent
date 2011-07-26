/* git-reports.c for creating git equivalent of cvs-reports.
 * 
 *  Copyright Galt Barber 2010.   
 * 
 *  Anyone is free to use it.
 *  Just include me in your credits.
 *  Likewise cvs-reports was created by Mark Diekhans.
 */

#include "common.h"
#include "options.h"
#include "dystring.h"
#include "errabort.h"
#include "hash.h"
#include "linefile.h"
#include "htmshell.h"
#include "portable.h"

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
int  contextSize;

char gitCmd[1024];
char *tempMakeDiffName = NULL;

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
    char *merge;  
    char *author;
    char *date;
    char *comment;
    struct files *files;
    };


struct comFile
    {
    struct comFile *next;
    struct files *f;
    struct commit *commit;
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
    "  --context=N - show N lines of context around a change\n"
    "  --help - this help screen\n",
    msg);
}

static struct optionSpec options[] =
{
    {"-context", OPTION_INT},
    {"-help", OPTION_BOOLEAN},
    {NULL, 0},
};

void runShell(char *cmd)
/* Run a command and do simple error-checking */
{
int exitCode = system(cmd);
if (exitCode != 0)
    errAbort("system command [%s] failed with exitCode %d", cmd, exitCode);
}

void makeDiffAndSplit(struct commit *c, char *u, boolean full);  // FOREWARD REFERENCE

struct commit* getCommits()
/* Get all commits from startTag to endTag */
{
int numCommits = 0;
safef(gitCmd,sizeof(gitCmd), ""
"git log %s..%s --name-status > commits.tmp"
, startTag, endTag);
runShell(gitCmd);
struct lineFile *lf = lineFileOpen("commits.tmp", TRUE);
int lineSize;
char *line;
struct commit *commits = NULL, *commit = NULL;
struct files *files = NULL, *f = NULL;
char *sep = "";
while (lineFileNext(lf, &line, &lineSize))
    {
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
	commit->merge = cloneString(line);
	lineFileNext(lf, &line, &lineSize);
	w = nextWord(&line);
	}
    if (!sameString("Author:", w))
	errAbort("expected keyword Author: parsing commits.tmp\n");

    /* by request, keep just the email account name */
    char *lc = strchr(line, '<');
    if (!lc)
	errAbort("expected '<' char in email address in Author: parsing commits.tmp\n");
    ++lc;
    char *rc = strchr(lc, '>');
    if (!rc)
	errAbort("expected '>' char in email address in Author: parsing commits.tmp\n");
    char *ac = strchr(lc, '@');
    if (ac)
	rc = ac;
    commit->author = cloneStringZ(lc, rc-lc);

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

    if (commit->merge)
	{
	makeDiffAndSplit(commit, "getFileNamesForMergeCommit", FALSE);  // special tricks to get this list (status field will not be applicable).
	}
    else
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

	commit->files = files;
	}

    
    if (!startsWith("Merge branch 'master' of", commit->comment) &&
	!endsWith(commit->comment, "elease log update"))  /* filter out automatic release log commits */
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
	{
    	verbose(2, "%c %s\n", f->type, f->path);

	// anything other than M or A?
	if (f->type != 'M' && f->type != 'A' )
	    verbose(2, "special type: %c %s\n", f->type, f->path);
	}


    verbose(2, "------------\n");

    }
lineFileClose(&lf);
/* We want to keep them chronological order,
so do not need slReverse since the addHead reversed git log's rev chron order already */


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
char fmtString[256];
boolean inBody = FALSE;
boolean inBlock = TRUE;
int blockP = 0, blockN = 0;
fprintf(h, "<html>\n<head>\n<title>%s %s</title>\n</head>\n</body>\n<pre>\n", path, commitId);
boolean hasMore = TRUE;
boolean combinedDiff = FALSE;
while (hasMore)
    {
    boolean checkEob = FALSE;
    hasMore = lineFileNext(lf, &line, &lineSize);
    if (hasMore)
	{
	char *color = NULL;
	xline = htmlEncode(line);	
	if ((line[0] == '-') || (combinedDiff && (line[1] == '-')))
	    {
	    color = "#FF9999";  /* deleted text light red */
	    if (inBody)
		{
		inBlock = TRUE;
		++blockN;
		}
	    }
	else if ((line[0] == '+') || (combinedDiff && (line[1] == '+')))
	    {
	    color = "#99FF99";  /* added text light green */
	    if (inBody)
		{
		inBlock = TRUE;
		++blockP;
		}
	    }
	else
	    {
	    if (line[0] == '@')
		{
		color = "#FFFF99";  /* diff control text light yellow (red+green) */
		if (!combinedDiff && startsWith("@@@", line))
		    combinedDiff = TRUE;
		}
	    checkEob = TRUE;
	    }
	if (color)
	    safef(fmtString, sizeof(fmtString), "<span style=\"background-color:%s\">%%s</span>\n", color);
	else
	    safef(fmtString, sizeof(fmtString), "%%s\n");
	fprintf(h, fmtString, xline);
	
	if (line[0] == '@')
	    inBody = TRUE;

	freeMem(xline);
	}
    else
	{
	checkEob = TRUE;
	}

    if (checkEob && inBlock)
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


void makeDiffAndSplit(struct commit *c, char *u, boolean full)
/* Generate a full diff and then split it up into its parts.
 * This was motivated because no other way to show deleted files
 * since they are not in repo and git paths must actually exist
 * in working repo dir.  However leaving off the path produces
 * a diff with everything we want, we just have to split it up. */
{

if (c->merge)
    {
    safef(gitCmd,sizeof(gitCmd), 
	"git diff-tree --cc -b -w --no-prefix --unified=%d %s > %s"  // -b -w currently ignored for combined-diff
    , full ? 1000000 : contextSize
    , c->commitId, tempMakeDiffName);
    }
else
    {

    safef(gitCmd,sizeof(gitCmd), 
	"git diff -b -w --no-prefix --unified=%d %s^! > %s"  
	, full ? 1000000 : contextSize
	, c->commitId, tempMakeDiffName);
    //git shorthand: x^! is equiv to range x^ x, 
    //  i.e. just the one commit and nothing more.

    // hack until better fix - this is the case where there is no previous commit
    if (sameString(c->commitId, "dc78303b079985b5a146d093bbb8a5d06489562d"))
	{
	safef(gitCmd,sizeof(gitCmd), 
	    "git show -b -w --no-prefix --unified=%d %s > %s" // -b -w probably get ignored 
	, full ? 1000000 : contextSize
	, c->commitId, tempMakeDiffName);
	}
    }
runShell(gitCmd);


// now parse it and split it into separate files with the right path.
boolean getNamesOnly = c->merge && sameString(u,"getFileNamesForMergeCommit");
struct lineFile *lf = lineFileOpen(tempMakeDiffName, TRUE);
int lineSize;
char *line;
FILE *h = NULL;
char *section = "@@";
if (c->merge)
    section = "@@@";
while (lineFileNext(lf, &line, &lineSize))
    {
    char *pattern = "diff --git ";
    if (c->merge)
	pattern = "diff --cc ";
    if (startsWith(pattern, line))
	{
	if (h)
	    {
	    fclose(h);
	    h = NULL;
	    }
	char *fpath = line + strlen(pattern);
	if (getNamesOnly) // too bad we had not choice but to get the merge names this way.
	    {
	    /* collect the files-list */
	    struct files *f = NULL;
	    AllocVar(f);
	    f->path = cloneString(fpath);
	    slAddHead(&c->files, f);
	    }
	else
	    {
	    if (!c->merge)
		{
		fpath = strchr(fpath, ' ');
		++fpath;   // now we should be pointing to the world
		}

	    char path[1024];
	    char *r = strrchr(fpath, '/');
	    if (r)
		{
		*r = 0;
		/* make internal levels of subdirs */
		safef(path, sizeof(path), "mkdir -p %s/%s/%s/%s/%s/%s", outDir, outPrefix, "user", u, full ? "full" : "context", fpath);
		runShell(path);
		*r = '/';
		}
	    safef(path, sizeof(path), "%s/%s/%s/%s/%s/%s.%s.diff"
		, outDir, outPrefix, "user", u, full ? "full" : "context", fpath, c->commitId);

	    h = mustOpen(path, "w");
	    fprintf(h, "%s\n", c->commitId);
	    if (c->merge)
		fprintf(h, "Merge parents %s\n", c->merge);
	    fprintf(h, "%s\n", c->author);
	    fprintf(h, "%s\n", c->date);
	    fprintf(h, "%s\n", c->comment);
	    }
	}
    else if (startsWith(section, line))
	{
	char *end = strchr(line+strlen(section), '@');
	*(end+strlen(section)) = 0;  // chop the weird unwanted context string from here following e.g. 
        //@@ -99,7 +99,9 @@ weird unwanted context string here
        // converts to
        //@@ -99,7 +99,9 @@
	// saves 17 seconds over the more expensive sed command
	}
    if (h)
	fprintf(h, "%s\n", line);
    }
if (h)
    {
    fclose(h);
    h = NULL;
    }
lineFileClose(&lf);
if (getNamesOnly) // too bad we had not choice but to get the merge names this way.
    slReverse(&c->files);
}


void doUserCommits(char *u, struct commit *commits, int *saveUlc, int *saveUfc)
/* process one user, commit-view */
{


char userPath[1024];
safef(userPath, sizeof(userPath), "%s/%s/%s/%s/index.html", outDir, outPrefix, "user", u);

FILE *h = mustOpen(userPath, "w");
fprintf(h, "<html>\n<head>\n<title>Commits for %s</title>\n</head>\n</body>\n", u);
fprintf(h, "<h1>Commits for %s</h1>\n", u);

fprintf(h, "switch to <A href=\"index-by-file.html\">files view</A>, <A href=\"../index.html\">user index</A>\n");
fprintf(h, "<h3>%s to %s (%s to %s) %s</h3>\n", startTag, endTag, startDate, endDate, title);

fprintf(h, "<ul>\n");


int userLinesChanged = 0;
int userFileCount = 0;

char *cDiff = NULL, *cHtml = NULL, *fDiff = NULL, *fHtml = NULL;
char *relativePath = NULL;
char *commonPath = NULL;

struct commit *c = NULL;
struct files *f = NULL;
for(c = commits; c; c = c->next)
    {
    if (sameString(c->author, u))
	{
	//fprintf(h, "%s\n", c->commitId);
	//fprintf(h, "%s\n", c->date);

	char *cc = htmlEncode(c->comment);
	char *ccc = replaceChars(cc, "\n", "<br>\n");
	fprintf(h, "<li>%s\n", ccc);
	freeMem(cc);
	freeMem(ccc);

	makeDiffAndSplit(c, u, FALSE);
	makeDiffAndSplit(c, u, TRUE);
	for(f = c->files; f; f = f->next)
	    {
	    char path[1024];

            // context unified
	    safef(path, sizeof(path), "%s/%s.%s", "context", f->path, c->commitId);
	    relativePath = cloneString(path);

	    safef(path, sizeof(path), "%s/%s/%s/%s/%s", outDir, outPrefix, "user", u, relativePath);
	    commonPath = cloneString(path);

	    safef(path, sizeof(path), "%s.html", commonPath);
	    cHtml = cloneString(path);

	    safef(path, sizeof(path), "%s.diff", commonPath);
	    cDiff = cloneString(path);

	    // make context html page
	    f->linesChanged = makeHtml(cDiff, cHtml, f->path, c->commitId);

	    userLinesChanged += f->linesChanged;
	    ++userFileCount;

	    freeMem(cDiff);
	    freeMem(cHtml);
	    safef(path, sizeof(path), "%s.html", relativePath);
	    cHtml = cloneString(path);
	    safef(path, sizeof(path), "%s.diff", relativePath);
	    cDiff = cloneString(path);



            // full text (up to 10,000 lines)
	    freeMem(relativePath);
	    safef(path, sizeof(path), "%s/%s.%s", "full", f->path, c->commitId);
	    relativePath = cloneString(path);

	    safef(path, sizeof(path), "%s/%s/%s/%s/%s", outDir, outPrefix, "user", u, relativePath);
	    freeMem(commonPath);
	    commonPath = cloneString(path);

	    safef(path, sizeof(path), "%s.html", commonPath);
	    fHtml = cloneString(path);

	    safef(path, sizeof(path), "%s.diff", commonPath);
	    fDiff = cloneString(path);


	    //git show --unified=10000 11a20b6cd113d75d84549eb642b7f2ac7a2594fe src/utils/qa/weeklybld/buildEnv.csh

	    // make full html page
	    makeHtml(fDiff, fHtml, f->path, c->commitId);

	    freeMem(fDiff);
	    freeMem(fHtml);
	    safef(path, sizeof(path), "%s.html", relativePath);
	    fHtml = cloneString(path);
	    safef(path, sizeof(path), "%s.diff", relativePath);
	    fDiff = cloneString(path);

	    // make file diff links
	    fprintf(h, "<ul><li> %s - lines changed %d, "
		"context: <A href=\"%s\">html</A>, <A href=\"%s\">text</A>, "
		"full: <A href=\"%s\">html</A>, <A href=\"%s\">text</A>%s</li></ul>\n"
		, f->path, f->linesChanged
		, cHtml, cDiff, fHtml, fDiff, f->linesChanged == 0 ? " (a binary file or whitespace-only change shows no diff)" : "");

	    freeMem(relativePath);
	    freeMem(commonPath);
	    freeMem(cDiff);
	    freeMem(cHtml);
	    freeMem(fDiff);
	    freeMem(fHtml);

	    }
	fprintf(h, "</li>\n");
	}
    }
fprintf(h, "</ul>\n");
fprintf(h, "switch to <A href=\"index-by-file.html\">files view</A>, <A href=\"../index.html\">user index</A>\n");
fprintf(h, "</body>\n</html>\n");
fclose(h);
*saveUlc = userLinesChanged;
*saveUfc = userFileCount;
}




int slComFileCmp(const void *va, const void *vb)
/* Compare two comFiles. */
{
const struct comFile *a = *((struct comFile **)va);
const struct comFile *b = *((struct comFile **)vb);
int result = strcmp(a->f->path, b->f->path);
if (result == 0)
    result = b->commit->commitNumber - a->commit->commitNumber;
return result;
}


void doUserFiles(char *u, struct commit *commits)
/* process one user's files-view (or all if u is NULL) */
{

// http://hgwdev.cse.ucsc.edu/cvs-reports/branch/user/galt/index-by-file.html
// if u is NULL
// http://hgwdev.cse.ucsc.edu/cvs-reports/branch/file/index.html
char userPath[1024];
if (u)
    safef(userPath, sizeof(userPath), "%s/%s/%s/%s/index-by-file.html", outDir, outPrefix, "user", u);
else
    safef(userPath, sizeof(userPath), "%s/%s/%s/index.html", outDir, outPrefix, "file");

FILE *h = mustOpen(userPath, "w");
if (u)
    {
    fprintf(h, "<html>\n<head>\n<title>File Changes for %s</title>\n</head>\n</body>\n", u);
    fprintf(h, "<h1>File Changes for %s</h1>\n", u);
    fprintf(h, "switch to <A href=\"index.html\">commits view</A>, <A href=\"../index.html\">user index</A>");
    }
else
    {
    fprintf(h, "<html>\n<head>\n<title>All File Changes</title>\n</head>\n</body>\n");
    fprintf(h, "<h1>All File Changes</h1>\n");
    }

fprintf(h, "<h3>%s to %s (%s to %s) %s</h3>\n", startTag, endTag, startDate, endDate, title);

fprintf(h, "<ul>\n");


int totalLinesChanged = 0;
int totalFileCount = 0;

char *cDiff = NULL, *cHtml = NULL, *fDiff = NULL, *fHtml = NULL;
char *relativePath = NULL;

struct commit *c = NULL;
struct files *f = NULL;

struct comFile *comFiles = NULL, *cf = NULL;

// pre-filter for u if u is not NULL  
for(c = commits; c; c = c->next)
    {
    if (!u || (u && sameString(c->author, u)))
	{
	for(f = c->files; f; f = f->next)
	    {
	    AllocVar(cf);
	    cf->f = f;
	    cf->commit = c;
	    slAddHead(&comFiles, cf);
	    }
	}
    }
// sort by file path, and then by commitNumber
//  so that newest commit is on the bottom.
slSort(&comFiles, slComFileCmp);

char *lastPath = "";
char *closure = "";

for(cf = comFiles; cf; cf = cf->next)
    {
    c = cf->commit;
    f = cf->f;
 
    if (!sameString(f->path, lastPath))
	{
	fprintf(h, "%s", closure);
	lastPath = f->path;
	fprintf(h, "<li>%s\n", f->path);
    	closure = "</li>\n";
	}


    char path[1024];

    // context unified
    if (u)
	safef(path, sizeof(path), "%s/%s.%s", "context", f->path, c->commitId);
    else
	safef(path, sizeof(path), "../user/%s/%s/%s.%s", c->author, "context", f->path, c->commitId);
    relativePath = cloneString(path);
    safef(path, sizeof(path), "%s.html", relativePath);
    cHtml = cloneString(path);
    safef(path, sizeof(path), "%s.diff", relativePath);
    cDiff = cloneString(path);



    // full text (up to 10,000 lines)
    freeMem(relativePath);
    if (u)
	safef(path, sizeof(path), "%s/%s.%s", "full", f->path, c->commitId);
    else
	safef(path, sizeof(path), "../user/%s/%s/%s.%s", c->author, "full", f->path, c->commitId);
    relativePath = cloneString(path);
    safef(path, sizeof(path), "%s.html", relativePath);
    fHtml = cloneString(path);
    safef(path, sizeof(path), "%s.diff", relativePath);
    fDiff = cloneString(path);

    // make file view links
    fprintf(h, "<ul><li>");
    fprintf(h, " lines changed %d, "
	"context: <A href=\"%s\">html</A>, <A href=\"%s\">text</A>, "
	"full: <A href=\"%s\">html</A>, <A href=\"%s\">text</A><br>\n"
	, f->linesChanged
	, cHtml, cDiff, fHtml, fDiff);

    //fprintf(h, "  %s\n", c->commitId);
    //fprintf(h, "  %s\n", c->date);
    char *cc = htmlEncode(c->comment);
    char *ccc = replaceChars(cc, "\n", "<br>\n");
    fprintf(h, "    %s\n", ccc);
    freeMem(cc);
    freeMem(ccc);
    fprintf(h, "</li></ul>\n");

    freeMem(relativePath);
    freeMem(cDiff);
    freeMem(cHtml);
    freeMem(fDiff);
    freeMem(fHtml);

    totalLinesChanged += f->linesChanged;
    ++totalFileCount;

    fprintf(h, "\n");
    }
fprintf(h, "%s", closure);
fprintf(h, "</ul>\n");
if (u)
    {
    fprintf(h, "switch to <A href=\"index.html\">commits view</A>, <A href=\"../index.html\">user index</A>");
    }
else
    {
    fprintf(h, "<ul>\n");
    fprintf(h, "<li> lines changed: %d</li>\n", totalLinesChanged);
    fprintf(h, "<li> files changed: %d</li>\n", totalFileCount);
    fprintf(h, "</ul>\n");
    }
fprintf(h, "</body>\n</html>\n");
fclose(h);
}


void doMainIndex()
/* Create simple main index page */
{
char path[256];
safef(path, sizeof(path), "%s/%s/index.html", outDir, outPrefix);

FILE *h = mustOpen(path, "w");
fprintf(h, "<html>\n<head>\n<title>Source Code Changes</title>\n</head>\n</body>\n");
fprintf(h, "<h1>%s %s Changes</h1>\n", title, outPrefix);

fprintf(h, "<h2>%s to %s (%s to %s) %s</h2>\n", startTag, endTag, startDate, endDate, title);

fprintf(h, "<ul>\n");

fprintf(h, "<li> <A href=\"user/index.html\">Changes by User</A></li>\n");
fprintf(h, "\n");
fprintf(h, "<li> <A href=\"file/index.html\">All File Changes</A></li>\n");

fprintf(h, "</ul>\n</body>\n</html>\n");
fclose(h);

}

void makeMyDir(char *path)
/* Make a single dir if it does not already exit */
{
if (!fileExists(path) && mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
    errnoAbort("unable to mkdir %s", path);
}

void gitReports()
/* Generate code-review reports from git repo */
{
int totalChangedLines = 0;
int totalChangedFiles = 0;

int userChangedLines = 0;
int userChangedFiles = 0;

tempMakeDiffName = cloneString(rTempName("/tmp", "makeDiff", ".tmp"));

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
makeMyDir(path);

/* create file dir */
safef(path, sizeof(path), "%s/%s/%s", outDir, outPrefix, "file");
makeMyDir(path);

/* create user dir */
safef(path, sizeof(path), "%s/%s/%s", outDir, outPrefix, "user");
makeMyDir(path);


char usersPath[1024];
safef(usersPath, sizeof(usersPath), "%s/%s/%s/index.html", outDir, outPrefix, "user");

FILE *h = mustOpen(usersPath, "w");
fprintf(h, "<html>\n<head>\n<title>Changes By User</title>\n</head>\n</body>\n");
fprintf(h, "<h1>Changes By User</h1>\n");

fprintf(h, "<h2>%s to %s (%s to %s) %s</h2>\n", startTag, endTag, startDate, endDate, title);

fprintf(h, "<ul>\n");



struct slName*u;
for(u = users; u; u = u->next)
    {
    printf("user: %s\n", u->name);

    /* create user/name dir */
    safef(path, sizeof(path), "%s/%s/%s/%s", outDir, outPrefix, "user", u->name);
    makeMyDir(path);

    /* create user/name/context dir */
    safef(path, sizeof(path), "%s/%s/%s/%s/%s", outDir, outPrefix, "user", u->name, "context");
    makeMyDir(path);

    /* create user/name/full dir */
    safef(path, sizeof(path), "%s/%s/%s/%s/%s", outDir, outPrefix, "user", u->name, "full");
    makeMyDir(path);

    userChangedLines = 0;
    userChangedFiles = 0;

    /* make user's reports */
    doUserCommits(u->name, commits, &userChangedLines, &userChangedFiles);

    doUserFiles(u->name, commits);

    char relPath[1024];
    safef(relPath, sizeof(relPath), "%s/index.html", u->name);
    fprintf(h, "<li> <A href=\"%s\">%s</A> - changed lines: %d, files: %d</li>\n", relPath, u->name, userChangedLines, userChangedFiles);

    totalChangedLines += userChangedLines;
    totalChangedFiles += userChangedFiles;  

    }

fprintf(h, "</ul>\n");
if (u)
    {
    fprintf(h, "switch to <A href=\"index.html\">commits view</A>, <A href=\"../index.html\">user index</A>");
    }
else
    {
    fprintf(h, "<ul>\n");
    fprintf(h, "<li>  lines changed: %d</li>\n", totalChangedLines);
    fprintf(h, "<li>  files changed: %d</li>\n", totalChangedFiles);
    fprintf(h, "</ul>\n");
    }
fprintf(h, "</body>\n</html>\n");

fclose(h);

// make index of all files view
doUserFiles(NULL, commits);

// make main index page
doMainIndex();

// tidying up
unlink(tempMakeDiffName);
freez(&tempMakeDiffName);
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, options);
if (argc != 9)
    usage("wrong number of args");
if (optionExists("-help"))
    usage("help");

startTag = argv[1];
endTag = argv[2];
startDate = argv[3];
endDate = argv[4];
title = argv[5];
repoDir = argv[6];
outDir = argv[7];
outPrefix = argv[8];

contextSize = optionInt("-context", 15);

userHash = hashNew(5);

setCurrentDir(repoDir);

gitReports();

hashFree(&userHash);
printf("Done.\n");
return 0;
}

