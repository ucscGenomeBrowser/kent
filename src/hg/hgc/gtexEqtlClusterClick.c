/* Details page for GTEx eQTL Clusters */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hCommon.h"
#include "web.h"
#include "gtexTissue.h"
#include "gtexInfo.h"
#include "gtexEqtlCluster.h"
#include "hgc.h"

static struct gtexEqtlCluster *getGtexEqtl(char *item, char *chrom, int start, int end, char *table)
/* Retrieve this item from the track table */
{
char *gene = firstWordInLine(cloneString(item));
struct sqlConnection *conn = hAllocConn(database);
struct gtexEqtlCluster *eqtls = NULL, *eqtl;
char **row;
int offset;
char where[512];
sqlSafefFrag(where, sizeof(where), "target='%s'", gene);
struct sqlResult *sr = hRangeQuery(conn, table, chrom, start, end, where, &offset); 
while ((row = sqlNextRow(sr)) != NULL)
    {
    eqtl = gtexEqtlClusterLoad(row+offset);
    slAddHead(&eqtls, eqtl);
    }
slReverse(&eqtls);
sqlFreeResult(&sr);
hFreeConn(&conn);
return eqtls;
}

static char *getGeneDescription(struct sqlConnection *conn, char *geneName)
/* Return description from KnownGenes track */
{
char query[256];
sqlSafef(query, sizeof query,
                "SELECT kgXref.description FROM kgXref WHERE geneSymbol='%s'", geneName);
return sqlQuickString(conn, query);
}

static void printMinorAlleleFreq(char *rsId, struct sqlConnection *conn)
/* Print minor allele frequency for a SNP (from UCSC dbSNP table) */
{
#define ALLELE_COUNT 10
char query[256];
sqlSafef(query, sizeof query, "SELECT alleleFreqs FROM snp147 WHERE name='%s'", rsId);
double freqs[ALLELE_COUNT];
int count = sqlDoubleArray(sqlQuickString(conn, query), freqs, ALLELE_COUNT);
doubleSort(count, freqs);
printf("<br><b>Minor allele frequency (1000 Genomes):</b> %.0f%%\n", 100.0 * freqs[count-2]);
}

static void printGwasCatalogTrait(char *rsId, struct sqlConnection *conn)
/* Print trait/disease for a SNP (from UCSC gwasCatalog table) */
{
char query[256];
sqlSafef(query, sizeof query, "SELECT count(*) FROM gwasCatalog WHERE name='%s'", rsId);
int count = sqlQuickNum(conn, query);
if (count)
    {
    sqlSafef(query, sizeof query, "SELECT trait FROM gwasCatalog WHERE name='%s' LIMIT 1", rsId);
    char *trait = sqlQuickString(conn, query);
    printf("<br><b>GWAS disease or trait");
    if (count > 1)
        printf(" (1 of %d)", count);
    printf(": </b>%s <a target='_blank' href='https://www.ebi.ac.uk/gwas/search?query=%s'>"
                    "GWAS Catalog</a>\n", trait, rsId);
    }
}

static void printEqtlRegion(struct gtexEqtlCluster *eqtl, char *table, struct sqlConnection *conn)
/* Print position of region encompassing all identified eQTL's for this gene */
{
#define FLANK  1000 
char query[256];
sqlSafef(query, sizeof query, "SELECT MIN(chromStart) from %s WHERE target='%s'", 
            table, eqtl->target);
int start = sqlQuickNum(conn, query) - FLANK;
sqlSafef(query, sizeof query, "SELECT MAX(chromEnd) from %s WHERE target='%s'", 
            table, eqtl->target);
int end = sqlQuickNum(conn, query) + FLANK;
char posLink[1024];
safef(posLink, sizeof posLink,"<a href='%s&db=%s&position=%s%%3A%d-%d'>%s:%d-%d</a>",
        hgTracksPathAndSettings(), database,
            eqtl->chrom, start+1, end,
            eqtl->chrom, start+1, end);
printf("<br><b>Region containing eQTLs for this gene:</b> %s (%d bp, including +-%dbp flank)\n", 
        posLink, end-start, FLANK);
}

