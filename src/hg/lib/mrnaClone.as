table mrnaClone
"The name of the mRNA clone associated with a genbank sequence"
    (
    uint id;	"Unique numerical id"
    char name;	"Associated text"
    uint crc;   "Checksum of name, which is used to speedup the update of this table"
    )
