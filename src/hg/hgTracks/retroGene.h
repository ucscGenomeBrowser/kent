
#include "common.h"
#include "hCommon.h"
#include "linefile.h"
#include "portable.h"
#include "memalloc.h"
#include "localmem.h"
#include "obscure.h"
#include "dystring.h"
#include "hash.h"
#include "jksql.h"
#include "gfxPoly.h"
#include "memgfx.h"
#include "vGfx.h"
#include "browserGfx.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
#include "spDb.h"
#include "hui.h"
#include "hgFind.h"
#include "hgTracks.h"
#include "pseudoGeneLink.h"

struct linkedFeatures *lfFromRetroGene(struct pseudoGeneLink *pg);
/* Return a linked feature from a retroGene. */

void lfRetroGene(struct track *tg);
/* Load the items in one custom track - just move beds in
 * window... */

void lookupRetroNames(struct track *tg);
/* This converts the accession to a gene name where possible. */

void loadRetroGene(struct track *tg);
/* Load up RetroGenes. */

void retroGeneMethods(struct track *tg);
/* Make track of retroGenes from bed */

