table gencodeAttrs
"Basic set of attributes associated with all Gencode transcripts."
    (
    string geneId; "Gene identifier"
    string geneName; "Gene name"
    string geneType; "BioType of gene"
    string transcriptId; "Transcript identifier"
    string transcriptName; "Transcript name"
    string transcriptType; "BioType of transcript"
    string ccdsId; "CCDS identifier if transcript is in CCDS"
    int level; "GENCODE level: 1 = experimental confirmed, 2 = manual, 3 = automated"
    string proteinId; "Protein identifier (not loaded on many older versions of GENCODE)"
    string transcriptClass; "high level type of transcript"
    )
