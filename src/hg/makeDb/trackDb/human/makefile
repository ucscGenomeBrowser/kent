# Descend into any directories with makefiles

all::
	@for D in *; do \
            if [ -d "$$D" ]; then \
                if [ -e "$$D/makefile" ]; then \
                    ( cd $$D && echo $$D && $(MAKE) ) ;\
                    x=$$? ; if [ $$x -ne 0 ]; then exit $$x; fi \
                fi; \
            fi; \
	done

clean::
	@for D in *; do \
            if [ -d "$$D" ]; then \
                if [ -e "$$D/makefile" ]; then \
                    ( cd $$D && echo $$D && $(MAKE) clean ) ;\
                    x=$$? ; if [ $$x -ne 0 ]; then exit $$x; fi \
                fi; \
            fi; \
	done
