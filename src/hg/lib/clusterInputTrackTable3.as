table clusterInputTrackTable3
"Some information on tracks used as input for a clustering track"
    (
    string tableName;	"Name of table used as an input"
    struct source;	"Name of source - linked to Exps table.  Cell+treatment.
    string factor;	"Name of factor/antibody"
    string cell;	"Name of cell type"
    string treatment;	"Drug or other treatment given to cells while alive"
    )
