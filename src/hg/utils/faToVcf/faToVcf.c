/* faToVcf - Convert a FASTA alignment file to VCF. */
#include "common.h"
#include "fa.h"
#include "linefile.h"
#include "hash.h"
#include "iupac.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faToVcf - Convert a FASTA alignment file to Variant Call Format (VCF) single-nucleotide diffs\n"
  "usage:\n"
  "   faToVcf in.fa out.vcf\n"
  "options:\n"
  "   -ambiguousToN         Treat all IUPAC ambiguous bases (N, R, V etc) as N (no call).\n"
  "   -excludeFile=file     Exclude sequences named in file which has one sequence name per line\n"
  "   -includeRef           Include the reference in the genotype columns\n"
  "                         (default: omitted as redundant)\n"
  "   -maskSites=file       Exclude variants in positions recommended for masking in file\n"
  "                         (typically https://github.com/W-L/ProblematicSites_SARS-CoV2/raw/master/problematic_sites_sarsCov2.vcf)\n"
  "   -minAc=N              Ignore alternate alleles observed fewer than N times\n"
  "   -noGenotypes          Output 8-column VCF, without the sample genotype columns\n"
  "   -ref=seqName          Use seqName as the reference sequence; must be present in faFile\n"
  "                         (default: first sequence in faFile)\n"
  "   -resolveAmbiguous     For IUPAC ambiguous characters like R (A or G), if the character\n"
  "                         represents two bases and one is the reference base, convert it to the\n"
  "                         non-reference base.  Otherwise convert it to N.\n"
  "   -startOffset=N        Add N bases to each position (for trimmed alignments)\n"
  "   -vcfChrom=seqName     Use seqName for the CHROM column in VCF (default: ref sequence)\n"
  "in.fa must contain a series of sequences with different names and the same length.\n"
  "Both N and - are treated as missing information.\n"
  );
}

#define MAX_ALTS 256

/* Command line validation table. */
static struct optionSpec options[] = {
    { "ambiguousToN", OPTION_BOOLEAN },
    { "excludeFile", OPTION_STRING },
    { "includeRef", OPTION_BOOLEAN },
    { "maskSites", OPTION_STRING },
    { "minAc", OPTION_INT },
    { "noGenotypes", OPTION_BOOLEAN },
    { "ref", OPTION_STRING },
    { "resolveAmbiguous", OPTION_BOOLEAN },
    { "startOffset", OPTION_INT},
    { "vcfChrom", OPTION_STRING },
    { NULL, 0 },
};

static struct dnaSeq *removeExcludedSequences(struct dnaSeq *sequences)
/* If -excludeFile was passed in, remove any sequences whose names are found in the file and
 * return the modified list. */
{
char *excludeFile = optionVal("excludeFile", NULL);
if (excludeFile)
    {
    struct lineFile *lf = lineFileOpen(excludeFile, TRUE);
    struct hash *excludedSeqs = hashNew(0);
    char *line;
    while (lineFileNextReal(lf, &line))
        {
        line = trimSpaces(line);
        hashAdd(excludedSeqs, line, NULL);
        }
    lineFileClose(&lf);
    // Make sure there's not a conflict between the reference sequence and the excluded seqs
    char *explicitRef = optionVal("ref", NULL);
    if (explicitRef && hashLookup(excludedSeqs, explicitRef))
        errAbort("-ref sequence '%s' is included in -excludeFile %s."
                 "Please resolve this contradiction.",
                 explicitRef, excludeFile);
    int excludeCount = 0;
    struct dnaSeq *newList = NULL, *seq, *nextSeq = NULL;
    for (seq = sequences;  seq != NULL;  seq = nextSeq)
        {
        nextSeq = seq->next;
        if (hashLookup(excludedSeqs, seq->name))
            excludeCount++;
        else
            slAddHead(&newList, seq);
        }
    hashFree(&excludedSeqs);
    slReverse(&newList);
    sequences = newList;
    verbose(2, "Excluded %d sequences named in %s (%d sequences remaining including reference)\n",
            excludeCount, excludeFile, slCount(sequences));
    }
return sequences;
}

static struct dnaSeq *readSequences(char *faFile)
/* Read all sequences in faFile.  Make sure there are at least 2 sequences and that
 * all have the same length.  If a reference sequence is specified and is not the first
 * sequence in the file, then move the reference sequence to the head of the list. */
{
verbose(2, "Reading sequences from %s\n", faFile);
struct dnaSeq *sequences = faReadAllMixed(faFile);
int seqCount = slCount(sequences);
verbose(2, "Read %d sequences.\n", seqCount);

if (seqCount < 2)
    errAbort("faToVcf: expecting multiple sequences in %s but found only %d.", faFile, seqCount);

int seqSize = sequences->size;
struct dnaSeq *seq;
for (seq = sequences->next;  seq != NULL;  seq = seq->next)
    if (seq->size != seqSize)
        errAbort("faToVcf: first sequence in %s (%s) has size %d, but sequence %s has size %d. "
                 "All sequences must have the same size.",
                 faFile, sequences->name, seqSize, seq->name, seq->size);

char *refName = optionVal("ref", sequences->name);
if (differentString(sequences->name, refName))
    {
    verbose(2, "Using %s as reference.\n", refName);
    struct dnaSeq *seq;
    for (seq = sequences;  seq->next != NULL;  seq = seq->next)
        {
        if (sameString(seq->next->name, refName))
            {
            struct dnaSeq *ref = seq->next;
            seq->next = ref->next;
            ref->next = sequences;
            sequences = ref;
            break;
            }
        }
    if (differentString(sequences->name, refName))
        errAbort("Could not find -ref sequence '%s' in %s.", refName, faFile);
    }

sequences = removeExcludedSequences(sequences);
return sequences;
}

