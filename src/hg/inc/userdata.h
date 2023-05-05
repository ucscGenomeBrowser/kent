/* userdata.c - code for managing data stored on a per user basis */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef USERDATA_H
#define USERDATA_H

char *storeUserFile(char *userName, char *fileName, void *data, size_t dataSize);
/* Give a fileName and a data stream, write the data to:
 * userdata/userStore/hashedUserName/userName/fileName
 * where hashedUserName is based on the md5sum of the userName
 * to prevent proliferation of too many directories. 
 *
 * After sucessfully saving the file, return a web accessible url
 * to the file. */

#endif /* USERDATA_H */
