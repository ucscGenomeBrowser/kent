/* hgHgvs.c - Parse the Human Genome Variation Society (HGVS) nomenclature for variants. */
/* See http://varnomen.hgvs.org/ and https://github.com/mutalyzer/mutalyzer/ */

/* Copyright (C) 2016 The Regents of the University of California
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hgHgvs.h"
#include "genbank.h"
#include "hdb.h"
#include "pslTransMap.h"
#include "regexHelper.h"

// Regular expressions for HGVS-recognized sequence accessions: LRG or versioned RefSeq:
#define lrgTranscriptExp "(LRG_[0-9]+t[0-9+])"
#define lrgRegionExp "(LRG_[0-9+])"

// NC = RefSeq reference assembly chromosome
// NG = RefSeq incomplete genomic region (e.g. gene locus)
// NM = RefSeq curated mRNA
// NP = RefSeq curated protein
// NR = RefSeq curated (non-coding) RNA
#define geneSymbolExp "([A-Z0-9./_-]+)"
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

// Nucleotide substitution regexes
// (c. = CDS, g. = genomic, m. = mitochondrial, n.= non-coding RNA, r. = RNA)
#define hgvsNtSubstExp "([0-9]+)([ACGT]+)>([ACGT]+)"
//                       ......                     1-based position
//                               .......            original sequence
//                                         .......  replacement sequence
#define hgvsCDotSubstExp "c\\." hgvsNtSubstExp
#define hgvsGDotSubstExp "g\\." hgvsNtSubstExp
#define hgvsMDotSubstExp "m\\." hgvsNtSubstExp
#define hgvsNDotSubstExp "n\\." hgvsNtSubstExp
#define hgvsRDotSubstExp "r\\." hgvsNtSubstExp

// Protein substitution regex
#define AA3 "Ala|Arg|Asn|Asp|Cys|Gln|Glu|Gly|His|Ile|Leu|Lys|Met|Phe|Pro|Ser|Thr|Trp|Tyr|Val|Ter"
#define hgvsAminoAcidExp "([ARNDCQEGHILKMFPSTWYVX*]|" AA3 ")"
#define hgvsAminoAcidSubstExp hgvsAminoAcidExp "([0-9]+)" hgvsAminoAcidExp
#define hgvsPDotSubstExp "p\\." hgvsAminoAcidSubstExp
//                                 ...                                  // original sequence
//                                              ......                  // 1-based position
//                                                           ...        // replacement sequence


// Complete HGVS term regexes combining sequence identifiers and change operations
#define hgvsFullRegex(seq, op) "^" seq ":" op "$"

#define hgvsLrgCDotSubstExp hgvsFullRegex(lrgTranscriptExp, hgvsCDotSubstExp)
// substring numbering:
//      0.....................................................  whole matching string
//      1...................                                    LRG transcript
//                                   2.....                     1-based position
//                                           3......            original sequence
//                                                     4......  replacement sequence

#define hgvsRefSeqNMCDotSubstExp hgvsFullRegex(versionedRefSeqNMExp, hgvsCDotSubstExp)
// substring numbering:
//      0.....................................................  whole matching string
//      1...............                                        accession and optional dot version
//             2........                                        optional dot version
//                         3.....                               optional gene symbol in ()s
//                          4...                                optional gene symbol
//                                   5.....                     1-based position
//                                           6......            original sequence
//                                                     7......  replacement sequence

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

// Pseudo-HGVS in common usage
#define pseudoHgvsGeneSymbolSubstExp "^" geneSymbolExp "[: ]" hgvsAminoAcidSubstExp
//      0.....................................................  whole matching string
//      1...................                                    gene symbol
//                                   2.....                     original sequence
//                                           3......            1-based position
//                                                     4......  replacement sequence

static struct hgvsVariant *hgvsParseCDotSubst(char *term)
/* If term is parseable as an HGVS CDS substitution, return the parsed representation,
 * otherwise NULL. */
{
struct hgvsVariant *hgvs = NULL;
boolean matches = FALSE;
int accIx = 1;
int posIx = 0;
int refIx = 0;
int altIx = 0;
int geneSymbolIx = -1;
regmatch_t substrs[8];
if (regexMatchSubstrNoCase(term, hgvsLrgCDotSubstExp, substrs, ArraySize(substrs)))
    {
    matches = TRUE;
    posIx = 2;
    refIx = 3;
    altIx = 4;
    }
else if (regexMatchSubstrNoCase(term, hgvsRefSeqNMCDotSubstExp, substrs, ArraySize(substrs)))
    {
    matches = TRUE;
    geneSymbolIx = 4;
    posIx = 5;
    refIx = 6;
    altIx = 7;
    }
if (matches)
    {
    AllocVar(hgvs);
    hgvs->type = hgvstCoding;
    hgvs->changeSymbol = cloneString(">");
    hgvs->seqAcc = regexSubstringClone(term, substrs[accIx]);
    hgvs->start1 = regexSubstringInt(term, substrs[posIx]);
    hgvs->refAllele = regexSubstringClone(term, substrs[refIx]);
    hgvs->altAllele = regexSubstringClone(term, substrs[altIx]);
    if (geneSymbolIx >= 0 && regexSubstrMatched(substrs[geneSymbolIx]))
        hgvs->seqGeneSymbol = regexSubstringClone(term, substrs[geneSymbolIx]);
    hgvs->end = hgvs->start1 - 1 + strlen(hgvs->refAllele);
    }
return hgvs;
}

