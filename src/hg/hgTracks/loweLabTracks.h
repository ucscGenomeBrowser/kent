/**** Lowe lab declarations ***/

#include "rnaLpFold.h"
#include "variation.h"
#define rnaLpFoldInvDefault  FALSE

Color gbGeneColor(struct track *tg, void *item, struct vGfx *vg);
void archaeaGeneMethods(struct track *tg);
void gbGeneMethods(struct track *tg);
Color tigeGeneColor(struct track *tg, void *item, struct vGfx *vg);
void tigrGeneMethods(struct track *tg);
char *llBlastPName(struct track *tg, void *item);
void llBlastPMethods(struct track *tg);
void codeBlastMethods(struct track *tg);
void tigrOperonMethods(struct track *tg);
void rnaGenesMethods(struct track *tg);
void sargassoSeaMethods(struct track *tg);


void rnaLpFoldMethods(struct track *tg);
/* setup special methods for the RNA LP FOLD track */

