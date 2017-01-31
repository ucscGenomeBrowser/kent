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
#define versionedRefSeqNRExp versionedAccPrefixExp("NR")

// Nucleotide position regexes
// (c. = CDS, g. = genomic, m. = mitochondrial, n.= non-coding RNA, r. = RNA)
#define posIntExp "([0-9]+)"
#define hgvsNtPosExp posIntExp "(_" posIntExp ")?"
//                   ......                     1-based start position
//                           .............      optional range separator and end position
//                               ......         1-based end position
// Now for a regex that can handle positions like "26" or "*80" or "-24+75_-24+92"...
#define anchorExp "([-*])?"
#define offsetExp "([-+])"
#define complexNumExp anchorExp posIntExp "(" offsetExp posIntExp ")?"
#define hgvsCdsPosExp complexNumExp "(_" complexNumExp ")?"
//                    ...                               optional anchor "-" or "*"
//                        ...                           mandatory first number
//                            .......                   optional offset separator and offset
//                            ...                       offset separator
//                                ...                   offset number
//                                    ...............   optional range separator and complex num
//                                    ...               optional anchor "-" or "*"
//                                        ...           first number
//                                            .......   optional offset separator and offset
//                                            ...       offset separator
//                                                ...   offset number

// It's pretty common for users to omit the '.' so if it's missing but the rest of the regex fits,
// roll with it.
#define hgvsCDotPosExp "c\\.?" hgvsCdsPosExp
#define hgvsGDotPosExp "g\\.?" hgvsNtPosExp
#define hgvsMDotPosExp "m\\.?" hgvsNtPosExp
#define hgvsNDotPosExp "n\\.?" hgvsNtPosExp
#define hgvsRDotPosExp "r\\.?" hgvsNtPosExp

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
#define hgvsFullRegex(seq, op) "^" seq ":" op

#define hgvsRefSeqNCGDotPosExp hgvsFullRegex(versionedRefSeqNCExp, hgvsGDotPosExp)
#define hgvsRefSeqNCGDotExp hgvsRefSeqNCGDotPosExp "(.*)"
// substring numbering:
//      0.....................................  whole matching string
//      1.................                      accession and optional dot version
//               2........                      optional dot version
//                       3......                (n/a) optional gene symbol in ()s
//                        4....                 (n/a) optional gene symbol
//                               5...           1-based start position
//                                   6....      optional range separator and end position
//                                    7...      1-based end position
//                                       8....  change description

#define hgvsLrgCDotPosExp hgvsFullRegex(lrgTranscriptExp, hgvsCDotPosExp)
#define hgvsLrgCDotExp hgvsLrgCDotPosExp "(.*)"
// substring numbering:
//      0.....................................................  whole matching string
//      1...................                                    LRG transcript
//                   2...                                       optional anchor "-" or "*"
//                       3...                                   mandatory first number
//                           4.......                           optional offset separator and offset
//                           5...                               offset separator
//                               6...                           offset number
//                                   7...............           optional range sep and complex num
//                                   8...                       optional anchor "-" or "*"
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
//                              5...                                      optional anchor
//                                  6...                                  mandatory first number
//                                      7.......                          optional offset
//                                      8...                              offset separator
//                                          9...                          offset number
//                                             10...............          optional range
//                                             11...                      optional anchor
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
#define pseudoHgvsNMPDotSubstExp "^" versionedRefSeqNMExp "[: p.]+" hgvsAminoAcidSubstExp
// substring numbering:
//      0.....................................................  whole matching string
//      1...............                                        acc & optional dot version
//             2........                                        optional dot version
//                       3.....                                 optional gene sym in ()s
//                        4...                                  optional gene symbol
//                                   5.....                     original sequence
//                                           6......            1-based position
//                                                     7......  replacement sequence

#define pseudoHgvsNMPDotRangeExp "^" versionedRefSeqNMExp "[: p.]+" hgvsAaRangeExp
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
#define pseudoHgvsGeneSymbolProtSubstExp "^" geneSymbolExp "[: p.]+" hgvsAminoAcidSubstExp
//      0.....................................................  whole matching string
//      1...................                                    gene symbol
//                                   2.....                     original sequence
//                                           3......            1-based position
//                                                     4......  replacement sequence

#define pseudoHgvsGeneSymbolProtRangeExp "^" geneSymbolExp "[: p.]+" hgvsAaRangeExp
//      0.....................................................  whole matching string
//      1...................                                    gene symbol
//                                 2...                         original start AA
//                                       3...                   1-based start position
//                                           4................  optional range sep and AA+pos
//                                             5...             original end AA
//                                                  6...         1-based end position
//                                                       7.....  change description

