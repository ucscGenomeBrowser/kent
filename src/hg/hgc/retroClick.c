/* retroClick - retroGene click handling */

/*
 * Tables associated with retroGenes:
 *   retroXxxInfo - features of retroGene
 *   retroXxxAli - mapped PSLs aligments
 *   retroXxxAliGene - mapped alignmetns with CDS and frame annotation
 *
 * Xxx is:
 *    - Mrna - GenBank mRNAs and refSeq
 */

#include "common.h"
#include "hgc.h"
#include "retroClick.h"
#include "retroMrnaInfo.h"
#include "genbank.h"
/* combine blocks separated by gaps less than this number */
#define MAXBLOCKGAP 50

/* space to allocate for a id */
#define ID_BUFSZ 64

struct mappingInfo
/* various pieces of information about mapping from table name and 
 * retroXxxInfo table */
{
    char tblPre[64];           /* table prefix */
    char geneSet[6];           /* source gene set abbrv used in table name */
    struct retroMrnaInfo *pg;  /* general info for retro gene */
    boolean indirect;          /* an indirect mapping */
    char gbAcc[ID_BUFSZ];      /* src accession */
    short gbVer;               /* version from gbId */
    char seqId[ID_BUFSZ];      /* id used to look up sequence, different than
                                * srcAcc if multiple levels of mappings have
                                * been done */
    char suffix[ID_BUFSZ];
    char *sym;                 /* src gene symbol and desc */
    char *desc;
    short gbCurVer;            /* version from genbank table */
};

static void parseSrcId(struct mappingInfo *mi)
/* parse srcId parts and save in mi */
{
char idBuf[ID_BUFSZ];
char *id, *colon = NULL, *dot = NULL, *under = NULL, *dash = NULL;

/* direct mappings (cDNA is aligned to source organism) have ids in the form:
 * NM_012345.1-1.1 Indirect mappings, where mapped predictions are used to
 * form a gene set that is mapped again, have ids in the form
 * db:NM_012345.1_1.1, where db is the source organism, and '_1' is added so
 * that db:NM_012345.1_1 uniquely identifies the source. */

safef(idBuf, sizeof(idBuf), "%s", mi->pg->name);
id = idBuf;

/* find various parts */
colon = strchr(id, ':');
dot = strchr(id, '.');
if (dot != NULL)
    {
    under = strchr(dot, '_');
    dash = strchr(dot, '-');
    }
if ((dot == NULL) || (dash == NULL))
    errAbort("can't parse accession from srcId: %s", mi->pg->name);
mi->indirect = (colon != NULL);

/* id used to get sequence is before `-'.  For direct, it excludes
 * genbank version */
*dash = '\0';
if (!mi->indirect)
    *dot = '\0';
safef(mi->seqId, sizeof(mi->seqId), "%s", id);

/* now pull out genbank accession for obtaining description */
id = (colon != NULL) ? colon+1 : id;
*dot = '\0';

safef(mi->gbAcc, sizeof(mi->gbAcc), "%s", id);
if (under != NULL)
    *under = '\0';
//mi->gbVer = sqlSigned(dot+1);
}

static void getGenbankInfo(struct sqlConnection *conn, struct mappingInfo *mi)
/* get source gene info and version from gbCdnaInfo and save in mi */
{
char query[512], **row;
struct sqlResult *sr;
char *defDb = hDefaultDb();

/* if id has been modified for multi-level ancestor mappings:
 *    NM_012345.1-1.1 -> db:NM_012345a.1.1
 * then hack it back to the original accession.  However, don't get version,
 * since the sequence is different.
 */

safef(query, sizeof(query),
      "select gbCdnaInfo.version, geneName.name, description.name "
      "from %s.gbCdnaInfo, %s.geneName, %s.description "
      "where gbCdnaInfo.acc=\"%s\" and gbCdnaInfo.geneName=geneName.id and gbCdnaInfo.description = description.id",
      defDb, defDb, defDb, mi->gbAcc);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    mi->gbCurVer = sqlSigned(row[0]);
    mi->sym = cloneString(row[1]);
    mi->desc = cloneString(row[2]);
    }
