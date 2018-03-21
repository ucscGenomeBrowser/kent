/* hgHgvs - support for a subset of the Human Genome Variation Society's (HGVS) nomenclature
 * for describing variants with respect to a specific gene sequence (or genome assembly).
 * See http://www.hgvs.org/mutnomen/ */

/* Copyright (C) 2016 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#ifndef HGHGVS_H
#define HGHGVS_H

#include "bed.h"
#include "dnaseq.h"
#include "seqWindow.h"
#include "variantProjector.h"

/* The full nomenclature is extremely complicated, able to encode things such as gene fusions and
 * advanced clinical info (e.g. "=/" for somatic mosaicism, "=//" for chimerism).  UCSC supports
 * substitutions, insertions, deletions, duplications and inversions.  Conversions are parsed out
 * of HGVS terms but not detected in genomic variants when generating HGVS terms.
 * UCSC does not fully support repeated sequences because in practice they seem to be frequently
 * incorrect and inherently error-prone.
 *
 * At the same time, since the spec has repeatedly changed, we will need to be flexible in our
 * parsing in order to support previously published HGVS (or HGVS-like) terms. */

enum hgvsSeqType
    // HGVS describes changes relative to several types of sequence: genomic, coding, protein, etc
    {
    hgvstUndefined,  // the usual int default value means we haven't actually checked
    hgvstCoding,     // "c.": Coding cDNA sequence only. Beware: complex coords for intron & UTR.
    hgvstGenomic,    // "g.": Genomic DNA
    hgvstMito,       // "m.": Mitochondrial DNA
    hgvstNoncoding,  // "n.": non-coding cDNA w/complex coords for intron
    hgvstRna,        // "r.": RNA (like DNA but lowercase and 'u' instead of 'T')
    hgvstProtein,    // "p.": Protein
    };

struct hgvsVariant
    // Components that we are able parse out of an HGVS term. (there's more to HGVS than just this)
    {
    struct hgvsVariant *next;
    char *seqAcc;            // The reference sequence for the variant (optional -- may be NULL!)
    char *seqGeneSymbol;     // Usually NULL, but DNA/RNA can have HGNC gene symbol in ()'s.
    char *changes;           // The sequence change(s) (following the position in nucleotide terms,
                             // with embedded position in protein terms); can be parsed into
                             // syntax tree by hgvsParseNucleotideChange (below)
    int start1;              // 1-based start of the variant in reference seq: can be negative !
                             // For [cnr]. terms with complex intron coords, this is the
                             // anchor coord (nearest exon boundary)
    int end;                 // End of the variant in the reference seq; can be negative !
                             // For a range, this is the second anchor coord; else same as start
    int startOffset;         // offset into intron; 1-based, 0 means N/A; can be negative!
    int endOffset;           // offset into intron; 0 means N/A; can be negative!
                             // For a range, this is the second offset; else same as startOffset
    enum hgvsSeqType type;   // type of sequence: genomic/mito, coding, noncoding, protein
    // These two fields apply only to hgvstCoding ("c.") terms and Rna ("r.") for coding RNAs:
    boolean startIsUtr3;     // TRUE if start is relative to *end* of coding sequence
    boolean endIsUtr3;       // TRUE if end is relative to *end* of coding sequence
    };

//
// HGVS sequence change(s) syntax tree
//
// An HGVS description contains two parts: position and sequence change.
// Positions may be simple 1-based, nested ranges, or relative to various CDS boundaries.
// Currently positions are parsed using regular expressions in hgHgvs.c
// Sequence changes may contain nested HGVS terms (insertion from some other sequence).
// They are tokenized and parsed in hgHgvsParse.c.

enum hgvsChangeType
    // There may be no change, or a change that specifies ref and/or alt bases, or a change
    // in which the sequence is all implied by the reference sequence.
    {
    hgvsctUndefined,
    hgvsctNoChange,    // No change; ref sequence is implied, alt sequence is the same.
    hgvsctSubst,       // Substitution: ref sequence and (possibly complex) alternate sequence
    hgvsctDel,         // Deletion; ref sequence is implied (or maybe asserted).
    hgvsctDup,         // Duplication; ref sequence is implied (or maybe asserted)
    hgvsctInv,         // Inversion; ref sequence is implied (or maybe asserted)
    hgvsctIns,         // Insertion: ref is implied; (possibly complex) inserted sequence follows
    hgvsctCon,         // Conversion: basically Deletion followed by Insertion
    hgvsctRepeat,      // Ref sequence is implied (first unit); alt sequence is some # of copies
    hgvsctFrameshift,  // First alt amino acid and possibly offset of early stop
    };

// Sometimes instead of a sequence (or in addition to a del, dup etc), a length is given,
// sometimes not.  Use int's sign to specify whether length was included or not.
#define HGVS_LENGTH_UNSPECIFIED -1

