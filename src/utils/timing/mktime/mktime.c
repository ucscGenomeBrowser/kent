/* mktime - command line implementation of mktime() C library function	*/

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include	<stdio.h>
#include	<time.h>
#include	<unistd.h>
#include	<stdlib.h>


void usage()
{
fprintf(stderr,"mktime - convert date string to unix timestamp\n");
fprintf(stderr,"usage: mktime YYYY-MM-DD HH:MM:SS\n");
fprintf(stderr,"valid dates: 1970-01-01 00:00:00 to 2038-01-19 03:14:07\n");
}

int
main( int argc, char **argv)
{
time_t timep = 0;
struct tm tm;

if (argc != 3){ usage(); exit(255);}

if (sscanf(argv[1], "%4d-%2d-%2d",
           &(tm.tm_year), &(tm.tm_mon), &(tm.tm_mday)) != 3)
    {
    fprintf(stderr,"Couldn't parse given date \"%s\"", argv[1]);
    exit(255);
    }
if (sscanf(argv[2], "%2d:%2d:%2d",
           &(tm.tm_hour), &(tm.tm_min), &(tm.tm_sec))  != 3)
    {
    fprintf(stderr,"Couldn't parse given date \"%s\"", argv[2]);
    exit(255);
    }

tm.tm_year -= 1900;
tm.tm_mon  -= 1;
tm.tm_isdst = -1;	/*	do not know, figure it out	*/
timep = mktime(&tm);

printf("%d-%02d-%02d %02d:%02d:%02d %ld\n",
    1900+tm.tm_year, 1+tm.tm_mon, tm.tm_mday,
	tm.tm_hour, tm.tm_min, tm.tm_sec, (unsigned long)timep);

return(0);
}
