/** 
 \page altSplice altSplice - construct alt-splicing graphs using est and mrna alignments.

 \brief Construct altGraphX structures for genePredictions using information in est and mrna alignments.

 <p><b>usage:</b> <pre>altSplice <db> <outputFile.AltGraph.tab> <optional:bedFile></pre>

 <p> Mar 16, 2002: altSplice.c uses code from Jim's geneGraph.h
 library to cluster mrna's and ests onto genomic sequence and try to
 determine various types of alternative splicing. Currently cassette
 exons are particularly highlighted. Only spliced ests are used, as
 they tend to be less noisy and are also easier to orient correctly
 using splice sites from genomic sequence.

 <p>The basic algorithm is as follows: 
 
 - Load refGene table from chromosome 22, or any specified bed file, 
 into a genePred structure.

 - Sort the genePred's by size, smallest first. This is to avoid 
 doubling up on small genes that overlap or are internal to another
 gene.

 - For each genePred determine alternative splicing as follows:
 
    - Load all mrna and est alignments for that portion of genomic sequence.

    - Throw away mrnas and ests that don't have at least their first
    and last exon within the genePred txStart and txEnd. For example
    <pre>
    1) ---------------########-----#######------######----------- genePred
    2) -------------##########-----#######------########--------- est1 alignment 
    3) --#####-----------------------------####-------------##### est2 alignment
    </pre>
    est1 alignment would be kept but est2 would be removed from
    consideration.  Thus the 5' and '3 UTRs can grow using est/mrna
    evidence but not the number of exons.

    - The genePred boundaries are extended to include the most upstream and
    most downstream alignments.

    - Dna from genome is cut out and used to orient ests using splice 
    sites, gt->ag, and less the common gc->ag.

    - The psl alignments are converted to ggMrnaAli structures and in the
    process inserts/deletions smaller than 5bp are removed. 

    - These ggMrnaAli structures are converted to ggMrnaInput
    structures and clustered together into ggMrnaCluster structures.

    - The largest ggMrnaCluster structure is used to create a geneGraph.
    Where each splice site and/or exon boundary forms a vertex and the
    introns and exons are represented by edges. The supporting mrna/est
    evidence for each edge is remembered for use later.

    - The geneGraph is converted to an altGraphX structre for storage
    into a database. In this process unused vertices are removed, vertices
    are sorted by genomic position, and cassette exons are flagged.

 */
#include "common.h"
#include "altGraphX.h"
#include "geneGraph.h"
#include "genePred.h"
#include "ggMrnaAli.h"
#include "psl.h"
#include "dnaseq.h"
#include "hdb.h"
#include "jksql.h"
#include "bed.h"
#include "nib.h"
#include "options.h"
#include "binRange.h"
#include "obscure.h"
#include "errabort.h"
#define USUAL
//#define AFFYSPLICE
static char const rcsid[] = "$Id: altSplice.c,v 1.30 2008/09/03 19:18:14 markd Exp $";

int cassetteCount = 0; /* Number of cassette exons counted. */
int misSense = 0;      /* Number of cassette exons that would introduce a missense mutation. */
int clusterCount = 0;  /* Number of gene clusters identified. */
struct hash *uniqPos = NULL; /* Hash to make sure we're not outputting doubles. */
double minCover = 0.0; /* Minimum percent of transcript aligning. */
double minAli = 0.0;   /* Minimum percent identity of alignments to keep. */
boolean weightMrna = FALSE; /* Add more weight to alignments of mRNAs? */
boolean useChromKeeper = FALSE; /* Load all of the info from the database once and store in local memory. */
boolean singleExonOk = FALSE;	/* If true keep single exon alignments. */
int numDbTables =0;     /* Number of tables we're going to load psls from. */
char **dbTables = NULL; /* Tables that psls will be loaded from. */
struct hash *tissLibHash = NULL; /* Hash of slInts by accession where first slInt is lib, second is tissue. */
int minPslStart = BIGNUM; /* Start of first psl on chromosome. */
int maxPslEnd = -1;       /* End of the last psl on chromosome. */
char *chromNib = NULL;    /* Nib file name if not using database. */
FILE *chromNibFile = NULL; /* File handle for nib file access. */
int chromNibSize = 0;      /* Size of the chromosome nib file. */
struct binKeeper *chromPslBin = NULL; /* Bin keeper for chromosome. */
struct binKeeper *agxSeenBin = NULL; /* AltGraphX records that have already been seen. */
struct hash *killHash = NULL; /* Hash of psl qNames that we want to avoid, 
				 hashed with an int value of 1. */
