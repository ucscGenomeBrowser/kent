
object submission
"Keeps track of a job submission"
    (
    string id;	"Submission ID from scheduler"
    lstring errFile;	"Error file associated with submission"
    float cpuTime;	"CPU time in seconds"
    uint submitTime;	"Time submitted"
    uint startTime;	"Start time of job"
    uint endTime;	"End time of job"
    int retVal;		"Return value of job"
    ubyte gotRetVal;	"True if got return value"
    ubyte submitError;	"An error occurred submitting it"
    ubyte inQueue;	"Currently in queuing system"
    ubyte queueError;	"In error stat in queue"
    ubyte trackingError; "Have lost track of this somehow - no output, not on queue"
    ubyte running;	"Currently running"
    ubyte crashed;	"Looks like it ran but crashed"
    ubyte slow;		"Run so long we warn user"
    ubyte hung;		"Run so long we kill it"
    ubyte ranOk;	"Looks like it ran and finished ok"
    )

object check
"How to check a job"
    (
    string when;	"When to check - currently either 'in' or 'out'"
    string what;	"What to check - 'exists' 'nonzero' 'lastLine'"
    lstring file;	"File to check"
    )

object job
"Keeps track of a job"
    (
    lstring command;	"Command line for job"
    int checkCount;	"Count of checks"
    object check[checkCount] checkList;	"Ways to check success of job."
    int submissionCount;	"The number of times submitted"
    object submission[submissionCount] submissionList; "List of submissions"
    lstring spec;	"Specification for job"
    )

object jobDb
"Keeps track of a batch of jobs. "
    (
    int jobCount;	"The number of total jobs"
    object job[jobCount] jobList;  "List of all jobs"
    )

