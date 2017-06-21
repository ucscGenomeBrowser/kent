
/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "axt.h"
#include "common.h"
#include "bigWig.h"
#include "bigBed.h"
#include "dystring.h"
#include "errCatch.h"
#include "hgBam.h"
#include "htmshell.h"
#include "htmlPage.h"
#include "hui.h"
#include "net.h"
#include "options.h"
#include "trackDb.h"
#include "trackHub.h"
#include "udc.h"
#include "vcf.h"
#include "hubSearchText.h"

static int cacheTime = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubCrawl - Crawl a track data hub and output search strings\n"
  "usage:\n"
  "   hubCrawl http://yourHost/yourDir/hub.txt\n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for hub, genome, and trackDb text files.\n"
  "   -cacheTime=N - set cache refresh time in seconds, default %d\n"
  "   -verbose=2            - output verbosely\n"
  , cacheTime
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {"cacheTime", OPTION_INT},
   {NULL, 0},
};


char *stripJavascriptCssAndHtmlTags(char *s)
/* Scrub text from a description page to remove all html tags, inline javascript,
 * and css. */
{
if (s == NULL)
    return NULL;
char *scrubbed = needMem(strlen(s));
char *from=s;
char *to=scrubbed;
while (*from!='\0')
    {
    if (startsWithNoCase("<script", from))
        {
        from++;
        while (*from!='\0' && !startsWithNoCase("</script>", from))
            from++;
        if (*from == '\0')  // The last open tag was never closed!
            break;
        from += strlen("</script>");
        *to++ = ' ';
        }
    else if (startsWithNoCase("<style", from))
        {
        from++;
        while (*from!='\0' && !startsWithNoCase("</style>", from))
            from++;
        if (*from == '\0')  // The last open tag was never closed!
            break;
        from += strlen("</style>");
        *to++ = ' ';
        }
    else if (*from == '<')
        {
        from++;
        while (*from!='\0' && *from != '>')
            from++;
        if (*from == '\0')  // The last open tag was never closed!
            break;
        from++;
        *to++ = ' ';
        }
    else
        *to++ = *from++;
    }
return scrubbed;
}


char *removeExtraSpaces(char *s)
/* Replace long spans of whitespace in s with just the first whitespace
 * character in each span. */
{
if (s == NULL)
    return NULL;
char *scrubbed = needMem(strlen(s));
char *from=s;
char *to=scrubbed;
while (*from!='\0')
    {
    if (isspace(*from) && (*(from+1) == '\0' || isspace(*(from+1))))
        from++;
    else
        *to++ = *from++;
    }
return scrubbed;
}


char *cleanHubHtml(char *html)
/* Clean up an HTML description page by removing all tags, javascript, and css,
 * and deleting a number of awkward special characters. */
{
if (isEmpty(html))
    return NULL;
char *stripHtml = stripJavascriptCssAndHtmlTags(html);
strSwapChar(stripHtml, '\n', ' ');
strSwapChar(stripHtml, '\t', ' ');
strSwapChar(stripHtml, '\015', ' ');
strSwapChar(stripHtml, ')', ' ');
strSwapChar(stripHtml, '(', ' ');
strSwapChar(stripHtml, '[', ' ');
strSwapChar(stripHtml, ']', ' ');
char *withoutExtraSpaces = removeExtraSpaces(stripHtml);
return withoutExtraSpaces;
}


void trackHubCrawlTrack(struct trackDb *tdb, struct trackHubGenome *genome, char *hubUrl, char *dbName, FILE *searchFp)
/* Given a trackDb and the hub genome it comes from, write out hubSearchText lines for all of
 * the tracks in that trackDb */
{
struct hubSearchText *trackHst = NULL;
AllocVar(trackHst);
trackHst->hubUrl = cloneString(hubUrl);
trackHst->db = cloneString(dbName);
trackHst->track = cloneString(trackHubSkipHubName(tdb->track));
if (isNotEmpty(tdb->longLabel))
    {
    trackHst->label = cloneString(tdb->longLabel);
    }
else if (isNotEmpty(tdb->shortLabel))
    {
    trackHst->label = cloneString(tdb->shortLabel);
    }
else
    trackHst->label = cloneString(trackHubSkipHubName(tdb->track));

trackHst->textLength = hubSearchTextShort;
trackHst->text = cloneString(trackHubSkipHubName(tdb->track));
hubSearchTextTabOut(trackHst, searchFp);

if (isNotEmpty(tdb->shortLabel))
    {
    trackHst->text = cloneString(tdb->shortLabel);
    hubSearchTextTabOut(trackHst, searchFp);
    }
if (isNotEmpty(tdb->longLabel))
    {
    trackHst->text = cloneString(tdb->longLabel);
    hubSearchTextTabOut(trackHst, searchFp);
    }

trackHubAddOneDescription(genome->trackDbFile, tdb);
if (isNotEmpty(tdb->html))
    {
    trackHst->textLength = hubSearchTextLong;
    trackHst->text = cleanHubHtml(tdb->html);
    hubSearchTextTabOut(trackHst, searchFp);
    }
}


