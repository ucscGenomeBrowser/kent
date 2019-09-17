/* soTerm.h --- a subset of locally used Sequence Ontology terms */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

// To explore the full tree of SO terms and their relationships, try the MISO browser:
// http://www.sequenceontology.org/browser/obob.cgi

// would be nice to more closely match Ensembl's subset of SO:
// http://uswest.ensembl.org/info/docs/variation/predicted_data.html#consequences

#ifndef SOTERM_H
#define SOTERM_H

enum soTerm	// the various variant effects
    {
    soUnknown=0,
    frameshift=865,
    regulatory_region_variant=1566,
    stop_retained_variant=1567,
    exon_loss_variant=1572,
    splice_acceptor_variant=1574,
    splice_donor_variant=1575,
    complex_transcript_variant=1577,
    stop_lost=1578,
    coding_sequence_variant=1580,
    initiator_codon_variant=1582,
    missense_variant=1583,
    stop_gained=1587,
    frameshift_variant=1589,
    terminator_codon_variant=1590,
    nc_transcript_variant=1619,
    mature_miRNA_variant=1620,
    NMD_transcript_variant=1621,
    UTR_variant=1622,
    _5_prime_UTR_variant=1623,
    _3_prime_UTR_variant=1624,
    incomplete_terminal_codon_variant=1626,
    intron_variant=1627,
    intergenic_variant=1628,
    splice_site_variant=1629,
    splice_region_variant=1630,
    upstream_gene_variant=1631,
    downstream_gene_variant=1632,
    TF_binding_site_variant=1782,
    non_coding_transcript_exon_variant=1792,
    protein_altering_variant=1818,
    synonymous_variant=1819,
    inframe_indel=1820,
    inframe_insertion=1821,
    inframe_deletion=1822,
    feature_variant=1878,
    transcript_ablation=1893,
    upstream_transcript_variant=1986,
    downstream_transcript_variant=1987,
    no_sequence_alteration=2073,
    genic_downstream_transcript_variant=2152,
    genic_upstream_transcript_variant=2153,
    };

char *soTermToString(enum soTerm termNumber);
/* Translate termNumber to its string equivalent; errAbort if not found.
 * Do not modify or free result. */

int soTermStringToId(char *soTermStr);
/* Translate soTermStr into its numeric ID.  Return -1 if soTermStr is not recognized. */

enum soTerm soTermStringIdToId(char *soIdStr);
/* Given a string like "SO:0001627", parse out the numeric ID and convert to enum soTerm. */

char *soTermToMisoLink(enum soTerm term);
/* Return an HTML <a> link to the MISO browser page for term
 * (except if it's soUnknown, just return text not a link). */

int soTermCmp(const void *a, const void *b);
/* Compare two enum soTerms for sorting by descending order of functional impact. */

#endif /* SOTERM_H */