enum hgvsSeqChangeType
    // A sequence is usually just a sequence, but it might come from a nested expression and/or
    // be subjected to yet another change.
    {
    hgvsstUndefined,
    hgvsstSimple,        // IUPAC single-letter codes + '?' if some unspecified
    hgvsstLength,        // Only the length of the sequence is provided
    hgvsstNestedChange,  // Simple sequence... and another change that's applied to this one.
    hgvsstNestedTerm,    // HGVS term for sequence copied from some other reference sequence.
    };

union hgvsSeqValue
    // A sequence string, nested change, or nested term (external sequence)
    {
    char *seq;                      // IUPAC single-letter codes + '?' if some unspecified
    int length;                     // Sequence length (content unspecified)
    struct hgvsVariant term;        // Nested HGVS term: acc, pos, possibly change
    };

struct hgvsChangeRefAlt
    // 'Before and after' (reference and alternate) sequence representations
    {
    char *refSequence;            // NULL if the reference sequence is implied by the position,
                                  // or if only length is asserted; otherwise it's asserted
                                  // reference sequence in IUPAC single-letter codes
                                  // and when some but not all bases are included in HGVS term,
                                  // '?' for unspecified bases.
    int refLength;                // Sometimes, (redundant) length is given after del, dup, etc.
    enum hgvsSeqChangeType altType; // Type of alternate sequence -- can be complex.
    union hgvsSeqValue altValue;  // IUPAC single-letter sequence, or a sequence of nested changes
    };

struct hgvsChangeFrameshift
    // Info that may or may not be given along with a frameshift that disrupts the protein sequence
    {
    int offset;                   // The number of AA bases downstream of a new termination
    boolean offsetValid;          // FALSE if offset was not provided
    boolean elongation;           // TRUE if offset appears after '*', i.e. after old termination
    char *altSequence;            // Sequence of IUPAC single-letter AA codes or NULL if not given
    char refBase;                 // IUPAC single-letter AA code (or '\0' if not given)
    };

struct hgvsChangeRepeat
    // Repeat count (range) and optional repeated sequence
    {
    char *seq;                    // The sequence that is repeated, may be NULL (implied from ref)
    int min;                      // Minimum repeat count specified
    int max;                      // Max repeat count specified
    };

union hgvsChangeValue
    // Depending on type of change, ref & alt sequences, frameshift description or repeat count
    {
    struct hgvsChangeRefAlt refAlt;  // A description of reference and alternate sequence
    struct hgvsChangeFrameshift fs;  // Optional info about early termination of translation
    struct hgvsChangeRepeat repeat;  // Optional repeated sequence and min/max observed counts.
    int count;                       // Repeat unit count in alternate sequence (ref count is 1)
    };

struct hgvsChange
    // Usually this contains a sequence of bases from the reference and a sequence of bases from
    // the alternate allele, but can contain nested HGVS terms.  For deletions, reference bases
    // are usually not specified, except for protein which includes the first and last amino acid.
    {
    struct hgvsChange *next;           // One HGVS term can specify a sequence of changes
    enum hgvsChangeType type;          // HGVS operator: >, del, ins, fs etc.
    union hgvsChangeValue value;       // the actual sequences changed (possibly complex)
    };

//
// HGVS output option bit flags
//
// Output an HGVS genomic (g.) term:
#define HGVS_OUT_G  0x01
// Output either an HGVS coding (c.) term if applicable, otherwise noncoding (n.) term:
#define HGVS_OUT_CN 0x02
// Output an HGVS protein (p.) term if applicable:
#define HGVS_OUT_P  0x04
// Add parentheses around predicted protein (p.) changes e.g. p.(Arg159del):
#define HGVS_OUT_P_ADD_PARENS 0x10
// Add deleted sequence to delins changes (e.g. show 'delAGinsTT' instead of 'delinsTT'):
#define HGVS_OUT_BREAK_DELINS 0x20


void hgvsVariantFree(struct hgvsVariant **pHgvs);
// Free *pHgvs and its contents, and set *pHgvs to NULL.

struct hgvsVariant *hgvsParseTerm(char *term);
/* If term is a parseable form of HGVS, return the parsed representation, otherwise NULL.
 * This does not check validity of accessions or alleles. */

struct hgvsVariant *hgvsParsePseudoHgvs(char *db, char *term);
/* Attempt to parse things that are not strict HGVS, but that people might intend as HGVS. */

boolean hgvsValidate(char *db, struct hgvsVariant *hgvs, char **retFoundAcc, int *retFoundVersion,
                     char **retDiffRefAllele);
/* Return TRUE if hgvs coords are within the bounds of the sequence for hgvs->seqAcc.
 * Note: Transcript terms may contain coords outside the bounds (upstream, intron, downstream) so
 * those can't be checked without mapping the term to the genome; this returns TRUE if seq is found.
 * If retFoundAcc is not NULL, set it to our local accession (which may be missing the .version
 * of hgvs->seqAcc) or NULL if we can't find any match.
 * If retFoundVersion is not NULL and hgvs->seqAcc has a version number (e.g. NM_005957.4),
 * set retFoundVersion to our version from latest GenBank, otherwise 0 (no version for LRG).
 * If coords are OK and retDiffRefAllele is not NULL: if our sequence at the coords
 * matches hgvs->refAllele then set it to NULL; if mismatch then set it to our sequence. */

