#!/usr/bin/env python3

# Program Header
# Name:   Gerardo Perez
# Description: A program that determines the target, query, clades, and full GC assembly hub name to run the pairLastz script
#
#
#
# pairLastzWrapper
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
import argparse
from datetime import datetime

# Creates an arguement passer
parser = argparse.ArgumentParser(description="A program that determines the target, query, clades, and full GC assembly hub name to run the pairLastz script")

# Adds arguemets by calling the arguement passer, for assembly one
parser.add_argument("-a1", "--assembly_one", help="Specify assembly one. Ex. hg38 or GCF_001704415.1", required=True)

# Adds arguemets by calling the arguement passer, for assembly two
parser.add_argument("-a2", "--assembly_two", help="Specify assembly two Ex. hg38 or GCF_001704415.1", required=True)

# Parses arguemets through using the parse args method.
args = parser.parse_args()

validClades = ['primate', 'mammal']

#List of prefix names of the native primate databases
primates=['hg','panTro', 'panPan', 'gorGor', 'ponAbe', 'nomLeu', 'chlSab', 'macFas', 'rheMac', 'papAnu', 'papHam', 'nasLar', 'rhiRox', 'calJac', 'saiBol', 'tarSyr']

def bash(cmd):
    """Input bash cmd and return stdout"""
    rawOutput = subprocess.run(cmd,check=True, shell=True, stdout=subprocess.PIPE, universal_newlines=True)
    return(rawOutput.stdout.split('\n')[0:-1])

