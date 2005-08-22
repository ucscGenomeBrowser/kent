"""Miscellaneous file operations"""

import os,os.path,errno

def ensureDir(dir):
    """Ensure that a directory exists, creating it (and parents) if needed."""
    try: 
        os.makedirs(dir)
    except OSError, e:
        if e.errno != errno.EEXIST:
            raise e

def ensureFileDir(fname):
    """Ensure that the directory for a file exists, creating it (and parents) if needed.
    Returns the directory path"""
    dir = os.path.dirname(fname)
    if len(dir) > 0:
        ensureDir(dir)
        return dir
    else:
        return "."

def prLine(fh, *objs):
    "write each str(obj) followed by a newline"
    for o in objs:
        fh.write(str(o))
    fh.write("\n")

def prStrs(fh, *objs):
    "write each str(obj), with no newline"
    for o in objs:
        fh.write(str(o))

def prRow(fh, row):
    """Print a row (list or tupe) to a tab file.
    Does string conversion on each columns"""
    first = True
    for col in row:
        if not first:
            fh.write("\t")
        fh.write(str(col))
        first = False
    fh.write("\n")

def prRowv(fh, *objs):
    """Print a row from each argument to a tab file.
    Does string conversion on each columns"""
    first = True
    for col in objs:
        if not first:
            fh.write("\t")
        fh.write(str(col))
        first = False
    fh.write("\n")
