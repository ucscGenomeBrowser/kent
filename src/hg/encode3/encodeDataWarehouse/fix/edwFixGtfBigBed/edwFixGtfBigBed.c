/* edwFixGtfBigBed - In original import the .gtf.bigBed files were bad about half the time.  
 * Cricket caught this because a bunch of them ended up with the same md5 sum.  This program 
 * regenerates them all. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "md5.h"
#include "cheapcgi.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "encode3/encode3Valid.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixGtfBigBed - In original import the .gtf.bigBed files were bad about half the time.  Cricket caught this because a bunch of them ended up with the same md5 sum.  This program regenerates them all.\n"
  "usage:\n"
  "   edwFixGtfBigBed how\n"
  "where 'how' is either 'test' or 'real' meaning just print out commands that would be executed or\n"
  "actually executing commands\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

boolean doReal;

void doSystem(char *command)
/* Do system call if doReal is set,  otherwise just print command. */
{
printf("%s\n", command);
if (doReal)
    {
    mustSystem(command);
    }
}

void makeGtfBigBed(char *ucscDb, char *sourceFileName, char *destFileName)
/* Convert GTF source file to bigBed dest file. */
{
char chromSizes[PATH_LEN];
safef(chromSizes, sizeof(chromSizes), "%s%s/chrom.sizes", edwValDataDir, ucscDb);
extern char *edwValDataDir; /* Data files we need for validation go here. */
char command[1024];
safef(command, sizeof(command), "encode2GffDoctor %s tmp.gtf", sourceFileName);
doSystem(command);
doSystem("gffToBed tmp.gtf tmp.unsorted");
doSystem("sort -k1,1 -k2,2n tmp.unsorted > tmp.sorted");
safef(command, sizeof(command), "bedClip tmp.sorted %s tmp.clipped", chromSizes);
doSystem(command);
safef(command, sizeof(command), "bedToBigBed tmp.clipped %s tmp.bigBed", chromSizes);
doSystem(command);
safef(command, sizeof(command), "mv tmp.bigBed %s", destFileName);
doSystem(command);
doSystem("rm tmp.gtf tmp.unsorted tmp.sorted tmp.clipped");
}

void redoOne(struct sqlConnection *conn, struct edwFile *redoEf)
/* Redo one file. */
{
/* Figure out submit file name of the gtf file. */
char gtfFileName[PATH_LEN];
strcpy(gtfFileName, redoEf->submitFileName);
chopSuffix(gtfFileName);
strcat(gtfFileName, ".gz");

/* Get edwFile record for gtf file. */
char query[PATH_LEN+64];
safef(query, sizeof(query), "select * from edwFile where submitFileName='%s'", gtfFileName);
struct edwFile *sourceEf = edwFileLoadByQuery(conn, query);
assert(slCount(sourceEf) == 1);

/* Get UCSC database */
safef(query, sizeof(query), "select ucscDb from edwValidFile where fileId=%u", sourceEf->id);
char ucscDb[64] = "";
sqlQuickQuery(conn, query, ucscDb, sizeof(ucscDb));
assert(ucscDb[0] != 0);

/* Remake the big bed file. */
char sourceFileName[PATH_LEN], destFileName[PATH_LEN];
safef(sourceFileName, sizeof(sourceFileName), "%s%s", edwRootDir, sourceEf->edwFileName);
safef(destFileName, sizeof(destFileName), "%s%s", edwRootDir, redoEf->edwFileName);
makeGtfBigBed(ucscDb, sourceFileName, destFileName);

/* Recalculate size and md5 sum and validation key. */
char *md5 = md5HexForFile(destFileName);
long long size = fileSize(destFileName);
char *validKey = encode3CalcValidationKey(md5, size);

/* Issue command to update md5 in database. */
char command[2*PATH_LEN];
safef(command, sizeof(command),
    "hgsql -e 'update edwFile set md5=\"%s\" where id=%u' encodeDataWarehouse", md5, redoEf->id);
doSystem(command);

/* Issue command to update tags in database. */
char *newTags = cgiStringNewValForVar(redoEf->tags, "valid_key", validKey); 
if (doReal)
    {
    edwFileResetTags(conn, redoEf, newTags);
    }
}

void edwFixGtfBigBed(char *how)
/* edwFixGtfBigBed - In original import the .gtf.bigBed files were bad about half the time.  Cricket 
 * caught this because a bunch of them ended up with the same md5 sum.  This program regenerates them 
 * all. */
{
doReal = sameString(how, "real");
struct sqlConnection *conn = edwConnectReadWrite();
struct edwFile *redoEf, *redoList = 
    edwFileLoadByQuery(conn, "select * from edwFile where submitFileName like '%.gtf.bigBed'");
for (redoEf = redoList; redoEf != NULL; redoEf = redoEf->next)
    {
    redoOne(conn, redoEf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwFixGtfBigBed(argv[1]);
return 0;
}
