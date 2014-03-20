
table eapJob
"An analysis pipeline job to be run asynchronously and not too many all at once."
    (
    uint id primary auto;    "Job id"
    lstring commandLine; "Command line of job"
    bigInt startTime index; "Start time in seconds since 1970"
    bigInt endTime index; "End time in seconds since 1970"
    lstring stderr; "The output to stderr of the run - may be nonempty even with success"
    int returnCode; "The return code from system command - 0 for success"
    int cpusRequested; "Number of CPUs to request from job control system"
    string parasolId index[12];	"Parasol job id for process." 
    )

table eapSoftware
"Software that is tracked by the analysis pipeline."
    (
    uint id primary auto;  "Software id"
    string name unique; "Command line name"
    string url; "Suggested reference URL"
    string email; "Suggested contact email"
    char[36] metaUuid index[16]; "UUID into Stanford metadata system if synced"
    )

table eapSwVersion
"A version of a particular piece of software"
    (
    uint id primary auto;  "Version id"
    string software index; "Name field of software this is associated with"
    lstring version; "Version as carved out of program run with --version or the like"
    char[32] md5 index; "md5 sum of executable file"
    byte redoPriority;  "-1 for routine recompile, 0 for unknown, 1 for recommended, 2 for required."
    lstring notes;	"Any notes on the version" 
    char[36] metaUuid index[16]; "UUID into Stanford metadata system if synced"
    )

table eapStep
"A step in an analysis pipeline - something that takes one set of files to another"
    (
    uint id primary auto; "Step id"
    string name unique;  "Name of this analysis step"
    int cpusRequested; "Number of CPUs to request from job control system"
    string description;  "Description of step, about a sentence."
    uint inCount; "Total number of inputs"
    string[inCount] inputTypes; "List of types to go with input files"
    string[inCount] inputFormats; "List of formats of input files"
    string[inCount] inputDescriptions; "List of descriptions of input files"
    uint outCount; "Total number of outputs"
    string[outCount] outputNamesInTempDir; "list of all output file names in output dir"
    string[outCount] outputFormats; "list of formats of output files"
    string[outCount] outputTypes; "list of outputType of output files"
    string[outCount] outputDescriptions; "list of descriptions of outputs"
    char[36] metaUuid index[16]; "UUID into Stanford metadata system if synced"
    )
 
table eapStepSoftware
"Relates steps to the software they use"
    (
    uint id primary auto; "Link id - helps give order to software within step among other things"
    string step index[24];	"name of associated step"
    string software index[24]; "name of associated software"
    )

table eapStepVersion
"All the versions of a step - a new row if any subcomponent is versioned too."
    (
    uint id primary auto;  	  "ID of step version -used to tie together rows in edwAnalysisStepVector"
    string step;  "name of associated step"
    uint version; "Version of given step - just increases by 1 with each change"
    )

table eapStepSwVersion
"A table that is queried for list of all software versions used in a step"
    (
    uint id primary auto;	"Link id - helps give order to steps in a given version"
    uint stepVersionId;  "Key in edwAnalysisStepVersion table"
    uint swVersionId;    "Key in edwAnalysisSwVersion table"
    )

table eapRun
"Information on an compute job that produces files by running a step."
    (
    uint id primary auto; "Analysis run ID"
    uint jobId;  "ID in edwAnalysisJob table"
    char[16] experiment index; "Something like ENCSR000CFA."
    string analysisStep; "Name of analysis step.  Different data can be analysed with same step"
    uint stepVersionId; "Keep track of versions of everything"
    lstring tempDir; "Where analysis is to be computed"
    uint assemblyId; "Id of assembly we are working with if analysis is all on one assembly"
    lstring jsonResult; "JSON formatted object with result for Stanford metaDatabase"
    byte createStatus;  "1 if output files made 0 if not made, -1 if make tried and failed"
    char[36] metaUuid index[16]; "UUID into Stanford metadata system if synced"
    )

table eapInput
"Inputs to an eapAnalysis"
    (
    uint id primary auto; "Input table ID"
    uint runId index; "Which eapAnalysis this is associated with"
    string name;  "Input name within step"
    uint ix;  "Inputs always potentially vectors.  Have single one with zero ix for scalar input"
    uint fileId;  "Associated file - 0 for no file, look perhaps to val below instead."
    lstring val;  "Non-file data"
    )

table eapOutput
"Outputs to an eapAnalysis"
    (
    uint id primary auto; "Output table ID"
    uint runId index; "Which eapAnalysis this is associated with"
    string name;  "Output name within step"
    uint ix;  "Outputs always potentially vectors. Have single one with zero ix for scalar output"
    uint fileId;  "Associated file - 0 for no file, look perhaps to val below instead."
    lstring val;  "Non-file data"
    )

table eapPhantomPeakStats
"Statistics on a BAM file that contains reads that will align in a peaky fashion - deprecated"
    (
    uint fileId;    "ID of BAM file this is taken from"
    uint numReads;  "Number of mapped reads in that file"
    string estFragLength; "Up to three comma separated strand cross-correlation peaks"
    string corrEstFragLen; "Up to three cross strand correlations at the given peaks"
    int phantomPeak;  "Read length/phantom peak strand shift"
    double corrPhantomPeak; "Correlation value at phantom peak"
    int argMinCorr; "strand shift at which cross-correlation is lowest"
    double minCorr; "minimum value of cross-correlation"
    double nsc; "Normalized strand cross-correlation coefficient (NSC) = COL4 / COL8"
    double rsc; "Relative strand cross-correlation coefficient (RSC) = (COL4 - COL8) / (COL6 - COL8)A"
    int qualityTag; "based on thresholded RSC (codes: -2:veryLow,-1:Low,0:Medium,1:High,2:veryHigh)"
    )

