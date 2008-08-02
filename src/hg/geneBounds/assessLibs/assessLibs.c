/* assessLibs - Make table that assesses the percentage of library that covers 5' and 3' ends. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"
#include "genePred.h"
#include "bits.h"

static char const rcsid[] = "$Id: assessLibs.c,v 1.6.116.2 2008/08/02 04:06:18 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "assessLibs - Make table that assesses the percentage of library that covers 5' and 3' ends\n"
  "usage:\n"
  "   assessLibs database refTrack\n"
  "options:\n"
  "   -chrom=chrNN - restrict this to chromosome NN\n"
  "NOTE - CURRENTLY A WORK IN PROGRESS, NOT YET USEFUL TO RUN\n"
  );
}

struct estExtraInfo
/* Extra information about an EST */
    {
    char *name;		/* Name - allocated in hash */
    char direction;     /* 5 or 3 or 0 - as characters. */
    };

struct libEstHash
/* A list of libraries that includes all a hash of all ESTs in the library */
    {
    struct libEstHash *next; /* Next in list. */
    char *id;		   /* Library ID as ascii number - allocated in hash. */
    char *name;		   /* Library name. */
    struct hash *estHash;  /* Hash of all ESTs in the library. Key is est accession, 
                              value is estExtraInfo */
    struct libStats *stats;	/* Stats on this library. */
    };


struct genePred *loadGenePredsOnChrom(char *database, char *chrom, char *refTrack)
/* Load all of the gene predictionsA for a given chromosome
 * for a given track. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
struct genePred *gpList = NULL, *gp;

sprintf(query, "select * from %s where chrom = '%s'", refTrack, chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row);
    slAddHead(&gpList, gp);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&gpList);
return gpList;
}

struct psl *loadEstsOnChrom(char *database, char *chrom)
/* Load all of the ESTs on a given chromosome. */
{
int rowOffset;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = hChromQuery(conn, "est", chrom, NULL, &rowOffset);
char **row;
struct psl *pslList = NULL, *psl;

while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+rowOffset);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&pslList);
return pslList;
}

void pslSetBits(Bits *bits, struct psl *psl)
/* Set bits covered by psl */
{
int i, count = psl->blockCount;
unsigned *starts = psl->tStarts, *sizes = psl->blockSizes;

for (i=0; i<count; ++i)
    bitSetRange(bits, starts[i], sizes[i]);
}

void setEstsInHash(Bits *bits, struct psl *pslList, struct hash *estHash)
/* Set bits covered by ests that are in hash . */
{
struct psl *psl;
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    if (hashLookup(estHash, psl->qName))
        {
	pslSetBits(bits, psl);
	}
    }
}

void assessLibsOnChroms(char *database, struct slName *chromList, char *refTrack, struct libEstHash *libList)
/* Load up each chromosome and evaluate EST alignment stats on it. */
{
struct slName *chrom;
struct dnaSeq *seq;
struct genePred *gpList = NULL;
struct psl *estList;
struct libEstHash *lib;
Bits *bits = NULL;

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    seq = hLoadChrom(database, chrom->name);
    bits = bitAlloc(seq->size);
    gpList = loadGenePredsOnChrom(database, chrom->name, refTrack);
    estList = loadEstsOnChrom(database, chrom->name);
    uglyf("%s: %d bases, %d %s, %d ests\n", chrom->name, seq->size, slCount(gpList), refTrack, slCount(estList));
    for (lib = libList; lib != NULL; lib = lib->next)
        {
	bitClear(bits, seq->size);
	setEstsInHash(bits, estList, lib->estHash);
	uglyf("%s\n", lib->name);
	}
    pslFreeList(&estList);
    genePredFreeList(&gpList);
    bitFree(&bits);
    freeDnaSeq(&seq);
    }
}

void readLibAndEstInfo(char *database, struct libEstHash **retList, struct hash **retHash)
/* Make a list of libraries including all of the ESTs in that library.
 * Return list, and hash of list. */
{
struct libEstHash *libList = NULL, *lib, *newList = NULL, *next;
struct hash *libHash = newHash(0);
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int estCount = 0;
struct estExtraInfo *est;

/* Set up hash and list of libraries. */
sprintf(query, "select id,name from library");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(lib);
    lib->name = cloneString(row[1]);
    hashAddSaveName(libHash, row[0], lib, &lib->id);
    slAddHead(&libList, lib);
    }
sqlFreeResult(&sr);
uglyf("Got %d libraries\n", slCount(libList));

/* Read all ESTs and lump them into libraries. */
sprintf(query, "select acc,direction,library from gbCdnaInfo where type = 'EST'");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *acc = row[0], *direction = row[1], *library = row[2];
    ++estCount;
    lib = hashFindVal(libHash, library);
    if (lib == NULL)
        {
	warn("Lib ID %s is in gbCdnaInfo table for %s but not in library table", library, acc);
	continue;
	}
    if (lib->estHash == NULL)
         lib->estHash = newHash(12);
    AllocVar(est);
    est->direction = direction[0];
    hashAddSaveName(lib->estHash, acc, est, &est->name);
    }
uglyf("Got %d ESTs\n", estCount);

/* Weed out libraries that have no ESTs */
for (lib = libList; lib != NULL; lib = next)
    {
    next = lib->next;
    if (lib->estHash != NULL)
        {
	slAddHead(&newList, lib);
	}
    else
        {
	freeMem(lib->name);
	freez(&lib);
	}
    }
libList = newList;


hFreeConn(&conn);
*retList = libList;
*retHash = libHash;
}


void assessLibs(char *database, char *refTrack)
/* assessLibs - Make table that assesses the percentage of library that covers 5' and 3' ends. */
{
char *chrom;
struct slName *chromList = NULL;
struct libEstHash *libList = NULL;
struct hash *libHash;

/* See if user wants to do a particular chromosome, otherwise get
 * list of all chromosome. */
chrom = cgiOptionalString("chrom");
if (chrom == NULL)
    chromList = hAllChromNames(database);
else
    chromList = newSlName(chrom);

readLibAndEstInfo(database, &libList, &libHash);
assessLibsOnChroms(database, chromList, refTrack, libList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
assessLibs(argv[1], argv[2]);
return 0;
}
