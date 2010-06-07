/* msort - a little sort in memory. */

#ifndef MSORT_H
#define MSORT_H

void _pf_cm_mSort_r (void *b, int n, int s, void *pf
	      , int (*cmp_r)(void *pf, const void *va, const void *vb)) ;

#endif /* MSORT_H */
