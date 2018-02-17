/* variantProjector -- use sequence alignments and indel realignment to transform genomic variants
 * to transcript and protein variants.  Compute sufficient information to predict functional
 * effects on proteins and to form HGVS g., n., and if applicable c. and p. terms. */

/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#ifndef VARIANTPROJECTOR_H
#define VARIANTPROJECTOR_H

#include "basicBed.h"
#include "dnaseq.h"
#include "genbank.h"
#include "gpFx.h"
#include "psl.h"
#include "seqWindow.h"

enum vpTxRegion
/* Which region of a transcript contains the start or end of a variant. */
    {
    vpUnknown,
    vpUpstream,
    vpDownstream,
    vpExon,
    vpIntron,
    };

struct vpTxPosition
/* A genomic start or end position projected into transcript-relative coords.
 * The position may be within the transcript itself (exonic) or not (intronic, up/downstream)
 * or on the boundary between adjacent regions.
 * If the reference genome has a deletion then a false "intron" will be present; detect this
 * by txOffset != intron3TxOffset. */
    {
    enum vpTxRegion region;        // Region of transcript in which genomic position falls.
                                   // Start pos: region to right/3'; End pos: region to left/5'
    uint txOffset;                 // * exonic: offset within transcript
                                   // * intronic: open end offset of 5' exon
                                   // * upstream: 0
                                   // * downstream: tx size (open end)
    uint gDistance;                // * intronic: distance from end of 5' exon (txOffset)
                                   // * up/downstream: distance to tx start/end (txOffset)
    uint intron3TxOffset;          // For intron, the txOffset of the 3' exon, usually same as txO
                                   // but not when genome has a deletion - then this isn't intron!
    uint intron3Distance;          // For intron, the distance to intron3TxOffset
    uint gOffset;                  // Genomic position that was projected onto this tx position
    int aliBlkIx;                  // Aligned block in or after which g pos falls or beginning/end
                                   // block if up/downstream;
                                   // this is often the exon number (or complement) but not always!
    uint gInsBases;                // Number of bases inserted into the reference genome relative
                                   // to transcript, 5' if start or 3' if end
    };

struct vpTx
/* Detailed projection of a variant onto a possibly coding transcript structure. */
    {
    struct vpTx *next;
    char *txName;                  // transcript sequence accession
    struct vpTxPosition start;     // transcript-relative coords of variant start-in-transcript
    struct vpTxPosition end;       // transcript-relative coords of variant end-in-transcript
    char *gRef;                    // genomic reference allele (on tx strand)
    char *gAlt;                    // genomic alternate allele (on tx strand); usually same as txAlt
    char *txRef;                   // transcript reference allele (if variant overlaps transcript)
    char *txAlt;                   // transcript alternate allele
    int basesShifted;              // # of bases by which genomic variant was shifted in dir of tx
    boolean genomeMismatch;        // true if the genomic reference does not match txRef
    };

struct vpPep
/* Projection of a variant onto a translated protein. */
{
    struct vpTx *next;
    char *name;                    // protein/peptide sequence accession
    uint start;                    // variant start within peptide sequence
    uint end;                      // variant end within peptide sequence
    char *ref;                     // protein reference allele
    char *alt;                     // protein alternate allele
    int rightShiftedBases;         // # of pep bases by which transcript variant was right-shifted
    boolean txMismatch;            // true if translated transcript reference does not match ref
    boolean frameshift;            // true if variant is an indel that causes a frameshift
    boolean cantPredict;           // true if it's just too complicated e.g. exonic+intronic
    // maybe someday:
    // int altStartCodonShift;        // # of transcript bases shifted to find alternate start codon
    // int altStopCodonShift;         // # of transcript bases shifted to find alternate stop codon
    // boolean riboFrameShift;        // CDS encodes ribosomal frameshift (complex translation)
    };

void vpExpandIndelGaps(struct psl *txAli, struct seqWindow *gSeqWin, struct dnaSeq *txSeq);
/* If txAli has any gaps that are too short to be introns, they are often indels that could be
 * shifted left and/or right.  If so, turn those gaps into double-sided gaps that span the
 * ambiguous region. This may change gSeqWin's range. */

struct vpTx *vpGenomicToTranscript(struct seqWindow *gSeqWin, struct bed3 *gBed3, char *gAlt,
                                   struct psl *txAli, struct dnaSeq *txSeq);
/* Project a genomic variant onto a transcript, trimming identical bases at the beginning and/or
 * end of ref and alt alleles and shifting ambiguous indel placements in the direction of
 * transcription except across an exon-intron boundary.
 * Both ref and alt must be [ACGTN]-only (no symbolic alleles like "." or "-" or "<DUP>"
 * but "<DEL>" is OK).
 * Calling vpExpandIndelGaps on txAli before calling this will improve detection of variants
 * near ambiguously placed indels between genome and transcript.
 * This may change gSeqWin's range. */

void vpTxFree(struct vpTx **pVt);
/* Free a vpTx. */

struct vpPep *vpTranscriptToProtein(struct vpTx *vpTx, struct genbankCds *cds,
                                    struct dnaSeq *txSeq, struct dnaSeq *protSeq);
/* Project a coding transcript variant onto a protein sequence, shifting position to the first
 * differing amino acid position.  Return NULL if vpTx does not overlap the coding region or
 * if unable to reliably predict the protein change. */

void vpPepFree(struct vpPep **pVp);
/* Free a vpPep. */

void vpPosGenoToTx(uint gOffset, struct psl *txAli, struct vpTxPosition *txPos, boolean isTxEnd);
/* Use txAli to project gOffset onto transcript-relative coords in txPos.
 * Set isTxEnd to TRUE if we are projecting to the end coordinate in transcript space:
 * higher genomic coord if transcript on '+' strand, lower genomic coord if tx on '-' strand. */

char *vpTxRegionToString(enum vpTxRegion region);
/* Return a static string for region.  Do not free result! */

boolean vpTxPosIsInsertion(struct vpTxPosition *startPos, struct vpTxPosition *endPos);
/* Return TRUE if startPos and endPos describe a zero-length region. */

void vpTxPosSlideInSameRegion(struct vpTxPosition *txPos, int bases);
/* Move txPos's region-appropriate offsets and distances by bases (can be negative).
 * *Caller must ensure that this will not slide us into another region!* */

boolean vpTxPosRangeIsSingleBase(struct vpTxPosition *startPos, struct vpTxPosition *endPos);
/* Return true if [startPos, endPos) is a single-base region. */

char *vpTxGetRef(struct vpTx *vpTx);
/* If vpTx->txRef is non-NULL and both start & end are exonic, return txRef;
 * otherwise return genomic.  For example, if a deletion spans exon/intron boundary, use genomic
 * ref because it includes the intron bases.  Do not free the returned value. */

struct gpFx *vpTranscriptToGpFx(struct vpTx *vpTx, struct psl *psl, struct genbankCds *cds,
                                struct dnaSeq *txSeq, struct vpPep *vpPep, struct dnaSeq *protSeq,
                                struct lm *lm);
/* Make gpFx functional prediction(s) (SO term & additional data) from vpTx and sequence. */

#endif /* VARIANTPROJECTOR_H */
