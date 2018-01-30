/* soTerm - Sequence Ontology terms that we use for compatibility with Ensembl & others. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "soTerm.h"

struct soStringToId
// Map SO term string name to int ID
    {
    char *term;      // string name for term, e.g. "stop_lost"
    enum soTerm id;  // integer ID for term, e.g. 1578 for "SO:0001578"
    };

static struct soStringToId soStringToId[] = {
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
    { "no_sequence_alteration", no_sequence_alteration },
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
