include ../../inc/common.mk


O = paraNode.o

paraNode: $(O)
	gcc -o ../bin/paraNode $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip ../bin/paraNode

