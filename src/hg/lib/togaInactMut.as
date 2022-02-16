table TOGAInactMut
"Inactivating Mutations"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"

    int exon_num;         "exon number"
    int position;         "possition where mutation happened"
    string mut_class;     "mutation class such as FS deletion"
    string mutation;      "what exactly happened"
    ubyte is_inact;       "is this mutation inactivating, yes 1 or not 0"
    string mut_id;        "mut identifier"
    )
