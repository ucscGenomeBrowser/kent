# Docent tests for MethBase on the RR.  See README.txt for what "MethBase on the RR"
# means, which is not the same thing as the Methbase composite on hgwdev.
#
#     make test               # every *.docent.yaml here
#     make test T=mbSignal    # just one
#     make preflight          # the hubs and the server, no browser
#
# Same rules as ../regress: a script that passed leaves no log and no stills.

DOCENT = ../../docent.js

PREFLIGHT = ../preflight.js

# `make parity` runs one script FAST, then slow, then again.  mbSignal is the right one:
# it is short, and its assertion is on the pixels, which is what FAST could plausibly
# change.  Note the slow leg records an mp4 against the RR.
PARITY = mbSignal

include ../docentTest.mk
