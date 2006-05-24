/* sessionDbCrawl - Go through dump of sessionDb table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: sessionDbCrawl.c,v 1.1 2006/04/28 16:32:10 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sessionDbCrawl - Go through dump of sessionDb table\n"
  "usage:\n"
  "   sessionDbCrawl input.tab output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

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
    };

void addVar(char *name, char *val, struct hash *varHash,
	struct trackVar **pList)
{
int valType = 0;
struct trackVar *tv;
if (sameString(val, "full"))
    valType = 0;
else if (sameString(val, "pack"))
    valType = 1;
else if (sameString(val, "squish"))
    valType = 2;
else if (sameString(val, "dense"))
    valType = 3;
else if (sameString(val, "hide"))
    valType = 4;
else
    return;
tv = hashFindVal(varHash, name);
if (tv == NULL)
    {
    AllocVar(tv);
    // uglyf("Adding %s\n", name);
    hashAddSaveName(varHash, name, tv, &tv->name);
    slAddHead(pList, tv);
    }
switch (valType)
    {
    case 0:
	tv->full += 1;
        break;
    case 1:
	tv->pack += 1;
        break;
    case 2:
	tv->squish += 1;
        break;
    case 3:
	tv->dense += 1;
        break;
    case 4:
	tv->hide += 1;
        break;
    }
}

int anyOn(const struct trackVar *tv)
{
return tv->full + tv->pack + tv->squish + tv->dense;
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
	struct trackVar **pList)
/* Parse list of CGI vars.  and add to varHash/list */
{
char *s = contents, *e;

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
	    addVar(name, val, varHash, pList);
	}
    s = e;
    }
    
}

void sessionDbCrawl(char *input, char *output)
/* sessionDbCrawl - Go through dump of sessionDb table. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *row[6];
struct hash *varHash = hashNew(0);
struct trackVar *tvList = NULL, *tv;
while (lineFileRowTab(lf, row))
    {
    char *contents;
    int useCount;
    contents = row[1];
    char *lastUse = row[4];
    useCount = atoi(row[5]);
    if (useCount > 1 && startsWith("2006-04", lastUse) && stringIn("db=hg17", contents))
	{
	parseContents(contents, varHash, &tvList);
	}
    }
slSort(&tvList, trackVarCmp);
for (tv = tvList; tv != NULL; tv = tv->next)
    {
    fprintf(f, "%s\t%f\t%d\t%d\t%d\t%d\t%d\n",
    	tv->name, percentOn(tv), tv->full, tv->pack, tv->squish, 
	tv->dense, tv->hide);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
sessionDbCrawl(argv[1], argv[2]);
return 0;
}
