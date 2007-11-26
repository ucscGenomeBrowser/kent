table bToBeCfg 
"Helper to convert bed files to bed+exp file."
    (
    string factor;	"Protein factor, antibody, etc."
    string source;	"Cell line, tissue, etc."
    string sourceId;	"Short ID (one letter usually) for source"
    string dataSource;	"Either 'file' or a database name"
    string dataType;	"Either 'bedScore' or 'bedGraph'"
    float multiplier;	"Multiply by this to get score close to 0-1 range."
    string dataTable;	"File name or table name"
    )
