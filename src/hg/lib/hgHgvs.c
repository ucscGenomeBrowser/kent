/* hgHgvs.c - Parse the Human Genome Variation Society (HGVS) nomenclature for variants. */
/* See http://varnomen.hgvs.org/ and https://github.com/mutalyzer/mutalyzer/ */

/* Copyright (C) 2016 The Regents of the University of California
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hgHgvs.h"
#include "bPlusTree.h"
#include "chromInfo.h"
#include "genbank.h"
#include "hdb.h"
#include "indelShift.h"
#include "lrg.h"
#include "pslTransMap.h"
#include "regexHelper.h"
#include "trackHub.h"

void hgvsVariantFree(struct hgvsVariant **pHgvs)
// Free *pHgvs and its contents, and set *pHgvs to NULL.
{
if (pHgvs && *pHgvs)
    {
    struct hgvsVariant *hgvs = *pHgvs;
    freez(&hgvs->seqAcc);
    freez(&hgvs->seqGeneSymbol);
    freez(&hgvs->changes);
    freez(pHgvs);
    }
}

// Regular expressions for HGVS-recognized sequence accessions: LRG, ENS* or versioned RefSeq:
#define lrgTranscriptExp "(LRG_[0-9]+t[0-9]+)"
#define lrgProteinExp "(LRG_[0-9]+p[0-9]+)"
#define lrgRegionExp "(LRG_[0-9]+)"

#define ensTranscriptExp "(ENS([A-Z]{3})?T[0-9]+\\.[0-9]+)"
//      0.....................................  whole matching string
//      1.....................................  ENS transcript ID including optional lift suffix
//         2...                                 optional non-human species code e.g. MUS for mouse
#define ensProteinExp "(ENS([A-Z]{3})?P[0-9]+\\.[0-9]+)"
//      0.....................................  whole matching string
//      1.....................................  ENS protein ID
//         2...                                 optional non-human species code e.g. MUS for mouse

// NC = RefSeq reference assembly chromosome (NT = contig (e.g. alt), NW = patch)
// NG = RefSeq incomplete genomic region (e.g. gene locus)
// NM = RefSeq curated mRNA
// NP = RefSeq curated protein
// NR = RefSeq curated (non-coding) RNA
#define geneSymbolExp "([A-Za-z0-9./_-]+)"
#define optionalGeneSymbolExp "(\\(" geneSymbolExp "\\))?"
#define versionedAccPrefixExp(p) "(" p "_[0-9]+(\\.[0-9]+)?)" optionalGeneSymbolExp
//                                 ........................       accession and optional dot version
//                                              ...........       optional dot version
//                                                         ...... optional gene symbol in ()s
//                                                          ....  optional gene symbol
#define versionedRefSeqNCExp versionedAccPrefixExp("[NX][CTW]")
#define versionedRefSeqNGExp versionedAccPrefixExp("[NX]G")
#define versionedRefSeqNMExp versionedAccPrefixExp("[NX]M")
#define versionedRefSeqNPExp versionedAccPrefixExp("[NX]P")
#define versionedRefSeqNMRExp versionedAccPrefixExp("[NX][MR]")

// Nucleotide position regexes
// (c. = CDS, g. = genomic, m. = mitochondrial, n.= non-coding RNA, r. = RNA)
#define posIntExp "([0-9]+)"
#define hgvsGenoPosExp posIntExp "(_" posIntExp ")?"
//                   ......                     1-based start position
//                           .............      optional range separator and end position
//                               ......         1-based end position
// n. terms can have exonic anchor base and intron offset for both start and end:
#define offsetExp "([-+])"
// c. terms may also have a UTR indicator before the anchor base (- for UTR5, * for UTR3)
#define anchorExp "([-*])?"
#define complexNumExp anchorExp posIntExp "(" offsetExp posIntExp ")?"
#define hgvsCdsPosExp complexNumExp "(_" complexNumExp ")?"
//                    ...                               optional UTR anchor "-" or "*"
//                        ...                           mandatory 1-based start anchor base offset
//                            .......                   optional offset separator and offset
//                            ...                       intron offset separator
//                                ...                   intron offset number
//                                    ...............   optional range separator and complex end
//                                    ...               optional UTR anchor "-" or "*"
//                                        ...           1-based end anchor base offset
//                                            .......   optional offset separator and offset
//                                            ...       intron offset separator
//                                                ...   intron offset number

// It's pretty common for users to omit the '.' so if it's missing but the rest of the regex fits,
// roll with it.
#define hgvsCDotPosExp "c\\.?" hgvsCdsPosExp
#define hgvsGMDotPosExp "([gm])\\.?" hgvsGenoPosExp
#define hgvsNDotPosExp "n\\.?" hgvsCdsPosExp
// Not supporting RDot at this point because r. terms may use either n. or c. numbering!
// #define hgvsRDotPosExp "r\\.?" hgvsCdsPosExp

// Protein substitution regex
#define aa3Exp "Ala|Arg|Asn|Asp|Cys|Gln|Glu|Gly|His|Ile|Leu|Lys|Met|Phe|Pro|Ser|Thr|Trp|Tyr|Val|Ter"
#define hgvsAminoAcidExp "[ARNDCQEGHILKMFPSTWYVX*]|" aa3Exp
#define hgvsAminoAcidSubstExp "(" hgvsAminoAcidExp ")" posIntExp "(" hgvsAminoAcidExp "|=)"
#define hgvsPDotSubstExp "p\\.\\(?" hgvsAminoAcidSubstExp "\\)?"
//                                 ...                                  // original sequence
//                                              ......                  // 1-based position
//                                                           ...        // replacement sequence

// Protein range (or just single pos) regex
#define hgvsAaRangeExp "(" hgvsAminoAcidExp ")" posIntExp "(_(" hgvsAminoAcidExp ")" posIntExp ")?(.*)"
#define hgvsPDotRangeExp "p\\.\\(?" hgvsAaRangeExp "\\)?"
//  original start AA           ...
//  1-based start position                       ...
//  optional range sep and AA+pos                          .....................................
//  original end AA                                              ...
//  1-based end position                                                          ...
//  change description                                                                    ...

// Complete HGVS term regexes combining sequence identifiers and change operations
#define hgvsFullRegex(seq, op) "^" seq "[ :]+" op

#define hgvsLrgGDotPosExp hgvsFullRegex(lrgRegionExp, hgvsGMDotPosExp)
#define hgvsLrgGDotExp hgvsLrgGDotPosExp "(.*)"
// substring numbering:
//      0.....................................  whole matching string
//      1.......................                LRG region
//                              2.              g or m
//                                3..           1-based start position
//                                   4...       optional range separator and end position
//                                     5..      1-based end position
//                                        6...  change description


#define hgvsRefSeqNCGDotPosExp hgvsFullRegex(versionedRefSeqNCExp, hgvsGMDotPosExp)
#define hgvsRefSeqNCGDotExp hgvsRefSeqNCGDotPosExp "(.*)"
// substring numbering:
//      0.....................................  whole matching string
//      1.................                      accession and optional dot version
//               2........                      optional dot version
//                       3......                optional gene symbol in ()s
//                        4....                 optional gene symbol
//                              5.              g or m
//                                6..           1-based start position
//                                   7...       optional range separator and end position
//                                     8..      1-based end position
//                                        9...  change description

#define hgvsLrgCDotPosExp hgvsFullRegex(lrgTranscriptExp, hgvsCDotPosExp)
#define hgvsLrgCDotExp hgvsLrgCDotPosExp "(.*)"
// substring numbering:
//      0.....................................................  whole matching string
//      1.............                                          LRG transcript
//                   2...                                       optional UTR anchor "-" or "*"
//                       3...                                   mandatory first number
//                           4.......                           optional offset separator and offset
//                           5...                               offset separator
//                               6...                           offset number
//                                   7...............           optional range sep and complex num
//                                   8...                       optional UTR anchor "-" or "*"
//                                       9...                   first number
//                                          10.......           optional offset separator and offset
//                                          11...               offset separator
//                                              12...           offset number
//                                                  13........  sequence change

#define hgvsEnsCDotPosExp hgvsFullRegex(ensTranscriptExp, hgvsCDotPosExp)
#define hgvsEnsCDotExp hgvsEnsCDotPosExp "(.*)"
// substring numbering:
//      0.....................................................  whole matching string
//      1............                                           ENS transcript ID
//         2...                                                 optional non-human species code
//                   3...                                       optional UTR anchor "-" or "*"
//                       4...                                   mandatory first number
//                           5.......                           optional offset separator and offset
//                           6...                               offset separator
//                               7...                           offset number
//                                   8...............           optional range sep and complex num
//                                   9...                       optional UTR anchor "-" or "*"
//                                      10...                   first number
//                                          11.......           optional offset separator and offset
//                                          12...               offset separator
//                                              13...           offset number
//                                                  14........  sequence change

#define hgvsRefSeqNMCDotPosExp hgvsFullRegex(versionedRefSeqNMExp, hgvsCDotPosExp)
#define hgvsRefSeqNMCDotExp hgvsRefSeqNMCDotPosExp "(.*)"
// substring numbering:
//      0...............................................................  whole matching string
//      1...............                                                  acc & optional dot version
//             2........                                                  optional dot version
//                       3.....                                           optional gene sym in ()s
//                        4...                                            optional gene symbol
//                              5...                                      optional UTR anchor
//                                  6...                                  mandatory first number
//                                      7.......                          optional offset
//                                      8...                              offset separator
//                                          9...                          offset number
//                                             10...............          optional range
//                                             11...                      optional UTR anchor
//                                                12...                   first number
//                                                     13.......          optional offset
//                                                     14...              offset separator
//                                                         15...          offset number
//                                                             16........ sequence change

#define hgvsLrgNDotExp hgvsFullRegex(lrgTranscriptExp, hgvsNDotPosExp) "(.*)"
// substring numbering:
//      0.....................................................  whole matching string
//      1.............                                          LRG transcript
//                   2...                                       UTR anchor "-" or "*"
//                       3...                                   mandatory first number
//                           4.......                           optional offset separator and offset
//                           5...                               offset separator
//                               6...                           offset number
//                                   7...............           optional range sep and complex num
//                                   8...                       UTR anchor "-" or "*"
//                                       9...                   first number
//                                          10.......           optional offset separator and offset
//                                          11...               offset separator
//                                              12...           offset number
//                                                  13........  sequence change

#define hgvsEnsNDotExp hgvsFullRegex(ensTranscriptExp, hgvsNDotPosExp) "(.*)"
// substring numbering:
//      0.....................................................  whole matching string
//      1............                                           ENS transcript ID
//         2...                                                 optional non-human species code
//                   3...                                       UTR anchor "-" or "*"
//                       4...                                   mandatory first number
//                           5.......                           optional offset separator and offset
//                           6...                               offset separator
//                               7...                           offset number
//                                   8...............           optional range sep and complex num
//                                   9...                       UTR anchor "-" or "*"
//                                      10...                   first number
//                                          11.......           optional offset separator and offset
//                                          12...               offset separator
//                                              13...           offset number
//                                                  14........  sequence change

#define hgvsRefSeqNMRNDotExp hgvsFullRegex(versionedRefSeqNMRExp, hgvsNDotPosExp) "(.*)"
// substring numbering:
//      0...............................................................  whole matching string
//      1...............                                                  acc & optional dot version
//             2........                                                  optional dot version
//                       3.....                                           optional gene sym in ()s
//                        4...                                            optional gene symbol
//                              5...                                      UTR anchor
//                                  6...                                  mandatory first number
//                                      7.......                          optional offset
//                                      8...                              offset separator
//                                          9...                          offset number
//                                             10...............          optional range
//                                             11...                      UTR anchor
//                                                12...                   first number
//                                                     13.......          optional offset
//                                                     14...              offset separator
//                                                         15...          offset number
//                                                             16........ sequence change

#define hgvsLrgPDotSubstExp hgvsFullRegex(lrgProteinExp, hgvsPDotSubstExp)
// substring numbering:
//      0.....................................................  whole matching string
//      1............................                           LRG protein
//                                   2.....                     original sequence
//                                           3......            1-based position
//                                                     4......  replacement sequence

#define hgvsEnsPDotSubstExp hgvsFullRegex(ensProteinExp, hgvsPDotSubstExp)
// substring numbering:
//      0.....................................................  whole matching string
//      1...........................                            ENS protein ID
//         2...                                                 optional non-human species code
//                                   3.....                     original sequence
//                                           4......            1-based position
//                                                     5......  replacement sequence

#define hgvsRefSeqNPPDotSubstExp hgvsFullRegex(versionedRefSeqNPExp, hgvsPDotSubstExp)
// substring numbering:
//      0.....................................................  whole matching string
//      1................                                       accession and optional dot version
//                 2.....                                       optional dot version
//                         3.....                               optional gene symbol in ()s
//                          4...                                optional gene symbol
//                                   5.....                     original sequence
//                                           6......            1-based position
//                                                     7......  replacement sequence

#define hgvsLrgPDotRangeExp hgvsFullRegex(lrgProteinExp, hgvsPDotRangeExp)
// substring numbering:
//      0.....................................................  whole matching string
//      1..........................                             accession and optional dot version
//                                 2...                         original start AA
//                                       3...                   1-based start position
//                                          4.................  optional range sep and AA+pos
//                                            5...              original end AA
//                                                 6...         1-based end position
//                                                     7......  change description

#define hgvsEnsPDotRangeExp hgvsFullRegex(ensProteinExp, hgvsPDotRangeExp)
// substring numbering:
//      0.....................................................  whole matching string
//      1...........................                            ENS protein ID
//         2...                                                 optional non-human species code
//                                 3...                         original start AA
//                                       4...                   1-based start position
//                                          5.................  optional range sep and AA+pos
//                                            6...              original end AA
//                                                 7...         1-based end position
//                                                     8......  change description

#define hgvsRefSeqNPPDotRangeExp hgvsFullRegex(versionedRefSeqNPExp, hgvsPDotRangeExp)
// substring numbering:
//      0.....................................................  whole matching string
//      1................                                       accession and optional dot version
//                 2.....                                       optional dot version
//                         3.....                               optional gene symbol in ()s
//                          4...                                optional gene symbol
//                                 5...                         original start AA
//                                       6...                   1-based start position
//                                          7.................  optional range sep and AA+pos
//                                            8...              original end AA
//                                                 9...         1-based end position
//                                                     10.....  change description

// Pseudo-HGVS in common usage
// g. with "chr" ID:
#define pseudoHgvsChrGDotExp hgvsFullRegex("(chr[0-9A-Za-z_]+)", hgvsGMDotPosExp) "(.*)"
// substring numbering:
//      0.....................................  whole matching string
//      1...........                            chr...
//                   2.                         g or m
//                     3......                  1-based start position
//                            4.......          optional range separator and end position
//                              5.....          1-based end position
//                                    6....     change description


// Sometimes users give an NM_ accession, but a protein change.
#define maybePDot "[ :]+p?\\.?\\(?"
#define pseudoHgvsNMPDotSubstExp "^" versionedRefSeqNMExp maybePDot hgvsAminoAcidSubstExp "\\)?"
// substring numbering:
//      0.....................................................  whole matching string
//      1...............                                        acc & optional dot version
//             2........                                        optional dot version
//                       3.....                                 optional gene sym in ()s
//                        4...                                  optional gene symbol
//                                   5.....                     original sequence
//                                           6......            1-based position
//                                                     7......  replacement sequence

#define pseudoHgvsNMPDotRangeExp "^" versionedRefSeqNMExp maybePDot hgvsAaRangeExp "\\)?"

// substring numbering:
//      0.....................................................  whole matching string
//      1...............                                        acc & optional dot version
//             2........                                        optional dot version
//                       3.....                                 optional gene sym in ()s
//                        4...                                  optional gene symbol
//                                 5...                         original start AA
//                                       6...                   1-based start position
//                                           7..........        optional range sep and AA+pos
//                                             8...             original end AA
//                                                  9...        1-based end position
//                                                      10....  change description

// Common: gene symbol followed by space and/or punctuation followed by protein change
#define pseudoHgvsGeneSymbolProtSubstExp "^" geneSymbolExp maybePDot hgvsAminoAcidSubstExp "\\)?"
//      0.....................................................  whole matching string
//      1...................                                    gene symbol
//                                   2.....                     original sequence
//                                           3......            1-based position
//                                                     4......  replacement sequence

#define pseudoHgvsGeneSymbolProtRangeExp "^" geneSymbolExp maybePDot hgvsAaRangeExp "\\)?"
//      0.....................................................  whole matching string
//      1...................                                    gene symbol
//                                 2...                         original start AA
//                                       3...                   1-based start position
//                                           4................  optional range sep and AA+pos
//                                             5...             original end AA
//                                                  6...         1-based end position
//                                                       7.....  change description

// As above but omitting the protein change
#define pseudoHgvsGeneSymbolProtPosExp "^" geneSymbolExp maybePDot posIntExp "\\)?"
//      0..........................                             whole matching string
//      1...................                                    gene symbol
//                           2.....                             1-based position


// Gene symbol, maybe punctuation, and a clear "c." position (and possibly change)
#define pseudoHgvsGeneSympolCDotPosExp "^" geneSymbolExp "[: ]+" hgvsCDotPosExp
//      0.....................................................  whole matching string
//      1...................                                    gene symbol
//                           2.....                             optional beginning of position exp
//                                   3.....                     beginning of position exp

static struct hgvsVariant *hgvsParseGDotPos(char *term)
/* If term is parseable as an HGVS NC_...g. term, return the parsed representation, else NULL. */
{
struct hgvsVariant *hgvs = NULL;
int accIx = 1;
int startPosIx = 3;
int endPosIx = 5;
int changeIx = 6;
boolean matches = FALSE;
regmatch_t substrs[17];
if (regexMatchSubstr(term, hgvsLrgGDotExp, substrs, ArraySize(substrs)))
    matches = TRUE;
else if (regexMatchSubstr(term, hgvsRefSeqNCGDotExp, substrs, ArraySize(substrs)))
    {
    matches = TRUE;
    // The LRG accession regex has only one substring but the RefSeq acc regex has 4, so that
    // affects all substring offsets after the accession.
    int refSeqExtra = 3;
    startPosIx += refSeqExtra;
    endPosIx += refSeqExtra;
    changeIx += refSeqExtra;
    }
if (matches)
    {
    AllocVar(hgvs);
    // HGVS recommendation May 2017: replace m. with g. since mitochondrion is genomic too
    hgvs->type = hgvstGenomic;
    hgvs->seqAcc = regexSubstringClone(term, substrs[accIx]);
    hgvs->start1 = regexSubstringInt(term, substrs[startPosIx]);
    if (regexSubstrMatched(substrs[endPosIx]))
        hgvs->end = regexSubstringInt(term, substrs[endPosIx]);
    else
        hgvs->end = hgvs->start1;
    hgvs->changes = regexSubstringClone(term, substrs[changeIx]);
    }
return hgvs;
}

