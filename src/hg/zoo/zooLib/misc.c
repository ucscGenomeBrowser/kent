/* Copyright 2002 Trevor Bruen */
#include "misc.h"
#include <stdio.h>


/* Allocates space checking safety - should be improved */
void *safe_alloc(size_t size)
{
void *p;

if((p=malloc(size)) == NULL)
    {
    fprintf(stderr,"Fatal cannot allocate memory");
    exit(1);
    }
return p;
}

/* Create new copy of string */
char *strClone(char *s2)
{
char *s1;
s1=malloc((strlen(s2)+1) * sizeof(char));
memcpy(s1,s2,strlen(s2));
s1[strlen(s2)+1] = 0;
return s1;
}

/* Open safely */
FILE *safe_open(char *name, char *mode)
{
FILE *handle;

if((handle = fopen(name,mode)) == NULL)
    {
    fprintf(stderr,"Error: Couldn't open %s\n",name);
    exit(1);
    }

return handle;
}
