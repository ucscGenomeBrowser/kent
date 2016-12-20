/* edwFixReplaced - Clean up files that were replaced in ENCODE2. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* The input for this program was generated with:
 * mdbQuery "select fileName,objStatus from hg19 where objStatus like 'replaced%'" -out=tab -table=metaDb_cricket > replacedHg19.tab
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixReplaced - Clean up files that were replaced in ENCODE2\n"
  "usage:\n"
  "   edwFixReplaced database tabFile spikeinRenameTab out.sql out.ra\n"
  "where tabFile is two columns: fileName and objStatus from the metaDb\n"
  "See source code for how this file was generated. and spikeinRenameTab is\n"
  "generated from edwFixCshlSpikeins\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *rootRenameHash()
/* Make up hash of cases where rename was more than just adding V2 or bumping the version # */
{
struct hash *hash = hashNew(0);
hashAdd(hash, "wgEncodeBroadHistoneGm12878H3k4me1StdAlnRep1", 
	      "wgEncodeBroadHistoneGm12878H3k04me1StdAlnRep1");
hashAdd(hash, "wgEncodeBroadHistoneGm12878H3k4me1StdPk", 
	      "wgEncodeBroadHistoneGm12878H3k04me1StdPk");
hashAdd(hash, "wgEncodeBroadHistoneGm12878H3k4me1StdRawDataRep1",
              "wgEncodeBroadHistoneGm12878H3k04me1StdRawDataRep1");
hashAdd(hash, "wgEncodeBroadHistoneGm12878H3k4me1StdSig", 
              "wgEncodeBroadHistoneGm12878H3k04me1StdSig");
hashAdd(hash, "wgEncodeBroadHistoneGm12878H3k4me3StdAlnRep2", 
              "wgEncodeBroadHistoneGm12878H3k04me3StdAlnRep2");
hashAdd(hash, "wgEncodeBroadHistoneGm12878H3k4me3StdPk", 
              "wgEncodeBroadHistoneGm12878H3k04me3StdPk");
hashAdd(hash, "wgEncodeBroadHistoneGm12878H3k4me3StdRawDataRep2", 
              "wgEncodeBroadHistoneGm12878H3k04me3StdRawDataRep2");
hashAdd(hash, "wgEncodeBroadHistoneGm12878H3k4me3StdSig", 
              "wgEncodeBroadHistoneGm12878H3k04me3StdSig");

hashAdd(hash, "wgEncodeCaltechRnaSeqGm12878R1x75dMinusSignalRep1V3",
              "wgEncodeCaltechRnaSeqGm12878R1x75dTh1014UMinusRawRep1V3");
hashAdd(hash, "wgEncodeCaltechRnaSeqGm12878R1x75dPlusSignalRep1V3",
              "wgEncodeCaltechRnaSeqGm12878R1x75dTh1014UPlusRawRep1V3");
hashAdd(hash, "wgEncodeCaltechRnaSeqGm12878R1x75dMinusSignalRep2V3",
              "wgEncodeCaltechRnaSeqGm12878R1x75dTh1014UMinusRawRep2V3");
hashAdd(hash, "wgEncodeCaltechRnaSeqGm12878R1x75dPlusSignalRep2V3",
              "wgEncodeCaltechRnaSeqGm12878R1x75dTh1014UPlusRawRep2V3");
return hash;
}