sqlFreeResult(&sr);
}

static struct mappingInfo *mappingInfoNew(struct sqlConnection *conn,
                                          char *tbl, char *mappedId)
/* load mapping info for a mapped gene */
{
struct mappingInfo *mi;
int preLen;
char *suffix = containsStringNoCase(tbl,"Info");
AllocVar(mi);
if (suffix != NULL)
    {
    suffix +=4;
    safef(mi->suffix,ID_BUFSZ,"%s",suffix);
    }

if (startsWith("retroAnc", tbl))
    strcpy(mi->tblPre, "retroAnc");
else if (startsWith("retroOld", tbl))
    strcpy(mi->tblPre, "retroOld");
else
    strcpy(mi->tblPre, "retro");
preLen = strlen(mi->tblPre);
if (startsWith("retroAugust", tbl))
    strcpy(mi->geneSet, "August");
else
    strcpy(mi->geneSet, "Mrna");
#ifdef junk

if (startsWith("Ref", tbl+preLen))
    strcpy(mi->geneSet, "Ref");
else if (startsWith("Mrna", tbl+preLen))
    strcpy(mi->geneSet, "Mrna");
else
    errAbort("can't determine source gene set from table: %s", tbl);
#endif
if (suffix != NULL)
    mi->pg = sqlQueryObjs(conn, (sqlLoadFunc)retroMrnaInfoLoad, sqlQueryMust|sqlQuerySingle,
                      "select * from %s%sInfo%s where name='%s'", mi->tblPre, mi->geneSet, suffix,
                       mappedId);
else
    mi->pg = sqlQueryObjs(conn, (sqlLoadFunc)retroMrnaInfoLoad, sqlQueryMust|sqlQuerySingle,
                      "select * from %s%sInfo where name='%s'", mi->tblPre, mi->geneSet,
                       mappedId);
parseSrcId(mi);
getGenbankInfo(conn, mi);
return mi;
}

static void mappingInfoFree(struct mappingInfo **mip)
/* free mappingInfo object */
{
struct mappingInfo *mi = *mip;
if (mi != NULL)
    {
    retroMrnaInfoFree(&mi->pg);
    freeMem(mi->sym);
    freeMem(mi->desc);
    }
}

static void displaySrcGene(struct sqlConnection *conn, struct mappingInfo *mi)
/* display information about the source gene that was mapped */
{
char srcGeneUrl[1024];

/* description will be NULL if deleted */
if (!startsWith("retroAugust",mi->geneSet))
    getGenbankInfo(conn, mi);

/* construct URL to browser */
safef(srcGeneUrl, sizeof(srcGeneUrl),
      "../cgi-bin/hgTracks?db=%s&position=%s:%d-%d",
       hGetDb(), mi->pg->gChrom, mi->pg->gStart, mi->pg->gEnd);

printf("<TABLE class=\"transMap\">\n");
printf("<CAPTION>Source gene</CAPTION>\n");
printf("<TBODY>\n");
printf("<TD CLASS=\"transMapNoWrap\"><A HREF=\"%s\" target=_blank>%s</A>", srcGeneUrl, mi->pg->name);
if (mi->desc == NULL)
    printf("<TD>&nbsp;<TD>gene no longer in source database");
else
    printf("<TD>%s<TD>%s", mi->sym, mi->desc);
printf("</TR>\n");
printf("</TBODY></TABLE>\n");
}


static void displayRetroDetails(struct sqlConnection *conn, struct mappingInfo *mi)
/* display information from a retroXXXInfo table */
{
struct retroMrnaInfo *pg = mi->pg;
printf("<TABLE class=\"transMap\">\n");
printf("<THEAD>\n");
printf("<TR><TH>Orthology (net) Break<TH>Coverage %%</TR>\n");
printf("</THEAD><TBODY>\n");
if (sameString(organism,"Human"))
    printf("<TR><TH>Mouse <TD>%d</TR>\n",  pg->overlapMouse);
else
    printf("<TR><TH>Human <TD>%d</TR>\n", pg->overlapMouse);
printf("<TR><TH>Dog<TD>%d</TR>\n", pg->overlapDog);
printf("<TR><TH>Rhesus<TD>%d</TR>\n", pg->overlapRhesus);
printf("</TBODY></TABLE>\n");
}

