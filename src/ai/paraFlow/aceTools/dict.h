/*  File: dict.h
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
 * Description: public header for cut-out lex package in dict.c
 * Exported functions:
 * HISTORY:
	   DICT library was rewritten nov 2002 to ensure
	   that the char* returned by dictName are valid
	   untill the whole dict is destroyed.
	   i.e. they are never reallocated even if you
	   keep adding names foreever 
 * Last edited: Dec  4 14:50 2002 (mieg)
 * Created: Tue Jan 17 17:34:44 1995 (rd)
 *-------------------------------------------------------------------
 */

/* @(#)dict.h	1.4 9/16/97 */
#ifndef DICT_H
#define DICT_H

#include "regular.h"

typedef struct dict_struct DICT ;

       /* size is approximate number of name you wish to store */
       /* a reasonable value will diminish the number of rehashing */
DICT *dictCreate (int size) ; 
DICT *dictHandleCreate (int size, STORE_HANDLE handle) ;
DICT *dictCaseSensitiveHandleCreate (int size, STORE_HANDLE handle) ;
void uDictDestroy (DICT *dict) ;
#define dictDestroy(_dict) {uDictDestroy(_dict) ; _dict=0;}
BOOL dictFind (DICT *dict, const char *s, int *ip) ;
BOOL dictAdd (DICT *dict, const char *s, int *ip) ;
BOOL dictRemove (DICT *dict, const char *s) ;
const char *dictName (DICT *dict, int i) ;
int dictCount (DICT *dict) ;		/* number of names */
int dictMax (DICT *dict) ;		/* max index, to limit looping */

#endif /* ndef DICT_H */
/******* end of file ********/
 
 
