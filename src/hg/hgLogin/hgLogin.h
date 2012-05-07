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

/* -------- password functions ---- */
void cryptWikiWay(char *password, char *salt, char* result);
/* encrypt password as mediawiki does:  ':B:'.$salt.':'.md5($salt.'-'.md5($password ) */
void encryptNewPwd(char *password, char *buf, int bufsize);
/* encrypt a new password */
void encryptPWD(char *password, char *salt, char *buf, int bufsize);
/* encrypt a password */
void findSalt(char *encPassword, char *salt, int saltSize);
/* find the salt part from the password field */
bool checkPwd(char *password, char *encPassword);
/* check an encrypted password */
unsigned int randInt(unsigned int n);
/* little randome number helper returns 0 to n-1 */
char *generateRandomPassword();
/* Generate valid random password for users who have lost their old one.
 * Free the returned value.*/
void lostPassword(struct sqlConnection *conn, char *username);
/* Generate and mail new password to user */
void mailNewPassword(char *username, char *email, char *password);
/* send user new password */
void sendNewPassword(struct sqlConnection *conn, char *username, char *password);
/* email user new password  */

void clearNewPasswordFields(struct sqlConnection *conn, char *username);
/* clear the newPassword fields */

void displayLoginPage(struct sqlConnection *conn);
/* draw the account login page */
void displayLogin(struct sqlConnection *conn);
/* display and process login info */
void  displayLoginSuccess(char *userName, int userId);
/* display login success msg, and set cookie */
void displayLogoutSuccess();
/* display logout success msg, and reset cookie */
void backToHgSession(int nSec);
/* delay for N micro seconds then go back to hgSession page */
void backToDoLoginPage(int nSec);
/* delay for N micro seconds then go back to Login page */
void displayLoginPage(struct sqlConnection *conn);
/* draw the account login page */
void displayAccHelpPage(struct sqlConnection *conn);
/* draw the account help page */
void accountHelp(struct sqlConnection *conn);
/* email user username(s) or new password */




