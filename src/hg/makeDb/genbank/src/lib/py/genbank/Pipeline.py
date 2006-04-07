"File-like object to create and manage a pipeline of subprocesses"

import subprocess

def hasWhiteSpace(word):
    "check if a string contains any whitespace"
    for c in word:
        if c.isspace():
            return True
    return False

class Proc(subprocess.Popen):
    """A process in the pipeline.  This extends subprocess.Popen(),
    it also has the following members:

    cmd - command argument vector
    """

    def __init__(self, cmd, stdin, stdout):
        self.cmd = list(cmd)  # clone list

        # need close_fds, or write pipe line fails due to pipes being
        # incorrectly left open (FIXME: report bug)
        subprocess.Popen.__init__(self, self.cmd, stdin=stdin, stdout=stdout, close_fds=True)

    def getDesc(self):
        """get command as a string to use as a description of the process.
        Single quote white-space containing arguments."""
        strs = []
        for w in self.cmd:
            if hasWhiteSpace(w):
                strs.append("'" + w + "'")
            else:
                strs.append(w)
        return " ".join(strs)
    
class Pipeline(object):
    """File-like object to create and manage a pipeline of subprocesses.

    procs - an ordered list of Proc objects that compose the pipeine"""

    def __init__(self, cmds, mode='r', otherEnd=None):
        """cmds is either a list of arguments for a single process, or
        a list of such lists for a pipeline.  Mode is 'r' for a pipeline
        who's output will be read, or 'w' for a pipeline to that is to
        have data written to it.  If otherEnd is specified, and is a string,
        it is a file to open as stdio file at the other end of the pipeline.
        If it's not a string, it is assumed to be a file object to use for output.
        
        read pipeline ('r'):
          otherEnd --> cmd[0] --> ... --> cmd[n] --> fh
        
        write pipeline ('w')
          fh --> cmd[0] --> ... --> cmd[n] --> otherEnd

        The field fh is the file object used to access the pipeline.
        """
        if (mode == "r") and (mode == "w"):
            raise IOError('invalid mode "' + mode + '"')
        self.mode = mode
        self.procs = []
        self.isRunning = True
        self.failExcept = None

        if isinstance(cmds[0], str):
            cmds = [cmds]  # one-process pipeline

        (firstIn, lastOut, otherFh) = self._setupEnds(otherEnd)

        for cmd in cmds:
            self._createProc(cmd, cmds, firstIn, lastOut)
        
        # finish up
        if otherFh != None:
            otherFh.close()
        if mode == "r":
            self.fh = self.procs[len(self.procs)-1].stdout
        else:
            self.fh = self.procs[0].stdin

    def _setupEnds(self, otherEnd):
        "set files at ends of a pipeline"

        # setup other end of pipeline
        if otherEnd != None:
            if isinstance(otherEnd, str):
                otherFh = file(otherEnd, self.mode)
            else:
                otherFh = otherEnd
            if self.mode == "r":
                firstIn = otherFh
            else:
                lastOut = otherFh
        else:
            otherFh = None
            if self.mode == "r":
                firstIn = 0
            else:
                lastOut = 1

        # setup this end of pipe
        if self.mode == "r":
            lastOut = subprocess.PIPE
        else:
            firstIn = subprocess.PIPE
        return (firstIn, lastOut, otherFh)

    def _createProc(self, cmd, cmds, firstIn, lastOut):
        """create one process"""
        if (cmd == cmds[0]):
            stdin = firstIn  # first process in pipeline
        else:
            stdin = self.procs[len(self.procs)-1].stdout
        if (cmd == cmds[len(cmds)-1]):
            stdout = lastOut # last process in pipeline
        else:
            stdout = subprocess.PIPE
        p = Proc(cmd, stdin=stdin, stdout=stdout)
        self.procs.append(p)

    def getDesc(self):
        """get the pipeline commands as a string to use as a description"""
        strs = []
        for p in self.procs:
            strs.append(p.getDesc())
        return " | ".join(strs)

    def wait(self, noError=False):
        """wait to for processes to complete, generate an exception if one exits
        no-zero, unless noError is True, in which care return the exit code of the
        first process that failed"""

        self.isRunning = False

        # must close before waits for output pipeline
        if self.mode == 'w':
            self.fh.close()

        # wait on processes
        firstFail = None
        for p in self.procs:
            if p.wait() != 0:
                if firstFail == None:
                    firstFail = p

        # must close after waits for input pipeline
        if self.mode == 'r':
            self.fh.close()

        # handle failures
        if firstFail != None:
            self.failExcept = OSError(("process exited with %d: \"%s\" in pipeline \"%s\""
                                       % (firstFail.returncode, firstFail.getDesc(), self.getDesc())))
            if not noError:
                raise self.failExcept
            else:
                return firstFail.returncode
        else:
            return 0
                          

    def close(self):
        "wait for process to complete, with an error if it exited non-zero"
        if self.isRunning:
            self.wait()
        if self.failExcept != None:
            raise failExcept
