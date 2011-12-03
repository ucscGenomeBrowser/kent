/* pwdtest - testing password generation */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "hgConfig.h"
#include "cheapcgi.h"
#include "memalloc.h"
#include "jksql.h"
#include "htmshell.h"
#include "cart.h"
#include "hPrint.h"
#include "hdb.h"
#include "hui.h"
#include "web.h"
#include "ra.h"
#include "hgColors.h"
#include <crypt.h>

#include "net.h"

#include "pwdtest.h"
#include "members.h"
#include "bio.h"
#include "paypalSignEncrypt.h"
#include "versionInfo.h"


char *excludeVars[] = { "submit", "Submit", "debug", "fixMembers", "update", "gsidM_password", NULL }; 
/* The excludeVars are not saved to the cart. (We also exclude
 * any variables that start "near.do.") */

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
struct hash *oldCart;	/* Old cart hash. */
char *errMsg;           /* Error message to show user when form data rejected */

/* -------- password functions ---- */

void encryptPWD(char *password, char *salt, char *buf, int bufsize)
/* encrypt a password */
{
/* encrypt user's password. */
safef(buf,bufsize,crypt(password, salt));
}

void encryptNewPwd(char *password, char *buf, int bufsize)
/* encrypt a new password */
{
unsigned long seed[2];
char salt[] = "$1$........";
const char *const seedchars =
"./0123456789ABCDEFGHIJKLMNOPQRST"
"UVWXYZabcdefghijklmnopqrstuvwxyz";
int i;
/* Generate a (not very) random seed. */
seed[0] = time(NULL);
seed[1] = getpid() ^ (seed[0] >> 14 & 0x30000);
/* Turn it into printable characters from `seedchars'. */
for (i = 0; i < 8; i++)
    salt[3+i] = seedchars[(seed[i/5] >> (i%5)*6) & 0x3f];
encryptPWD(password, salt, buf, bufsize);
}

bool checkPwd(char *password, char *encPassword)
/* check an encrypted password */
{
char encPwd[35] = "";
encryptPWD(password, encPassword, encPwd, sizeof(encPwd));
if (sameString(encPassword,encPwd))
    {
    return TRUE;
    }
else
    {
    return FALSE;
    }
}

boolean checkPwdCharClasses(char *password)
/* check that password uses at least 2 character classes */
{
/* [A-Z] [a-z] [0-9] [!@#$%^&*()] */
int classes[4]={0,0,0,0};
char c;
while ((c=*password++))
    {
    if (c >= 'A' && c <= 'Z') classes[0] = 1;
    if (c >= 'a' && c <= 'z') classes[1] = 1;
    if (c >= '0' && c <= '9') classes[2] = 1;
    if (strchr("!@#$%^&*()",c)) classes[3] = 1;
    }
return ((classes[0]+classes[1]+classes[2]+classes[3])>=2);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char buf[200];
encryptNewPwd(argv[1], buf, sizeof(buf));
printf("%s\n", buf);
printf("%d\n", checkPwd(argv[1], buf));
return 0;
}