static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"beds", OPTION_STRING},
    {"genePreds", OPTION_STRING},
    {"agxOut", OPTION_STRING},
    {"consensus", OPTION_BOOLEAN},
    {"minCover", OPTION_FLOAT},
    {"minAli", OPTION_FLOAT},
    {"skipTissues", OPTION_BOOLEAN},
    {"weightMrna", OPTION_BOOLEAN},
    {"localMem", OPTION_BOOLEAN},
    {"chromNib", OPTION_STRING},
    {"pslFile", OPTION_STRING},
    {"killList", OPTION_STRING},
    {"noEst", OPTION_BOOLEAN},
    {"singleExonOk", OPTION_BOOLEAN},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Database (i.e. hg15) to load psl records from.",
    "Coordinate file to base clustering on in bed format.",
    "Coordinate file to base clustering on in genePred format.",
    "Name of file to output to.",
    "Try to extend partials to consensus site instead of farthest [recommended].",
    "Minimum percent of a sequence that an alignment can contain and be included.",
    "Minimum percent id of alignment to keep.",
    "Skip loading the tissues and libraries.",
    "Add more weight to mRNAs and RefGenes.",
    "Load all of the psls and associated information once from from db and cache in memory.",
    "Chromosome sequence in nib format.",
    "Use psl alignments in file rather than database.",
    "File of accessions to avoid.",
    "Don't include ESTs, just refSeq and mRNA.",
    "Allow single exon genes through.",
};

void usage()
/* print usage and quit */
{
int i;
printf(
       "altSplice - constructs altSplice graphs using psl alignments\n"
       "from est and mrna databases. Must specify either a bed file\n"
       "or a genePred file to load coordinates. This file should contain\n"
       "data for one and only one chromosome.\n"
       "usage:\n"
       "   altSplice -db=hg15 -beds=rnaCluster.bed -agxOut=out.agx\n"
       "where options are:\n");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "   -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
noWarnAbort();
}

void initializeChromNib(char *fileName)
/** Setup things to use the local nib sequence file
    instead of using the database. */
{
chromNib = fileName;
nibOpenVerify(fileName, &chromNibFile, &chromNibSize);
}

boolean passFilters(struct psl *psl)
/* Does this psl pass our filters? */
{
int milliMin = 1000 *minAli;
boolean pass = FALSE;
/* Check min coverage. */
pass = (psl->match + psl->repMatch >= minCover * (psl->qSize - psl->nCount));
/* Check min alignment percentage. */
pass = ((1000-pslCalcMilliBad(psl, TRUE)) > milliMin) && pass;
return pass;
}

struct psl *clonePsl(struct psl *psl)
/* Copy a psl to separate memory. */
{
struct psl *c = CloneVar(psl);
c->next = NULL;
c->qName = cloneString(psl->qName);
c->tName = cloneString(psl->tName);
AllocArray(c->blockSizes, c->blockCount);
CopyArray(psl->blockSizes, c->blockSizes, c->blockCount);
AllocArray(c->qStarts, c->blockCount);
CopyArray(psl->qStarts, c->qStarts, c->blockCount);
AllocArray(c->tStarts, c->blockCount);
CopyArray(psl->tStarts, c->tStarts, c->blockCount);
return c;
}

struct psl *filterPsls(struct psl *pslList, int chromStart, int chromEnd)
/** Filter pslList to get only "good" ones. */
{
struct psl *goodList = NULL, *psl = NULL, *pslNext = NULL;
for(psl = pslList; psl != NULL; psl = pslNext)
    {
    boolean notTooBig = FALSE;
    pslNext = psl->next;

    /* Figure out chromStart/chromEnd put us in the middle of a big
     * intron or the like inside of another gene.  If so ignore alignments
     * from enclosing genes by setting notTooBig. */
    int endFirstExon = psl->tStarts[0] + psl->blockSizes[0];
    int startLastExon = psl->tStarts[psl->blockCount-1];
    int pad=100;   
    notTooBig = (endFirstExon + pad >= chromStart && startLastExon -pad <= chromEnd);
#ifdef AFFYSPLICE
    notTooBig = TRUE;
#endif
    if(passFilters(psl) && notTooBig)
	{
	slSafeAddHead(&goodList, psl);
	}
    else
	{
	if(!useChromKeeper)
	    pslFree(&psl);
	}
    }
slReverse(&goodList);
return goodList;
}

