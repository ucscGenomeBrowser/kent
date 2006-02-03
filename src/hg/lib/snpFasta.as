table snpFasta
"Polymorphism data from dbSnp rs_fasta files"
    (
    string  rsId;       "Reference SNP identifier"
    string  chrom;	"Reference sequence chromosome or scaffold (can be 'multi')"
    string  molType;    "Sample type from exemplar ss"
    string  class;       "Single, in-del, heterozygous, microsatelite, named, etc."
    lstring  observed;   "The sequences of the observed alleles from rs-fasta files"
    lstring  leftFlank;  "Left flanking sequence"
    lstring  rightFlank; "Right flanking sequence"
    )
