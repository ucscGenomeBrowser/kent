#!/usr/bin/env python3
# WARNING: This can spam user emails - be careful

import logging, sys, optparse, os, sys, urllib3, atexit, urllib
from os.path import isfile
from email.mime.text import MIMEText
import subprocess
from subprocess import Popen, PIPE

# Define a function to execute a shell command and capture its output
def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        # Run the command with subprocess.run, capturing stdout and stderr
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout  # Extract standard output
    except subprocess.CalledProcessError as e:
        # Handle errors by raising an exception with relevant details
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return bashStdoutt

# Define a function to send an email
def sendEmail(dest, body):
    """Send email to dest"""
    msg = MIMEText(body)  # Create the email body
    msg["From"] = fromEmail  # Set the sender email
    # Combine recipient emails (to dest and a hardcoded email)
    msg["To"] = dest+","+"browserqa-group@ucsc.edu"
#     msg["To"] = dest + "," + "lrnassar@ucsc.edu" #For troubleshooting
    msg["Subject"] = "Your UCSC Genome Browser Hub Usage Statistics"  # Email subject
    # Send the email using sendmail command
    p = Popen(["/usr/sbin/sendmail", "-t", "-oi"], stdin=PIPE)
    p.communicate(msg.as_bytes())

# Sender email
# fromEmail = "lrnassar@ucsc.edu" #For testing
fromEmail="qateam@gi.ucsc.edu"

# Email template with placeholders for hub name and unique hits
emailTemplate = """Dear UCSC Public Hub Author,

This is an automated email from the UCSC Genome Browser Group. 

Your public hub %s received %s unique users over the last calendar year (January 2024 - December 2024). A unique user is defined as a continuous browsing session with at least two page loads, typically unique to the web browser tab.

Thank you for your continued support and contributions to the UCSC Genome Browser community. Your hub plays an essential role in providing valuable data to researchers and users worldwide.

If you have any questions or need assistance, please don’t hesitate to contact us at genome-www@soe.ucsc.edu. We’re here to help.

Warm regards,
The UCSC Genome Browser Group
"""

# Dictionary to store hub usage statistics
myDic = {}
# Base file path for usage data
fileUrl = "/usr/local/apache/htdocs-genecats/qa/test-results/usageStats/publicHubUsageCounts/pubHubUsageCounts."
# List of monthly data files
dates = ['2024-11.txt', '2024-10.txt', '2024-09.txt', '2024-08.txt']

# Parse usage data from the listed files
n = 0
for date in dates:
    n += 1
    fileToOpen = fileUrl + date  # Construct the file path
    trackList = bash("cat " + fileToOpen).split("\n")  # Read the file contents
    for line in trackList:
        if line and not any(line.startswith(prefix) for prefix in ["This", "individual", "\n", "assembly"]):
            line = line.split("\t")  # Split the line by tab
            databse = line[0]  # Database field
            useCount = int(line[1])  # Usage count field
            hubName = line[3]  # Hub name field
            if n == 1:
                # Initialize entry for the first file
                myDic[hubName] = {'useCount': useCount}
            else:
                # Accumulate usage counts for subsequent files
                if hubName not in myDic:
                    myDic[hubName] = {'useCount': useCount}
                else:
                    myDic[hubName]['useCount'] += useCount

# Read hub public email status
hubEmailFile = bash("cat /cluster/home/qateam/cronScripts/hubPublicMailStatus.tab").split("\n")

# Adjust usage counts and retrieve hub URLs
for key in myDic.keys():
    myDic[key]['useCount'] *= 3  # Multiply usage counts by 3 for averaging
    # Query hub URL using hgsql
    hubUrl = bash("""hgsql -e "select hubUrl from hubPublic where shortLabel='"""+key+"""'" hgcentraltest""").split("\n")[1]
    myDic[key]['hubUrl'] = hubUrl  # Save hub URL
    # Match hub URL to email address in the hubEmailFile
    for line in hubEmailFile:
        if myDic[key]["hubUrl"] in line:
            line = line.split("\t")
            myDic[key]['email'] = line[1]  # Save email address

# Send emails
n = 0
for key in myDic.keys():
    n += 1
    # Retrieve email details for each hub
#     destEmail = "lrnassar@ucsc.edu"  # For testing
    destEmail = myDic[key]['email']
    hubName = key
    uniqueHits = myDic[key]['useCount']
    emailText = emailTemplate % (hubName, uniqueHits)  # Format email text
#     sendEmail(destEmail, emailText) # This can be used to just go full send
#     print(destEmail, emailText) # For testing

# The section below can be used to test one email first, then uncomment the 2nd to send the rest

#     if n == 1:
#         sendEmail(destEmail, emailText)  # Send the first email for testing
#     if n != 1:
#         sendEmail(destEmail, emailText)

    if "email" not in myDic[key]:
        print("Problem with: " + key)  # Report missing email information

# Print the total number of emails sent
print("Total number of emails sent: " + str(n))
