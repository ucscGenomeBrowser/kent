#!/usr/bin/env python3

# Program Header
# Name:   Gerardo Perez
# Description: A program that gets user sessions from the RR and checks if sessions crash on hgwdev/hgwbeta
#
#
#
# checkSessionsFromRR.py
#
#
# Development Environment: VIM - Vi IMproved version 7.4.629
# Version: Python 3.6.5 
#
# IGNORE the following error notes: 
# For error:
#           hExtendedRangeQuery: table t1_genome_65e19_d5ded0 doesn't exist in customTrash database, or hFindTableInfoWithConn failed
#           bedGraphLoadItems: table t1_genome_65e19_d5ded0 only has 0 data columns, must be at least 4 
#This error is due to custom tracks in track collections expiring which were stored as a hub. 
# ^IGNORE

import os # Module to interact with the operating system
import getpass # Module to get the username of the person running the script
import sys # Module to access system-specific parameters and functions
import re # Module for regular expression matching operations
import json # Module for working with JSON data
import io # Module for handling I/O operations
import requests # Module to make HTTP requests
import random # Module to generate random numbers and select random items
import subprocess # Module to run subprocesses
from datetime import datetime # Module to handle dates and times
from urllib.parse import unquote # Module to decode URL-encoded strings

# Get the username of the person running the script
user = getpass.getuser()

def bash(cmd):
    """Input bash cmd and return stdout"""
    rawOutput = subprocess.run(cmd,check=True, shell=True, stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT, errors='backslashreplace')
    return(rawOutput.stdout.split('\n')[0:-1])

def random_session_lines(file_path, num_lines):
    """Gets a number of random lines that consist of session contents and encodes as Latin-1 of the Unicode character set"""
    with open(file_path, 'r', encoding="latin-1") as file:
        lines = file.readlines()
        selected_lines = random.sample(lines, num_lines)
        return selected_lines

def parseSessions(session_line):
    """Gets the field of the line that has the session contents """
    session=session_line.strip().split('\t')
    if len(session)==6:
        contents=session[1]
    elif len(session)==7:
       contents=session[2]
    else:
        contents='missing'
    # Strings to convert in order for the settings to load as a session
    contents=unquote(contents)
    contents=contents.replace("&", "\n")
    contents=contents.replace("=", " ")
    contents=contents.replace("group+auto-scale", "group auto-scale")
    contents=contents.replace("use+vertical+viewing+range+setting", "use vertical viewing range setting")
    contents=contents.replace("auto-scale+to+data+view", "auto-scale to data view")
    contents=contents.replace("+/userdata/", " /userdata/")
    contents=contents.replace("+../trash", " ../trash")
    contents=contents.replace("Normalized+Score", "Normalized Score")
    contents=contents.replace("all+genes", "all genes")
    contents=contents.replace("Roswell+Park+Cancer+Institute", "Roswell Park Cancer Institute")
    contents=contents.replace("Gray+scale", "Gray scale")
    return contents

def getSessionContents(session_contents, line):
    """Saves the contents of a session to a file with utf-8 encoding"""
    save_sessions='/hive/users/qateam/sessionsFromRR'
    session_settings=save_sessions+'/session_settings_'+str(line)+'.txt'
    write_session_settings= open(session_settings, 'w', encoding='utf-8')
    write_session_settings.write(session_contents)
    write_session_settings.close()
    return session_settings 

def loadSessionhgw0(session, machine):
    """Makes session URL to load on hgw0"""
    session=session.split('qateam')[1]
    session=machine+server+session
    return session

def checkSessionhgw0(session):
    """Checks if session loads on hgw0"""
    #String to check when hgTracks finishes loading
    check_hgTracks='// END hgTracks'
    
    try:
         checkLoad=bash("curl -Ls '%s'" % session)
         try:
              checkLoad=bash("curl -Ls '%s'" % session)
              checkLoad=str(checkLoad)[1:-1]
              #If the string to check when hgTracks finishes loading is present then save the session to a variable
              if check_hgTracks in checkLoad:
                 sessionLoads=session
              else: # If string is not present then delete the file with the session contents 
                  sessionLoads='no'
                  session_dir=session.split('genecats.gi.ucsc.edu')[1]
                  session_path=myDir+session_dir
                  # Uncomment the line below to actually remove the session files
                  #os.system('rm '+session_path)
         except subprocess.CalledProcessError:
             sessionLoads='no'
    except subprocess.CalledProcessError:
        sessionLoads='no'

    return sessionLoads

