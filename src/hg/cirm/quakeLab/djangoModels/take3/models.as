table SequencerModel
"SequencerModel needs documentation"
    (
    uint id; "Autoincrement unique ID for this row"
    string name; "sequencer model name"
    )

table SequencerInstrument
"SequencerInstrument needs documentation"
    (
    uint id; "Autoincrement unique ID for this row"
    string name; "sequencer id name"
    uint model; "Foreign key (id) in SequencerModel table"
    string serialnumber; "serial number reported in run name"
    string location; "location"
    )

table RunType
"RunType needs documentation"
    (
    uint id; "Autoincrement unique ID for this row"
    string name; "run type"
    int read1cycles; "needs comment"
    int read2cycles; "needs comment"
    int index1cycles; "needs comment"
    int index2cycles; "needs comment"
    )

table Run
"Run needs documentation"
    (
    uint id; "Autoincrement unique ID for this row"
    string name; "Run ID"
    string flowcellid; "flow cell ID"
    string date; "Run start date"
    uint instrument; "Foreign key (id) in SequencerInstrument table"
    uint runtype; "Foreign key (id) in RunType table"
    string sequenced; "sequenced?"
    string cirmproject; "Associated with CIRM Cell Atlas Project"
    string cirmxfer; "Transferred to CIRM Cell Atlas Repository"
    string path; "path to run folder"
    )

table BarcodeSet
"BarcodeSet needs documentation"
    (
    uint id; "Autoincrement unique ID for this row"
    string name; "name for barcode set"
    )

table Barcode
"Barcode needs documentation"
    (
    uint id; "Autoincrement unique ID for this row"
    string name; "core unique name for barcode"
    uint barcodeset; "Foreign key (id) in BarcodeSet table"
    string index1; "first index sequence"
    string index2; "second index sequence"
    string index2rc; "reverse complement of index2, currently required by NextSeq"
    string dualindex; "legacy combined index separated by \"-\""
    string adapter1; "first complete adapter sequence containing index"
    string adapter2; "second complete adapter sequence containing index"
    )

table CoreLibraryID
"CoreLibraryID needs documentation"
    (
    uint id; "Autoincrement unique ID for this row"
    string name; "core IL or IM name"
    lstring description; "Long Library description"
    )

table LibrarySample
"LibrarySample needs documentation"
    (
    uint id; "Autoincrement unique ID for this row"
    string name; "user-provided descriptive sample name"
    uint libraryid; "Foreign key (id) in CoreLibraryID table"
    string index1; "first index sequence"
    string index2; "second index sequence"
    uint barcode; "Foreign key (id) in Barcode table"
    lstring description; "Long sample description"
    uint chipid; "Foreign key (id) in 'cellcapture.CaptureArray' table"
    uint chipsite; "Foreign key (id) in 'cellcapture.ChipSite' table"
    uint user; "Foreign key (id) in 'cellcapture.User' table"
    )

table RunEntry
"RunEntry needs documentation"
    (
    uint id; "Autoincrement unique ID for this row"
    uint run; "Foreign key (id) in Run table"
    int lane; "lane number"
    uint sample; "Foreign key (id) in LibrarySample table"
    uint user; "Foreign key (id) in 'cellcapture.User' table"
    uint pi; "Foreign key (id) in 'cellcapture.PI' table"
    string toanalyze; "Perform automated analysis"
    )

