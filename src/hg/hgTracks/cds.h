#ifndef CDS_H
#define CDS_H

#ifndef VGFX_H
#include "vGfx.h"
#endif

#ifndef PSL_H
#include "psl.h"
#endif

#ifndef GENEPRED_H
#include "genePred.h"
#endif

#ifndef CDS_COLORS_H
#include "cdsColors.h"
#endif

#ifndef HGTRACKS_H
#include "hgTracks.h"
#endif

struct simpleFeature *splitGenePredByCodon( char *chrom, struct linkedFeatures *lf, 
        struct genePred *gp, unsigned *gaps);
int getCdsDrawOptionNum(char *mapName);
void lfSplitByCodonFromPslX(char *chromName, struct linkedFeatures *lf, 
        struct psl *psl, int sizeMul, boolean isXeno, int maxShade);
void drawCdsColoredBox(struct track *tg,  struct linkedFeatures *lf,
        int grayIx, Color *cdsColor, struct vGfx *vg, int xOff, int y,
        double scale, MgFont *font, int s, int e, int heightPer,
        boolean zoomedToCodonLevel, struct dnaSeq *mrnaSeq, struct psl
        *psl, int drawOptionNum, boolean errorColor, 
        boolean *foundStart, int maxPixels);
 int cdsColorSetup(struct vGfx *vg, struct track *tg, Color *cdsColor,
         struct dnaSeq *mrnaSeq, struct psl *psl, boolean *errorColor,
         struct linkedFeatures *lf, boolean cdsColorsMade);
#endif /* CDS_H */
