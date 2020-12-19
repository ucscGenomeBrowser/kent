
hubDirs = primates mammals birds fish vertebrate

all:
	@echo "potential targets: symLinks index genomes"

symLinks::
	@for D in ${hubDirs} x; do \
	    if test "$$D" != "x" && test -d "../$${D}AsmHub" ; then \
		(cd ../$${D}AsmHub && echo $$D && make symLinks ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

genomes::
	@for D in ${hubDirs} x; do \
	    if test "$$D" != "x" && test -d "../$${D}AsmHub" ; then \
		(cd ../$${D}AsmHub && echo $$D && make mkGenomes ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

index::
	@for D in ${hubDirs} x; do \
	    if test "$$D" != "x" && test -d "../$${D}AsmHub" ; then \
		(cd ../$${D}AsmHub && echo $$D && make hubIndex ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

asmStats::
	@for D in ${hubDirs} x; do \
	    if test "$$D" != "x" && test -d "../$${D}AsmHub" ; then \
		(cd ../$${D}AsmHub && echo $$D && make asmStats ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done
