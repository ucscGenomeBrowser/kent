/* ga4ghToBed - Use GA4GH API to extract read data for a genomic region and save it as a BED file */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "net.h"
#include "htmlPage.h"
#include "jsonWrite.h"
#include "jsonParse.h"
#include "sqlNum.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ga4ghToBed - Use GA4GH API to extract read data for a genomic region and save it as a BED file\n"
  "usage:\n"
  "   ga4ghToBed url key chrom start end output.bed\n"
  "where url is typically something like https://www.googleapis.com/genomics/v1beta/reads/search\n"
  "and key is something you might get from doll@google.com\n"
  "and chrom is the chromosome number (no 'chr')\n"
  "and start/end are zero-based half open coordinates on the chromosome\n"
  "and output.bed is the simple three column bed output\n"
  "options:\n"
  "   -verbose=N set verbose=2 or higher for more debugging output\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct htmlPage *postJson(char *url, struct jsonWrite *jw)
/* Post json to a URL and return response */
{
struct htmlPage *newPage = NULL;
struct dyString *dyHeader = dyStringNew(0);
struct dyString *dyText = NULL;

/* Create header */
int contentLength = jw->dy->stringSize;
dyStringPrintf(dyHeader, "Content-Length: %d\r\n", contentLength);
dyStringPrintf(dyHeader, "Content-Type: application/json\r\n");

/* Post header and json and get response */
verboseTimeInit();
int sd = netOpenHttpExt(url, "POST", dyHeader->string);
mustWriteFd(sd, jw->dy->string, contentLength);
dyText = netSlurpFile(sd);
verboseTime(1, "milleseconds to get response");
close(sd);

/* Parse lightly response (just into header and rest), clean up and return response */
newPage = htmlPageParse(url, dyStringCannibalize(&dyText));
dyStringFree(&dyHeader);
return newPage;
}

struct jsonWrite *readSearchJson(char **setIds, int setCount, char *chrom, int start, int end)
/* Return a jsonWrite object set up to make the search reads in region query. */
{
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
jsonWriteListStart(jw, "readsetIds");
int i;
for (i=0; i<setCount; ++i)
    jsonWriteString(jw, NULL, setIds[i]);
jsonWriteListEnd(jw);
jsonWriteString(jw, "sequenceName", chrom);
jsonWriteNumber(jw, "sequenceStart", start);
jsonWriteNumber(jw, "sequenceEnd", end);
jsonWriteObjectEnd(jw);
return jw;
}

int cigarSize(char *cigar)
/* Return number of bases in target genome covered by cigar string */
{
int size = 0;
char c;
for (;;)
    {
    char *keyPt = skipNumeric(cigar);
    int count = atoi(cigar);
    c = *keyPt++;
    if (c == 0)
        break;
    switch (c)
        {
	case 'M':
	case 'N':
	case 'D':
	case '=':
	    size += count;
	    break;
	case 'S':
	case 'H':
	case 'I':
	case 'P':
	    break;
	default:
	    errAbort("Unrecognized %c in CIGAR string", c);
	}
    cigar = keyPt;
    }
return size;
}

void outputJsonToBed(struct jsonElement *root, char *chrom, char *fileName)
/* Rummage through GA4GH 0.1 read search result and make it into a
 * simple bed file. */
{
FILE *f = mustOpen(fileName, "w");
struct jsonElement *reads = jsonFindNamedField(root, "root", "reads");
assert(reads->type == jsonList);
struct slRef *list = reads->val.jeList, *el;
for (el = list; el != NULL; el = el->next)
    {
    struct jsonElement *one = el->val;
    assert(one->type == jsonObject);
    struct hash *hash = one->val.jeHash;
    struct jsonElement *position = hashMustFindVal(hash, "position");
    assert(position->type == jsonNumber);
    long pos = position->val.jeNumber;
    struct jsonElement *cigar = hashMustFindVal(hash, "cigar");
    long size = cigarSize(cigar->val.jeString);
    assert(cigar->type == jsonString);
    fprintf(f, "chr%s\t%ld\t%ld\n", chrom, pos-1, pos+size);
    }

carefulClose(&f);
}

/* NA12787 read set ID */
char *setIds[] = {"CJDmkYn8ChDPtMO55665i4kB"};

void ga4ghToBed(char *url, char *key, char *chrom, char *startString, char *endString, 
    char *outFile)
/* ga4ghToBed - Use GA4GH API to extract variant data for a genomic region and save it as a BED file.. */
{
int start = sqlUnsigned(startString) + 1;
int end = sqlUnsigned(endString);
struct jsonWrite *jw = readSearchJson(setIds, ArraySize(setIds), chrom, start, end);
verbose(2, "Json is %s\n\n", jw->dy->string);
struct dyString *fullUrl = dyStringNew(0);
dyStringPrintf(fullUrl, "%s?key=%s&fields=reads(cigar,position)", url, key);
struct htmlPage *page = postJson(fullUrl->string, jw);
struct htmlStatus *st = page->status;
verbose(2, "st->status = %d\n", st->status);
verbose(2, "%s\n", page->fullText);
if (st->status != 200)
    errAbort("Status %d error", st->status);
struct jsonElement *root = jsonParse(page->htmlText);
outputJsonToBed(root, chrom, outFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
ga4ghToBed(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
