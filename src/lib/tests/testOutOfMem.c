/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "sqlNum.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
unsigned long rlimitSize = 0;
unsigned long chunkSize = 0;

if (argc != 3)
    errAbort("%s: Specify rlimit chunkSize\n"
       , argv[0]);
	    
rlimitSize =sqlUnsignedLong(argv[1]);
chunkSize  =sqlUnsignedLong(argv[2]);

printf("rlimit = %lu  chunkSize = %lu\n", rlimitSize, chunkSize);

struct rlimit rlim;
rlim.rlim_cur = rlim.rlim_max = rlimitSize;
if (setrlimit(RLIMIT_DATA, &rlim) < 0)
    warn("setrlimit failed with RLIMIT_DATA rlim_cur=%lld rlim_max=%lld"
	, (long long) rlim.rlim_cur , (long long) rlim.rlim_max); 
// although RLIMIT_AS is not supported/enforced on all platforms,
// it is useful for linux and some other unix OSes. 
if (setrlimit(RLIMIT_AS, &rlim) < 0)
    warn("setrlimit failed with RLIMIT_AS rlim_cur=%lld rlim_max=%lld"
	, (long long) rlim.rlim_cur , (long long) rlim.rlim_max); 

if (1)
{
// show the results of the limits, did they get set as expected?
struct rlimit rlim; 
int rv; 
rv = getrlimit(RLIMIT_DATA,&rlim); 
if ( rv == -1 ) 
    warn("error getrlimit RLIMIT_DATA %s", strerror(errno)); 
else 
    printf("rlimit_data:%lu,%lu\n", (unsigned long) rlim.rlim_max, (unsigned long) rlim.rlim_cur); 
rv = getrlimit(RLIMIT_AS,&rlim); 
if ( rv == -1 ) 
    warn("error getrlimit RLIMIT_AS %s", strerror(errno)); 
else 
    printf("rlimit_as:%lu,%lu\n", (unsigned long) rlim.rlim_max, (unsigned long) rlim.rlim_cur); 
}


unsigned long allocated = 0;
while (allocated < rlimitSize)
    {
    needMem(chunkSize);
    allocated += chunkSize;
    }

printf("allocated = %lu\n", allocated);

// can we get 1 more byte beyond the limit?  
// this should push it into errAbort
needMem(1); 

return 0;
}

