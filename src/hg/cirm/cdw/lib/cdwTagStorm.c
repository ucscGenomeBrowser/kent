/* cdwTagStorm - create a tag storm out of database contents. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "tagStorm.h"
#include "intValTree.h"
#include "cdw.h"
#include "cdwLib.h"

static void addPairedEndTags(struct tagStorm *tagStorm, struct tagStanza *stanza, 
    struct cdwQaPairedEndFastq *pair, char *mateAccession)
/* Add tags from pair to stanza, also add mateAccession */
{
if (mateAccession != NULL)
    tagStanzaAdd(tagStorm, stanza, "paired_end_mate", mateAccession);
tagStanzaAddDouble(tagStorm, stanza, "paired_end_concordance", pair->concordance);
tagStanzaAddDouble(tagStorm, stanza, "paired_end_distance_mean", pair->distanceMean);
tagStanzaAddDouble(tagStorm, stanza, "paired_end_distance_min", pair->distanceMin);
tagStanzaAddDouble(tagStorm, stanza, "paired_end_distance_max", pair->distanceMax);
tagStanzaAddDouble(tagStorm, stanza, "paired_end_distance_std", pair->distanceStd);
}

void addRepeatInfo(struct sqlConnection *conn, struct tagStorm *tagStorm,
    struct rbTree *fileTree, char *repeatClass, char *label)
/* Add info for given repeat class to tag storm. */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwQaRepeat where repeatClass='%s'",
         repeatClass);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwQaRepeat rep;
    cdwQaRepeatStaticLoad(row, &rep);
    struct tagStanza *stanza = intValTreeFind(fileTree, rep.fileId);
    if (stanza != NULL)
	{
	char tagName[128];
	safef(tagName, sizeof(tagName), "map_to_%s", label);
	tagStanzaAddDouble(tagStorm, stanza, tagName, rep.mapRatio);
	}
    }
sqlFreeResult(&sr);
}


void addContamInfo(struct sqlConnection *conn, struct tagStorm *tagStorm,
    struct rbTree *fileTree, int taxon, char *label)
/* Add info for given contamination mapping to tag storm. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select cdwQaContam.* from cdwQaContam,cdwQaContamTarget,cdwAssembly "
    "where cdwQaContam.qaContamTargetId = cdwQaContamTarget.id "
    "and cdwQaContamTarget.assemblyId = cdwAssembly.id "
    "and taxon=%d", taxon);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwQaContam item;
    cdwQaContamStaticLoad(row, &item);
    struct tagStanza *stanza = intValTreeFind(fileTree, item.fileId);
    if (stanza != NULL)
	{
	char tagName[128];
	safef(tagName, sizeof(tagName), "map_to_%s", label);
	tagStanzaAddDouble(tagStorm, stanza, tagName, item.mapRatio);
	}
    }
sqlFreeResult(&sr);
}

static void addReadSizeInfo(struct tagStorm *tagStorm, struct tagStanza *stanza, 
    double readSizeMin, double readSizeMax, double readSizeMean, double readSizeStd)
/* Store read size info in stanza */
{
if (readSizeMin == readSizeMax)
    tagStanzaAddLongLong(tagStorm, stanza, "read_size", readSizeMin);
else
    {
    tagStanzaAddDouble(tagStorm, stanza, "read_size_mean", readSizeMean);
    tagStanzaAddDouble(tagStorm, stanza, "read_size_min", readSizeMin);
    tagStanzaAddDouble(tagStorm, stanza, "read_size_max", readSizeMax);
    tagStanzaAddDouble(tagStorm, stanza, "read_size_std", readSizeStd);
    }
}

struct tagStorm *cdwTagStormRestricted(struct sqlConnection *conn, struct rbTree *restrictTo)
/* Load  cdwMetaTags.tags, cdwFile.tags, and select other fields into a tag
 * storm for searching */
{
/* Build up a cache of cdwSubmitDir */
char query[512];
sqlSafef(query, sizeof(query), "select * from cdwSubmitDir");
struct cdwSubmitDir *dir, *dirList = cdwSubmitDirLoadByQuery(conn, query);
struct rbTree *submitDirTree = intValTreeNew();
for (dir = dirList; dir != NULL; dir = dir->next)
   intValTreeAdd(submitDirTree, dir->id, dir);
verbose(2, "cdwTagStorm: %d items in submitDirTree\n", submitDirTree->n);

/* First pass through the cdwMetaTags table.  Make up a high level stanza for each
 * row, and save a reference to it in metaTree. */
struct tagStorm *tagStorm = tagStormNew("constructed from cdwMetaTags and cdwFile");
struct rbTree *metaTree = intValTreeNew();
sqlSafef(query, sizeof(query), "select id,tags from cdwMetaTags");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned id = sqlUnsigned(row[0]);
    char *cgiVars = row[1];
    struct tagStanza *stanza = tagStanzaNew(tagStorm, NULL);
    char *var, *val;
    while (cgiParseNext(&cgiVars, &var, &val))
	 {
         tagStanzaAdd(tagStorm, stanza, var, val);
	 }
    intValTreeAdd(metaTree, id, stanza);
    }
