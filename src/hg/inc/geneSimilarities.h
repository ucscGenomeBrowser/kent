/* Objects to find genePreds given similarity in genome bases annotated.  This
 * is used to relate genes between two gene sets. */

/* Copyright (C) 2008 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef GENESIMILARITIES_H
#define GENESIMILARITIES_H
struct genePred;
struct sqlConnection;

struct geneSim
/* a target hit and it's similarity to the query */
{
    struct geneSim *next;
    struct genePred *gene;   // target gene
    float sim;               // similarity score (fraction)
};


struct geneSimilarities
/* a genePred and associated genes and their similarity, computed by CDS or all exon
 * alignments */
{
    struct geneSimilarities *next;
    struct genePred *query;        // the query genePred
    boolean cdsSim;                // CDS or exon similarity?
    struct geneSim *genes;         // similar genes, sorted in descending order
};

struct geneSimilarities *geneSimilaritiesNew(struct genePred *query,
                                             boolean cdsSim);
/* construct a new geneSimilarities object. Ownership of query is passed. */

void geneSimilaritiesFree(struct geneSimilarities **gssPtr);
/* free a geneSimilarities object and all contained genePreds */

void geneSimilaritiesFreeList(struct geneSimilarities **gssList);
/* free a list of geneSimilarities objects and all contained genePreds */

int geneSimCmp(const void *va, const void *vb);
/* compare geneSim objects by descending similarity score. */

struct geneSimilarities *geneSimilaritiesBuild(struct sqlConnection *conn,
                                               boolean cdsSim,
                                               struct genePred *query,
                                               char *targetGeneTbl);
/* Construct a geneSimilarities object by finding all similar genes in
 * the specified table. Ownership of query is passed. */

struct geneSimilarities *geneSimilaritiesBuildAt(struct sqlConnection *conn,
                                                 boolean cdsSim,
                                                 char *queryName,
                                                 char *queryChrom,
                                                 int queryStart,
                                                 char *queryGeneTbl,
                                                 char *targetGeneTbl);
/* Construct a geneSimilarities object by finding the genePred named
 * queryName with txStart of queryStart in queryGeneTbl and
 * find similar genes in targetGeneTbl. */

struct geneSimilarities *geneSimilaritiesBuildAll(struct sqlConnection *conn,
                                                  boolean cdsSim,
                                                  char *queryName,
                                                  char *queryGeneTbl,
                                                  char *targetGeneTbl);
/* Construct a list of geneSimilarities objects by finding all entries
 * of queryName in queryGeneTbl and build a geneSimilarities object for each
 * one from targetGeneTbl. */

#endif
