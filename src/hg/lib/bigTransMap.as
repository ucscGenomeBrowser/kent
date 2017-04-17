table bigTransMap
"bigPsl derived pairwise alignment with additional information"
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "alignment Id"
    uint score;         "Score (0-1000), faction identity * 1000"
    char[1] strand;     "+ or - indicates whether the query aligns to the + or - strand on the reference"
    uint thickStart;    "Start of where display should be thick (start codon)"
    uint thickEnd;      "End of where display should be thick (stop codon)"
    uint reserved;       "RGB value (use R,G,B string in input file)"
    int blockCount;     "Number of blocks"
    int[blockCount] blockSizes; "Comma separated list of block sizes"
    int[blockCount] chromStarts; "Start positions relative to chromStart"

    uint    oChromStart;"Start position in other chromosome"
    uint    oChromEnd;  "End position in other chromosome"
    char[1] oStrand;    "+ or -, - means that psl was reversed into BED-compatible coordinates" 
    uint    oChromSize; "Size of other chromosome."
    int[blockCount] oChromStarts; "Start positions relative to oChromStart or from oChromStart+oChromSize depending on strand"

    lstring  oSequence;  "Sequence on other chrom (or edit list, or empty)"
    lstring  oCDS;       "CDS in NCBI format"

    uint    chromSize;"Size of target chromosome"

    uint match;        "Number of bases matched."
    uint misMatch; " Number of bases that don't match "
    uint repMatch; " Number of bases that match but are part of repeats "
    uint nCount;   " Number of 'N' bases "
    uint seqType;    "0=empty, 1=nucleotide, 2=amino_acid"
    string srcDb;   "source database"
    string srcTransId; "source transcript id"
    string srcChrom;  "source chromosome"
    uint srcChromStart; "start position in source chromosome"
    uint srcChromEnd; "end position in source chromosome"
    uint srcIdent;    "source score (fraction identity * 1000)
    uint srcAligned;   "fraction of source transcript aligned (fraction aligned * 1000)
    string geneName;  "gene name"
    string geneId;  "gene id"
    string geneType; "gene type"
    string transcriptType; "transcript type"
    string chainType;  "type of chains used for mapping"
    string commonName; "common name"
    string scientificName; "scientific name"
    string orgAbbrev;  "organism abbreviation"
    )

