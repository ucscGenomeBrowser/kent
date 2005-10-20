/* printCaption - query database for info and print it out
 * in a caption. */

#ifndef PRINTCAPTION_H
#define PRINTCAPTION_H

void smallCaption(struct sqlConnection *conn, int imageId);
/* Write out small format caption. */

void fullCaption(struct sqlConnection *conn, int id);
/* Print information about image. */

#endif /* PRINTCAPTION_H */

