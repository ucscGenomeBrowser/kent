/* hgHgvsParse.c - Parse the Human Genome Variation Society (HGVS) nomenclature for variants. */
/* See http://varnomen.hgvs.org/ and https://github.com/mutalyzer/mutalyzer/ */

/* Copyright (C) 2016 The Regents of the University of California
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hgHgvs.h"
#include "regexHelper.h"

//
// Tokenizer for HGVS description (the part after the /[cgnmpr]\./)
//
// Some tokens are valid only for certain prefixes/types of sequence, e.g. some are valid
// only after "p." (protein), some only after "c." (CDS), some only for /[cgmnr]\./ (nucleotides).
// Some operator tokens have different meanings depending on what prefix they follow.

enum hdTokenType
{
    tk_undefined,
    tk_eof,
    tk_int,
    tk_seq3Letter, // IUPAC protein 3-letter codes
    tk_seq1Letter, // IUPAC single-letter codes for protein, DNA or RNA
    // Operators
    tk_del,        // deletion
    tk_inv,        // inversion
    tk_dup,        // duplication
    tk_ins,        // insertion
    tk_con,        // conversion http://varnomen.hgvs.org/recommendations/DNA/variant/conversion
                   //            http://varnomen.hgvs.org/recommendations/RNA/variant/conversion
                   // -- looks like "delins" to me.
    tk_fs,         // frameshift (protein only)
    tk_star,       // Stop codon position anchor
    tk_underscore, // Coordinate range separator
    tk_minus,      // CDS intron relative to start of next exon | offset for protein ext operator
    tk_plus,       // CDS intron relative to end of previous exon | offset for protein ext operator
    tk_equal,      // synonymous (protein) or no change from reference (nucleotide)
    tk_ntSubst,    // nucleotide substitution
    tk_question,   // unknown position
    tk_leftParen,  // begin uncertain subrange | predicted consequence | grouping of "or" terms |
                   // surrounding semicolon with no square brackets when synteny is uncertain
    tk_rightParen, // end "

    // Advanced operators, not likely to be supported soon:
    tk_colon,      // oops, the previous tokens, possibly including a "." + version, and
                   // possibly followed by a /[cgmnpr]\./, were
                   // actually the accession of a sequence from which some bases are copied here.
                   // Fortunately it's irrelevant for mapping to genomic coords in browser.
                   // example from http://varnomen.hgvs.org/recommendations/DNA/variant/insertion/ :
                   // g.123_124insL37425.1:23_361
                   // "the insertion of nucleotides 23 to 361 as described in GenBank file L37425.1
                   //  between nucleotides g.123 and g.124"
                   // and ClinVar: NM_020533.2(MCOLN1):c.236_237insNC_012920.1:m.12435_12527
    tk_period,     // hopefully this is followed by a version number and colon, or after a colon
                   // and [cgmnpr].
    tk_caret,      // "or" (e.g. c.(370A>C^372C>R) as back translation of p.Ser124Arg)
    tk_ext,        // extension http://varnomen.hgvs.org/recommendations/protein/variant/extension
    tk_leftSquare, // begin haplotype allele; or, if followed by number, repeat count.
    tk_rightSquare,// "
    tk_semicolon,  // separator for variants of a genomic haplotype allele inside []s,
                   // separator for multiple haplotypes (e.g. diploid) outside []s
                   // enclosed in parens "(;)" when not certain if variants are on same allele
    tk_comma,      // sep transcript or protein (not genomic) variants inside []s
    tk_doubleSlash,// chimerism http://varnomen.hgvs.org/recommendations/DNA/variant/complex/
    tk_singleSlash,// mosaicism http://varnomen.hgvs.org/recommendations/DNA/variant/complex/
    tk_leftCurly,  // see http://varnomen.hgvs.org/recommendations/open-issues/#imperfectcopy
    tk_rightCurly, // "
};

struct hdTokenProps
    // Associate the token string that we'll scan for in input with type and attributes
    {
    char *value;                  // token string literal (do not clobber!)
    enum hdTokenType type;        // token as enum
    };

static struct hdTokenProps dnaTokens[] =
    // Tokens that may appear after "g." (genomic), "m." (mitochondrial) and "n." (non-coding)
    {
    // Operators
    { "ins", tk_ins },
    { "del", tk_del },
    { "dup", tk_dup },
    { "inv", tk_inv },
    { "_", tk_underscore },
    { "=", tk_equal },
    { ">", tk_ntSubst },
    { "?", tk_question },
    { "(", tk_leftParen },
    { ")", tk_rightParen },
    { "[", tk_leftSquare },
    { "]", tk_rightSquare },
    // IUPAC nucleotide single-letter codes
    // Note: the HGVS website doesn't spell out whether ambiguous codes are OK,
    // but ClinVar uses them so let's support them.
    { "A", tk_seq1Letter },
    { "C", tk_seq1Letter },
    { "G", tk_seq1Letter },
    { "T", tk_seq1Letter },
    { "B", tk_seq1Letter },
    { "D", tk_seq1Letter },
    { "H", tk_seq1Letter },
    { "K", tk_seq1Letter },
    { "M", tk_seq1Letter },
    { "N", tk_seq1Letter },
    { "R", tk_seq1Letter },
    { "S", tk_seq1Letter },
    { "V", tk_seq1Letter },
    { "W", tk_seq1Letter },
    { "Y", tk_seq1Letter },
    { NULL, tk_undefined },
    };

static struct hdTokenProps codingTokens[] =
    // Tokens that may appear after "c." (cDNA with CDS numbering).
    {
    // Operators
    { "ins", tk_ins },
    { "del", tk_del },
    { "dup", tk_dup },
    { "inv", tk_inv },
    { "*", tk_star },
    { "_", tk_underscore },
    { "-", tk_minus },
    { "+", tk_plus },
    { "=", tk_equal },
    { ">", tk_ntSubst },
    { "?", tk_question },
    { "(", tk_leftParen },
    { ")", tk_rightParen },
    { "[", tk_leftSquare },
    { "]", tk_rightSquare },
    // IUPAC nucleotide single-letter codes
    // Note: the HGVS website doesn't spell out whether ambiguous codes are OK,
    // but ClinVar uses them so let's support them.
    { "A", tk_seq1Letter },
    { "C", tk_seq1Letter },
    { "G", tk_seq1Letter },
    { "T", tk_seq1Letter },
    { "B", tk_seq1Letter },
    { "D", tk_seq1Letter },
    { "H", tk_seq1Letter },
    { "K", tk_seq1Letter },
    { "M", tk_seq1Letter },
    { "N", tk_seq1Letter },
    { "R", tk_seq1Letter },
    { "S", tk_seq1Letter },
    { "V", tk_seq1Letter },
    { "W", tk_seq1Letter },
    { "Y", tk_seq1Letter },
    { NULL, tk_undefined },
    };

static struct hdTokenProps rnaTokens[] =
    // Tokens that may appear after "r." (RNA) -- maddeningly, these may use CDS or noncoding
    // numbering, unspecified, I guess you have to guess from NM_ vs NR_ but who knows about LRG.
    // I hope this isn't used too often.
    {
    // Operators
    { "ins", tk_ins },
    { "del", tk_del },
    { "dup", tk_dup },
    { "inv", tk_inv },
    { "*", tk_star },
    { "_", tk_underscore },
    { "-", tk_minus },
    { "+", tk_plus },
    { "=", tk_equal },
    { ">", tk_ntSubst },
    { "?", tk_question },
    { "(", tk_leftParen },
    { ")", tk_rightParen },
    { "[", tk_leftSquare },
    { "]", tk_rightSquare },
    // IUPAC RNA bases: lower-case, u instead of t
    // Note: the HGVS website doesn't spell out whether ambiguous codes are OK,
    // but ClinVar uses them so let's support them.
    { "a", tk_seq1Letter },
    { "c", tk_seq1Letter },
    { "g", tk_seq1Letter },
    { "u", tk_seq1Letter },
    { "b", tk_seq1Letter },
    { "d", tk_seq1Letter },
    { "h", tk_seq1Letter },
    { "k", tk_seq1Letter },
    { "m", tk_seq1Letter },
    { "n", tk_seq1Letter },
    { "r", tk_seq1Letter },
    { "s", tk_seq1Letter },
    { "v", tk_seq1Letter },
    { "w", tk_seq1Letter },
    { "y", tk_seq1Letter },
    { NULL, tk_undefined },
    };

static struct hdTokenProps proteinTokens[] =
    // Tokens that may appear after "p." (protein)
    {
    // Operators
    { "ins", tk_ins },
    { "del", tk_del },
    { "dup", tk_dup },
    { "inv", tk_inv },
    { "fs", tk_fs },
    { "_", tk_underscore },
    { "=", tk_equal },
    { "?", tk_question },
    { "(", tk_leftParen },
    { ")", tk_rightParen },
    { "*", tk_star },
    // IUPAC protein 3-letter codes
    { "Ala", tk_seq3Letter },
    { "Arg", tk_seq3Letter },
    { "Asn", tk_seq3Letter },
    { "Asp", tk_seq3Letter },
    { "Cys", tk_seq3Letter },
    { "Gln", tk_seq3Letter },
    { "Glu", tk_seq3Letter },
    { "Gly", tk_seq3Letter },
    { "His", tk_seq3Letter },
    { "Ile", tk_seq3Letter },
    { "Leu", tk_seq3Letter },
    { "Lys", tk_seq3Letter },
    { "Met", tk_seq3Letter },
    { "Phe", tk_seq3Letter },
    { "Pro", tk_seq3Letter },
    { "Ser", tk_seq3Letter },
    { "Thr", tk_seq3Letter },
    { "Trp", tk_seq3Letter },
    { "Tyr", tk_seq3Letter },
    { "Val", tk_seq3Letter },
    { "Ter", tk_seq3Letter },
    // IUPAC protein (and/or nucleotide ACGT) single-letter codes
    { "A", tk_seq1Letter },
    { "R", tk_seq1Letter },
    { "N", tk_seq1Letter },
    { "D", tk_seq1Letter },
    { "C", tk_seq1Letter },
    { "Q", tk_seq1Letter },
    { "E", tk_seq1Letter },
    { "G", tk_seq1Letter },
    { "H", tk_seq1Letter },
    { "I", tk_seq1Letter },
    { "L", tk_seq1Letter },
    { "K", tk_seq1Letter },
    { "M", tk_seq1Letter },
    { "F", tk_seq1Letter },
    { "P", tk_seq1Letter },
    { "S", tk_seq1Letter },
    { "T", tk_seq1Letter },
    { "W", tk_seq1Letter },
    { "Y", tk_seq1Letter },
    { "V", tk_seq1Letter },
    { "X", tk_seq1Letter },
    { NULL, tk_undefined },
    };

static struct hdTokenProps *hdTokenAlphabetFromType(enum hgvsSeqType type)
/* Return a set of acceptable tokens for type. */
{
struct hdTokenProps *props = NULL;
switch (type)
    {
    case hgvstCoding:
        props = codingTokens;
        break;
    case hgvstGenomic:
    case hgvstMito:
    case hgvstNoncoding:
        props = dnaTokens;
        break;
    case hgvstProtein:
        props = proteinTokens;
        break;
    case hgvstRna:
        props = rnaTokens;
        break;
    default:
        errAbort("Unrecognized hgvsSeqType %d", type);
    }
return props;
}

