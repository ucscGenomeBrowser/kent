/* toBb - Convert tables to bb format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "hdb.h"
#include "rmskOut.h"
#include "bbRepMask.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "toBb - Convert tables to bb format\n"
  "usage:\n"
  "   toBb database sourceTable bbTable\n"
  "options:\n"
  "   -test\n"
  );
}

static int binOffsets[] = {512+64+8+1, 64+8+1, 8+1, 1, 0};

int findBin(int start, int end)
/* Given start,end in chromosome coordinates assign it
 * a bin.   There's a bin for each 128k segment, for each
 * 1M segment, for each 8M segment, for each 64M segment,
 * and for each chromosome (which is assumed to be less than
 * 512M.)  A range goes into the smallest bin it will fit in. */
{
static int shiftFactors[] = {17, 20, 23, 26, 29};
int startBin = start, endBin = end-1, i;
startBin >>= 17;
endBin >>= 17;
for (i=0; i<ArraySize(binOffsets); ++i)
    {
    if (startBin == endBin)
        return binOffsets[i] + startBin;
    startBin >>= 3;
    endBin >>= 3;
    }
errAbort("start %d, end %d out of range in findBin (max is 512M)", start, end);
return 0;
}

char *tableRoot(char *name)
/* Given table, chrN_table, or chrN_random_table, return table. */
{
char *s;
if (!startsWith("chr", name))
    return name;
if ((s = strchr(name, '_')) == NULL)
    return name;
s += 1;
if (!startsWith("random_", s))
    return s;
return s+7;
}

struct seqId
/* Sequence ID. */
    {
    struct seqId *next;
    char *name;	/* Name, allocated in hash. */
    int id;	/* Id. */
    };

struct seqId *addId(struct hash *seqIdHash, char *name, int id)
/* Allocate seqId and add it to hash. */
{
struct seqId *si;
AllocVar(si);
si->id = id;
hashAddSaveName(seqIdHash, name, si, &si->name);
return si;
}

int getSeqId(struct hash *seqIdHash, char *chrom)
/* Get ID for sequence. */
{
static char *lastChrom = "";
static struct seqId *si;
if (!sameString(lastChrom, chrom))
    {
    si = hashMustFindVal(seqIdHash, chrom);
    lastChrom = si->name;
    }
return si->id;
}

struct hash *makeSeqIdHash()
/* Make hash of sequence ID's. */
{
int isRandom, offset, i;
char seqName[256];
char *suffix = "";
struct seqId *si;
struct hash *seqIdHash = newHash(6);
static char *extras[] = {"X", "Y", "UL", "NA"};

for (isRandom = 0, offset = 0; isRandom <= 1; ++isRandom, offset += 100)
    {
    for (i=1; i<=22; ++i)
        {
	sprintf(seqName, "chr%d%s", i, suffix);
	addId(seqIdHash, seqName, i + offset);
	}
    for (i=0; i< ArraySize(extras); ++i)
        {
	sprintf(seqName, "chr%s%s", extras[i], suffix);
	addId(seqIdHash, seqName, i + 23 + offset);
	}
    suffix = "_random";
    }
return seqIdHash;
}

char *bbRepMaskCreate =
"CREATE TABLE %s (\n"
"    seqId tinyint unsigned not null,    # Id of chromosome\n"
"    seqStart int unsigned not null,     # Start in sequence\n"
"    seqEnd int unsigned not null,       # End in sequence\n"
"    bin smallint unsigned not null,     # Which bin it's in.\n"
"    name varchar(255) not null, # Name of repeat\n"
"    score int unsigned not null,        # Smith-Waterman score.\n"
"    strand char(1) not null,    # Query strand (+ or C)\n"
"    repFamily varchar(255) not null,      # Repeat name\n"
"    repStart int not null,      # Start position in repeat.\n"
"    repEnd int not null, 	 # End position in repeat.\n"
"    repLeft int not null,       # Bases left in repeat.\n"
"    milliDiv int unsigned not null,     # Percentage base divergence.\n"
"    milliDel int unsigned not null,     # Percentage deletions.\n"
"    milliIns int unsigned not null,     # Percentage inserts.\n"
"              #Indices\n"
"    INDEX(bin),\n"
"    UNIQUE(seqStart),\n"
"    UNIQUE(seqEnd)\n"
")\n";

void convertRepMask(struct sqlConnection *conn, FILE *f, 
        struct hash *seqIdHash, char *sourceTable, char *destTable)
/* Convert table. */
{
struct rmskOut old;
static struct bbRepMask bb;
struct dyString *dy = newDyString(1024);
struct sqlResult *sr;
char **row;
char *lastChr = "";
int seqId;


dyStringPrintf(dy, bbRepMaskCreate, destTable);
sqlUpdate(conn, dy->string);

dyStringClear(dy);
dyStringPrintf(dy, "select * from %s", sourceTable);
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rmskOutStaticLoad(row, &old);
    bb.seqId = getSeqId(seqIdHash, old.genoName);
    bb.seqStart = old.genoStart;
    bb.seqEnd = old.genoEnd;
    bb.bin = findBin(bb.seqStart, bb.seqEnd);
    bb.name = old.repName;
    bb.score = old.swScore;
    strcpy(bb.strand, old.strand);
    bb.repFamily = old.repFamily;
    bb.repStart = old.repStart;
    bb.repEnd = old.repEnd;
    bb.repLeft = old.repLeft;
    bb.milliDiv = old.milliDiv;
    bb.milliDel = old.milliDel;
    bb.milliIns = old.milliIns;
    bbRepMaskTabOut(&bb, f);
    }
freeDyString(&dy);
}

void toBb(char *database, char *sourceTable, char *destTable)
/* toBb - Convert tables to bb format. */
{
struct sqlConnection *conn;
char *sourceRoot = tableRoot(sourceTable);
char query[256];
char *tabFileName = "bb.tab";
FILE *f = mustOpen(tabFileName, "w");
struct hash *seqIdHash = makeSeqIdHash();

hSetDb(database);
conn = hAllocConn();

/* Drop existing dest table if it exists. */
if (sqlTableExists(conn, destTable))
    {
    sprintf(query, "drop table %s", destTable);
    sqlUpdate(conn, query);
    }

/* Look through all the types we know and generate tab file. */
printf("Generating tab file from %s\n", sourceTable);
if (sameString(sourceRoot, "rmsk"))
    convertRepMask(conn, f, seqIdHash, sourceTable, destTable);
else
    errAbort("This program doesn't know how to convert %s", sourceTable);
carefulClose(&f);

/* Load database. */
printf("Loading %s.%s\n", database, destTable);
sprintf(query, "load data local infile '%s' into table %s", 
    tabFileName, destTable);
sqlUpdate(conn, query);
hFreeConn(&conn);
}


void doTest(int start, int end)
{
int startBin = (start>>17), endBin = ((end-1)>>17), i, j;
printf("bins for %d - %d:\n", start, end);
for (i=0; i<ArraySize(binOffsets); ++i)
    {
    printf("  %d-%d\n", startBin+binOffsets[i], endBin+binOffsets[i]);
    startBin >>= 3;
    endBin >>= 3;
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (cgiVarExists("test"))
    {
    doTest(atoi(argv[1]),atoi(argv[2]));
    exit(0);
    }
if (argc != 4)
    usage();
toBb(argv[1], argv[2], argv[3]);
return 0;
}
