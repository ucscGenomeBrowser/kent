/* hgHgvs.c - Parse the Human Genome Variation Society (HGVS) nomenclature for variants. */
/* See http://varnomen.hgvs.org/ and https://github.com/mutalyzer/mutalyzer/ */

/* Copyright (C) 2016 The Regents of the University of California
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hgHgvs.h"
#include "chromInfo.h"
#include "genbank.h"
#include "hdb.h"
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

// Regular expressions for HGVS-recognized sequence accessions: LRG or versioned RefSeq:
#define lrgTranscriptExp "(LRG_[0-9]+t[0-9+])"
#define lrgRegionExp "(LRG_[0-9+])"

// NC = RefSeq reference assembly chromosome
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
#define versionedRefSeqNCExp versionedAccPrefixExp("NC")
#define versionedRefSeqNGExp versionedAccPrefixExp("NG")
#define versionedRefSeqNMExp versionedAccPrefixExp("NM")
#define versionedRefSeqNPExp versionedAccPrefixExp("NP")
#define versionedRefSeqNMRExp versionedAccPrefixExp("N[MR]")

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
#define hgvsPDotSubstExp "p\\." hgvsAminoAcidSubstExp
//                                 ...                                  // original sequence
//                                              ......                  // 1-based position
//                                                           ...        // replacement sequence

// Protein range (or just single pos) regex
#define hgvsAaRangeExp "(" hgvsAminoAcidExp ")" posIntExp "(_(" hgvsAminoAcidExp ")" posIntExp ")?(.*)"
#define hgvsPDotRangeExp "p\\." hgvsAaRangeExp
//  original start AA           ...
//  1-based start position                       ...
//  optional range sep and AA+pos                          .....................................
//  original end AA                                              ...
//  1-based end position                                                          ...
//  change description                                                                    ...

// Complete HGVS term regexes combining sequence identifiers and change operations
#define hgvsFullRegex(seq, op) "^" seq "[ :]+" op

#define hgvsRefSeqNCGDotPosExp hgvsFullRegex(versionedRefSeqNCExp, hgvsGMDotPosExp)
#define hgvsRefSeqNCGDotExp hgvsRefSeqNCGDotPosExp "(.*)"
// substring numbering:
//      0.....................................  whole matching string
//      1.................                      accession and optional dot version
//               2........                      optional dot version
//                       3......                (n/a) optional gene symbol in ()s
//                        4....                 (n/a) optional gene symbol
//                              5.              g or m
//                                6..           1-based start position
//                                   7...       optional range separator and end position
//                                     8..      1-based end position
//                                        9...  change description

#define hgvsLrgCDotPosExp hgvsFullRegex(lrgTranscriptExp, hgvsCDotPosExp)
#define hgvsLrgCDotExp hgvsLrgCDotPosExp "(.*)"
// substring numbering:
//      0.....................................................  whole matching string
//      1...................                                    LRG transcript
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
//      1...................                                    LRG transcript
//                   2...                                       n/a 4 n.: UTR anchor "-" or "*"
//                       3...                                   mandatory first number
//                           4.......                           optional offset separator and offset
//                           5...                               offset separator
//                               6...                           offset number
//                                   7...............           optional range sep and complex num
//                                   8...                       n/a 4 n.: UTR anchor "-" or "*"
//                                       9...                   first number
//                                          10.......           optional offset separator and offset
//                                          11...               offset separator
//                                              12...           offset number
//                                                  13........  sequence change

#define hgvsRefSeqNMRNDotExp hgvsFullRegex(versionedRefSeqNMRExp, hgvsNDotPosExp) "(.*)"
// substring numbering:
//      0...............................................................  whole matching string
//      1...............                                                  acc & optional dot version
//             2........                                                  optional dot version
//                       3.....                                           optional gene sym in ()s
//                        4...                                            optional gene symbol
//                              5...                                      n/a 4 n.: UTR anchor
//                                  6...                                  mandatory first number
//                                      7.......                          optional offset
//                                      8...                              offset separator
//                                          9...                          offset number
//                                             10...............          optional range
//                                             11...                      n/a 4 n.: UTR anchor
//                                                12...                   first number
//                                                     13.......          optional offset
//                                                     14...              offset separator
//                                                         15...          offset number
//                                                             16........ sequence change

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
// Sometimes users give an NM_ accession, but a protein change.
#define pseudoHgvsNMPDotSubstExp "^" versionedRefSeqNMExp "[ :]+p?\\.?" hgvsAminoAcidSubstExp
// substring numbering:
//      0.....................................................  whole matching string
//      1...............                                        acc & optional dot version
//             2........                                        optional dot version
//                       3.....                                 optional gene sym in ()s
//                        4...                                  optional gene symbol
//                                   5.....                     original sequence
//                                           6......            1-based position
//                                                     7......  replacement sequence

#define pseudoHgvsNMPDotRangeExp "^" versionedRefSeqNMExp "[ :]+p?\\.?" hgvsAaRangeExp
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
#define pseudoHgvsGeneSymbolProtSubstExp "^" geneSymbolExp "[ :]+p?\\.?" hgvsAminoAcidSubstExp
//      0.....................................................  whole matching string
//      1...................                                    gene symbol
//                                   2.....                     original sequence
//                                           3......            1-based position
//                                                     4......  replacement sequence

#define pseudoHgvsGeneSymbolProtRangeExp "^" geneSymbolExp "[ :]+p?\\.?" hgvsAaRangeExp
//      0.....................................................  whole matching string
//      1...................                                    gene symbol
//                                 2...                         original start AA
//                                       3...                   1-based start position
//                                           4................  optional range sep and AA+pos
//                                             5...             original end AA
//                                                  6...         1-based end position
//                                                       7.....  change description

// As above but omitting the protein change
#define pseudoHgvsGeneSymbolProtPosExp "^" geneSymbolExp "[ :]+p?\\.?" posIntExp
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
regmatch_t substrs[17];
if (regexMatchSubstr(term, hgvsRefSeqNCGDotExp, substrs, ArraySize(substrs)))
    {
    int accIx = 1;
    int startPosIx = 6;
    int endPosIx = 8;
    int changeIx = 9;
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
// The LRG accession regex has only one substring but the RefSeq acc regex has 4, so that
// affects all substring offsets after the accession.
int refSeqExtra = 3;
int geneSymbolIx = -1;
regmatch_t substrs[17];
if (regexMatchSubstr(term, hgvsLrgCDotExp, substrs, ArraySize(substrs)) ||
    (isNoncoding = regexMatchSubstr(term, hgvsLrgNDotExp, substrs, ArraySize(substrs))))
    {
    matches = TRUE;
    }
else if (regexMatchSubstr(term, hgvsRefSeqNMCDotExp, substrs, ArraySize(substrs)) ||
         (isNoncoding = regexMatchSubstr(term, hgvsRefSeqNMRNDotExp, substrs, ArraySize(substrs))))
    {
    matches = TRUE;
    geneSymbolIx = 4;
    startAnchorIx += refSeqExtra;
    endAnchorIx += refSeqExtra;
    endPosIx += refSeqExtra;
    changeIx += refSeqExtra;
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
regmatch_t substrs[8];
if (regexMatchSubstr(term, hgvsRefSeqNPPDotSubstExp, substrs, ArraySize(substrs)))
    {
    int accIx = 1;
    int geneSymbolIx = 4;
    int refIx = 5;
    int posIx = 6;
    int altIx = 7;
    AllocVar(hgvs);
    hgvs->type = hgvstProtein;
    hgvs->seqAcc = regexSubstringClone(term, substrs[accIx]);
    int changeStart = substrs[refIx].rm_so;
    int changeEnd = substrs[altIx].rm_eo;
    int len = changeEnd - changeStart;
    char change[len+1];
    safencpy(change, sizeof(change), term+changeStart, len);
    hgvs->changes = cloneString(change);
    if (regexSubstrMatched(substrs[geneSymbolIx]))
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
regmatch_t substrs[11];
if (regexMatchSubstr(term, hgvsRefSeqNPPDotRangeExp, substrs, ArraySize(substrs)))
    {
    int accIx = 1;
    int geneSymbolIx = 4;
    int startRefIx = 5;
    int startPosIx = 6;
    int endPosIx = 9;
    int changeIx = 10;
    AllocVar(hgvs);
    hgvs->type = hgvstProtein;
    hgvs->seqAcc = regexSubstringClone(term, substrs[accIx]);
    int changeStart = substrs[startRefIx].rm_so;
    int changeEnd = substrs[changeIx].rm_eo;
    int len = changeEnd - changeStart;
    char change[len+1];
    safencpy(change, sizeof(change), term+changeStart, len);
    hgvs->changes = cloneString(change);
    if (regexSubstrMatched(substrs[geneSymbolIx]))
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

static boolean dbHasNcbiRefSeq(char *db)
/* Test whether NCBI's RefSeq alignments are available in db. */
{
// hTableExists() caches results so this shouldn't make for loads of new SQL queries if called
// more than once.
return (hTableExists(db, "ncbiRefSeq") && hTableExists(db, "ncbiRefSeqPsl") &&
        hTableExists(db, "ncbiRefSeqCds") && hTableExists(db, "ncbiRefSeqLink") &&
        hTableExists(db, "ncbiRefSeqPepTable") &&
        hTableExists(db, "seqNcbiRefSeq") && hTableExists(db, "extNcbiRefSeq"));
}

