/* soTerm.h --- Sequence Ontology terms and supporting data structures */

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

struct soCall  // a single variant effect call
    {
    struct soCall *next;
    char *transcript;		// ID of feature affected by this call
    uint    soNumber;           // Sequence Ontology Number of effect
    union
	{
	struct codingChange     // (non)synonymous variant, deletions in CDS
	    {
	    uint exonNumber;

	    // the next three should have a length specified too
	    uint cDnaPosition;		// offset of variant in transcript cDNA
	    uint cdsPosition;		// offset of variant from transcript's cds start
	    uint pepPosition;		// offset of variant in translated product
	    char *aaOld;		// peptides, before change by variant (starting at pepPos)
	    char *aaNew;		// peptides, changed by variant
	    char *codonOld;		// codons, before change by variant (starting at cdsPos)
	    char *codonNew;		// codons, changed by variant
	    } codingChange;
	struct     // intron_variant
	    {
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
	    char    *soOther7;           // Ancillary detail 7
	    } generic;
	} sub;
    };

char *soTermToString(enum soTerm termNumber);
/* Translate termNumber to its string equivalent.  Do not modify or free result. */

#endif /* SOTERM_H */
