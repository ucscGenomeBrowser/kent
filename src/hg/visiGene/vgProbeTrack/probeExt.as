table probeExt
"Probe table Extended"
    (
    int id;             "ID of probeExt"
    string type;        "Type of probe  - probe,primersMrna,primersGenome,fPrimer,bac,acc,gene"
    int probe;          "Associated probe"
    lstring seq;        "Probe sequence, if any"
    string tName;       "mRNA Accession or a chromosome, if any"
    int tStart;         "Start of target, if any "
    int tEnd;           "End of target, if any"
    char[1] tStrand;    "+ or if reverse-complemented then -"
    string state;       "use for multiple passes? e.g. new,isPcr,ok,failed"
    )