static void extractComplexNum(char *term, regmatch_t *substrs, int substrOffset,
                              boolean *retIsUtr3, int *retPos, int *retOffset)
/* Extract matches from substrs starting at substrOffset to parse a complex number
 * description into anchor type, 1-based position and optional offset. */
{
int anchorIx = substrOffset;
int posIx = substrOffset + 1;
int offsetOpIx = substrOffset + 3;
int offsetIx = substrOffset + 4;
// Determine what startPos is relative to, based on optional anchor and optional offset
char anchor[16]; // string length 0 or 1
regexSubstringCopy(term, substrs[anchorIx], anchor, sizeof(anchor));
char offsetOp[16]; // string length 0 or 1
regexSubstringCopy(term, substrs[offsetOpIx], offsetOp, sizeof(offsetOp));
*retIsUtr3 = (anchor[0] == '*');
*retPos = regexSubstringInt(term, substrs[posIx]);
// Is pos negative?
if (anchor[0] == '-')
    *retPos = -*retPos;
// Grab offset, if there is one.
if (isNotEmpty(offsetOp))
    {
    *retOffset = regexSubstringInt(term, substrs[offsetIx]);
    if (offsetOp[0] == '-')
        *retOffset = -*retOffset;
    }
}

static struct hgvsVariant *hgvsParseCNDotPos(char *term)
/* If term is parseable as an HGVS CDS term, return the parsed representation, otherwise NULL. */
{
struct hgvsVariant *hgvs = NULL;
boolean isNoncoding = FALSE;
boolean matches = FALSE;
int accIx = 1;
int startAnchorIx = 2;
int endAnchorIx = 8;
int endPosIx = 9;
int changeIx = 13;
// The LRG accession regex has only one substring but the RefSeq acc regex has 4 and ENS has 2,
// so that affects all substring offsets after the accession.
int ixExtra = 0;
int geneSymbolIx = -1;
regmatch_t substrs[17];
if (regexMatchSubstr(term, hgvsLrgCDotExp, substrs, ArraySize(substrs)) ||
    (isNoncoding = regexMatchSubstr(term, hgvsLrgNDotExp, substrs, ArraySize(substrs))))
    {
    matches = TRUE;
    }
else if (regexMatchSubstr(term, hgvsEnsCDotExp, substrs, ArraySize(substrs)) ||
    (isNoncoding = regexMatchSubstr(term, hgvsEnsNDotExp, substrs, ArraySize(substrs))))
    {
    matches = TRUE;
    ixExtra = 1;
    }
else if (regexMatchSubstr(term, hgvsRefSeqNMCDotExp, substrs, ArraySize(substrs)) ||
         (isNoncoding = regexMatchSubstr(term, hgvsRefSeqNMRNDotExp, substrs, ArraySize(substrs))))
    {
    matches = TRUE;
    ixExtra = 3;
    geneSymbolIx = 4;
    }
if (ixExtra > 0)
    {
    startAnchorIx += ixExtra;
    endAnchorIx += ixExtra;
    endPosIx += ixExtra;
    changeIx += ixExtra;
    }
if (matches)
    {
    AllocVar(hgvs);
    hgvs->type = isNoncoding ? hgvstNoncoding : hgvstCoding;
    hgvs->seqAcc = regexSubstringClone(term, substrs[accIx]);
    extractComplexNum(term, substrs, startAnchorIx,
                      &hgvs->startIsUtr3, &hgvs->start1, &hgvs->startOffset);
    if (isNoncoding && hgvs->startIsUtr3)
        {
        warn("hgvsParseCNDotPos: noncoding term '%s' appears to start in UTR3 (*), "
             "not applicable for noncoding", term);
        hgvs->startIsUtr3 = FALSE;
        }
    if (geneSymbolIx >= 0 && regexSubstrMatched(substrs[geneSymbolIx]))
        hgvs->seqGeneSymbol = regexSubstringClone(term, substrs[geneSymbolIx]);
    if (regexSubstrMatched(substrs[endPosIx]))
        {
        extractComplexNum(term, substrs, endAnchorIx,
                          &hgvs->endIsUtr3, &hgvs->end, &hgvs->endOffset);
        if (isNoncoding && hgvs->endIsUtr3)
            {
            warn("hgvsParseCNDotPos: noncoding term '%s' appears to end in UTR3 (*), "
                 "not applicable for noncoding", term);
            hgvs->endIsUtr3 = FALSE;
            }
        }
    else
        {
        hgvs->end = hgvs->start1;
        hgvs->endIsUtr3 = hgvs->startIsUtr3;
        hgvs->endOffset = hgvs->startOffset;
        }
    hgvs->changes = regexSubstringClone(term, substrs[changeIx]);
    }
return hgvs;
}

static struct hgvsVariant *hgvsParsePDotSubst(char *term)
/* If term is parseable as an HGVS protein substitution, return the parsed representation,
 * otherwise NULL. */
{
struct hgvsVariant *hgvs = NULL;
boolean matches = FALSE;
int accIx = 1;
int refIx = 2;
int posIx = 3;
int altIx = 4;
int geneSymbolIx = -1;
// The LRG accession regex has only one substring but the RefSeq acc regex has 4 and ENS has 2,
// so that affects all substring offsets after the accession.
int ixExtra = 0;
regmatch_t substrs[8];
if (regexMatchSubstr(term, hgvsLrgPDotSubstExp, substrs, ArraySize(substrs)))
    matches = TRUE;
else if (regexMatchSubstr(term, hgvsEnsPDotSubstExp, substrs, ArraySize(substrs)))
    {
    ixExtra = 1;
    matches = TRUE;
    }
else if (regexMatchSubstr(term, hgvsRefSeqNPPDotSubstExp, substrs, ArraySize(substrs)))
    {
    matches = TRUE;
    ixExtra = 3;
    geneSymbolIx = 4;
    }
if (ixExtra > 0)
    {
    refIx += ixExtra;
    posIx += ixExtra;
    altIx += ixExtra;
    }
if (matches)
    {
    AllocVar(hgvs);
    hgvs->type = hgvstProtein;
    hgvs->seqAcc = regexSubstringClone(term, substrs[accIx]);
    int changeStart = substrs[refIx].rm_so;
    int changeEnd = substrs[altIx].rm_eo;
    int len = changeEnd - changeStart;
    char change[len+1];
    safencpy(change, sizeof(change), term+changeStart, len);
    hgvs->changes = cloneString(change);
    if (geneSymbolIx >= 0 && regexSubstrMatched(substrs[geneSymbolIx]))
        hgvs->seqGeneSymbol = regexSubstringClone(term, substrs[geneSymbolIx]);
    hgvs->start1 = regexSubstringInt(term, substrs[posIx]);
    hgvs->end = hgvs->start1;
    }
return hgvs;
}

static struct hgvsVariant *hgvsParsePDotRange(char *term)
/* If term is parseable as an HGVS protein range change, return the parsed representation,
 * otherwise NULL. */
{
struct hgvsVariant *hgvs = NULL;
boolean matches = FALSE;
int accIx = 1;
int startRefIx = 2;
int startPosIx = 3;
int endPosIx = 6;
int changeIx = 7;
int geneSymbolIx = -1;
// The LRG accession regex has only one substring but the RefSeq acc regex has 4 and ENS has 2,
// so that affects all substring offsets after the accession.
int ixExtra = 0;
regmatch_t substrs[11];
if (regexMatchSubstr(term, hgvsLrgPDotRangeExp, substrs, ArraySize(substrs)))
    matches = TRUE;
else if (regexMatchSubstr(term, hgvsEnsPDotRangeExp, substrs, ArraySize(substrs)))
    {
    matches = TRUE;
    ixExtra = 1;
    }
else if (regexMatchSubstr(term, hgvsRefSeqNPPDotRangeExp, substrs, ArraySize(substrs)))
    {
    matches = TRUE;
    ixExtra = 3;
    geneSymbolIx = 4;
    }
if (ixExtra > 0)
    {
    startRefIx += ixExtra;
    startPosIx += ixExtra;
    endPosIx += ixExtra;
    changeIx += ixExtra;
    }
if (matches)
    {
    AllocVar(hgvs);
    hgvs->type = hgvstProtein;
    hgvs->seqAcc = regexSubstringClone(term, substrs[accIx]);
    int changeStart = substrs[startRefIx].rm_so;
    int changeEnd = substrs[changeIx].rm_eo;
    int len = changeEnd - changeStart;
    char change[len+1];
    safencpy(change, sizeof(change), term+changeStart, len);
    hgvs->changes = cloneString(change);
    if (geneSymbolIx >= 0 && regexSubstrMatched(substrs[geneSymbolIx]))
        hgvs->seqGeneSymbol = regexSubstringClone(term, substrs[geneSymbolIx]);
    hgvs->start1 = regexSubstringInt(term, substrs[startPosIx]);
    if (regexSubstrMatched(substrs[endPosIx]))
        hgvs->end = regexSubstringInt(term, substrs[endPosIx]);
    else
        hgvs->end = hgvs->start1;
    }
return hgvs;
}

struct hgvsVariant *hgvsParseTerm(char *term)
/* If term is a parseable form of HGVS, return the parsed representation, otherwise NULL.
 * This does not check validity of accessions, coordinates or alleles. */
{
struct hgvsVariant *hgvs = hgvsParseCNDotPos(term);
if (hgvs == NULL)
    hgvs = hgvsParsePDotSubst(term);
if (hgvs == NULL)
    hgvs = hgvsParsePDotRange(term);
if (hgvs == NULL)
    hgvs = hgvsParseGDotPos(term);
return hgvs;
}

static char *npForGeneSymbol(char *db, char *geneSymbol)
/* Given a gene symbol, look up and return its NP_ accession; if not found return NULL. */
{
char query[2048];
if (hDbHasNcbiRefSeq(db))
    {
    sqlSafef(query, sizeof(query), "select protAcc from ncbiRefSeqLink where name = '%s' "
             "and protAcc != 'n/a' and protAcc != '' "
             "order by length(protAcc), protAcc",
             geneSymbol);
    }
else if (hTableExists(db, "refGene"))
    {
    // Join with refGene to make sure it's an NP for this species.
    sqlSafef(query, sizeof(query), "select l.protAcc from %s l, refGene r "
             "where l.name = '%s' and l.mrnaAcc = r.name "
             "and l.protAcc != '' order by length(l.protAcc), l.protAcc"
             , refLinkTable, geneSymbol);
    }
else
    return NULL;
struct sqlConnection *conn = hAllocConn(db);
char *npAcc = sqlQuickString(conn, query);
hFreeConn(&conn);
return npAcc;
}

static char *nmForGeneSymbol(char *db, char *geneSymbol)
/* Given a gene symbol, look up and return its NM_ accession; if not found return NULL. */
{
if (trackHubDatabase(db))
    return NULL;
char query[2048];
char *geneTable = NULL;
if (hDbHasNcbiRefSeq(db))
    geneTable = "ncbiRefSeq";
else if (hTableExists(db, "refGene"))
    geneTable = "refGene";
if (geneTable == NULL)
    return NULL;
sqlSafef(query, sizeof(query), "select name from %s where name2 = '%s' "
         "order by length(name), name", geneTable, geneSymbol);
struct sqlConnection *conn = hAllocConn(db);
char *nmAcc = sqlQuickString(conn, query);
hFreeConn(&conn);
return nmAcc;
}

static char *npForNm(char *db, char *nmAcc)
/* Given an NM_ accession, look up and return its NP_ accession; if not found return NULL. */
{
if (trackHubDatabase(db))
    return NULL;
char query[2048];
if (hDbHasNcbiRefSeq(db))
    {
    // ncbiRefSeq tables use versioned NM_ accs, but the user might have passed in a
    // versionless NM_, so adjust query accordingly:
    if (strchr(nmAcc, '.'))
        sqlSafef(query, sizeof(query), "select protAcc from ncbiRefSeqLink where id = '%s'", nmAcc);
    else
        sqlSafef(query, sizeof(query), "select protAcc from ncbiRefSeqLink where id like '%s.%%'",
                 nmAcc);
    }
else if (hTableExists(db, "refGene"))
    {
    // Trim .version if present since our genbank tables don't use versioned names.
    char *trimmed = cloneFirstWordByDelimiter(nmAcc, '.');
    sqlSafef(query, sizeof(query), "select l.protAcc from %s l, refGene r "
             "where r.name = '%s' and l.mrnaAcc = r.name "
             "and l.protAcc != '' order by length(l.protAcc), l.protAcc",
             refLinkTable, trimmed);
    }
else return NULL;
struct sqlConnection *conn = hAllocConn(db);
char *npAcc = sqlQuickString(conn, query);
hFreeConn(&conn);
return npAcc;
}

static char *lrgProteinToTx(char *db, char *protAcc)
/* Return the LRG_ transcript accession for protAcc.  Each LRG_NpM has a corresponding LRG_NtM. */
{
int accLen = strlen(protAcc);
char txAcc[accLen+1];
safecpy(txAcc, sizeof(txAcc), protAcc);
char *p = strrchr(txAcc, 'p');
if (p)
    *p = 't';
return cloneString(txAcc);
}

static char *gencodeProteinToTx(char *db, char *protAcc)
/* Return the ENS*T transcript accession for protAcc, or NULL if not found. */
{
char *txAcc = NULL;
struct sqlConnection *conn = hAllocConn(db);
char *attrsTable = hFindLatestGencodeTableConn(conn, "Attrs");
if (attrsTable && hHasField(db, attrsTable, "proteinId"))
    {
    char query[2048];
    sqlSafef(query, sizeof(query), "select transcriptId from %s where proteinId = '%s'",
             attrsTable, protAcc);
    txAcc = sqlQuickString(conn, query);
    }
hFreeConn(&conn);
return txAcc;
}

