/* gffPeek - Look at a gff file and report some basic stats. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gffPeek - Look at a gff file and report some basic stats\n"
  "usage:\n"
  "   gffPeek file.gff\n"
  "file can be stdin.\n"
  "options:\n"
  "   -seq - include seq in output\n"
  );
}

static struct optionSpec options[] = {
   {"seq", OPTION_BOOLEAN},
   {NULL, 0},
};

struct countedName
/* A name and a count. */
    {
    struct countedName *next;	/* Next in list. */
    char *name;		/* Name of item - allocated in hash. */
    int count;		/* Number of times seen. */
    };

int countedNameCmp(const void *va, const void *vb)
/* Compare to sort based on name. */
{
const struct countedName *a = *((struct countedName **)va);
const struct countedName *b = *((struct countedName **)vb);
return strcmp(a->name, b->name);
}

struct countedHash
/* A hash of counted names. */
    {
    struct hash *hash;
    char *name;			/* Name of this type */
    struct countedName *list;
    };

struct countedHash *countedHashNew(char *name, int hashSize)
/* Make a new counted hash of given name, and given power of two
 * size (zero for default) */
{
struct countedHash *ch;
AllocVar(ch);
ch->name = cloneString(name);
ch->hash = newHash(hashSize);
return ch;
}

void countedHashAdd(struct countedHash *ch, char *name)
/* Increment count associated with name. */
{
struct countedName *cn = hashFindVal(ch->hash, name);
if (cn == NULL)
    {
    AllocVar(cn);
    hashAddSaveName(ch->hash, name, cn, &cn->name);
    slAddHead(&ch->list, cn);
    }
cn->count += 1;
}

void countedHashDump(struct countedHash *ch, FILE *f)
/* Dump out contents of counted hash. */
{
struct countedName *cn;
slSort(&ch->list, countedNameCmp);
fprintf(f, "%s:\n", ch->name);
for (cn=ch->list; cn != NULL; cn = cn->next)
    fprintf(f, "  %s\t%d\n", cn->name, cn->count);
}

void gffPeek(char *fileName)
/* gffPeek - Look at a gff file and report some basic stats. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
char *row[9];
int commentCount = 0;
int badCount = 0;
int wordCount;
struct countedHash *seqHash = countedHashNew("seq", 16);
struct countedHash *sourceHash = countedHashNew("source", 16);
struct countedHash *featureHash = countedHashNew("feature", 16);
struct countedHash *strandHash = countedHashNew("strand", 8);
struct countedHash *frameHash = countedHashNew("frame", 8);
// boolean complexGroup = FALSE;  unused
// char *group;  unused

while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    if (line[0] == '#')
        {
	++commentCount;
	continue;
	}
    wordCount = 0;
    for (wordCount=0; wordCount<8; ++wordCount)
        {
	word = nextWord(&line);
	if (word == NULL)
	    break;
	row[wordCount] = word;
	}
    if (wordCount < 8)
        {
	++badCount;
	continue;
	}
    line = skipLeadingSpaces(line);
/* unused code
    group = NULL;
    if (line != NULL && line[0] != 0)
        {
	group = line;
	if (strchr(group, '=') != 0)
	    complexGroup = TRUE;
	}
*/
    countedHashAdd(seqHash, row[0]);
    countedHashAdd(sourceHash, row[1]);
    countedHashAdd(featureHash, row[2]);
    countedHashAdd(strandHash, row[6]);
    countedHashAdd(frameHash, row[7]);
    }
if (optionExists("seq"))
    countedHashDump(seqHash, stdout);
countedHashDump(sourceHash, stdout);
countedHashDump(featureHash, stdout);
countedHashDump(strandHash, stdout);
countedHashDump(frameHash, stdout);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
gffPeek(argv[1]);
return 0;
}
