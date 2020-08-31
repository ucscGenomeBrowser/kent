table gencodeAttrs
"Basic set of attributes associated with all Gencode transcripts."
    (
    string geneId; "Gene identifier"
    string geneName; "Gene name"
    string geneType; "BioType of gene"
    string unused1; "unused (was geneStatus in wgGencode tracks)"
    string transcriptId; "Transcript identifier"
    string transcriptName; "Transcript name"
    string transcriptType; "BioType of transcript"
    string unused2; "unused (was transcriptStatus in wgGencode tracks)"
    string unused3; "unused (was havanaGeneId in wgGencode tracks)"
    string unused4; "unused (was havanaTranscriptId in wgGencode tracks)"
    string ccdsId; "CCDS identifier if transcript is in CCDS"
    int level; "GENCODE level: 1 = experimental confirmed, 2 = manual, 3 = automated"
    string transcriptClass; "high level type of transcript"
    string proteinId; "Protein identifier (not loaded on many older versions of GENCODE)"
    )
