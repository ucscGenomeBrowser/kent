table geneName
"The gene name of a genbank sequence"
    (
    uint id;	"Unique numerical id"
    char name;	"Associated text"
    uint crc;   "Checksum of name, which is used to speedup the update of this table"
    )
