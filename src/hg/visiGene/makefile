include ../../inc/common.mk
# Build all directories in makeDb 

all:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

test::
	cd clusterGenes && ${MAKE} test

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
    gensat \
    hgVisiGene \
    jackson \
    knownToVisiGene \
    vgChangeContributors \
    vgGetText \
    vgLoadAllen \
    vgLoadJax \
    vgLoadMahoney \
    vgLoadNibb \
    vgPatchJax \
    vgProbeTrack \
    vgRemoveSubmission \
    visiGeneLoad \
    visiGeneSearch \
    visiGeneTest 

# Needs work to compile
#	vgPrepImage \
