/* cdwMakeTrackViz - If possible make cdwTrackViz table entry for a file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "portable.h"
#include "cdw.h"
#include "cdwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwMakeTrackViz - If possible make cdwTrackViz table entry for a file.\n"
  "usage:\n"
  "   cdwMakeTrackViz startFileId endFileId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct tagStorm *getTags(struct sqlConnection *conn)
/* Get cached tagStorm */
{
static struct tagStorm *tags;
if (tags == NULL)
    tags = cdwTagStorm(conn);
return tags;
}

struct hash *getAccHash(struct sqlConnection *conn)
/* Get tags indexed by accession */
{
static struct hash *hash;
if (hash == NULL)
     hash = tagStormUniqueIndex(getTags(conn), "accession");
return hash;
}

struct cdwTrackViz *cdwTrackVizForFileId(struct sqlConnection *conn, long long fileId)
/* Load track of given ID */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwTrackViz where fileId=%lld", fileId);
return cdwTrackVizLoadByQuery(conn, query);
}

boolean ifNotTooLong(struct dyString *dy, char *label, char *value, int maxSize)
/* If not too big, add to string */
{
int valSize = strlen(value);
if (label != NULL)
    {
    int labelSize = strlen(label);
    int totalSize = labelSize + valSize + 1;
    if (totalSize + dy->stringSize <= maxSize)
        dyStringPrintf(dy, " %s %s", label, value);
    return TRUE;
    }
else
    {
    if (valSize + dy->stringSize <= maxSize)
        dyStringPrintf(dy, " %s", value);
    return TRUE;
    }
return FALSE;
}

void addIfNotTooLong(struct dyString *dy, struct tagStanza *stanza, char *label,
    char *field, int maxSize)
/* If field exists, and it's not too long, add it to dyString */
{
char *val = tagFindVal(stanza, field);
if (!isEmpty(val))
    ifNotTooLong(dy, label, val, maxSize);
}


char *makeLongLabel(struct sqlConnection *conn, struct hash *accHash,
    struct cdwFile *ef, struct cdwValidFile *vf)
/* Make attempt at meaningful long label. */
{
struct dyString *dy = dyStringNew(0);
dyStringPrintf(dy, "CIRM %s", vf->licensePlate);
struct tagStanza *stanza = hashFindVal(accHash, vf->licensePlate);
if (stanza != NULL)
    {
    addIfNotTooLong(dy, stanza, NULL, "lab", 100);
    addIfNotTooLong(dy, stanza, NULL, "assay", 100);
    addIfNotTooLong(dy, stanza, NULL, "output", 100);
    char *target = tagFindVal(stanza, "target_epitope");
    if (target == NULL)
        target = tagFindVal(stanza, "target_gene");
    if (target == NULL)
        target = tagFindVal(stanza, "antibody");
    if (target == NULL)
        target = tagFindVal(stanza, "control");
    if (target == NULL)
        target = tagFindVal(stanza, "assay_seq");
    if (target != NULL)
	ifNotTooLong(dy, NULL, target, 100);
    addIfNotTooLong(dy, stanza, "t", "t", 100);
    addIfNotTooLong(dy, stanza, NULL, "donor", 100);
    addIfNotTooLong(dy, stanza, NULL, "strain", 100);
    addIfNotTooLong(dy, stanza, NULL, "disease", 100);
    addIfNotTooLong(dy, stanza, NULL, "sample_label", 100);
    addIfNotTooLong(dy, stanza, NULL, "submit_file_name", 100);
    }

return dyStringCannibalize(&dy);
}

boolean gotTrackViz(struct sqlConnection *conn, long long fileId)
/* Return TRUE if have viz info */
{
char query[128];
sqlSafef(query, sizeof(query), "select count(*) from cdwTrackViz where fileId=%lld", fileId);
return sqlQuickNum(conn, query) > 0;
}


void sortAndBuildBai(struct sqlConnection *conn,
    struct cdwFile *ef, struct cdwValidFile *vf)
/* If need be sort bam.  Build bai.  Update cdwTrackViz table 
 * after that's all done */
{
if (gotTrackViz(conn, ef->id))
    return;
struct cdwBamFile *bam = cdwBamFileFromFileId(conn, ef->id);
if (bam == NULL)
    return;
char sortedBamName[PATH_LEN];
if (!bam->isSortedByTarget)
    {
    warn("Skipping %d because it's not sorted", ef->id);
    return;
    }
else
    {
    safef(sortedBamName, sizeof(sortedBamName), "%s", ef->cdwFileName);
    }

/* See if we have a .bai file already */
char indexName[PATH_LEN];
safef(indexName, sizeof(indexName), "%s%s.bai", cdwRootDir, sortedBamName);
if (!fileExists(indexName))
    {
    char command[2*PATH_LEN];
    safef(command, sizeof(command), "samtools index %s%s", cdwRootDir, sortedBamName);
    mustSystem(command);
    }

/* Make up cdwTrackViz record */
struct hash *accHash = getAccHash(conn);
char *longLabel = makeLongLabel(conn, accHash, ef, vf);
char update[3*PATH_LEN];
sqlSafef(update, sizeof(update), 
    "insert cdwTrackViz (fileId,shortLabel,longLabel,type,bigDataFile) "
    "values (%d,\"%s\", \"%s\", \"%s\", \"%s\")"
    , ef->id, vf->licensePlate, longLabel, "bam", ef->cdwFileName);
uglyf("update: %s\n", update);
sqlUpdate(conn, update);

}

