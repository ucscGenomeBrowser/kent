table encodeExp
"ENCODE experiments"
    (
    uint ix;            "auto-increment ID"
    string organism;   "human or mouse"
    string lab;        "lab name from ENCODE cv.ra"
    string dataType;   "dataType from ENCODE cv.ra"
    string cellType;   "cellType from ENCODE cv.ra"
    string expVars;    "var=value list of experiment-defining variables. May be NULL if none."
    string accession;  "wgEncodeE[H|M]00000N or NULL if proposed but not yet approved"
    string updateTime; "auto-update timestamp"
    )
