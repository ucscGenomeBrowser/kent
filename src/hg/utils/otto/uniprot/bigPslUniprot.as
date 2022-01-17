table bigPsl
"bigPsl pairwise alignment"
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "UniProt isoform seq. ID"
    uint score;         "Score (0-1000)"
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
    string   oCDS;       "CDS in NCBI format"

    uint    chromSize;"Size of target chromosome"

    uint match;        "Number of bases matched."
    uint misMatch; " Number of bases that don't match "
    uint repMatch; " Number of bases that match but are part of repeats "
    uint nCount;   " Number of 'N' bases "
    uint seqType;    "0=empty, 1=nucleotide, 2=amino_acid"

    lstring transList; "Mapped to genome through these transcripts"
    string acc; "UniProt record accession"
    lstring uniprotName; "UniProt record name"
    string status; "UniProt status"
    lstring accList; "UniProt previous and alternative accessions"
    lstring isoIds; "All UniProt sequence isoform accessions"

    lstring protFullNames; "UniProt protein name"
    lstring protShortNames; "UniProt protein short name"
    lstring protAltFullNames; "UniProt alternative names"
    lstring protAltShortNames; "UniProt alternative short names"
    lstring geneName; "UniProt gene name"
    lstring geneSynonyms; "UniProt gene synonyms"
    lstring functionText; "UniProt function"

    lstring hgncSym; "HGNC Gene Symbol"
    lstring hgncId; "HGNC IDs"
    lstring refSeq; "RefSeq Transcript IDs"
    lstring refSeqProt; "RefSeq Protein IDs"
    lstring entrezGene; "NCBI Gene IDs"
    lstring ensGene; "Ensembl Gene IDs"
    lstring ensProt; "Ensembl Protein IDs"
    lstring ensTrans; "Ensembl Transcript IDs"
    )

