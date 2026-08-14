/* mailViaPipe - sendmail via pipeline.  */
#ifndef MAILVIAPIPE_H
#define MAILVIAPIPE_H

int mailViaPipe(char *toAddress, char *theSubject, char *theBody, char *fromAddress);
/* sendmail via pipeline */
int mailViaPipeBounce(char *toAddress, char *theSubject, char *theBody, char *fromAddress);
/* sendmail via pipeline with a working 'bounce' email address */

#endif