static struct genePred *getGencodeGp(char *db, char *acc)
/* Return the genePred for acc in the latest wgEncodeGencodeCompV* table, or NULL if not found. */
{
struct genePred *gp = NULL;
struct sqlConnection *conn = hAllocConn(db);
char *gencodeTable = hFindLatestGencodeTableConn(conn, "Comp");
if (gencodeTable)
    {
    int hasBin = 1;
    char query[2048];
    sqlSafef(query, sizeof(query), "select * from %s where name = '%s'", gencodeTable, acc);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    if ((row = sqlNextRow(sr)) != NULL)
        gp = genePredExtLoad(row+hasBin, GENEPREDX_NUM_COLS);
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return gp;
}

static char *txProtFromGp(char *db, struct genePred *gp)
/* Return a string containing the translated CDS portion of gp using genomic sequence. */
{
int cdsLen = 0;
int i, eCdsStart, eCdsEnd;
for (i = 0;  i < gp->exonCount;  i++)
    {
    if (genePredCdsExon(gp, i, &eCdsStart, &eCdsEnd))
        cdsLen += (eCdsEnd - eCdsStart);
    }
char cdsSeq[cdsLen+1];
int offset = 0;
for (i = 0;  i < gp->exonCount;  i++)
    {
    if (genePredCdsExon(gp, i, &eCdsStart, &eCdsEnd))
        {
        struct dnaSeq *exonSeq = hChromSeq(db, gp->chrom, eCdsStart, eCdsEnd);
        safencpy(cdsSeq+offset, cdsLen+1-offset, exonSeq->dna, exonSeq->size);
        offset += exonSeq->size;
        dnaSeqFree(&exonSeq);
        }
    }
if (gp->strand[0] == '-')
    reverseComplement(cdsSeq, cdsLen);
char *seq = needMem(cdsLen+1);
dnaTranslateSome(cdsSeq, seq, cdsLen+1);
return seq;
}

static char *getProteinSeq(char *db, char *acc)
/* Return amino acid sequence for acc, or NULL if not found. */
{
if (trackHubDatabase(db))
    return NULL;
char *seq = NULL;
if (startsWith("ENS", acc))
    {
    char *txAcc = gencodeProteinToTx(db, acc);
    if (txAcc)
        {
        struct genePred *gp = getGencodeGp(db, txAcc);
        if (gp)
            seq = txProtFromGp(db, gp);
        genePredFree(&gp);
        freeMem(txAcc);
        }
    }
else if (startsWith("LRG_", acc))
    {
    if (hTableExists(db, "lrgPep"))
        {
        char query[2048];
        // lrgPep is indexed by transcript ID not protein ID
        char *txAcc = lrgProteinToTx(db, acc);
        sqlSafef(query, sizeof(query), "select seq from lrgPep where name = '%s'", txAcc);
        struct sqlConnection *conn = hAllocConn(db);
        seq = sqlQuickString(conn, query);
        hFreeConn(&conn);
        freeMem(txAcc);
        }
    }
else
    {
    if (hDbHasNcbiRefSeq(db))
        {
        char query[2048];
        sqlSafef(query, sizeof(query), "select seq from ncbiRefSeqPepTable "
                 "where name = '%s'", acc);
        struct sqlConnection *conn = hAllocConn(db);
        seq = sqlQuickString(conn, query);
        hFreeConn(&conn);
        }
    else
        {
        aaSeq *aaSeq = hGenBankGetPep(db, acc, NULL);
        if (aaSeq)
            seq = aaSeq->dna;
        }
    }
return seq;
}

static char refBaseForNp(char *db, char *npAcc, int pos)
// Get the amino acid base in NP_'s sequence at 1-based offset pos.
{
char *seq = getProteinSeq(db, npAcc);
char base = seq ? seq[pos-1] : '\0';
freeMem(seq);
return base;
}

struct hgvsVariant *hgvsParsePseudoHgvs(char *db, char *term)
/* Attempt to parse things that are not strict HGVS, but that people might intend as HGVS: */
// Note: this doesn't support non-coding gene symbol terms (which should have nt alleles)
{
struct hgvsVariant *hgvs = NULL;
regmatch_t substrs[11];
int geneSymbolIx = 1;
boolean isSubst;
if ((isSubst = regexMatchSubstr(term, pseudoHgvsNMPDotSubstExp,
                                     substrs, ArraySize(substrs))) ||
         regexMatchSubstr(term, pseudoHgvsNMPDotRangeExp, substrs, ArraySize(substrs)))
    {
    // User gave an NM_ accession but a protein change -- swap in the right NP_.
    int nmAccIx = 1;
    int geneSymbolIx = 4;
    int len = substrs[nmAccIx].rm_eo - substrs[nmAccIx].rm_so;
    char nmAcc[len+1];
    safencpy(nmAcc, sizeof(nmAcc), term, len);
    char *npAcc = npForNm(db, nmAcc);
    if (isNotEmpty(npAcc))
        {
        // Make it a real HGVS term with the NP and pass that on to the usual parser.
        int descStartIx = 5;
        char *description = term + substrs[descStartIx].rm_so;
        struct dyString *npTerm;
        if (regexSubstrMatched(substrs[geneSymbolIx]))
            {
            len = substrs[geneSymbolIx].rm_eo - substrs[geneSymbolIx].rm_so;
            char geneSymbol[len+1];
            safencpy(geneSymbol, sizeof(geneSymbol), term, len);
            npTerm = dyStringCreate("%s(%s):p.%s", npAcc, geneSymbol, description);
            }
        else
            npTerm = dyStringCreate("%s:p.%s", npAcc, description);
        hgvs = hgvsParseTerm(npTerm->string);
        dyStringFree(&npTerm);
        freeMem(npAcc);
        }
    }
else if ((isSubst = regexMatchSubstr(term, pseudoHgvsGeneSymbolProtSubstExp,
                                     substrs, ArraySize(substrs))) ||
         regexMatchSubstr(term, pseudoHgvsGeneSymbolProtRangeExp, substrs, ArraySize(substrs)))
    {
    int len = substrs[geneSymbolIx].rm_eo - substrs[geneSymbolIx].rm_so;
    char geneSymbol[len+1];
    safencpy(geneSymbol, sizeof(geneSymbol), term, len);
    char *npAcc = npForGeneSymbol(db, geneSymbol);
    if (isNotEmpty(npAcc))
        {
        // Make it a real HGVS term with the NP and pass that on to the usual parser.
        int descStartIx = 2;
        char *description = term + substrs[descStartIx].rm_so;
        struct dyString *npTerm = dyStringCreate("%s(%s):p.%s",
                                                 npAcc, geneSymbol, description);
        hgvs = hgvsParseTerm(npTerm->string);
        dyStringFree(&npTerm);
        freeMem(npAcc);
        }
    }
else if (regexMatchSubstr(term, pseudoHgvsGeneSymbolProtPosExp, substrs, ArraySize(substrs)))
    {
    int len = substrs[geneSymbolIx].rm_eo - substrs[geneSymbolIx].rm_so;
    char geneSymbol[len+1];
    safencpy(geneSymbol, sizeof(geneSymbol), term, len);
    char *npAcc = npForGeneSymbol(db, geneSymbol);
    if (isNotEmpty(npAcc))
        {
        // Only position was provided, no change.  Look up ref base and make a synonymous subst
        // so it's parseable HGVS.
        int posIx = 2;
        int pos = regexSubstringInt(term, substrs[posIx]);
        char refBase = refBaseForNp(db, npAcc, pos);
        struct dyString *npTerm = dyStringCreate("%s(%s):p.%c%d=",
                                                 npAcc, geneSymbol, refBase, pos);
        hgvs = hgvsParseTerm(npTerm->string);
        dyStringFree(&npTerm);
        freeMem(npAcc);
        }
    }
else if (regexMatchSubstr(term, pseudoHgvsGeneSympolCDotPosExp, substrs, ArraySize(substrs)))
    {
    int len = substrs[geneSymbolIx].rm_eo - substrs[geneSymbolIx].rm_so;
    char geneSymbol[len+1];
    safencpy(geneSymbol, sizeof(geneSymbol), term, len);
    char *nmAcc = nmForGeneSymbol(db, geneSymbol);
    if (isNotEmpty(nmAcc))
        {
        // Make it a real HGVS term with the NM and pass that on to the usual parser.
        int descStartIx = regexSubstrMatched(substrs[2]) ? 2 : 3;
        char *description = term + substrs[descStartIx].rm_so;
        struct dyString *nmTerm = dyStringCreate("%s(%s):c.%s",
                                                 nmAcc, geneSymbol, description);
        hgvs = hgvsParseTerm(nmTerm->string);
        dyStringFree(&nmTerm);
        freeMem(nmAcc);
        }
    }
else if (regexMatchSubstr(term, pseudoHgvsChrGDotExp, substrs, ArraySize(substrs)))
    {
    int chrIx = 1;
    int startPosIx = 3;
    int endPosIx = 5;
    int changeIx = 6;
    AllocVar(hgvs);
    hgvs->type = hgvstGenomic;
    hgvs->seqAcc = regexSubstringClone(term, substrs[chrIx]);
    hgvs->start1 = regexSubstringInt(term, substrs[startPosIx]);
    if (regexSubstrMatched(substrs[endPosIx]))
        hgvs->end = regexSubstringInt(term, substrs[endPosIx]);
    else
        hgvs->end = hgvs->start1;
    hgvs->changes = regexSubstringClone(term, substrs[changeIx]);
    }
return hgvs;
}

static struct bbiFile *getLrgBbi(char *db)
/* Return bbiFile for LRG regions or NULL if not found. */
{
struct bbiFile *bbi = NULL;
// I don't think this will be called often enough to warrant caching open bbi file (and index?).
// I expect it to be called a couple times when the user enters a LRG genomic HGVS pos/search term.
// It would be cleaner to get fileName from tdb or db -- but this is much quicker & easier:
char fileName[1024];
safef(fileName, sizeof(fileName), "/gbdb/%s/bbi/lrg.bb", db);
char *fileNameRep = hReplaceGbdb(fileName);
if (fileExists(fileNameRep))
    bbi = bigBedFileOpen(fileNameRep);
freeMem(fileNameRep);
return bbi;
}

static struct lrg *loadLrgByName(char *db, char *lrgId)
/* Retrieve lrg data from bigBed. */
{
struct lrg *lrg = NULL;
struct bbiFile *bbi = getLrgBbi(db);
if (bbi)
    {
    int fieldIx = 0;
    struct bptFile *index = bigBedOpenExtraIndex(bbi, "name", &fieldIx);
    if (index)
        {
        struct lm *lm = lmInit(0);
        struct bigBedInterval *bb = bigBedNameQuery(bbi, index, fieldIx, lrgId, lm);
        if (bb)
            {
            char *fields[bbi->fieldCount];
            char chrom[512], startBuf[16], endBuf[16];
            bigBedIntervalToRowLookupChrom(bb, NULL, bbi, chrom, sizeof(chrom),
                                           startBuf, endBuf, fields, bbi->fieldCount);
            lrg = lrgLoad(fields);
            }
        lmCleanup(&lm);
        }
    // Don't bptFileClose(&index) -- index shares pointers with bbi!
    }
bigBedFileClose(&bbi);
return lrg;
}

static char refBaseFromNucSubst(char *change)
/* If sequence change is a nucleotide substitution, return the reference base, else null char. */
{
if (regexMatchNoCase(change, "^([ACGTU])>"))
    return toupper(change[0]);
return '\0';
}

static void hgvsStartEndToZeroBasedHalfOpen(struct hgvsVariant *hgvs, int *retStart, int *retEnd)
/* Convert HGVS's fully closed, 1-based-unless-negative start and end to UCSC start and end. */
{
// If hgvs->start1 is negative, it is effectively 0-based, so subtract 1 only if positive.
int start = hgvs->start1;
if (start > 0)
    start--;
// If hgvs->end is negative, it is effectively 0-based, so add 1 to get half-open end.
int end = hgvs->end;
if (end < 0)
    end++;
if (retStart)
    *retStart = start;
if (retEnd)
    *retEnd = end;
}

static boolean hgvsValidateGenomic(char *db, struct hgvsVariant *hgvs,
                                   char **retFoundAcc, int *retFoundVersion,
                                   char **retDiffRefAllele)
/* Return TRUE if hgvs->seqAcc can be mapped to a chrom/LRG in db and coords are in range.
 * If retFoundAcc is non-NULL, set it to the versionless seqAcc if chrom is found, else NULL.
 * If retFoundVersion is non-NULL, set to seqAcc's version if chrom is found, -1 for LRG, else NULL.
 * If retDiffRefAllele is non-NULL and hgvs specifies a reference allele then compare it to
 * the genomic sequence at that location; set *retDiffRefAllele to NULL if they match, or to
 * genomic sequence if they differ. */
{
boolean coordsOK = FALSE;
char hgvsBase = '\0';
if (retDiffRefAllele)
    {
    hgvsBase = refBaseFromNucSubst(hgvs->changes);
    *retDiffRefAllele = NULL;
    }
if (retFoundAcc)
    *retFoundAcc = NULL;
if (retFoundVersion)
    *retFoundVersion = 0;
int start, end;
hgvsStartEndToZeroBasedHalfOpen(hgvs, &start, &end);
if (startsWith("LRG_", hgvs->seqAcc))
    {
    struct lrg *lrg = loadLrgByName(db, hgvs->seqAcc);
    if (lrg && start >= 0 && start < lrg->lrgSize && end > 0 && end <= lrg->lrgSize)
        {
        coordsOK = TRUE;
        if (hgvsBase != '\0')
            {
            struct dnaSeq *lrgSeq = lrgReconstructSequence(lrg, db);
            lrgSeq->dna[start] = toupper(lrgSeq->dna[start]);
            if (lrgSeq->dna[start] != hgvsBase)
                *retDiffRefAllele = cloneStringZ(lrgSeq->dna+start, 1);
            dnaSeqFree(&lrgSeq);
            }
        if (retFoundAcc)
            *retFoundAcc = cloneString(hgvs->seqAcc);
        }
    lrgFree(&lrg);
    }
else
    {
    char *chrom = hgOfficialChromName(db, hgvs->seqAcc);
    if (isNotEmpty(chrom))
        {
        struct chromInfo *ci = hGetChromInfo(db, chrom);
        if (start >= 0 && start < ci->size && end > 0 && end <= ci->size)
            {
            coordsOK = TRUE;
            if (hgvsBase != '\0')
                {
                struct dnaSeq *refBase = hFetchSeq(ci->fileName, chrom, start, end);
                touppers(refBase->dna);
                if (refBase->dna[0] != hgvsBase)
                    *retDiffRefAllele = cloneString(refBase->dna);
                dnaSeqFree(&refBase);
                }
            }
        // Since we found hgvs->seqAcc, split it into versionless and version parts.
        if (retFoundAcc)
            *retFoundAcc = cloneFirstWordByDelimiter(hgvs->seqAcc, '.');
        if (retFoundVersion)
            {
            char *p = strchr(hgvs->seqAcc, '.');
            if (p)
                *retFoundVersion = atoi(p+1);
            }
        // Don't free chromInfo, it's cached!
        }
    }
return coordsOK;
}

static char *txSeqFromGp(char *db, struct genePred *gp)
/* Return transcribed-from-genome sequence for gp */
{
int seqLen = 0;
int i;
for (i = 0;  i < gp->exonCount;  i++)
    seqLen += (gp->exonEnds[i] - gp->exonStarts[i]);
char *seq = needMem(seqLen+1);
int offset = 0;
for (i = 0;  i < gp->exonCount;  i++)
    {
    int exonStart = gp->exonStarts[i];
    int exonEnd = gp->exonEnds[i];
    struct dnaSeq *exonSeq = hChromSeq(db, gp->chrom, exonStart, exonEnd);
    safencpy(seq+offset, seqLen+1-offset, exonSeq->dna, exonSeq->size);
    offset += exonSeq->size;
    dnaSeqFree(&exonSeq);
    }
if (gp->strand[0] == '-')
    reverseComplement(seq, seqLen);
return seq;
}

static char *getGencodeSeq(char *db, char *acc)
/* Return transcribed-from-genome sequence for acc, or NULL if not found. */
{
char *seq = NULL;
struct genePred *gp = getGencodeGp(db, acc);
if (gp)
    seq = txSeqFromGp(db, gp);
return seq;
}

static char *getCdnaSeq(char *db, char *acc)
/* Return cdna sequence for acc, or NULL if not found. */
{
if (trackHubDatabase(db))
    return NULL;
char *seq = NULL;
if (startsWith("LRG_", acc))
    {
    if (hTableExists(db, "lrgCdna"))
        {
        char query[2048];
        sqlSafef(query, sizeof(query), "select seq from lrgCdna where name = '%s'", acc);
        struct sqlConnection *conn = hAllocConn(db);
        seq = sqlQuickString(conn, query);
        hFreeConn(&conn);
        }
    }
else if (startsWith("ENS", acc ))
    {
    // Construct it from the genome I guess?
    seq = getGencodeSeq(db, acc);
    }
else
    {
    struct dnaSeq *cdnaSeq = NULL;
    if (hDbHasNcbiRefSeq(db))
        cdnaSeq = hDnaSeqGet(db, acc, "seqNcbiRefSeq", "extNcbiRefSeq");
    else
        cdnaSeq = hGenBankGetMrna(db, acc, NULL);
    if (cdnaSeq)
        seq = dnaSeqCannibalize(&cdnaSeq);
    }
return seq;
}

static boolean getCds(char *db, char *acc, struct genbankCds *retCds)
/* Get the CDS info for genbank/LRG/ENS acc; return FALSE if not found or not applicable. */
{
if (trackHubDatabase(db))
    return FALSE;
boolean gotCds = FALSE;
char *cdsStr = NULL;
char query[1024];
if (startsWith("ENS", acc))
    {
    // Infer CDS from genePred coords
    struct genePred *gp = getGencodeGp(db, acc);
    if (gp)
        {
        genePredToCds(gp, retCds);
        gotCds = (retCds->end > retCds->start);
        }
    }
else
    {
    if (startsWith("LRG_", acc))
        sqlSafef(query, sizeof(query),
                 "select cds from lrgCds where id = '%s'", acc);
    else if (hDbHasNcbiRefSeq(db) &&
             // This is a hack to allow us to fall back on refSeqAli if ncbiRefSeqPsl is incomplete:
             strchr(acc, '.'))
        sqlSafef(query, sizeof(query),
                 "select cds from ncbiRefSeqCds where id = '%s'", acc);
    else
        sqlSafef(query, sizeof(query),
                 "SELECT c.name FROM %s as c, %s as g WHERE (g.acc = '%s') AND "
                 "(g.cds != 0) AND (g.cds = c.id)", cdsTable, gbCdnaInfoTable, acc);
    struct sqlConnection *conn = hAllocConn(db);
    char cdsBuf[2048];
    cdsStr = sqlQuickQuery(conn, query, cdsBuf, sizeof(cdsBuf));
    hFreeConn(&conn);
    }
if (isNotEmpty(cdsStr))
    gotCds = (genbankCdsParse(cdsStr, retCds) &&
              retCds->startComplete && retCds->start != retCds->end);
return gotCds;
}

static char refBaseFromProt(char *change)
/* If change starts with an amino acid 3-letter or 1-letter code then return the 1-letter code,
 * else null char. */
{
regmatch_t substrs[1];
if (regexMatchSubstr(change, "^" hgvsAminoAcidExp, substrs, ArraySize(substrs)))
    {
    char match[256];  // 1- or 3-letter code
    regexSubstringCopy(change, substrs[0], match, sizeof(match));
    if (strlen(match) == 1)
        return toupper(match[0]);
    else
        return aaAbbrToLetter(match);
    }
return '\0';
}

static char *cloneStringFromChar(char c)
/* Return a cloned string from a single character. */
{
char str[2];
str[0] = c;
str[1] = '\0';
return cloneString(str);
}

static void checkRefAllele(struct hgvsVariant *hgvs, int start, char *accSeq,
                           char **retDiffRefAllele)
/* If hgvs change includes a reference allele, and if accSeq at start does not match,
 *  then set retDiffRefAllele to the accSeq version.  retDiffRefAllele must be non-NULL. */
{
char hgvsRefBase = (hgvs->type == hgvstProtein) ? refBaseFromProt(hgvs->changes) :
                                                  refBaseFromNucSubst(hgvs->changes);
if (hgvsRefBase != '\0')
    {
    char seqRefBase = toupper(accSeq[start]);
    if (seqRefBase != hgvsRefBase)
        *retDiffRefAllele = cloneStringFromChar(seqRefBase);
    }
if (hgvs->type == hgvstProtein && *retDiffRefAllele == NULL)
    {
    // Protein ranges have a second ref allele base to check
    char *p = strchr(hgvs->changes, '_');
    if (p != NULL)
        {
        hgvsRefBase = refBaseFromProt(p+1);
        if (hgvsRefBase != '\0')
            {
            int end1 = atoi(skipToNumeric(p+1));
            char seqRefBase = toupper(accSeq[end1-1]);
            if (seqRefBase != hgvsRefBase)
                *retDiffRefAllele = cloneStringFromChar(seqRefBase);
            }
        }
    }
}

static int getGbVersion(char *db, char *acc)
/* Return the local version that we have for acc. */
{
if (trackHubDatabase(db))
    return 0;
char query[2048];
sqlSafef(query, sizeof(query), "select version from %s where acc = '%s'", gbSeqTable, acc);
struct sqlConnection *conn = hAllocConn(db);
int version = sqlQuickNum(conn, query);
hFreeConn(&conn);
return version;
}

static char *normalizeVersion(char *db, char *acc, int *retFoundVersion)
/* LRG accessions don't have versions, so just return the same acc and set *retFoundVersion to 0.
 * ENS accessions must have versions.
 * The user may give us a RefSeq accession with or without a .version.
 * If ncbiRefSeq tables are present, return acc with version, looking up version if necessary.
 * If we look it up and can't find it, this returns NULL.
 * If instead we're using genbank tables, return acc with no version.
 * That ensures that acc will be found in our local tables. */
{
if (trackHubDatabase(db))
    return NULL;
char *normalizedAcc = NULL;
int foundVersion = 0;
if (startsWith("LRG_", acc))
    {
    normalizedAcc = cloneString(acc);
    }
else if (startsWith("ENS", acc))
    {
    normalizedAcc = cloneString(acc);
    char *p = strchr(acc, '.');
    if (p)
        foundVersion = atoi(p+1);
    }
else if (hDbHasNcbiRefSeq(db))
    {
    // ncbiRefSeq tables need versioned accessions.
    if (strchr(acc, '.'))
        normalizedAcc = cloneString(acc);
    else
        {
        char query[2048];
        sqlSafef(query, sizeof(query), "select name from ncbiRefSeq where name like '%s.%%'", acc);
        struct sqlConnection *conn = hAllocConn(db);
        normalizedAcc = sqlQuickString(conn, query);
        hFreeConn(&conn);
        }
    if (isNotEmpty(normalizedAcc))
        {
        char *p = strchr(normalizedAcc, '.');
        assert(p);
        foundVersion = atoi(p+1);
        }
    }
else
    {
    // genbank tables -- no version
    normalizedAcc = cloneFirstWordByDelimiter(acc, '.');
    foundVersion = getGbVersion(db, normalizedAcc);
    }
if (retFoundVersion)
    *retFoundVersion = foundVersion;
return normalizedAcc;
}

static boolean hgvsValidateGene(char *db, struct hgvsVariant *hgvs,
                                char **retFoundAcc, int *retFoundVersion, char **retDiffRefAllele)
/* Return TRUE if hgvs coords are within the bounds of the sequence for hgvs->seqAcc.
 * Note: Transcript terms may contain coords outside the bounds (upstream, intron, downstream) so
 * those can't be checked without mapping the term to the genome.
 * If retFoundAcc is not NULL, set it to our local accession (which may be missing the .version
 * of hgvs->seqAcc) or NULL if we can't find any match.
 * If retFoundVersion is not NULL and hgvs->seqAcc has a version number (e.g. NM_005957.4),
 * set retFoundVersion to our version from latest GenBank, otherwise 0 (no version for LRG).
 * If coords are OK and retDiffRefAllele is not NULL: if our sequence at the coords
 * matches hgvs->refAllele then set it to NULL; if mismatch then set it to our sequence. */
{
char *acc = normalizeVersion(db, hgvs->seqAcc, retFoundVersion);
if (isEmpty(acc))
    return FALSE;
boolean coordsOK = FALSE;
char *accSeq = (hgvs->type == hgvstProtein ? getProteinSeq(db, acc) : getCdnaSeq(db, acc));
if (accSeq)
    {
    // By convention, foundAcc is always versionless because it's accompanied by foundVersion.
    if (retFoundAcc)
        *retFoundAcc = cloneFirstWordByDelimiter(acc, '.');
    int seqLen = strlen(accSeq);
    int start, end;
    hgvsStartEndToZeroBasedHalfOpen(hgvs, &start, &end);
    if (hgvs->type == hgvstCoding)
        {
        struct genbankCds cds;
        coordsOK = getCds(db, acc, &cds);
        if (coordsOK)
            {
            start += (hgvs->startIsUtr3 ? cds.end : cds.start);
            end += (hgvs->endIsUtr3 ? cds.end : cds.start);
            if (start > end)
                coordsOK = FALSE;
            else if (retDiffRefAllele && hgvs->startOffset == 0 && start >= 0 && start <= seqLen)
                checkRefAllele(hgvs, start, accSeq, retDiffRefAllele);
            }
        }
    else
        {
        if (start <= end)
            {
            coordsOK = TRUE;
            if (retDiffRefAllele && hgvs->startOffset == 0 && start >= 0 && start < seqLen)
                checkRefAllele(hgvs, start, accSeq, retDiffRefAllele);
            }
        }
    }
freeMem(accSeq);
freeMem(acc);
return coordsOK;
}

boolean hgvsValidate(char *db, struct hgvsVariant *hgvs,
                     char **retFoundAcc, int *retFoundVersion, char **retDiffRefAllele)
/* Return TRUE if hgvs coords are within the bounds of the sequence for hgvs->seqAcc.
 * Note: Transcript terms may contain coords outside the bounds (upstream, intron, downstream) so
 * those can't be checked without mapping the term to the genome; this returns TRUE if seq is found.
 * If retFoundAcc is not NULL, set it to our local accession (which may be missing the .version
 * of hgvs->seqAcc) or NULL if we can't find any match.
 * If retFoundVersion is not NULL and hgvs->seqAcc has a version number (e.g. NM_005957.4),
 * set retFoundVersion to our version from latest GenBank, otherwise 0 (no version for LRG).
 * If coords are OK and retDiffRefAllele is not NULL: if our sequence at the coords
 * matches hgvs->refAllele then set it to NULL; if mismatch then set it to our sequence. */
{
if (hgvs->type == hgvstGenomic || hgvs->type == hgvstMito)
    return hgvsValidateGenomic(db, hgvs, retFoundAcc, retFoundVersion, retDiffRefAllele);
else
    return hgvsValidateGene(db, hgvs, retFoundAcc, retFoundVersion, retDiffRefAllele);
}

static struct bed *bed6New(char *chrom, unsigned chromStart, unsigned chromEnd, char *name,
                           int score, char strand)
/* Alloc & return a new bed with only the first 6 fields populated. */
{
struct bed *bed6;
AllocVar(bed6);
bed6->chrom = cloneString(chrom);
bed6->chromStart = chromStart;
bed6->chromEnd = chromEnd;
bed6->name = cloneString(name);
bed6->score = score;
bed6->strand[0] = strand;
return bed6;
}

static int pslMapQStartToT(struct psl *psl, int qStartIn)
/* Map a query start coord to a target start coord; return -1 if qStart is not in the alignment. */
{
int t = -1, qStart = qStartIn, ix;
boolean isRc = pslQStrand(psl) == '-';
if (isRc)
    {
    int qEnd = qStart + 1;
    qStart = psl->qSize - qEnd;
    }
for (ix = 0;  ix < psl->blockCount;  ix++)
    {
    int qOffset = qStart - pslQStart(psl, ix);
    if (qOffset >= 0 && qStart < pslQEnd(psl, ix))
        {
        t = pslTStart(psl, ix) + qOffset;
        break;
        }
    }
return t;
}

static int pslMapQEndToT(struct psl *psl, int qEnd)
/* Map a query end coord to a target end coord; return -1 if qEnd is not in the alignment. */
{
int tEnd = -1;
int qStart = qEnd - 1;
int tStart = pslMapQStartToT(psl, qStart);
if (tStart >= 0)
    tEnd = tStart + 1;
return tEnd;
}

boolean hgvsIsInsertion(struct hgvsVariant *hgvs)
/* Return TRUE if hgvs is an insertion (end == start1+1, change starts with "ins"). */
{
return (hgvs->end == hgvs->start1+1 && startsWith("ins", hgvs->changes));
}

static void adjustInsStartEnd(struct hgvsVariant *hgvs, uint *retStart0, uint *retEnd)
/* HGVS insertion coordinates include the base before and the base after the insertion point,
 * while we treat it as a zero-length point.  So if this is insertion, adjust the start and end. */
{
if (hgvsIsInsertion(hgvs))
    {
    *retStart0 = *retStart0 + 1;
    *retEnd = *retEnd - 1;
    }
}

static struct bed *hgvsMapGDotToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Given an NC_*g. or LRG_*g. term, if we can map to chrom then return the region, else NULL. */
{
struct bed *region = NULL;
if (startsWith("LRG_", hgvs->seqAcc))
    {
    struct lrg *lrg = loadLrgByName(db, hgvs->seqAcc);
    if (lrg)
        {
        uint chromSize = hChromSize(db, lrg->chrom);
        struct psl *psl = lrgToPsl(lrg, chromSize);
        int start = pslMapQStartToT(psl, hgvs->start1 - 1);
        int end = pslMapQEndToT(psl, hgvs->end);
        if (start >= 0 && end >= 0)
            region = bed6New(lrg->chrom, start, end, "", 0, '+');
        pslFree(&psl);
        }
    }
else
    {
    char *chrom = hgOfficialChromName(db, hgvs->seqAcc);
    if (isNotEmpty(chrom) && hgvs->start1 > 0)
        region = bed6New(chrom, hgvs->start1 - 1, hgvs->end, "", 0, '+');
    }
if (region)
    adjustInsStartEnd(hgvs, &region->chromStart, &region->chromEnd);
if (retPslTable)
    *retPslTable = NULL;
return region;
}

static void hgvsTranscriptToZeroBasedHalfOpen(struct hgvsVariant *hgvs,
                                              int maxCoord, struct genbankCds *cds,
                                              int *retStart, int *retEnd,
                                              int *retUpstreamBases, int *retDownstreamBases)
/* Convert a transcript HGVS's start1 and end into UCSC coords plus upstream and downstream lengths
 * for when the transcript HGVS has coordinates that extend beyond its sequence boundaries.
 * ret* args must be non-NULL. */
{
hgvsStartEndToZeroBasedHalfOpen(hgvs, retStart, retEnd);
if (hgvs->type == hgvstCoding)
    {
    // If the position follows '*' that means it's relative to cdsEnd; otherwise rel to cdsStart
    *retStart += (hgvs->startIsUtr3 ? cds->end : cds->start);
    *retEnd += (hgvs->endIsUtr3 ? cds->end : cds->start);
    }
// Now check for coords that extend beyond the transcript('s alignment to the genome)
if (*retStart < 0)
    {
    // hgvs->start1 is upstream of transcript.
    *retUpstreamBases = -*retStart;
    *retStart = 0;
    }
else if (*retStart >= maxCoord)
    {
    // Even the start coord is downstream of transcript -- make a negative "upstream"
    // for adjusting start.
    *retUpstreamBases = -(*retStart - maxCoord + 1);
    *retStart = maxCoord - 1;
    }
else
    *retUpstreamBases = 0;
if (*retEnd > maxCoord)
    {
    // hgvs->end is downstream of transcript.
    *retDownstreamBases = *retEnd - maxCoord;
    *retEnd = maxCoord;
    }
else if (*retEnd <= 0)
    {
    // Even the end coord is upstream of transcript -- make a negative "downstream"
    // for adjusting end.
    *retEnd += *retUpstreamBases;
    *retDownstreamBases = -*retUpstreamBases;
    }
else
    *retDownstreamBases = 0;
}

static struct psl *pslFromHgvsNuc(struct hgvsVariant *hgvs, char *acc, int accSize, int accEnd,
                                  struct genbankCds *cds,
                                  int *retUpstreamBases, int *retDownstreamBases)
/* Allocate and return a PSL modeling the variant in nucleotide sequence acc.
 * The PSL target is the sequence and the query is the changed part of the sequence.
 * accEnd is given in case accSize includes an unaligned poly-A tail.
 * If hgvs is coding ("c.") then the caller must pass in a valid cds.
 * In case the start or end position is outside the bounds of the sequence, set retUpstreamBases
 * or retDownstreamBases to the number of bases beyond the beginning or end of sequence.
 * NOTE: intron offsets are ignored; the PSL contains the exon anchor point
 * and the caller will have to map that to the genome and then apply the intron offset. */
{
if (hgvs == NULL)
    return NULL;
if (hgvs->type == hgvstProtein)
    errAbort("pslFromHgvsNuc must be called only on nucleotide HGVSs, not protein.");
struct psl *psl;
AllocVar(psl);
psl->tName = cloneString(acc);
safecpy(psl->strand, sizeof(psl->strand), "+");
psl->tSize = accSize;
if (hgvs->type == hgvstGenomic || hgvs->type == hgvstMito)
    {
    // Sane 1-based fully closed coords.
    hgvsStartEndToZeroBasedHalfOpen(hgvs, &psl->tStart, &psl->tEnd);
    }
else
    {
    // Simple or insanely complex CDS-relative coords.
    hgvsTranscriptToZeroBasedHalfOpen(hgvs, accEnd, cds, &psl->tStart, &psl->tEnd,
                                      retUpstreamBases, retDownstreamBases);
    }
int refLen = psl->tEnd - psl->tStart;
// Just use refLen for alt until we parse the sequence change portion of the term:
int altLen = refLen;
psl->qName = cloneString(hgvs->changes);
psl->qSize = altLen;
psl->qStart = 0;
psl->qEnd = altLen;
psl->blockCount = 1;
AllocArray(psl->blockSizes, psl->blockCount);
AllocArray(psl->qStarts, psl->blockCount);
AllocArray(psl->tStarts, psl->blockCount);
psl->blockSizes[0] = refLen <= altLen ? refLen : altLen;
psl->qStarts[0] = psl->qStart;
psl->tStarts[0] = psl->tStart;
return psl;
}

static struct psl *pslDelFromCoord(struct psl *txAli, int tStart, struct psl *variantPsl)
/* Return a PSL with same target and query as txAli, but as a deletion at offset tStart:
 * two zero-length blocks surrounding no target and query = variantPsl's target coords. */
{
struct psl *del = pslNew(txAli->qName, txAli->qSize, variantPsl->tStart, variantPsl->tEnd,
                         txAli->tName, txAli->tSize, tStart, tStart,
                         txAli->strand, 2, 0);
// I wonder if zero-length blockSizes would trigger crashes somewhere...
del->blockSizes[0] = del->blockSizes[1] = 0;
del->tStarts[0] = del->tStarts[1] = del->tStart;
del->qStarts[0] = del->qStart;
del->qStarts[1] = del->qEnd;
return del;
}

struct psl *pslFromGap(struct psl *txAli, int blkIx, struct psl *variantPsl)
/* Return a PSL with same target and query as txAli, but as a potentially double-sided gap between
 * two zero-length blocks surrounding skipped bases in target and/or query. */
{
int qGapStart = txAli->qStarts[blkIx] + txAli->blockSizes[blkIx];
int qGapEnd = txAli->qStarts[blkIx+1];
int tGapStart = txAli->tStarts[blkIx] + txAli->blockSizes[blkIx];
int tGapEnd = txAli->tStarts[blkIx+1];
struct psl *gapPsl = pslNew(txAli->qName, txAli->qSize, qGapStart, qGapEnd,
                            txAli->tName, txAli->tSize, tGapStart, tGapEnd,
                            txAli->strand, 2, 0);
int qBlockStart = txAli->qStarts[blkIx];
if (variantPsl->tStart > qBlockStart && variantPsl->tStart < qGapStart)
    {
    // keep non-zero overlapping part of preceding block, if any
    int overlapSize = qGapStart - variantPsl->tStart;
    gapPsl->blockSizes[0] = overlapSize;
    gapPsl->tStart = gapPsl->tStarts[0] = tGapStart - overlapSize;
    gapPsl->qStart = gapPsl->qStarts[0] = variantPsl->tStart;
    }
else
    {
    // zero-length block at beginning of gap
    gapPsl->blockSizes[0] = 0;
    gapPsl->tStarts[0] = tGapStart;
    gapPsl->qStarts[0] = qGapStart;
    }

int qNextBlkEnd = txAli->qStarts[blkIx+1] + txAli->blockSizes[blkIx+1];
if (variantPsl->tEnd > qGapEnd && variantPsl->tEnd <= qNextBlkEnd)
    {
    // keep non-zero overlapping part of next block, if any
    int overlapSize = variantPsl->tEnd - qGapEnd;
    gapPsl->blockSizes[1] = overlapSize;
    gapPsl->tStarts[1] = tGapEnd;
    gapPsl->tEnd = tGapEnd + overlapSize;
    gapPsl->qStarts[1] = qGapEnd;
    gapPsl->qEnd = variantPsl->tEnd;
    }
else
    {
    // zero-length block at end of gap
    gapPsl->blockSizes[1] = 0;
    gapPsl->tStarts[1] = tGapEnd;
    gapPsl->qStarts[1] = qGapEnd;
    }
return gapPsl;
}

static struct psl *mapToDeletion(struct psl *variantPsl, struct psl *txAli)
/* If the variant falls on a transcript base that is deleted in the reference genome,
 * (or upstream/downstream mapped to a zero-length point),
 * return the deletion coords (pslTransMap returns NULL), otherwise return NULL. */
{
// variant start and end coords, in transcript coords:
int vStart = variantPsl->tStart;
int vEnd = variantPsl->tEnd;
boolean isRc = (pslQStrand(txAli) == '-');
if (isRc)
    {
    vStart = variantPsl->tSize - variantPsl->tEnd;
    vEnd = variantPsl->tSize - variantPsl->tStart;
    }
if (vEnd < txAli->qStart)
    return pslDelFromCoord(txAli, isRc ? txAli->tEnd : txAli->tStart, variantPsl);
else if (vStart > txAli->qEnd)
    return pslDelFromCoord(txAli, isRc ? txAli->tStart : txAli->tEnd, variantPsl);
int i;
for (i = 0;  i < txAli->blockCount - 1;  i++)
    {
    int qBlockEnd = txAli->qStarts[i] + txAli->blockSizes[i];
    int qNextBlockStart = txAli->qStarts[i+1];
    int tBlockEnd = txAli->tStarts[i] + txAli->blockSizes[i];
    int tNextBlockStart = txAli->tStarts[i+1];
    if (vStart >= qBlockEnd && vEnd <= qNextBlockStart)
        {
        if (tBlockEnd == tNextBlockStart)
            return pslDelFromCoord(txAli, tBlockEnd, variantPsl);
        else
            return pslFromGap(txAli, i, variantPsl);
        }
    }
// Not contained in a deletion from reference genome (txAli target) -- return NULL.
return NULL;
}


static struct psl *mapPsl(struct hgvsVariant *hgvs, char *acc, struct psl *txAli,
                          struct genbankCds *cds, int *retUpstream, int *retDownstream)
/* Use pslTransMap to map hgvs onto the genome. */
{
// variantPsl contains the anchor if a non-cdsStart anchor is used because
// the actual position might be outside the bounds of the transcript sequence (intron/UTR)
struct psl *variantPsl = pslFromHgvsNuc(hgvs, acc, txAli->qSize, txAli->qEnd, cds,
                                        retUpstream, retDownstream);
struct psl *mappedToGenome = pslTransMap(pslTransMapNoOpts, variantPsl, txAli);
// If there is a deletion in the genome / insertion in the transcript then pslTransMap cannot
// map those bases to the genome.  In that case take a harder look and return the deletion
// coords.
if (mappedToGenome == NULL)
    mappedToGenome = mapToDeletion(variantPsl, txAli);
pslFree(&variantPsl);
return mappedToGenome;
}

static char *pslTableForAcc(char *db, char *acc)
/* Based on acc (and whether db has NCBI RefSeq alignments), pick a PSL (or genePred for GENCODE)
 * table where acc should be found (table may or may not exist).  Don't free the returned string. */
{
char *pslTable = NULL;
if (startsWith("LRG_", acc))
    pslTable = "lrgTranscriptAli";
else if (startsWith("NM_", acc) || startsWith("NR_", acc) ||
         startsWith("XM_", acc) || startsWith("XR_", acc))
    {
    // Use NCBI's alignments if they are available
    if (hDbHasNcbiRefSeq(db))
        pslTable = "ncbiRefSeqPsl";
    else
        pslTable = "refSeqAli";
    }
return pslTable;
}

#define limitToRange(val, min, max) { if (val < min) { val = min; }  \
                                      if (val > max) { val = max; } }