struct bed *hgvsMapToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable);
/* Return a bed6 with the variant's span on the genome and strand, or NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */

struct bed *hgvsValidateAndMap(struct hgvsVariant *hgvs, char *db, char *term,
                               struct dyString *dyWarn, char **retPslTable);
/* If we have sequence for hgvs->seqAcc and the hgvs coords are within the bounds
 * of that sequence, and we are able to map the coords in seqAcc to genomic coords in
 * db, return a bed6 with those coords and strand.  If unsuccessful, or if the reference
 * sequence differs at the mapped location, then write a warning message to dyWarn.
 * If mapping is successful and retPslTable is not NULL, set it to the psl table
 * used for mapping. */

struct hgvsChange *hgvsParseNucleotideChange(char *changeStr, enum hgvsSeqType type,
                                             struct dyString *dyError);
/* Return a parse tree of an HGVS nt sequence change (the part after the position range)
 * possibly followed by additional changes.  If there are any parse errors, they will be
 * appended to dyError. */

struct vcfRow
/* char* fields of VCF row (no genotypes) except POS (int for sorting) and QUAL (omitted). */
// See src/inc/vcf.h and VCF spec for definitions of fields.
// Currently this is used only in the context of HGVS.  If it turns out to be useful elsewhere
// then it could be libified on its own or perhaps into vcf.h.
    {
    struct vcfRow *next;
    char *chrom;
    int pos;
    char *id;
    char *ref;
    char *alt;
    char *filter;
    char *info;
    };

int vcfRowCmp(const void *va, const void *vb);
/* Compare for sorting based on chrom,pos. */

void vcfRowTabOutList(FILE *f, struct vcfRow *rowList);
/* Write out each row of VCF to f. */

// VCF header definitions of FILTER and INFO terms created by hgvsToVcfRow:
#define HGVS_VCF_HEADER_DEFS \
        "##FILTER=<ID=HgvsRefAssertedMismatch," \
        "Description=\"Asserted reference sequence in HGVS term does not match actual " \
        "reference sequence\">\n" \
        "##FILTER=<ID=HgvsRefGenomicMismatch," \
        "Description=\"HGVS reference sequence does not match genomic sequence; " \
        "HGVS reference sequence is included in ALT\">\n" \
        "##INFO=<ID=DupToIns,Number=0,Type=Flag," \
        "Description=\"HGVS dup (duplication) was converted to insertion\">\n" \
        "##INFO=<ID=BasesShifted,Number=1,Type=Integer," \
        "Description=\"Position of HGVS variant was shifted this number of bases to the left\">\n"

struct vcfRow *hgvsToVcfRow(char *db, char *term, boolean doLeftShift, struct dyString *dyError);
/* Convert HGVS to a row of VCF suitable for sorting & printing.  If unable, return NULL and
 * put the reason in dyError.  Protein terms are ambiguous at the nucleotide level so they are
 * not supported at this point. */

uint hgvsTxToCds(uint txOffset, struct genbankCds *cds, boolean isStart, char pPrefix[2]);
/* Return the cds-relative HGVS coord and prefix corresponding to 0-based txOffset & cds. */

char *hgvsGFromVariant(struct seqWindow *gSeqWin, struct bed3 *variantBed, char *alt, char *acc,
                       boolean breakDelIns);
/* Return an HGVS g. string representing the genomic variant at the position of variantBed with
 * reference allele from gSeqWin and alternate allele alt.  If acc is non-NULL it is used
 * instead of variantBed->chrom.
 * If breakDelIns, then show deleted bases (eg show 'delAGinsTT' instead of 'delinsTT'). */

char *hgvsNFromVpTx(struct vpTx *vpTx, struct seqWindow *gSeqWin, struct psl *txAli,
                    struct dnaSeq *txSeq, boolean breakDelIns);
/* Return an HGVS n. (noncoding transcript) term for a variant projected onto a transcript.
 * gSeqWin must already have at least the correct seqName if not the surrounding sequence.
 * If breakDelIns, then show deleted bases (eg show 'delAGinsTT' instead of 'delinsTT'). */

char *hgvsCFromVpTx(struct vpTx *vpTx, struct seqWindow *gSeqWin, struct psl *txAli,
                    struct genbankCds *cds,  struct dnaSeq *txSeq, boolean breakDelIns);
/* Return an HGVS c. (coding transcript) term for a variant projected onto a transcript w/cds.
 * gSeqWin must already have at least the correct seqName (chrom) if not the surrounding sequence.
 * If breakDelIns, then show deleted bases (eg show 'delAGinsTT' instead of 'delinsTT'). */

char *hgvsPFromVpPep(struct vpPep *vpPep, struct dnaSeq *protSeq, boolean addParens);
/* Return an HGVS p. (protein) term for a variant projected into protein space.
 * Strict HGVS compliance requires parentheses around predicted protein changes (addParens=TRUE),
 * but nobody seems to do that in practice.
 * Return NULL if an input is NULL. */

#endif /* HGHGVS_H */