struct psl *loadPslsFromDb(struct sqlConnection *conn, int numTables, char **tables, 
			   char *chrom, unsigned int chromStart, unsigned int chromEnd)
/* load up all of the psls that align on a given section of the database */
{
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = -100;
struct psl *pslList = NULL;
struct psl *psl = NULL;
int i=0;
/* for each table load up the relevant psls */
for(i = 0; i < numTables; i++)
    {
    sr = hRangeQuery(conn, tables[i], chrom, chromStart, chromEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	psl = pslLoad(row+rowOffset);
	slSafeAddHead(&pslList, psl);
	if(weightMrna && (stringIn("refSeqAli",tables[i]) || stringIn("mrna", tables[i])))
	    {
	    psl = clonePsl(psl);
	    slSafeAddHead(&pslList, psl);
	    }
	}    
    sqlFreeResult(&sr);
    }
slReverse(&pslList);
return pslList;
}

void expandToMaxAlignment(struct psl *pslList, char *chrom, int *chromStart, int *chromEnd)
/* finds the farthest up stream and downstream boundaries of alignments */
{
struct psl *psl=NULL;
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    if(differentString(psl->tName, chrom))
	errAbort("test_geneGraph::expandToMrnaAlignment() - psls on chrom: %s, need chrom: %s.", 
		 psl->tName, chrom);
    else 
	{
	if(psl->tStart < *chromStart)
	    *chromStart = psl->tStart;
	if(psl->tEnd > *chromEnd)
	    *chromEnd = psl->tEnd;
	}
    }
}

int agIndexFromEdge(struct altGraphX *ag, struct ggEdge *edge)
/* Find the index in edgeStarts & edgeEnds for edge. Returns -1 if
 * not found. */
{
int i;
assert(edge->vertex1 < ag->vertexCount && edge->vertex2 < ag->vertexCount);
for(i=0; i<ag->edgeCount; i++)
    {
    if(edge->vertex1 == ag->edgeStarts[i] && edge->vertex2 == ag->edgeEnds[i])
	return i;
    }
return -1;
}

int mcLargestFirstCmp(const void *va, const void *vb)
/* Compare to sort based sizes of chromEnd - chromStart, largest first. */
{
const struct ggMrnaCluster *a = *((struct ggMrnaCluster **)va);
const struct ggMrnaCluster *b = *((struct ggMrnaCluster **)vb);
int dif;
dif = -1 * (abs(a->tEnd - a->tStart) - abs(b->tEnd - b->tStart));
return dif;
}


boolean agIsUnique(struct altGraphX *ag)
/* Return TRUE if there isn't an altGraphX record already seen like this one. */
{
char buff[256];
safef(buff, sizeof(buff), "%s-%s-%d-%d", ag->tName, ag->strand, ag->tStart, ag->tEnd);
if(hashFindVal(uniqPos, buff))
    return FALSE;
else 
    hashAdd(uniqPos, buff, NULL);
return TRUE;
}    


boolean agxIsSubset(struct altGraphX *query, struct altGraphX *target)
/** Return TRUE if query is just a subset of target, FALSE otherwise. */
{
int *qPos = query->vPositions, *tPos = target->vPositions;
int *qStarts = query->edgeStarts, *tStarts = target->edgeStarts;
int *qEnds = query->edgeEnds, *tEnds = target->edgeEnds;
int qECount = query->edgeCount, tECount = target->edgeCount;
int qIx = 0, tIx = 0;
if(query->tStart < target->tStart || query->tEnd > target->tEnd ||
   query->strand[0] != target->strand[0])
    return FALSE;

/* Look to see if every query edge is subsumed by 
   a target edge. */
for(qIx = 0; qIx < qECount; qIx++) 
    {
    boolean edgeFound = FALSE;
    /* only looking at exons. */
    if(altGraphXEdgeType(query, qIx) != ggExon)
	continue;
    /* Look at each target exon to try and find one that 
       subsumes the query exon. */
    for(tIx = 0; tIx < tECount; tIx++)
	{
	if(altGraphXEdgeType(target, tIx) != ggExon)
	    continue;
	if(rangeIntersection(qPos[qStarts[qIx]], qPos[qEnds[qIx]],
			     tPos[tStarts[tIx]], tPos[tEnds[tIx]]) > 0)
	    {
	    edgeFound |= TRUE; /* Found one, update edge found. */
	    break; /* No need to keep looking for this query exon. */
	    }
	}
    if (!edgeFound)
        return FALSE;
    }
return TRUE;
}


