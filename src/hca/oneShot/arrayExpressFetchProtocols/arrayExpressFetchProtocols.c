/* arrayExpressFetchProtocols - Fetch protocol files for a given array express accession.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "htmlPage.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "arrayExpressFetchProtocols - Fetch protocol files for a given array express accession.\n"
  "usage:\n"
  "   arrayExpressFetchProtocols accession outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

boolean nextProtocolHref(char **pos, char **retNumId, char **retProtocol)
/* Parse through string starting at *pos looking for a protocol href.  
 * Return FALSE if not found, otherwise update *pos with where to look for
 * next one and *retNumId and *retProtocol for what is parsed out. */
{
char *numId = nextStringBetween("href=\"/arrayexpress/protocols/", "/?ref=", pos);
if (numId == NULL)
    return FALSE;
char *protocol = nextStringBetween("\">", "</a>", pos);
if (protocol == NULL)
    return FALSE;
*retProtocol = protocol;
*retNumId = numId;
return TRUE;
}

void arrayExpressFetchProtocols(char *accession, char *outDir)
/* arrayExpressFetchProtocols - Fetch protocol files for a given array express accession.. */
{
char rootUrl[1024];
safef(rootUrl, sizeof(rootUrl), "http://www.ebi.ac.uk/arrayexpress/experiments/%s/protocols/",
    accession);
verbose(1, "fetching %s\n", rootUrl);
struct htmlPage *rootPage = htmlPageGet(rootUrl);
verbose(2, "%s has %d chars of html\n", rootUrl, (int)strlen(rootPage->htmlText));

makeDirsOnPath(outDir);

char *pos = rootPage->htmlText;
char *numId, *protocol;
while (nextProtocolHref(&pos, &numId, &protocol))
     {
     uglyf("%s %s\n", numId, protocol);
     char protocolFile[PATH_LEN];
     safef(protocolFile, sizeof(protocolFile), "%s/%s", outDir, protocol);
     char protocolUrl[1024];
     safef(protocolUrl, sizeof(protocolUrl), 
	"http://www.ebi.ac.uk/arrayexpress/protocols/%s/", numId);
     verbose(1, "fetching %s\n", protocolUrl);
     struct htmlPage *page = htmlPageGet(protocolUrl);
     uglyf("%s has %d chars of html\n", protocolUrl, (int)strlen(page->htmlText));
     char *description = stringBetween("<div>Description</div></td><td class=\"value\"><div>",
				       "</div>", page->htmlText);
     htmlPageFree(&page);
     FILE *f = mustOpen(protocolFile, "w");
     fputs(description, f);
     carefulClose(&f);
     }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
arrayExpressFetchProtocols(argv[1], argv[2]);
return 0;
}
