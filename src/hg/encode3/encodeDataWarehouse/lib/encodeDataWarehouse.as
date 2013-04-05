
table edwUser
"Someone who submits files to or otherwise interacts with big data warehouse"
    (
    uint id;            "Autoincremented user ID"
    string name;        "user name"
    char[64] sid;       "sha384 generated user ID - used to identify user in secure way if need be"
    char[64] access;    "access code - sha385'd from password and stuff"
    string email;       "Email address - required"
    )

table edwHost
"A web host we have collected files from - something like www.ncbi.nlm.gov or google.com"
    (
    uint id;            "Autoincremented host id"
    string name;        "Name (before DNS lookup)"
    bigint lastOkTime;   "Last time host was ok in seconds since 1970"
    bigint lastNotOkTime;  "Last time host was not ok in seconds since 1970"
    bigint firstAdded;     "Time host was first seen"
    lstring errorMessage; "If non-empty contains last error message from host. If empty host is ok"
    bigint openSuccesses;  "Number of times files have been opened ok from this host"
    bigint openFails;      "Number of times files have failed to open from this host"
    bigint historyBits; "Open history with most recent in least significant bit. 0 for connection failed, 1 for success"
    )

table edwSubmitDir
"An external data directory we have collected a submit from"
    (
    uint id;            "Autoincremented id"
    lstring url;        "Web-mounted directory. Includes protocol, host, and final '/'"
    uint hostId;        "Id of host it's on"
    bigint lastOkTime;   "Last time submit dir was ok in seconds since 1970"
    bigint lastNotOkTime;  "Last time submit dir was not ok in seconds since 1970"
    bigint firstAdded;     "Time submit dir was first seen"
    lstring errorMessage; "If non-empty contains last error message from dir. If empty dir is ok"
    bigint openSuccesses;  "Number of times files have been opened ok from this dir"
    bigint openFails;      "Number of times files have failed to open from this dir"
    bigint historyBits; "Open history with most recent in least significant bit. 0 for upload failed, 1 for success"
    )

table edwFile
"A file we are tracking that we intend to and maybe have uploaded"
    (
    uint id;                    "Autoincrementing file id"
    char[16] licensePlate;      "A abc123 looking license-platish thing"
    uint submitId;              "Links to id in submit table"
    uint submitDirId;           "Links to id in submitDir table"
    lstring submitFileName;     "File name in submit relative to submit dir"
    lstring edwFileName;        "File name in big data warehouse relative to edw root dir"
    bigint startUploadTime;     "Time when upload started - 0 if not started"
    bigint endUploadTime;       "Time when upload finished - 0 if not finished"
    bigint updateTime;          "Update time (on system it was uploaded from)"
    bigint size;                "File size"
    char[32] md5;               "md5 sum of file contents"
    lstring tags;               "CGI encoded name=val pairs from manifest"
    lstring errorMessage; "If non-empty contains last error message from upload. If empty upload is ok"
    string deprecated; "If non-empty why you shouldn't user this file any more."
    string replacedBy; "If non-empty license plate of file that replaces this one."
    )

table edwSubmit
"A data submit, typically containing many files.  Always associated with a submit dir."
    (
    uint id;                 "Autoincremented submit id"
    lstring url;              "Url to validated.txt format file. We copy this file over and give it a fileId if we can." 
    bigint startUploadTime;   "Time at start of submit"
    bigint endUploadTime;     "Time at end of upload - 0 if not finished"
    uint userId;        "Connects to user table id field"
    uint submitFileId;       "Points to validated.txt file for submit."
    uint submitDirId;    "Points to the submitDir"
    uint fileCount;          "Number of files that will be in submit if it were complete."
    uint oldFiles;           "Number of files in submission that were already in warehouse."
    uint newFiles;           "Number of files in submission that are newly uploaded."
    lstring errorMessage; "If non-empty contains last error message. If empty submit is ok"
    )

