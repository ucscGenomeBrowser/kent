/* soterm.h --- Sequence Ontology terms and supporting data structures */

#ifndef SOTERM_H
#define SOTERM_H

enum 	// the various variant effects
    {
    regulatory_region_variant=1566,
    stop_retained_variant=1567,
    splice_acceptor_variant=1574,
    splice_donor_variant=1575,
    Complex_transcript_variant=1577,
    stop_lost=1578,
    coding_sequence_variant=1580,
    initiator_codon_variant=1582,
    missense=1583,
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
    inframe_deletion=1651,
    inframe_insertion=1652,
    TF_binding_site_variant=1782,
    non_coding_exon_variant=1792,
    non_synonymous_variant=1818,
    synonymous_variant=1819,
    } soTerm;

struct soCall  // a single variant effect call
    {
    struct soCall *next;
    uint    soNumber;           // Sequence Ontology Number
    union
	{
	struct codingChange     // (non)synonymous variant
	    {
	    char *transcript;
	    uint exonNumber;

	    // the next three should have a length specified too
	    uint cDnaPosition;
	    uint cdsPosition;
	    uint pepPosition;
	    char *aaChanges;
	    char *codonChanges;
	    } codingChange;
	struct     // intron_variant
	    {
	    char *transcript;
	    uint intronNumber;
	    } intron;
	struct    // a generic variant
	    {
	    char    *soOther0;           // Ancillary detail 0
	    char    *soOther1;           // Ancillary detail 1
	    char    *soOther2;           // Ancillary detail 2
	    char    *soOther3;           // Ancillary detail 3
	    char    *soOther4;           // Ancillary detail 4
	    char    *soOther5;           // Ancillary detail 5
	    char    *soOther6;           // Ancillary detail 6
	    } generic;
	} sub;
    };

#endif /* SOTERM_H */
