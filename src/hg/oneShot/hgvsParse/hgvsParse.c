/* hgvsParse - Command-line interface for testing hgHgvsParse lib.. */
#include "common.h"
#include "hgHgvs.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgvsParse - Command-line interface for testing hgHgvsParse lib.\n"
  "usage:\n"
  "   hgvsParse <term>\n"
  "This program attempts to parse <term> as an HGVS term and reconstruct it."
//  "options:\n"
//  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *hgvsSeqTypeToString(enum hgvsSeqType type)
/* enum symbol to string */
{
char *string = "undefined!";
if (type == hgvstCoding)
    string = "hgvstCoding";
else if (type == hgvstGenomic)
    string = "hgvstGenomic";
else if (type == hgvstMito)
    string = "hgvstMito";
else if (type == hgvstNoncoding)
    string = "hgvstNoncoding";
else if (type == hgvstRna)
    string = "hgvstRna";
else if (type == hgvstProtein)
    string = "hgvstProtein";
return string;
}

char *hgvsChangeTypeToString(enum hgvsChangeType type)
/* enum symbol to string */
{
char *string = "undefined!";
if (type == hgvsctNoChange)
    string = "NoChange";
else if (type == hgvsctSubst)
    string = "Subst";
else if (type == hgvsctDel)
    string = "Del";
else if (type == hgvsctDup)
    string = "Dup";
else if (type == hgvsctInv)
    string = "Inv";
else if (type == hgvsctIns)
    string = "Ins";
else if (type == hgvsctCon)
    string = "Con";
else if (type == hgvsctRepeat)
    string = "Repeat";
else if (type == hgvsctFrameshift)
    string = "Frameshift";
return string;
}

void hgvsRefAltDump(struct hgvsChangeRefAlt *refAlt)
{
if (refAlt->refSequence != NULL)
    printf("  REF: '%s'\n", refAlt->refSequence);
else if (refAlt->refLength != HGVS_LENGTH_UNSPECIFIED)
    printf("  REF: %d bases\n", refAlt->refLength);
if (refAlt->altType == hgvsstSimple)
    {
    if (refAlt->altValue.seq != NULL)
        printf("  ALT: '%s'\n", refAlt->altValue.seq);
    }
else if (refAlt->altType == hgvsstLength)
    {
    if (refAlt->altValue.length != HGVS_LENGTH_UNSPECIFIED)
        printf("  ALT: %d bases\n", refAlt->altValue.length);
    }
else if (refAlt->altType == hgvsstNestedChange)
    {
    errAbort("Sorry, don't do nestedChange yet");
    }
else if (refAlt->altType == hgvsstNestedTerm)
    {
    struct hgvsVariant *nested = &refAlt->altValue.term;
    printf("  NESTED: seqAcc = '%s'\n", nested->seqAcc);
    printf("  NESTED: seqType = %s\n", hgvsSeqTypeToString(nested->type));
    printf("  NESTED: start1 = %d\n", nested->start1);
    printf("  NESTED: end = %d\n", nested->end);
    if (isNotEmpty(nested->change))
        printf("  NESTED: change = '%s'\n", nested->change);
    }
else
    errAbort("hgvsRefAltDump: unexpected hgvsSeqChangeType %d", refAlt->altType);
}

static void printNonnegative(char *label, int val)
/* If val is nonnegative, print it out, otherwise '?'. */
{
if (val >= 0)
    printf("%s%d\n", label, val);
else
    printf("%s?\n", label);
}

void hgvsRepeatDump(struct hgvsChangeRepeat *repeat)
/* Print out a repeat with optional sequence and count or range of counts */
{
if (repeat->seq != NULL)
    printf("  SEQ: '%s'\n", repeat->seq);
if (repeat->min == repeat->max)
    printNonnegative("  NUM: ", repeat->min);
else
    {
    printNonnegative("  MIN: ", repeat->min);
    printNonnegative("  MAX: ", repeat->max);
    }
}

void hgvsChangeDump(struct hgvsChange *change)
{
printf("%s\n", hgvsChangeTypeToString(change->type));
if (change->type == hgvsctSubst || change->type == hgvsctNoChange ||
    change->type == hgvsctDel || change->type == hgvsctDup || change->type == hgvsctInv ||
    change->type == hgvsctIns || change->type == hgvsctCon)
    {
    hgvsRefAltDump(&change->value.refAlt);
    }
else if (change->type == hgvsctRepeat)
    {
    hgvsRepeatDump(&change->value.repeat);
    }
else if (change->type != hgvsctNoChange)
    errAbort("Sorry, don't know how to dump type %d", change->type);
}

void hgvsParse(char *term)
/* hgvsParse - Command-line interface for testing hgHgvsParse lib.. */
{
printf("Parsing '%s'\n", term);
struct hgvsVariant *hgvs = hgvsParseTerm(term);
if (hgvs == NULL)
    errAbort("Unable to parse '%s' as HGVS", term);
if (hgvs->type == hgvstProtein)
    errAbort("Sorry, don't do protein terms yet.");
struct dyString *dyError = dyStringNew(0);
struct hgvsChange *changeList = hgvsParseNucleotideChange(hgvs->change, hgvs->type, dyError);
if (changeList == NULL)
    errAbort("%s", dyError->string);
else
    {
    struct hgvsChange *change;
    for (change = changeList;  change;  change = change->next)
        hgvsChangeDump(change);
    if (isNotEmpty(dyError->string))
        puts(dyError->string);
    }
dyStringFree(&dyError);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
hgvsParse(argv[1]);
return 0;
}
