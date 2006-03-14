/* rtRefCount - stuff for thread safe reference counting in
 * the runtime. */
#ifndef RTREFCOUNT

#if defined(__i686__)


#define rtRefInc(obj) \
	asm("lock ;" \
		"incl %1;" \
		:"=m" (obj->_pf_refCount) \
		:"m" (obj->_pf_refCount) \
		:"memory")

#define rtRefDec(obj) \
	asm("lock ;" \
		"decl %1;" \
		:"=m" (obj->_pf_refCount) \
		:"m" (obj->_pf_refCount) \
		:"memory")

#else
// Really should give warning here

#define rtRefInc(obj) ((obj)->_pf_refCount += 1)
#define rtRefDec(obj) ((obj)->_pf_refCount -= 1)

#endif

#define rtRefCleanup(obj,type) \
	if ((obj) != NULL) \
	    { \
	    rtRefDec(obj); \
	    if ((obj)->_pf_refCount == 0) \
		(obj)->_pf_cleanup((obj), (type)); \
	    }
	        
#ifdef DEBUG   
	if ((obj) != NULL) \
	    { \
	    uglyf("rtRefCleanup of %p (%s): refCount %d\n", obj, obj->s, obj->_pf_refCount); \
	    rtRefDec(obj); \
	    uglyf(" after dec: refCount %d\n", obj->_pf_refCount); \
	    if ((obj)->_pf_refCount == 0) \
		(obj)->_pf_cleanup((obj), (type)); \
	    }
#endif /* DEBUG */

#endif /* RTREFCOUNT */
