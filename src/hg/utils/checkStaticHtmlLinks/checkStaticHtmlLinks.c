/* checkStaticHtmlLinks - checks htdocs and htdocsExtra static html files
 *   
 *   TODO - fix to process included files the same way that hgMenubar does.
 */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "obscure.h"

boolean makeLinksRelative = FALSE;

char *fixMsg = "WOULD FIX";

static struct optionSpec options[] = {
   {"help", OPTION_BOOLEAN},
   {"makeLinksRelative", OPTION_BOOLEAN},
   {NULL, 0},
};

void usage()
/* Explain usage and exit. */
{
errAbort(
	"\n"
	"usage:\n"
	"\n"
	"checkStaticHtmlLinks > checkLinks.log\n"
	"\n"
	"Checks html files under $HOME/kent/src/hg/htdocs\n"
	"Checks for correct link formatting.\n"
	"Checks for existence of link target.\n"
	"Checks for relative links.\n"
	"\n"
	"Option:\n"
	" -makeLinksRelative overwrites the input file with the fixed relative links.\n"
	"\n"
	"Relative links are critical for allowing genome browser software\n"
	"software to be installed in subdirectories below the web root.\n"
	"For example, /project1/cgi-bin instead of /cgi-bin\n"
	"For links that do not begin with a protocol, this means avoiding a leading slash '/'.\n"
	"\n"
	"Server-Side Includes issues:\n"
	"There are many SSI included files. The ones in /inc are an exception and this utility ignores them.\n"
	"These html files need a special exception for allowing leading slashes in links\n"
	"because these topbar menu files are included by multiple files at different subdirectory depths,\n"
	"so a single relative link cannot work for these.\n"
	"These require a special solution to add the prefix like /project1 above, if needed.\n"
	"\n"
	"Note that the output log is large and detailed but can be easily saved, grepped, sorted, counted.\n"
	"This query will find all cases where the link target was not found:\n"
	"  grep 'NOT EXIST' checkLinks.log\n"
	"If found, either the link is misspelled or the target resource is missing from your working directory.\n"
	);
}

int globalLinksFixed = 0;
int globalFilesFixed = 0;
char *globalErrPath = NULL;
int globalLineNo = 0;


// Repair of the Links in a line
struct lineRepair
/* Keep track of line edits */
    {
    struct lineRepair *next;
    int editPos;       /* position in original string */
    int editLength;    /* length of original section */
    char *newText;     /* new text to replace original */
    boolean wasQuoted; /* was the original section quoted? */
    };

// SSI Support
struct ssiInclude
/* Keep track of all ssi includes seen */
    {
    struct ssiInclude *next;
    char *includerUrl;      /* file from which it was included */
    char *originalUrl;      /* original file in which all inclusions apply */
    int originalLevel;      /* level of the original inclusion page */
    };



struct hash *globalIncludeHash = NULL;



char *parseLink(char *link, boolean *wasQuoted)
/* check for optional quotation, parse to end of link,
 * do not include quotes */
{
char *start = NULL, *end = NULL;
if (link[0] == '\"')
    {
    *wasQuoted = TRUE;
    ++link;
    start = link;
    end = strchr(link, '\"');
    if (!end)
	{
	errAbort("unexpected error: endquote \" not found in line [%s]\n in %s at %d", link, globalErrPath, globalLineNo);
	end = start + strlen(start); // just to keep going.	
	}
    }
else if (link[0] == '\'')
    {
    *wasQuoted = TRUE;
    ++link;
    start = link;
    end = strchr(link, '\'');
    if (!end)
	{
	errAbort("unexpected error: endquote ' not found in line [%s]\n in %s at %d", link, globalErrPath, globalLineNo);
	end = start + strlen(start); // just to keep going.	
	}
    }
else
    {

    //warn("DEBUG FOUND UNQUOTED LINK [%s]\n in %s at %d", link, globalErrPath, globalLineNo); // DEBUG REMOVE
    

    if (link[0]==' ')
	errAbort("FOUND SPACE AFTER tag= in LINK [%s]\n in %s at %d", link, globalErrPath, globalLineNo); 

    *wasQuoted = FALSE;
    start = link;
    end = strchr(link, '>');
    if (!end)
	{
	errAbort("FOUND UNQUOTED LINK AND NO SPACE at END of link [%s]\n in %s at %d", link, globalErrPath, globalLineNo);
	end = start + strlen(start); // just to keep going.	
	}
    }
return cloneStringZ(start, end-start);
}