void compressAndBuildTabix(struct sqlConnection *conn,
    struct cdwFile *ef, struct cdwValidFile *vf)
/* Make bgzipped version of VCF file, and build a tabix on it.
 * After that is done update cdwTrackViz table */
{
if (gotTrackViz(conn, ef->id))
    return;

/* See if we can find tabix entry in same submission. */
char submittedTabexPath[PATH_LEN];
safef(submittedTabexPath, sizeof(submittedTabexPath), "%s.tbi", ef->submitFileName);
long long tabixId = cdwFindInSameSubmitDir(conn, ef, submittedTabexPath);

char bgzippedFile[PATH_LEN];
safef(bgzippedFile, sizeof(bgzippedFile), "%s", ef->cdwFileName);

if (tabixId == 0)
     {
     uglyf("tabixId=0\n");
     char bgzippedVcf[PATH_LEN];
     safef(bgzippedVcf, sizeof(bgzippedVcf), "%s%s", cdwRootDir, ef->cdwFileName);
     char command[3*PATH_LEN];
     /* Do bgzipping */
     if (!endsWith(ef->cdwFileName, ".gz"))
         {
	 safef(command, sizeof(command), "bgzip -c %s > %s.gz", bgzippedVcf, bgzippedVcf);
	 mustSystem(command);
	 strcat(bgzippedVcf, ".gz");
	 strcat(bgzippedFile, ".gz");
	 }
     /* Do tabixing */
     safef(command, sizeof(command), "tabix -p vcf %s", bgzippedVcf);
     mustSystem(command);
     }
else
     {
     uglyf("tabixId=%lld\n", tabixId);
     char symLinkNew[PATH_LEN];
     safef(symLinkNew, sizeof(symLinkNew), "%s%s.tbi", cdwRootDir, ef->cdwFileName);
     if (!fileExists(symLinkNew))
         {
	 struct cdwFile *indexFile = cdwFileFromId(conn, tabixId);
	 char relIndexPath[PATH_LEN];
	 safef(relIndexPath, sizeof(relIndexPath), "../../../%s", indexFile->cdwFileName);
	 verbose(1, "ln -s %s %s\n", relIndexPath, symLinkNew);
	 makeSymLink(relIndexPath, symLinkNew);
	 }
     }

/* Make up cdwTrackViz record */
struct hash *accHash = getAccHash(conn);
char *longLabel = makeLongLabel(conn, accHash, ef, vf);
char update[3*PATH_LEN];
sqlSafef(update, sizeof(update), 
    "insert cdwTrackViz (fileId,shortLabel,longLabel,type,bigDataFile) "
    "values (%d,\"%s\", \"%s\", \"%s\", \"%s\")"
    , ef->id, vf->licensePlate, longLabel, "vcfTabix", bgzippedFile);
uglyf("update: %s\n", update);
sqlUpdate(conn, update);

}

void cdwTrackBigViz(struct sqlConnection *conn, 
    struct cdwFile *ef, struct cdwValidFile *vf, char *format)
/* Make wrapper around big wig and big bed */
{
/* See if we got track viz already, and if so just return early. */
int fileId = ef->id;
if (gotTrackViz(conn, fileId))
    return;

struct hash *accHash = getAccHash(conn);
char *longLabel = makeLongLabel(conn, accHash, ef, vf);
char update[3*PATH_LEN];
sqlSafef(update, sizeof(update), 
    "insert cdwTrackViz (fileId,shortLabel,longLabel,type,bigDataFile) "
    "values (%d,\"%s\", \"%s\", \"%s\", \"%s\")"
    , fileId, vf->licensePlate, longLabel, format, ef->cdwFileName);
uglyf("update: %s\n", update);
sqlUpdate(conn, update);
}

void cdwMakeTrackViz(int startFileId, int endFileId)
/* cdwMakeTrackViz - If possible make cdwTrackViz table entry for a file.. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
struct cdwFile *ef, *efList = cdwFileAllIntactBetween(conn, startFileId, endFileId);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    struct cdwValidFile *vf = cdwValidFileFromFileId(conn, ef->id);
    if (vf != NULL)
	{
	char *format = vf->format;
	if (sameString(format, "bam"))
	    {
	    sortAndBuildBai(conn, ef, vf);
	    }
	else if (sameString(format, "vcf"))
	    {
	    compressAndBuildTabix(conn, ef, vf);
	    }
	else if (sameString(format, "narrowPeak"))
	    {
	    cdwTrackBigViz(conn, ef, vf, format);
	    }
	else if (sameString(format, "broadPeak"))
	    {
	    cdwTrackBigViz(conn, ef, vf, format);
	    }
	else if (sameString(format, "bigWig"))
	    {
	    cdwTrackBigViz(conn, ef, vf, format);
	    }
	else if (sameString(format, "bigBed"))
	    {
	    cdwTrackBigViz(conn, ef, vf, format);
	    }
	}
    }
sqlDisconnect(&conn);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cdwMakeTrackViz(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