def loadSession(session, machine):
    """Makes session URL to load on machine"""
    session=session.split('cgi-bin')[1]
    session=machine+session
    return session

def checkSession(session):
    """Checks if session loads on machine"""

    #Strings to check
    check_hgTracks='// END hgTracks'
    check_error='<!-- ERROR -->'
    check_warning="id='warnBox'"
    hubid_error="Couldn't connect to database"
    hubid_error_dev="can not find any trackDb tables for hub_"
    hubCollection_error= r'doesn\\x27t\\x20exist\\x20in\\x20customTrash\\x20database\\x2C\\x20or\\x20hFindTableInfoWithConn\\x20failed'
    buffer_error= r'buffer\\x20overflow\\x2C\\x20size\\x204096\\x2C\\x20format\\x3A\\x20Click\\x20to\\x20alter\\x20the\\x20display'
    hub_error="Error: The database"
    genomeName_error="Error: The genome"
    # List to append session load error
    error_list=[]
    try:
        checkLoad=bash("curl -Ls '%s'" % session)
        checkLoad=str(checkLoad)[1:-1]
        
        #If session contains strings to check if session loaded, set a variable that session loaded
        if check_hgTracks in checkLoad:
            if check_error in checkLoad: #checks if there is an error when session is loaded
               if check_warning in checkLoad: #checks if the error is a warning when session is loaded
                  check3='loads'
               else: #If session contains the error string to check, add error to list 
                  error_list.append('error')
            check2='loads'
       #Pass if session load error is Couldn't connect to database hub error (hub id issue)
        elif hubid_error in checkLoad:
            pass
       #Pass if session load error is can not find any trackDb tables for hub_ ((hub id issue))
        elif hubid_error_dev in checkLoad:
            pass
        # Check for the hubCollection error by matching the exact string pattern
        elif re.search(re.escape(hubCollection_error), checkLoad):
            pass
        # Check for buffer overflow error by matching the exact string pattern
        elif re.search(re.escape(buffer_error), checkLoad):
            pass
       #Pass if session load error is Error: The database...
       #The hubId is not in hubStatus at all, that is, it's never been connected.
       #The hub has an error in the hubStatus table.
        elif hub_error in checkLoad:
            pass
       #Pass if session load error is Error: The genome
       #This would most likely happen if the hub used to have that genome but doesn't anymore.
        elif genomeName_error in checkLoad:
            pass
        else: #If session does not contains strings to check, add error to list 
             error_list.append('error')
    except subprocess.CalledProcessError as e:
        #If session fails to curl, add error to list 
        error_list.append('error')
    #If an error is present in the error list, save the session URL to variable 
    if 'error' in error_list:
        sessionLoad=session
    else: #If no error is present in the error list, set variable that session loaded
        sessionLoad='loads'
        session_dir=session.split('genecats.gi.ucsc.edu')[1]
        session_path=myDir+session_dir
   
    return sessionLoad

def makeURL(session, url_txt, count):
    """Creates a text file that appends crash sessions"""
    failed_session=open(url_txt, 'a')
    failed_session.write(str(datetime.now().strftime("%Y-%m-%d %H:%M:%S"))+'\t'+str(session)+'\n')
    failed_session.close()
    return session

def output_if_file_exists(file_path):
    """Outputs if text file was created for crash sessions"""
    if os.path.exists(file_path):
        print("This cronjob outputs sessions that crash on hgwbeta or hgwdev. The session URLs have '#' in ...session_settings_#.txt which is the random number session tested")
        with open(file_path, 'r') as file:
            print()
            print(file.read())
        print("\nErrors that output 'hui::wiggleScaleStringToEnum() - Unknown option' can be ignored.")


server='https://genecats.gi.ucsc.edu/qa/qaCrons'


num_lines=10000 # Number of random lines to select

# Get the current date and format it as 'YYYY-MM-01'
current_year_month = datetime.now().strftime("%Y-%m-"+"01")

