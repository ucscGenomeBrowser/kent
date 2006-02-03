table altProbe
"Little info about the probe sets for altGraphX."
(
    string chrom;             "Reference sequence chromosome or scaffold"
    int chromStart;          "Chromosome Start."
    int chromEnd;            "Chromosome End."
    int type;                "Type of splicing event 2==cassette."
    char[2] strand;          "Genomic Strand."
    string name;              "Name from altSplice clustering."
    int maxCounts;           "Maximum possible counts, used for  memory management."
    int contProbeCount;      "Number of constitutive probe sets."
    string[contProbeCount] contProbeSets;    "Names of constitutive probe sets."
    int alt1ProbeCount;      "Number of alternate1 probe sets."
    string[alt1ProbeCount] alt1ProbeSets;    "Names of alternate1 probe sets."
    int alt2ProbeCount;      "Number of alternate2 probe sets."
    string[alt2ProbeCount] alt2ProbeSets;    "Names of alternate2 probe sets."
    int transcriptCount;     "Number of altMerge transcripts involved."
    string[transcriptCount] transcriptNames;  "Names of altMerge transcripts."
)

