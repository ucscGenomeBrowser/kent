#ifndef __MSORT_H
#define __MSORT_H
void _pf_cm_mSort (void *b, int n, int s, int (*cmp)(void *va, void *vb)) ;
void _pf_cm_mSort_r (void *b, int n, int s, void *pf
	      , int (*cmp_r)(void *pf, void *va, void *vb)) ;

#endif
