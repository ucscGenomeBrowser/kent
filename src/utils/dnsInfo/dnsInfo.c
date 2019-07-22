/* dnsInfo - Get info from DNS about a machine. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "dnsInfo - Get info from DNS about a machine\n"
  "usage:\n"
  "   dnsInfo machine\n"
  );
}

void dnsInfo(char *machine)
/* dnsInfo - Get info from DNS about a machine. */
{


struct addrinfo hints;
ZeroVar(&hints);
hints.ai_flags    = AI_NUMERICSERV;
hints.ai_family   = AF_UNSPEC;
hints.ai_socktype = SOCK_STREAM;  // Get apparent dupes without this.

struct addrinfo *ai, *p = NULL;

if (sameString(machine, "localhost"))
    {
    struct utsname myname;
    if (uname(&myname) < 0)
         errnoAbort("Couldn't uname.");
    printf("localhost: %s\n", myname.nodename);
    machine = myname.nodename;
        {
	char buf[256];
	if (getdomainname(buf, sizeof(buf)) < 0)
	    errnoAbort("getdomainname error");
	printf("domain name: %s\n", buf);
	}
    }

/********************************************************************/
/* Get the address information for the server using getaddrinfo().  */
/********************************************************************/
int rc = getaddrinfo(machine, NULL, &hints, &ai);
if (rc != 0)
    errAbort("getaddrinfo() failed");

for (p = ai; p; p = p->ai_next)
    {
    char host[256];
    getnameinfo(p->ai_addr, p->ai_addrlen, host, sizeof (host), NULL, 0, NI_NUMERICHOST);
    puts(host);
    }
freeaddrinfo(ai);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
dnsInfo(argv[1]);
return 0;
}