boolean agxIsRedundant(struct altGraphX *agx)
/** Return TRUE if there has already been an altGraphX record that
    was a superSet of this data. */
{
struct binElement *be = NULL, *beList = NULL;
struct altGraphX *agxSeen = NULL;
boolean alreadySeen = FALSE;
beList = binKeeperFind(agxSeenBin, agx->tStart, agx->tEnd);
for(be = beList; be != NULL; be = be->next)
    {
    agxSeen = (struct altGraphX *)be->val;
    if(agxSeen == agx) 
	continue;
    if(agxIsSubset(agx, agxSeen)) 
	{
	alreadySeen = TRUE;
	break;
	}
    }
slFreeList(&beList);
return alreadySeen;
}

struct altGraphX *agFromAlignments(char *db, struct ggMrnaAli *maList, struct dnaSeq *seq, struct sqlConnection *conn,
				   int chromStart, int chromEnd, FILE *out )
/** Custer overlaps from maList into altGraphX structure. */
{
struct altGraphX *ag = NULL, *agList = NULL;
struct ggMrnaCluster *mcList=NULL, *mc=NULL;
struct ggMrnaInput *ci = NULL;
struct geneGraph *gg = NULL;
static int count = 0;
ci = ggMrnaInputFromAlignments(maList, seq);
mcList = ggClusterMrna(ci);
if(mcList == NULL)
    {	
    freeGgMrnaInput(&ci);
    return NULL;
    }    

clusterCount++;
for(mc = mcList; mc != NULL; mc = mc->next)
    {
    if(optionExists("consensus"))
	{
	gg = ggGraphConsensusCluster(db, mc, ci, tissLibHash, !optionExists("skipTissues"));
	}
    else
	gg = ggGraphCluster(db, mc,ci);
    assert(checkEvidenceMatrix(gg));
    ag = ggToAltGraphX(gg);
    if(ag != NULL)
	{
	char name[256];
	freez(&ag->name);
	safef(name, sizeof(name), "%s.%d", ag->tName, count++);
	ag->name = cloneString(name);
	/* Convert back to genomic coordinates. */
	altGraphXoffset(ag, chromStart);
	/* Sort vertices so that they are chromosomal order */
 	altGraphXVertPosSort(ag);
	/* write to file */
	binKeeperAdd(agxSeenBin, ag->tStart, ag->tEnd, ag);
	slAddHead(&agList, ag);
	}
    }

/* Sometimes get nested, partial transcripts. Want to filter 
   those out. */
for(ag = agList; ag != NULL; ag = ag->next)
    {
    if(!agxIsRedundant(ag))
       altGraphXTabOut(ag, out); 
    }
/* genoSeq and maList are freed with ci and gg */
ggFreeMrnaClusterList(&mcList);
freeGgMrnaInput(&ci);
freeGeneGraph(&gg);
return agList;
}



struct psl *getPslsFromCache(char *chrom, int chromStart, int chromEnd)
/** Get all of the psls for a given gp of interest. */
{
struct psl *pslList = NULL, *psl = NULL;
struct binElement *beList = NULL, *be = NULL;
beList = binKeeperFind(chromPslBin, chromStart, chromEnd);
for(be = beList; be != NULL; be = be->next)
    {
    psl = be->val;
    slAddHead(&pslList, psl);
    }
slFreeList(&beList);
return pslList;
}