# Construct the path for the current month's file
monthly_hgcentral_dump = f"/usr/local/apache/htdocs-genecats/qa/test-results/hgcentral/{current_year_month}/rr.namedSessionDb"

# Check if the file exists for the current month
if not os.path.exists(monthly_hgcentral_dump):
    # If the file doesn't exist, calculate the previous month
    current_year = int(datetime.now().strftime("%Y"))
    current_month = int(datetime.now().strftime("%m"))

    # Handle previous month calculation manually
    if current_month == 1:
        previous_year = current_year - 1
        previous_month = 12
    else:
        previous_year = current_year
        previous_month = current_month - 1

    # Format the previous month as 'YYYY-MM-01'
    previous_year_month = f"{previous_year:04d}-{previous_month:02d}-01"

    # Construct the path for the previous month's file
    monthly_hgcentral_dump = f"/usr/local/apache/htdocs-genecats/qa/test-results/hgcentral/{previous_year_month}/rr.namedSessionDb"

    # Check if the file exists for the previous month
    if not os.path.exists(monthly_hgcentral_dump):
        print(f"Error: rr.namedSessionDb monthly hgcentral dump file not found for both current and previous month.")
    else:
        # File for the previous month found, proceed to get random lines
        random_lines = random_session_lines(monthly_hgcentral_dump, num_lines)
else:
    # File for the current month found, proceed to get random lines
    random_lines = random_session_lines(monthly_hgcentral_dump, num_lines)



count=0

#Machine partial URL to load session
hgw0='http://hgw0.soe.ucsc.edu/cgi-bin/hgTracks?hgS_doLoadUrl=submit&hgS_loadUrlName='
hgwbeta='http://hgwbeta.soe.ucsc.edu/cgi-bin'
hgwdev='http://hgwdev.gi.ucsc.edu/cgi-bin'

#Path to file to append  crash sessions
url_txt='/usr/local/apache/htdocs-genecats/qa/qaCrons/sessionsFromRR/crashedSessions.txt'

#Directory to save files for the script 
myDir='/usr/local/apache/htdocs-genecats'


def main(random_lines, server, count, hgw0, hgwbeta, hgwdev, url_txt, myDir):
    """ Gets a number of random of sessions from the RR, if sessions crash on hgwdev/hgwbeta then outputs the crash sessions"""
    #Remove session contents files
    os.system("rm /hive/users/qateam/sessionsFromRR/*")

    for line in random_lines:
        count=count +1
        session_contents=parseSessions(line)
        if session_contents=='missing':
           continue
        session=getSessionContents(session_contents, count)
        hgw0session=loadSessionhgw0(session, hgw0)
        session=checkSessionhgw0(hgw0session)
        if session=='no':
           continue 
        hgwbetaSession=loadSession(session, hgwbeta)
        beta_session=checkSession(hgwbetaSession)
        if beta_session=='loads':
           pass
        else: 
             makeURL(beta_session, url_txt, count) 
        hgwdevSession=loadSession(session, hgwdev)
        dev_session=checkSession(hgwdevSession)
        if dev_session=='loads':
           continue
        else: 
            makeURL(dev_session, url_txt, count)
    output_if_file_exists(url_txt)

if __name__ == "__main__":
    sys.exit(main(random_lines, server, count, hgw0, hgwbeta, hgwdev, url_txt, myDir))

# Program Output (Commented out)
#This cronjob outputs sessions that crash on hgwbeta or hgwdev. The session URLs have '#' in ...session_settings_#.txt which is the random number session tested


#2024-05-09 00:21:25	http://hgwbeta.soe.ucsc.edu/cgi-bin/hgTracks?hgS_doLoadUrl=submit&hgS_loadUrlName=https://genecats.gi.ucsc.edu/qa/qaCrons/sessionsFromRR/session_settings_4642.txt
#2024-05-09 00:21:56	http://hgwdev.gi.ucsc.edu/cgi-bin/hgTracks?hgS_doLoadUrl=submit&hgS_loadUrlName=https://genecats.gi.ucsc.edu/qa/qaCrons/sessionsFromRR/session_settings_4642.txt

#Errors that output 'hui::wiggleScaleStringToEnum() - Unknown option' can be ignored.
