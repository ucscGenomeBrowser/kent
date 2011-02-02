table encodeExp
"ENCODE experiments"
    (
    int ix;            "auto-increment ID"
    string organism;   "human | mouse"
    string lab;        "lab name from ENCODE cv.ra"
    string dataType;   "dataType from ENCODE cv.ra"
    string cellType;   "cellType from ENCODE cv.ra"
    string vars;       "RA of experiment-defining variables, defined per dataType"
    )