struct hdTokenizer
/* Tokenizer on string input, with properties of current token, that can reuse the current token. */
{
    char *input;                          // Text to be parsed (after current token)
    struct hdTokenProps *tokenAlphabet;   // Array of all tokens that we might encounter
    enum hdTokenType type;                // Type of the current token
    char *string;                         // String value of current token
    int sAlloc;                           // Allocated string size
    bool reuse;	                          // TRUE if want to give back this token
    bool eof;                             // TRUE at end of input
};

// Expand sAlloc as needed, this many bytes at a time:
#define HDT_BUF_SIZE 128

static void hdtReuse(struct hdTokenizer *hdt)
/* Reuse current token, i.e. don't advance on the next call to hdtNext(). */
{
hdt->reuse = TRUE;
}

static void hdtAdvance(struct hdTokenizer *hdt, int len)
/* Copy len chars of hdt->input into hdt->string & null-terminate, resizing if necessary,
 * and advance hdt->input by len. */
{
if (len >= hdt->sAlloc)
    {
    while (len >= hdt->sAlloc)
        hdt->sAlloc += HDT_BUF_SIZE;
    hdt->string = needMoreMem(hdt->string, 0, hdt->sAlloc);
    }
memcpy(hdt->string, hdt->input, len);
hdt->string[len] = '\0';
hdt->input += len;
}

