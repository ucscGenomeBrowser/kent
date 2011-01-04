/* paraSync - uses paraFetch to recursively mirror url to given path. */
#include "common.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "portable.h"
#include "net.h"

void usage()
/* Explain usage and exit */
{
errAbort(
    "paraSync 1.0\n"
    "paraSync - uses paraFetch to recursively mirror url to given path\n"
    "usage:\n"
    "   paraSync {options} N R URL outPath\n"
    "   where N is the number of connections to use\n"
    "         R is the number of retries\n"
    "   Options:\n"
    "    -A='ext1,ext2'  means accept only files with ext1 or ext2\n" 
    );
}

static struct optionSpec options[] = {
   {"A", OPTION_STRING},
   {NULL, 0},
};

char *acceptString = NULL;
char **acceptExtensions = NULL; 
int acceptExtensionsCount = 0;

boolean paraSync(int numConnections, int numRetries, struct dyString *url, struct dyString *outPath)
/* Fetch given URL, send to stdout. */
{
// requirements:
//   URL must end in / slash
//   foo must end in / slash
if (!endsWith(url->string,"/"))
    errAbort("URL must end in slash /");
if (!endsWith(outPath->string,"/"))
    errAbort("outPath must end in slash /");
// create subdir if it does not exist
makeDir(outPath->string);
struct dyString *dy = netSlurpUrl(url->string);
char *p = dy->string;

verbose(2,"response=[%s]\n", dy->string);

boolean result = TRUE;

char *pattern = "<a href=\"";
while (TRUE)
    {
    char *q = strstr(p,pattern);
    if (!q)
	break;
    q += strlen(pattern);
    p = strchr(q,'"');
    if (!p)
	errAbort("unmatched \" in URL");
    *p = 0;
    ++p; // get past the terminator that we added earlier.   

    // We want to skip several kinds of links
    if (q[0] == '?') continue;
    if (q[0] == '/') continue;
    if (startsWith(q, "ftp:")) continue;
    if (startsWith(q, "http:")) continue;
    if (startsWith(q, "https:")) continue;
    if (startsWith(q, "./")) continue;
    if (startsWith(q, "../")) continue;

    verbose(1, "%s\n", q);

    int saveUrlSize = url->stringSize;
    int saveOutPathSize = outPath->stringSize;

    dyStringAppend(url, q);
    dyStringAppend(outPath, q);
 
    // URL found
    if (endsWith(q, "/")) // directory
	{   
	// recursive
	if (!paraSync(numConnections, numRetries, url, outPath))
	    result = FALSE;
	}
    else    // file
	{
	// Test accepted extensions if applicable.
        boolean accepted = (acceptExtensionsCount == 0);
        int i = 0;
        for(i=0; i<acceptExtensionsCount; ++i) 
	    {
	    if (endsWith(q, acceptExtensions[i]))
		{
		accepted = TRUE;
		break;
		}
	    }
	if (accepted)
	    {
	    // check to see if it needs download, i.e. file does not exist, or it needs a resume
	    int restoreSize = outPath->stringSize;
	    dyStringAppend(outPath, ".paraFetchStatus");
	    boolean needsDownload = fileExists(outPath->string);
	    dyStringResize(outPath, restoreSize);
	    if (!fileExists(outPath->string))
		needsDownload = TRUE;
	    if (needsDownload)
		{
		if (!parallelFetch(url->string, outPath->string, numConnections, numRetries))
		    {
		    warn("failed to download %s\n", url->string);
		    // write to a log that this one failed
		    // and try to continue
		    result = FALSE;
		    }
		else
		    {
		    verbose(1,"%s downloaded successfully\n", url->string);
		    }
		}
	    }
	}
    
    dyStringResize(url, saveUrlSize);
    dyStringResize(outPath, saveOutPathSize);

    }


return result;

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
acceptString = optionVal("A", NULL);
if (acceptString)
    {
    acceptExtensionsCount = chopByChar(acceptString, ',', NULL, 0);
    AllocArray(acceptExtensions, acceptExtensionsCount);
    chopByChar(acceptString, ',', acceptExtensions, acceptExtensionsCount);
    verbose(1, "accept-option count: %d\n", acceptExtensionsCount);
    int i = 0;
    for(i=0; i<acceptExtensionsCount; ++i) 
	{
	verbose(2, "accept-option: %s\n", acceptExtensions[i]);
	}
    }
struct dyString *url = dyStringNew(4096);
struct dyString *outPath = dyStringNew(4096);
dyStringAppend(url, argv[3]);
dyStringAppend(outPath, argv[4]);
if (!paraSync(atoi(argv[1]), atoi(argv[2]), url, outPath))
    exit(1);
return 0;
}