sqlFreeResult(&sr);
verbose(2, "cdwTagStorm: %d items in metaTree\n", metaTree->n);


/* Now go through the cdwFile table, adding its files as substanzas to 
 * meta cdwMetaTags stanzas. */
sqlSafef(query, sizeof(query), 
    "select cdwFile.*,cdwValidFile.* from cdwFile,cdwValidFile "
    "where cdwFile.id=cdwValidFile.fileId "
    "and (errorMessage='' or errorMessage is null)"
    );
sr = sqlGetResult(conn, query);
struct rbTree *fileTree = intValTreeNew();
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwFile cf;
    struct cdwValidFile vf;
    cdwFileStaticLoad(row, &cf);
    cdwValidFileStaticLoad(row + CDWFILE_NUM_COLS, &vf);

    if (restrictTo != NULL && intValTreeFind(restrictTo, cf.id) == NULL)
        continue;

    /* Figure out file name independent of cdw location */
    char name[FILENAME_LEN], extension[FILEEXT_LEN];
    splitPath(cf.cdwFileName, NULL, name, extension);
    char fileName[PATH_LEN];
    safef(fileName, sizeof(fileName), "%s%s", name, extension);


    struct tagStanza *metaStanza = intValTreeFind(metaTree, cf.metaTagsId);
    struct tagStanza *stanza = tagStanzaNew(tagStorm, metaStanza);
    intValTreeAdd(fileTree, cf.id, stanza);

    /** Add stuff we want in addition to what is in cf.tags */

    /* Deprecated is important to know */
    if (!isEmpty(cf.deprecated))
	tagStanzaAdd(tagStorm, stanza, "deprecated", cf.deprecated);

    /* Basic file name info */
    tagStanzaAdd(tagStorm, stanza, "file_name", fileName);
    tagStanzaAddLongLong(tagStorm, stanza, "file_size", cf.size);
    tagStanzaAdd(tagStorm, stanza, "md5", cf.md5);
    tagStanzaAddLongLong(tagStorm, stanza, "file_id", cf.id);

    /* Stuff gathered from submission */
    tagStanzaAdd(tagStorm, stanza, "submit_file_name", cf.submitFileName);
    struct cdwSubmitDir *dir = intValTreeFind(submitDirTree, cf.submitDirId);
    if (dir != NULL)
	{
	tagStanzaAdd(tagStorm, stanza, "submit_dir", dir->url);
	}

    safef(fileName, sizeof(fileName), "%s%s", cdwRootDir, cf.cdwFileName);
    tagStanzaAdd(tagStorm, stanza, "cdw_file_name", fileName);
    tagStanzaAdd(tagStorm, stanza, "accession", vf.licensePlate);
    if (vf.itemCount != 0)
	tagStanzaAddLongLong(tagStorm, stanza, "item_count", vf.itemCount);
    if (vf.depth != 0)
	tagStanzaAddDouble(tagStorm, stanza, "seq_depth", vf.depth);
    if (vf.mapRatio != 0)
	tagStanzaAddDouble(tagStorm, stanza, "map_ratio", vf.mapRatio);

    /* Add tag field */
    char *cgiVars = cf.tags;
    char *var,*val;
    while (cgiParseNext(&cgiVars, &var, &val))
	tagStanzaAdd(tagStorm, stanza, var, val);

    /* Compute fields based on other fields */
    // TODO - calculate days_after_conception here
    
    }
sqlFreeResult(&sr);
verbose(2, "cdwTagStorm: %d items in fileTree\n", fileTree->n);

/** Add selected tags from other tables as part of stanza tags. */