static boolean hdtEof(struct hdTokenizer *hdt)
/* If input is empty, update hdt to EOF state and return TRUE; */
{
if (isEmpty(hdt->input))
    {
    hdt->eof = TRUE;
    hdt->type = tk_eof;
    hdt->string[0] = '\0';
    return TRUE;
    }
return FALSE;
}

static boolean hdtNextFromAlphabet(struct hdTokenizer *hdt)
/* If hdt->input starts with a static string in the tokenAlphabet, update hdt and return TRUE */
{
if (hdtEof(hdt))
    return FALSE;
int i;
for (i = 0;  isNotEmpty(hdt->tokenAlphabet[i].value);  i++)
    {
    struct hdTokenProps *props = &hdt->tokenAlphabet[i];
    if (startsWith(props->value, hdt->input))
        {
        hdtAdvance(hdt, strlen(props->value));
        hdt->type = props->type;
        return TRUE;
        }
    }
hdt->type = tk_undefined;
return FALSE;
}

static void dyPrependf(struct dyString *dyError, char *format, ...)
/* Add a warning to *beginning* of dyError so that when we pop the call stack, higher
 * level errors appear before lower lever errors. */
{
char oldErr[dyStringLen(dyError)+1];
safecpy(oldErr, sizeof(oldErr), dyStringContents(dyError));
dyStringClear(dyError);
va_list args;
va_start(args, format);
dyStringVaPrintf(dyError, format, args);
va_end(args);
if (isNotEmpty(oldErr))
    {
    char newErr[dyStringLen(dyError)+1];
    safecpy(newErr, sizeof(newErr), dyStringContents(dyError));
    dyStringClear(dyError);
    dyStringAppend(dyError, newErr);
    dyStringAppend(dyError, "; ");
    dyStringAppend(dyError, oldErr);
    }
}

