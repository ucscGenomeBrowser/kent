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

struct simpleFeature *splitGenePredByCodon( char *chrom, 
        struct linkedFeatures *lf, struct genePred *gp, unsigned
        *gaps, boolean extraInfo, boolean colorStopStart);
/*divide a genePred record into a linkedFeature, where each simple
  feature is a 3-base codon (or a partial codon if on a gap boundary).
  It starts at the cdsStarts position on the genome and goes to 
  cdsEnd. It only relies on the genomic sequence to determine the
  frame so it works with any gene prediction track*/

int getCdsDrawOptionNum(struct track *tg);
/*query the cart for the current track's CDS coloring option. See
 * cdsColors.h for return value meanings*/

void lfSplitByCodonFromPslX(char *chromName, struct linkedFeatures *lf, 
        struct psl *psl, int sizeMul, boolean isXeno, int maxShade,
        int displayOption);
/*divide a pslX record into a linkedFeature, where each simple feature
 * is a 3-base codon (or a partial codon if on a boundary). Uses
 * GenBank to get the CDS start/stop of the psl record and also grabs
 * the sequence. This takes care of hidden gaps in the alignment, that
 * alter the frame. Therefore this function relies on the mRNA
 * sequence (rather than the genomic) to determine the frame.*/


void drawCdsColoredBox(struct track *tg,  struct linkedFeatures *lf,
        int grayIx, Color *cdsColor, struct vGfx *vg, int xOff, int y,
        double scale, MgFont *font, int s, int e, int heightPer,
        boolean zoomedToCodonLevel, struct dnaSeq *mrnaSeq, struct psl
        *psl, int drawOptionNum, boolean errorColor, 
        boolean *foundStart, int maxPixels, int winStart, Color
        originalColor);
/*draw a box that is colored by the bases inside it and its
 * orientation. Stop codons are red, start are green, otherwise they
 * alternate light/dark blue colors. These are defined in
 * cdsColors.h*/


int cdsColorSetup(struct vGfx *vg, struct track *tg, Color *cdsColor,
         struct dnaSeq **mrnaSeq, struct psl **psl, boolean *errorColor,
         struct linkedFeatures *lf, boolean cdsColorsMade);
/*gets the CDS coloring option, allocates colors, and returns the
 * sequence and psl record for the given lf->name (only returns
 sequence and psl for mRNA, EST, or xenoMrna*/

struct simpleFeature *splitDnaByCodon(int frame, int chromStart, int chromEnd,
                                            struct dnaSeq *seq, bool reverse);
/* Create list of codons from a DNA sequence */

void makeCdsShades(struct vGfx *vg, Color *cdsColor);
/* setup CDS colors */

Color colorAndCodonFromGrayIx(struct vGfx *vg, char *codon, int grayIx, 
                                        Color *cdsColor, Color ixColor);
/*convert grayIx value to color and codon which
 * are both encoded in the grayIx*/

void drawGenomicCodons(struct vGfx *vg, struct simpleFeature *sfList,
                double scale, int xOff, int y, int height, MgFont *font, 
                Color *cdsColor,int winStart, int maxPixels, bool zoomedToText);
/* Draw amino acid translation of genomic sequence based on a list
   of codons. Used for browser ruler in full mode*/

void drawCdsDiffCodonsOnly(struct track *tg,  struct linkedFeatures *lf,
			   Color *cdsColor, struct vGfx *vg, int xOff,
			   int y, double scale, int heightPer,
			   struct dnaSeq *mrnaSeq, struct psl *psl,
			   int winStart);
/* Draw red boxes only where mRNA codons differ from genomic.  This assumes
 * that lf has been drawn already, we're zoomed out past zoomedToCdsColorLevel,
 * we're not in dense mode etc. */

#endif /* CDS_H */