table edwSubscriber
"Subscribers can have programs that are called at various points during data submission"
    (
    uint id;             "ID of subscriber"
    string name;         "Name of subscriber"
    double runOrder;     "Determines order subscribers run in. In case of tie lowest id wins."
    string filePattern;  "A string with * and ? wildcards to match files we care about"
    string dirPattern;   "A string with * and ? wildcards to match hub dir URLs we care about"
    lstring tagPattern;  "A cgi-encoded string of tag=wildcard pairs."
    string onFileEndUpload;     "A unix command string to run with a %u where file id goes"
    )

table edwAssembly
"An assembly - includes reference to a two bit file, and a little name and summary info."
    (
    uint id;    "Assembly ID"
    uint taxon; "NCBI taxon number"
    string name;  "Some human readable name to distinguish this from other collections of DNA"
    string ucscDb;  "Which UCSC database (mm9?  hg19?) associated with it."
    uint twoBitId;  "File ID of associated twoBit file"
    bigInt baseCount;  "Count of bases"
    )

table edwValidFile
"A file that has been uploaded, the format checked, and for which at least minimal metadata exists"
    (
    uint id;          "ID of validated file"
    char[16] licensePlate;  "A abc123 looking license-platish thing. Same as in edwFile table"
    uint fileId;      "Pointer to file in main file table"
    string format;    "What format it's in from manifest"
    string outputType; "What output_type it is from manifest"
    string experiment; "What experiment it's in from manifest"
    string replicate;  "What replicate it is from manifest"
    string validKey;  "The valid_key tag from manifest"
    string enrichedIn; "The enriched_in tag from manifest"
    string ucscDb;    "Something like hg19 or mm9"

    bigint itemCount; "# of items in file: reads for fastqs, lines for beds, bases w/data for wig."
    bigint basesInItems; "# of bases in items"
    string samplePath;  "Path to a temporary sample file"
    bigint sampleCount; "# of items in sample if we are just subsampling as we do for reads." 
    bigint basesInSample; "# of bases in our sample"
    double sampleCoverage; "Proportion of assembly covered by at least one item in sample"
    double depth;   "Estimated genome-equivalents covered by possibly overlapping data"
    )

table edwQaAgent
"A program plus parameters with a standard command line that gets run on new files"
    (
    uint id;         "ID of this agent"
    string name;     "Name of agent"
    string program;  "Program command line name"
    string options; "Program command line options"
    string deprecated; "If non-empty why it isn't run any more."
    )

table edwQaRun
"Records a bit of information from each QA run we've done on files."
    (
    uint id;          "ID of this run"
    uint agentId;     "ID of agent that made this run"
    uint startFileId; "ID of file we started on."
    uint endFileId;   "One past last file we did QA on"
    bigint startTime; "Start time in seconds since 1970"
    bigint endTime;   "Start time in seconds since 1970"
    lstring stderr;   "The output to stderr of the run"
    )

table edwQaEnrichTarget
"A target for our enrichment analysis."
    (
    uint id;    "ID of this enrichment target"
    uint assemblyId; "Which assembly this goes to"
    string name;  "Something like 'exon' or 'promoter'"
    uint fileId;    "A simple BED 3 format file that defines target. Bases covered are unique"
    bigint targetSize;  "Total number of bases covered by target"
    )

table edwQaEnrich
"An enrichment analysis applied to file."
    (
    uint id;    "ID of this enrichment analysis"
    uint fileId;  "File we are looking at skeptically"
    uint qaEnrichTargetId;  "Information about an target for this analysis"
    bigInt targetBaseHits;  "Number of hits to bases in target"
    bigInt targetUniqHits;  "Number of unique bases hit in target"
    double coverage;    "Coverage of target - just targetUniqHits/targetSize"
    double enrichment;  "Amount we hit target/amount we hit genome"
    double uniqEnrich;  "coverage/sampleCoverage"
    )

