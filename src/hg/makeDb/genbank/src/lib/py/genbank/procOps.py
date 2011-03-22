"functions operation on processes"

import subprocess

# FIXME: thes should build on pipeline

def procErr(cmd, code, err=None):
    "handle an error from subprocess.communicate"
    if (code < 0):
        msg = "process signaled " + str(-code)
    else:
        msg = "process error"
    msg += ": \"" + " ".join(cmd) + "\""
    if (err != None) and (len(err) != 0):
        msg += ": " + err
    raise Exception(msg)

def callProc(cmd, keepLastNewLine=False, inheritStderr=False):
    """call a process and return stdout, exception with stderr in
    message or inherited from parent"""
    if inheritStderr:
        stderr = None
    else:
        stderr = subprocess.PIPE
    p = subprocess.Popen(cmd, stdin=None, stdout=subprocess.PIPE, stderr=stderr)
    (out, err) = p.communicate()
    if (p.returncode != 0):
        procErr(cmd, p.returncode, err)
    if (not keepLastNewLine) and (len(out) > 0) and (out[-1] == "\n"):
        out = out[0:-1]
    return out

def callProcLines(cmd, inheritStderr=False):
    """call a process and return stdout, split into a list of lines, exception
    with message or inherited from parent"""
    out = callProc(cmd, inheritStderr=inheritStderr)
    if len(out) == 0:
        return []
    else:
        return out.split("\n")

def _needsQuoted(w):
    "determine if a cmd word needs quoted"
    if len(w) == 0:
        return True
    for c in w:
        if c.isspace():
            return True
    return False

def fmtCmd(cmd):
    "format a command pipeline for printing"
    if (len(cmd) > 0) and (isinstance(cmd[0], list) or isinstance(cmd[0], tuple)):
        fc = []
        for c in cmd:
            fc.append(fmtCmd(c))
        return " | ".join(fc)
    else:
        fc = []
        for a in cmd:
            if  _needsQuoted(a):
                fc.append("'" + a + "'")
            else:
                fc.append(a)
        return " ".join(fc)

def _getStdFile(spec, mode):
    if (spec == None) or (type(spec) == int):
        return spec
    if type(spec) == str:
        return file(spec, mode)
    if type(spec) == file:
        if mode == "w":
            spec.flush()
        return spec
    raise Exception("don't know how to deal with stdio file of type " + type(spec))

def runProc(cmd, stdin="/dev/null", stdout=None, stderr=None):
    """run a process, with I/O redirection to specified file paths or open
    file objects. None specifies inheriting."""
    p = subprocess.Popen(cmd, stdin=_getStdFile(stdin, "r"), stdout=_getStdFile(stdout, "w"), stderr=_getStdFile(stderr, "w"))
    p.communicate()
    if (p.returncode != 0):
        procErr(cmd, p.returncode)
