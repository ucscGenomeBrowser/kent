# NOTE: jobResult.{c,h} were modified to support bookmark for para.results
#  struct jobResult *jobResultLoadAll(char *fileName, off_t *resultBookMark) 

table jobResult
"Info about the result of one job from parasol"
     (
     int status;	"Job status - wait() return format. 0 is good."
     string host;	"Machine job ran on."
     string jobId;	"Job queuing system job ID"
     string exe;	"Job executable file (no path)"
     int usrTicks;	"'User' CPU time in ticks."
     int sysTicks;	"'System' CPU time in ticks."
     uint submitTime;	"Job submission time in seconds since 1/1/1970"
     uint startTime;	"Job start time in seconds since 1/1/1970"
     uint endTime;	"Job end time in seconds since 1/1/1970"
     string user;	"User who ran job"
     string errFile;	"Location of stderr file on host"
     )
