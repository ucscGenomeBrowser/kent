import subprocess
import shlex

def callHgsql(database, command):
    """ Run hgsql command using subprocess, return stdout data if no error."""
    cmd = ["hgsql", database, "-Ne", command]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    cmdout, cmderr = p.communicate()
    if p.returncode != 0:
        # keep command arguments nicely quoted
        cmdstr = " ".join([shlex.quote(arg) for arg in cmd])
        raise Exception("Error from: " + cmdstr + ": " + cmderr)
    return cmdout

def runCommand(command):
    """Runs command in subprocess and raises exception if return code is not 0.
    Returns tuple (stdoutdata, stderrdata). """
    p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    cmdout, cmderr = p.communicate()
    if p.returncode != 0:
        # keep command arguments nicely quoted
        cmdstr = " ".join([shlex.quote(arg) for arg in command])
        raise Exception("Error from: " + cmdstr + "\nError Message: " + cmderr)
    return cmdout, cmderr

def runCommandMergedOutErr(command):
    """Runs command in subprocess and raises exception if return code is not 0.
    Combines stdout and stderr into a single output. Returns stdoutdata string."""
    p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    # cmderr is empty, but we still need to call both so cmdout is properly set
    cmdout, cmderr = p.communicate()
    if p.returncode != 0:
        # keep command arguments nicely quoted
        cmdstr = " ".join([shlex.quote(arg) for arg in command])
        raise Exception("Error from: " + cmdstr)
    return cmdout

def runCommandNoAbort(command):
    """Runs command in subprocess. Returns tuple (stdoutdata, stderrdata)
    and returncode from process."""
    p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    cmdout, cmderr = p.communicate()
    return cmdout, cmderr, p.returncode
