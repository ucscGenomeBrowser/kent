/* transcriptome - stuff to handle loading and coloring of the
   affymetrix transcriptome tracks. These are sets of wiggle tracks
   representing probes tiled through non-repetitive portions of the
   human genome at 5 bp resolution. Also included are transfrags
   tracks which are bed tracks indicating what is thought to be
   expressed.
*/

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "bed.h"

static char const rcsid[] = "$Id: transcriptome.c,v 1.1 2004/07/19 21:46:38 sugnet Exp $";



static Color affyTransfragColor(struct track *tg, void *item, struct vGfx *vg)
/* Color for an affyTransfrag item. Different colors are returned depending
   on score. 
   0 - Passes all filters
   1 - Overlaps a pseudogene
   2 - Overlaps a blat hit
   3 - Overlaps both a pseudogene and a blat hit.
*/
{
static Color cleanCol = MG_WHITE;
static Color blatCol = MG_WHITE;
static Color pseudoCol = MG_WHITE;
struct bed *bed = item;

/* If first time through make the colors. */
if(cleanCol == MG_WHITE)
    {
    cleanCol = tg->ixColor;
    blatCol = vgFindColorIx(vg, 100, 100, 160);
    pseudoCol = vgFindColorIx(vg, 190, 190, 230);
    }

switch(bed->score) 
    {
    case 0 :
	return cleanCol;
    case 1 :
	return pseudoCol;
    case 2 :
	return blatCol;
    case 3 : 
	return pseudoCol;
    default:
	errAbort("Don't recognize score for transfrag %s", bed->name);
    };
return tg->ixColor;
}

static struct slList *affyTransfragLoader(char **row)
/* Wrapper around bedLoad5 to conform to ItemLoader type. */
{
return (struct slList *)bedLoad5(row);
}

static void affyTransfragsLoad(struct track *tg)
/* Load the items from the chromosome range indicated. Filter as
   necessary to remove things the user has requested not to see. Score
   indicates the status of a transfrag:
   0 - Passes all filters
   1 - Overlaps a pseudogene
   2 - Overlaps a blat hit
   3 - Overlaps both a pseudogene and a blat hit.
*/
{
struct bed *bed = NULL, *bedNext = NULL, *bedList = NULL;
boolean skipPseudos = cartUsualBoolean(cart, "affyTransfrags.skipPseudos", TRUE);
boolean skipDups = cartUsualBoolean(cart, "affyTransfrags.skipDups", FALSE);
/* Use simple bed loader to do database work. */
bedLoadItem(tg, tg->mapName, affyTransfragLoader);

/* Now filter based on options. */
for(bed = tg->items; bed != NULL; bed = bedNext) 
    {
    bedNext = bed->next;
    if((bed->score == 1 || bed->score == 3) && skipPseudos) 
	bedFree(&bed);
    else if((bed->score == 2 || bed->score == 3) && skipDups)
	bedFree(&bed);
    else 
	{
	slAddHead(&bedList, bed);
	}
    }
slReverse(&bedList);
tg->items = bedList;
}

    
void affyTransfragsMethods(struct track *tg)
/* Substitute a new load method that filters based on score. Also add
   a new itemColor() method that draws transfrags that overlap dups
   and pseudoGenes in a different color. */
{
tg->itemColor = affyTransfragColor;
tg->loadItems = affyTransfragsLoad;
}