static struct psl *loadPslRangeT(char *table, char *qName, char *tName, int tStart, int tEnd)
/* Load a list of psls given qName tName tStart tEnd */
{
struct sqlResult *sr = NULL;
char **row;
struct psl *psl = NULL, *pslList = NULL;
boolean hasBin;
char splitTable[64];
char query[256];
struct sqlConnection *conn = hAllocConn();

hFindSplitTable(seqName, table, splitTable, &hasBin);
safef(query, sizeof(query), "select * from %s where qName = '%s' and tName = '%s' and tEnd > %d and tStart < %d", splitTable, qName, tName, tStart, tEnd);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+hasBin);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);
hFreeConn(&conn);
return pslList;
}

/* get overlap with psl and set the number of overlapping blocks to numBlocks */
int getOverlap(struct psl *psl, int start, int end, int *numBlocks)
{
int total = 0;
int i;
int blocks = 0;
for (i = 0 ; i < psl->blockCount ; i++)
    {
    int qs = psl->qStarts[i];
    int qe = psl->qStarts[i] + psl->blockSizes[i];
    int overlap = 0;
    float coverage = 0;
    if (psl->strand[0] == '-')
        reverseIntRange(&qs, &qe, psl->qSize);
    overlap = positiveRangeIntersection(start, end, qs, qe);
    coverage = (float)overlap/(float)(qe-qe);
    total += overlap;
    if (overlap > 25 || coverage > 0.4)
         blocks++;
    }
*numBlocks = blocks;
return total;
}

void printBlocks (struct psl *psl, int maxBlockGap, struct psl *nestedPsl)
{
int i, qsStart = 0;
int exonsCovered = 0;
int totalBases = 0;
int totalExons = 0;
int intronsPresent = 0;
int intronsPresentBases = 0;
int intronsSpliced = 0;
for (i = 0 ; i < psl->blockCount ; i++)
    {
    int qs = psl->qStarts[i];
    int qe = psl->qStarts[i] + psl->blockSizes[i];
    int te = psl->tStarts[i] + psl->blockSizes[i];
    int tdiff = 0;
    int qdiff = 0;
    int cumTdiff = 0;
    int oqs, oqe;
    int tsNext = 999999;
    int qsNext = 0;
    float coverage = 0;
    int bases = 0;
    int oldte = te;
    int gapRatio = 0;
    if (i < psl->blockCount -1)
        {
        tsNext = psl->tStarts[i+1];
        qsNext = psl->qStarts[i+1];
        tdiff = tsNext - te;
        qdiff = qsNext - qe;
        }
    else
        tdiff = 9999999;
    cumTdiff = tdiff;
    qsStart = qs;
/* combine blocks that are close together */
    while (tdiff < maxBlockGap && i< (psl->blockCount)-1)
        {
        i++;
        te = psl->tStarts[i] + psl->blockSizes[i];
        qe = psl->qStarts[i] + psl->blockSizes[i];
        if (i < psl->blockCount -1)
            {
            tsNext = psl->tStarts[i+1];
            qsNext = psl->qStarts[i+1];
            tdiff = tsNext - te;
            qdiff = qsNext - qe;
            }
        else
            tdiff = 9999999;
        cumTdiff += tdiff;
        oldte = te;
        }
    oqs = qs; oqe = qe;
    if (psl->strand[0] == '-')
        reverseIntRange(&oqs, &oqe, psl->qSize);
    gapRatio = qdiff*100/tdiff;
    printf("%d-%d:%d",oqs,oqe,gapRatio); 
    if (gapRatio < 30 && tdiff > 30 && 
            i < (psl->blockCount)-1)
        {
        int bases = (tdiff-qdiff > 0) ? tdiff - qdiff : 0;
        intronsPresent++;
        intronsPresentBases += bases;
        }
    if (nestedPsl != NULL)
        {
        int numBlocks = 0;
        bases = getOverlap(nestedPsl, oqs, oqe, &numBlocks); 
        coverage = (float)bases/(float)(oqe-oqs);
        totalBases += bases;
        totalExons++;
        if (coverage > 0.20)
            exonsCovered++;
        printf("/%d", numBlocks-1);
        intronsSpliced += numBlocks-1;
        }
    printf(", ");
    }
}

