/*
 * Routines for reading file of tab column into refAlign objects./
 */
#ifndef HG_REF_ALIGN_TAB_H
#define HG_REF_ALIGN_TAB_H

struct refAlign* parseTabFiles(int nfiles, char** fnames);
/* Read and parse multiple files, return head of list */

#endif