static struct bed *pslAndFriendsToRegion(struct psl *psl, struct hgvsVariant *hgvs,
                                          int upstream, int downstream)
/* If hgvs has any intron offsets and/or upstream/downstream offsets, add those to the
 * anchor coords in psl and return the variant's region of the genome. */
{
struct bed *region = bed6New(psl->tName, psl->tStart, psl->tEnd, psl->qName, 0, psl->strand[0]);
// If the start and/or end is in an intron, add the intron offset now.
boolean revStrand = (psl->strand[0] == '-');
if (hgvs->startOffset != 0)
    {
    if (revStrand)
        region->chromEnd -= hgvs->startOffset;
    else
        region->chromStart += hgvs->startOffset;
    }
if (hgvs->endOffset != 0)
    {
    if (revStrand)
        region->chromStart -= hgvs->endOffset;
    else
        region->chromEnd += hgvs->endOffset;
    }
// Apply extra up/downstream offsets (usually 0)
if (revStrand)
    {
    region->chromStart -= downstream;
    region->chromEnd += upstream;
    }
else
    {
    region->chromStart -= upstream;
    region->chromEnd += downstream;
    }
limitToRange(region->chromStart, 0, psl->tSize);
limitToRange(region->chromEnd, 0, psl->tSize);
return region;
}