static boolean hdtNext(struct hdTokenizer *hdt, struct dyString *dyError, char *context)
/* If hdt can advance to the next token in input, then make that hdt's current token and return
 * TRUE. EOF is considered valid. If input starts with something unexpected then return FALSE. */
{
if (hdt->reuse)
    {
    // Stay on current token.
    hdt->reuse = FALSE;
    return TRUE;
    }
if (hdtEof(hdt))
    return TRUE;
if (isdigit(hdt->input[0]))
    {
    // Handle numbers separately from static tokens.
    int len = 1;
    while (isdigit(hdt->input[len]))
        len++;
    hdtAdvance(hdt, len);
    hdt->type = tk_int;
    return TRUE;
    }
else
    {
    char *start = hdt->input;
    if (hdtNextFromAlphabet(hdt))
        {
        enum hdTokenType firstType = hdt->type;
        if (firstType == tk_seq1Letter || firstType == tk_seq3Letter)
            {
            // Accumulate sequence into string instead of returning one base at a time.
            boolean isValid = TRUE;
            while (hdt->type == firstType && isValid)
                isValid = hdtNextFromAlphabet(hdt);
            // We advanced to something that is not sequence, so we'll need to back up.
            int len = hdt->input - start;
            if (isValid)
                len -= strlen(hdt->string);
            hdt->input = start;
            hdt->type = firstType;
            hdt->eof = FALSE;
            hdtAdvance(hdt, len);
            }
        return TRUE;
        }
    }
dyPrependf(dyError, "bad token %s at '%s'", context, hdt->input);
return FALSE;
}