struct psl *pslFromGenePred(struct genePred *gp, int targetSize)
/* Create a psl record from a genePred record. */
{
struct psl *psl = NULL;
int i;
AllocVar(psl);
psl->tName = cloneString(gp->chrom);
psl->qName = cloneString(gp->name);
psl->tStart = gp->txStart;
psl->tEnd = gp->txEnd;
psl->strand[0] = gp->strand[0];
AllocArray(psl->blockSizes, gp->exonCount);
AllocArray(psl->tStarts, gp->exonCount);
AllocArray(psl->qStarts, gp->exonCount);
for(i=0; i<gp->exonCount; i++)
    {
    psl->blockSizes[i] = gp->exonEnds[i] - gp->exonStarts[i];
    psl->qStarts[i] = psl->qSize;
    psl->qSize += psl->blockSizes[i];
    psl->tStarts[i] = gp->exonStarts[i];
    }
psl->match = psl->qSize;
psl->misMatch = 0;
psl->repMatch = 0;
psl->nCount = 0;
psl->qNumInsert = gp->exonCount;
psl->tNumInsert = 0;
psl->tBaseInsert = 0;
psl->qStart = 0;
psl->qEnd = psl->qSize;
psl->tSize = targetSize;
psl->tStart = gp->txStart;
psl->tEnd = gp->txEnd;
psl->blockCount = gp->exonCount;
return psl;
}

struct psl *getParentAligns(struct sqlConnection *conn, struct mappingInfo *mi, char **table)
{
struct retroMrnaInfo *pg = mi->pg;
struct psl *pslList = NULL;
char query[512];
if (startsWith("August",mi->geneSet))
    {
    if (hTableExists("augustusXAli"))
        {
        *table = cloneString( "augustusXAli");
        pslList = loadPslRangeT(*table, mi->seqId, pg->gChrom, pg->gStart, pg->gEnd);
        }
    else if (hTableExists("augustusX"))
        {
        struct sqlResult *sr;
        char **row;
        int targetSize = 0;
        *table = cloneString( "augustusX");
        safef(query, sizeof(query), "select * from augustusX where chrom = '%s' and txEnd > %d and txStart < %d and name like '%s%%'", 
                pg->gChrom, pg->gStart, pg->gEnd , mi->seqId );
        sr = sqlGetResult(conn, query);
        if ((row = sqlNextRow(sr)) != NULL)
            {
            struct genePred *gp = genePredLoad(row+1);
            safef(query, sizeof(query), 
                    "select size from chromInfo where chrom = '%s' " , gp->chrom); 
            sqlFreeResult(&sr);
            targetSize = sqlNeedQuickNum(conn, query) ;
            pslList = pslFromGenePred(gp, targetSize);
            }   
        }

    }
else if (hTableExists("all_mrna"))
    {
    char parent[255];
    char *dotPtr ;
    *table = cloneString( "all_mrna");
    safef(parent, sizeof(parent), "%s",pg->name);
    /* strip off version and unique suffix when looking for parent gene*/
    dotPtr = rStringIn(".",parent) ;
    if (dotPtr != NULL)
        *dotPtr = '\0';
    pslList = loadPslRangeT(*table, mi->gbAcc, pg->gChrom, pg->gStart, pg->gEnd);
    if (pslList == NULL)
        {
        *table = cloneString( "refSeqAli");
        pslList = loadPslRangeT(*table, mi->gbAcc, pg->gChrom, pg->gStart, pg->gEnd);
        }
    }
else
    printf("no all_mrna table found<br>\n");
return pslList;
}

static void displayParentAligns(struct mappingInfo *mi, struct psl *pslList, char *table)
{
struct retroMrnaInfo *pg = mi->pg;
if (pslList != NULL && *table )
    {
    printf("<H3>Parent Locus/Parent mRNA Alignments </H3>");
    printAlignments(pslList, pslList->tStart, "htcCdnaAli", table, \
            mi->gbAcc);
    }
else
    printf("missing alignment %s chr %s:%d-%d from table %s<br>\n",
             mi->gbAcc, pg->gChrom, pg->gStart, pg->gEnd, table);
}

