/* localtime - command line implementation of localtime() C library function */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include	<stdio.h>
#include	<time.h>
#include	<unistd.h>
#include	<stdlib.h>


void usage()
{
fprintf(stderr,"localtime - convert unix timestamp to date string\n");
fprintf(stderr,"usage: localtime <time stamp>\n");
fprintf(stderr,"\t<time stamp> - integer 0 to 2147483647\n");
}

int
main( int argc, char **argv)
{
int timeStamp;
time_t timep;
struct tm *tm;

if (argc != 2){ usage(); exit(255);}

timeStamp = atoi(argv[1]);
timep = (time_t) timeStamp;

tm = localtime(&timep);
printf("%d-%02d-%02d %02d:%02d:%02d %ld\n",
    1900+tm->tm_year, 1+tm->tm_mon, tm->tm_mday,
	tm->tm_hour, tm->tm_min, tm->tm_sec, (unsigned long)timep);

return(0);
}