/* Add cdwFastqFile info */
sqlSafef(query, sizeof(query), "select * from cdwFastqFile");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwFastqFile *fq = cdwFastqFileLoad(row);
    struct tagStanza *stanza = intValTreeFind(fileTree, fq->fileId);
    if (stanza != NULL)
	{
	addReadSizeInfo(tagStorm, stanza, fq->readSizeMin, fq->readSizeMax, fq->readSizeMean,
	    fq->readSizeStd);
	tagStanzaAdd(tagStorm, stanza, "fastq_qual_type", fq->qualType);
	tagStanzaAddDouble(tagStorm, stanza, "fastq_qual_mean", fq->qualMean);
	tagStanzaAddDouble(tagStorm, stanza, "at_ratio", fq->atRatio);
	cdwFastqFileFree(&fq);
	}
    }
sqlFreeResult(&sr);

/* Add cdwQaPairedEndFastq */
sqlSafef(query, sizeof(query), "select * from cdwQaPairedEndFastq where recordComplete != 0");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwQaPairedEndFastq pair;
    cdwQaPairedEndFastqStaticLoad(row, &pair);
    struct tagStanza *p1 = intValTreeFind(fileTree, pair.fileId1);
    struct tagStanza *p2 = intValTreeFind(fileTree, pair.fileId2);
    if (p1 != NULL && p2 != NULL)
	{
	char *acc1 = tagFindVal(p1, "accession");
	char *acc2 = tagFindVal(p2, "accession");
	addPairedEndTags(tagStorm, p1, &pair, acc2);
	addPairedEndTags(tagStorm, p2, &pair, acc1);
	}
    }
sqlFreeResult(&sr);

/* Build up in-memory random access data structure for enrichment targets */
struct rbTree *enrichTargetTree = intValTreeNew();
sqlSafef(query, sizeof(query), "select * from cdwQaEnrichTarget");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwQaEnrichTarget *target = cdwQaEnrichTargetLoad(row);
    intValTreeAdd(enrichTargetTree, target->id, target);
    }
sqlFreeResult(&sr);

/* Build up in-memory random access data structure for common_snp enrichments.  We'll
 * use this later in the VCF bits. */
struct rbTree *snpEnrichTree = intValTreeNew();

/* Add cdwQaEnrich - here we'll supply exon, chrY, and whatever they put in their enriched_in
 * data, a subset of all */
sqlSafef(query, sizeof(query), "select * from cdwQaEnrich");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwQaEnrich rich;
    cdwQaEnrichStaticLoad(row, &rich);
    struct cdwQaEnrichTarget *target = intValTreeFind(enrichTargetTree, rich.qaEnrichTargetId);
    if (target != NULL)
        {
	char *targetName = target->name;
	if (sameString(targetName, "common_snp"))
	    {
	    struct cdwQaEnrich *keepRich = cloneMem(&rich, sizeof(rich));
	    intValTreeAdd(snpEnrichTree, keepRich->fileId, keepRich);
	    }
	struct tagStanza *stanza = intValTreeFind(fileTree, rich.fileId);
	if (stanza != NULL)
	    {
	    char *enrichedIn = tagFindVal(stanza, "enriched_in");
	    boolean onTarget = FALSE;
	    if (enrichedIn != NULL && sameWord(enrichedIn, targetName))
	        onTarget = TRUE;
	    else if (sameWord(targetName, "exon"))
	        onTarget = TRUE;
	    else if (sameWord(targetName, "promoter"))
	        onTarget = TRUE;
	    else if (sameWord(targetName, "chrX"))
	        onTarget = TRUE;
	    else if (sameWord(targetName, "chrY"))
	        onTarget = TRUE;
	    if (onTarget)
	        {
		char tagName[128];
		safef(tagName, sizeof(tagName), "enrichment_%s", targetName);
		if (!tagFindVal(stanza, tagName))
		    tagStanzaAddDouble(tagStorm, stanza, tagName, rich.enrichment);
		}
	    }
	}
    else
        verbose(2, "Missing on qaEnrichTargetId %u\n", rich.qaEnrichTargetId);
    }
sqlFreeResult(&sr);

/* Add cdwQaRepeat - here we'll supply rRNA, and total mapRatio out of the table */
addRepeatInfo(conn, tagStorm, fileTree, "rRNA", "ribosome");
addRepeatInfo(conn, tagStorm, fileTree, "total", "repeat");

/* Add contamination targets */
addContamInfo(conn, tagStorm, fileTree, 9606, "human");
addContamInfo(conn, tagStorm, fileTree, 10090, "mouse");
addContamInfo(conn, tagStorm, fileTree, 10116, "rat");
addContamInfo(conn, tagStorm, fileTree, 7227, "fly");
addContamInfo(conn, tagStorm, fileTree, 559292, "yeast");
addContamInfo(conn, tagStorm, fileTree, 6239, "worm");
addContamInfo(conn, tagStorm, fileTree, 562, "ecoli");

