table dbSnpDetails
"dbSNP annotations that are too lengthy to include in bigDbSnp file; for variant details page"
    (
    string    name;            "rs# ID of variant"
    int freqSourceCount;       "Number of frequency sources"
    string[freqSourceCount] alleleCounts;  "Array of each source's |-sep list of allele:count"
    int[freqSourceCount] alleleTotals; "Array of each source's total number of chromosomes sampled; may be > sum of observed counts and differ across variants."
    int soTermCount;           "Number of distinct SO terms annotated on variant"
    int[soTermCount] soTerms;  "SO term numbers annotated on RefSeq transcripts"
    int clinVarCount;          "Number of ClinVar accessions associated with variant"
    string[clinVarCount] clinVarAccs;  "ClinVar accessions associated with variant"
    string[clinVarCount] clinVarSigs;  "ClinVar significance for each accession"
    int submitterCount;                "Number of organizations/projects that reported variant"
    string[submitterCount] submitters; "dbSNP 'handles' of submitting orgs/projects"
    int pubMedIdCount;         "Number of PubMed-indexed publications associated with variant"
    int [pubMedIdCount]pubMedIds;      "PMIDs of associated publications"
    )