void setupTables(char *chrom)
/* What tables are we using for constructing graphs. */
{
char *tablePrefixes[8];
int prefixCount = 0;

#ifdef AFFYSPLICE
char *wholeTables[] = {"splicesTmp"}; 
#endif
#ifdef USUAL
/* char *wholeTables[] = {}; */ /* Uncomment this if want to skip refSeq alignments
				   usually for comparison purposes. */
char *wholeTables[] = {"refSeqAli"};
#endif
int i;
char buff[256];

#ifndef AFFYSPLICE
tablePrefixes[prefixCount++] = "_mrna";
if (!optionExists("noEst"))
    tablePrefixes[prefixCount++] = "_est";
#endif

numDbTables = (prefixCount + ArraySize(wholeTables));
dbTables = needMem(sizeof(char*) * numDbTables);
for(i=0; i< prefixCount; i++)
    {
    snprintf(buff, sizeof(buff),"%s%s", chrom, tablePrefixes[i]);
    dbTables[i] = cloneString(buff);
    }
for(i = prefixCount; i < numDbTables; i++)
    {
    dbTables[i] = cloneString(wholeTables[i-prefixCount]);
    }
}

struct psl *getPslsFromDatabase(char *chrom, int chromStart, int chromEnd,
				struct sqlConnection *conn)
/** Load the psls for a given table directly from the database. */
{

struct psl *pslList = NULL;
pslList = loadPslsFromDb(conn, numDbTables, dbTables, chrom, chromStart, chromEnd);
return pslList;
}

void readTissueLibraryIntoCache(char *fileName)
/* Read in the tissue and library information from fileName. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[3];
struct slInt *tissue = NULL, *library = NULL;
tissLibHash = newHash(12);
while(lineFileNextRow(lf, words, ArraySize(words))) 
    {
    library = newSlInt(sqlSigned(words[1]));
    tissue = newSlInt(sqlSigned(words[2]));
    slAddTail(&library, tissue);
    hashAdd(tissLibHash, words[0], library);
    }
lineFileClose(&lf);
}


void setupTissueLibraryCache(struct sqlConnection *conn)
/* Hash all of the library and organism ids for this organism from
   the mrna table. The key to the hash is the accession and the
   value is a list of integers where the first is the library and
   the second is the tissue. */
{
struct sqlResult *sr = NULL;
char query[256];
struct slInt *tissue = NULL, *library = NULL;
char **row = NULL;
int i = 0;
tissLibHash = newHash(12);

/* Now load up all of the accessions for this organism. */
for(i = 0; i < numDbTables; i++)
    {
    safef(query, sizeof(query), "select library, tissue, acc from %s inner join gbCdnaInfo on qName = acc",
	  dbTables[i]);
    warn("%s", query);
    sr = sqlGetResult(conn, query);
    while((row = sqlNextRow(sr)) != NULL)
	{
	library = newSlInt(sqlSigned(row[0]));
	tissue = newSlInt(sqlSigned(row[1]));
	slAddTail(&library, tissue);
	hashAdd(tissLibHash, row[2], library);
	}
    sqlFreeResult(&sr);
    }

}

void loadPslsFromFile(char *pslFile, char *chrom, struct sqlConnection *conn)
/** Load the psls from the directed file (instead of the database. */
{
struct psl *psl = NULL, *pslNext = NULL, *pslList = NULL;
pslList = pslLoadAll(pslFile);
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    minPslStart = min(psl->tStart, minPslStart);
    maxPslEnd = max(psl->tEnd, maxPslEnd);
    }
chromPslBin = binKeeperNew(minPslStart, maxPslEnd);
agxSeenBin = binKeeperNew(minPslStart, maxPslEnd);
for(psl = pslList; psl != NULL; psl = pslNext)
    {
    pslNext = psl->next;
    if(sameString(psl->tName, chrom))
	binKeeperAdd(chromPslBin, psl->tStart, psl->tEnd, psl);
    else
	pslFree(&psl);
    }
}