/* Add info from cdwBamFile */
sqlSafef(query, sizeof(query), "select * from cdwBamFile");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwBamFile bam;
    cdwBamFileStaticLoad(row, &bam);
    struct tagStanza *stanza = intValTreeFind(fileTree, bam.fileId);
    if (stanza != NULL)
	{
	tagStanzaAddLongLong(tagStorm, stanza, "paired_end_reads", bam.isPaired);
	tagStanzaAddLongLong(tagStorm, stanza, "sorted_by_target", bam.isSortedByTarget);
	addReadSizeInfo(tagStorm, stanza, bam.readSizeMin, bam.readSizeMax, bam.readSizeMean,
	    bam.readSizeStd);
	tagStanzaAddDouble(tagStorm, stanza, "u4m_unique_ratio", bam.u4mUniqueRatio);
	tagStanzaAddLongLong(tagStorm, stanza, "map_target_base_count", bam.targetBaseCount);
	tagStanzaAddLongLong(tagStorm, stanza, "map_target_seq_count", bam.targetSeqCount);
	}
    }
sqlFreeResult(&sr);

/* Add info from cdwVcfFile */
sqlSafef(query, sizeof(query), "select * from cdwVcfFile");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwVcfFile vcf;
    cdwVcfFileStaticLoad(row, &vcf);
    struct tagStanza *stanza = intValTreeFind(fileTree, vcf.fileId);
    if (stanza != NULL)
	{
	tagStanzaAddLongLong(tagStorm, stanza, "vcf_genotype_count", vcf.genotypeCount);
	tagStanzaAddDouble(tagStorm, stanza, "vcf_pass_ratio", vcf.passRatio);
	tagStanzaAddDouble(tagStorm, stanza, "vcf_snp_ratio", vcf.snpRatio);
	tagStanzaAddLongLong(tagStorm, stanza, "vcf_genotype_count", vcf.genotypeCount);
	if (vcf.haploidCount > 0)
	    tagStanzaAddDouble(tagStorm, stanza, "vcf_haploid_ratio", vcf.haploidRatio);
	if (vcf.phasedCount > 0)
	    tagStanzaAddDouble(tagStorm, stanza, "vcf_phased_ratio", vcf.phasedRatio);
	if (vcf.gotDepth)
	    tagStanzaAddDouble(tagStorm, stanza, "vcf_dp", vcf.depthMean);
	struct cdwQaEnrich *commonEnrich = intValTreeFind(snpEnrichTree, vcf.fileId);
	if (commonEnrich != NULL)
	    {
	    // Attempt to calculate coverage of data set by common snps
	    double commonCov = (double)commonEnrich->targetBaseHits / vcf.sumOfSizes;
	    tagStanzaAddDouble(tagStorm, stanza, "vcf_common_snp_ratio", commonCov);
	    }
	}
    }
sqlFreeResult(&sr);

/* Clean up and go home */
rbTreeFree(&submitDirTree);
rbTreeFree(&metaTree);
rbTreeFree(&fileTree);
tagStormReverseAll(tagStorm);
return tagStorm;
}

struct tagStorm *cdwTagStorm(struct sqlConnection *conn)
/* Load  cdwMetaTags.tags, cdwFile.tags, and select other fields into a tag
 * storm for searching */
{
return cdwTagStormRestricted(conn, NULL);
}

struct tagStorm *cdwUserTagStormFromList(struct sqlConnection *conn, 
    struct cdwUser *user, struct cdwFile *validList ,struct rbTree *groupedFiles)
/* Return tag storm just for files user has access to with the list
 * of validated files and the list of files the user shares group rights to
 * already calculated */
{
struct rbTree *accessTree = cdwAccessTreeForUser(conn, user, validList, groupedFiles);
struct tagStorm *tags = cdwTagStormRestricted(conn, accessTree);
rbTreeFree(&accessTree);
return tags;
}

struct tagStorm *cdwUserTagStorm(struct sqlConnection *conn, struct cdwUser *user)
/* Return tag storm just for files user has access to. */
{
struct cdwFile *validList = cdwFileLoadAllValid(conn);
int userId = 0;
if (user != NULL)
    userId = user->id;
struct rbTree *groupedFiles = cdwFilesWithSharedGroup(conn, userId);
struct tagStorm *tags = cdwUserTagStormFromList(conn, user, validList, groupedFiles);
rbTreeFree(&groupedFiles);
cdwFileFreeList(&validList);
return tags;
}