static char *aaAbbrsToLetters(char *aaStr)
/* If the input string is composed of AA abbreviations like "Ala", "Asp" etc., convert them to
 * single letter AAs like "A", "D" etc.  Otherwise return a copy of aaStr. */
{
char *letters = NULL;
if (regexMatch(aaStr, "^(" AA3 ")+$"))
    {
    int len = strlen(aaStr) / 3;
    letters = needMem(len + 1);
    int i;
    for (i = 0;  i < len;  i++)
        letters[i] = aaAbbrToLetter(&aaStr[i*3]);
    }
else
    letters = cloneString(aaStr);
return letters;
}

static struct hgvsVariant *hgvsParsePDotSubst(char *term)
/* If term is parseable as an HGVS protein substitution, return the parsed representation,
 * otherwise NULL. */
{
struct hgvsVariant *hgvs = NULL;
regmatch_t substrs[8];
if (regexMatchSubstrNoCase(term, hgvsRefSeqNPPDotSubstExp, substrs, ArraySize(substrs)))
    {
    int accIx = 1;
    int geneSymbolIx = 4;
    int refIx = 5;
    int posIx = 6;
    int altIx = 7;
    AllocVar(hgvs);
    hgvs->type = hgvstProtein;
    hgvs->seqAcc = regexSubstringClone(term, substrs[accIx]);
    if (regexSubstrMatched(substrs[geneSymbolIx]))
        hgvs->seqGeneSymbol = regexSubstringClone(term, substrs[geneSymbolIx]);
    hgvs->start1 = regexSubstringInt(term, substrs[posIx]);
    // Convert 3-letter abbreviations to single-letter codes like "Ala" -> "A",
    // so strlen(allele) is the number of amino acids.
    char buf[2048];
    regexSubstringCopy(term, substrs[refIx], buf, sizeof(buf));
    hgvs->refAllele = aaAbbrsToLetters(buf);
    regexSubstringCopy(term, substrs[altIx], buf, sizeof(buf));
    hgvs->altAllele = aaAbbrsToLetters(buf);
    hgvs->end = hgvs->start1 - 1 + strlen(hgvs->refAllele);
    }
return hgvs;
}

struct hgvsVariant *hgvsParseTerm(char *term)
/* If term is a parseable form of HGVS, return the parsed representation, otherwise NULL.
 * This does not check validity of accessions or alleles. */
{
struct hgvsVariant *hgvs = hgvsParseCDotSubst(term);
if (hgvs == NULL)
    hgvs = hgvsParsePDotSubst(term);
return hgvs;
}

