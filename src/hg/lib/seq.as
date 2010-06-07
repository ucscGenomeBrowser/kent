table seq
"Information about sequences contained in files described in extFile"
    (
    uint id;		"ID/index"
    string acc;		"Accession of sequence"
    uint size;		"Size of sequence (number of bases)"
    string gb_date;	"unused"
    uint extFile;	"ID/index of file in extFile"
    bigint file_offset;	"byte offset of sequence in file"
    bigint file_size;	"byte size of sequence in file"
    )