static struct psl *pslForQName(char *db, char *pslTable, char *acc)
/* Look up acc in pslTable's qName column; return psl or NULL if not found. */
{
struct psl *psl = NULL;
char query[2048];
struct sqlConnection *conn = hAllocConn(db);
char **row;
sqlSafef(query, sizeof(query), "select * from %s where qName = '%s'", pslTable, acc);
struct sqlResult *sr = sqlGetResult(conn, query);
if (sr && (row = sqlNextRow(sr)))
    {
    // All PSL tables used here, and any made in the future, use the bin column.
    int bin = 1;
    psl = pslLoad(row+bin);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return psl;
}

static struct psl *getPslAndCds(char *db, struct hgvsVariant *hgvs, char **pAcc,
                                struct genbankCds *cds, char **retPslTable)
/* Get PSL and cds (if applicable) for acc. */
{
struct psl *txAli = NULL;
cds->start = cds->end = -1;
cds->startComplete = cds->endComplete = cds->complement = FALSE;
char *acc = *pAcc;
if (startsWith("ENS", acc))
    {
    struct genePred *gp = getGencodeGp(db, acc);
    if (gp)
        {
        int qSize = 0; // infer -- no polyAs in Gencode because it's genome-based
        txAli = genePredToPsl(gp, hChromSize(db, gp->chrom), qSize);
        genePredToCds(gp, cds);
        if (retPslTable)
            {
            // Well, it's not PSL but it's the track table:
            struct sqlConnection *conn = hAllocConn(db);
            *retPslTable = hFindLatestGencodeTableConn(conn, "Comp");
            hFreeConn(&conn);
            }
        }
    }
else
    {
    char *pslTable = pslTableForAcc(db, acc);
    if (pslTable && hTableExists(db, pslTable))
        {
        if (hgvs->type == hgvstCoding)
            getCds(db, acc, cds);
        txAli = pslForQName(db, pslTable, acc);
        }
    // As of 9/26/16, ncbiRefSeqPsl is missing some items (#13673#note-443) -- so fall back
    // on UCSC alignments.
    if (!txAli && sameString(pslTable, "ncbiRefSeqPsl") && hTableExists(db, "refSeqAli"))
        {
        char *accNoVersion = cloneFirstWordByDelimiter(acc, '.');
        if (hgvs->type == hgvstCoding)
            getCds(db, accNoVersion, cds);
        txAli = pslForQName(db, "refSeqAli", accNoVersion);
        if (txAli)
            {
            pslTable = "refSeqAli";
            *pAcc = accNoVersion;
            }
        }
    if (txAli && retPslTable != NULL)
        *retPslTable = pslTable;
    }
return txAli;
}

static struct bed *hgvsMapNucToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Return a bed6 with the variant's span on the genome and strand, or NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */
{
if (hgvs->type == hgvstGenomic)
    return hgvsMapGDotToGenome(db, hgvs, retPslTable);
char *acc = normalizeVersion(db, hgvs->seqAcc, NULL);
if (isEmpty(acc))
    return NULL;
struct bed *region = NULL;
char *pslTable = NULL;
struct genbankCds cds;
struct psl *txAli = getPslAndCds(db, hgvs, &acc, &cds, &pslTable);
boolean gotCds = (cds.end > cds.start);
if (txAli && (hgvs->type != hgvstCoding || gotCds))
    {
    int upstream = 0, downstream = 0;
    struct psl *mappedToGenome = mapPsl(hgvs, acc, txAli, &cds, &upstream, &downstream);
    if (mappedToGenome)
        {
        region = pslAndFriendsToRegion(mappedToGenome, hgvs, upstream, downstream);
        pslFree(&mappedToGenome);
        }
    pslFree(&txAli);
    }
if (region)
    {
    adjustInsStartEnd(hgvs, &region->chromStart, &region->chromEnd);
    if (retPslTable)
        *retPslTable = cloneString(pslTable);
    }
return region;
}

static struct bed *hgvsMapPDotToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Return a bed6 with the variant's span on the genome and strand, or NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */
{
char *acc = normalizeVersion(db, hgvs->seqAcc, NULL);
if (isEmpty(acc))
    return NULL;
struct bed *region = NULL;
char *txAcc = NULL;
if (startsWith("LRG_", acc))
    txAcc = lrgProteinToTx(db, acc);
else if (startsWith("ENS", acc))
    txAcc = gencodeProteinToTx(db, acc);
else if (startsWith("NP_", acc) || startsWith("XP_", acc))
    {
    struct sqlConnection *conn = hAllocConn(db);
    char query[2048];
    if (hDbHasNcbiRefSeq(db))
        sqlSafef(query, sizeof(query), "select mrnaAcc from ncbiRefSeqLink where protAcc = '%s'",
                 acc);
    else if (hTableExists(db, "refGene"))
        sqlSafef(query, sizeof(query), "select mrnaAcc from %s l, refGene r "
                 "where l.protAcc = '%s' and r.name = l.mrnaAcc", refLinkTable, acc);
    else
        return NULL;
    txAcc = sqlQuickString(conn, query);
    hFreeConn(&conn);
    }
if (txAcc)
    {
    // Translate the p. to c. and map c. to the genome.
    struct hgvsVariant cDot;
    zeroBytes(&cDot, sizeof(cDot));
    cDot.type = hgvstCoding;
    cDot.seqAcc = txAcc;
    cDot.start1 = ((hgvs->start1 - 1) * 3) + 1;
    cDot.end = ((hgvs->end - 1) * 3) + 3;
    cDot.changes = hgvs->changes;
    region = hgvsMapNucToGenome(db, &cDot, retPslTable);
    freeMem(txAcc);
    }
return region;
}

struct bed *hgvsMapToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Return a bed6 with the variant's span on the genome and strand, or NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */
{
if (hgvs == NULL)
    return NULL;
struct bed *region = NULL;
if (hgvs->type == hgvstProtein)
    region = hgvsMapPDotToGenome(db, hgvs, retPslTable);
else
    region = hgvsMapNucToGenome(db, hgvs, retPslTable);
return region;
}

static int getDotVersion(char *acc)
/* If acc ends in .<number> then return number; otherwise return -1. */
{
char *p = strrchr(acc, '.');
if (p && isdigit(p[1]))
    return atoi(p+1);
return -1;
}

struct bed *hgvsValidateAndMap(struct hgvsVariant *hgvs, char *db, char *term,
                               struct dyString *dyWarn, char **retPslTable)
/* If we have sequence for hgvs->seqAcc and the hgvs coords are within the bounds
 * of that sequence, and we are able to map the coords in seqAcc to genomic coords in
 * db, return a bed6 with those coords and strand.  If unsuccessful, or if the reference
 * sequence differs at the mapped location, then write a warning message to dyWarn.
 * If mapping is successful and retPslTable is not NULL, set it to the psl table
 * used for mapping. */
{
struct bed *mapping = NULL;
char *pslTable = NULL;
char *foundAcc = NULL, *diffRefAllele = NULL;
int foundVersion = 0;
boolean coordsOk = hgvsValidate(db, hgvs, &foundAcc, &foundVersion, &diffRefAllele);
if (foundAcc == NULL)
    dyStringPrintf(dyWarn, "Can't find sequence for accession '%s'", hgvs->seqAcc);
else
    {
    int hgvsVersion = getDotVersion(hgvs->seqAcc);
    char foundAccWithV[strlen(foundAcc)+20];
    if (foundVersion)
        safef(foundAccWithV, sizeof(foundAccWithV), "%s.%d", foundAcc, foundVersion);
    else
        safecpy(foundAccWithV, sizeof(foundAccWithV), foundAcc);
    if (hgvsVersion >= 0 && hgvsVersion != foundVersion)
        dyStringPrintf(dyWarn, "HGVS term '%s' is based on %s but UCSC has version %s",
                       term, hgvs->seqAcc, foundAccWithV);
    if (! coordsOk)
        dyStringPrintf(dyWarn, "HGVS term '%s' has coordinates outside the bounds of %s",
                       term, foundAccWithV);
    else if (diffRefAllele != NULL)
        dyStringPrintf(dyWarn, "HGVS term '%s' reference value does not match %s value '%s'",
                       term, foundAccWithV, diffRefAllele);
    if (coordsOk)
        mapping = hgvsMapToGenome(db, hgvs, &pslTable);
    }
if (retPslTable)
    *retPslTable = cloneString(pslTable);
return mapping;
}


char *hgvsChangeGetAssertedRef(struct hgvsChange *change)
/* If change asserts a reference sequence value, return that, otherwise return NULL. */
{
if (change->type == hgvsctSubst || change->type == hgvsctDel || change->type == hgvsctDup ||
    change->type == hgvsctInv || change->type == hgvsctNoChange || change->type == hgvsctIns ||
    change->type == hgvsctCon)
    {
    return cloneString(change->value.refAlt.refSequence);
    }
// When change->type is hgvsctRepeat, the actual reference sequence usually spans a larger region
// than the repeating unit.  Without applying a fuzzy* tandem-repeat search to the reference
// assembly in the neighborhood of the HGVS coordinates, we can't tell what the true reference
// coords and sequence are.
// *fuzzy: ClinVar sometimes gives repeat counts that imply a few mismatches from
// the strict repetition of the consensus sequence.
// HGVS repeat notation is inherently flaky because the reference's repeat count may differ
// between assemblies, and is subject to how many mismatches are considered tolerable.
// So don't support it.
return NULL;
}

static struct dnaSeq *getGenbankSequenceRange(char *db, char *acc, int start, int end)
/* Look up acc in our local GenBank data; if found, and if both start and end are in bounds,
 * return sequence from start to end; otherwise return NULL. */
{
struct dnaSeq *seq = NULL;
int accVersion = getDotVersion(acc);
int foundVersion = 0;
char *normalizedAcc = normalizeVersion(db, acc, &foundVersion);
if (accVersion < 0 || accVersion == foundVersion)
    {
    char *wholeSeq = getCdnaSeq(db, normalizedAcc);
    if (wholeSeq && start >= 0 && end <= strlen(wholeSeq))
        {
        int size = end - start;
        char *name = ""; // ignored
        // newDnaSeq does not clone its dna input!  So allocate here.
        char *dna = cloneStringZ(wholeSeq+start, size);
        touppers(dna);
        seq = newDnaSeq(dna, size, name);
        }
    freez(&wholeSeq);
    }
return seq;
}

static boolean hgvsToTxCoords(struct hgvsVariant *hgvs, char *db, uint *retStart, uint *retEnd)
/* If hgvs is not coding then set *retStart to hgvs->start1-1 and *retEnd to hgvs->end & ret TRUE.
 * If coding and we have complete CDS info then apply cdsStart and/or cdsEnd offset to start & end.
 * If start or end is intronic, or start is negative (genomic upstream of tx) then return FALSE.
 * Note: at this point we don't know the length of the sequence so can't tell if start and/or end
 * fall past the end of the transcript (genomic downstream). */
{
boolean coordsOk = TRUE;
int start, end;
hgvsStartEndToZeroBasedHalfOpen(hgvs, &start, &end);
if (hgvs->type == hgvstCoding)
    {
    struct genbankCds cds;
    char *acc = normalizeVersion(db, hgvs->seqAcc, NULL);
    if (getCds(db, acc, &cds))
        {
        // Determine whether to use cdsStart or cdsEnd for hgvs start and hgvs end.
        // Make sure the cds start and end are marked as complete.
        if (hgvs->startIsUtr3 && cds.endComplete)
            start += cds.end;
        else if (!hgvs->startIsUtr3 && cds.startComplete)
            start += cds.start;
        else
            coordsOk = FALSE;
        if (hgvs->endIsUtr3 && cds.endComplete)
            end += cds.end;
        else if (!hgvs->endIsUtr3 && cds.startComplete)
            end += cds.start;
        else
            coordsOk = FALSE;
        }
    else
        coordsOk = FALSE;
    }
if (hgvs->startOffset != 0 || hgvs->endOffset != 0)
    // intronic start and/or end -- not in transcript bounds
    coordsOk = FALSE;
if (start < 0)
    coordsOk = FALSE;
// Unfortunately we don't know if start and end are past the end of transcript without
// doing another database query...
if (retStart)
    *retStart = start;
if (retEnd)
    *retEnd = end;
return coordsOk;
}

static char *getHgvsSequenceRange(struct hgvsVariant *hgvs, char *db)
/* If we have the sequence specified in hgvs, return the portion covered by hgvs. */
{
char *seq = NULL;
char *acc = hgvs->seqAcc;
// First see if acc can be translated to an internal chrom name.
// Note: an NC_ version from a different assembly might be used.  To be really fancy
// we could try hgOfficialChromName in other dbs... but wait for users to request it!
char *chrom = hgOfficialChromName(db, acc);
uint start = hgvs->start1-1;
uint end = hgvs->end;
if (chrom)
    {
    // Simple sequence range
    struct dnaSeq *dnaSeq = hChromSeq(db, chrom, start, end);
    touppers(dnaSeq->dna);
    seq = dnaSeqCannibalize(&dnaSeq);
    }
else if (hgvsToTxCoords(hgvs, db, &start, &end))
    {
    // Leave it up to getGenbankSequenceRange to reject out-of-bounds coords because
    // it will look up the sequence size.
    struct dnaSeq *dnaSeq = getGenbankSequenceRange(db, hgvs->seqAcc, start, end);
    seq = dnaSeqCannibalize(&dnaSeq);
    }
return seq;
}

char *hgvsGetRef(struct hgvsVariant *hgvs, char *db)
/* Return the sequence from the HGVS accession at the HGVS coords, or NULL if HGVS coords are
 * out of bounds (e.g. up/downstream or intronic) or otherwise unsupported.  Use "" as insertion
 * reference sequence. */
{
return getHgvsSequenceRange(hgvs, db);
}

static char *altFromNestedTerm(struct hgvsVariant *nestedTerm, char *db)
/* If we have sequence for the accession */
{
char *nestedRefSeq = getHgvsSequenceRange(nestedTerm, db);
if (nestedRefSeq && sameOk(nestedTerm->changes, "inv"))
    {
    // If nested term ended with "inv" then reverse-complement the sequence.
    reverseComplement(nestedRefSeq, strlen(nestedRefSeq));
    }
return nestedRefSeq;
}

static void copyMaybeRc(char *buf, size_t bufSize, char *seq, boolean isRc)
/* Copy seq into buf; if isRc, reverse-complement the copied sequence in buf. */
{
int seqLen = strlen(seq);
safencpy(buf, bufSize, seq, seqLen);
if (isRc)
    reverseComplement(buf, seqLen);
}

char *hgvsChangeGetAlt(struct hgvsChange *changeList, char *hgvsSeqRef, char *db)
/* Unpack changeList and return the alternate (changed from hgvsSeqRef) sequence, or NULL
 * if unable. */
{
struct dyString *alt = dyStringNew(0);
boolean success = TRUE;
if (hgvsSeqRef == NULL)
    errAbort("hgvsChangeGetAlt: must provide non-NULL hgvsSeqRef");
struct hgvsChange *change;
for (change = changeList;  change != NULL && success;  change = change->next)
    {
    if (change->type == hgvsctRepeat)
        // Unsupported -- abort and return NULL.
        success = FALSE;
    if (change->type == hgvsctNoChange)
        dyStringAppend(alt, hgvsSeqRef);
    else if (change->type == hgvsctDup)
        {
        dyStringAppend(alt, hgvsSeqRef);
        dyStringAppend(alt, hgvsSeqRef);
        }
    else if (change->type == hgvsctDel)
        // alt is the empty string
        ;
    else if (change->type == hgvsctInv)
        {
        int refLen = strlen(hgvsSeqRef);
        char refInv[refLen+1];
        copyMaybeRc(refInv, sizeof(refInv), hgvsSeqRef, TRUE);
        dyStringAppend(alt, refInv);
        }
    else if (change->type == hgvsctSubst || change->type == hgvsctIns || change->type == hgvsctCon)
        {
        struct hgvsChangeRefAlt *refAlt = &change->value.refAlt;
        if (refAlt->altType == hgvsstSimple)
            {
            dyStringAppend(alt, refAlt->altValue.seq);
            }
        else if (refAlt->altType == hgvsstLength)
            {
            int i;
            for (i = 0;  i < refAlt->altValue.length;  i++)
                dyStringAppendC(alt, 'N');
            }
        else if (refAlt->altType == hgvsstNestedTerm)
            {
            char *nestedTermSeq = altFromNestedTerm(&refAlt->altValue.term, db);
            if (nestedTermSeq)
                dyStringAppend(alt, nestedTermSeq);
            else
                success = FALSE;
            }
        else
            {
            success = FALSE;
            }
        }
    }
if (success)
    return dyStringCannibalize(&alt);
else
    {
    dyStringFree(&alt);
    return NULL;
    }
}

static boolean doDupToIns(boolean isRc, int *pLeftShiftOffset,
                          char *genomicRef, char *genomicRefFwd, char *hgvsSeqRef, char *hgvsSeqAlt,
                          char *pLeftBase, struct bed *mapping)
/* HGVS dup is TMI for VCF, so simplify to an insertion -- this can modify all of its inputs!
 * We need to maintain HVGS right-shifting before we do VCF left-shifting, so if this is a dup
 * then change chromStart to chromEnd (vice versa if isRc), update leftBase, trim duplicate seq
 * from start of alt, and set ref to "". */
{
boolean dupToIns = FALSE;
int refLen = mapping->chromEnd - mapping->chromStart;
int altLen = strlen(hgvsSeqAlt);
if (refLen > 0 && altLen >= 2*refLen &&
    sameString(genomicRef, hgvsSeqRef) &&
    startsWith(genomicRef, hgvsSeqAlt))
    {
    dupToIns = TRUE;
    if (isRc || sameString(genomicRef, hgvsSeqAlt+refLen))
        {
        // '-' strand or pure dup ==> insertion at chromStart, no change to leftBase
        mapping->chromEnd = mapping->chromStart;
        }
    else
        {
        // '+' strand and extra seq after dup ==> insertion at chromEnd, update leftBase & offset
        *pLeftShiftOffset += refLen;
        *pLeftBase = genomicRef[refLen-1];
        mapping->chromStart = mapping->chromEnd;
        }
    memmove(hgvsSeqAlt, hgvsSeqAlt+refLen, altLen-refLen+1);
    genomicRef[0] = genomicRefFwd[0] = hgvsSeqRef[0] = '\0';
    }
return dupToIns;
}

static char *makeVcfAlt(char *genomicRef, char *hgvsSeqRef, char *hgvsSeqAlt, struct bed *mapping,
                        boolean isRc, char leftBase, boolean leftBaseRight, boolean *retIsIndel)
/* Based on comparing the three possibly differing alleles (genomic, hgvs ref, hgvs alt),
 * determine which sequence(s) will go in alt.  Determine whether this variant is an indel
 * (alleles not all the same length), and if so prepend leftBase to alt allele(s). */
{
int hgvsSeqRefLen = strlen(hgvsSeqRef);
int hgvsSeqAltLen = strlen(hgvsSeqAlt);
// VCF alleles are always on '+' (Fwd) strand of genome.
char hgvsSeqRefFwd[hgvsSeqRefLen+1];
copyMaybeRc(hgvsSeqRefFwd, sizeof(hgvsSeqRefFwd), hgvsSeqRef, isRc);
char hgvsSeqAltFwd[hgvsSeqAltLen+1];
copyMaybeRc(hgvsSeqAltFwd, sizeof(hgvsSeqAltFwd), hgvsSeqAlt, isRc);
int genomicRefLen = strlen(genomicRef);
// The alt allele string will contain 0 to 2 sequences, maybe a comma, and 0 to 2 leftBases:
int altMaxLen = genomicRefLen + hgvsSeqRefLen + hgvsSeqAltLen + 16;
char vcfAlt[altMaxLen];
// If the HGVS change is "=" (no change) then vcfAlt will be "."
boolean noChange = sameString(hgvsSeqRef, hgvsSeqAlt);
// VCF indels have start coord one base to the left of the actual indel point.
// HGVS treats multi-base substitutions as indels but VCF does not, so look for
// differing length of ref vs. each alt sequence in order to call it a VCF indel.
boolean isIndel = (genomicRefLen != hgvsSeqRefLen || genomicRefLen != hgvsSeqAltLen ||
                   genomicRefLen == 0);
if (sameString(hgvsSeqRef, genomicRef))
    {
    // VCF alt is HGVS alt
    if (noChange)
        safecpy(vcfAlt, sizeof(vcfAlt), ".");
    else if (isIndel)
        {
        if (leftBaseRight)
            safef(vcfAlt, sizeof(vcfAlt), "%s%c", hgvsSeqAltFwd, leftBase);
        else
            safef(vcfAlt, sizeof(vcfAlt), "%c%s", leftBase, hgvsSeqAltFwd);
        }
    else
        safecpy(vcfAlt, sizeof(vcfAlt), hgvsSeqAltFwd);
    }
else if (sameString(genomicRef, hgvsSeqAlt))
    {
    // Genomic reference allele is HGVS alt allele; VCF alt is HGVS ref allele
    if (noChange)
        safecpy(vcfAlt, sizeof(vcfAlt), ".");
    else if (isIndel)
        {
        if (leftBaseRight)
            safef(vcfAlt, sizeof(vcfAlt), "%s%c", hgvsSeqRefFwd, leftBase);
        else
            safef(vcfAlt, sizeof(vcfAlt), "%c%s", leftBase, hgvsSeqRefFwd);
        }
    else
        safecpy(vcfAlt, sizeof(vcfAlt), hgvsSeqRefFwd);
    }
else
    {
    // Both HGVS ref and HGVS alt differ from genomicRef, so both are VCF alts (unless noChange)
    if (noChange)
        {
        if (isIndel)
            {
            if (leftBaseRight)
                safef(vcfAlt, sizeof(vcfAlt), "%s%c", hgvsSeqRefFwd, leftBase);
            else
                safef(vcfAlt, sizeof(vcfAlt), "%c%s", leftBase, hgvsSeqRefFwd);
            }
        else
            safef(vcfAlt, sizeof(vcfAlt), "%s", hgvsSeqRefFwd);
        }
    else if (isIndel)
        {
        if (leftBaseRight)
            safef(vcfAlt, sizeof(vcfAlt), "%s%c,%s%c",
                  hgvsSeqRefFwd, leftBase, hgvsSeqAltFwd, leftBase);
        else
            safef(vcfAlt, sizeof(vcfAlt), "%c%s,%c%s",
                  leftBase, hgvsSeqRefFwd, leftBase, hgvsSeqAltFwd);
        }
    else
        safef(vcfAlt, sizeof(vcfAlt), "%s,%s", hgvsSeqRefFwd, hgvsSeqAltFwd);
    }
if (retIsIndel)
    *retIsIndel = isIndel;
return cloneString(vcfAlt);
}

static char *makeHgvsToVcfInfo(boolean dupToIns, int basesShifted, boolean isIndel, int end)
/* Make a semicolon-separated list of any applicable interesting things. */
{
struct dyString *info = dyStringNew(0);
if (dupToIns)
    dyStringAppend(info, "DupToIns");
if (basesShifted)
    {
    dyStringAppendSep(info, ";");
    dyStringPrintf(info, "BasesShifted=%d", basesShifted);
    }
if (isIndel)
    {
    dyStringAppendSep(info, ";");
    // yet another special case for indel at beginning of chrom
    if (end == 0)
        end = 1;
    dyStringPrintf(info, "END=%d", end);
    }
if (dyStringIsEmpty(info))
    dyStringAppendC(info, '.');
return dyStringCannibalize(&info);
}

int vcfRowCmp(const void *va, const void *vb)
/* Compare for sorting based on chrom,pos. */
{
const struct vcfRow *a = *((struct vcfRow **)va);
const struct vcfRow *b = *((struct vcfRow **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->pos - b->pos;
return dif;
}

void vcfRowTabOutList(FILE *f, struct vcfRow *rowList)
/* Write out each row of VCF to f. */
{
struct vcfRow *row;
for (row = rowList;  row != NULL;  row = row->next)
    fprintf(f, "%s\t%d\t%s\t%s\t%s\t.\t%s\t%s\n",
            row->chrom, row->pos, row->id, row->ref, row->alt, row->filter, row->info);
}

static char *makeHgvsToVcfFilter(char *genomicRef, char *hgvsSeqRef, char *hgvsTermRef)
/* Check for sequence disagreements that we can report in the FILTER column */
{
struct dyString *filter = dyStringNew(0);
if (differentString(hgvsSeqRef, genomicRef))
    dyStringAppend(filter, "HgvsRefGenomicMismatch");
if (hgvsTermRef != NULL && differentString(hgvsTermRef, hgvsSeqRef))
    {
    dyStringAppendSep(filter, ";");
    dyStringAppend(filter, "HgvsRefAssertedMismatch");
    }
if (dyStringIsEmpty(filter))
    dyStringAppend(filter, "PASS");
return dyStringCannibalize(&filter);
}

static struct vcfRow *vcfFromHgvs(char *db, char *term, struct bed *mapping,
                                  struct hgvsVariant *hgvs, struct hgvsChange *changeList,
                                  boolean doLeftShift, struct dyString *dyError)
/* Determine ref and alt sequence, move start left one base if indel, and return vcfRow.
 * If doLeftShift, also shift items with ambiguous start as far left on the genome as
 * possible.  That is the VCF convention but it is inconvenient for variant annotation;
 * the HGVS standard is to shift right as far as possible on the tx strand, so consequences
 * are reported at the point where the sequence has changed. */
{
// Include some genomic sequence to the left (if possible) in case we need to left-shift
// an ambiguous mapping and/or include the base to the left of an indel.
int genomicRefLen = mapping->chromEnd - mapping->chromStart;
uint winStart, winEnd;
indelShiftRangeForVariant(mapping->chromStart, genomicRefLen, strlen(hgvs->changes),
                          &winStart, &winEnd);
struct seqWindow *seqWin = chromSeqWindowNew(db, mapping->chrom, winStart, winEnd);
int varWinStart = mapping->chromStart - seqWin->start;
int indelOffset = (mapping->chromStart == 0) ? 0 : 1;
char leftBase = seqWin->seq[varWinStart-indelOffset];
char genomicRefFwd[genomicRefLen+1];
safencpy(genomicRefFwd, sizeof(genomicRefFwd), seqWin->seq + varWinStart, genomicRefLen);
// VCF is always on '+' (Fwd) strand of reference.  HGVS sequences may be on the '-' strand.
// Make a version of the genome assembly allele for comparison to HGVS sequences.
boolean isRc = (mapping->strand[0] == '-');
char genomicRef[genomicRefLen+1];
copyMaybeRc(genomicRef, sizeof(genomicRef), genomicRefFwd, isRc);
// Get HGVS ref and alt alleles
char *hgvsSeqRef = hgvsGetRef(hgvs, db);
if (hgvsSeqRef == NULL)
    // hgvsSeqRef can be NULL when sequence coords are out of transcript bounds e.g. intron.
    // Use genomic reference sequence (rev-complemented if mapped to '-' strand)
    hgvsSeqRef = cloneString(genomicRef);
char *hgvsTermRef = hgvsChangeGetAssertedRef(changeList);
char *hgvsSeqAlt = hgvsChangeGetAlt(changeList, hgvsSeqRef, db);
if (hgvsIsInsertion(hgvs))
    // HGVS insertions are two bases (before and after), but ours are zero-length points,
    // so adjust hgvsSeqRef accordingly.
    hgvsSeqRef[0] = '\0';
if (hgvsSeqAlt == NULL)
    {
    dyStringAppend(dyError, "Unable to get alternate sequence");
    return NULL;
    }
char *filter = makeHgvsToVcfFilter(genomicRef, hgvsSeqRef, hgvsTermRef);
// doDupToIns and indelShift may modify the contents of their arguments:
boolean dupToIns = doDupToIns(isRc, &varWinStart, genomicRef, genomicRefFwd,
                              hgvsSeqRef, hgvsSeqAlt, &leftBase, mapping);
if (dupToIns)
    genomicRefLen = strlen(genomicRef);
int basesShifted = 0;
if (doLeftShift &&
    sameString(genomicRef, hgvsSeqRef))
    {
    int altLen = strlen(hgvsSeqAlt);
    char hgvsSeqAltFwd[altLen+1];
    copyMaybeRc(hgvsSeqAltFwd, sizeof(hgvsSeqAltFwd), hgvsSeqAlt, isRc);
    basesShifted = indelShift(seqWin, &mapping->chromStart, &mapping->chromEnd,
                              hgvsSeqAltFwd, INDEL_SHIFT_NO_MAX, isdLeft);
    if (basesShifted)
        {
        varWinStart = mapping->chromStart - seqWin->start;
        int leftBaseOffset = max(0, varWinStart - 1);
        leftBase = seqWin->seq[leftBaseOffset];
        copyMaybeRc(hgvsSeqAlt, altLen+1, hgvsSeqAltFwd, isRc);
        safencpy(genomicRefFwd, sizeof(genomicRefFwd), seqWin->seq+varWinStart, genomicRefLen);
        copyMaybeRc(genomicRef, sizeof(genomicRef), genomicRefFwd, isRc);
        safecpy(hgvsSeqRef, genomicRefLen+1, genomicRef);
        }
    }
// VCF special case for indel at beginning of chromosome: there is no left base, so instead
// put the first base of the chromosome to the right!
// See https://samtools.github.io/hts-specs/VCFv4.2.pdf 1.4.1.4 REF and discussion at
// https://sourceforge.net/p/vcftools/mailman/vcftools-spec/thread/508CAD45.8010301%40broadinstitute.org/
boolean leftBaseRight = (mapping->chromStart == 0);
// Make REF and ALT strings ('+' strand, leftBase for indels) and write VCF
boolean isIndel = FALSE;
char *vcfAlt = makeVcfAlt(genomicRef, hgvsSeqRef, hgvsSeqAlt, mapping, isRc,
                          leftBase, leftBaseRight,  &isIndel);
int pos = mapping->chromStart + 1;
if (isIndel && !leftBaseRight)
    pos--;
char vcfRef[genomicRefLen+1+1];
if (isIndel)
    {
    if (leftBaseRight)
        safef(vcfRef, sizeof(vcfRef), "%s%c", genomicRefFwd, leftBase);
    else
        safef(vcfRef, sizeof(vcfRef), "%c%s", leftBase, genomicRefFwd);
    }
else
    safecpy(vcfRef, sizeof(vcfRef), genomicRefFwd);
char *info = makeHgvsToVcfInfo(dupToIns, basesShifted, isIndel, mapping->chromEnd);
struct vcfRow *row;
AllocVar(row);
row->chrom = cloneString(mapping->chrom);
row->pos = pos;
row->id = cloneString(term);
row->ref = cloneString(vcfRef);
row->alt = vcfAlt;
row->filter = filter;
row->info = info;
freeMem(hgvsSeqRef);
freeMem(hgvsSeqAlt);
chromSeqWindowFree(&seqWin);
return row;
}

struct vcfRow *hgvsToVcfRow(char *db, char *term, boolean doLeftShift, struct dyString *dyError)
/* Convert HGVS to a row of VCF suitable for sorting & printing.  If unable, return NULL and
 * put the reason in dyError.  Protein terms are ambiguous at the nucleotide level so they are
 * not supported at this point. */
{
struct vcfRow *row = NULL;
struct hgvsVariant *hgvs = hgvsParseTerm(term);
if (!hgvs)
    {
    dyStringPrintf(dyError, "Unable to parse '%s' as HGVS", term);
    return NULL;
    }
else if (hgvs->type == hgvstProtein)
    {
    dyStringPrintf(dyError, "HGVS protein changes such as '%s' are ambiguous at the nucleotide "
                   "level, so not supported", term);
    return NULL;
    }
struct dyString *dyWarn = dyStringNew(0);
char *pslTable = NULL;
struct bed *mapping = hgvsValidateAndMap(hgvs, db, term, dyWarn, &pslTable);
if (!mapping)
    {
    dyStringPrintf(dyError, "Unable to map '%s' to reference %s: %s",
                   term, db, dyStringContents(dyWarn));
    }
else
    {
    if (dyStringIsNotEmpty(dyWarn))
        {
        dyStringAppend(dyError, dyStringContents(dyWarn));
        dyStringClear(dyWarn);
        }
    struct hgvsChange *changeList = hgvsParseNucleotideChange(hgvs->changes, hgvs->type,
                                                              dyWarn);
    if (changeList == NULL)
        {
        if (isEmpty(hgvs->changes))
            dyStringPrintf(dyError, "HGVS term '%s' does not specify any sequence changes", term);
        else if (dyStringIsNotEmpty(dyWarn))
            dyStringPrintf(dyError, "Unable to parse HGVS description in '%s': %s", term,
                           dyStringContents(dyWarn));
        else
            dyStringPrintf(dyError, "Unable to parse HGVS description in '%s'", term);
        }
    else if (dyStringIsNotEmpty(dyWarn))
        {
        dyStringPrintf(dyError, "Unable to parse HGVS description in '%s': %s",
                       term, dyStringContents(dyWarn));
        }
    else if (changeList->type == hgvsctRepeat)
        {
        dyStringPrintf(dyError,
                       "Can't convert '%s' to VCF: HGVS repeat notation is not supported.", term);
        }
    else
        {
        row = vcfFromHgvs(db, term, mapping, hgvs, changeList, doLeftShift, dyWarn);
        if (dyStringIsNotEmpty(dyWarn))
            {
            dyStringPrintf(dyError, "Can't convert '%s' to VCF: %s",
                           term, dyStringContents(dyWarn));
            }
        }
    }
bedFree(&mapping);
dyStringFree(&dyWarn);
return row;
}

static int allNCount(char *seq)
/* If seq is entirely N's, return its length, otherwise 0. */
{
int nLen = 0;
for (nLen = 0;  seq[nLen] != '\0';  nLen++)
    if (seq[nLen] != 'N')
        return 0;
return nLen;
}

// HGVS allows terms to specify deleted or duplicated bases if there are "several bases".
// Search for "NOTE: it is allowed" here:
//   http://varnomen.hgvs.org/recommendations/DNA/variant/deletion/
//   http://varnomen.hgvs.org/recommendations/DNA/variant/duplication/
// It does not say how many "several" can be.  It doesn't specify for inv, but common
// practice is to include bases and I don't see why it should be different from del or dup.
// If somebody complains about hardcoding then I guess we could make it a parameter of
// hgvsAppendChangesFromNucRefAlt.
#define HGVS_SEVERAL 30

static void hgvsAppendChangesFromNucRefAlt(struct dyString *dy, char *ref, char *alt, int dupLen,
                                           boolean breakDelIns)
/* Translate reference allele and alternate allele into an HGVS change description, append to dy.
 * If breakDelIns, then show deleted bases (e.g. show 'delAGinsTT' instead of 'delinsTT').
 * Note: this forces ref and alt to uppercase.
 * No support for con or repeats at this point, just {=, >, del, dup, ins, inv}. */
{
touppers(ref);
touppers(alt);
if (sameString(ref, alt))
    dyStringPrintf(dy, "%s=", ref);
else
    {
    int refLen = strlen(ref);
    int altLen = strlen(alt);
    char refInv[refLen+1];
    safencpy(refInv, sizeof(refInv), ref, refLen);
    reverseComplement(refInv, refLen);
    if (refLen == 1 && altLen == 1)
        dyStringPrintf(dy, "%c>%c", ref[0], alt[0]);
    else if (dupLen > 0)
        {
        dyStringAppend(dy, "dup");
        if (dupLen <= HGVS_SEVERAL)
            dyStringAppendN(dy, alt, dupLen);
        // Could be a pure duplication followed by insertion:
        if (altLen > dupLen)
            dyStringPrintf(dy, "ins%s", alt+dupLen);
        }
    else if (refLen == 0)
        {
        int allNLen = allNCount(alt);
        if (allNLen)
            dyStringPrintf(dy, "ins%d", allNLen);
        else
            dyStringPrintf(dy, "ins%s", alt);
        }
    else if (altLen == 0)
        {
        dyStringAppend(dy, "del");
        if (refLen <= HGVS_SEVERAL)
            dyStringAppendN(dy, ref, refLen);
        }
    else if (refLen == altLen && refLen > 1 && sameString(refInv, alt))
        {
        dyStringAppend(dy, "inv");
        if (refLen <= HGVS_SEVERAL)
            dyStringAppendN(dy, ref, refLen);
        }
    else
        {
        dyStringAppend(dy, "del");
        if (breakDelIns && refLen <= HGVS_SEVERAL)
            dyStringAppendN(dy, ref, refLen);
        int allNLen = allNCount(alt);
        if (allNLen)
            dyStringPrintf(dy, "ins%d", allNLen);
        else
            dyStringPrintf(dy, "ins%s", alt);
        }
    }
}

void hgvsAppendChangesFromPepRefAlt(struct dyString *dy, char *ref, char *alt, int dupLen)
/* Translate reference allele and alternate allele into an HGVS change description, append to dy.
 * ref and alt must be sequences of single-letter IUPAC amino acid codes.
 * Note: this forces ref and alt to uppercase. */
{
touppers(ref);
touppers(alt);
if (sameString(ref, alt))
    dyStringAppendC(dy, '=');
else
    {
    int refLen = strlen(ref);
    int altLen = strlen(alt);
    char altAbbr[3*altLen+1];
    int ix;
    for (ix = 0;  ix < altLen;  ix++)
        aaToAbbr(alt[ix], altAbbr+3*ix, sizeof(altAbbr)-3*ix);
    if (refLen == 1 && altLen == 1)
        // Simple substitution
        dyStringAppend(dy, altAbbr);
    else if (dupLen > 0)
        {
        dyStringAppend(dy, "dup");
        // Could be a pure duplication followed by insertion:
        if (altLen > dupLen)
            dyStringPrintf(dy, "ins%s", altAbbr+(dupLen*3));
        }
    else if (refLen == 0)
        dyStringPrintf(dy, "ins%s", altAbbr);
    else if (altLen == 0)
        {
        dyStringAppend(dy, "del");
        }
    else
        {
        dyStringPrintf(dy, "delins%s", altAbbr);
        }
    }
}

char *hgvsGFromVariant(struct seqWindow *gSeqWin, struct bed3 *variantBed, char *alt, char *acc,
                       boolean breakDelIns)
/* Return an HGVS g. string representing the genomic variant at the position of variantBed with
 * reference allele from gSeqWin and alternate allele alt.  3'-shift indels if applicable.
 * If acc is non-NULL it is used instead of variantBed->chrom.
 * If breakDelIns, then show deleted bases (eg show 'delAGinsTT' instead of 'delinsTT'). */
{
struct dyString *dy = dyStringCreate("%s:g.", acc ? acc : variantBed->chrom);
uint vStart = variantBed->chromStart, vEnd = variantBed->chromEnd;
assert (vEnd >= vStart);
gSeqWin->fetch(gSeqWin, variantBed->chrom, vStart, vEnd);
int refLen = vEnd - vStart;
char ref[refLen+1];
seqWindowCopy(gSeqWin, vStart, refLen, ref, sizeof(ref));
int altLen = strlen(alt);
char altCpy[altLen+1];
safecpy(altCpy, sizeof(altCpy), alt);
if (differentString(ref, altCpy))
    // Trim identical bases from start and end -- unless this is an assertion that there is
    // no change, in which case it's good to keep the range on which that assertion was made.
    trimRefAlt(ref, altCpy, &vStart, &vEnd, &refLen, &altLen);
if (indelShiftIsApplicable(refLen, altLen) &&
    indelShift(gSeqWin, &vStart, &vEnd, altCpy, INDEL_SHIFT_NO_MAX, isdRight))
    // update ref
    seqWindowCopy(gSeqWin, vStart, refLen, ref, sizeof(ref));
int dupLen = 0;
if (refLen == 1)
    // Single base: single 1-based coordinate
    dyStringPrintf(dy, "%d", vStart+1);
else if (refLen == 0)
    {
    // Insertion or duplication
    if (altLen > 0 && altLen <= vStart)
        {
        // Detect duplicated sequence
        uint seqStart = vStart - altLen;
        char precedingRef[altLen+1];
        seqWindowCopy(gSeqWin, seqStart, altLen, precedingRef, sizeof(precedingRef));
        if (sameString(altCpy, precedingRef))
            dupLen = altLen;
        }
    if (dupLen > 0)
        {
        if (dupLen == 1)
            // Single-base duplication
            dyStringPrintf(dy, "%d", vStart); // - dupLen + 1 cancel each other out
        else
            // Multi-base duplication: range is the dupLen bases preceding vStart
            dyStringPrintf(dy, "%d_%d", vStart - dupLen + 1, vStart);
        }
    else
        // Insertion: two-base range enclosing zero-base insertion point
        dyStringPrintf(dy, "%d_%d", vStart, vEnd+1);
    }
else
    // Deletion or MNV
    dyStringPrintf(dy, "%d_%d", vStart+1, vEnd);
hgvsAppendChangesFromNucRefAlt(dy, ref, altCpy, dupLen, breakDelIns);
return dyStringCannibalize(&dy);
}

static int findDup(char *alt, struct seqWindow *seqWin, uint refPoint, boolean isRc)
/* Given that the variant is an insertion of alt at refPoint (minding isRc), if alt looks
 * like a duplicate of the sequence before refPoint, return the length of duplicated sequence. */
{
int altLen = strlen(alt);
// Don't underflow the sequence:
if (!isRc && altLen > refPoint)
    return 0;
uint seqStart = refPoint - (isRc ? 0 : altLen);
uint minEnd = seqStart + altLen;
if (seqWin->start > seqStart || seqWin->end < minEnd)
    seqWin->fetch(seqWin, seqWin->seqName,
                  min(seqStart, seqWin->start),
                  max(seqWin->end, minEnd));
// Get sequence, reverse-complement if necessary
char precedingRef[altLen+1];
seqWindowCopy(seqWin, seqStart, altLen, precedingRef, sizeof(precedingRef));
if (isRc)
    reverseComplement(precedingRef, altLen);
if (sameString(alt, precedingRef))
    return altLen;
else
    {
    // Detect an insertion plus a few slop bases at the end, like "dupinsTAT" where
    // alt starts with precedingRef[3..] followed by new "TAT".  Then dupLen is altLen-3.
    // Unfortunately, indelShift can actually shift far enough to prevent a perfect match. :(
    // Example: ClinVar NM_000179.2(MSH6):c.1573_3439-429dupinsTAT -- the insertion is right-
    // shifted by 1 and then there's no longer a perfect dup. :b  We would have to do dup-aware
    // indel shifting!!
    int searchLimit = 5;
    int offset;
    for (offset = 1;  offset < searchLimit;  offset++)
        {
        if (sameStringN(alt, precedingRef+offset, altLen-offset))
            return altLen-offset;
        }
    }
return 0;
}

static int tweakInsDup(struct vpTxPosition *startPos, struct vpTxPosition *endPos, char *alt,
                       struct seqWindow *gSeqWin, struct psl *txAli, struct dnaSeq *txSeq)
/* If this variant is an insertion, HGVS needs the 2-base range surrounding the insertion point.
 * However, if an inserted sequence happens to be identical to the preceding sequence then
 * HGVS calls it a dup on the range of preceding sequence.  If this is a dup, return the length of
 * duplicated sequence (else return 0).
 * This modifies startPos and endPos if the variant is an insertion/dup.
 * For non-exonic insertions, the preceding reference sequence is from the genome not tx.
 * Note: this doesn't yet detect "dupins", where most of the inserted sequence (starting from the
 * beginning of the inserted sequence) matches the previous ref sequence */
{
int dupLen = 0;
int isIns = vpTxPosIsInsertion(startPos, endPos);
if (isIns)
    {
    // Yes, this is a zero-length insertion point -- see if it happens to be a duplication.
    // "insTCA" is preferable to "dupTinsCA"... where to draw the line for when dup is worth it??
    // For now, just say half of altLen... but an argument could be made for some small constant too
    int minDup = strlen(alt) / 2;
    // Fetch preceding sequence from tx if exonic, otherwise from genome.
    if (startPos->region == vpExon && endPos->region == vpExon)
        {
        struct seqWindow *txSeqWin = memSeqWindowNew(txSeq->name, txSeq->dna);
        dupLen = findDup(alt, txSeqWin, startPos->txOffset, FALSE);
        if (dupLen > minDup)
            {
            // Since txSeqWin was used to find dup, the new startPos must also be exonic.
            // In case startPos was looking forward from the boundary between an exon and an intron
            // or downstream, adjust its other fields to be exonic now:
            startPos->region = vpExon;
            startPos->txOffset -= dupLen;
            startPos->gDistance = startPos->intron3TxOffset = startPos->intron3Distance = 0;
            }
        else
            dupLen = 0;
        memSeqWindowFree(&txSeqWin);
        }
    else
        {
        boolean isRc = (pslQStrand(txAli) == '-');
        dupLen = findDup(alt, gSeqWin, startPos->gOffset, isRc);
        if (dupLen > minDup)
            {
            uint newGOffset = startPos->gOffset + (isRc ? dupLen : -dupLen);
            vpPosGenoToTx(newGOffset, txAli, startPos, FALSE);
            }
        else
            dupLen = 0;
        }
    }
if (dupLen == 0 && isIns)
    {
    // Expand insertion point to 2-base region around insertion point for HGVS.
    // startPos = base to left of endPos whose region looks left/5';
    // endPos = base to right of startPos whose region looks right/3'.
    struct vpTxPosition newStart = *endPos;
    vpTxPosSlideInSameRegion(&newStart, -1);
    struct vpTxPosition newEnd = *startPos;
    vpTxPosSlideInSameRegion(&newEnd, 1);
    *startPos = newStart;
    *endPos = newEnd;
    }
return dupLen;
}

uint hgvsTxToCds(uint txOffset, struct genbankCds *cds, boolean isStart, char pPrefix[2])
/* Return the cds-relative HGVS coord and prefix corresponding to 0-based txOffset & cds. */
{
// Open end adjustment for determining CDS/UTR region:
int endCmp = isStart ? 0 : 1;
// For adjusting non-negative coords to HGVS's positive 1-based closed:
int oneBased = isStart ? 1 : 0;
// For adjusting negative coords to HGVS's negative 0-based closed:
int closedEnd = isStart ? 0 : 1;
pPrefix[1] = '\0';
uint cdsCoord = 0;
if (txOffset - endCmp < cds->start)
    {
    // 5'UTR: coord is negative distance from cdsStart
    pPrefix[0] = '-';
    cdsCoord = cds->start - txOffset + closedEnd;
    }
else if (txOffset - endCmp < cds->end)
    {
    // CDS: coord is positive distance from cdsStart
    pPrefix[0] = '\0';
    cdsCoord = txOffset - cds->start + oneBased;
    }
else
    {
    // 3'UTR: coord is positive distance from cdsEnd
    pPrefix[0] = '*';
    cdsCoord = txOffset - cds->end + oneBased;
    }
return cdsCoord;
}

static void appendHgvsNucPos(struct dyString *dy, struct vpTxPosition *txPos, boolean isStart,
                             struct genbankCds *cds)
/* Translate txPos (start or end) into an HGVS position (coding if cds is non-NULL). */
{
// Open end adjustment for determining region:
int endCmp = isStart ? 0 : 1;
// For adjusting non-negative coords to HGVS's positive 1-based closed:
int oneBased = isStart ? 1 : 0;
// For adjusting negative coords to HGVS's negative 0-based closed:
int closedEnd = isStart ? 0 : 1;
if (txPos->region == vpUpstream)
    {
    uint distance = txPos->gDistance;
    if (cds)
        distance += cds->start;
    dyStringPrintf(dy, "-%u", distance + closedEnd);
    }
else if (txPos->region == vpDownstream)
    {
    uint distance = txPos->txOffset + txPos->gDistance;
    if (cds)
        dyStringPrintf(dy, "*%u", distance - cds->end + oneBased);
    else
        dyStringPrintf(dy, "%u", distance + oneBased);
    }
else if (txPos->region == vpExon)
    {
    char cdsPrefix[2];
    cdsPrefix[0] = '\0';
    uint hgvsCoord = cds ? hgvsTxToCds(txPos->txOffset, cds, isStart, cdsPrefix) :
                           txPos->txOffset + oneBased;
    dyStringPrintf(dy, "%s%u", cdsPrefix, hgvsCoord);
    }
else if (txPos->region == vpIntron)
    {
    // If intron length is odd, bias toward picking the 5' exon (middle base has positive offset).
    char cdsPrefix[2];
    cdsPrefix[0] = '\0';
    char direction;
    uint exonAnchor, intronOffset;
    boolean anchorIsStart;
    if (txPos->gDistance - endCmp < txPos->intron3Distance)
        {
        exonAnchor = txPos->txOffset;
        direction = '+';
        intronOffset = txPos->gDistance + oneBased;
        anchorIsStart = FALSE;
        }
    else
        {
        exonAnchor = txPos->intron3TxOffset;
        direction = '-';
        intronOffset = txPos->intron3Distance + closedEnd;
        anchorIsStart = TRUE;
        }
    exonAnchor = cds ? hgvsTxToCds(exonAnchor, cds, anchorIsStart, cdsPrefix) :
                       exonAnchor + (anchorIsStart ? oneBased : 0);
    dyStringPrintf(dy, "%s%u%c%u", cdsPrefix, exonAnchor, direction, intronOffset);
    }
else
    errAbort("appendHgvsNucPos: unrecognized vpTxRegion value %d", txPos->region);
}

char *hgvsNFromVpTx(struct vpTx *vpTx, struct seqWindow *gSeqWin, struct psl *txAli,
                    struct dnaSeq *txSeq, boolean breakDelIns)
/* Return an HGVS n. (noncoding transcript) term for a variant projected onto a transcript.
 * gSeqWin must already have at least the correct seqName if not the surrounding sequence.
 * If breakDelIns, then show deleted bases (eg show 'delAGinsTT' instead of 'delinsTT'). */
{
struct dyString *dy = dyStringCreate("%s:n.", vpTx->txName);
// Make local copies of vpTx->{start,end} -- we may need to modify them for HGVS ins/dup.
struct vpTxPosition startPos = vpTx->start, endPos = vpTx->end;
int dupLen = tweakInsDup(&startPos, &endPos, vpTx->txAlt, gSeqWin, txAli, txSeq);
appendHgvsNucPos(dy, &startPos, TRUE, NULL);
if (!vpTxPosRangeIsSingleBase(&startPos, &endPos))
    {
    dyStringAppendC(dy, '_');
    appendHgvsNucPos(dy, &endPos, FALSE, NULL);
    }
char *ref = vpTxGetRef(vpTx);
hgvsAppendChangesFromNucRefAlt(dy, ref, vpTx->txAlt, dupLen, breakDelIns);
return dyStringCannibalize(&dy);
}


char *hgvsCFromVpTx(struct vpTx *vpTx, struct seqWindow *gSeqWin, struct psl *txAli,
                    struct genbankCds *cds,  struct dnaSeq *txSeq, boolean breakDelIns)
/* Return an HGVS c. (coding transcript) term for a variant projected onto a transcript w/cds.
 * gSeqWin must already have at least the correct seqName if not the surrounding sequence.
 * If breakDelIns, then show deleted bases (eg show 'delAGinsTT' instead of 'delinsTT'). */
{
struct dyString *dy = dyStringCreate("%s:c.", vpTx->txName);
// Make local copies of vpTx->{start,end} -- we may need to modify them for HGVS ins/dup.
struct vpTxPosition startPos = vpTx->start, endPos = vpTx->end;
int dupLen = tweakInsDup(&startPos, &endPos, vpTx->txAlt, gSeqWin, txAli, txSeq);
appendHgvsNucPos(dy, &startPos, TRUE, cds);
if (!vpTxPosRangeIsSingleBase(&startPos, &endPos))
    {
    dyStringAppendC(dy, '_');
    appendHgvsNucPos(dy, &endPos, FALSE, cds);
    }
char *ref = vpTxGetRef(vpTx);
hgvsAppendChangesFromNucRefAlt(dy, ref, vpTx->txAlt, dupLen, breakDelIns);
return dyStringCannibalize(&dy);
}

static boolean isStartLoss(struct vpPep *vpPep)
/* Return TRUE if vpPep shows that the start codon has been lost. */
{
return (vpPep->start == 0 &&
        isNotEmpty(vpPep->ref) && vpPep->ref[0] == 'M' &&
        (isEmpty(vpPep->alt) || vpPep->alt[0] != 'M'));
}

char *hgvsPFromVpPep(struct vpPep *vpPep, struct dnaSeq *protSeq, boolean addParens)
/* Return an HGVS p. (protein) term for a variant projected into protein space.
 * Strict HGVS compliance requires parentheses around predicted protein changes, but
 * nobody seems to do that in practice.
 * Return NULL if an input is NULL. */
{
if (vpPep == NULL || protSeq == NULL)
    return NULL;
struct dyString *dy = dyStringCreate("%s:p.", vpPep->name);
if (addParens)
    dyStringAppendC(dy, '(');
int refLen = vpPep->end - vpPep->start;
// When predicting frameshift/extension, the length of ref may be different from refLen
int refExtLen = vpPep->ref ? strlen(vpPep->ref) : refLen;
int altLen = vpPep->alt ? strlen(vpPep->alt) : 0;
char refStartAbbr[4];
if (vpPep->ref)
    aaToAbbr(vpPep->ref[0], refStartAbbr, sizeof(refStartAbbr));
else
    // If ref is null then we should be writing just '=' or '?' but prevent garbage just in case:
    safecpy(refStartAbbr, sizeof(refStartAbbr), "?");
// protSeq may or may not end with X, so treat protSeq->size accordingly
boolean hitsStopCodon = (vpPep->end > protSeq->size ||
                         ((protSeq->dna[protSeq->size-1] == 'X') && vpPep->end == protSeq->size));
if (vpPep->cantPredict || isStartLoss(vpPep))
    dyStringAppend(dy, "?");
else if (vpPep->frameshift)
    {
    dyStringPrintf(dy, "%s%d", refStartAbbr, vpPep->start+1);
    if (altLen == 1)
        dyStringAppend(dy, "Ter");
    else
        {
        char altStartAbbr[4];
        aaToAbbr(vpPep->alt[0], altStartAbbr, sizeof(altStartAbbr));
        // For stop-loss extension, make it "ext*"
        if (hitsStopCodon && altLen > refExtLen)
            dyStringPrintf(dy, "%sext*%d", altStartAbbr, altLen - refExtLen);
        else
            dyStringPrintf(dy, "%sfsTer%d", altStartAbbr, altLen);
        }
    }
else if (hitsStopCodon && altLen > refExtLen)
    {
    // Stop loss extension from something that doesn't disrupt frame
    char altStartAbbr[4];
    aaToAbbr(vpPep->alt[0], altStartAbbr, sizeof(altStartAbbr));
    dyStringPrintf(dy, "%s%d%sext*%d", refStartAbbr, vpPep->start+1,
                   altStartAbbr, altLen - refExtLen);
    }
else
    {
    int dupLen = 0;
    if (refLen == 0 && altLen > 0)
        {
        // It's an insertion; is it a duplication?
        struct seqWindow *pSeqWin = memSeqWindowNew(protSeq->name, protSeq->dna);
        dupLen = findDup(vpPep->alt, pSeqWin, vpPep->start, FALSE);
        memSeqWindowFree(&pSeqWin);
        }
    if (refLen == 1)
        dyStringPrintf(dy, "%s%d", refStartAbbr, vpPep->start+1);
    else
        {
        int rangeStart = vpPep->start, rangeEnd = vpPep->end;
        char *pSeq = protSeq->dna;
        if (dupLen > 0)
            {
            // Duplication; position range changes to preceding bases.
            rangeEnd = rangeStart;
            rangeStart -= dupLen;
            aaToAbbr(pSeq[rangeStart], refStartAbbr, sizeof(refStartAbbr));
            }
        else if (refLen == 0)
            {
            // Insertion; expand to two-AA range around insertion point
            rangeStart--;
            aaToAbbr(pSeq[rangeStart], refStartAbbr, sizeof(refStartAbbr));
            rangeEnd++;
            }
        hitsStopCodon = (rangeEnd > protSeq->size ||
                         ((protSeq->dna[protSeq->size-1] == 'X') && rangeEnd == protSeq->size));
        char refLastAbbr[4];
        if (hitsStopCodon)
            aaToAbbr('X', refLastAbbr, sizeof(refLastAbbr));
        else
            aaToAbbr(pSeq[rangeEnd-1], refLastAbbr, sizeof(refLastAbbr));
        if (dupLen == 1)
            dyStringPrintf(dy, "%s%d", refStartAbbr, rangeStart+1);
        else
            dyStringPrintf(dy, "%s%d_%s%d", refStartAbbr, rangeStart+1, refLastAbbr, rangeEnd);
        }
    hgvsAppendChangesFromPepRefAlt(dy, vpPep->ref, vpPep->alt, dupLen);
    }
if (addParens)
    dyStringAppendC(dy, ')');
return dyStringCannibalize(&dy);
}
