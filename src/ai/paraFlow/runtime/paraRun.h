/* paraRun - stuff to help manage para operations on collections.  */

#ifndef PARARUN_H
#define PARARUN_H

void paraRunInit(int cpuCount);
/* Initialize manager and worker threads who will proceed
 * to wait for work. */

void paraRunArray(struct _pf_array *array, 
	void *localVars, void (*process)(void *item, void *localVars));
/* Run process on each item in array. */

void paraRunDir(struct _pf_dir *dir,
	void *localVars, void (*process)(void *item, void *localVars));
/* Run process on each item in dir. */

void paraRunRange(int start, int end,
	void *localVars, void (*process)(void *item, void *localVars));
/* Run process on each item in range. */

#endif /* PARARUN_H */

