#!/usr/bin/env python3

# Program Header
# Name:   Gerardo Perez
# Description: A Python program that omits the hubSearchText cron and the makeHelpDocs.sh output when there
# are no errors 
#
#
#
# checkHubSearchCronWrapper.py 
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

# A variable set to 2 due to the two lines to check
checkError=2

# The strings to check that are not errors
check1='words were longer than limit 31 length and were ignored. Run with -verbose=2 to see them.'
check2='The longest failed word length was'

# A list to save the cron output
cron_ouput=[]

count=0
# A for loop that iterates the cron output line by line
for line in sys.stdin:
    count=count + 1
    line=line.strip()
    # Saves the line to list
    cron_ouput.append(line)
    # If the string is present then subtract 1 from the checkError variable
    if check1 in line:
       checkError=checkError-1
    if check2 in line:
       checkError=checkError-1


if checkError == 0 and count == 2: # If both strings are present and there are two line then no cron output  
   quit()
else: # If diffeent strings are present then print cron output 
  for line in cron_ouput:
     print(line)
