#!/bin/bash

# verify umask for local hgwdev users

thisMachine=`uname -n`
if [ "${thisMachine}" = "hgwdev" ]; then
  uMask=`umask`
  if [ "${uMask}" != "0002" ]; then
    echo "WARNING: local hgwdev users should run umask 002, yours is '${uMask}'" 1>&2
  fi
fi

