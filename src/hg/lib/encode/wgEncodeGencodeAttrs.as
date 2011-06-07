table wgEncodeGencodeAttrs
"Basic set of attributes associated with all Gencode transcripts."
    (
    string geneId; "Gene identifier"
    string geneName; "Gene name"
    string geneType; "BioType of gene"
    string geneStatus; "Status of gene"
    string transcriptId; "Transcript identifier"
    string transcriptName; "Transcript name"
    string transcriptType; "BioType of transcript"
    string transcriptStatus; "Status of transcript"
    string havanaGeneId; "HAVANA identifier if gene is in HAVANA"
    string havanaTranscriptId; "HAVANA identifier if transcript is in HAVANA"
    string ccdsId; "CCDS identifier if transcript is in CCDS"
    int level; "GENCODE level: 1 = experimental confirmed, 2 = manual, 3 = automated"
    string transcriptClass; "high level type of transcript"
    )
