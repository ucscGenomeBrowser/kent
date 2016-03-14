/* Details pages for GTEx tracks */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "hvGfx.h"
#include "trashDir.h"
#include "hgc.h"
#include "hCommon.h"

#include "gtexGeneBed.h"
#include "gtexTissue.h"
#include "gtexSampleData.h"
#include "gtexUi.h"
#include "gtexInfo.h"

struct tissueSampleVals
/* RPKM expression values for multiple samples */
    {
    struct tissueSampleVals *next;
    char *name;         /* GTEx tissue name */
    char *description;  /* GTEx tissue description */
    int color;          /* GTEx tissue color */
    int count;          /* number of samples */
    double *vals;       /* RPKM values */
    double min, max;    /* minimum, maximum value */
    double q1, median, q3;      /* quartiles */
    struct slDouble *valList;   /* used to create val array */
    };

char *gencodeTranscriptClassColorCode(char *transcriptClass)
/* Get HTML color code used by GENCODE for transcript class
 * WARNING: should share code with transcript color handling in hgTracks */
{
char *unknown = "#010101";
if (transcriptClass == NULL)
    return unknown;
if (sameString(transcriptClass, "coding"))
    return "#0C0C78";
if (sameString(transcriptClass, "nonCoding"))
    return "#006400";
if (sameString(transcriptClass, "pseudo"))
    return "#FF33FF";
if (sameString(transcriptClass, "problem"))
    return "#FE0000";
return unknown;
}

/********************************************************/
/* R implementation.  Invokes R script */

void drawGtexRBoxplot(struct gtexGeneBed *gtexGene, struct tissueSampleVals *tsvList,
                        boolean doLogTransform, char *version)
/* Draw a box-and-whiskers plot from GTEx sample data, using R boxplot */
{
/* Create R data frame.  This is a tab-sep file, one row per sample, 
 * with columns for sample, tissue, rpkm */
struct tempName dfTn;
trashDirFile(&dfTn, "hgc", "gtexGene", ".df.txt");
FILE *f = fopen(dfTn.forCgi, "w");
if (f == NULL)
    errAbort("can't create temp file %s", dfTn.forCgi);
fprintf(f, "sample\ttissue\trpkm\n");
struct tissueSampleVals *tsv;
int sampleId=1;
int i;
for (tsv = tsvList; tsv != NULL; tsv = tsv->next)
    {
    int count = tsv->count;
    for (i=0; i<count; i++)
        fprintf(f, "%d\t%s\t%0.3f\n", sampleId++, tsv->name, tsv->vals[i]);
    }
fclose(f);

// Plot to PNG file
struct tempName pngTn;
trashDirFile(&pngTn, "hgc", "gtexGene", ".png");
char cmd[256];

/* Exec R in quiet mode, without reading/saving environment or workspace */
safef(cmd, sizeof(cmd), "Rscript --vanilla --slave hgcData/gtexBoxplot.R %s %s %s %s %s %s",  
                                gtexGene->name, dfTn.forCgi, pngTn.forHtml, 
                                doLogTransform ? "log=TRUE" : "log=FALSE", "order=alpha", version);
//NOTE: use "order=score" to order bargraph by median RPKM, descending

int ret = system(cmd);
if (ret == 0)
    {
    printf("<IMG SRC = \"%s\" BORDER=1><BR>\n", pngTn.forHtml);
    //printf("<IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d><BR>\n",
                    //pngTn.forHtml, imageWidth, imageHeight);
                    //pngTn.forHtml, 900, 500);
    }
}

