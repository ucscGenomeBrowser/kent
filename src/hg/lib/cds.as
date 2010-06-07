table cds
"The coding region of a genbank sequence"
    (
    uint id;	"Unique numerical id"
    char name;	"Associated text.  Usually start..end.  May be more complex."
    uint crc;   "Checksum of name, which is used to speedup the update of this table"
    )
