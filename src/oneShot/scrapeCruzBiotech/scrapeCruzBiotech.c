/* scrapeCruzBiotech - Do some screen scraping of Santa Cruz biotech site looking for antibodies with immunofluorescence.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmlPage.h"

static char const rcsid[] = "$Id: scrapeCruzBiotech.c,v 1.1 2006/06/15 15:27:00 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "scrapeCruzBiotech - Do some screen scraping of Santa Cruz biotech site looking for antibodies with immunofluorescence.\n"
  "usage:\n"
  "   scrapeCruzBiotech startUrl destFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

char *catBase = "http://www.scbt.com/catalog/";

void weedString(char *weed, char *s)
{
s = stringIn(weed, s);
if (s != NULL)
    {
    int i, len = strlen(weed);
    for (i=0; i<len; ++i)
         s[i] = 'X';
    }
}

void scrapeProduct(char *url, int id, FILE *f)
/* Scrape one product */
{
struct htmlPage *page = htmlPageGet(url);
weedString("Multicolor FCM Systems", page->htmlText);
boolean gotFcm = (stringIn("FCM", page->htmlText) != NULL);
boolean gotIhc = (stringIn("IHC", page->htmlText) != NULL);
char *scIdStart = stringIn("sc-", page->htmlText);
char *fcmStart = stringIn("FCM", page->htmlText);
char *ihcStart = stringIn("IHC", page->htmlText);
char *type = "other";

if (stringIn(">Transcription Regulators<", page->htmlText))
    type = "txn";
else if (stringIn(">Homeodomain Proteins<", page->htmlText))
    type = "hox";
if (scIdStart != NULL)
    {
    char *scId = nextWord(&scIdStart);
    fprintf(f, "%s\tsc-%d\t%s\t%d\t%d\n", scId, id, type, gotFcm, gotIhc);
    uglyf("%s\tsc-%d\t%s\t%d\t%d\n", scId, id, type, gotFcm, gotIhc);
    }
htmlPageFree(&page);
}

void scrapeCruzBiotech(char *outFile)
/* scrapeCruzBiotech - Do some screen scraping of Santa Cruz biotech site looking for antibodies with immunofluorescence.. */
{
FILE *f = mustOpen(outFile, "w");
char url[1024];
int id, minId = 1, maxId=46000;

for (id=minId; id<=maxId; ++id)
    {
    safef(url, sizeof(url), 
       "http://www.scbt.com/catalog/detail.lasso?-token.order_id=&-database=catalog&-layout=web_detail&-Op=eq&catalog_number=sc-%d&-search", id);
    scrapeProduct(url, id, f);
    uglyf("Done %d of %d\n", id, maxId);
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
scrapeCruzBiotech(argv[1]);
return 0;
}
