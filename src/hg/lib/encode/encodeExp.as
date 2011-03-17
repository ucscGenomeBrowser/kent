table encodeExp
"ENCODE experiments"
    (
    int ix;            "auto-increment ID"
    string organism;   "human | mouse"
    string accession;  "wgEncodeE[H|M]00000N"
    string lab;        "lab name from ENCODE cv.ra"
    string dataType;   "dataType from ENCODE cv.ra"
    string cellType;   "cellType from ENCODE cv.ra"
    string factors;    "var=value list of experiment-defining variables"
    string lastUpdated; "auto-update timestamp"
    )