struct hgvsVariant *hgvsParsePseudoHgvs(char *db, char *term)
/* Attempt to parse things that are not strict HGVS, but that people might intend as HGVS: */
{
struct hgvsVariant *hgvs = NULL;
regmatch_t substrs[5];
if (regexMatchSubstrNoCase(term, pseudoHgvsGeneSymbolSubstExp, substrs, ArraySize(substrs)))
    {
    int geneSymbolIx = 1;
    int refIx = 2;
    int posIx = 3;
    int altIx = 4;
    char *geneSymbol = regexSubstringClone(term, substrs[geneSymbolIx]);
    // Try to find an NP_ for geneSymbol, using refGene to make sure it's an NP for this species.
    struct sqlConnection *conn = hAllocConn(db);
    char query[2048];
    sqlSafef(query, sizeof(query), "select l.protAcc from %s l, refGene r "
             "where l.name = '%s' and l.mrnaAcc = r.name "
             "and l.protAcc != '' order by l.protAcc"
             , refLinkTable, geneSymbol);
    char *npAcc = sqlQuickString(conn, query);
    hFreeConn(&conn);
    if (isNotEmpty(npAcc))
        {
        AllocVar(hgvs);
        hgvs->type = hgvstProtein;
        hgvs->seqAcc = npAcc;
        hgvs->seqGeneSymbol = geneSymbol;
        hgvs->start1 = regexSubstringInt(term, substrs[posIx]);
        // Convert 3-letter abbreviations to single-letter codes like "Ala" -> "A",
        // so strlen(allele) is the number of amino acids.
        char buf[2048];
        regexSubstringCopy(term, substrs[refIx], buf, sizeof(buf));
        hgvs->refAllele = aaAbbrsToLetters(buf);
        regexSubstringCopy(term, substrs[altIx], buf, sizeof(buf));
        hgvs->altAllele = aaAbbrsToLetters(buf);
        int refLen = strlen(hgvs->refAllele);
        hgvs->end = hgvs->start1 - 1 + refLen;
        }
    // Note: this doesn't support non-coding gene symbol substs (which should have nt alleles)
    }
return hgvs;
}

static char *getCdnaSeq(char *db, char *acc,  char **retFoundAcc)
/* Return cdna sequence for acc, or NULL if not found.  If retFoundAcc is not NULL,
 * set it to the accession for which we grabbed seq (might have a .version chopped off). */
{
char *seq = NULL;
char *foundAcc = NULL;
if (startsWith("LRG_", acc))
    {
    if (hTableExists(db, "lrgCdna"))
        {
        char query[2048];
        sqlSafef(query, sizeof(query), "select seq from lrgCdna where name = '%s'", acc);
        struct sqlConnection *conn = hAllocConn(db);
        seq = sqlQuickString(conn, query);
        hFreeConn(&conn);
        if (seq)
            foundAcc = cloneString(acc);
        }
    }
else
    {
    char *trimmedAcc = cloneFirstWordByDelimiter(acc, '.');
    struct dnaSeq *gbSeq = hGenBankGetMrna(db, trimmedAcc, gbSeqTable);
    if (gbSeq)
        {
        seq = gbSeq->dna;
        foundAcc = trimmedAcc;
        }
    }
if (retFoundAcc)
    *retFoundAcc = foundAcc;
return seq;
}

static char *getProteinSeq(char *db, char *acc,  char **retFoundAcc)
/* Return amino acid sequence for acc, or NULL if not found.  If retFoundAcc is not NULL,
 * set it to the accession for which we grabbed seq (might have a .version chopped off). */
{
char *seq = NULL;
char *foundAcc = NULL;
if (startsWith("LRG_", acc))
    {
    if (hTableExists(db, "lrgPep"))
        {
        char query[2048];
        sqlSafef(query, sizeof(query), "select seq from lrgPep where name = '%s'", acc);
        struct sqlConnection *conn = hAllocConn(db);
        seq = sqlQuickString(conn, query);
        hFreeConn(&conn);
        if (seq)
            foundAcc = cloneString(acc);
        }
    }
else
    {
    char *trimmedAcc = cloneFirstWordByDelimiter(acc, '.');
    aaSeq *aaSeq = hGenBankGetPep(db, trimmedAcc, gbSeqTable);
    if (aaSeq)
        {
        seq = aaSeq->dna;
        foundAcc = trimmedAcc;
        }
    }
if (retFoundAcc)
    *retFoundAcc = foundAcc;
return seq;
}

static int getCdsStart(char *db, char *acc)
/* Return the start coordinate for CDS in genbank or LRG acc, or -1 if not found. */
{
int cdsStart = -1;
char query[1024];
if (startsWith("LRG_", acc))
    sqlSafef(query, sizeof(query),
             "select cds from lrgCds where id = '%s'", acc);
else
    sqlSafef(query, sizeof(query),
             "SELECT c.name FROM %s as c, %s as g WHERE (g.acc = '%s') AND "
             "(g.cds != 0) AND (g.cds = c.id)", cdsTable, gbCdnaInfoTable, acc);
struct sqlConnection *conn = hAllocConn(db);
char cdsBuf[2048];
char *cdsStr = sqlQuickQuery(conn, query, cdsBuf, sizeof(cdsBuf));
hFreeConn(&conn);
if (isNotEmpty(cdsStr))
    {
    struct genbankCds cds;
    if (genbankCdsParse(cdsStr, &cds) && cds.startComplete && cds.start != cds.end)
        cdsStart = cds.start;
    }
return cdsStart;
}