static void writeVcfHeader(FILE *outF, char *faFile, char *vcfFile,
                           struct dnaSeq *sequences, boolean includeRef, boolean includeGenotypes)
/* Write a simple VCF header with sequence names as genotype columns. */
{
fprintf(outF,
        "##fileformat=VCFv4.2\n"
        "##reference=%s:%s\n"
        "##source=faToVcf %s %s\n"  //#*** Should document options used.
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO",
        faFile, sequences->name, faFile, vcfFile);
if (includeGenotypes)
    {
    fprintf(outF, "\tFORMAT");
    struct dnaSeq *seqsForGt = (includeRef ? sequences : sequences->next), *seq;
    for (seq = seqsForGt;  seq != NULL;  seq = seq->next)
        fprintf(outF, "\t%s", seq->name);
    }
fprintf(outF, "\n");
}

static boolean *getMaskSites(char *maskSitesFile, struct dnaSeq *ref)
/* If -maskSites=file is given, parse sites to be masked from file (expecting VCF with 'mask' in
 * the filter column at reference positions to be masked) and return an array of booleans indexed
 * by reference position, TRUE for mask.  Otherwise return NULL. */
{
boolean *maskSites = NULL;
if (maskSitesFile)
    {
    int chromSize = strlen(ref->dna) - countChars(ref->dna, '-');
    AllocArray(maskSites, chromSize);
    struct lineFile *lf = lineFileOpen(maskSitesFile, TRUE);
    char *line;
    while (lineFileNext(lf, &line, NULL))
        {
        if (line[0] == '#')
            continue;
        char *words[9];
        int wordCount = chopTabs(line, words);
        lineFileExpectWords(lf, 8, wordCount);
        if (!isAllDigits(words[1]))
            lineFileAbort(lf, "Expected second column to contain numeric position but got '%s'",
                          words[1]);
        int pos = atol(words[1]);
        if (pos < 1 || pos > chromSize)
            {
            warn("Warning line %d of %s: "
                 "Expected second column to contain positions between 1 and %d but got %d",
                 lf->lineIx, lf->fileName, chromSize, pos);
            }
        else if (sameString(words[6], "mask"))
            maskSites[pos-1] = TRUE;
        }
    lineFileClose(&lf);
    }
return maskSites;
}