/* return count of coding exons */
int genePredcountCdsExons(struct genePred *gp)
{
int i;
int count = 0;
for (i=0; i<(gp->exonCount); i++)
    {
    if ( (gp->cdsStart <= gp->exonEnds[i]) &&  
         (gp->cdsEnd >= gp->exonStarts[i]) )
         count++;
    }
return count;

}
static void displayMappingInfo(struct sqlConnection *conn, struct mappingInfo *mi)
/* display information from a transMap table */
{
struct retroMrnaInfo *pg = mi->pg;
int overlapOrtholog = max(pg->overlapMouse, pg->overlapDog);
double  wt[12];     /* weights on score function*/
char query[512];
char *name;
char alignTbl[128];
struct psl *psl;
float coverFactor = 0;
float maxOverlap = 0, rawScore = 0;
if (mi->suffix == NULL)
    safef(alignTbl, sizeof(alignTbl), "%s%sAli", mi->tblPre, mi->geneSet);
else
    safef(alignTbl, sizeof(alignTbl), "%s%sAli%s", mi->tblPre, mi->geneSet, mi->suffix);
printf("<TABLE class=\"transMap\">\n");
printf("<CAPTION>Retrogene stats</CAPTION>\n");
printf("<THEAD>\n");
printf("<TR><TH>Feature<TH>Value </TR>\n");
printf("</THEAD><TBODY>\n");
if (sameString(pg->type, "singleExon"))
    printf("<TR><TH>Type of Parent<TD>%s</tr>\n",pg->type);
else 
    printf("<TR><TH>Type of RetroGene<TD>%s</TR>\n",pg->type);
printf("<TR><TH>Score <TD>%d (range from 0 - %d)</TR>\n",  
        pg->score,
        sqlQuickNum(conn, "select max(score) from retroMrnaInfo") );
printf("<TR><TH>Alignment Coverage of parent gene (Bases&nbsp;matching Parent) <TD>%d %% &nbsp;(%d bp) </TR>\n", pg->coverage, pg->matches);
printf("<TR><TH>Introns Procesed Out <TD>%d out of %d (%d exons covered)\n", pg->processedIntrons, (pg->parentSpliceCount/2)-1, pg->exonCover);
printf("<TR><TH>Introns Present<TD>%d\n", pg->intronCount);
printf("<TR><TH>Conserved Splice Sites<TD>%d</TR>\n",  pg->conservedSpliceSites);
printf("<TR><TH>Parent Splice Sites<TD>%d</TR>\n",  pg->parentSpliceCount);
psl = getAlignments(conn, alignTbl, mi->pg->name);
if (psl != NULL)
    {
    maxOverlap = (float)pg->maxOverlap/(float)(psl->match+psl->misMatch+psl->repMatch)  ;
    coverFactor = ((float)(psl->qSize-psl->qEnd)/(float)psl->qSize);
    }
else 
    {
    maxOverlap = 0;
    }
wt[0] = 0; wt[1] = 0.85; wt[2] = 0.2; wt[3] = 0.3; wt[4] = 0.8; 
wt[5] = 1; wt[6] = 1  ; wt[7] = 0.5; wt[8] = 0.5; wt[9] = 1; wt[10] = 1;
rawScore = wt[0]*pg->milliBad+
                wt[1]*(log(pg->exonCover+1)/log(2))*200 + 
                wt[2]*(((log(pg->axtScore>0?pg->axtScore:1)/log(2))*170)-1000)+
                wt[3]*(log(pg->polyAlen+2)*200) +
                wt[4]*overlapOrtholog*10 + 
                wt[5]*(((log(pg->processedIntrons > 0 ? pg->processedIntrons : 1))/log(2))*600) +
                (float)wt[6]*pow(pg->intronCount,0.5)*750 +
                (float)wt[7]*(maxOverlap*300)+
                wt[8]*((pg->coverage/100.0)*(1.0-coverFactor)*300.0)+
                wt[9]*(pg->tReps*10)+ 
                wt[10]*pg->oldIntronCount;
#ifdef debug
char table[512];
struct psl *pslList = getParentAligns(conn, mi, &table);
if (psl != NULL)
    {
    printf("<TR><TH>Blocks in retro:gap%%/intronsSpliced <TD>\n");
    printBlocks(psl, MAXBLOCKGAP, pslList);
    printf("</td></TR>\n");  
    }
if (pslList != NULL)
    {
    printf("<TR><TH>Exons in parent:gap%% <TD>\n");
    printBlocks(pslList, MAXBLOCKGAP, NULL);
    printf("</td></TR>\n");  
    pslFreeList(&pslList);
    }
#endif
printf("<TR><TH>Length of PolyA Tail<TD>%d As&nbsp;out&nbsp;of&nbsp;%d&nbsp;bp </TR><TR><TH>PolyA %% identity(position)<TD>%5.1f&nbsp;%%\n",pg->polyA,pg->polyAlen, (float)pg->polyA*100/(float)pg->polyAlen);
printf("&nbsp;(%d&nbsp;bp&nbsp;from&nbsp;end&nbsp;of&nbsp;retrogene)<br>\n",pg->polyAstart);

printf("<tr><th>Expression evidence<td>");
if (!sameString(pg->overName, "none"))
    printf("Bases&nbsp;overlapping:&nbsp;%s&nbsp;(%d&nbsp;bp)\n", pg->overName, pg->maxOverlap);
else
    printf("No&nbsp;overlapping");
printf("<TR><TH>bestorf score (>50 is good)<TD>%4.0f</td></TR>\n",pg->posConf);
#ifdef score
printf("<TR><TH>score function<TD>1:xon %d %4.1f conSS %d 2: ax %4.1f 3: pA %4.1f 4: net + %4.1f max (%d, %d) 5: procIntrons %d %4.1f 6:in.cnt %d -%4.1f 7:overlap - %4.1f  8:cov %d*(qe %d- qsz %d)/%d=%4.1f 9:tRep - %4.1f 10:oldintron %d %4.1f </td></TR>\n",
                pg->exonCover,
                wt[1]*(log(pg->exonCover+1)/log(2))*200 , 
                pg->conservedSpliceSites,
                wt[2]*(((log(pg->axtScore>0?pg->axtScore:1)/log(2))*170)-1000),
                wt[3]*(log(pg->polyAlen+2)*200) ,
                wt[4]*overlapOrtholog*10 , pg->overlapMouse, pg->overlapDog,
                pg->processedIntrons,
                wt[5]*(((log(pg->processedIntrons > 0 ? pg->processedIntrons : 1))/log(2))*600) ,
                pg->intronCount, 
                wt[6]*pow(pg->intronCount,0.5)*750 ,
                wt[7]*(maxOverlap*300),
                pg->coverage, pg->qEnd, pg->qSize , pg->qSize,
                wt[8]*((pg->coverage/100.0)*(1.0-coverFactor)*300.0),
                wt[9]*(pg->tReps*10), 
                pg->oldIntronCount,
                wt[10]*pg->oldIntronCount);
printf("<TR><TH>score function<TD>%4.1f+ %4.1f+ %4.1f+ %4.1f+ %4.1f - %4.1f - %4.1f+ %4.1f - %4.1f - %4.1f=%4.1f %4.1f</td></TR>\n",
                wt[1]*(log(pg->exonCover+1)/log(2))*200 , 
                wt[2]*(((log(pg->axtScore>0?pg->axtScore:1)/log(2))*170)-1000),
                wt[3]*(log(pg->polyAlen+2)*200) ,
                wt[4]*overlapOrtholog*10 , 
                wt[5]*(((log(pg->processedIntrons > 0 ? pg->processedIntrons : 1))/log(2))*600) ,
                (float)wt[6]*pow(pg->intronCount,0.5)*750 ,
                (float)wt[7]*(maxOverlap*300),
                wt[8]*((pg->coverage/100.0)*(1.0-coverFactor)*300.0),
                wt[9]*(pg->tReps*10), 
                wt[10]*pg->oldIntronCount, rawScore , rawScore/3.0);
if (pg->kaku > 0 && pg->kaku < 1000000)
    printf("<TR><TH>KA/KU mutation rate in non-syn sites vs utr with repect to parent gene<TD>%4.2f</TR>\n",  pg->kaku);
#endif
#ifdef xxx
safef(query, sizeof(query), "select * from refGene where chrom = '%d' and txEnd > %d and txStart %d and name = '%s'", 
        pg->chrom, pg->gStart, pg->gEnd , pg->overName );
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    overlappingGene = genePredLoad(row);
if (overlappingGene != NULL)
    {
    printf ("CDS exons %d ",genePredcountCdsExons(overlappingGene));
    }

#endif
printf("</tr>\n");
if ( differentString("none",pg->overName) &&
    sqlFieldIndex(conn, "refGene", "exonFrames") != -1)
    {
    safef(query, sizeof(query), 
            "select concat(exonFrames,'(',cdsStart,')') from refGene where name = '%s' and chrom = '%s'" , 
            pg->overName, pg->chrom);
    if (sqlQuickString(conn, query) != NULL)
        printf("<TR><TH>Frame of retro %s (start)<TD>%s</TR>\n",  
            pg->overName, sqlQuickString(conn, query));
    }

name = cloneString(pg->name);
chopSuffix(name);
safef(query, sizeof(query), 
        "select concat(exonFrames,'(',cdsStart,')') from rbRetroParent where name like '%s%%' and chrom = '%s'" , 
        name, pg->chrom);
if (hTableExists("rbRetroParent"))
    {
    if ( sqlQuickString(conn, query) != NULL)
        printf("<TR><TH>Frames of mapped parent %s (start)<TD>%s</TR>\n",  
            name, sqlQuickString(conn, query));
    }
printf("</TBODY></TABLE>\n");
}

