table mrna
"Basic information about mRNA and ESTs with links to other tables"
   (
   uint   id;              "Id, same as seq ID"
   string acc;             "GenBank accession"
   int    version;         "GenBank version"
   date   moddate;         "Last modification date"
   enum   type;            "Full length ('mRNA') or EST ('EST')"
   enum   direction;       "Read direction ('5','3','0')"
   uint   source;          "Ref in source table"
   uint   organism;        "Ref in organism table"
   uint   library;         "Ref in library table"
   uint   mrnaClone;       "Ref in clone table"
   uint   sex;             "Ref in sex table"
   uint   tissue;          "Ref in tissue table"
   uint   development;     "Ref in development table"
   uint   cell;            "Ref in cell table"
   uint   cds;             "Ref in CDS table"
   uint   keyword;         "Ref in key table"
   uint   description;     "Ref in description table"
   uint   geneName;        "Ref in geneName table"
   uint   productName;     "Ref in productName table"
   uint   author;          "Ref in author table"
   )


table mrnaCharacteristic
"Characteristics of mRNA and EST referenced in the mrna table"
   (
   int    id;              "Unique id of characteristic (referenced by mrna table)"
   string name;            "Characteristic/value"
   int 	  crc;             "Cyclic redundancy check (performance optimization for loading tables)"
   )