char *combineUrlOnBase(char *dir, char *url)
/* Combine a url with . and .. onto the base dir */
{
char path[1024];
dir = cloneString(dir);
if (url[0] == '/') // absolute top-level url ignores base dir.
    {
    safef(path, sizeof path, ".%s", url);
    }
else
    {
    while(startsWith("../", url))
	{
	char *lastSlash = strrchr(dir, '/');
	if (lastSlash)
	    {
	    *lastSlash = 0;  // truncate the last dir part
	    url += strlen("../");
	    }
	else
	    {
	    errAbort("ERROR ? FOUND TOO MANY ../ in url [%s], dir now [%s]\n in %s at %d", url, dir, globalErrPath, globalLineNo);
	    }
	}
    safef(path, sizeof path, "%s/%s", dir, url);
    }
freeMem(dir);
return cloneString(path);

}


char *makeRelativeUrl(char *directory, char *absolutePath)
/* make relative path for absolutePath on base dir */
{
/*
 abs current path   (base dir)
 abs target path.   (absolutePath)
 relative link = ?  (result)

 same length/levels, then relative between them is "".
 if target is longer, then relative is just the rest after current.
 if target is shorter, then
   make a copy of current (cc)
     while target shorter than cc,
         add "../" to relative string
             chop back cc by one directory
               loop
                 append the rest of target.


sometimes the absolutepath might end in / ?
what about dir itself? never.

at the root level, dir will be "."
one level down would be e.g. "./ENCODE"


*/

char result[1024];
char abs[1024];
char dir[1024];

// TODO is this the right thing?
if (sameString(absolutePath,""))
    return cloneString("");

// make copies of the strings to mess with
safecpy(abs, sizeof abs, absolutePath);  
safecpy(dir, sizeof dir, directory);  

boolean slashTerminated = FALSE;
int la = strlen(abs);
int ld = strlen(dir);

// for simplicity make sure both are terminated by a slash
if (abs[la-1] == '/')
    slashTerminated = TRUE;
else
    {
    abs[la++] = '/';
    abs[la] = 0;
    }
dir[ld++] = '/';
dir[ld] = 0;
char dots[256] = "";
int ldots = 0;
while (!startsWith(dir,abs))
    { // chop dir back until they do match!
    //printf("dots=%s dir=%s abs=%s ld=%d\n", dots, dir, abs, ld); // DEBUG
    --ld; // skip over 0-terminator.
    // yes, this will skip over the trailing / and find the previous.
    while (dir[--ld] != '/') 
	/* do nothing */ ;	
    dir[++ld] = 0;
    safecat(dots, sizeof dots, "../");
    ldots += 3;
    }
//printf("dots=%s dir=%s abs=%s ld=%d\n", dots, dir, abs, ld); // DEBUG

safef(result, sizeof result, "%s%s", dots, abs+ld);
if (!slashTerminated)
    result[ldots+la-ld-1] = 0;  // trim trailing slash

return cloneString(result);

}

char *makePathFromUrl(char *absoluteUrl)
/* remove any extraneous junk from the url leaving the filename */
{
char *result = cloneString(absoluteUrl);
char *pound = strchr(result, '#');
if (pound)
    *pound = 0;
char *query = strchr(result, '?');
if (query)
    *query = 0;
return result;
}