void printRetroAlignments(struct psl *pslList, int startFirst, char *hgcCommand,
		     char *typeName, char *itemIn)
/* Print list of mRNA alignments. */
{
if (pslList == NULL || typeName == NULL)
    return;
printAlignmentsSimple(pslList, startFirst, hgcCommand, typeName, itemIn);

struct psl *psl = pslList;
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    if ( pslTrimToTargetRange(psl, winStart, winEnd) != NULL 
	&& 
	!startsWith("xeno", typeName)
	&& !(startsWith("user", typeName) && pslIsProtein(psl))
	&& psl->tStart == startFirst
	)
	{
        char otherString[512];
	safef(otherString, sizeof(otherString), "%d&aliTrack=%s",
	      psl->tStart, typeName);
	}
    }

}

static void displayAligns(struct sqlConnection *conn, struct mappingInfo *mi)
/* display cDNA alignments */
{
int start = cartInt(cart, "o");
char alignTbl[128];
struct psl *psl;
safef(alignTbl, sizeof(alignTbl), "%s%sAli%s", mi->tblPre, mi->geneSet,mi->suffix);

/* this should only ever have one alignment */
psl = getAlignments(conn, alignTbl, mi->pg->name);
printf("<H3>Retro Locus/Parent mRNA Alignments</H3>");
printRetroAlignments(psl, start, "hgcRetroCdnaAli", alignTbl, mi->pg->name);
pslFreeList(&psl);
}

