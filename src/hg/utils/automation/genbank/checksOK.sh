#!/bin/sh

find ./archaea ./bacteria ./fungi ./invertebrate ./other ./plant ./protozoa ./vertebrate_mammalian ./vertebrate_other -type f | grep checkAgpStatusOK.txt \
  > checksOK.list