static struct hdTokenizer *hdTokenizerNew(char *input, struct hdTokenProps *tokenAlphabet)
/* Return a new tokenizer on input that will use tokenAlphabet.  Call htdNext on the returned
 * tokenizer to get the first token (and the next...) loaded into tokenizer.  Note that this
 * does not clone input, it simply scans it. */
{
struct hdTokenizer *hdt;
AllocVar(hdt);
hdt->input = input;
hdt->tokenAlphabet = tokenAlphabet;
hdt->sAlloc = HDT_BUF_SIZE;
hdt->string = needMem(hdt->sAlloc);
return hdt;
}

static void hdTokenizerFree(struct hdTokenizer **pHdt)
/* Free up what we allocated (which does not include hdt->input). */
{
struct hdTokenizer *hdt = *pHdt;
if (hdt != NULL)
    {
    freeMem(hdt->string);
    freez(pHdt);
    }
}

static boolean parseNonNegNumOrRange(struct hdTokenizer *hdt, int *retMin, int *retMax,
                                     struct dyString *dyError)
/* Parse a nonnegative number or _-separated range of nonneg numbers, possibly '?' for
 * unknown value.
 * Set retMin and retMax to -1 for '?', otherwise the min and max number. */
{
int min = 0, max = 0;
// Expect a number
if (hdt->type == tk_int || hdt->type == tk_question)
    {
    min = max = (hdt->type == tk_int) ? atoi(hdt->string) : HGVS_LENGTH_UNSPECIFIED;
    if (!hdtNext(hdt, dyError, "after number"))
        return FALSE;
    if (hdt->type == tk_underscore)
        {
        // range of counts -- get next number
        if (!hdtNext(hdt, dyError, "after underscore"))
            return FALSE;
        if (hdt->type == tk_int || hdt->type == tk_question)
            max = (hdt->type == tk_int) ? atoi(hdt->string) : HGVS_LENGTH_UNSPECIFIED;
        else
            {
            dyPrependf(dyError,
                       "expecting nonnegative numeric range but got non-number after '%d_' at '%s'",
                       min, hdt->string);
            return FALSE;
            }
        }
    else
        // just a number, not a range; don't consume this token
        hdtReuse(hdt);
    }
else
    {
    dyPrependf(dyError, "expecting nonnegative number or range (or '?') but got '%s'", hdt->string);
    return FALSE;
    }
*retMin = min;
*retMax = max;
return TRUE;
}

static boolean parseNonNegNumOrRangeMaybeParens(struct hdTokenizer *hdt, int *retMin, int *retMax,
                                                struct dyString *dyError)
{
/* Parse a nonnegative number or _-separated range of nonneg numbers, possibly '?' for
 * unknown value, possibly enclosed in parens.
 * Set retMin and retMax to -1 for '?', otherwise the min and max number. */
if (hdt->type == tk_leftParen)
    {
    if (!hdtNext(hdt, dyError, "after '('"))
        return FALSE;
    if (! parseNonNegNumOrRange(hdt, retMin, retMax, dyError))
        {
        dyPrependf(dyError, "after '('");
        return FALSE;
        }
    if (!hdtNext(hdt, dyError, "after '(' followed by number/range"))
        return FALSE;
    if (hdt->type != tk_rightParen)
        {
        dyPrependf(dyError, "expected ')' after number/range, got '%s'", hdt->string);
        return FALSE;
        }
    return TRUE;
    }
else
    return parseNonNegNumOrRange(hdt, retMin, retMax, dyError);
}

static struct hgvsChange *hgvsChangeNewRepeat(char *seq, int min, int max)
/* Allocate and return an hgvsChange specifying a repeated sequence. */
{
struct hgvsChange *change;
AllocVar(change);
change->type = hgvsctRepeat;
change->value.repeat.seq = seq;
change->value.repeat.min = min;
change->value.repeat.max = max;
return change;
}