char *manglePath(char *absoluteUrl)
/* remove any extraneous junk from the url leaving the filename */
{
char *result = absoluteUrl;
char path[1024];
if (startsWith("./js/", absoluteUrl)) // exception, this is really "../js/"
    {
    safef(path, sizeof path, ".%s", absoluteUrl);
    result = path;
    }
if (startsWith("./cgi-bin/", absoluteUrl)) // examine real file location 
    {
    safef(path, sizeof path, "/usr/local/apache/cgi-bin/%s", absoluteUrl+strlen("./cgi-bin/"));
    result = path;
    }
if (startsWith("./cgi-bin/das/", absoluteUrl)) // das exception, has weird urls, trying to be RESTFUL.
    {
    safecpy(path, sizeof path, "/usr/local/apache/cgi-bin/das");
    result = path;
    }
return cloneString(result);
}

void processHtmlFile(char *dir, char *htmlName, char *fullName, int level)
/* Read the html file, parse for links href, src */
{
globalErrPath = fullName;
printf("-----------------\n");
printf("%s\n", fullName);

if (fileSize(fullName) == 0) // nothing to do for empty files
    return;


char *line;
globalLineNo = 0;
int linesFixed = 0;
struct slName *lines = readAllLines(fullName);
struct slName *current = NULL, *prev = NULL, *prevPrev = NULL;
for (current = lines; current->next; ++globalLineNo, prevPrev = prev, prev = current, current = current->next)
    {
    line = current->name;
    char *origLine = line;
    // loop through the line in case there are multiple locations in one line
    boolean found = TRUE;
    int foundCount = 0;
    int patternLength = 0;
    char *posFound = NULL;
    struct lineRepair *edits = NULL;
    while (found)
	{
	found = FALSE;
	char *patterns[] = { "href=", "HREF=", "src=", "SRC=", "action=", "ACTION=", "<!--#include virtual=" };
	// always keep include virtual as last pattern
	int pattCount = sizeof (patterns) / sizeof (*patterns);
	int pattNo = 0;
	for(pattNo = 0; pattNo < pattCount; ++pattNo)
	    {
	    char *link = strstr(line, patterns[pattNo]);
	    if (link)
		{
		found = TRUE;
		patternLength = strlen(patterns[pattNo]);
		posFound = link;
		printf("pattern: %s\n", patterns[pattNo]);
		boolean wasQuoted = FALSE;
		char *url = parseLink(link+patternLength, &wasQuoted);
		// just for extra-credit, test for illegal chars in url:
		// should not contain spaces or quotes single or double, except in js
		if (strchr(url, ' ') && !startsWith("javascript:", url))
		    errAbort("FOUND ILLEGAL char [%c] in url [%s]\n in %s at %d", ' ', url, globalErrPath, globalLineNo);
		if (strchr(url, '\'') && !startsWith("javascript:", url))
		    errAbort("FOUND ILLEGAL char [%c] in url [%s]\n in %s at %d", '\'', url, globalErrPath, globalLineNo);
		if (strchr(url, '\"') && !startsWith("javascript:", url))
		    errAbort("FOUND ILLEGAL char [%c] in url [%s]\n in %s at %d", '\"', url, globalErrPath, globalLineNo);
		printf("url: %s\n", url);
		// skip the links which need no change
		// TODO there is probably a better way than just checking for a protocol,
		// although usually the presence of a protocol signals that there is nothing 
		// for us to worry about here.  Typically it will nearlyl always have a domain, etc.
		// but we could prove that by calling the url-parse routine in net.c
		if (startsWith("http://", url))
		    {
		    printf("STATUS: nothing to do\n");
		    }
		else if (startsWith("https://", url))
		    {
		    printf("STATUS: nothing to do\n");
		    }
		else if (startsWith("ftp://", url))
		    {
		    printf("STATUS: nothing to do\n");
		    }
		else if (startsWith("mailto:", url))
		    {
		    printf("STATUS: nothing to do\n");
		    }
		else if (startsWith("javascript:", url))
		    {
		    printf("STATUS: nothing to do\n");
		    }
		else if (startsWith("#", url))
		    {
		    printf("STATUS: internal page #tag, nothing to do\n");
		    }
		else
		    { // we have a real resource we can check its existence.

		    char *absoluteUrl = combineUrlOnBase(dir, url);
		    printf("absoluteUrl = [%s]\n", absoluteUrl);
		    // there should be no more . .. in the path ignoring the leading ./ from the basedir.
		    if (strstr(absoluteUrl, "../"))
			errAbort("UNEXPECTED ../ FOUND in absolute url [%s]\n in %s at %d", absoluteUrl, globalErrPath, globalLineNo);
		    if (strstr(absoluteUrl+1, "./"))
			errAbort("UNEXPECTED ./ FOUND in absolute url [%s]\n in %s at %d", absoluteUrl, globalErrPath, globalLineNo);

		    char *absolutePath = makePathFromUrl(absoluteUrl);
		    printf("absolutePath = [%s]\n", absolutePath);

		    absolutePath = manglePath(absolutePath); // e.g. deal with ./js
		    printf("mangled absolutePath = [%s]\n", absolutePath);

		    if (fileExists(absolutePath))
			{
			printf("[%s] exists in workingDir\n", absolutePath);

			if (pattNo==pattCount-1) // include virtual
			    {
			    struct ssiInclude *ssi = NULL;
			    AllocVar(ssi);
			    ssi->includerUrl = cloneString(fullName);
			    ssi->originalUrl = cloneString(fullName); // TODO when recursive includes worked out, RE-VISIT!
			    ssi->originalLevel = level;
			    hashAdd(globalIncludeHash, absolutePath, ssi);
			    printf("%s included from %s for %s at level %d\n", absolutePath, ssi->includerUrl, ssi->originalUrl, ssi->originalLevel);
			    //if ( (hel = hashLookup(globalIncludeHash, absolutePath)) == NULL )
			    // THESE things in /inc are so weird that I am not going to develop this part any further for now.
			    }

    
			}
		    else
			{
			// check htdocsExtra too
			char *homeDir = getenv("HOME");
			char extras[256];
			safef(extras, sizeof extras, "%s/htdocsExtras/%s", homeDir, absolutePath);
			//printf("DEBUG htdocsExtras path: %s\n", extras); // DEBUG REMOVE
			if (fileExists(extras))
			    {
			    printf("[%s] exists in htdocsExtras\n", absolutePath);
			    }
			else
			    {
			    if (!(endsWith(globalErrPath, "newsarch.html") && endsWith(url, "pbGateway")))
				printf("[%s] DOES NOT EXIST in workingDir, from page %s at %d\n", absolutePath, globalErrPath, globalLineNo);
			    }
			}


		    char *relativeUrl = makeRelativeUrl(dir, absoluteUrl);
		    printf("relativeUrl = [%s]\n", relativeUrl);
			    

		    if (startsWith("/", url))
			{
			printf("STATUS: top-level url needs fixing!\n");
			}

		    if (sameString(url,relativeUrl))
			{
			printf("STATUS: url good !\n");
			}
		    else
			{
			printf("STATUS: url differs from optimal relative !\n");

			// This works because we know we do not have any malformed links.
			struct lineRepair *e;
			AllocVar(e);
			e->editPos = (link + patternLength) - origLine;
			if (wasQuoted)
			    ++e->editPos;
			e->editLength = strlen(url);
			e->newText = cloneString(relativeUrl);
			e->wasQuoted = wasQuoted;
			slAddHead(&edits, e);


			// TEMPORARY DEBUGGING
			//printf("edit: pos=%d length=%d wasQuoted=%d newText=%s origLine=%s\n",
			//	    e->editPos, e->editLength, e->wasQuoted , e->newText, origLine);
			}



		    }
		
		
		++foundCount;
		line = posFound + patternLength;
		break;
		}
	    }

	}


    if (foundCount > 0)
	printf("origLine: %s\n", origLine);

    // TRY EDITS
    slReverse(&edits);
    int editCount = slCount(edits);
    if (editCount > 0)
	{
	if (editCount > 1)
	    printf("%d MULTIPLE EDITS ON ONE LINE %s\n", editCount, origLine);
	char newLine[4096] = "";
	int start = 0;
	struct lineRepair *e;
	for(e=edits; e; e=e->next)
	    {
	    // pick up the left-side from the original string.
	    safencat(newLine, sizeof newLine, origLine + start, e->editPos - start);
	    // pick up the new text
	    safencat(newLine, sizeof newLine, e->newText, strlen(e->newText));
	    start = e->editPos + e->editLength;
	    //TODO this could be used to check for quotes around expected positions in orig and new lines.
	    //e->wasQuoted
	    ++globalLinksFixed;
	    }
	// pick up the trailing bit on right
	safencat(newLine, sizeof newLine, origLine + start, strlen(origLine) - start);
	printf("newLine : %s\n%s\n", newLine, fixMsg);
        
        struct slName *temp = slNameNew(newLine);
        if (prev)
            prev->next = temp;
        temp->next = current->next;
        current = temp;
        linesFixed += 1;
	}

    if (foundCount > 1)
	printf("%d MULTIPLE URLS ON ONE LINE %s\n", foundCount, origLine);

    }

if (linesFixed > 0)
    ++globalFilesFixed;

// if any fixes, write the fixed file back out
if (linesFixed > 0 && makeLinksRelative)
    {
    char outName[512];
    safef(outName, sizeof outName, "%s.tmp", fullName);
    FILE *f = mustOpen(outName, "w");
    for (current = lines; current; current = current->next)
        fprintf(f,"%s\n", current->name);
    carefulClose(&f);
    rename(outName, fullName);
    }



// DEBUG REMOVE
if (optionExists("justOne"))
    {
    printf("artificially terminating early for testing.\n");
    exit(1); 
    }
}



