/*  File: dict.c
 *  Author: Richard Durbin and Jean Thierry-Mieg (rd@sanger.ac.uk)
 *  Copyright (C) J Thierry-Mieg and R Durbin, 1995
 * -------------------------------------------------------------------
 * Acedb is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * or see the on-line version at http://www.gnu.org/copyleft/gpl.txt
 * -------------------------------------------------------------------
 * This file is part of the ACEDB genome database package, written by
 * 	Richard Durbin (MRC LMB, UK) rd@mrc-lmb.cam.ac.uk, and
 *	Jean Thierry-Mieg (CRBM du CNRS, France) mieg@kaa.cnrs-mop.fr
 *
 * Description: 
 * Exported functions:
 * HISTORY:
	   DICT library was rewritten nov 2002 to ensure
	   that the char* returned by dictName are valid
	   untill the whole dict is destroyed.
	   i.e. they are never reallocated even if you
	   keep adding names foreever
	   and dictRemove was added
 * Last edited: Dec  4 14:50 2002 (mieg)
 * Created: Tue Jan 17 17:33:44 1995 (rd)
 *-------------------------------------------------------------------
 */

/* $Id: dict.c,v 1.1 2006/03/08 10:06:04 mieg Exp $ */
#include "regular.h"
                             /* every thing here is private */

static char *DICT_MAGIC = "dict-magic" ;
typedef struct { unsigned curr ; int pos, free, max ; char *base ; } DVOC ;
typedef struct dict_struct {
  BOOL caseSensitive ;          /* default FALSE, public */
  int dim ;         /* dimension of the hash table, has to be a power of 2 */
  int max ;           /* 2 to the  dict->dim */
  int count ;         /* number of active names in the dict */
  int nVoc ;           /* current dVoc */
  int newPos ;          /* moving index in the hash table */
  char *magic ;
  Array table ;			/* hash table */
  Array keys ;		/* ofsets in the set of vocs */
  Array dVocs ;                  /* array of DVOC */
  STORE_HANDLE handle ;
} DICT_STRUCT ;

#include "dict.h"
typedef DICT _DICT ;

/*
  The names are stored incrementally in large buffers, dVoc->buff, which
  are never reallocated, so that the pointers returned by dictName
  remain valid until the dict is destroyed.

  The key is as usual in acedb a composite 1 byte for the dVoc number
  3 bytes for the offsett in dVoc->buff. In reality we divide by 8
  the offset since we align the names on 64 bit boundaries.

  The table is a double hashing hash table, its dimension is a power of 2
  so it is prime with the delta hash value which is always odd. In this
  way the orbit (first hash value modulo delta covers the whole table).

  The first 0 found during the travle indicates that the name is absent
  in the hash table. The first negative value indicates a spot than has 
  been freed and can be reused, but does not stop the bouncing search .

  We retrieve a name by direct dereferencing.
  We recognize a name by hash search.
*/

#define DEBUG_MODE 
#ifndef DEBUG_MODE
/* optimised mode */
/* these 2 private complex macros are useful to accelerate the hashing code */
/* whereas the public interface always checks the range of the parameters */

#define dictKey2Name(_dict,_k) ((arrp (_dict->dVocs, ((k >> 24) & 0x000000ff) - 1, DVOC))->base + ((0x00ffffff & (_k)) << 3))
#define dictIndex2Name(_dict_i) (dictKey2Name(_dict,arr(-dict->keys,_i,KEY)))

#else
/* debugging mode */
static const char *dictKey2Name (_DICT *dict, KEY k) 
{
  int nVoc = ((k >> 24) & 0x000000ff) - 1 ;
  int pos = (0x00ffffff & k) << 3 ;
  DVOC *dVoc ;
  
  if (nVoc < 0 || nVoc >= arrayMax (dict->dVocs))
    messcrash ("uDictKey2Name bad nVoc = %d", nVoc) ;
  dVoc = arrp (dict->dVocs, nVoc, DVOC) ;
  if (pos >= dVoc->curr)
    messcrash ("uDictKey2Name bad pos = %d >= curr = %d", pos, dVoc->curr) ;
  return dVoc->base + pos ;
}

