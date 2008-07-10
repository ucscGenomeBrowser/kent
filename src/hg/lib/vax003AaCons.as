table vax003AaCons
"VAX003 HIV-1 Protein Sequence Conservation"
    (
    string chrom;         "Reference sequence chromosome or scaffold"
    uint chromStart;      "Start position in chromosome"
    uint chromEnd;        "End position in chromosome"
    string name;          "Name of item"
    uint span;            "each value spans this many bases"
    uint count;           "number of values in this block"
    uint offset;          "offset in File to fetch data"
    string file;          "path name to data file, one byte per value"
    double lowerLimit;    "lowest data value in this block"
    double dataRange;     "lowerLimit + dataRange = upperLimit"
    uint validCount;      "number of valid data values in this block"
    double sumData;       "sum of the data points, for average and stddev calc"
    double sumSquares;    "sum of data points squared, for stddev calc"
    )