static int getGbVersion(char *db, char *acc)
/* Return the local version that we have for acc. */
{
char query[2048];
sqlSafef(query, sizeof(query), "select version from %s where acc = '%s'", gbSeqTable, acc);
struct sqlConnection *conn = hAllocConn(db);
int version = sqlQuickNum(conn, query);
hFreeConn(&conn);
return version;
}

boolean hgvsValidate(char *db, struct hgvsVariant *hgvs, char **retFoundAcc, int *retFoundVersion,
                     char **retDiffRefAllele)
/* Return TRUE if hgvs coords are within the bounds of the sequence for hgvs->seqAcc.
 * If retFoundAcc is not NULL, set it to our local accession (which may be missing the .version
 * of hgvs->seqAcc) or NULL if we can't find any match.
 * If retFoundVersion is not NULL and hgvs->seqAcc has a version number (e.g. NM_005957.4),
 * set retFoundVersion to our version from latest GenBank, otherwise 0 (no version for LRG).
 * If coords are OK and retDiffRefAllele is not NULL: if our sequence at the coords
 * matches hgvs->refAllele then set it to NULL; if mismatch then set it to our sequence. */
{
boolean coordsOK = FALSE;
char *acc = hgvs->seqAcc;
char *foundAcc = NULL;
char *accSeq = NULL;
int cdsOffset = 0;
int refLen = strlen(hgvs->refAllele);
if (hgvs->type == hgvstProtein)
    accSeq = getProteinSeq(db, acc, &foundAcc);
else
    accSeq = getCdnaSeq(db, acc, &foundAcc);
if (retFoundVersion)
    {
    *retFoundVersion = 0;
    if (foundAcc && ! startsWith("LRG_", foundAcc))
        *retFoundVersion = getGbVersion(db, foundAcc);
    }
if (hgvs->type == hgvstCoding && foundAcc)
    cdsOffset = getCdsStart(db, foundAcc);
if (accSeq && cdsOffset >= 0 &&
    (cdsOffset + hgvs->start1 - 1 + refLen) <= strlen(accSeq))
    {
    coordsOK = TRUE;
    }
if (coordsOK && retDiffRefAllele)
    {
    char *ourSeq = cloneStringZ(accSeq + cdsOffset + hgvs->start1 - 1, refLen);
    toUpperN(ourSeq, refLen);
    if (sameString(hgvs->refAllele, ourSeq))
        *retDiffRefAllele = NULL;
    else
        *retDiffRefAllele = ourSeq;
    }
if (retFoundAcc)
    *retFoundAcc = foundAcc;
return coordsOK;
}

static struct psl *pslFromHgvs(struct hgvsVariant *hgvs, int accSize, int cdsStart)
/* Allocate and return a PSL modeling the variant in transcript named acc. */
{
if (hgvs == NULL)
    return NULL;
struct psl *psl;
AllocVar(psl);
int refLen = strlen(hgvs->refAllele);
int altLen = strlen(hgvs->altAllele);
struct dyString *dy = dyStringCreate("%s%s%s",
                                     hgvs->refAllele, hgvs->changeSymbol, hgvs->altAllele);
psl->qName = dyStringCannibalize(&dy);
psl->qSize = altLen;
psl->qStart = 0;
psl->qEnd = altLen;
psl->tName = cloneFirstWordByDelimiter(hgvs->seqAcc, '.');
psl->tSize = accSize;
safecpy(psl->strand, sizeof(psl->strand), "+");
psl->tStart = cdsStart + hgvs->start1 - 1;
psl->tEnd = psl->tStart + refLen;
psl->blockCount = 1;
AllocArray(psl->blockSizes, psl->blockCount);
AllocArray(psl->qStarts, psl->blockCount);
AllocArray(psl->tStarts, psl->blockCount);
psl->blockSizes[0] = refLen <= altLen ? refLen : altLen;
psl->qStarts[0] = psl->qStart;
psl->tStarts[0] = psl->tStart;
return psl;
}