static struct hgvsChange *parseNtRepeat(char *seq, struct hdTokenizer *hdt,
                                        struct dyString *dyError)
/* Optionally preceded by seq, enclosed in square brackets and/or parens, a number or range. */
{
// Make sure we start with '(' or '['
enum hdTokenType lType = hdt->type;
if (lType != tk_leftParen && lType != tk_leftSquare)
    errAbort("parseNtRepeat: expected to start with ( or [");
if (!hdtNext(hdt, dyError, "after '[' or '(' in repeat"))
    return NULL;
// Next should be a repeat count or range of counts
int min, max;
if (! parseNonNegNumOrRangeMaybeParens(hdt, &min, &max, dyError))
    {
    dyPrependf(dyError, "in brackets for repeat");
    return NULL;
    }
// Now matching ')' or ']'
if (!hdtNext(hdt, dyError, "after '[' or '(' followed by number"))
    return NULL;
struct hgvsChange *change = NULL;
if ((lType == tk_leftParen && hdt->type == tk_rightParen) ||
    (lType == tk_leftSquare && hdt->type == tk_rightSquare))
    {
    change = hgvsChangeNewRepeat(seq, min, max);
    }
else
    dyPrependf(dyError, "mismatching brackets/parens for repeat");
return change;
}

static struct hgvsChange *hgvsChangeNewSimple(enum hgvsChangeType type, char *ref, int refLength,
                                              char *alt, int altLength)
/* Allocate and return a simple change with optional ref and optional alt (or just lengths).
 * ref and alt are not cloned here, they must be allocated by caller! */
{
struct hgvsChange *change;
AllocVar(change);
change->type = type;
change->value.refAlt.refSequence = ref;
change->value.refAlt.refLength = refLength;
if (alt == NULL && altLength != HGVS_LENGTH_UNSPECIFIED)
    {
    change->value.refAlt.altType = hgvsstLength;
    change->value.refAlt.altValue.length = altLength;
    }
else
    {
    change->value.refAlt.altType = hgvsstSimple;
    change->value.refAlt.altValue.seq = alt;
    }
return change;
}

static struct hgvsChange *parseNtIupac(struct hdTokenizer *hdt, struct dyString *dyError)
/* IUPAC sequence should be followed by '>' (subst) or '=' (no change) or '[' (repeat) */
{
if (hdt->type != tk_seq1Letter)
    errAbort("parseNtIupac: expected to start with nucleotide IUPAC sequence");
char *ref = cloneString(hdt->string);
if (!hdtNext(hdt, dyError, "after IUPAC sequence"))
    return NULL;
if (hdt->type == tk_ntSubst)
    {
    if (!hdtNext(hdt, dyError, "after '>'"))
        return NULL;
    else if (hdt->type == tk_seq1Letter)
        return hgvsChangeNewSimple(hgvsctSubst, ref, HGVS_LENGTH_UNSPECIFIED,
                                   cloneString(hdt->string), HGVS_LENGTH_UNSPECIFIED);
    else
        dyPrependf(dyError, "expected IUPAC sequence after '>', got '%s'", hdt->string);
    }
else if (hdt->type == tk_equal)
    {
    // Reference sequence was asserted, so store that for comparison with actual reference.
    return hgvsChangeNewSimple(hgvsctNoChange, ref, HGVS_LENGTH_UNSPECIFIED,
                               NULL, HGVS_LENGTH_UNSPECIFIED);
    }
else if (hdt->type == tk_leftSquare || hdt->type == tk_leftParen)
    return parseNtRepeat(ref, hdt, dyError);
else
    dyPrependf(dyError, "something unexpected after IUPAC sequence");
return NULL;
}

