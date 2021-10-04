/* Objects to find genePreds given similarity in genome bases annotated.  This
 * is used to relate genes between two gene sets. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "geneSimilarities.h"
#include "genePred.h"
#include "genePredReader.h"
#include "jksql.h"

static struct geneSim *geneSimNew(struct genePred *gene, float sim)
/* construct a new geneSim */
{
struct geneSim *gs;
AllocVar(gs);
gs->gene = gene;
gs->sim = sim;
return gs;
}

static void geneSimFreeList(struct geneSim **gss)
/* free list of geneSims and associated genePreds */
{
struct geneSim *gs;
while ((gs = slPopHead(gss)) != NULL)
    {
    genePredFree(&gs->gene);
    freeMem(gs);
    }
}

struct geneSimilarities *geneSimilaritiesNew(struct genePred *query,
                                             boolean cdsSim)
/* construct a new geneSimilarities object. Ownership of query is passed. */
{
struct geneSimilarities *gss;
AllocVar(gss);
gss->query = query;
gss->cdsSim = cdsSim;
return gss;
}

void geneSimilaritiesFree(struct geneSimilarities **gssPtr)
/* free a geneSimilarities object and all contained genePreds */
{
struct geneSimilarities *gss = *gssPtr;
if (gss != NULL)
    {
    geneSimFreeList(&gss->genes);
    genePredFree(&gss->query);
    freez(gssPtr);
    }
}

void geneSimilaritiesFreeList(struct geneSimilarities **gssList)
/* free a list of geneSimilarities objects and all contained genePreds */
{
struct geneSimilarities *gss;
while ((gss = slPopHead(gssList)) != NULL)
    geneSimilaritiesFree(&gss);
}
                                             
static int cntOverlapCdsBases(struct genePred *ga, struct genePred *gb)
/* determine the number of CDS bases overlaping */
{
int overBases = 0, ia, ib;
int cdsStartA, cdsEndA, cdsStartB, cdsEndB;

for (ia = 0; ia < ga->exonCount; ia++)
    {
    if (genePredCdsExon(ga, ia, &cdsStartA, &cdsEndA))
        {
        for (ib = 0; ib < gb->exonCount; ib++)
            {
            if (genePredCdsExon(gb, ib, &cdsStartB, &cdsEndB))
                {
                int o = min(cdsEndA, cdsEndB) - max(cdsStartA, cdsStartB);
                if (o > 0)
                    overBases += o;
                }
            }
        }
    }
return overBases;
}

static float geneCdsSimilarity(struct genePred *ga, struct genePred *gb)
/* computer a CDS similary score for two genes, which is a fraction */
{
int gaBases = genePredCodingBases(ga);
int gbBases = genePredCodingBases(gb);
return ((float)(2*cntOverlapCdsBases(ga, gb)))/(float)(gaBases+gbBases);
}

static int cntOverlapExonBases(struct genePred *ga, struct genePred *gb)
/* determine the number of exon bases overlaping */
{
int overBases = 0, ia, ib;
for (ia = 0; ia < ga->exonCount; ia++)
    {
    for (ib = 0; ib < gb->exonCount; ib++)
        {
        int o = min(ga->exonEnds[ia], gb->exonEnds[ib])
            - max(ga->exonStarts[ia], gb->exonStarts[ib]);
        if (o > 0)
            overBases += o;
        }
    }
return overBases;
}

static float geneExonSimilarity(struct genePred *ga, struct genePred *gb)
/* computer an exon similary score for two genes, which is a fraction */
{
int gaBases = genePredBases(ga);
int gbBases = genePredBases(gb);
return ((float)(2*cntOverlapExonBases(ga, gb)))/(float)(gaBases+gbBases);
}

int geneSimCmp(const void *va, const void *vb)
/* compare geneSim objects by descending similarity score. */
{
struct geneSim *ga = *((struct geneSim **)va);
struct geneSim *gb = *((struct geneSim **)vb);

if (ga->sim > gb->sim)
    return -1;
else if (ga->sim < gb->sim)
    return -1;
else
    return 0;
}

static void findSimilarities(struct geneSimilarities *gss,
                             struct sqlConnection *conn,
                             char *targetGeneTbl)
/* find genes similar to the query in gss and add them. */
{
char where[64];
sqlSafefFrag(where, sizeof(where), "strand = \"%s\"", gss->query->strand);
int start = gss->cdsSim ? gss->query->cdsStart :gss->query->txStart;
int end = gss->cdsSim ? gss->query->cdsEnd :gss->query->txEnd;

struct genePred *targetGenes
    = genePredReaderLoadRangeQuery(conn, targetGeneTbl, gss->query->chrom,
                                   start, end, where);
struct genePred *gp;
while ((gp = slPopHead(&targetGenes)) != NULL)
    {
    float sim = gss->cdsSim
        ? geneCdsSimilarity(gp, gss->query)
        : geneExonSimilarity(gp, gss->query);
    slSafeAddHead(&gss->genes, geneSimNew(gp, sim));
    }
}

struct geneSimilarities *geneSimilaritiesBuild(struct sqlConnection *conn,
                                               boolean cdsSim,
                                               struct genePred *query,
                                               char *targetGeneTbl)
/* Construct a geneSimilarities object by finding all similar genes in
 * the specified table. Ownership of query is passed. */
{
struct geneSimilarities *gss = geneSimilaritiesNew(query, cdsSim);
findSimilarities(gss, conn, targetGeneTbl);
slSort(&gss->genes, geneSimCmp);
return gss;
}

struct geneSimilarities *geneSimilaritiesBuildAt(struct sqlConnection *conn,
                                                 boolean cdsSim,
                                                 char *queryName,
                                                 char *queryChrom,
                                                 int queryStart,
                                                 char *queryGeneTbl,
                                                 char *targetGeneTbl)
/* Construct a geneSimilarities object by finding the genePred named
 * queryName with txStart of queryStart in queryGeneTbl and
 * find similar genes in targetGeneTbl. */
{
char where[256];
sqlSafefFrag(where, sizeof(where), "(chrom = \"%s\") and (txStart = %d)", queryChrom, queryStart);
struct genePred *query = genePredReaderLoadQuery(conn, queryGeneTbl, where);
if (query == NULL)
    errAbort("gene %s starting at %s:%d not found in %s", queryName,
             queryChrom, queryStart, queryGeneTbl);

// only use the first if by some wierd reason multiple are found
genePredFreeList(&query->next);

return geneSimilaritiesBuild(conn, cdsSim, query, targetGeneTbl);
}

struct geneSimilarities *geneSimilaritiesBuildAll(struct sqlConnection *conn,
                                                  boolean cdsSim, 
                                                  char *queryName,
                                                  char *queryGeneTbl,
                                                  char *targetGeneTbl)
/* Construct a list of geneSimilarities objects by finding all entries
 * of queryName in queryGeneTbl and build a geneSimilarities object for each
 * one from targetGeneTbl. */
{
struct geneSimilarities *gssList = NULL;
char where[64];
sqlSafefFrag(where, sizeof(where), "name = \"%s\"", queryName);
struct genePred *queries = genePredReaderLoadQuery(conn, queryGeneTbl, where);
struct genePred *query;
while ((query = slPopHead(&queries)) != NULL)
    slSafeAddHead(&gssList, geneSimilaritiesBuild(conn, cdsSim, query, targetGeneTbl));
return gssList;
}

