#!/bin/sh
mdbQuery "select fileName, dccAccession from hg19" | sed -e 's/^.*fileName=//' -e 's/; dccAccession=/      /' -e 's/;//' | grep '\.' | sort 