void processDirectory(char *dir, int level)
/* Read the directory. Process sub-dirs recursively. */
{
struct slName *el=NULL, *list = listDir(dir, "*");
for (el=list; el; el=el->next)
    {
    char fullName[1024];
    safef(fullName, sizeof fullName, "%s/%s", dir, el->name);
    struct stat st;
    if (stat(fullName, &st) < 0)
	errAbort("stat failed in listDirX");
    if (S_ISDIR(st.st_mode))
	{
	if (sameString(el->name, "inc")) // skip inc/ dir
	    {
	    continue;  // do nothing
	    }
	//  DEBUG RESTORE
	if (!optionExists("justOne"))
	    {
	    printf("=================\n");
	    printf("%s/\n", fullName);
	    processDirectory(fullName, level+1);
	    }
	}
    else
	{
	if (sameString(el->name, "bigImage.html"))
	    {
	    printf("skipping bigImage.html for now\n");
	    continue;  // do nothing
	    }
	if (endsWith(el->name, ".html"))
	    {
	    processHtmlFile(dir, el->name, fullName, level);
	    }
	}
    }
}



void checkStaticHtmlLinks()
{
//char *pwd = getCurrentDir();
char *homeDir = getenv("HOME");
char rootDir[256];
safef(rootDir, sizeof rootDir, "%s/kent/src/hg/htdocs", homeDir);
printf("using the location %s\n", rootDir);
setCurrentDir(rootDir);

processDirectory(".", 0);


printf("Total files %s %d\n", fixMsg, globalFilesFixed); 
printf("Total links %s %d\n", fixMsg, globalLinksFixed); 

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if ((argc != 1) || optionExists("-help"))
    usage();
globalIncludeHash = newHash(0);
makeLinksRelative = optionExists("makeLinksRelative");
if (makeLinksRelative)
    fixMsg = "FIXED";
checkStaticHtmlLinks();
return 0;
}