void faToVcf(char *faFile, char *vcfFile)
/* faToVcf - Convert a FASTA alignment file to VCF. */
{
struct dnaSeq *sequences = readSequences(faFile);
int seqCount = slCount(sequences);
int seqSize = sequences->size;
boolean includeRef = optionExists("includeRef");

verbose(2, "Writing VCF to %s\n", vcfFile);
FILE *outF = mustOpen(vcfFile, "w");
boolean noGenotypes = optionExists("noGenotypes");
writeVcfHeader(outF, faFile, vcfFile, sequences, includeRef, !noGenotypes);

struct dnaSeq *refSeq = sequences;
char *vcfChrom = optionVal("vcfChrom", refSeq->name);
int startOffset = optionInt("startOffset", 0);
boolean *maskSites = getMaskSites(optionVal("maskSites", NULL), refSeq);

struct dnaSeq *seqsForGt = (includeRef ? sequences : sequences->next);
int gtCount = includeRef ? seqCount : seqCount - 1;
int *genotypes = needMem(sizeof(int) * gtCount);
boolean *missing = needMem(sizeof(boolean) * gtCount);

int baseIx, chromStart = 0;
for (baseIx = 0, chromStart = 0;  baseIx < seqSize;  baseIx++, chromStart++)
    {
    char ref = toupper(refSeq->dna[baseIx]);
    if (ref == '-')
        {
        // Insertion into reference -- ignore and adjust reference coordinate.
        chromStart--;
        continue;
        }
    if (ref == 'N')
        continue;
    if (maskSites && maskSites[chromStart])
        continue;
    if (ref == 'U')
        ref = 'T';
    char altAlleles[MAX_ALTS];
    int altAlleleCounts[MAX_ALTS];
    int altCount = 0;
    memset(altAlleles, 0, sizeof(altAlleles));
    memset(genotypes, 0, sizeof(int) * gtCount);
    memset(missing, 0, sizeof(boolean) * gtCount);
    int nonNCount = seqCount;
    struct dnaSeq *seq;
    int gtIx;
    for (seq = seqsForGt, gtIx = 0;  seq != NULL;  seq = seq->next, gtIx++)
        {
        char al = toupper(seq->dna[baseIx]);
        if (al == 'U')
            al = 'T';
        if (isIupacAmbiguous(al))
            {
            if (optionExists("ambiguousToN"))
                al = 'N';
            else if (optionExists("resolveAmbiguous"))
                {
                char *bases = iupacAmbiguousToString(al);
                if (strlen(bases) > 2 ||
                    (toupper(bases[0]) != ref && toupper(bases[1]) != ref))
                    al = 'N';
                else if (toupper(bases[0]) == ref)
                    al = toupper(bases[1]);
                else
                    al = toupper(bases[0]);
                }
            }
        if (al == 'N' || al == '-')
            {
            missing[gtIx] = TRUE;
            nonNCount--;
            }
        else if (al != ref)
            {
            char *altFound = strchr(altAlleles, al);
            int altIx = 0;
            if (! altFound)
                {
                altIx = altCount++;
                if (altCount == MAX_ALTS)
                    errAbort("faToVcf: fasta base %d (reference base %d) has at least %d alternate alleles; increase "
                             "MAX_ALTS in faToVcf.c!", baseIx, chromStart, MAX_ALTS);
                altAlleleCounts[altIx] = 1;
                altAlleles[altIx] = al;
                }
            else
                {
                altIx = altFound - altAlleles;
                altAlleleCounts[altIx]++;
                }
            genotypes[gtIx] = altIx+1;
            }
        }
    int minAc = optionInt("minAc", 0);
    if (minAc > 0)
        {
        int oldToNewIx[altCount];
        char newAltAlleles[altCount];
        int newAltAlleleCounts[altCount];
        int newAltCount = 0;
        int altIx;
        for (altIx = 0;  altIx < altCount;  altIx++)
            {
            if (altAlleleCounts[altIx] >= minAc)
                {
                // This alt passes the filter.
                int newAltIx = newAltCount++;
                newAltAlleleCounts[newAltIx] = altAlleleCounts[altIx];
                newAltAlleles[newAltIx] = altAlleles[altIx];
                oldToNewIx[altIx] = newAltIx;
                }
            else
                {
                // We're removing this alt; count its genotypes as missing since they're not
                // really reference.
                for (gtIx = 0;  gtIx < gtCount;  gtIx++)
                    if (genotypes[gtIx] == altIx+1)
                        {
                        missing[gtIx] = TRUE;
                        nonNCount--;
                        }
                oldToNewIx[altIx] = -1;
                }
            }
        if (newAltCount != altCount)
            {
            for (gtIx = 0;  gtIx < gtCount;  gtIx++)
                {
                int oldGt = genotypes[gtIx];
                if (oldGt == 0)
                    genotypes[gtIx] = 0;
                else
                    genotypes[gtIx] = oldToNewIx[oldGt-1] + 1;
                }
            altCount = newAltCount;
            for (altIx = 0;  altIx < newAltCount;  altIx++)
                {
                altAlleles[altIx] = newAltAlleles[altIx];
                altAlleleCounts[altIx] = newAltAlleleCounts[altIx];
                }
            }
        }
    if (altCount)
        {
        int pos = chromStart + startOffset + 1;
        fprintf(outF, "%s\t%d\t", vcfChrom, pos);
        int altIx;
        for (altIx = 0;  altIx < altCount;  altIx++)
            fprintf(outF, "%s%c%d%c", (altIx > 0) ? "," : "",
                    ref, pos, altAlleles[altIx]);
        fprintf(outF, "\t%c\t", ref);
        for (altIx = 0;  altIx < altCount;  altIx++)
            {
            if (altIx > 0)
                fprintf(outF, ",");
            fprintf(outF, "%c", altAlleles[altIx]);
            }
        fprintf(outF, "\t.\t.\tAC=");
        for (altIx = 0;  altIx < altCount;  altIx++)
            {
            if (altIx > 0)
                fprintf(outF, ",");
            fprintf(outF, "%d", altAlleleCounts[altIx]);
            }
        fprintf(outF, ";AN=%d", nonNCount);
        if (!noGenotypes)
            {
            fputs("\tGT", outF);
            for (gtIx = 0;  gtIx < gtCount;  gtIx++)
                {
                if (missing[gtIx])
                    fprintf(outF, "\t.");
                else
                    fprintf(outF, "\t%d", genotypes[gtIx]);
                }
            }
        fprintf(outF, "\n");
        }
    }
carefulClose(&outF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (optionExists("ambiguousToN") && optionExists("resolveAmbiguous"))
    errAbort("-ambiguousToN and -resolveAmbiguous conflict with each other; "
             "please use only one.");
if (argc != 3)
    usage();
faToVcf(argv[1], argv[2]);
return 0;
}
