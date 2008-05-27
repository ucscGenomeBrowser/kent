/* cds.h - code for coloring of bases, codons, or alignment differences. */

#ifndef CDS_H
#define CDS_H

#ifndef HVGFX_H
#include "hvGfx.h"
#endif

#ifndef PSL_H
#include "psl.h"
#endif

#ifndef GENEPRED_H
#include "genePred.h"
#endif

#ifndef HGTRACKS_H
#include "hgTracks.h"
#endif

enum baseColorDrawOpt baseColorGetDrawOpt(struct track *tg);
/* Determine what base/codon coloring option (if any) has been selected 
 * in trackDb/cart, and gate with zoom level. */


struct simpleFeature *baseColorCodonsFromGenePred(char *chrom,
	struct linkedFeatures *lf, struct genePred *gp, unsigned *gaps,
	boolean useExonFrames, boolean colorStopStart);
/* Given an lf and the genePred from which the lf was constructed, 
 * return a list of simpleFeature elements, one per codon (or partial 
 * codon if the codon falls on a gap boundary.  If useExonFrames is true, 
 * use the frames portion of gp (which should be from a genePredExt);
 * otherwise determine frame from genomic sequence. */

void baseColorCodonsFromPsl(char *chromName, struct linkedFeatures *lf, 
        struct psl *psl, int sizeMul, boolean isXeno, int maxShade,
        enum baseColorDrawOpt drawOpt, struct track *tg);
/* Given an lf and the psl from which the lf was constructed, 
 * set lf->codons to a list of simpleFeature elements, one per codon (or partial 
 * codon if the codon falls on a gap boundary.  sizeMul, isXeno and maxShade
 * are for defaulting to one-simpleFeature-per-exon if cds is not found. */


enum baseColorDrawOpt baseColorDrawSetup(struct hvGfx *hvg, struct track *tg,
			struct linkedFeatures *lf,
			struct dnaSeq **retMrnaSeq, struct psl **retPsl);
/* Returns the CDS coloring option, allocates colors if necessary, and 
 * returns the sequence and psl record for the given item if applicable. 
 * Note: even if base coloring is not enabled, this will return psl and 
 * mrna seq if query insert/polyA display is enabled. */

void baseColorDrawItem(struct track *tg,  struct linkedFeatures *lf,
        int grayIx, struct hvGfx *hvg, int xOff, int y,
        double scale, MgFont *font, int s, int e, int heightPer,
        boolean zoomedToCodonLevel, struct dnaSeq *mrnaSeq, struct psl *psl,
	enum baseColorDrawOpt drawOpt,
        int maxPixels, int winStart, Color originalColor);
/*draw a box that is colored by the bases inside it and its
 * orientation. Stop codons are red, start are green, otherwise they
 * alternate light/dark blue colors. */

void baseColorOverdrawDiff(struct track *tg,  struct linkedFeatures *lf,
			   struct hvGfx *hvg, int xOff,
			   int y, double scale, int heightPer,
			   struct dnaSeq *mrnaSeq, struct psl *psl,
			   int winStart, enum baseColorDrawOpt drawOpt);
/* If we're drawing different bases/codons, and zoomed out past base/codon 
 * level, draw 1-pixel wide red lines only where bases/codons differ from 
 * genomic.  This tests drawing mode and zoom level but assumes that lf itself 
 * has been drawn already and we're not in dense mode etc. */

void baseColorOverdrawQInsert(struct track *tg,  struct linkedFeatures *lf,
			      struct hvGfx *hvg, int xOff,
			      int y, double scale, int heightPer,
			      struct dnaSeq *mrnaSeq, struct psl *psl,
			      int winStart, enum baseColorDrawOpt drawOpt,
			      boolean indelShowQInsert, boolean indelShowPolyA);
/* If applicable, draw 1-pixel wide orange lines for query insertions in the
 * middle of the query, 1-pixel wide blue lines for query insertions at the 
 * end of the query, and 1-pixel wide green (instead of blue) when a query 
 * insertion at the end is a valid poly-A tail. */

void baseColorDrawCleanup(struct linkedFeatures *lf, struct dnaSeq **pMrnaSeq,
			  struct psl **pPsl);
/* Free structures allocated just for base/cds coloring. */


struct simpleFeature *baseColorCodonsFromDna(int frame, int chromStart,
					     int chromEnd, struct dnaSeq *seq,
					     bool reverse);
/* Create list of codons from a DNA sequence */

void baseColorDrawRulerCodons(struct hvGfx *hvg, struct simpleFeature *sfList,
                double scale, int xOff, int y, int height, MgFont *font, 
                int winStart, int maxPixels, bool zoomedToText);
/* Draw amino acid translation of genomic sequence based on a list
   of codons. Used for browser ruler in full mode*/

void baseColorSetCdsBounds(struct linkedFeatures *lf, struct psl *psl,
                           struct track *tg);
/* set CDS bounds in linked features for a PSL.  Used when zoomed out too far
 * for codon or base coloring, but still want to render CDS bounds */

#endif /* CDS_H */
