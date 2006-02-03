table bacCloneXRef
"BAC clones and corresponding STS information"
    (
    string name;                "External name for BAC clone"
    string intName;             "Internal Sanger FPC name"
    string chroms;              "Reference sequence chromosome(s) or scaffold(s) to which at one or both BAC ends are mapped by BLAT"
    string genbank;             "Genbank accession for the BAC Clone"
    string sangerName;          "Sanger STS name"
    uint relationship;          "Relationship type - method of finding STS"
    string uniStsId;            "UniSTS ID(s) for STS"
    string leftPrimer;          "STS 5' primer sequence"
    string rightPrimer;         "STS 3' primer sequence"
    )
