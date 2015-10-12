#!/bin/bash

usage() {
    echo "errCheck - run a program and compare error response to expected.
usage:
    errCheck <expectedStderr.txt> <command> [command arguments]
Returns zero if command returns with a non-crashing nonzero return and the
stderr output of the command matches expected.txt" 1>&2
}

if [ $# -lt 2 ]; then
    usage
    exit 255
fi

expectTextFile="$1"
if [ ! -s "${expectTextFile}" ]; then
    echo "ERROR: can not read expectedStderr.txt file: $expectTextFile" 1>&2
    usage
fi
command="$2"
stderrOut="${TMPDIR:-/tmp}/errCheck.err.$$"
stdoutOut="${TMPDIR:-/tmp}/errCheck.out.$$"
shift 2
echo "# ${command} $*" 1>&2
${command} $* 2> "${stderrOut}" > "${stdoutOut}"
returnCode=$?
exitValue=0
if [ "${returnCode}" -ne 0 ]; then
    diffCount=`diff "${stderrOut}" "${expectTextFile}" 2> /dev/null | wc -l`
    if [ "${diffCount}" -ne 0 ]; then
        echo "ERROR: command returns exit code ${returnCode}" 1>&2
        echo "ERROR: did not find expected stderr content" 1>&2
        exitValue=255
fi
else
    echo "ERROR: command returns zero exit code" 1>&2
    exitValue=255
fi
rm -f "${stderrOut}" "${stdoutOut}"
exit $exitValue