static struct hgvsChange *parseNtDelOrDupOrInv(struct hdTokenizer *hdt, struct dyString *dyError)
/* del, dup or inv may be followed by an asserted reference sequence, a redundant length (ignore),
 * or a new change. */
{
struct hgvsChange *change = NULL;
if (hdt->type != tk_del && hdt->type != tk_dup && hdt->type != tk_inv)
    errAbort("parseNtDelOrDupOrInv: expecting 'del', 'dup' or 'inv'");
enum hdTokenType opType = hdt->type;
if (!hdtNext(hdt, dyError, "after del, dup or inv"))
    return NULL;
char *ref = NULL;
int refLength = HGVS_LENGTH_UNSPECIFIED;
if (hdt->type == tk_seq1Letter)
    ref = cloneString(hdt->string);
else if (hdt->type == tk_int)
    refLength = atoi(hdt->string);
else
    // Something else -- hopefully the beginning of the next change or eof
    hdtReuse(hdt);
enum hgvsChangeType type = hgvsctUndefined;
if (opType == tk_del)
    type = hgvsctDel;
else if (opType == tk_dup)
    type = hgvsctDup;
else if (opType == tk_inv)
    type = hgvsctInv;
change = hgvsChangeNewSimple(type, ref, refLength, NULL, HGVS_LENGTH_UNSPECIFIED);
return change;
}

static enum hgvsSeqType hgvsSeqTypeFromString(char changeType)
/* Translate [cgmnrp] to enum hgvsSeqType */
{
switch (changeType)
    {
    // Not using break below, because return does the job
    case 'c':
        return hgvstCoding;
    case 'g':
        return hgvstGenomic;
    case 'm':
        return hgvstMito;
    case 'n':
        return hgvstNoncoding;
    case 'r':
        return hgvstRna;
    case 'p':
        return hgvstProtein;
    default:
        errAbort("hgvsSeqTypeFromString: unrecognized input '%c'", changeType);
    }
return hgvstUndefined;
}

// After ins or con, it's possible to have a nested term to specify sequence from outside the ref:
#define nestedExp "^([A-Z_]+[0-9]+(\\.[0-9]+)?):([cgmnr])\\.([0-9]+)_([0-9]+)(inv[0-9]*)?"
//                  1 .......................                                      accession
//                                2 .........                                      dot suffix?
//                                              3 ....                             type
//                                                          4 ....                 start
//                                                                   5 ....        end
//                                                                           6 ... inv?

static struct hgvsChange *hgvsChangeNewNested(struct hdTokenizer *hdt, regmatch_t *substrs)
/* hdt's current token is inv or con; next up is a nested HGVS term with components in
 * substrs as outlined for nestedExp above.  Return an hgvsChange with nested hgvsVariant. */
{
struct hgvsChange *change;
AllocVar(change);
change->type = (hdt->type == tk_ins) ? hgvsctIns : hgvsctCon;
struct hgvsChangeRefAlt *refAlt = &change->value.refAlt;
refAlt->refLength = HGVS_LENGTH_UNSPECIFIED;
refAlt->altType = hgvsstNestedTerm;
struct hgvsVariant *nestedTerm = &refAlt->altValue.term;
nestedTerm->seqAcc = regexSubstringClone(hdt->input, substrs[1]);
char changeType = hdt->input[substrs[3].rm_so];  // [cgmnrp]
nestedTerm->type = hgvsSeqTypeFromString(changeType);
nestedTerm->start1 = regexSubstringInt(hdt->input, substrs[4]);
nestedTerm->end = regexSubstringInt(hdt->input, substrs[5]);
nestedTerm->changes = regexSubstringClone(hdt->input, substrs[6]);
return change;
}