void loadPslsFromDatabase(struct sqlConnection *conn, char *db, char *chrom) 
/** Load all of the desired alignments into the chromkeeper structure
    from the desired pslTables. */
{
int i = 0;
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;
struct psl *pslList = NULL, *psl = NULL;
for(i = 0; i < numDbTables; i++)
    {
    sr = hChromQuery(conn, dbTables[i], chrom, NULL, &rowOffset); 
    while((row = sqlNextRow(sr)) != NULL)
	{
	psl = pslLoad(row+rowOffset);
	slAddHead(&pslList, psl);
	minPslStart = min(psl->tStart, minPslStart);
	maxPslEnd = max(psl->tEnd, maxPslEnd);
	/* This just adds the mrna twice to the list, cheat way to add more
	   weight to certain tables. */
	if(weightMrna && (stringIn("refSeqAli", dbTables[i]) || stringIn("mrna", dbTables[i])))
	    {
	    psl = clonePsl(psl);
	    slAddHead(&pslList, psl);
	    }
	}
    sqlFreeResult(&sr);
    }

chromPslBin = binKeeperNew(minPslStart, maxPslEnd);
agxSeenBin = binKeeperNew(minPslStart, maxPslEnd);
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    binKeeperAdd(chromPslBin, psl->tStart, psl->tEnd, psl);
    }
}

void setupChromKeeper(struct sqlConnection *conn, char *db, char *chrom) 
/** Load all of the desired alignments into the chromkeeper structure
    from the desired pslTables. */
{
if(optionVal("pslFile", NULL) != NULL)
    loadPslsFromFile(optionVal("pslFile", NULL), chrom, conn);
else
    loadPslsFromDatabase(conn, db, chrom);
}

int pslCmpTargetQuery(const void *va, const void *vb)
/* Compare to sort based on target start. */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
int dif;
dif = strcmp(a->tName, b->tName);
if (dif == 0)
    dif = a->tStart - b->tStart;
if(dif == 0)
    dif = a->tEnd - b->tEnd;
if(dif == 0)
    dif = strcmp(a->qName, b->qName);
return dif;
}

struct psl *removeKillList(struct psl* pslList)
/* Remove all of the psls that are in the kill hash. */
{
struct psl *psl = NULL, *pslNext = NULL,  *pslNewList = NULL;
if(killHash == NULL)
    return pslList;
for(psl = pslList; psl != NULL; psl = pslNext)
    {
    pslNext = psl->next;
    /* If the accession is in the kill list with value 1
       don't add it. */
    if(hashIntValDefault(killHash, psl->qName, 0) == 0)
	{
	slAddHead(&pslNewList, psl);
	}
    }
slReverse(&pslNewList);
return pslNewList;
}

struct psl *getPslsInRange(char *chrom, int chromStart, int chromEnd, 
			   struct sqlConnection *conn)
/** Load all of the psls in a given range from the appropriate
    cache or database. */
{
struct psl *pslList = NULL, *goodList = NULL, *psl = NULL;
if(useChromKeeper)
    pslList = getPslsFromCache(chrom, chromStart, chromEnd);
else
    pslList = getPslsFromDatabase(chrom, chromStart, chromEnd, conn);
goodList = filterPsls(pslList, chromStart, chromEnd);
goodList = removeKillList(goodList);
for(psl = goodList; psl != NULL; psl = psl->next)
    {
    if(psl->tStart < minPslStart)
	minPslStart = psl->tStart;
    if(psl->tEnd > maxPslEnd)
	maxPslEnd = psl->tEnd;
    }
slSort(&goodList, pslCmpTargetQuery);
return goodList;
}

struct psl *getPsls(struct genePred *gp, struct sqlConnection *conn)
/** Load psls from local chromKeeper structure or from database 
    depending on useChromKeeper flag. */
{
return getPslsInRange(gp->chrom, gp->txStart, gp->txEnd, conn);
}

struct dnaSeq *dnaFromChrom(char *db, char *chrom, int chromStart, int chromEnd, enum dnaCase seqCase)
/** Return the dna for the chromosome region specified. */
{
struct dnaSeq *seq = NULL;
if(chromNib != NULL)
    {
    seq = (struct dnaSeq *)nibLdPart(chromNib, chromNibFile, chromNibSize, 
				     chromStart, chromEnd - chromStart);
    }
else
    seq = hDnaFromSeq(db, chrom, chromStart, chromEnd, seqCase);
return seq;
}

struct altGraphX *agFromGp(char *db, struct genePred *gp, struct sqlConnection *conn, 
			   int maxGap, FILE *out)
