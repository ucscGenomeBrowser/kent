/* hubConnect - stuff to manage connections to track hubs.  Most of this is mediated through
 * the hubConnect table in the hgCentral database.  Here there are routines to translate between
 * hub symbolic names and hub URLs,  to see if a hub is up or down or sideways (up but badly
 * formatted) etc. */

#ifndef HUBCONNECT_H
#define HUBCONNECT_H

#define hubConnectTableName "hubConnect"
/* Name of our table. */

boolean hubConnectTableExists();
/* Return TRUE if the hubConnect table exists. */

#endif /* HUBCONNECT_H */
