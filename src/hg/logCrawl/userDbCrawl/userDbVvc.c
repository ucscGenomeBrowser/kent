/* userDbToVvc - Analyze tab-delimited dump of userDb (or sessionDb) table.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "userDbVvc - Analyze tab-delimited dump of userDb (or sessionDb) table and output.\n"
  "list of variable, values, counts\n"
  "usage:\n"
  "   userDbVvc input.tab output\n"
  "options:\n"
  "   -year=20NN\n"
  );
}

char *clYear = NULL;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"year", OPTION_STRING},
   {NULL, 0},
};

struct varValCount
/* Stats on a track. */
    {
    struct varValCount *next;
    char *var;
    char *val;
    int count;
    };

void addVar(char *var, char *val, struct hash *varHash,
	struct varValCount **pList)
/* Add var/val to hash */
{
/* Get hash of values associated with variable.  If first time
 * we've seen variable, make hash.  Do a little technical stuff
 * to get permanent name for variable without having to clone it. */
struct hashEl *varHel = hashLookup(varHash, var);
char *varSavedName = NULL;
struct hash *valHash = NULL;
if (varHel == NULL)
    {
    valHash = hashNew(4);
    hashAddSaveName(varHash, var, valHash, &varSavedName);
    }
else
    {
    varSavedName = varHel->name;
    valHash = varHel->val;
    }

/* Get/make value record, and bump up count. */
struct varValCount *vvc = hashFindVal(valHash, val);
if (vvc == NULL)
    {
    AllocVar(vvc);
    vvc->var = varSavedName;
    hashAddSaveName(valHash, val, vvc, &vvc->val);
    slAddHead(pList, vvc);
    }
vvc->count += 1;
}

int varValCountCmp(const void *va, const void *vb)
/* Compare to sort based on percent on . */
{
const struct varValCount *a = *((struct varValCount **)va);
const struct varValCount *b = *((struct varValCount **)vb);
int diff = strcmp(a->var, b->var);
if (diff == 0)
    diff = strcmp(a->val, b->val);
return diff;
}


void parseContents(char *contents, struct hash *varHash, 
	struct varValCount **pList)
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
	char firstChar = name[0];
	if (firstChar == '_' || isalnum(firstChar))
	    if (!startsWith("ct_", name))
		addVar(name, val, varHash, pList);
	}
    s = e;
    }
    
}


void userDbVvc(char *input, char *output)
/* userDbVvc - Analyze tab-delimited dump of userDb (or sessionDb) table.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *row[7];
struct hash *varHash = hashNew(0);
struct varValCount *vvcList = NULL, *vvc;
while (lineFileRowTab(lf, row))
    {
    char *contents = row[1];
    char *lastUse = row[4];
    if (clYear == NULL || startsWith(clYear, lastUse))
	{
	int useCount;
	useCount = atoi(row[5]);
	if (useCount > 1)
	    {
	    parseContents(contents, varHash, &vvcList);
	    }
	}
    }
slSort(&vvcList, varValCountCmp);
for (vvc = vvcList; vvc != NULL; vvc = vvc->next)
    {
    if (vvc->count > 4 || isSymbolString(vvc->var))
	fprintf(f, "%s\t%s\t%d\n", vvc->var, vvc->val, vvc->count);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clYear = optionVal("year", NULL);
userDbVvc(argv[1], argv[2]);
return 0;
}
