table omiciaAuto
"table for OMICIA auto-generated data"
    (
    ushort  bin;            "A field to speed indexing"
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  name;           "ID for this mutation"
    uint    score;          "confidence score"
    char[1] strand;         "+ or -"
    )

table omiciaHand
"table for OMICIA hand curated data"
    (
    ushort  bin;            "A field to speed indexing"
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  name;           "ID for this mutation"
    uint    score;          "confidence score"
    char[1] strand;         "+ or -"
    )

table omiciaLink
"links"
    (
    string id;              "id into the omicia composite table"
    string attrType;        "attribute type"
    string raKey;           "key into .ra file on how to do link"
    string acc;             "accession or id used by link"
    string displayVal;      "value to display if different from acc"
    )

table omiciaAttr
"attributes"
    (
    string id;              "id into the omicia composite table"
    string attrType;        "attribute type, label"
    string attrVal;         "value for this attribute"
    )

