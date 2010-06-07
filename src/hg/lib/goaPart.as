table goaPart
"Partial GO Association. Useful subset of goa"
    (
    string dbObjectId;	"Database accession - like 'Q13448'"
    string dbObjectSymbol;	"Name - like 'CIA1_HUMAN'"
    string notId;  "(Optional) If 'NOT'. Indicates object isn't goId"
    string goId;   "GO ID - like 'GO:0015888'"
    string aspect;  " P (process), F (function) or C (cellular component)"
    )
