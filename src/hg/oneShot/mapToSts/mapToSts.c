/* mapToSts - convert from WashU map file to an STS file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"


void usage()
/* Explain usage and exit */
{
errAbort(
"mapToSts - convert from WashU map to STS file\n"
"usage:\n"
"   mapToSts map.wu out.sts");
}

boolean isAcc(char *s)
/* Return TRUE if s looks to be an accession name. */
{
int len = strlen(s);
if (len < 6 || len > 8)
    return FALSE;
if (!isupper(s[0]))
    return FALSE;
if (!isdigit(s[len-1]))
    return FALSE;
return TRUE;
}

struct cloneSts
/* A clone with an STS position. */
    {
    struct cloneSts *next;	/* Next in list. */
    char *cloneName;
    char *chromosome;
    float stsPos;
    };

int cmpCloneSts(const void *va, const void *vb)
/* Compare two cloneSts. */
{
const struct cloneSts *a = *((struct cloneSts **)va);
const struct cloneSts *b = *((struct cloneSts **)vb);
int dif;
dif = strcmp(a->chromosome, b->chromosome);
if (dif == 0)
    {
    if (a->stsPos - b->stsPos < 0.0)
	dif = -1;
    else
	dif = 1;
    }
return dif;
}

void mapToSts(char *inName, char *outName)
/* mapToSts - convert from WashU map file to an STS file. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
int lineSize, wordCount;
char *line, *words[32];
struct hash *cloneHash = newHash(16);
struct hash *chromHash = newHash(6);
struct hashEl *hel;
char *acc;
struct cloneSts *cs, *csList = NULL;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount == 11)
	{
	acc = words[0];
	if (isAcc(acc))
	    {
	    if ((hel = hashLookup(cloneHash, acc)) == NULL)
		{
		AllocVar(cs);
		hel = hashAdd(cloneHash, acc, cs);
		cs->cloneName = hel->name;
		cs->chromosome = hashStoreName(chromHash, words[9]);
		cs->stsPos = atof(words[10]);
		slAddHead(&csList, cs);
		}
	    else
		{
		warn("Duplicate %s", acc);
		}
	    }
	}
    }
lineFileClose(&lf);
slSort(&csList, cmpCloneSts);
for (cs = csList; cs != NULL; cs = cs->next)
    {
    fprintf(f, "%s\t%s\t%0.2f\n", cs->cloneName, cs->chromosome, cs->stsPos);
    }
fclose(f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
mapToSts(argv[1], argv[2]);
return 0;
}
