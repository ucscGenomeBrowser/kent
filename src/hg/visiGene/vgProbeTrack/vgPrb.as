table vgPrb
"Permanent Probes defining vgPrb_ id"
    (
    int id;             "ID of probeExt"
    string type;        "Type of probe  - probe,primersMrna,primersGenome,fPrimer,bac,acc,gene"
    lstring seq;        "Probe sequence, if any"
    string tName;       "mRNA Accession or a chromosome, if any"
    int tStart;         "Start of target, if any "
    int tEnd;           "End of target, if any"
    char[1] tStrand;    "+ or if reverse-complemented then -"
    string db;          "assembly/source used to define this sequence originally"
    int taxon;          "taxon+seq for unique identity"
    string state;       "use for multiple passes? e.g. new,isPcr,seq,etc"
    )