void retroClickHandler(struct trackDb *tdb, char *mappedId)
/* Handle click on a transMap tracks */
{
struct sqlConnection *conn = hAllocConn();
struct mappingInfo *mi = mappingInfoNew(conn, tdb->tableName, mappedId);
struct psl *pslList = NULL;
char *table;

genericHeader(tdb, mappedId);
printf("<B>Description:</B> Retrogenes are processed mRNAs that are inserted back into the genome. Most are pseudogenes, and some are functional genes or anti-sense transcripts that may impede mRNA translation.<p>\n");
printf("<TABLE border=0>\n");
printf("<TR CLASS=\"transMapLayout\">\n");
printf("<TD COLSPAN=3>\n");
displaySrcGene(conn, mi);
printf("</TR>\n");
printf("<TR CLASS=\"transMapLayout\">\n");
printf("<TD>\n");
displayMappingInfo(conn, mi);
printf("<TD>\n");
#if 0
struct geneCheck *gc = displayGeneCheck(conn, &mti, mappedId);
printf("<TD>\n");
displayProtSim(conn, &mti, mappedId);
#endif
printf("</TR>\n");
#if 0
if (!sameString(gc->stat, "ok"))
    {
    printf("<TR CLASS=\"transMapLayout\">\n");
    printf("<TD COLSPAN=3>\n");
    displayGeneCheckDetails(conn, &mti, gc);
    printf("</TR>\n");
    }
#endif
printf("</TABLE>\n");
displayRetroDetails(conn, mi);
displayAligns(conn, mi);
pslList = getParentAligns(conn, mi, &table);
displayParentAligns(mi, pslList, table);
pslFreeList(&pslList);
printTrackHtml(tdb);
#if 0
geneCheckFree(&gc);
#endif
mappingInfoFree(&mi);
hFreeConn(&conn);
}

