#!/usr/bin/env python3

# Program Header
# Name:   Gerardo Perez
# Description: A Python program that omits the checkSync/checkMetaData All cron output when there
# are no errors 
#
#
#
# checkSyncMetaDataWrapper
#
#
# Development Environment: VIM - Vi IMproved version 7.4.629
# Version: Python 3.6.5 

# Imports module
import os
import getpass
import sys
import re
import json
import io
import requests
import subprocess
from datetime import datetime

user = getpass.getuser()


# A variable set to 2 due to the errors to check: checkSync and checkMetaData
checkError=2

# The string to check for no errors
check1='There were no errors found between hgw1/hgw2 and beta tables (checkSync.csh)'
check2='There were no errors found between rr and hgwbeta MetaData (checkMetaData.csh)'

# A list to save the cron output
cron_ouput=[]

# A for loop that iterates the cron output line by line
for line in sys.stdin:
    line=line.strip()
    # Saves the line to list
    cron_ouput.append(line)
    # If the no error string is present then subtract 1 from the checkError variable
    if line==check1:
       checkError=checkError-1
    if line==check2:
       checkError=checkError-1


if checkError == 0: # If both no error strings are present then checkError variable is set to 0 and no cron output  
   quit()
else: # If an error is present then print cron output 
  for line in cron_ouput:
     print(line)
