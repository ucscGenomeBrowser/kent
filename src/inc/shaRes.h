/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Shared Resource file */
#ifndef SHARES_H
#define SHARES_H

struct shaResNode
/* A shared resource node. */
    {
    struct shaResNode *next;
    struct shaResNode *prev;
    int links;
    char *name;
    struct shaResList *list;
    void *data;
    };

struct shaResList
/* A shared resource list. */
    {
    struct shaResNode *head;
    void (*freeData)(void *pData);
    };


void shaUnlink(struct shaResNode *node);
/* Decrement link count and free if down to zero. */

void shaLink(struct shaResNode *node);
/* Increment link count. */

struct shaResNode *shaNewNode(struct shaResList *list, char *name, void *data);
/* Create a new node with link count of one. */

void shaCleanup(struct shaResList *list);
/* Free every node on list. */

void shaInit(struct shaResList *list, void (*freeData)(void *pData));
/* Start up resource list. */
#endif /* SHARES_H */