static struct genbankCds getCds(struct sqlConnection *conn, struct mappingInfo *mi)
/* Get CDS, return empty genebankCds if not found or can't parse  */
{
char query[256];
struct sqlResult *sr;
struct genbankCds cds;
char **row;

safef(query, sizeof(query),
      "select cds.name "
      "from %s.gbCdnaInfo, %s.cds "
      "where gbCdnaInfo.acc=\"%s\" and gbCdnaInfo.cds=cds.id",
      hGetDb(), hGetDb(), mi->gbAcc);

sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if ((row == NULL) || !genbankCdsParse(row[0], &cds))
    ZeroVar(&cds);  /* can't get or parse cds */
sqlFreeResult(&sr);
return cds;
}

static struct psl *loadAlign(struct sqlConnection *conn, struct mappingInfo *mi, int start)
/* load a psl that must exist */
{
char rootTable[256], table[256], query[256];
boolean hasBin;
struct sqlResult *sr;
char **row;
struct psl *psl;

safef(rootTable, sizeof(rootTable), "%s%sAli", mi->tblPre, mi->geneSet);
hFindSplitTable(seqName, rootTable, table, &hasBin);

safef(query, sizeof(query), "select * from %s where qName = '%s' and tStart = %d",
      table, mi->pg->name, start);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
psl = pslLoad(row+hasBin);
sqlFreeResult(&sr);
return psl;
}

void retroShowCdnaAli(char *mappedId)
/* Show alignment for accession, mostly ripped off from htcCdnaAli */
{
char *track = cartString(cart, "aliTrack");
char *table = cartString(cart, "table");
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *defDbConn = NULL;
struct mappingInfo *mi = mappingInfoNew(conn, table, mappedId);
struct genbankCds cds = getCds(conn, mi);
struct psl *psl;
struct dnaSeq *rnaSeq;
char acc[512];

/* Print start of HTML. */
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>%s vs Genomic [%s]</TITLE>\n</HEAD>\n\n", mi->seqId, track);

/* Look up alignment and sequence in database.  Always get sequence
 * from defaultDb */
psl = loadAlign(conn, mi, start);
if (startsWith("August",mi->geneSet))
    safef(acc, sizeof(acc), "aug-%s.T1",mi->seqId);
else
    safef(acc, sizeof(acc), "%s.%d",mi->seqId, mi->gbCurVer);
rnaSeq = hDnaSeqGet(conn, acc, "retroSeq", "retroExtFile");
if (rnaSeq == NULL)
    {
    rnaSeq = hDnaSeqGet(conn, acc, "seq", "extFile");
    if (rnaSeq == NULL)
        errAbort("can't get mRNA sequence from %s prefix %s for %s from retroSeq", 
            hGetDbName(), mi->geneSet, acc);
    }
sqlDisconnect(&defDbConn);

showSomeAlignment(psl, rnaSeq, gftDna, 0, rnaSeq->size, NULL, cds.start, cds.end);
pslFree(&psl);
dnaSeqFree(&rnaSeq);
mappingInfoFree(&mi);
hFreeConn(&conn);
}

