table gbCharacteristic
"Characteristics of GenBank sequences referenced in the gbCdnaInfo table"
   (
   int    id;              "Unique id of characteristic (referenced by mrna table)"
   string name;            "Characteristic/value"
   int 	  crc;             "Cyclic redundancy check (performance optimization for loading tables)"
   )

table gbLoaded
"Release, updates and partitions loaded (cached to speed up loading process)"
    (
    enum srcDb;		"Source database: 'GenBank' or 'RefSeq'"
    enum type;		"Full length ('mRNA') or EST ('EST')"
    string loadRelease;	"release version that was loaded"
    string loadUpdate;	"update that was loaded (date or 'full')"
    string accPrefix;   "first two characters of accessions (or empty)"
    string time;	"time that this entry was inserted"
    ubyte extFileUpdated;	"true if extFile has been updated"
    )

table gbStatus
"GenBank version info for alignments in the database"
    (
    string acc;		"GenBank accession"
    short version;	"GenBank version number suffix"
    string modDate;	"last modified date"
    enum type;		"Full length ('mRNA') or EST ('EST')"
    enum srcDb;		"Source database: 'GenBank' or 'RefSeq'"
    enum orgCat;	"Organism category: this ('native') or other ('xeno')"
    uint gbSeq;		"ID/index in gbSeq table"
    uint numAligns;	"number of alignments of the accession"
    string seqRelease;	"release version where the sequence was obtained"
    string seqUpdate;	"update where sequence was obtained (date or 'full')"
    string metaRelease;	"release version where the metadata was obtained"
    string metaUpdate;	"update where metadata was obtained (date or 'full')"
    string extRelease;	"release version containing the external file"
    string extUpdate;	"update containing the external file (date or 'full')"
    string time;	"time that this entry was inserted"
    )

table mgcFullStatus
"Status of full-CDS MGC clones"
    (
    uint imageId;	"IMAGE ID for clone"
    enum status;	"MGC status code: ('unpicked','candidate','picked','notBack','noDecision','fullLength','fullLengthShort','fullLengthSynthetic','incomplete','chimeric','frameShift','contaminated','retainedIntron','mixedWells','noGrowth','noInsert','no5est','microDel','artifact','noPolyATail','cantSequence','inconsistentWithGene')"
    enum state;		"MGC state code: ('unpicked', 'pending', 'fullLength', 'problem')"
    string acc;		"GenBank accession"
    string organism;	"organism code"
    string geneName;	"RefSeq accession for gene, if available"
    )

table refSeqSummary
"Summary or completeness info for RefSeqs (when given in comments)"
    (
    string mrnaAcc;	"NM_* RefSeq mRNA accession"
    enum completeness;	"'Complete5End', 'Complete3End', 'FullLength', 'IncompleteBothEnds', 'Incomplete5End', 'Incomplete3End', 'Partial', 'Unknown'"
    string summary;	"Summary comments"
    )
