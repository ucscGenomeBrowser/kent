/* encodeUserDbCrawl - Mine user DB for ENCODE info.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodeUserDbCrawl - Mine user DB for ENCODE info.\n"
  "usage:\n"
  "   encodeUserDbCrawl input.tab output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

enum trackVis
/* How to look at a track. */
    {
    tvHide=0, 		/* Hide it. */
    tvDense=1,          /* Squish it together. */
    tvFull=2,           /* Expand it out. */
    tvPack=3,           /* Zig zag it up and down. */
    tvSquish=4,         /* Pack with thin boxes and no labels. */
    tvShow=5,		/* Supertrack on */
    };

static struct optionSpec options[] = {
   {NULL, 0},
};

struct trackVar
/* Stats on a track. */
    {
    struct trackVar *next;
    char *name;
    int full;
    int pack;
    int squish;
    int dense;
    int hide;
    int show;
    };

boolean addVisibilityVar(char *name, char *val, struct hash *varHash,
	struct trackVar **pList, enum trackVis *retVis)
/* Given variable of name with val, add stats on it it to hash if it looks like the
 * val is a visibility. Return TRUE if this is the case, else FALSE. */
{
enum trackVis valType = tvHide;
struct trackVar *tv;
if (sameString(val, "full"))
    valType = tvFull;
else if (sameString(val, "pack"))
    valType = tvPack;
else if (sameString(val, "squish"))
    valType = tvSquish;
else if (sameString(val, "dense"))
    valType = tvDense;
else if (sameString(val, "hide"))
    valType = tvHide;
else if (sameString(val, "show"))
    valType = tvShow;
else
    return FALSE;
tv = hashFindVal(varHash, name);
if (tv == NULL)
    {
    AllocVar(tv);
    hashAddSaveName(varHash, name, tv, &tv->name);
    slAddHead(pList, tv);
    }
switch (valType)
    {
    case tvFull:
	tv->full += 1;
        break;
    case tvPack:
	tv->pack += 1;
        break;
    case tvSquish:
	tv->squish += 1;
        break;
    case tvDense:
	tv->dense += 1;
        break;
    case tvHide:
	tv->hide += 1;
        break;
    case tvShow:
    	tv->show += 1;
	break;
    }
*retVis = valType;
return TRUE;
}

int anyOn(const struct trackVar *tv)
{
return tv->full + tv->pack + tv->squish + tv->dense + tv->show;
}

double percentOn(const struct trackVar *tv)
/* Return percentage of time track is on. */
{
long on = anyOn(tv);
long all = on + tv->hide;
return 100.0 * on / all;
}

int trackVarCmp(const void *va, const void *vb)
/* Compare to sort based on percent on . */
{
const struct trackVar *a = *((struct trackVar **)va);
const struct trackVar *b = *((struct trackVar **)vb);
double diff = anyOn(b) - anyOn(a);
if (diff < 0)
    return -1;
else if (diff > 0)
    return 1;
else
    return 0;
}


void parseContents(char *contents, struct hash *varHash, 
	struct trackVar **pList, boolean *retAnyTrack, boolean *retIsEncode)
/* Parse list of CGI vars.  and add to varHash/list.  */
{
char *s = contents, *e;
boolean isEncode = FALSE;
boolean anyTrack = FALSE;

while (s != NULL && s[0] != 0)
    {
    char *name, *val;
    e = strchr(s, '&');
    if (e != 0)
        *e++ = 0;
    name = s;
    val = strchr(s, '=');
    if (val != NULL)
	{
        *val++ = 0;
	if (!startsWith("ct_", name))
	    {
	    enum trackVis vis;
	    int isTrack = addVisibilityVar(name, val, varHash, pList, &vis);
	    if (isTrack)
	        anyTrack = TRUE;
	    if (isTrack && startsWith("wgEncode", name) && vis != tvHide)
	        isEncode = TRUE;
	    }
	}
    s = e;
    }
*retAnyTrack = anyTrack;
*retIsEncode = isEncode;
}

void encodeUserDbCrawl(char *input, char *output)
/* encodeUserDbCrawl - Mine user DB for ENCODE info.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *row[6];
struct hash *varHash = hashNew(0);
struct trackVar *tvList = NULL, *tv;
int totalCount = 0;
int wgEncodeCount = 0;
while (lineFileRowTab(lf, row))
    {
    char *contents;
    contents = row[1];
    int useCount;
    char *lastUse = row[4];
    useCount = atoi(row[5]);
    if (useCount > 1 && startsWith("2011-1", lastUse) && (stringIn("db=hg18", contents) || stringIn("db=hg19", contents)))
	{
	boolean anyTrack, isEncode;
	parseContents(contents, varHash, &tvList, &anyTrack, &isEncode);
	if (isEncode)
	    wgEncodeCount++;
	if (anyTrack)
	    ++totalCount;
	}
    }
slSort(&tvList, trackVarCmp);
for (tv = tvList; tv != NULL; tv = tv->next)
    {
    fprintf(f, "%s\t%f\t%d\t%d\t%d\t%d\t%d\t%d\n",
    	tv->name, percentOn(tv), tv->full, tv->pack, tv->squish, 
	tv->dense, tv->show, tv->hide);
    }
printf("wgEncode in %d of %d\n", wgEncodeCount, totalCount);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
encodeUserDbCrawl(argv[1], argv[2]);
return 0;
}
