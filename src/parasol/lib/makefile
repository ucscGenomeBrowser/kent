include ../../inc/common.mk



O = paraLib.o machSpec.o

$(MACHTYPE)/paralib.a: $(O) 
	ar rcu ../lib/$(MACHTYPE)/paralib.a $(O)
	ranlib ../lib/$(MACHTYPE)/paralib.a
