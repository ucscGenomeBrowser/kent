#!/usr/bin/env python3

# Program Header
# Name:   Gerardo Perez
# Description: A program that parses the hubCheck output into email drafts for hub authors regarding
#              missing description pages and couldn't open errors
#
# hubCheckDraftEmails.py
#
#
# Version: Python 3.6.5 
#
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

def bash(cmd):
    """Input bash cmd and return stdout"""
    rawOutput = subprocess.run(cmd,check=True, shell=True, stdout=subprocess.PIPE, universal_newlines=True)
    return(rawOutput.stdout.split('\n')[0:-1])

#Make directories for the month (Y-M) 
try:
   os.makedirs("/hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m"), exist_ok=True)
except OSError as e:
    print(f"Error creating directory: {e}")

try:
   os.makedirs("/usr/local/apache/htdocs-genecats/qa/test-results/hubCheckCron/"+datetime.now().strftime("%Y-%m"), exist_ok=True)
except OSError as e:
    print(f"Error creating directory: {e}")

# Creates list for the hub.txt URLs that have the error of missing description pages
descPageMis=[]

# Creates list for the hub.txt URLs that have the error of couldn't open
couldNotOpen=[]

# Gets hubCheck output
output_line=bash("cat /hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/hubCheck_output")

lineCount=0
for line in output_line:
    lineCount=lineCount+1
    line=str(line)
    if "Couldn't open" in line: #Gets each hub.txt that has the error of couldn't open
       couldNotOpen.append(bash("head -"+str(lineCount)+" /hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/hubCheck_output | grep -A 1 '####' | tail -1"))
    if "missing description page" in line: #Gets each hub.txt that has the error of missing description page
       descPageMis.append(bash("head -"+str(lineCount)+" /hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/hubCheck_output | grep -A 1 '####' | tail -1"))

 

def checkDuplicates(list):
    """Input list to get distinct items"""
    newList=[]
    for item in list:
        if item not in newList:
           newList.append(item)
    return newList

def stringTerm(term):
    """Coverts list input to string"""
    newString=str(term)[1:-1].replace("\'", "")
    return newString

def getEmail(hubUrl):
    """Gets email from hubUrl"""
    email=stringTerm(bash("curl -Ls "+hubUrl+" | grep '^email' | awk '{print $2}'"))
    empty=""
    if email==empty:
       email="N/A <---------- Check: https://genecats.gi.ucsc.edu/qa/test-results/publicHubContactInfo/publicHubContact.html"
    return email

count=0
# pattern for line that has number of problems
pattern = r"Found (\d+) problem."
with open("/hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/filtered_output.txt", 'a') as f:
    f.write("#############################################\n")
    # For loop that goes through each line from the hubCheck output
    for line in (bash("cat /hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/hubCheck_output")):
       count=count+1
       # Checks if the line has is number of problems line
       if stringTerm(re.findall(pattern, line)).isdigit():
          if int(stringTerm(re.findall(pattern, line))) >= 6:# If the line is above the limit then write 5 lines of errors, ... and ###
             for l in (bash("head -"+str(count+5)+" /hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/hubCheck_output | tail -7")):
                 f.write(l+'\n')
             f.write('...\n')
             f.write("#############################################\n")
          else: # Else write all the errors within the limit
             for l in (bash("head -"+str(count+ int(stringTerm(re.findall(pattern, line))))+" /hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/hubCheck_output | tail -"+str(int(stringTerm(re.findall(pattern, line)))+2))):
                f.write(l+'\n')
             f.write("#############################################\n")


emailIntro= """Dear UCSC Public Hub author,

I am writing on behalf of the UCSC Genome Browser. We wanted to alert you that your
public track hub at the address:"""


endEmail="""
hubCheck is a command-line utility that checks files in the hub are correctly formatted. If you
would like to run the hubCheck utility on your own machine, you can download the tool from the
utilities directory: 
https://hgdownload.soe.ucsc.edu/downloads.html#utilities_downloads

Please update your public track hub. If you have any questions, please let us know, and we will be
happy to assist. Do not hesitate to let us know if we can help you resolve this situation, e.g. by
updating the URL where the hub is hosted or possibly hosting the files on our servers.

You can reach us at genome-www@soe.ucsc.edu.

Thank you for your interest and contributions,
The UCSC Genome Browser Group
"""

# Gets the total lines number from the hubCheck output 
totalLines=bash("wc -l /hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/filtered_output.txt | tr ' ' '\t' | cut -f1")
with open("/hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/draftedMessages.txt", 'a') as f:
     f.write('##########################\n')
     # For loop that gets each hub.txt that has the error of missing description page
     for item in checkDuplicates(descPageMis):
             f.write('Send email to: '+getEmail(stringTerm(item))+'\n')
             f.write(emailIntro % item)
             # For loop that gets hubCheck output for each hub.txt that has the error of missing description page
             for line in (bash("cat /hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/filtered_output.txt | grep -A "+stringTerm(totalLines)+" "+stringTerm(item))):
                if '#' not in line:
                   f.write(line+'\n')
                else: break
             f.write("\nWhen running hubCheck "+stringTerm(item)+'\n')
             f.write(endEmail)
             f.write('##########################\n')
     # For loop that gets each hub.txt that has the error of couldn't open 
     for item in checkDuplicates(couldNotOpen):
             f.write('Send email to: '+getEmail(stringTerm(item))+'\n')
             f.write(emailIntro % item)
             # For loop that gets hubCheck output for each hub.txt that has the error of couldn't open
             for line in (bash("cat  /hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/filtered_output.txt | grep -A "+stringTerm(totalLines)+" "+stringTerm(item))):
                if '#' not in line:
                   f.write(line+'\n')
                else: break
             f.write("\nWhen running hubCheck "+stringTerm(item)+'\n')
             f.write(endEmail)
             f.write('##########################\n')

bash("cp /hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m")+"/draftedMessages.txt /usr/local/apache/htdocs-genecats/qa/test-results/hubCheckCron/"+datetime.now().strftime("%Y-%m"))
print("Check https://genecats.gi.ucsc.edu/qa/test-results/hubCheckCron/"+datetime.now().strftime("%Y-%m")+"/draftedMessages.txt to email hub authors about missing/broken public hub files")
print("Archive of monthly raw data can be found here: /hive/users/qateam/hubCheckCronArchive/"+datetime.now().strftime("%Y-%m"))