/** Create an altGraphX record by clustering psl records within coordinates
    specified by genePred record. */
{
struct altGraphX *ag = NULL;
struct dnaSeq *genoSeq = NULL;
struct ggMrnaAli *maList=NULL, *ma=NULL, *maNext=NULL, *maSameStrand=NULL;
struct psl *pslList = NULL, *psl = NULL, *pslCluster = NULL, *pslNext = NULL;
char *chrom = gp->chrom;
int chromStart = BIGNUM;
int chromEnd = -1;

verbose(2, "agFromGp on %s %s:%d-%d\n", gp->name, gp->chrom, gp->txStart, gp->txEnd);

pslList = getPsls(gp, conn);
verbose(3, "  got %d psls\n", slCount(pslList));
if(slCount(pslList) == 0)
    {
    verbose(2, "No available alignments for %s.", gp->name);
    return NULL;
    }
/* expand to find the furthest boundaries of alignments */
expandToMaxAlignment(pslList, chrom, &chromStart, &chromEnd);
verbose(3, "  expanded to %s:%d-%d\n", chrom, chromStart, chromEnd);

/* get the sequence */
genoSeq = dnaFromChrom(db, chrom, chromStart, chromEnd, dnaLower);

for(psl = pslList; psl != NULL; psl = pslNext)
    {
    pslNext = psl->next;
    if(singleExonOk || pslHasIntron(psl, genoSeq, chromStart))
	{
	slAddHead(&pslCluster, psl);
	}
    else 
	{
	if(!useChromKeeper)
	    pslFree(&psl);
	}
    }
verbose(3, "  got %d psls after intron/singleExon check\n", slCount(pslCluster));
/* load and check the alignments */
maList = pslListToGgMrnaAliList(pslCluster, gp->chrom, chromStart, chromEnd, genoSeq, maxGap);
verbose(3, "  got %d in maList\n", slCount(maList));

for(ma = maList; ma != NULL; ma = maNext)
    {
    maNext = ma->next;
    verbose(4, "      ma->strand %s, gp->strand %s\n", ma->strand, gp->strand);
    if(ma->strand[0] == gp->strand[0])
	{
	slSafeAddHead(&maSameStrand, ma);
	}
    else
	ggMrnaAliFree(&ma);
    }
slReverse(&maSameStrand);

verbose(3, "  got %d in ma on same strand\n", slCount(maSameStrand));

/* If there is a cluster to work with create an geneGraph */
if(maSameStrand != NULL)
    {
    ag = agFromAlignments(db, maSameStrand, genoSeq, conn, chromStart, chromEnd,  out);
    }
else
    {
    dnaSeqFree(&genoSeq);
    ggMrnaAliFreeList(&maSameStrand);
    }

/* Only free psls if not using cache... */
if(!useChromKeeper)
    pslFreeList(&pslCluster);
return ag;
}

struct genePred *convertBedsToGps(char *bedFile)
/* Load beds from a file and convert to bare bones genePredictions. */
{
struct genePred *gpList = NULL, *gp =NULL;
struct bed *bedList=NULL, *bed=NULL;
bedList = bedLoadNAll(bedFile, 6);
if(bedList->strand == NULL)
    errAbort("Beds must have strand information.");
for(bed=bedList; bed!=NULL; bed=bed->next)
    {
    AllocVar(gp);
    gp->chrom = cloneString(bed->chrom);
    gp->txStart = gp->cdsStart = bed->chromStart;
    gp->txEnd = gp->cdsEnd = bed->chromEnd;
    gp->name = cloneString(bed->name);
    safef(gp->strand, sizeof(gp->strand), "%s", bed->strand);
    slAddHead(&gpList, gp);
    }
bedFreeList(&bedList);
slReverse(&gpList);
return gpList;
}

