table oreganno
"track for regulatory regions from ORegAnno"
    (
    ushort  bin;            "A field to speed indexing"
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  id;             "unique ID to identify this regulatory region"
    char[1] strand;         "+ or -"
    string  name;           "name of regulatory region"
    )

table oregannoAttr
"attributes associated with an ORegAnno regulatory region"
    (
    string  id;             "key into the oreganno table"
    string  attribute;      "name of attribute being listed"
    string  attrVal;        "value of this oreganno attribute"
    )

table oregannoLink
"links for ORegAnno regulatory region"
    (
    string  id;             "key into the oreganno table"
    string  attribute;      "name of attribute being listed"
    string  raKey;          "key into RA file, tells how to link"
    string  attrAcc;        "accession used by link"
    )
