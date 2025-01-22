# common variables for CGIs and CGI loader programs

ifeq (${CGI_BIN_USER},)
    CGI_BIN_USER=${CGI_BIN}-${USER}
endif

ifeq (${CGI_BIN_BETA},)
    CGI_BIN_BETA=${CGI_BIN}-beta
endif

# these rules set CGI_BIN_DEST to the right cgi-bin directory depending 
# on the main goal (my (default), alpha or beta)
# this won't work if you supply multiple goals "(make my alpha beta")
# but we do not seem to do that
CGI_BIN_DEST=${CGI_BIN}
ifeq ($(MAKECMDGOALS),)
    CGI_BIN_DEST=${CGI_BIN}-${USER}
endif
ifneq ($(filter $(MAKECMDGOALS),my cgi),)
    CGI_BIN_DEST=${CGI_BIN}-${USER}
endif
ifeq ($(MAKECMDGOALS),beta) 
    CGI_BIN_DEST=${CGI_BIN}-beta
endif
