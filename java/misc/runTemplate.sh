#!/bin/sh -e
# run @PROG_CLASS@

JAVA_HOME="@JAVA_HOME@"
CLASSPATH="@CLASSPATH@"

export JAVA_HOME CLASSPATH

exec ${JAVA_HOME}/bin/java -Xms256m -Xmx512m @PROG_CLASS@ "$@"