static void printClusterDetails(struct gtexEqtlCluster *eqtl, char *table)
/* Print details of an eQTL cluster */
{
webNewSection("eQTL Cluster Details");
char *version = gtexVersion(table);
struct gtexTissue *tissues = gtexGetTissues(version);
struct hash *tissueHash = hashNew(0);
struct gtexTissue *tis = NULL;
for (tis = tissues; tis != NULL; tis = tis->next)
    hashAdd(tissueHash, tis->name, tis);
printf("<table id='eqtls' cellspacing=1 cellpadding=3>\n");
printf("<style>#eqtls th {text-align: left; background-color: #F3E0BE;}</style>");
printf("<tr><th>&nbsp;&nbsp;&nbsp;</th><th>Tissue</th><th>Effect &nbsp;&nbsp;</th><th>Probability </th></tr>\n");
int i;
for (i=0; i<eqtl->expCount; i++)
    {
    double effect= eqtl->expScores[i];
    double prob = eqtl->expProbs[i];
    struct gtexTissue *tis = (struct gtexTissue *)hashFindVal(tissueHash, eqtl->expNames[i]);
    unsigned color = tis ? tis->color : 0;       // BLACK
    char *name = tis ? tis->description : "Unknown";
    printf("<tr><td bgcolor=#%06X></td><td>%s</td><td>%s%0.2f</td><td>%0.2f</td></tr>\n", 
                                color, name, effect < 0 ? "" : "+", effect, prob); 
    }
printf("</table>");
webEndSection();
}

void doGtexEqtlDetails(struct trackDb *tdb, char *item)
/* Details of GTEx eQTL item */
{
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct gtexEqtlCluster *eqtl = getGtexEqtl(item, chrom, start, end, tdb->table);
char *geneName = eqtl->target;


genericHeader(tdb, item);
printf("<b>Gene: </b>");
struct sqlConnection *conn = hAllocConn(database);
char *desc = getGeneDescription(conn, geneName);
if (desc == NULL)
    printf("%s\n", geneName);
else
    {
    printf("<a target='_blank' href='%s?db=%s&hgg_gene=%s'>%s</a><br>\n",
                        hgGeneName(), database, geneName, geneName);
    printf("<b>Description:</b> %s\n", desc);
    }

// TODO: Consider adding Ensembl gene ID, GENCODE biotype and class (as in gtexGene track)
printf("<br><b>Variant: </b>%s ", eqtl->name);
if (startsWith("rs", eqtl->name))
    {
    printDbSnpRsUrl(eqtl->name, "dbSNP");
    printMinorAlleleFreq(eqtl->name, conn);
    printGwasCatalogTrait(eqtl->name, conn);
    }
else
    printf("%s\n", eqtl->name);

char posLink[1024];
safef(posLink, sizeof posLink,"<a href='%s&db=%s&position=%s%%3A%d-%d'>%s:%d-%d</a>",
        hgTracksPathAndSettings(), database,
            eqtl->chrom, eqtl->chromStart+1, eqtl->chromEnd,
            eqtl->chrom, eqtl->chromStart+1, eqtl->chromEnd);
printf("<br><b>Position:</b> %s\n", posLink);

printf("<br><b>Score:</b> %d\n", eqtl->score);

printEqtlRegion(eqtl, tdb->table, conn);

// print link to GTEx portal
printf("<br><a target='_blank' href='https://www.gtexportal.org/home/bubbleHeatmapPage/%s'>"
        "View eQTLs for this gene at the GTEx portal<a>\n", 
                geneName);
printf("<br><b>Number of tissues with this eQTL:</b> %d\n", eqtl->expCount);
hFreeConn(&conn);

printClusterDetails(eqtl, tdb->table);

webNewEmptySection();
printTrackHtml(tdb);
}



