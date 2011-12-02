/* raPriority - Set priority fields in order that they occur in trackDb.ra file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "raPriority - Set priority fields in order that they occur in trackDb.ra file\n"
  "usage:\n"
  "   raPriority in.ra out.ra\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct nameVal
/* A name/value pair. */
    {
    struct nameVal *next;	/* Next in list. */
    char *name;			/* Name - allocated in hash. */
    char *val;			/* Value - allocated here. */
    };

struct raRecord
/* A record from a .ra file. */
    {
    struct raRecord *next;	/* Next ra record. */
    struct nameVal *fieldList;	/* List of fields with values. */
    };

struct nameVal *raFind(struct raRecord *ra, char *name)
/* Return name/val of given name or NULL if it doesn't exist. */
{
struct nameVal *nv;
for (nv = ra->fieldList; nv != NULL; nv = nv->next)
    if (sameString(nv->name, name))
        return nv;
return NULL;
}

struct raRecord *raNext(struct lineFile *lf, struct hash *stringHash)
/* Return next ra record.  Returns NULL at end of file.  */
{
char *line, *key, *val;
struct raRecord *ra = NULL;
struct nameVal *nv;

for (;;)
   {
   if (!lineFileNext(lf, &line, NULL))
       break;
   line = skipLeadingSpaces(line);
   if (line[0] == '#')
       continue;
   if (line[0] == 0)
       break;
   if (ra == NULL)
       AllocVar(ra);
   key = nextWord(&line);
   AllocVar(nv);
   nv->name = hashStoreName(stringHash, key);
   val = skipLeadingSpaces(line);
   if (val == NULL)
       errAbort("Expecting line/value pair line %d of %s"
       	, lf->lineIx, lf->fileName);
   nv->val = hashStoreName(stringHash, val);
   slAddHead(&ra->fieldList, nv);
   }
if (ra != NULL)
    slReverse(&ra->fieldList);
return ra;
}

void raWrite(struct raRecord *ra, FILE *f)
/* Write out ra to file. */
{
struct nameVal *nv;
for (nv = ra->fieldList; nv != NULL; nv = nv->next)
    fprintf(f, "%s %s\n", nv->name, nv->val);
fprintf(f, "\n");
}

void requireField(struct raRecord *ra, struct lineFile *lf, char *field)
/* Squawk and die if no field of given name in ra. */
{
struct nameVal *nv = raFind(ra, field);
if (nv == NULL)
    errAbort("Missing %s in ra ending line %d of %s", field, 
    	lf->lineIx, lf->fileName);
}

void raPriority(char *inFile, char *outFile)
/* raPriority - Set priority fields in order that they occur in 
 * trackDb.ra file. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct raRecord *ra;
struct hash *stringHash = newHash(0);
struct hash *trackHash = newHash(9);
struct nameVal *nv;
int count = 0;
char priority[16];
char *track;

while ((ra = raNext(lf, stringHash)) != NULL)
    {
    requireField(ra, lf, "track");
    requireField(ra, lf, "shortLabel");
    requireField(ra, lf, "longLabel");
    requireField(ra, lf, "group");
    requireField(ra, lf, "priority");
    requireField(ra, lf, "visibility");

    track = raFind(ra, "track")->val;
    uglyf("track %s\n", track);
    if (hashLookup(trackHash, track))
        errAbort("duplicate track %s line %d of %s"
	, track, lf->lineIx, lf->fileName);
    hashAdd(trackHash, track, NULL);

    ++count;
    sprintf(priority, "%d", count);
    nv = raFind(ra, "priority");
    nv->val = priority;
    raWrite(ra, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
raPriority(argv[1], argv[2]);
return 0;
}
