#!/usr/bin/env python3

# Program Header
# Name:   Gerardo Perez
# Description: A program that gets a monthly count of hgSearch result clicks from the RR/asia/euro error logs.
#
#
#
# searchedTermsCron
#
#
# Development Environment: VIM - Vi IMproved version 7.4.629
# Version: Python 3.6.5 

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

#Variable to get the current year
year=datetime.now().strftime("%Y")
#Get the last 5 error logs from the RR/euro
latestLogs=bash("ls /hive/data/inside/wwwstats/RR/"+year+"/hgw1")
latestLogs_euro=bash("ls /hive/data/inside/wwwstats/euroNode/"+year+"/")
latestLogs = latestLogs[len(latestLogs)-5:]
latestLogs_euro=latestLogs_euro[len(latestLogs_euro)-5:]

#Make a directory for the month (Y-M) 
try:
   os.makedirs("/hive/users/qateam/searchedTermsCronArchive/"+datetime.now().strftime("%Y-%m"))
except OSError:
        print("mkdir: cannot create directory /hive/users/qateam/searchedTermsCronArchive/"+datetime.now().strftime("%Y-%m")+": File exists")
        sys.exit(1)

#Nodes to check for error logs
nodes = ['RR', 'asiaNode', 'euroNode']
machines = ['hgw1','hgw2'] #Add hgw machines to check

#For loop that goes through the RR/euro/asia error logs, trims down the hgSearch line, and writes to a file
for node in nodes:
    with open('/hive/users/qateam/searchedTermsCronArchive/'+datetime.now().strftime("%Y-%m")+'/hgSearchTrimLogs', 'a') as f:
        if node == 'RR':
            for machine in machines:
                for log in latestLogs:#Copy the 5 latest error logs for each of the rr machines
                    hgSearch=bash("zcat /hive/data/inside/wwwstats/"+node+"/"+year+"/"+machine+"/"+log+"  | grep 'hgSearch' | tr '?' '\t' | cut -f 2 | grep 'search' | uniq")
                    for i in hgSearch:
                        f.write(i+'\n')
        elif node == 'euroNode':
            for log in latestLogs_euro:
                hgSearch=bash(" zcat /hive/data/inside/wwwstats/"+node+"/"+year+"/"+log+" | grep 'hgSearch' | tr '?' '\t' | cut -f 2 | grep 'search' | uniq")
                for i in hgSearch:
                    f.write(i+'\n')
        else:
            for log in latestLogs:
                hgSearch=bash(" zcat /hive/data/inside/wwwstats/"+node+"/"+year+"/"+log+" | grep 'hgSearch' | tr '?' '\t' | cut -f 2 | grep 'search' | uniq")
                for i in hgSearch:
                    f.write(i+'\n')
f.close()

#Remove duplicates with the same hgsid and save the list to a variable 
search_lines= bash("cat /hive/users/qateam/searchedTermsCronArchive/"+datetime.now().strftime("%Y-%m")+"/hgSearchTrimLogs | sort | uniq ")

#For loop that removes hgsid and counts the search term 
searches_count = {}
for term in search_lines:
    if len(term.split('&'))>2:
        term=term.split('&')
        term.pop(1)
        term.reverse()
        term=str(term)[1:-1]
        term=term.split('=')
        term=term[1:3]
        term = str(term)[1:-1]
        term = term.replace("\"", "").replace(", 'search", "").replace("\'", "").replace(",", "")
        if term.lower() in searches_count:
            searches_count[term.lower()] += 1
        else:
            searches_count[term.lower()] = 1

#Sort the count values from largest to smallest and stores to a list           
sorted_searches_counts= sorted(searches_count.values(), reverse=True)

#Make a dictionary with the count values from largest to smallest 
sorted_searches_dict = {}
for i in sorted_searches_counts:
    for k in searches_count.keys():
        if searches_count[k] == i:
            sorted_searches_dict[k] = searches_count[k]

#Write the sorted count values and search terms to a file
file_searches = open('/hive/users/qateam/searchedTermsCronArchive/'+datetime.now().strftime("%Y-%m")+'/searchCount.txt', 'w')
file_searches.write("count"+'\t'+"db term"+'\n')
file_searches.write("--------------------"'\n')
for k in sorted_searches_dict.keys():
    file_searches.write("{}\t{}\n".format(sorted_searches_dict[k], k))
file_searches.close()

#Writes mouse sorted count values and search terms to a file
mouse=['mm10', 'mm39']
mouse_searches = open('/hive/users/qateam/searchedTermsCronArchive/'+datetime.now().strftime("%Y-%m")+'/mouseCount.txt', 'w')
mouse_searches.write("count"+'\t'+"db term"+'\n')
mouse_searches.write("--------------------"'\n')
for k in sorted_searches_dict.keys():
    if k.split(' ')[0] in mouse:
        mouse_searches.write("{}\t{}\n".format(sorted_searches_dict[k], k))
mouse_searches.close()

#Prints info regarding the cron job, top search terms, and top mouse search terms 
print('This cronjob outputs a monthly count of the top search terms using the hgSearch result clicks from the RR/asia/euro error logs. It only counts each hgsid occurrence once. The top search terms and mouse search terms files can be found here: /hive/users/qateam/searchedTermsCronArchive/'+datetime.now().strftime("%Y-%m")+'\n')
print('Top 20 search terms from hgSearch result clicks:')
topTwenty=bash('head -22 /hive/users/qateam/searchedTermsCronArchive/'+datetime.now().strftime("%Y-%m")+'/searchCount.txt')
for term in topTwenty:
    print(str(term))
print('\nTop 15 mouse search terms from hgSearch result clicks:')
topMouse=bash('head -17 /hive/users/qateam/searchedTermsCronArchive/'+datetime.now().strftime("%Y-%m")+'/mouseCount.txt' )
for term in topMouse:
    print(str(term))
print('\nArchive of monthly raw data can be found here: /hive/users/qateam/searchedTermsCronArchive/')

#Remove file that contains hgSearch lines
bash("rm /hive/users/qateam/searchedTermsCronArchive/"+datetime.now().strftime("%Y-%m")+"/hgSearchTrimLogs")
