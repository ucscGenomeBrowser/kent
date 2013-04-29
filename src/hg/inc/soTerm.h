/* soTerm.h --- a subset of locally used Sequence Ontology terms */

// To explore the full tree of SO terms and their relationships, try the MISO browser:
// http://www.sequenceontology.org/browser/obob.cgi

// would be nice to more closely match Ensembl's subset of SO:
// http://uswest.ensembl.org/info/docs/variation/predicted_data.html#consequences

#ifndef SOTERM_H
#define SOTERM_H

enum soTerm	// the various variant effects
    {
    regulatory_region_variant=1566,
    stop_retained_variant=1567,
    splice_acceptor_variant=1574,
    splice_donor_variant=1575,
    complex_transcript_variant=1577,
    stop_lost=1578,
    coding_sequence_variant=1580,
    initiator_codon_variant=1582,
    missense_variant=1583,
    stop_gained=1587,
    frameshift_variant=1589,
    nc_transcript_variant=1619,
    mature_miRNA_variant=1620,
    NMD_transcript_variant=1621,
    _5_prime_UTR_variant=1623,
    _3_prime_UTR_variant=1624,
    incomplete_terminal_codon_variant=1626,
    intron_variant=1627,
    intergenic_variant=1628,
    splice_region_variant=1630,
    upstream_gene_variant=1631,
    downstream_gene_variant=1632,
    TF_binding_site_variant=1782,
    non_coding_exon_variant=1792,
    protein_altering_variant=1818,
    synonymous_variant=1819,
    inframe_insertion=1821,
    inframe_deletion=1822,
    };

char *soTermToString(enum soTerm termNumber);
/* Translate termNumber to its string equivalent.  Do not modify or free result. */

#endif /* SOTERM_H */
