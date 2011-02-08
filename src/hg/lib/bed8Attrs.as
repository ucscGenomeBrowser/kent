table bed8Attrs
"Items with thin (outer) and/or thick (inner) regions and an arbitrary set of attributes"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000 (might not be applicable)"
    char[1] strand;    "+, - or . for unknown"
    uint thickStart;   "Start of where display should be thick"
    uint thickEnd;     "End of where display should be thick"
    int attrCount;     "Number of attributes"
    string[attrCount] attrTags; "Attribute tags/keys"
    string[attrCount] attrVals; "Attribute values"
    )
