
table knownAttrs
"Fields in Gencode attrs table that aren't in kgXref"
    (
    string kgID;            "Known Gene ID"
    string geneId;          "ENSG* locus identifier"
    string geneStatus;      "KNOWN or NOVEL"
    string geneType;        "gene function"
    string transcriptName;  "transcript Name"
    string transcriptType;  "transcript Type"
    string transcriptStatus;"KNOWN or NOVEL or PUTATIVE"
    string havanaGeneId;    "Havana project Id"
    string ccdsId;          "CCDS project Id"
    string supportLevel;    "transcript support level"
    string transcriptClass; "pseudo, nonCoding, coding, or problem"
    )
