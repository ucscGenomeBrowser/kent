#!/bin/bash
(cd $HOME/kent/src && make clean > /dev/null)
$HOME/bin/scripts/buildWithUseCases $HOME/kent/src alpha > $HOME/kent/build.log
