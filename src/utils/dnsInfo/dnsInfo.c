/* dnsInfo - Get info from DNS about a machine. */
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
struct hostent *h;
char **aliases, **addresses;
char str[INET6_ADDRSTRLEN];

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
if ((h = gethostbyname(machine)) == NULL)
    errAbort("Couldn't gethostbyname: %s", hstrerror(h_errno));
printf("official hostname: %s\n", h->h_name);
for (aliases = h->h_aliases; *aliases != NULL; ++aliases)
    printf("\talias: %s\n", *aliases);
switch (h->h_addrtype)
    {
    case AF_INET:
    case AF_INET6:
        addresses = h->h_addr_list;
        for (addresses = h->h_addr_list; *addresses != NULL; ++addresses)
            {
	    printf("\taddress: %s\n",
	    	inet_ntop(h->h_addrtype, *addresses, str, sizeof(str)));
            }
	break;
    default:
        errAbort("unknown address type %d", (int)h->h_addrtype);
	break;
    }
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
