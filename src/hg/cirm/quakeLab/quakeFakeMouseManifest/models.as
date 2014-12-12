table quakeSample
"Sample from quake lab"
    (
    string date;  "DD/MM/YY format date string"
    string subdir; "Subdirectory where files live"
    string fluidPlateAcc; "Shorter 5 char accession corresponding to plate?"
    uint unknown1; "Seems to be number 1-8 with some gaps"
    string seqFormat; "Form like 151 (8) (8) 151 or 101 (6) 101"
    string sequencer; "Looks like a sequencing machine id"
    string sample;  "Looks like a unique ID for each sample"
    string user;  "User name"
    string lab; "Lab name"
    string ilAcc; "SOme sort of accession starting with IL"
    string captureArrayBarcode; "Seems to vary in lock-step with ilAcc"
    string cellPos; "Position of cell in plate I think"
    )

table quakeChip
"A microfluidics chip from quake lab"
    (
    string captureArrayBarcode;  "Long unique id"
    string date;  "MM/DD/YY format"
    string machineName; "Name of machine run on"
    string user; "User name"
    string lab; "Lab"
    string sampleType; "Overall type of cell"
    string tissue;  "Tissue or organ"
    string species;  "M. musculus, etc"
    string strain; "Black 6 etc"
    string age;  "Adult, etc"
    string targetCell;  "Epithelium, etc"
    string sortMethod; "FACS usually"
    string markers; "Space delimited markers like CD45 etc"
    string attiRun; "Seems to include data and other info"
    string description;  "Typically not very descriptive...."
    )

table quakeSheet
"SampleSheet file info"
    (
    string fcid;   "Flow cell ID"
    int lane;	"Sequencing lane - small number"
    string sampleId;  "Same as quakeSample.sample"
    string sampleRef;  "Always SampleRef?"
    string seqIndex;	"Nucleotides used to separate out multiplexing in lane"
    string description; "Looks like some sort of IL #"
    string control;  "N"
    string recipe;  "Recipe"
    string operator;  "GM?"
    string SampleProject;  "Looks like this actually has operator"
    )
