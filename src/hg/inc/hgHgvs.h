/* hgHgvs - support for a subset of the Human Genome Variation Society's (HGVS) nomenclature
 * for describing variants with respect to a specific gene sequence (or genome assembly).
 * See http://www.hgvs.org/mutnomen/ */

/* Copyright (C) 2016 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#ifndef HGHGVS_H
#define HGHGVS_H

#include "basicBed.h"

/* The full nomenclature is extremely complicated, able to encode things such as gene fusions and
 * advanced clinical info (e.g. "=/" for somatic mosaicism, "=//" for chimerism).  I am starting
 * simple with single-base substitutions, which should cover the majority of use cases, and will
 * work up from there.
 *
 * At the same time, since the spec has repeatedly changed, we will need to be flexible in our
 * parsing in order to support previously published HGVS (or HGVS-like) terms. */

enum hgvsSeqType
/* HGVS describes changes relative to several types of sequence: genomic, coding, protein, etc */
    {
    hgvstUndefined,  // the usual int default value means we haven't actually checked
    hgvstCoding,     // "c.": Coding DNA sequence only. Beware: complex coords for intron & UTR.
    hgvstGenomic,    // "g.": Genomic DNA
    hgvstMito,       // "m.": Mitochondrial DNA
    hgvstNoncoding,  // "n.": non-coding RNA
    hgvstRna,        // "r.": RNA (like DNA but lowercase and 'u' instead of 'T')
    hgvstProtein,    // "p.": Protein
    };

struct hgvsVariant
/* Components that we are able parse out of an HGVS term. (there's more to HGVS than just this) */
    {
    struct hgvsVariant *next;
    char *seqAcc;            // The reference sequence for the variant (optional -- may be NULL!)
    char *seqGeneSymbol;     // Usually NULL, but DNA/RNA can have HGNC gene symbol in ()'s.
    char *change;            // The sequence change (following the position in nucleotide terms,
                             // with embedded position in protein terms); currently not parsed.
    int start1;              // 1-based start of the variant in reference seq: can be negative !
                             // For coding terms with complex intron or UTR coords, this is the
                             // anchor coord (nearest CDS exon boundary)
    int end;                 // End of the variant in the reference seq; can be negative !
                             // For a range, this is the second anchor coord; else same as start
    // These two fields apply only to hgvstCoding ("c.") terms and Rna ("r.") for coding RNAs:
    int startOffset;         // offset into intron; 1-based, 0 means N/A; can be negative!
    int endOffset;           // offset into intron; 0 means N/A; can be negative!
                             // For a range, this is the second offset; else same as startOffset
    enum hgvsSeqType type;   // type of sequence: genomic, coding, protein, etc
    boolean startIsUtr3;     // TRUE if start is relative to *end* of coding sequence
    boolean endIsUtr3;       // TRUE if end is relative to *end* of coding sequence
    };

struct hgvsVariant *hgvsParseTerm(char *term);
/* If term is a parseable form of HGVS, return the parsed representation, otherwise NULL.
 * This does not check validity of accessions or alleles. */

struct hgvsVariant *hgvsParsePseudoHgvs(char *db, char *term);
/* Attempt to parse things that are not strict HGVS, but that people might intend as HGVS. */

boolean hgvsValidate(char *db, struct hgvsVariant *hgvs, char **retFoundAcc, int *retFoundVersion,
                     char **retDiffRefAllele);
/* Return TRUE if hgvs coords are within the bounds of the sequence for hgvs->seqAcc.
 * Note: Coding terms may contain coords outside the bounds (upstream, intron, downstream) so
 * those can't be checked without mapping the term to the genome; this returns TRUE if seq is found.
 * If retFoundAcc is not NULL, set it to our local accession (which may be missing the .version
 * of hgvs->seqAcc) or NULL if we can't find any match.
 * If retFoundVersion is not NULL and hgvs->seqAcc has a version number (e.g. NM_005957.4),
 * set retFoundVersion to our version from latest GenBank, otherwise 0 (no version for LRG).
 * If coords are OK and retDiffRefAllele is not NULL: if our sequence at the coords
 * matches hgvs->refAllele then set it to NULL; if mismatch then set it to our sequence. */

struct bed3 *hgvsMapToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable);
/* Return a psl with target coords from db, mapping the variant to the genome.
 * Return NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */

#endif /* HGHGVS_H */