int gpSmallestFirstCmp(const void *va, const void *vb)
/* Compare to sort based on chrom and
 * sizes of chromEnd - chromStart, smallest first. */
{
const struct genePred *a = *((struct genePred **)va);
const struct genePred *b = *((struct genePred **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = abs(a->txEnd -a->txStart) - abs(b->txEnd - b->txStart);
return dif;
}

boolean gpAllSameChrom(struct genePred *list)
/* Return TRUE if all genePreds are on same chromosome. */
{
struct genePred *gp;
for (gp = list; gp != NULL; gp = gp->next)
    if (!sameString(gp->chrom, list->chrom))
         return FALSE;
return TRUE;
}

void createAltSplices(char *db, char *outFile,  boolean memTest)
/* Top level routine, gets genePredictions and runs through them to 
   build altSplice graphs. */
{
struct genePred *gp = NULL, *gpList = NULL;
struct altGraphX *ag=NULL;
FILE *out = NULL;
struct sqlConnection *conn = hAllocConn(db);
char *gpFile = NULL;
char *bedFile = NULL;
int count =0;

/* Figure out where to get coordinates from. */
bedFile = optionVal("beds", NULL);
gpFile = optionVal("genePreds", NULL);
if(bedFile != NULL)
    gpList = convertBedsToGps(bedFile);
else if(gpFile != NULL)
    gpList = genePredLoadAll(gpFile);
else 
    {
    warn("Must specify target loci as either a bed file or a genePred file");
    usage();
    }

if (!gpAllSameChrom(gpList))
    errAbort("Multiple chromosomes in bed or genePred file.");

/* Sanity check to make sure we got some loci to work
   with. */
if(gpList == NULL)
    errAbort("No gene boundaries were found.");
slSort(&gpList, genePredCmp);
setupTables(gpList->chrom);

/* If local memory get things going here. */
if(optionExists("localMem")) 
    {
    warn("Using local memory. Setting up caches...");
    useChromKeeper = TRUE;
    setupChromKeeper(conn, optionVal("db", NULL), gpList->chrom);
    if(!optionExists("skipTissues"))
	{
	if(optionExists("tissueLibFile"))
	    readTissueLibraryIntoCache(optionVal("tissueLibFile", NULL));
	else
	    setupTissueLibraryCache(conn);
	}
    warn("Done setting up local caches.");
    }
else /* Have to set up agxSeen binKeeper based on genePreds. */
    {
    int maxPos = 0;
    int minPos = BIGNUM;
    for(gp = gpList; gp != NULL; gp = gp->next)
	{
	maxPos = max(maxPos, gp->txEnd);
	minPos = min(minPos, gp->txStart);
	}
    agxSeenBin = binKeeperNew(max(0, minPos-10000), min(BIGNUM,maxPos+10000));
    }

dotForUserInit(max(slCount(gpList)/10, 1));
out = mustOpen(outFile, "w");
for(gp = gpList; gp != NULL && count < 5; )
    {
    dotForUser();
    fflush(stderr);
    ag = agFromGp(db, gp, conn, 5, out); /* memory held in binKeeper. Free
				      * later. */
    if (memTest != TRUE) 
	gp = gp->next;
    }
genePredFreeList(&gpList);
hFreeConn(&conn);
/* uglyf("%d genePredictions with %d clusters, %d cassette exons, %d of are not mod 3.\n", */
/*       slCount(gpList), clusterCount, cassetteCount, misSense); */
}

void initKillList()
/* Load up a hash of the accessions to avoid. */
{
struct lineFile *lf = NULL;
char *killFile = optionVal("killList", NULL);
char *words[1];
assert(killFile);
killHash = newHash(10);
lf = lineFileOpen(killFile, TRUE);
while(lineFileNextRow(lf, words, ArraySize(words)))
    {
    hashAddInt(killHash, words[0], 1);
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* main routine, calls optionInt and sets database */
{
char *outFile = NULL;
boolean memTest=FALSE;
char *db = NULL;
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
db = optionVal("db", NULL);
if(db == NULL)
    errAbort("Must set -db flag. Try -help for usage.");
if(optionExists("killList"))
    initKillList();
outFile = optionVal("agxOut", NULL);
singleExonOk = optionExists("singleExonOk");
if(outFile == NULL)
    errAbort("Must specify output file with -agxOut flag. Try -help for usage.");
if(optionVal("chromNib", NULL) != NULL)
    initializeChromNib(optionVal("chromNib", NULL));
minAli = optionFloat("minAli", 0.0);
minCover = optionFloat("minCover", 0.0);
memTest = optionExists("memTest");
weightMrna = optionExists("weightMrna");
if(memTest == TRUE)
    warn("Testing for memory leaks, use top to monitor and CTRL-C to stop.");
uniqPos = newHash(10);
createAltSplices(db, outFile, memTest );
return 0;
}

