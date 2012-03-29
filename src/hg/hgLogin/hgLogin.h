/* hgLogin.h  */

#ifndef hgLogin_H
#define hgLogin_H


/* ---- global variables ---- */
#define TITLE "UCSC Genome Browser Login v"CGI_VERSION


/* ---- Cart Variables ---- */
//#define signupName "hgLogin.do.signup"    /* Show  */
#define signupName "hgLogin.do.signup"    /* Do signup button */

/* ---- General purpose helper routines. ---- */


#endif /* hgLogin_H */

void  displayLoginSuccess(char *userName, int userId);
/* display login success msg, and set cookie */
void dosplayLogoutSuccess();
/* display logout success msg, and reset cookie */