static char *npForGeneSymbol(char *db, char *geneSymbol)
/* Given a gene symbol, look up and return its NP_ accession; if not found return NULL. */
{
char query[2048];
if (dbHasNcbiRefSeq(db))
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
if (dbHasNcbiRefSeq(db))
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
if (dbHasNcbiRefSeq(db))
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

static char *getProteinSeq(char *db, char *acc)
/* Return amino acid sequence for acc, or NULL if not found. */
{
if (trackHubDatabase(db))
    return NULL;
char *seq = NULL;
if (startsWith("LRG_", acc))
    {
    if (hTableExists(db, "lrgPep"))
        {
        char query[2048];
        sqlSafef(query, sizeof(query), "select seq from lrgPep where name = '%s'", acc);
        struct sqlConnection *conn = hAllocConn(db);
        seq = sqlQuickString(conn, query);
        hFreeConn(&conn);
        }
    }
else
    {
    if (dbHasNcbiRefSeq(db))
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
return hgvs;
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
/* Return TRUE if hgvs->seqAcc can be mapped to a chrom in db and coords are legal for the chrom.
 * If retFoundAcc is non-NULL, set it to the versionless seqAcc if chrom is found, else NULL.
 * If retFoundVersion is non-NULL, set it to seqAcc's version if chrom is found, else NULL.
 * If retDiffRefAllele is non-NULL and hgvs specifies a reference allele then compare it to
 * the genomic sequence at that location; set *retDiffRefAllele to NULL if they match, or to
 * genomic sequence if they differ. */
{
boolean coordsOK = FALSE;
if (retDiffRefAllele)
    *retDiffRefAllele = NULL;
if (retFoundAcc)
    *retFoundAcc = NULL;
if (retFoundVersion)
    *retFoundVersion = 0;
char *chrom = hgOfficialChromName(db, hgvs->seqAcc);
if (isNotEmpty(chrom))
    {
    struct chromInfo *ci = hGetChromInfo(db, chrom);
    int start, end;
    hgvsStartEndToZeroBasedHalfOpen(hgvs, &start, &end);
    if (start >= 0 && start < ci->size && end > 0 && end < ci->size)
        {
        coordsOK = TRUE;
        if (retDiffRefAllele)
            {
            char hgvsBase = refBaseFromNucSubst(hgvs->changes);
            if (hgvsBase != '\0')
                {
                struct dnaSeq *refBase = hFetchSeq(ci->fileName, chrom, start, end);
                touppers(refBase->dna);
                if (refBase->dna[0] != hgvsBase)
                    *retDiffRefAllele = cloneString(refBase->dna);
                dnaSeqFree(&refBase);
                }
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
return coordsOK;
}

static char *dnaSeqCannibalize(struct dnaSeq **pDnaSeq)
/* Return the already-allocated dna string and free the dnaSeq container. */
{
char *seq = NULL;
if (pDnaSeq && *pDnaSeq)
    {
    struct dnaSeq *dnaSeq = *pDnaSeq;
    seq = dnaSeq->dna;
    dnaSeq->dna = NULL;
    dnaSeqFree(pDnaSeq);
    }
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
else
    {
    struct dnaSeq *cdnaSeq = NULL;
    if (dbHasNcbiRefSeq(db))
        cdnaSeq = hDnaSeqGet(db, acc, "seqNcbiRefSeq", "extNcbiRefSeq");
    else
        cdnaSeq = hGenBankGetMrna(db, acc, NULL);
    if (cdnaSeq)
        seq = dnaSeqCannibalize(&cdnaSeq);
    }
return seq;
}

static boolean getCds(char *db, char *acc, struct genbankCds *retCds)
/* Get the CDS info for genbank or LRG acc; return FALSE if not found or not applicable. */
{
if (trackHubDatabase(db))
    return FALSE;
boolean gotCds = FALSE;
char query[1024];
if (startsWith("LRG_", acc))
    sqlSafef(query, sizeof(query),
             "select cds from lrgCds where id = '%s'", acc);
else if (dbHasNcbiRefSeq(db) &&
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
char *cdsStr = sqlQuickQuery(conn, query, cdsBuf, sizeof(cdsBuf));
hFreeConn(&conn);
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
else if (dbHasNcbiRefSeq(db))
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
        if (coordsOK && retDiffRefAllele)
            {
            start += (hgvs->startIsUtr3 ? cds.end : cds.start);
            if (hgvs->startOffset == 0 && start >= 0 && start < seqLen)
                checkRefAllele(hgvs, start, accSeq, retDiffRefAllele);
            }
        }
    else
        {
        coordsOK = TRUE;
        if (retDiffRefAllele && hgvs->startOffset == 0 && start >= 0 && start < seqLen)
            checkRefAllele(hgvs, start, accSeq, retDiffRefAllele);
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
/* Given an NC_*g. term, if we can map NC_ to chrom then return the region, else NULL. */
{
struct bed *region = NULL;
char *chrom = hgOfficialChromName(db, hgvs->seqAcc);
if (isNotEmpty(chrom) && hgvs->start1 > 0)
    {
    region = bed6New(chrom, hgvs->start1 - 1, hgvs->end, "", 0, '+');
    adjustInsStartEnd(hgvs, &region->chromStart, &region->chromEnd);
    }
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


static struct psl *mapPsl(char *db, struct hgvsVariant *hgvs, char *pslTable, char *acc,
                          struct genbankCds *cds, int *retUpstream, int *retDownstream)
/* If acc is found in pslTable, use pslTransMap to map hgvs onto the genome. */
{
struct psl *mappedToGenome = NULL;
char query[2048];
sqlSafef(query, sizeof(query), "select * from %s where qName = '%s'", pslTable, acc);
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
if (sr && (row = sqlNextRow(sr)))
    {
    int bin = 1; // All PSL tables used here, and any made in the future, use the bin column.
    struct psl *txAli = pslLoad(row+bin);
    // variantPsl contains the anchor if a non-cdsStart anchor is used because
    // the actual position might be outside the bounds of the transcript sequence (intron/UTR)
    struct psl *variantPsl = pslFromHgvsNuc(hgvs, acc, txAli->qSize, txAli->qEnd, cds,
                                            retUpstream, retDownstream);
    mappedToGenome = pslTransMap(pslTransMapNoOpts, variantPsl, txAli);
    // If there is a deletion in the genome / insertion in the transcript then pslTransMap cannot
    // map those bases to the genome.  In that case take a harder look and return the deletion
    // coords.
    if (mappedToGenome == NULL)
        mappedToGenome = mapToDeletion(variantPsl, txAli);
    pslFree(&variantPsl);
    pslFree(&txAli);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return mappedToGenome;
}

static char *pslTableForAcc(char *db, char *acc)
/* Based on acc (and whether db has NCBI RefSeq alignments), pick a PSL table where
 * acc should be found (table may or may not exist).  Don't free the returned string. */
{
char *pslTable = NULL;
if (startsWith("LRG_", acc))
    pslTable = "lrgTranscriptAli";
else if (startsWith("NM_", acc) || startsWith("NR_", acc))
    {
    // Use NCBI's alignments if they are available
    if (dbHasNcbiRefSeq(db))
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

static struct bed *hgvsMapNucToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Return a bed6 with the variant's span on the genome and strand, or NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */
{
char *acc = normalizeVersion(db, hgvs->seqAcc, NULL);
if (isEmpty(acc))
    return NULL;
if (hgvs->type == hgvstGenomic)
    return hgvsMapGDotToGenome(db, hgvs, retPslTable);
struct bed *region = NULL;
char *pslTable = pslTableForAcc(db, acc);
struct genbankCds cds;
boolean gotCds = (hgvs->type == hgvstCoding) ? getCds(db, acc, &cds) : FALSE;
if (pslTable && (hgvs->type != hgvstCoding || gotCds) && hTableExists(db, pslTable))
    {
    int upstream = 0, downstream = 0;
    struct psl *mappedToGenome = mapPsl(db, hgvs, pslTable, acc, &cds, &upstream, &downstream);
    // As of 9/26/16, ncbiRefSeqPsl is missing some items (#13673#note-443) -- so fall back
    // on UCSC alignments.
    if (!mappedToGenome && sameString(pslTable, "ncbiRefSeqPsl") && hTableExists(db, "refSeqAli"))
        {
        char *accNoVersion = cloneFirstWordByDelimiter(acc, '.');
        gotCds = (hgvs->type == hgvstCoding) ? getCds(db, accNoVersion, &cds) : FALSE;
        if (hgvs->type != hgvstCoding || gotCds)
            mappedToGenome = mapPsl(db, hgvs, "refSeqAli", accNoVersion, &cds,
                                    &upstream, &downstream);
        if (mappedToGenome)
            {
            pslTable = "refSeqAli";
            acc = accNoVersion;
            }
        }
    if (mappedToGenome)
        {
        region = pslAndFriendsToRegion(mappedToGenome, hgvs, upstream, downstream);
        pslFree(&mappedToGenome);
        }
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
struct bed *region = NULL;
char *acc = normalizeVersion(db, hgvs->seqAcc, NULL);
if (acc && startsWith("NP_", acc))
    {
    // Translate the NP_*:p. to NM_*:c. and map NM_*:c. to the genome.
    struct sqlConnection *conn = hAllocConn(db);
    char query[2048];
    if (dbHasNcbiRefSeq(db))
        sqlSafef(query, sizeof(query), "select mrnaAcc from ncbiRefSeqLink where protAcc = '%s'",
                 acc);
    else if (hTableExists(db, "refGene"))
        sqlSafef(query, sizeof(query), "select mrnaAcc from %s l, refGene r "
                 "where l.protAcc = '%s' and r.name = l.mrnaAcc", refLinkTable, acc);
    else
        return NULL;
    char *nmAcc = sqlQuickString(conn, query);
    hFreeConn(&conn);
    if (nmAcc)
        {
        struct hgvsVariant cDot;
        zeroBytes(&cDot, sizeof(cDot));
        cDot.type = hgvstCoding;
        cDot.seqAcc = nmAcc;
        cDot.start1 = ((hgvs->start1 - 1) * 3) + 1;
        cDot.end = ((hgvs->end - 1) * 3) + 3;
        cDot.changes = hgvs->changes;
        region = hgvsMapNucToGenome(db, &cDot, retPslTable);
        freeMem(nmAcc);
        }
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
    if (getCds(db, hgvs->seqAcc, &cds))
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

static void updateSeq(struct dnaSeq *seq, char *db, char *chrom, int start, int end)
/* Free and reallocate seq's contents with the new sequence range. */
// BEWARE: newDnaSeq does not clone dna so original seq->dna was allocated elsewhere!
// In this case I'm assuming that seq comes from a prior call to hChromSeq, which
// passes control of seq over to the caller (there's no stale pointer to cause trouble).
{
struct dnaSeq *newSeq = hChromSeq(db, chrom, start, end);
touppers(newSeq->dna);
freeMem(seq->dna);
freeMem(seq->name);
memcpy(seq, newSeq, sizeof(*seq));
freeMem(newSeq);
}

static boolean shiftAndFetchMoreSeq(char *db, int altLen, struct dnaSeq *genomicSeq,
                                    int *pLeftShiftOffset, struct bed *mapping,
                                    int *pTotalBasesShifted, int *pStart)
/* We need to fetch more genomic sequence to the left; update genomicSeq and remaining args
 * with the distance that we have shifted so far, and fetch a larger block of sequence.
 * Return TRUE if we have shifted all the way to the beginning of the chromosome. */
{
// Adjust mapping->chrom{Start,End} and totalBasesShifted
mapping->chromStart -= *pLeftShiftOffset;
mapping->chromEnd -= *pLeftShiftOffset;
*pTotalBasesShifted += *pLeftShiftOffset;
// Double the size of our left-shifting buffer (but don't underflow chrom)
*pLeftShiftOffset = min(mapping->chromStart, *pLeftShiftOffset * 2);
// Update genomicSeq and start offset
int genomicSeqStart = mapping->chromStart - *pLeftShiftOffset;
int genomicSeqEnd = mapping->chromEnd + altLen;
if (genomicSeqEnd > genomicSeqStart)
    updateSeq(genomicSeq, db, mapping->chrom, genomicSeqStart, genomicSeqEnd);
*pStart = *pLeftShiftOffset;
return (mapping->chromStart == 0);
}

static int leftShift(boolean isRc, char *db, struct dnaSeq *genomicSeq, int *pLeftShiftOffset,
                     char *genomicRef, char *genomicRefFwd, char *hgvsSeqRef, char *hgvsSeqAlt,
                     char pLeftBase[1], struct bed *mapping)
/* HGVS requires right-shifting ambiguous alignments while VCF requires left-shifting.
 * If it's possible to left-shift this ref/alt, then change pLeftBase and mapping.
 * Return */
{
int totalBasesShifted = 0;
if (sameString(genomicRef, hgvsSeqRef))
    {
    int refLen = mapping->chromEnd - mapping->chromStart;
    int altLen = strlen(hgvsSeqAlt);
    if ((refLen == 0 && altLen > 0) ||
        (refLen > 0 && altLen == 0))
        {
        char *genomeChunk = genomicSeq->dna;
        int start = *pLeftShiftOffset;
        boolean done = FALSE;
        // If insertion, first compare inserted seq to genomic seq to the left of insertion point.
        char hgvsSeqAltFwd[altLen+1];
        copyMaybeRc(hgvsSeqAltFwd, sizeof(hgvsSeqAltFwd), hgvsSeqAlt, isRc);
        int ix;
        for (ix = altLen-1;  !done && ix >= 0 && start > 0;  ix--)
            {
            if (genomeChunk[start-1] == hgvsSeqAltFwd[ix])
                {
                start--;
                if (start == 0)
                    {
                    // We have hit the beginning of genomeChunk and are not done; fetch more seq.
                    done = shiftAndFetchMoreSeq(db, altLen, genomicSeq, pLeftShiftOffset,
                                                mapping, &totalBasesShifted, &start);
                    genomeChunk = genomicSeq->dna;
                    }
                }
            else
                done = TRUE;
            }
        // If we're not done, keep trying to shift left.
        while (!done && start > 0)
            {
            if (genomeChunk[start-1] == genomeChunk[start-1+refLen+altLen])
                {
                start--;
                if (start == 0)
                    {
                    // We have hit the beginning of genomeChunk and are not done; fetch more seq.
                    done = shiftAndFetchMoreSeq(db, altLen, genomicSeq, pLeftShiftOffset,
                                                mapping, &totalBasesShifted, &start);
                    genomeChunk = genomicSeq->dna;
                    }
                }
            else
                done = TRUE;
            }
        // Done shifting; update mapping coords and ref/alt sequences if we shifted.
        int basesShifted = (*pLeftShiftOffset - start);
        totalBasesShifted += basesShifted;
        if (totalBasesShifted > 0)
            {
            mapping->chromStart -= basesShifted;
            mapping->chromEnd -= basesShifted;
            if (altLen > 0)
                {
                // Insertion: ref is still "", update hgvsSeqAlt.
                if (totalBasesShifted < altLen)
                    {
                    // The beginning of hgvsSeqAlt is from genomic sequence. The remainder is a
                    // shifted portion of the original.  Do the shifting first, then the copying
                    // from genome.
                    memmove(hgvsSeqAltFwd+totalBasesShifted, hgvsSeqAltFwd,
                            (altLen - totalBasesShifted));
                    memcpy(hgvsSeqAltFwd, genomeChunk+start, totalBasesShifted);
                    }
                else
                    safencpy(hgvsSeqAltFwd, sizeof(hgvsSeqAltFwd), genomeChunk+start, altLen);
                copyMaybeRc(hgvsSeqAlt, altLen+1, hgvsSeqAltFwd, isRc);
                *pLeftBase = (start > 0) ? genomeChunk[start-1] : genomeChunk[0];
                }
            if (refLen > 0)
                {
                // Deletion: update ref from genome, alt is still "".
                safencpy(genomicRefFwd, refLen+1, genomeChunk+start, refLen);
                copyMaybeRc(genomicRef, refLen+1, genomicRefFwd, isRc);
                safecpy(hgvsSeqRef, refLen+1, genomicRef);
                *pLeftBase = (start > 0) ? genomeChunk[start-1] : genomeChunk[0];
                }
            }
        }
    }
return totalBasesShifted;
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
int leftShiftOffset = min(mapping->chromStart,
                          max(128, 4*genomicRefLen));
int genomicSeqStart = mapping->chromStart - leftShiftOffset;
struct dnaSeq *genomicSeq = hChromSeq(db, mapping->chrom, genomicSeqStart, mapping->chromEnd);
touppers(genomicSeq->dna);
int indelOffset = (mapping->chromStart == 0) ? 0 : 1;
char leftBase = genomicSeq->dna[leftShiftOffset-indelOffset];
char genomicRefFwd[genomicRefLen+1];
safencpy(genomicRefFwd, sizeof(genomicRefFwd), genomicSeq->dna + leftShiftOffset, genomicRefLen);
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
// These two VCF-normalization functions may modify the contents of their arguments:
boolean dupToIns = doDupToIns(isRc, &leftShiftOffset, genomicRef, genomicRefFwd,
                              hgvsSeqRef, hgvsSeqAlt, &leftBase, mapping);
int basesShifted = 0;
if (doLeftShift)
    basesShifted = leftShift(isRc, db, genomicSeq, &leftShiftOffset, genomicRef, genomicRefFwd,
                             hgvsSeqRef, hgvsSeqAlt, &leftBase, mapping);
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
dnaSeqFree(&genomicSeq);
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