void trackHubCrawlGenome(struct trackHubGenome *genome, struct trackHub *hub, FILE *searchFp)
/* Given a hub genome and the hub it came from, write out hubSearchText lines for that genome */
{
struct hubSearchText *genomeHst = NULL;
AllocVar(genomeHst);
genomeHst->hubUrl = cloneString(hub->url);
genomeHst->db = cloneString(trackHubSkipHubName(genome->name));
genomeHst->track = cloneString("");
char label[256];
if (isNotEmpty(genome->description))
    safef(label, sizeof(label), "%s (%s)", genome->description, trackHubSkipHubName(genome->name));
else if (isNotEmpty(genome->organism))
    safef(label, sizeof(label), "%s (%s)", trackHubSkipHubName(genome->organism), trackHubSkipHubName(genome->name));
else
    safef(label, sizeof(label), "%s", trackHubSkipHubName(genome->name));
genomeHst->label = cloneString(label);
genomeHst->textLength = hubSearchTextShort;
genomeHst->text = cloneString(trackHubSkipHubName(genome->name));
hubSearchTextTabOut(genomeHst, searchFp);

if (isNotEmpty(genome->organism) && differentString(genome->organism, genome->name))
    {
    genomeHst->text = cloneString(trackHubSkipHubName(genome->organism));
    hubSearchTextTabOut(genomeHst, searchFp);
    }
if (isNotEmpty(genome->description))
    {
    genomeHst->text = cloneString(genome->description);
    hubSearchTextTabOut(genomeHst, searchFp);
    }
struct hashEl *hel = NULL;
if (genome->settingsHash && (hel = hashLookup(genome->settingsHash, "scientificName")) != NULL)
    {
    char *sciName = (char *)(hel->val);
    if (differentString(sciName, genome->name))
        {
        genomeHst->text = cloneString(sciName);
        hubSearchTextTabOut(genomeHst, searchFp);
        }
    }
if (genome->settingsHash && (hel = hashLookup(genome->settingsHash, "htmlPath")) != NULL)
    {
    char *htmlPath = (char *)(hel->val);
    genomeHst->textLength = hubSearchTextLong;
    char *rawHtml = netReadTextFileIfExists(htmlPath);
    genomeHst->text = cleanHubHtml(rawHtml);
    if (isNotEmpty(genomeHst->text))
        hubSearchTextTabOut(genomeHst, searchFp);
    }

/* Write out trackDb search text */
struct trackDb *tdbList = trackHubTracksForGenome(hub, genome), *tdb = NULL;
trackHubPolishTrackNames(hub, tdbList);
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    trackHubCrawlTrack(tdb, genome, hub->url, genomeHst->db, searchFp);
    }
}


int trackHubCrawl(char *hubUrl)
/* Crawl a track data hub and output strings useful in a search */
{
struct errCatch *errCatch = errCatchNew();
struct trackHub *hub = NULL;
int retVal = 0;

if (errCatchStart(errCatch))
    {
    hub = trackHubOpen(hubUrl, "hub_0");
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    retVal = 1;
    fprintf(stderr, "%s\n", errCatch->message->string);
    }
errCatchFree(&errCatch);

if (hub == NULL)
    return 1;

FILE *searchFp =stdout;
struct hubSearchText *hubHst;
AllocVar(hubHst);

hubHst->hubUrl = cloneString(hub->url);
hubHst->db = cloneString("");
hubHst->track = cloneString("");
hubHst->label = cloneString("");
hubHst->textLength = hubSearchTextShort;
hubHst->text = cloneString(hub->shortLabel);
hubSearchTextTabOut(hubHst, searchFp);

hubHst->text = cloneString(hub->longLabel);
hubSearchTextTabOut(hubHst, searchFp);

if (hub->descriptionUrl != NULL)
    {
    hubHst->textLength = hubSearchTextLong;
    char *rawHtml = netReadTextFileIfExists(hub->descriptionUrl);
    hubHst->text = cleanHubHtml(rawHtml);
    if (isNotEmpty(hubHst->text))
        hubSearchTextTabOut(hubHst, searchFp);
    }

struct trackHubGenome *genome;
for (genome = hub->genomeList; genome != NULL; genome = genome->next)
    trackHubCrawlGenome(genome, hub, searchFp);

trackHubClose(&hub);
return retVal;
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc != 2)
    usage();


udcSetCacheTimeout(cacheTime);
// UDC cache dir: first check for hg.conf setting, then override with command line option if given.
setUdcCacheDir();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

if (trackHubCrawl(argv[1]))
    {
    return 1;
    }
return 0;
}