static struct psl *hgvsMapCDotToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Return a psl with target coords from db, mapping the variant to the genome.
 * Return NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */
{
struct psl *mappedToGenome = NULL;
char *trackTable = NULL;
char *pslTable = NULL;
char *acc = cloneFirstWordByDelimiter(hgvs->seqAcc, '.');
int cdsStart = -1;
if (startsWith("NM_", acc))
    {
    // TODO: replace these with NCBI's alignments
    trackTable = "refGene";
    pslTable = "refSeqAli";
    }
else if (startsWith("LRG_", acc))
    {
    trackTable = "lrgTranscriptAli";
    pslTable = "lrgTranscriptAli";
    }
cdsStart = getCdsStart(db, acc);
if (trackTable && pslTable && cdsStart >= 0 &&
    hTableExists(db, trackTable) && hTableExists(db, pslTable))
    {
    struct dyString *dyQuery = sqlDyStringCreate("select * from %s where qName = '%s'",
                                                 pslTable, acc);
    struct sqlConnection *conn = hAllocConn(db);
    struct sqlResult *sr = sqlGetResult(conn, dyQuery->string);
    int bin = hIsBinned(db, pslTable);
    char **row;
    if (sr && (row = sqlNextRow(sr)))
        {
        struct psl *txAli = pslLoad(row+bin);
        struct psl *variantPsl = pslFromHgvs(hgvs, txAli->qSize, cdsStart);
        mappedToGenome = pslTransMap(pslTransMapNoOpts, variantPsl, txAli);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
if (mappedToGenome && retPslTable)
    *retPslTable = cloneString(pslTable);
return mappedToGenome;
}

static char *aaToArbitraryCodons(char *aaStr)
/* Translate amino acid string into codon string where we just take the first codon we
 * find for each amino. */
{
int aaLen = strlen(aaStr);
char *codonStr = needMem(aaLen*3 + 1);
int i;
for (i = 0;  i < aaLen;  i++)
    aaToArbitraryCodon(aaStr[i], &codonStr[i*3]);
return codonStr;
}

static struct psl *hgvsMapPDotToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Return a psl with target coords from db, mapping the variant to the genome.
 * Return NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */
{
struct psl *mappedToGenome = NULL;
char *acc = cloneFirstWordByDelimiter(hgvs->seqAcc, '.');
if (startsWith("NP_", acc))
    {
    // Translate the NP_*:p. to NM_*:c. and map NM_*:c. to the genome.
    struct sqlConnection *conn = hAllocConn(db);
    char query[2048];
    sqlSafef(query, sizeof(query), "select mrnaAcc from %s l, refGene r "
          "where l.protAcc = '%s' and r.name = l.mrnaAcc", refLinkTable, acc);
    char *nmAcc = sqlQuickString(conn, query);
    hFreeConn(&conn);
    if (nmAcc)
        {
        struct hgvsVariant cDot;
        zeroBytes(&cDot, sizeof(cDot));
        cDot.type = hgvstCoding;
        cDot.seqAcc = nmAcc;
        cDot.changeSymbol = ">";
        // These aren't necessarily right since AA doesn't have as much info as codon.
        // We could get the refAllele right using the reference sequence -- but altAllele
        // could still be ambiguous.  In the meantime we're not really using the alleles
        // for anything besides length so far...
        cDot.refAllele = aaToArbitraryCodons(hgvs->refAllele);
        cDot.altAllele = aaToArbitraryCodons(hgvs->altAllele);
        cDot.start1 = ((hgvs->start1 - 1) * 3) + 1;
        cDot.end = ((hgvs->end - 1) * 3) + 3;
        mappedToGenome = hgvsMapCDotToGenome(db, &cDot, retPslTable);
        }
    }
return mappedToGenome;
}

struct psl *hgvsMapToGenome(char *db, struct hgvsVariant *hgvs, char **retPslTable)
/* Return a psl with target coords from db, mapping the variant to the genome.
 * Return NULL if unable to map.
 * If successful and retPslTable is not NULL, set it to the name of the PSL table used. */
{
if (hgvs == NULL || !hgvsValidate(db, hgvs, NULL, NULL, NULL))
    return NULL;
struct psl *mappedToGenome = NULL;
if (hgvs->type == hgvstCoding)
    mappedToGenome = hgvsMapCDotToGenome(db, hgvs, retPslTable);
else if (hgvs->type == hgvstProtein)
    mappedToGenome = hgvsMapPDotToGenome(db, hgvs, retPslTable);
return mappedToGenome;
}
