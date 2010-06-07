include ../../../inc/common.mk

# copy updated scripts to bin dir

SCRIPT_DIR = /cluster/bin/scripts

SCRIPTS = \
	makeLoChain-align.csh \
	makeLoChain-chain.csh \
	makeLoChain-cleanup.csh \
	makeLoChain-lift.csh \
	makeLoChain-load.csh \
	makeLoChain-net.csh \
	makeLoChain-split.csh

all:
	@for F in $(SCRIPTS); do \
            I=$${F%.*}; \
            if test "$$F" -nt "$(SCRIPT_DIR)/$$I"; then \
                echo "installing $$I" in ${SCRIPT_DIR} ;\
                cp $$F $(SCRIPT_DIR)/$$I ;\
                chmod 775 $(SCRIPT_DIR)/$$I ;\
            fi ;\
	done