struct gtexGeneBed *getGtexGene(char *item, char *table)
/* Retrieve gene info for this item from the main track table */
{
struct gtexGeneBed *gtexGene = NULL;
struct sqlConnection *conn = hAllocConn(database);
char **row;
char query[512];
struct sqlResult *sr;
if (sqlTableExists(conn, table))
    {
    sqlSafef(query, sizeof(query), "select * from %s where name = '%s'", table, item);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        gtexGene = gtexGeneBedLoad(row);
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return gtexGene;
}

struct tissueSampleVals *getTissueSampleVals(struct gtexGeneBed *gtexGene, boolean doLogTransform,
                                                char *version, double *maxValRet)
/* Get sample data for the gene.  Optionally log10 it. Return maximum value seen */
{
// TODO: support version table name.  Likely move to lib.
struct hash *tsHash = hashNew(0);
struct tissueSampleVals *tsv;
struct hashEl *hel;
struct slDouble *val;
double maxVal = 0;
struct gtexSampleData *sd = NULL;
char query[256];
char **row;
char buf[256];
char *sampleDataTable = "gtexSampleData";
safef(buf, sizeof(buf), "%s%s", sampleDataTable, version);
struct sqlConnection *conn = hAllocConn("hgFixed");
assert(sqlTableExists(conn, buf));
sqlSafef(query, sizeof(query), "select * from %s where geneId='%s'", 
                buf, gtexGene->geneId);
struct sqlResult *sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sd = gtexSampleDataLoad(row);
    if ((hel = hashLookup(tsHash, sd->tissue)) == NULL)
        {
        AllocVar(tsv);
        hashAdd(tsHash, sd->tissue, tsv);
        }
    else
        tsv = (struct tissueSampleVals *)hel->val;
    maxVal = max(maxVal, sd->score);
    val = slDoubleNew(sd->score);
    slAddHead(&tsv->valList, val);
    }
/*  Fill in tissue descriptions, fill values array and calculate stats for plotting
        Then make a list, suitable for sorting by tissue or score
    NOTE: Most of this not needed for R implementation */
struct gtexTissue *tis = NULL, *tissues = gtexGetTissues();
struct tissueSampleVals *tsList = NULL;
int i;
if (doLogTransform)
    maxVal = log10(maxVal+1.0);
for (tis = tissues; tis != NULL; tis = tis->next)
    {
    tsv = hashFindVal(tsHash, tis->name);
    if (tsv == NULL)
        {
        /* no non-zero values for this tissue/gene */
        AllocVar(tsv);
        val = slDoubleNew(0.0);
        slAddHead(&tsv->valList, val);
        }
    tsv->name = tis->name;
    tsv->description = tis->description;
    tsv->color = tis->color;
    int count = tsv->count = slCount(tsv->valList);
    double *vals = AllocArray(tsv->vals, count);
    for (i=0; i<count; i++)
        {
        val = slPopHead(&tsv->valList);
        if (doLogTransform)
            vals[i] = log10(val->val+1.0);
        else
            vals[i] = val->val;
        }
    doubleBoxWhiskerCalc(tsv->count, tsv->vals, 
                                &tsv->min, &tsv->q1, &tsv->median, &tsv->q3, &tsv->max);
    slAddHead(&tsList, tsv);
    }
if (maxValRet != NULL)
    *maxValRet = maxVal;
return tsList;
}

char *getGeneDescription(struct gtexGeneBed *gtexGene)
/* Get description for gene. Needed because knownGene table semantics have changed in hg38 */
{
char query[256];
if (sameString(database, "hg38"))
    {
    char *geneId = cloneString(gtexGene->geneId);
    chopSuffix(geneId);
    sqlSafef(query, sizeof(query), 
        "select kgXref.description from kgXref, knownCanonical where knownCanonical.protein like '%%%s%%' and knownCanonical.transcript=kgXref.kgID", geneId);
    }
else
    {
    sqlSafef(query, sizeof(query), 
                "select kgXref.description from kgXref where geneSymbol='%s'", gtexGene->name);
    }
struct sqlConnection *conn = hAllocConn(database);
char *desc = sqlQuickString(conn, query);
hFreeConn(&conn);
return desc;
}

void doGtexGeneExpr(struct trackDb *tdb, char *item)
/* Details of GTEx gene expression item */
{
struct gtexGeneBed *gtexGene = getGtexGene(item, tdb->table);
if (gtexGene == NULL)
    errAbort("Can't find gene %s in GTEx gene table %s\n", item, tdb->table);

genericHeader(tdb, item);
printf("<b>Gene: </b>");
char *desc = getGeneDescription(gtexGene);
if (desc == NULL)
    printf("%s<br>\n", gtexGene->name);
else
    {
    printf("<a target='_blank' href='%s?db=%s&hgg_gene=%s'>%s</a><br>\n", 
                        hgGeneName(), database, gtexGene->name, gtexGene->name);
    printf("<b>Description:</b> %s<br>\n", desc);
    }
printf("<b>Ensembl Gene ID:</b> %s<br>\n", gtexGene->geneId);
// The actual transcript model is a union, so this identification is approximate
// (used just to find a transcript class)
//printf("<b>Ensembl Transcript ID:</b> %s<br>\n", gtexGene->transcriptId);
printf("<b>Ensembl Class: </b><span style='color: %s'>%s</span><br>\n", 
            gencodeTranscriptClassColorCode(gtexGene->transcriptClass), gtexGene->transcriptClass);
printf("<b>Genomic Position: </b><a href='%s&db=%s&position=%s%%3A%d-%d'>%s:%d-%d</a><br>\n", 
                        hgTracksPathAndSettings(), database, 
                        gtexGene->chrom, gtexGene->chromStart+1, gtexGene->chromEnd,
                        gtexGene->chrom, gtexGene->chromStart+1, gtexGene->chromEnd);
printf("<a target='_blank' href='http://www.gtexportal.org/home/gene/%s'>View at GTEx portal</a><br>\n", gtexGene->geneId);
puts("<p>");

boolean doLogTransform = cartUsualBooleanClosestToHome(cart, tdb, FALSE, GTEX_LOG_TRANSFORM,
                                                GTEX_LOG_TRANSFORM_DEFAULT);
double maxVal = 0.0;
char *versionSuffix = gtexVersionSuffix(tdb->table);
struct tissueSampleVals *tsvs = getTissueSampleVals(gtexGene, doLogTransform, 
                                                        versionSuffix, &maxVal);
char *version = gtexVersion(tdb->table);
drawGtexRBoxplot(gtexGene, tsvs, doLogTransform, version);

printTrackHtml(tdb);
}