void edwFixReplaced(char *database, char *inTab, char *spikedTab, char *outSql, char *outRa)
/* edwFixReplaced - Clean up files that were replaced in ENCODE2. */
{
struct sqlConnection *conn = edwConnect();
struct lineFile *lf = lineFileOpen(inTab, TRUE);
FILE *fSql = mustOpen(outSql, "w");
FILE *fRa = mustOpen(outRa, "w");
char *row[2];
struct hash *renameHash = rootRenameHash();
struct hash *spikedHash = hashTwoColumnFile(spikedTab);
int depCount = 0, repCount = 0;
while (lineFileRowTab(lf, row))
    {
    /* Get fields in local variables. */
    char *oldFileName = row[0];
    char *objStatus = row[1];

    /* Do spikein rename lookup. */
    char *spiked = hashFindVal(spikedHash, oldFileName);
    if (spiked != NULL)
	{
	verbose(2, "renaming spikeing %s to %s\n", oldFileName, spiked);
        oldFileName = spiked;
	}

    /* Get rid of bai name for bam,bai pairs. */
    char *comma = strchr(oldFileName, ',');
    if (comma != NULL)
        {
	if (!endsWith(comma, ".bai"))
	    errAbort("Unexpected conjoining of files line %d of %s", lf->lineIx, lf->fileName);
	*comma = 0;
	}

    /* For .fastq.tgz files we got to unpack them. */
    if (endsWith(oldFileName, ".fastq.tgz"))
	{
	/* Get root name - name minus suffix */
	char *oldRoot = cloneString(oldFileName);
	chopSuffix(oldRoot);
	chopSuffix(oldRoot);
	verbose(2, "Processing fastq.tgz %s %s\n", oldFileName, oldRoot);

	// Find records for old version.
	char query[512];
	sqlSafef(query, sizeof(query), 
	    "select * from edwFile where submitFileName like '%s/%%/%s.fastq.tgz.dir/%%'"
	    " order by submitFileName",
	    database, oldRoot);
	struct edwFile *oldList = edwFileLoadByQuery(conn, query);
	int oldCount = slCount(oldList);
	if (oldCount == 0)
	    errAbort("No records match %s", query);


	// Find record for replaced version.
	// Fortunately all of the fastq.tgz's are just V2, which simplifies code a bit
	sqlSafef(query, sizeof(query), 
	    "select * from edwFile where submitFileName like '%s/%%/%sV2.fastq.tgz.dir/%%'"
	    " order by submitFileName",
	    database, oldRoot);
	struct edwFile *newList = edwFileLoadByQuery(conn, query);
	int newCount = slCount(newList);
	if (newCount == 0)
	    errAbort("No records match %s", query);

	// Make a hash of new records keyed by new file name inside of tgz
	struct edwFile *newEf;
	struct hash *newHash = hashNew(0);
	for (newEf = newList; newEf != NULL; newEf = newEf->next)
	    {
	    char fileName[FILENAME_LEN];
	    splitPath(newEf->submitFileName, NULL, fileName, NULL);
	    hashAdd(newHash, fileName, newEf);
	    verbose(2, " %s\n", fileName);
	    }
	verbose(2, "%d in oldList, %d in newList\n", oldCount, newCount);

	// Loop through old records trying to find corresponding new record
	struct edwFile *oldEf;
	for (oldEf = oldList; oldEf != NULL; oldEf = oldEf->next)
	    {
	    char fileName[FILENAME_LEN];
	    splitPath(oldEf->submitFileName, NULL, fileName, NULL);
	    struct edwFile *newEf = hashFindVal(newHash, fileName);
	    char *newName = "n/a";
	    fprintf(fSql, "update edwFile set deprecated='%s' where id=%u;\n", objStatus, oldEf->id);
	    ++depCount;
	    if (newEf != NULL)
	        {
		fprintf(fSql, "update edwFile set replacedBy=%u where id=%u;\n", newEf->id, oldEf->id);
		newName = newEf->submitFileName;
		++repCount;
		}
	    fprintf(fRa, "objStatus %s\n", objStatus);
	    fprintf(fRa, "oldFile %s\n", oldEf->submitFileName);
	    fprintf(fRa, "newFile %s\n", newName);
	    fprintf(fRa, "\n");
	    verbose(2, "%s -> %s\n", oldEf->submitFileName, newName);
	    }
	}
    else
	{

	/* Figure out new file name by either adding V2 at end, or if there is already a V#,
	 * replacing it. */
#ifdef SOON
#endif /* SOON */
	int oldVersion = 1;
	char *noVersion = NULL;
	    {
	    /* Split old file name into root and suffix. */
	    char *suffix = edwFindDoubleFileSuffix(oldFileName);
	    if (suffix == NULL)
		errAbort("No suffix in %s line %d of %s", oldFileName, lf->lineIx, lf->fileName);
	    char *oldRoot = cloneStringZ(oldFileName, suffix - oldFileName);
	    char *renamed = hashFindVal(renameHash, oldRoot);
	    if (renamed != NULL)
		{
		verbose(2, "Overriding %s with %s\n", oldRoot, renamed);
		oldRoot = cloneString(renamed);
		}


	    /* Look for V# at end of old root, and if it's there chop it off and update oldVersion */
	    noVersion = oldRoot;  // If no V, we done. */
	    char *vPos = strrchr(oldRoot, 'V');
	    if (vPos != NULL)
		{
		char *numPos = vPos + 1;
		int numSize = strlen(numPos);
		if (numSize == 1 || numSize == 2)
		    {
		    if (isAllDigits(numPos))
			{
			oldVersion = atoi(numPos);
			*vPos = 0;
			}
		    else
			errAbort("Expecting numbers after V in file name got %s line %d of %s",
			    numPos, lf->lineIx, lf->fileName);
		    }
		}
	    verbose(2, "%s parses to  %s %d %s\n", oldFileName, noVersion, oldVersion, suffix);

	    /* Find record for old file. */
	    char query[512];
	    sqlSafef(query, sizeof(query), 
		"select * from edwFile where submitFileName like '%s/%%/%s'", 
		database, oldFileName);
	    struct edwFile *oldEf = edwFileLoadByQuery(conn, query);
	    if (slCount(oldEf) != 1)
		errAbort("Expecting one result got %d for %s\n", slCount(oldEf), query);
	    fprintf(fSql, "# %s %s\n", oldFileName, objStatus);
	    verbose(2, "%s: %s\n", oldFileName, objStatus);

	    /* Find record for new file. */
	    struct edwFile *newEf = NULL;
	    int newVersion;
	    for (newVersion = oldVersion+1; newVersion < 7; ++newVersion)
		{
		sqlSafef(query, sizeof(query), 
		    "select * from edwFile where submitFileName like '%s/%%/%sV%d%s'",
		    database, noVersion, newVersion, suffix); 
		newEf = edwFileLoadByQuery(conn, query);
		if (newEf != NULL)
		    break;
		}
	    if (newEf == NULL)
		verbose(2, "Could not find next version of %s (%s)", oldFileName, oldRoot);
	    if (slCount(newEf) > 1)
		errAbort("Expecting one result got %d for %s\n", slCount(newEf), query);

	    long long oldId = oldEf->id;
	    fprintf(fSql, "update edwFile set deprecated='%s' where id=%lld;\n", objStatus, oldId);
	    ++depCount;
	    char *newName = "n/a";
	    if (newEf != NULL)
		{
		long long newId = newEf->id;
		fprintf(fSql, "update edwFile set replacedBy=%lld where id=%lld;\n", newId, oldId);
		newName = newEf->submitFileName;
		++repCount;
		}
	    fprintf(fRa, "objStatus %s\n", objStatus);
	    fprintf(fRa, "oldFile %s\n", oldEf->submitFileName);
	    fprintf(fRa, "newFile %s\n", newName);
	    fprintf(fRa, "\n");
	    verbose(2, "%s -> %s\n", oldEf->submitFileName, newName);
	    }
	}
    }
verbose(1, "%d deprecated, %d replaced\n", depCount, repCount);
carefulClose(&fSql);
carefulClose(&fRa);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
edwFixReplaced(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