def bash_v2(cmd):
    """Input bash cmd and return stdout plus stderr"""
    rawOutput = subprocess.run(cmd,check=True, shell=True, stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
    return(rawOutput.stdout)

def goto(gc):
    """Input GC identifier and return full GC assembly hub name"""
    gcX=gc[0:3]
    d0=gc[4:7]
    d1=gc[7:10]
    d2=gc[10:13]
    if gcX == "GCF":
      destDir="/hive/data/genomes/asmHubs/refseqBuild/"+gcX+"/"+d0+"/"+d1+"/"+d2+"/"
    else:
      destDir="/hive/data/genomes/asmHubs/genbankBuild/"+gcX+"/"+d0+"/"+d1+"/"+d2+"/"
    try:
      cdDir = bash("ls -d "+destDir+"/"+gc+"*")
    except subprocess.CalledProcessError:
      sys.stdout = sys.__stdout__
      print('# can not find '+destDir)
      quit()
    cdDir = str(cdDir)[1:-1]
    gcName = cdDir.split('//')[1][:-1]
    return gcName


def getClade(assembly):
    """Input assembly and return clade of assembly"""
    if assembly[0:3]=='GCA' or assembly[0:3]=='GCF':
        chromSizes_1="/hive/data/genomes/asmHubs/"+assembly[0:3]+"/"+assembly[4:7]+"/"+assembly[7:10]+"/"+assembly[10:13]+"/"+assembly+"/"+assembly+".chrom.sizes.txt"
        assembly=goto(assembly)
        find_gcNum=bash("grep "+assembly+" /hive/data/outside/ncbi/genomes/reports/newAsm/all.*.today | head -1")
        find_gcNum=str(find_gcNum)[1:-1]
        try:
               clade=find_gcNum.split('.')[2][:-1]
        except IndexError:
               print('# can not find '+assembly+', the assembly might be suppressed')
               quit()
        if clade in validClades:
            return clade
        else: clade='other'
    else:
        chromSizes_1="/hive/data/genomes/"+assembly+"/chrom.sizes"
        try:
                bash('ls '+chromSizes_1)
                #print(bash('ls '+chromSizes_1))
        except subprocess.CalledProcessError:
                sys.stdout = sys.__stdout__
                print('ls: cannot access '+chromSizes_1+': No such file or directory')
                quit()
        digit_pos= re.search(r"\d", assembly)
        db_edit = assembly[0:digit_pos.start()]
        if db_edit in primates:
            clade='primate'
        else:
            db = bash("hgsql -e \"select genome from dbDb where name='"+assembly+"'\\G\" hgcentraltest | grep  genome | cut -c 9-")
            db = str(db)[1:-1]
            db = bash("hgsql -e \"select clade from genomeClade where genome="+db+"\\G\" hgcentraltest | grep  clade | cut -c 8-")
            db = str(db)[2:-2]
            if db in validClades:
                clade=db
            else: clade='other'
    return clade

def getAssembly(assembly):
    """Input assembly and return native assembly or GC assembly name"""
    if assembly[0:3]=='GCA' or assembly[0:3]=='GCF':
        gcAssembly=goto(assembly)
        return gcAssembly
    else: 
        return assembly

def scoringScheme(assembly):
    """Input assembly and return a score using Hiram's scoring scheme"""
    if assembly[0:3]=='GCA' or assembly[0:3]=='GCF':
        chromSizes_1="/hive/data/genomes/asmHubs/"+assembly[0:3]+"/"+assembly[4:7]+"/"+assembly[7:10]+"/"+assembly[10:13]+"/"+assembly+"/"+assembly+".chrom.sizes.txt"
    else:
        chromSizes_1="/hive/data/genomes/"+assembly+"/chrom.sizes"   
    try:
        nFiftyOutput = bash_v2('n50.pl '+chromSizes_1)
    except subprocess.CalledProcessError:
        sys.stdout = sys.__stdout__
        print('ls: cannot access '+chromSizes_1+': No such file or directory')
        quit()
    nFiftyOutputLineOne=nFiftyOutput.split('\n')[1]
    scores= re.findall(r'[0-9]+', nFiftyOutputLineOne)
    scores.pop(-1)
    scores.append(nFiftyOutput.split('\n')[3].split()[1])
    scores.append(nFiftyOutput.split('\n')[3].split()[3])
    return scores

def compareScores(scoresOne, scoresTwo):
    """Input scores of two assemblies and return a score comparison count"""
    count=0
    if int(scoresOne[0]) < int(scoresTwo[0]):
       count=count+1
   
    if int(scoresOne[1]) > int(scoresTwo[1]):
       count=count+1
   
    if int(scoresOne[2]) == int(scoresTwo[2]):
       count=count+0.5
   
    elif int(scoresOne[2]) < int(scoresTwo[2]):
       count=count+1
       
    if int(scoresOne[3]) > int(scoresTwo[3]):
       count=count+1
    return count



def main(assembly_one, assembly_two):
    """Input two assemblies and prints target, query, clades, and full GC assembly hub name to run pairLastz script"""
    sys.stdout = sys.__stdout__
    print("screen -S "+datetime.now().strftime("%Y%m%d")+"\n")
    count=compareScores(scoringScheme(assembly_one), scoringScheme(assembly_two))
    if count==2:
        if int(scoringScheme(assembly_one)[1])>int(scoringScheme(assembly_two)[1]):
            print("time (~/kent/src/hg/utils/automation/pairLastz.sh "+getAssembly(assembly_one)+" "+getAssembly(assembly_two)+" "+getClade(assembly_one)+" "+getClade(assembly_two)+") >  "+assembly_one+"."+assembly_two+"_"+datetime.now().strftime("%Y%m%d")+".log 2>&1 &")
            quit()
        else:
            print("time (~/kent/src/hg/utils/automation/pairLastz.sh "+getAssembly(assembly_two)+" "+getAssembly(assembly_one)+" "+getClade(assembly_two)+" "+getClade(assembly_one)+") >  "+assembly_two+"."+assembly_one+"_"+datetime.now().strftime("%Y%m%d")+".log 2>&1 &")
            quit()
    if count >= 2.5:
        print("time (~/kent/src/hg/utils/automation/pairLastz.sh "+getAssembly(assembly_one)+" "+getAssembly(assembly_two)+" "+getClade(assembly_one)+" "+getClade(assembly_two)+") >  "+assembly_one+"."+assembly_two+"_"+datetime.now().strftime("%Y%m%d")+".log 2>&1 &")
        quit()
    else:
        print("time (~/kent/src/hg/utils/automation/pairLastz.sh "+getAssembly(assembly_two)+" "+getAssembly(assembly_one)+" "+getClade(assembly_two)+" "+getClade(assembly_one)+") >  "+assembly_two+"."+assembly_one+"_"+datetime.now().strftime("%Y%m%d")+".log 2>&1 &")
        quit()       
    return

main(args.assembly_one, args.assembly_two)

# Program Output (Commented out)

#python3 pairLastzWrapper.py -a1 GCF_001704415.1 -a2 hg38 
#screen -S 20220323

#time (~/kent/src/hg/utils/automation/pairLastz.sh hg38 GCF_001704415.1_ARS1 primate mammal) >  hg38.GCF_001704415.1_20220323.log 2>&1 &