// As above but omitting the protein change
#define pseudoHgvsGeneSymbolProtPosExp "^" geneSymbolExp "[: p.]+" posIntExp
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
    int startPosIx = 5;
    int endPosIx = 7;
    int changeIx = 8;
    AllocVar(hgvs);
    hgvs->type = hgvstGenomic;
    hgvs->seqAcc = regexSubstringClone(term, substrs[accIx]);
    hgvs->start1 = regexSubstringInt(term, substrs[startPosIx]);
    if (regexSubstrMatched(substrs[endPosIx]))
        hgvs->end = regexSubstringInt(term, substrs[endPosIx]);
    else
        hgvs->end = hgvs->start1;
    hgvs->change = regexSubstringClone(term, substrs[changeIx]);
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

static struct hgvsVariant *hgvsParseCDotPos(char *term)
/* If term is parseable as an HGVS CDS term, return the parsed representation, otherwise NULL. */
{
struct hgvsVariant *hgvs = NULL;
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
if (regexMatchSubstr(term, hgvsLrgCDotExp, substrs, ArraySize(substrs)))
    {
    matches = TRUE;
    }
else if (regexMatchSubstr(term, hgvsRefSeqNMCDotExp, substrs, ArraySize(substrs)))
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
    hgvs->type = hgvstCoding;
    hgvs->seqAcc = regexSubstringClone(term, substrs[accIx]);
    extractComplexNum(term, substrs, startAnchorIx,
                      &hgvs->startIsUtr3, &hgvs->start1, &hgvs->startOffset);
    if (geneSymbolIx >= 0 && regexSubstrMatched(substrs[geneSymbolIx]))
        hgvs->seqGeneSymbol = regexSubstringClone(term, substrs[geneSymbolIx]);
    if (regexSubstrMatched(substrs[endPosIx]))
        {
        extractComplexNum(term, substrs, endAnchorIx,
                          &hgvs->endIsUtr3, &hgvs->end, &hgvs->endOffset);
        }
    else
        {
        hgvs->end = hgvs->start1;
        hgvs->endIsUtr3 = hgvs->startIsUtr3;
        hgvs->endOffset = hgvs->startOffset;
        }
    hgvs->change = regexSubstringClone(term, substrs[changeIx]);
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
    hgvs->change = cloneString(change);
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
    hgvs->change = cloneString(change);
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
struct hgvsVariant *hgvs = hgvsParseCDotPos(term);
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
        aaSeq *aaSeq = hGenBankGetPep(db, acc, gbSeqTable);
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
    if ((hgvs->start1 >= 1 && hgvs->start1 <= ci->size) &&
        (hgvs->end >=1 && hgvs->end <= ci->size))
        {
        coordsOK = TRUE;
        if (retDiffRefAllele)
            {
            char hgvsBase = refBaseFromNucSubst(hgvs->change);
            if (hgvsBase != '\0')
                {
                struct dnaSeq *refBase = hFetchSeq(ci->fileName, chrom,
                                                   hgvs->start1-1, hgvs->start1);
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
        cdnaSeq = hGenBankGetMrna(db, acc, gbSeqTable);
    if (cdnaSeq)
        seq = cdnaSeq->dna;
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

static void checkRefAllele(struct hgvsVariant *hgvs, int start1, char *accSeq,
                           char **retDiffRefAllele)
/* If hgvs change includes a reference allele, and if accSeq at start1 does not match,
 *  then set retDiffRefAllele to the accSeq version.  retDiffRefAllele must be non-NULL. */
{
char hgvsRefBase = (hgvs->type == hgvstProtein) ? refBaseFromProt(hgvs->change) :
                                                  refBaseFromNucSubst(hgvs->change);
if (hgvsRefBase != '\0')
    {
    char seqRefBase = toupper(accSeq[start1-1]);
    if (seqRefBase != hgvsRefBase)
        *retDiffRefAllele = cloneStringFromChar(seqRefBase);
    }
if (hgvs->type == hgvstProtein && *retDiffRefAllele == NULL)
    {
    // Protein ranges have a second ref allele base to check
    char *p = strchr(hgvs->change, '_');
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
 * Note: Coding terms may contain coords outside the bounds (upstream, intron, downstream) so
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
    if (hgvs->type == hgvstCoding)
        {
        // Coding term coords can extend beyond the bounds of the transcript so
        // we can't check them without mapping to the genome.  However, if the coords
        // are in bounds and a reference allele is provided, we can check that.
        struct genbankCds cds;
        coordsOK = getCds(db, acc, &cds);
        if (coordsOK && retDiffRefAllele)
            {
            int start = hgvs->start1 + (hgvs->startIsUtr3 ? cds.end : cds.start);
            if (hgvs->startOffset == 0 && start >= 1 && start <= seqLen)
                checkRefAllele(hgvs, start, accSeq, retDiffRefAllele);
            }
        }
    else
        {
        if (hgvs->start1 >= 1 && hgvs->start1 <= seqLen &&
            hgvs->end >= 1 && hgvs->end <= seqLen)
            {
            coordsOK = TRUE;
            if (retDiffRefAllele)
                checkRefAllele(hgvs, hgvs->start1, accSeq, retDiffRefAllele);
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
 * Note: Coding terms may contain coords outside the bounds (upstream, intron, downstream) so
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

static struct bed3 *hgvsMapGDotToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Given an NC_*g. term, if we can map NC_ to chrom then return the region, else NULL. */
{
struct bed3 *region = NULL;
char *chrom = hgOfficialChromName(db, hgvs->seqAcc);
if (isNotEmpty(chrom))
    {
    AllocVar(region);
    region->chrom = chrom;
    region->chromStart = hgvs->start1 - 1;
    region->chromEnd = hgvs->end;
    }
if (retPslTable)
    *retPslTable = NULL;
return region;
}

static void hgvsCodingToZeroBasedHalfOpen(struct hgvsVariant *hgvs,
                                          int maxCoord, struct genbankCds *cds,
                                          int *retStart, int *retEnd,
                                          int *retUpstreamBases, int *retDownstreamBases)
/* Convert a coding HGVS's start1 and end into UCSC coords plus upstream and downstream lengths
 * for when the coding HGVS has coordinates that extend beyond its sequence boundaries.
 * ret* args must be non-NULL. */
{
// If hgvs->start1 is negative, it is effectively 0-based, so subtract 1 only if positive.
int start = hgvs->start1;
if (start > 0)
    start -= 1;
// If hgvs->end is negative, it is effectively 0-based, so add 1 to get half-open end.
int end = hgvs->end;
if (end < 0)
    end += 1;
// If the position follows '*' that means it's relative to cdsEnd; otherwise rel to cdsStart
if (hgvs->startIsUtr3)
    *retStart = cds->end + start;
else
    *retStart = cds->start + start;
if (hgvs->endIsUtr3)
    *retEnd = cds->end + end;
else
    *retEnd = cds->start + end;
// Now check for coords that extend beyond the transcript('s alignment to the genome)
if (*retStart < 0)
    {
    // hgvs->start1 is upstream of coding transcript.
    *retUpstreamBases = -*retStart;
    *retStart = 0;
    }
else if (*retStart > maxCoord)
    {
    // Even the start coord is downstream of coding transcript -- make a negative "upstream"
    // for adjusting start.
    *retUpstreamBases = -(*retStart - maxCoord + 1);
    *retStart = maxCoord - 1;
    }
else
    *retUpstreamBases = 0;
if (*retEnd > maxCoord)
    {
    // hgvs->end is downstream of coding transcript.
    *retDownstreamBases = *retEnd - maxCoord;
    *retEnd = maxCoord;
    }
else if (*retEnd < 0)
    {
    // Even the end coord is upstream of coding transcript -- make a negative "downstream"
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
 * NOTE: coding intron offsets are ignored; the PSL contains the exon anchor point
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
if (hgvs->type != hgvstCoding)
    {
    // Sane 1-based fully closed coords.
    psl->tStart = hgvs->start1 - 1;
    psl->tEnd = hgvs->end;
    }
else
    {
    // Simple or insanely complex CDS-relative coords.
    hgvsCodingToZeroBasedHalfOpen(hgvs, accEnd, cds, &psl->tStart, &psl->tEnd,
                                  retUpstreamBases, retDownstreamBases);
    }
int refLen = psl->tEnd - psl->tStart;
// Just use refLen for alt until we parse the sequence change portion of the term:
int altLen = refLen;
psl->qName = cloneString(hgvs->change);
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
else if (startsWith("NM_", acc))
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

static struct bed3 *pslAndFriendsToRegion(struct psl *psl, struct hgvsVariant *hgvs,
                                          int upstream, int downstream)
/* If hgvs has any intron offsets and/or upstream/downstream offsets, add those to the
 * anchor coords in psl and return the variant's region of the genome. */
{
struct bed3 *region = bed3New(psl->tName, psl->tStart, psl->tEnd);
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

static struct bed3 *hgvsMapNucToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Return a bed3 with the variant's span on the genome, or NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */
{
char *acc = normalizeVersion(db, hgvs->seqAcc, NULL);
if (isEmpty(acc))
    return NULL;
if (hgvs->type == hgvstGenomic)
    return hgvsMapGDotToGenome(db, hgvs, retPslTable);
struct bed3 *region = NULL;
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
if (region && retPslTable)
    *retPslTable = cloneString(pslTable);
return region;
}

static struct bed3 *hgvsMapPDotToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Return a bed3 with the variant's span on the genome, or NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */
{
struct bed3 *region = NULL;
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
        cDot.change = hgvs->change;
        region = hgvsMapNucToGenome(db, &cDot, retPslTable);
        freeMem(nmAcc);
        }
    }
return region;
}

struct bed3 *hgvsMapToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Return a bed3 with the variant's span on the genome, or NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */
{
if (hgvs == NULL)
    return NULL;
struct bed3 *region = NULL;
if (hgvs->type == hgvstProtein)
    region = hgvsMapPDotToGenome(db, hgvs, retPslTable);
else
    region = hgvsMapNucToGenome(db, hgvs, retPslTable);
return region;
}