static struct hgvsChange *parseNtInsOrCon(struct hdTokenizer *hdt, struct dyString *dyError)
/* ins or con may be followed by a nested term (accession, type dot, pos range, possibly inv)
 * so do some extra lookahead.   ins can also be followed by numbers in parens (or missing the
 * parens...) to indicate a number of unknown bases (Ns); if so then we do an end run around the
 * tokenizer. */
{
struct hgvsChange *change = NULL;
if (hdt->type != tk_ins && hdt->type != tk_con)
    errAbort("parseNtInsOrCon: expecting 'ins' or 'con', got '%s' (followed by '%s')",
             hdt->string, hdt->input);
// Don't advance hdt until we first look ahead for accession:...
regmatch_t substrs[8];
if (regexMatchSubstr(hdt->input, nestedExp, substrs, ArraySize(substrs)))
    {
    change = hgvsChangeNewNested(hdt, substrs);
    // Skip over everything that nestedRegex matched, and update hdt->type.
    hdtAdvance(hdt, substrs[0].rm_eo - substrs[0].rm_so);
    hdt->type = (regexSubstrMatched(substrs[6])) ? tk_inv : tk_seq1Letter;
    }
else
    {
    // Tokenize and parse the usual way -- a sequence or length might be asserted after this.
    enum hdTokenType type = hdt->type;
    char context[hdt->sAlloc + 256];
    safef(context, sizeof(context), "after '%s'", hdt->string);
    if (!hdtNext(hdt, dyError, context))
        return NULL;
    char *insSeq = NULL;
    int insLen = HGVS_LENGTH_UNSPECIFIED;
    if (hdt->type == tk_seq1Letter)
        insSeq = cloneString(hdt->string);
    else if (hdt->type == tk_int || hdt->type == tk_leftParen)
        {
        // Length only, should be in parens but ClinVar just puts number.
        int min, max;
        if (! parseNonNegNumOrRangeMaybeParens(hdt, &min, &max, dyError))
            {
            dyPrependf(dyError, "expecting number (optionally in parens) after '%s'",
                       (type == tk_ins) ? "ins" : "con");
            return NULL;
            }
        if (min != max)
            {
//#*** A range is valid -- it means the insertion is from some other position range in this sequence!  And it can be inverted.
            dyPrependf(dyError, "expecting number after %s but got range (%d_%d)",
                       (type == tk_ins) ? "ins" : "con", min, max);
            return NULL;
            }
        insLen = max;
        }
    if (insSeq || insLen != HGVS_LENGTH_UNSPECIFIED)
        {
        enum hgvsChangeType changeType = (type == tk_ins) ? hgvsctIns : hgvsctCon;
        change = hgvsChangeNewSimple(changeType, NULL, HGVS_LENGTH_UNSPECIFIED,
                                     insSeq, insLen);
        }
    }
return change;
}

struct hgvsChange *hgvsParseNucleotideChange(char *changeStr, enum hgvsSeqType type,
                                             struct dyString *dyError)
/* Return a parse tree of a coding HGVS sequence change (the part after the position range)
 * possibly followed by additional changes.  If there are any parse errors, they will be
 * appended to dyError. */
{
struct hdTokenProps *alphabet = hdTokenAlphabetFromType(type);
struct hdTokenizer *hdt = hdTokenizerNew(changeStr, alphabet);
struct hgvsChange *changeList = NULL;
char context[strlen(changeStr) + 256];
safef(context, sizeof(context), "at start of '%s'", changeStr);
if (!hdtNext(hdt, dyError, context))
    {
    hdTokenizerFree(&hdt);
    return NULL;
    }
while (! hdt->eof)
    {
    struct hgvsChange *change = NULL;
    if (hdt->type == tk_seq1Letter)
        change = parseNtIupac(hdt, dyError);
    else if (hdt->type == tk_leftSquare || hdt->type == tk_leftParen)
        change = parseNtRepeat(NULL, hdt, dyError);
    else if (hdt->type == tk_del || hdt->type == tk_dup ||
             hdt->type == tk_inv)
        change = parseNtDelOrDupOrInv(hdt, dyError);
    else if (hdt->type == tk_ins || hdt->type == tk_con)
        change = parseNtInsOrCon(hdt, dyError);
    else if (hdt->type == tk_equal)
        change = hgvsChangeNewSimple(hgvsctNoChange, "", HGVS_LENGTH_UNSPECIFIED,
                                     NULL, HGVS_LENGTH_UNSPECIFIED);
    if (change != NULL)
        slAddHead(&changeList, change);
    else
        {
        dyPrependf(dyError, "couldn't parse HGVS nucleotide change string '%s'", changeStr);
        break;
        }
    if (!hdtNext(hdt, dyError, "after valid change"))
        {
        break;
        }
    }
slReverse(&changeList);
hdTokenizerFree(&hdt);
return changeList;
}
