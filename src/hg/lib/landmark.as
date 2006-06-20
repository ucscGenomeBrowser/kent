table landmark
"track for regulatory regions and other landmarks from locus experts"
    (
    ushort  bin;            "A field to speed indexing"
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  name;           "name of landmark"
    uint    landmarkId;     "unique ID to identify this landmark"
    string  landmarkType;   "type of landmark: HSS, promoter, .."
    )

table landmarkAttr 
"attributes associated with a landmark"
    (
    uint    landmarkId;     "key into the landmark table"
    uint    linkId;         "key into the landmarkAttrLink table"
    string  attribute;      "name of attribute being listed"
    string  attrVal;        "value of this landmark attribute"
    )

table landmarkAttrLink
"table to build links for landmarks"
    (
    uint    attrId;         "key into the landmarkAttr table"
    string  raKey;          "key into RA file, tells how to link"
    string  attrAcc;        "accession used by link"
    string  displayVal;     "how to display if different from acc"
    )
