/* isxLiveVar - Manage live variable lists for intermediate code. */

#ifndef ISXLIVEVAR_H
#define ISXLIVEVAR_H

struct isxAddress;
struct isxList;;
struct dlList;

struct isxLiveVar
/* A variable that is alive (which is defined and may be referenced later on) */
    {
    struct isxLiveVar *next;
    struct isxAddress *var;
    int useCount;	/* Number of following uses */
    int usePos[2];	/* The next two use relative positions */
    };

#define isxLiveVarFree(pLive) freez(pLive)
/* Free up liveVar */

#define isxLiveVarFreeList(pList) slFreeList(pList)
/* Free up list of live vars. */

void isxLiveList(struct isxList *isxList);
/* Create list of live variables at each instruction by scanning
 * backwards. Handles loops and conditionals appropriately.  
 * Also fill in loopList. */

struct isxLiveVar *isxLiveVarFind(struct isxLiveVar *liveList, 
	struct isxAddress *iad);
/* Return liveVar associated with address, or NULL if none. */

#endif /* ISXLIVEVAR_H */
