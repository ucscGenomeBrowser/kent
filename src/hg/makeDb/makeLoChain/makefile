include ../../../inc/common.mk

# copy updated scripts to bin dir

SCRIPT_DIR = /cluster/bin/scripts

SCRIPTS = \
	makeLoChain-align \
	makeLoChain-lift \
        makeLoChain-chain \
	makeLoChain-net \
	makeLoChain-load \

all:
	@for F in $(SCRIPTS); do \
            if test "$$F.csh" -nt "$(SCRIPT_DIR)/$$F"; then \
                echo "installing $$F" ;\
                cp $$F.csh $(SCRIPT_DIR)/$$F ;\
                chmod 775 $(SCRIPT_DIR)/$$F ;\
            fi ;\
	done