static const char *dictIndex2Name (_DICT *dict, int ii)
{
  KEY key ;
  
  if (ii < 0 || ii >= arrayMax (dict->keys))
    messcrash ("uDictIndex2Name") ;
  key = arr(dict->keys, ii, KEY) ;
  return dictKey2Name (dict, key) ;
}
#endif /* DEBUG_MODE */

/************* standard utility from Jean *************/
#define  SIZEOFINT (8 * sizeof (int))
static int dictHash (const char *cp, int n, BOOL isDiff)
{
  register int i ;
  register unsigned int j, x = 0 ;
  register int rotate = isDiff ? 21 : 13 ;
  register int leftover =  SIZEOFINT - rotate ;

  while (*cp)
    x = freeupper (*cp++) ^ (( x >> leftover) | (x << rotate)) ; 

				/* compress down to n bits */
  for (j = x, i = n ; i < SIZEOFINT ; i += n)
    j ^= (x >> i) ;
  j &= (1 << n) - 1 ;

  if (isDiff)			/* return odd number */
    j |= 1 ;

  return j ;
}  /* dictHash */

/****************************************************************************/

static void dictReHash (_DICT *dict, int newDim)
{
  int ii ;
  KEY *kp ;

  if (newDim <= dict->dim)
    return ;
  dict->dim = newDim ;
  dict->max = 1 << newDim ;
				/* remake the table */
  arrayDestroy (dict->table) ;
  dict->table = arrayHandleCreate (dict->max, int, dict->handle) ;
  array (dict->table, dict->max-1, int) = 0 ;	/* set arrayMax */

				/* reinsert all the names */
  for (ii = 1, kp = arrp(dict->keys, ii, KEY)  ; ii < arrayMax(dict->keys) ; ii++, kp++)
    { dictFind (dict, dictKey2Name (dict, *kp), 0) ;
				/* will fail, but sets dict->newPos */
      arr(dict->table, dict->newPos, int) = ii ;
    }
} /* dictReHash */

/****************************************************************************/

static DVOC *dictAddVoc (_DICT *dict, int s)
{
  DVOC *dVoc ;
  int n1 = 0 ;
 
  dVoc = arrayp (dict->dVocs, dict->nVoc, DVOC) ;
  if (dict->nVoc > 0)
    n1 = 2 * (dVoc - 1)->max ;
  if (n1 < 8 * s)
    n1 = 8 * s ;
  dVoc->base = (char *) halloc (n1, dict->handle) ; /* never realloc */
  dVoc->curr = 0 ;
  dVoc->max = n1 ;
  dVoc->free = n1 ;
  
  dict->nVoc++ ;
  return dVoc ;
}  /* dictAddVoc */


void uDictDestroy (_DICT *dict) 
{ 
  if (dict && dict->magic == DICT_MAGIC) 
    {  /* we need 2 lines, because otherwise messfree sets (freed) dict->handle=0 */
      STORE_HANDLE handle = dict->handle ;
      messfree (handle) ;
    }
}

/****************************************************************************/

_DICT *dictHandleCreate (int size, STORE_HANDLE handle)
{
  STORE_HANDLE h = handleHandleCreate (handle) ;
  _DICT *dict = (_DICT*) halloc (sizeof (_DICT), h) ;

  dict->handle = h ;
  dict->magic = DICT_MAGIC ;
  dict->caseSensitive = FALSE ;
  dict->count = 0 ;
  for (dict->dim = 6, dict->max = 64 ; 
       dict->max < size ; 
       ++dict->dim, dict->max *= 2) ;
  dict->table = arrayHandleCreate (dict->max, int, dict->handle) ;
  array (dict->table, dict->max-1, int) = 0 ;	/* set arrayMax */
  dict->dVocs = arrayHandleCreate (8, DVOC, dict->handle) ;
  dictAddVoc (dict, 8 * dict->max) ;
  dict->keys = arrayHandleCreate (dict->dim/4, KEY, dict->handle) ;
  array (dict->keys, 0, KEY) = 0 ;		/* reserved for empty table entry */

  return dict ;
} /* dictHandleCreate */

_DICT *dictCreate (int size) 
{
  return dictHandleCreate (size, 0) ; 
} /* dictCreate */

_DICT *dictCaseSensitiveHandleCreate (int size, STORE_HANDLE handle) 
{
  _DICT *dict = dictHandleCreate (size, handle) ;
  dict->caseSensitive = TRUE ;
  
  return dict ;
} /* dictCaseSensitiveHandleCreate */

