table edwAnalysisJob
"An analysis pipeline job to be run asynchronously and not too many all at once."
    (
    uint id primary auto;    "Job id"
    lstring commandLine; "Command line of job"
    bigInt startTime index; "Start time in seconds since 1970"
    bigInt endTime index; "End time in seconds since 1970"
    lstring stderr; "The output to stderr of the run - may be nonempty even with success"
    int returnCode; "The return code from system command - 0 for success"
    int pid;	"Process ID for running processes"
    int cpusRequested; "Number of CPUs to request from job control system"
    string parasolId index[12];	"Parasol job id for process." 
    )

table edwAnalysisSoftware
"Software that is tracked by the analysis pipeline."
    (
    uint id primary auto;  "Software id"
    string "name" unique; "Command line name"
    lstring "version"; "Current version"
    char[32] md5; "md5 sum of executable file"
    )

table edwAnalysisStep
"A step in an analysis pipeline - something that takes one file to another"
    (
    uint id primary auto; "Step id"
    string "name" unique;  "Name of this analysis step"
    int softwareCount;  "Number of pieces of software used in step"
    string[softwareCount] software; "Names of software used. First is the glue script"
    int cpusRequested; "Number of CPUs to request from job control system"
    )

table edwAnalysisRun
"Information on an analysis job that we're planning on running"
    (
    uint id primary auto; "Analysis run ID"
    uint jobId;  "ID in edwAnalysisJob table"
    char[16] experiment index; "Something like ENCSR000CFA."
    string analysisStep; "Name of analysis step"
    string configuration; "Configuration for analysis step"
    lstring tempDir; "Where analysis is to be computed"
    uint firstInputId;	"ID in edwFile of first input"
    uint inputFileCount; "Total number of input files"
    uint[inputFileCount] inputFileIds; "list of all input files as fileIds"
    string[inputFileCount] inputTypes; "List of types to go with input files in json output"
    uint assemblyId; "Id of assembly we are working with"
    uint outputFileCount; "Total number of output files"
    string[outputFileCount] outputNamesInTempDir; "list of all output file names in output dir"
    string[outputFileCount] outputFormats; "list of formats of output files"
    string[outputFileCount] outputTypes; "list of formats of output files"
    lstring jsonResult; "JSON formatted object with result for Stanford metaDatabase"
    char[37] uuid index; "Help to synchronize us with Stanford."
    byte createStatus;  "1 if output files made 0 if not made, -1 if make tried and failed"
    uint createCount;	"Count of files made"
    uint[createCount] createFileIds; "list of ids of output files in warehouse"
    )

