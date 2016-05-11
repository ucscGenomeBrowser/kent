/* Create a PNG file with boxplot of gene expression 
 *      for GTEx (Genotype Tissue Expression) data. */

/* Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "trashDir.h"
#include "gtexInfo.h"
#include "gtexTissue.h"
#include "gtexSampleData.h"

struct tissueSampleVals
/* RPKM expression values for multiple samples */
/* (Data structure shared by different implementations of boxplot (e.g. C, JS),
 * later abandoned) */
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

static struct tissueSampleVals *getTissueSampleVals(char *geneId, boolean doLogTransform,
                                                char *version, double *maxValRet)
/* Get sample data for the gene.  Optionally log10 it. Return maximum value seen */
{
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
safef(buf, sizeof(buf), "%s%s", sampleDataTable, gtexVersionSuffixFromVersion(version));
struct sqlConnection *conn = hAllocConn("hgFixed");
assert(sqlTableExists(conn, buf));
sqlSafef(query, sizeof(query), "select * from %s where geneId='%s'", buf, geneId);
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
struct gtexTissue *tis = NULL, *tissues = gtexGetTissues(version);
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

/********************************************************/
/* R implementation of GTEx boxplot.  This invokes R interpreter on an R script */

static boolean drawGtexRBoxplot(char *geneName, struct tissueSampleVals *tsvList,
                        boolean doLogTransform, char *version, struct tempName *pngTn)
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
    // remove trailing parenthesized phrases as not worth label length
    chopSuffixAt(tsv->description, '(');
    for (i=0; i<count; i++)
        fprintf(f, "%d\t%s\t%0.3f\n", sampleId++, tsv->description, tsv->vals[i]);
    }
fclose(f);

// Plot to PNG file
if (!pngTn)
    return FALSE;
trashDirFile(pngTn, "hgc", "gtexGene", ".png");
char cmd[256];

/* Exec R in quiet mode, without reading/saving environment or workspace */
safef(cmd, sizeof(cmd), "Rscript --vanilla --slave hgcData/gtexBoxplot.R %s %s %s %s %s %s",  
                                geneName, dfTn.forCgi, pngTn->forHtml, 
                                doLogTransform ? "log=TRUE" : "log=FALSE", "order=alpha", version);
//NOTE: use "order=score" to order bargraph by median RPKM, descending

int ret = system(cmd);
if (ret == 0)
    return TRUE;
return FALSE;
}

boolean gtexGeneBoxplot(char *geneId, char *geneName, char *version, 
                                boolean doLogTransform, struct tempName *pngTn)
/* Create a png temp file with boxplot of GTEx expression values for this gene. 
 * GeneId is the Ensembl gene ID.  GeneName is the HUGO name, used for graph title;
 * If NULL, label with the Ensembl gene ID */
{
struct tissueSampleVals *tsvs;
tsvs  = getTissueSampleVals(geneId, doLogTransform, version, NULL);
char *label = geneName ? geneName : geneId;
return drawGtexRBoxplot(label, tsvs, doLogTransform, version, pngTn);
}