/****************************************************************************/

BOOL dictFind (_DICT *dict, const char *s, int *ip)
{
  register int ii, h, dh = 0 ;
  int (*mystrcmp)(const char *, const char *) = dict->caseSensitive ?
    strcmp : strcasecmp ;

  if (!dict || !s || !*s)
    return FALSE ;

  dict->newPos = 0 ; /* will become first reusable spot */
  h = dictHash (s, dict->dim, FALSE) ;
  
  while (TRUE)
    { ii = arr (dict->table, h, KEY) ;
      if (!ii)			/* empty slot, s is unknown */
	{ 
	  if (ip) 
	    *ip = ii - 1 ;
	  if (dict->newPos == 0)
	    dict->newPos = h ;
	  return FALSE ;
	}
      else if (ii < 0)		/* freed stop */
	{ 
	  if (dict->newPos == 0)
	    dict->newPos = h ;
	  continue ;
	}
      else if (!mystrcmp (s, dictIndex2Name(dict,ii)))
	{ if (ip) 
	    *ip = ii - 1 ;
	  dict->newPos = h ;
	  return TRUE ;
	}
      if (!dh)
	dh = dictHash (s, dict->dim, TRUE) ;
      h += dh ;
      if (h >= dict->max)
	h -= dict->max ;
    }
} /* dictFind */

/****************************************************************************/

BOOL dictRemove (_DICT *dict, const char *s) 
{
  int ii = 0 ;

  if (!dict || 
      !s ||
      !dictFind (dict, s, &ii))	/* word unkown */
    return FALSE ;

  ii++ ;
  arr (dict->keys, ii, KEY) = 0 ;  /* will not be rehashed */
  arr (dict->table, dict->newPos, int) = -ii ; /* will be reusable */
  dict->count-- ;
  return TRUE ;
} /* dictRemove */

/****************************************************************************/
/* always fills ip, returns TRUE if added, FALSE if known */
BOOL dictAdd (_DICT *dict, const char *s, int *ip)
{
  int ii = 0, len ;
  DVOC *dVoc  ;

  if (!dict || !s)
    return FALSE ;

  if (dictFind (dict, s, &ii))	/* word already known */
    {
      if (ip)
	*ip = ii ;
      return FALSE ;
    }
  ii++ ;
  if (ii < 0)
    ii = -ii ; /* reuse */
  else
    ii = arrayMax(dict->keys) ;
  array (dict->table, dict->newPos, int) = ii ;
  
  dVoc = arrp (dict->dVocs, dict->nVoc - 1, DVOC) ;
  len = strlen (s) ;
  if (len + 1 >= dVoc->free)
    dVoc = dictAddVoc (dict, len) ;

  array (dict->keys, ii, KEY) = (0xff000000 & ((dict->nVoc)<<24)) | (0x00ffffff & (dVoc->curr)>>3) ;
  strcpy (dVoc->base + dVoc->curr, s) ;
  len++ ;            /* count the terminal zero */
  while (len%8) len++ ; /* adjust on word boundary */
  dVoc->curr += len ;
  dVoc->free -= len ;
  dict->count++ ;
  if (arrayMax(dict->keys) > 0.4 * dict->max)
    dictReHash (dict, dict->dim+1) ;

  if (ip)
    *ip = ii - 1 ;
  return TRUE ;
} /* dictAdd */

/********************** utilities ***********************/
/* dictName returns a pointer that never gets reallocated */

const char *dictName (_DICT *dict, int ii)
{
  KEY key ;

  if (ii < 0 || ii + 1 >= arrayMax(dict->keys))
    {
      messcrash ("Call to dictName() out of bounds: %d not in [0,%d] ", ii, dictMax(dict) - 1) ;
      return "(Dict error, NULL NAME)" ;
    }

  key = arr (dict->keys, ii + 1, KEY) ;
  return dictKey2Name(dict, key) ;
} /* dictName */

int dictCount (_DICT *dict)
{
  return dict->count ;   /* number of active names */
}  /* dictMax */

int dictMax (_DICT *dict)   /* max to be used if looping on all entries */
{
  return arrayMax(dict->keys) - 1 ;   /* 0 == reserved pseudo key */
}  /* dictMax */

/******************** end of file **********************/
/****************************************************************************/
/****************************************************************************/
