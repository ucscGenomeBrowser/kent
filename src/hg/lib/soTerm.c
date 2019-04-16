/* soTerm - Sequence Ontology terms that we use for compatibility with Ensembl & others. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dystring.h"
#include "soTerm.h"

#define SO_PREFIX "SO:"
#define MISO_URL_BASE "http://www.sequenceontology.org/browser/current_svn/term/"

struct soStringToId
// Map SO term string name to int ID
    {
    char *term;      // string name for term, e.g. "stop_lost"
    enum soTerm id;  // integer ID for term, e.g. 1578 for "SO:0001578"
    };

static struct soStringToId soStringToId[] = {
    { "unknown", soUnknown },
    { "frameshift", frameshift },
    { "regulatory_region_variant", regulatory_region_variant },
    { "stop_retained_variant", stop_retained_variant },
    { "exon_loss_variant", exon_loss_variant },
    { "splice_acceptor_variant", splice_acceptor_variant },
    { "splice_donor_variant", splice_donor_variant },
    { "complex_transcript_variant", complex_transcript_variant },
    { "stop_lost", stop_lost },
    { "coding_sequence_variant", coding_sequence_variant },
    { "initiator_codon_variant", initiator_codon_variant },
    { "missense_variant", missense_variant },
    { "stop_gained", stop_gained },
    { "frameshift_variant", frameshift_variant },
    { "terminator_codon_variant", terminator_codon_variant },
    { "nc_transcript_variant", nc_transcript_variant },
    { "mature_miRNA_variant", mature_miRNA_variant },
    { "NMD_transcript_variant", NMD_transcript_variant },
    { "UTR_variant", UTR_variant },
    { "5_prime_UTR_variant", _5_prime_UTR_variant },
    { "3_prime_UTR_variant", _3_prime_UTR_variant },
    { "incomplete_terminal_codon_variant", incomplete_terminal_codon_variant },
    { "intron_variant", intron_variant },
    { "intergenic_variant", intergenic_variant },
    { "splice_site_variant", splice_site_variant },
    { "splice_region_variant", splice_region_variant },
    { "upstream_gene_variant", upstream_gene_variant },
    { "downstream_gene_variant", downstream_gene_variant },
    { "TF_binding_site_variant", TF_binding_site_variant },
    { "non_coding_transcript_exon_variant", non_coding_transcript_exon_variant },
    { "protein_altering_variant", protein_altering_variant },
    { "synonymous_variant", synonymous_variant },
    { "inframe_indel", inframe_indel },
    { "inframe_insertion", inframe_insertion },
    { "inframe_deletion", inframe_deletion },
    { "feature_variant", feature_variant },
    { "transcript_ablation", transcript_ablation },
    { "upstream_transcript_variant", upstream_transcript_variant },
    { "downstream_transcript_variant", downstream_transcript_variant },
    { "no_sequence_alteration", no_sequence_alteration },
    { "genic_downstream_transcript_variant", genic_downstream_transcript_variant },
    { "genic_upstream_transcript_variant", genic_upstream_transcript_variant },
    { NULL, 0 }
};

char *soTermToString(enum soTerm termNumber)
/* Translate termNumber to its string equivalent; errAbort if not found.
 * Do not modify or free result. */
{
int i;
for (i = 0;  soStringToId[i].term != NULL;  i++)
    {
    if (termNumber == soStringToId[i].id)
        return soStringToId[i].term;
    }
errAbort("soTermToString: don't recognize term %u", termNumber);
return NULL;  // never get here
}

int soTermStringToId(char *soTermStr)
/* Translate soTermStr into its numeric ID.  Return -1 if soTermStr is not recognized. */
{
if (isEmpty(soTermStr))
    return -1;
int i;
for (i = 0;  soStringToId[i].term != NULL;  i++)
    {
    if (sameString(soTermStr, soStringToId[i].term))
        return soStringToId[i].id;
    }
return -1;
}

enum soTerm soTermStringIdToId(char *soIdStr)
/* Given a string like "SO:0001627", parse out the numeric ID and convert to enum soTerm. */
{
int id = 0;
if (startsWith(SO_PREFIX, soIdStr))
    {
    char *numPart = soIdStr + strlen(SO_PREFIX);
    if (!isAllDigits(numPart))
        errAbort("soTermStringToId: expected '"SO_PREFIX"' followed by a number, but got '%s'",
                 soIdStr);
    else
        id = atoi(numPart);
    }
else
    errAbort("soTermStringToId: expected string starting with '"SO_PREFIX"' but got '%s'", soIdStr);
return (enum soTerm)id;
}

char *soTermToMisoLink(enum soTerm term)
/* Return an HTML <a> link to the MISO browser page for term
 * (except if it's soUnknown, just return text not a link). */
{
char *link = NULL;
if (term == soUnknown)
    link = cloneString(soTermToString(term));
else
    {
    struct dyString *dy = dyStringCreate("<a href='" MISO_URL_BASE SO_PREFIX "%07d' "
                                         "target=_blank>%s</a>", term, soTermToString(term));
    link = dyStringCannibalize(&dy);
    }
return link;
}

// Ranking of functional impact, highest first, adapted from recommendations in
// http://snpeff.sourceforge.net/VCFannotationformat_v1.0.pdf :
enum soTerm funcImpactOrder[] =
    {
    transcript_ablation,
    exon_loss_variant,
    frameshift_variant,
    frameshift,
    initiator_codon_variant,
    stop_gained,
    stop_lost,
    splice_acceptor_variant,
    splice_donor_variant,
    splice_site_variant,
    inframe_insertion,
    inframe_deletion,
    inframe_indel,
    missense_variant,
    splice_region_variant,
    incomplete_terminal_codon_variant,
    terminator_codon_variant,
    protein_altering_variant,
    synonymous_variant,
    stop_retained_variant,
    coding_sequence_variant,
    _5_prime_UTR_variant,
    _3_prime_UTR_variant,
    UTR_variant,
    complex_transcript_variant,
    // Moved nc above up/downstream for bigDbSnp coloring (nc is colored like UTR),
    // while Pablo et al's doc has it almost at the bottom....
    nc_transcript_variant,
    genic_upstream_transcript_variant,
    upstream_transcript_variant,
    upstream_gene_variant,
    genic_downstream_transcript_variant,
    downstream_transcript_variant,
    downstream_gene_variant,
    TF_binding_site_variant,
    regulatory_region_variant,
    mature_miRNA_variant,
    feature_variant,
    intron_variant,
    intergenic_variant,
    non_coding_transcript_exon_variant,
    NMD_transcript_variant,
    no_sequence_alteration,
    soUnknown,
    };

INLINE int getFuncImpactRank(enum soTerm id)
/* Return the index of id in a sequence ordered by descending functional impact. */
{
int i;
for (i = 0;  i < ArraySize(funcImpactOrder);  i++)
    if (id == funcImpactOrder[i])
        return i;
errAbort("soTermCmp: unrecognized soTerm SO:%d", id);
return -1;
}

int soTermCmp(const void *a, const void *b)
/* Compare two enum soTerms for sorting by descending order of functional impact. */
{
enum soTerm id1 = *(enum soTerm *)a;
enum soTerm id2 = *(enum soTerm *)b;
int impact1 = getFuncImpactRank(id1);
int impact2 = getFuncImpactRank(id2);
return impact1 - impact2;
}
