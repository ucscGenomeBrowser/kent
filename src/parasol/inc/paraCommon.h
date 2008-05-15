/* This just defines _REENTRANT in front of everything
 * and includes a bunch more system includes than the
 * usual common.h.   The REENTRANT is for thread safety.
 * The other stuff is mostly internet stuff. */

#ifndef PARACOMMON_H
#define PARACOMMON_H

#define _REENTRANT
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "common.h"
#include "paraVersion.h"

#endif /* PARACOMMON_H */
