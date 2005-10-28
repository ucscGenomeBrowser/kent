
#include "retroGene.h"

static char const rcsid[] = "$Id: retroGene.c,v 1.8 2005/10/28 17:02:41 kent Exp $";

struct linkedFeatures *lfFromRetroGene(struct pseudoGeneLink *pg)
/* Return a linked feature from a retroGene. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf, *sfList = NULL;
int grayIx = grayInRange(pg->score, 0, 1000);
int *starts = pg->chromStarts, start;
int *sizes = pg->blockSizes;
int blockCount = pg->blockCount, i;

assert(starts != NULL && sizes != NULL && blockCount > 0);

AllocVar(lf);
lf->grayIx = grayIx;
safef(lf->name, sizeof(lf->name), "%s", pg->name);
lf->orientation = orientFromChar(pg->strand[0]);
for (i=0; i<blockCount; ++i)
    {
    AllocVar(sf);
    start = starts[i] + pg->chromStart;
    sf->start = start;
    sf->end = start + sizes[i];
    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
lf->components = sfList;
linkedFeaturesBoundsAndGrays(lf);
lf->tallStart = pg->thickStart;
lf->tallEnd = pg->thickEnd;
return lf;
}
void lfRetroGene(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct linkedFeatures *lfList = NULL, *lf;
struct pseudoGeneLink *pg = NULL, *list = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    pg = pseudoGeneLinkLoad(row+rowOffset);
    slAddHead(&list, pg);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

for (pg = list; pg != NULL; pg = pg->next)
    {
    lf = lfFromRetroGene(pg);
    lf->grayIx = 9;
    slReverse(&lf->components);
    slAddHead(&lfList,lf);
    }
tg->items = lfList;
}

void lookupRetroNames(struct track *tg)
/* This converts the accession to a gene name where possible. */
{
struct linkedFeatures *lf;
struct sqlConnection *conn = hAllocConn();
boolean isNative = sameString(tg->mapName, "pseudoGeneLink");
char *refGeneLabel = cartUsualString(cart, (isNative ? "pseudoGeneLink.label" : "xenoRefGene.label"), "gene");
boolean useGeneName = sameString(refGeneLabel, "gene")
    || sameString(refGeneLabel, "both");
boolean useAcc = sameString(refGeneLabel, "accession")
    || sameString(refGeneLabel, "both");

for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    struct dyString *name = dyStringNew(64);
    char *geneName = lf->extra;
    if (geneName == NULL)
        geneName = lf->name;
    if ((useGeneName || useAcc) && !isNative)
        {
        char *org = getOrganismShort(conn, lf->name);
        if (org != NULL)
            dyStringPrintf(name, "%s ", org);
        }
    /* prepend retro- to parent gene name */
    dyStringAppend(name, "retro-");
    /* lookup name using knowngene method */
    if ((useGeneName) && hTableExists("refLink") && hTableExists("knownGeneLink"))
        {
        char *gene = NULL;
        char cond_str[256];
        char *seqType;
        char *refSeqName;
        char *proteinID;
        char *hugoID;
        sprintf(cond_str, "name='%s' and seqType='g'", geneName);
        seqType = sqlGetField(conn, database, "knownGeneLink", "seqType", cond_str);

        if (seqType != NULL)
            {
	    // special processing for RefSeq DNA based genes
    	    sprintf(cond_str, "mrnaAcc = '%s'", geneName);
    	    refSeqName = sqlGetField(conn, database, "refLink", "name", cond_str);
	    if (refSeqName != NULL)
		{
		gene = cloneString(refSeqName);
		}
	    }
	else if (protDbName != NULL)
	    {
	    sprintf(cond_str, "mrnaID='%s'", geneName);
	    proteinID = sqlGetField(conn, database, "spMrna", "spID", cond_str);
 
            sprintf(cond_str, "displayID = '%s'", proteinID);
	    hugoID = sqlGetField(conn, protDbName, "spXref3", "hugoSymbol", cond_str);
	    if (!((hugoID == NULL) || (*hugoID == '\0')) )
		{
		gene = cloneString(hugoID);
		}
	    else
	    	{
                char query[256];
                struct sqlResult *sr;
                char **row;
	    	sprintf(query,"select refseq from %s.mrnaRefseq where mrna = '%s';",  
		        database, geneName);

	    	sr = sqlGetResult(conn, query);
	    	row = sqlNextRow(sr);
	    	if (row != NULL)
    	    	    {
    	    	    sprintf(query, "select * from refLink where mrnaAcc = '%s'", row[0]);
    	    	    sqlFreeResult(&sr);
    	    	    sr = sqlGetResult(conn, query); 
    	    	    if ((row = sqlNextRow(sr)) != NULL)
        	    	{
                        if (strlen(row[0]) > 0)
                            gene = cloneString(row[0]);
		    	}
            	    sqlFreeResult(&sr);
	    	    }
	    	else
            	    {
	    	    sqlFreeResult(&sr);
	    	    }
	    	}
	    }
        if (gene != NULL)
            {
            dyStringAppend(name, gene);
            if (useAcc)
                dyStringAppendC(name, '/');
            }
        else
            dyStringAppend(name, geneName);
        }
    if (useAcc)
        dyStringAppend(name, geneName);
    lf->extra = dyStringCannibalize(&name);
    }
hFreeConn(&conn);
}

void loadRetroGene(struct track *tg)
/* Load up RetroGenes. */
{
enum trackVisibility vis = tg->visibility;
lfRetroGene(tg);
if (vis != tvDense)
    {
    lookupRetroNames(tg);
    slSort(&tg->items, linkedFeaturesCmpStart);
    }
vis = limitVisibility(tg);
}

Color retroGeneColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw retroGene in. */
{
return vgFindColorIx(vg, CHROM_20_R, CHROM_20_G, CHROM_20_B);
}

void retroGeneMethods(struct track *tg)
/* Make track of retroGenes from bed */
{
tg->loadItems = loadRetroGene;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemColor = retroGeneColor;
}

