
object submission
"Keeps track of a job submission"
    (
    string id;	"Submission ID from scheduler"
    lstring errFile;	"Error file associated with submission"
    lstring outFile;	"Output file associated with submission"
    byte submitError;	"An error occurred submitting it"
    byte inQueue;	"Currently in queuing system"
    byte queueError;	"In error stat in queue"
    byte running;	"Currently running"
    byte crashed;	"Looks like it ran but crashed"
    byte ranOk;		"Looks like it ran and finished ok"
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
    lstring  command;	"Command line for job"
    int checkCount;	"Count of checks"
    object check[checkCount] checkList;	"Ways to check success of job."
    int submissionCount;	"The number of times submitted"
    object submission[submissionCount] submissionList; "List of submissions"
    )

object jobDb
"Keeps track of a batch of jobs. "
    (
    int jobCount;	"The number of total jobs"
    object job[jobCount] jobList;  "List of all jobs"
    )

