table seq
"Information about sequences contained in files described in extFile"
    (
    uint id;		"ID/index"
    string acc;		"Accession of sequence"
    uint size;		"Size of sequence (number of bases)"
    date gb_date;	"unused"
    uint extFile;	"ID/index of file in extFile"
    long file_offset;	"byte offset of sequence in file"
    uint file_size;	"byte size of sequence in file"
    )

