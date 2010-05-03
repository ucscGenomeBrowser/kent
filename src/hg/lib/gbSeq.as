table gbSeq
"Information about sequences contained in files described in gbExtFile"
    (
    uint id;		"ID/index"
    string acc;		"Accession of sequence"
    int version;        "Genbank version"
    uint size;		"Size of sequence (number of bases)"
    uint gbExtFile;	"ID/index of file in gbExtFile"
    bigint file_offset;	"byte offset of sequence in file"
    bigint file_size;	"byte size of sequence in file"
    enum('EST','mRNA','PEP')  type;   "Type of sequence"
    enum('GenBank','RefSeq','Other') srcDb;  "Source database"
    )

