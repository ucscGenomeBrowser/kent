#!/usr/bin/env python3

import subprocess, os, gzip, argparse, sys, json, operator, datetime
from collections import Counter, defaultdict

#####
# Define dictionaries/sets/etc. needed to record stats
#####
# Making these global so that they can be modified by functions whithout needing
#   a function argument to specify them
# Dictionaries for holding information about db use
dbUsers = defaultdict(Counter)
# Ex dbUsers struct: {"db":{"hgsid":count}}
dbCounts = dict()
# Ex dbCounts struct: {"db":count}
# Dictionaries for recording information per month
dbUsersMonth = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: defaultdict())))
# Ex dbUsersMonth struct: {"db":{"year":{"month":{"hgsid":count}}}}
dbCountsMonth = defaultdict(lambda: defaultdict(lambda: defaultdict()))
# Ex dbUsersMonth struct: {"db":{"year":{"month":count}}}}

# Dictionaries for holding information about track use per db
trackUsers = defaultdict(lambda: defaultdict(lambda: defaultdict()))
# Ex trackUsers struct: {"db":{"track":{"hgsid":count}}}
trackCounts = defaultdict(dict)
# Ex trackCounts struct: {"db":{"track":count}}
# Dictionaries for per month information
trackUsersMonth = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda:\
  defaultdict(lambda: defaultdict()))))
# Ex trackUsersMonth struct: {"db":{"year":{"month":{"track":{"hgsid":count}}}}}
trackCountsMonth = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda:\
  defaultdict())))
# Ex trackCountsMonth struct: {"db":{"year":{"month":{"track":count}}}}

# Dictionaries for holding information about track use per hub
trackUsersHubs = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda:
  defaultdict())))
# Ex trackUsersHubs struct: {"hubId":{"db":{"track":{"hgsid":count}}}}
trackCountsHubs = defaultdict(lambda: defaultdict(lambda: defaultdict()))
# Ex trackCountsHubs struct: {"hubId":{"db":{"track":count}}}
# Dictionaries for per month information
trackUsersHubsMonth = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda:\
  defaultdict(lambda: defaultdict(lambda: defaultdict())))))
# Ex trackUserHubsMonth struct: {"hubId":{"db":{"year":{"month":{"track":{"hgsid":count}}}}}}
trackCountsHubsMonth = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: defaultdict(
  lambda: defaultdict()))))
# Ex trackCountsHubsMonth struct: {"hubId":{"db":{"year":{"month":{"track":count}}}}}

monthYearSet = set() # Set containing monthYear strings (e.g. "Aug 2017")

# Create a dictionary of hubUrls, shortLabels, and hubStatus ids
publicHubs = dict()
# Use hgsql to grab hub ID from hubStatus table and shortLabel from hubPublic
# for each hub in hubPublic table
cmd = ["/cluster/bin/x86_64/hgsql", "hgcentral", "-h", "genome-centdb", "-Ne", "select s.id,p.hubUrl,p.shortLabel\
       from hubPublic p join hubStatus s where s.hubUrl=p.hubUrl", ]
p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
cmdout, cmderr = p.communicate()
hubs = cmdout.decode("ASCII")
hubs = hubs.split("\n")
for hub in hubs:
    if hub == "":
        continue
    else:
        splitHub = hub.split("\t")
        publicHubs[splitHub[0]] = [splitHub[1], splitHub[2]]

#####
#####

def parseTrackLog(line):
    """Parse trackLog line and return important fields: db, year, month, hgsid,
       and a list of tracks"""
    #### Sample line being processed ####
    # From Chris Lee's combined/trimmed log format produced by {script name}
    # [Sun Mar 05 04:11:27 2017] [error] [client ###.###.###.##] trackLog 0 hg38 hgsid_### cytoBandIdeo:1,cloneEndCTD:2

    ####
    splitLine = line.strip().split('\t')
    date = splitLine[3]
    month = date.split()[1]
    year = date.split()[4]
    ip = splitLine[1]
    hgsid = splitLine[2]
    # temporary, really should rewrite script to use a var like ip_hgsid everywhere
    #hgsid = ip + "_" + hgsid
    db = splitLine[4]
    if len(splitLine) > 5:
        tracks = splitLine[5].split(",")
    else:
        tracks = []
    return db, year, month, hgsid, tracks

def modDicts(db, year, month, hgsid, tracks, toProcess, perMonth=False):
    """Modify global dictionaries to store information on 
       db, track, and hub track usage"""
 
    ##### Set some variables based on input options
    processDbUsers = toProcess[0]
    processTrackUsers = toProcess[1]
    processTrackHubUsers = toProcess[2]

    ##### Process info about db usage
    # Count up number of times each hgsid shows up
    
    if processDbUsers == True:
        if hgsid not in dbUsers[db]:
            # If no entry in dictionary of users, intialize value to 1
            dbUsers[db][hgsid] = 1
        else:
            # Otherwise, increment usage by 1
            dbUsers[db][hgsid] += 1
        # If perMonth is true, then we record information about db usage on a perMonth basis
        if perMonth == True:
            if hgsid not in dbUsersMonth[db][year][month]:
                dbUsersMonth[db][year][month][hgsid] = 1
            else:
                dbUsersMonth[db][year][month][hgsid] += 1

    ##### Process per track information
    if processTrackUsers == True:
        for track in tracks:
            # Skip empty entries in trackList
            if track == "":
                continue
            # Remove trailing characters
            track = track[:-2]

            # Record track user
            if hgsid not in trackUsers[db][track]:
                trackUsers[db][track][hgsid] = 1
            else:
                trackUsers[db][track][hgsid] += 1

            # If perMonth is true, then we record information about track usage on a perMonth basis
            if perMonth == True:
                # Incremement count for track/hgsid by one
                if hgsid not in trackUsersMonth[db][year][month][track]:
                    trackUsersMonth[db][year][month][track][hgsid] = 1
                else:
                    trackUsersMonth[db][year][month][track][hgsid] += 1

            ##### Process public hub tracks
            if processTrackHubUsers == True:
                if track.startswith("hub_"):
                    track = track[:-2]
                    splitTrack = track.split("_", 2)
                    if len(splitTrack) > 2:
                        hubId = splitTrack[1]
                        hubTrack = splitTrack[2]

                        # Processed in same way as normal tracks, although now the top level dictionary is
                        #  keyed by hubIds, rather than db name
                        if hubId in publicHubs:
                            if hgsid not in trackUsersHubs[hubId][db][hubTrack]:
                                trackUsersHubs[hubId][db][hubTrack][hgsid] = 1
                            else:
                                trackUsersHubs[hubId][db][hubTrack][hgsid] += 1

                            if perMonth == True:
                                if hgsid not in trackUsersHubsMonth[hubId][db][year][month][hubTrack]:
                                    trackUsersHubsMonth[hubId][db][year][month][hubTrack][hgsid] = 1
                                else:
                                    trackUsersHubsMonth[hubId][db][year][month][hubTrack][hgsid] += 1

def processFile(fileName, toProcess, perMonth=False):
    """Process a file line by line using the function parseTrackLog and record usage information using
       the function modDicts"""
    if fileName.endswith(".gz"):
        ifh = gzip.open(fileName, "r")
    else:
        ifh = open(fileName, "r")
    for line in ifh:
        if "str" not in str(type(line)):
            line = line.decode("ASCII")
        db, year, month, hgsid, tracks = parseTrackLog(line)
        modDicts(db, year, month, hgsid, tracks, toProcess, perMonth)
        # Keep track of month/years covered
        monthYear = month + " " + year
        monthYearSet.add(monthYear)

def processDir(dirName, toProcess, perMonth=False):
    """Process files in a directory using processFile function"""
    fileNames = os.listdir(dirName)
    for log in fileNames:
        fileName = os.path.join(dirName, log)
        processFile(fileName, toProcess, perMonth)

def dumpToJson(data, outputFile, outputDir):
    """output data to named outputFile"""
    jsonOut = open(os.path.join(outputDir, outputFile), "w")
    json.dump(data, jsonOut)
    jsonOut.close()

def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="Generates usage statistics for dbs, tracks, and hubs \
tracks using processed Apache error_log files. \nThe processed files can be \
found in the following directory: /hive/users/chmalee/logs/trimmedLogs/result\n\n\
For more information, see RM#26191.")
    parser.add_argument("-f","--fileName", type=str, help='input file name, \
must be space-separated Apache error_log file')
    parser.add_argument("-d","--dirName", type=str , help='input directory \
name, files must be space-separated error_log files. No other files should be \
present in this directory.')
    parser.add_argument("--dbCounts", action='store_true', help='output a \
file containing users per ucsc database')
    parser.add_argument("--dbUsers", action='store_true', help='output a \
file containing hgsids associated with each ucsc databases and how many uses each one had')
    parser.add_argument("--dbUsage", action='store_true', help='output a \
file containing hgsids associated with each ucsc databases and how many uses each one had')
    parser.add_argument("--trackCounts", action='store_true', help='output a \
file containing counts of how many users had a track on for a particular ucsc database')
    parser.add_argument("--trackUsers", action='store_true', help='output a \
file containing hgsids, the tracks associated with them and how many trackLog lines they showed up in')
    parser.add_argument("--trackUsage", action='store_true', help='output a \
file containing hgsids associated with each ucsc databases and how many uses each one had')
    parser.add_argument("--trackHubCounts", action='store_true', help='output a \
file containing counts of how many users had a hub track on for a particular ucsc database or assembly hub')
    parser.add_argument("--trackHubUsers", action='store_true', help='output a \
file containing hgsids, the hub tracks associated with them and how many trackLog lines they showed up in')
    parser.add_argument("--trackHubUsage", action='store_true', help='output a \
file containing hgsids associated with each ucsc databases and how many uses each one had')
    parser.add_argument("--allOutput", action='store_true', help='output a \
file for each option above (dbCounts, dbUsers, dbUsage, trackCounts, trackUsers, trackHubCounts, trackHubUsers)')
    parser.add_argument("-p","--perMonth", action='store_true', help='output \
file containing info on db/track/hub track use per month')
    parser.add_argument("-m","--monthYear", action='store_true', help='output \
file containing month/year pairs (e.g. "Mar 2017")')
    parser.add_argument("-j","--jsonOut", action='store_true', help='output \
json files for summary dictionaries')
    parser.add_argument("-t","--outputDefaults", action='store_true',
help='output a file containing info on default track usage for top 15 most used assemblies')
    parser.add_argument("-o","--outDir", type=str, help='directory in which to place output files')
    args = parser.parse_args()

    # Print help message if no arguments are supplied
    if len(sys.argv) == 1:
        parser.print_help(sys.stderr)
        sys.exit(1)

    # File and directory options can't be used together. Catch this and exit.
    if args.fileName != None and args.dirName != None:
        print("-f/--fileName and -d/--dirName cannot be used together. Choose one and re-run.")
        sys.exit(1)

    # Catch it early if input file/directory doesn't exist and exit.
    if args.fileName:
        if not os.path.exists(args.fileName):
            print(args.fileName, "doesn't exist. Please run on a valid file.")
            exit(1)
    elif args.dirName:
        if not os.path.exists(args.dirName):
            print(args.dirName, "doesn't exist. Please run on a valid directory.")
            exit(1)

    if not any([args.dbCounts, args.dbUsers, args.dbUsage, args.trackUsers, args.trackCounts, args.trackUsage, args.trackHubUsers, args.trackHubCounts, args.trackHubUsage, args.allOutput]):
        print("You must specify at least one of: --allOutput, --dbCounts, --dbUsage, --dbUsers, --trackCounts, --trackUsers, --trackHubCounts, --trackHubUsers.")
        exit(1)

    # Setup output directory
    if args.outDir == None:
        # If an output directory is unspecified, then a new one with the current date/time is made
        currDateTime = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        os.makedirs(currDateTime)
        outDir = currDateTime
    else:
        # Otherwise, user supplied a output directory name
        outDir = args.outDir
        if not os.path.exists(outDir):
	    # If specied output directory doesn't exist, create it
            os.makedirs(outDir)

    # If "all" option is set change everyone to True
    if args.allOutput == True:
        args.dbCounts = True
        args.dbUsers = True
        args.dbUsage = True
        args.trackUsers = True
        args.trackCounts = True
        args.trackUsage = True
        args.trackHubUsers = True
        args.trackHubCounts = True
        args.trackHubUsage = True

    # Make list of users to process and build out dictionaries for later
    usersToProcess = [False, False, False]
    # Pos 0 - process db users
    # Pos 1 - process track hub users
    # Pos 2 - process public track hub users
    # The reason I've done it this way is to allow people to output just the counts
    # dbs/tracks/hub tracks without outputting the users files. I could have just set
    # args.*Users = True if args.*Counts was true, but that would mean that the users file
    # would also be output, defeating the purpose of the option to only output the *Counts
    if any([args.dbCounts, args.dbUsers, args.dbUsage]):
        # If dbCounts or dbUsers speficied, we need to process users
        usersToProcess[0] = True
    if any([args.trackCounts, args.trackUsers, args.trackUsage]):
        usersToProcess[1] = True
    if any([args.trackHubCounts, args.trackHubUsers, args.trackHubUsage]):
        usersToProcess[1] = True
        usersToProcess[2] = True
    
    # Process input to create dictionaries containing info about users per db/track/hub track
    if args.fileName:
        processFile(args.fileName, usersToProcess, args.perMonth)
    elif args.dirName:
        processDir(args.dirName, usersToProcess, args.perMonth)

    ##### Output files of summaries of db/track/hub track usage information

    # Output files of db users and db use
    if args.dbCounts == True:
        dbCountsFile = open(os.path.join(outDir, "dbCounts.tsv"), "w")
    if args.dbUsers == True:
        dbUsersFile = open(os.path.join(outDir, "dbUsers.tsv"), "w")
    if args.dbUsage == True:
        dbUsageFile = open(os.path.join(outDir, "dbUsage.tsv"), "w")
    # Set of nested for loops to go through dictionary level by level and
    # output and summarize results into appropriate counts dictionary
    if any([args.dbUsage, args.dbUsers, args.dbCounts]):
        for db in dbUsers:
            dbCounts[db] = 0
            dbUsage = 0
            for hgsid in dbUsers[db]:
                # Ignore those dbs only used once
                if dbUsers[db][hgsid] != 1:
                    dbCounts[db] += 1
                    # Output dbUsers info to a file
                    count = dbUsers[db][hgsid]
                    dbUsage += count
                    if args.dbUsers == True:
                        dbUsersFile.write(db + "\t" + hgsid + "\t" + str(count) + "\n")
            # Output counts of total users for each db
            if dbCounts[db] != 0:
                if args.dbCounts == True:
                    dbCountsFile.write(db + "\t" + str(dbCounts[db]) + "\n")
                if args.dbUsage == True:
                    dbUsageFile.write(db + "\t" + str(dbUsage) + "\n")
    # Close our output files
    if args.dbCounts == True:
        print("Finished outputting dbCounts.tsv")
        dbCountsFile.close()
    if args.dbUsers == True:
        print("Finished outputting dbUsers.tsv")
        dbUsersFile.close()
    if args.dbUsage == True:
        print("Finished outputting dbUsage.tsv")
        dbUsageFile.close()

    # Output files of track users and track use
    if args.trackCounts == True:
        trackCountsFile = open(os.path.join(outDir, "trackCounts.tsv"), "w")
    if args.trackUsers == True:
        trackUsersFile = open(os.path.join(outDir, "trackUsers.tsv"), "w")
    if args.trackUsage == True:
        trackUsageFile = open(os.path.join(outDir, "trackUsage.tsv"), "w")
    # Set of nested for loops to go through dictionary level by level and
    # output and summarize results into appropriate counts dictionary
    if any([args.trackUsage, args.trackUsers, args.trackCounts]):
        for db in trackUsers:
            for track in trackUsers[db]:
                # Initialize count for track to 0
                trackCounts[db][track] = 0
                trackUsage = 0
                for hgsid in trackUsers[db][track]:
                    # Filter out those hgsids who only used track once - likely to be bots
                    if trackUsers[db][track][hgsid] != 1:
                        trackCounts[db][track] += 1
                        # Output information on how much each user used each track
                        count = trackUsers[db][track][hgsid]
                        trackUsage += count
                        if args.trackUsers == True:
                            trackUsersFile.write(db + "\t" + track + "\t" + hgsid + "\t" + str(count) + "\n")
                if trackCounts[db][track] != 0:
                    if args.trackCounts == True:
                        # Output counts of total users for each track
                        count = trackCounts[db][track]
                        trackCountsFile.write(db + "\t" + track + "\t" + str(count) + "\n")
                    if args.trackUsage == True:
                        trackUsageFile.write(db + "\t" + track + "\t" + str(trackUsage) + "\n")
    # Close our output files
    if args.trackCounts == True:
        print("Finished outputting trackCounts.tsv")
        trackCountsFile.close()
    if args.trackUsers == True:
        print("Finished outputting trackUsers.tsv")
        trackUsersFile.close()
    if args.trackUsage == True:
        print("Finished outputting trackUsage.tsv")
        trackUsageFile.close()

    # Output files of hub track users and hub track use
    if args.trackHubUsers == True:
        trackUsersHubsFile = open(os.path.join(outDir, "trackUsersHubs.tsv"), "w")
    if args.trackHubCounts == True:
        trackCountsHubsFile = open(os.path.join(outDir, "trackCountsHubs.tsv"), "w")
    if args.trackHubUsage == True:
        trackUsageHubsFile = open(os.path.join(outDir, "trackUsageHubs.tsv"), "w")
    # Set of nested for loops to go through dictionary level by level and
    # output and summarize results into appropriate counts dictionary
    if any([args.trackHubUsage, args.trackHubUsers, args.trackHubCounts]):
        for hubId in trackUsersHubs:
            hubLabel = publicHubs[hubId][1]
            for db in trackUsersHubs[hubId]:
                for track in trackUsersHubs[hubId][db]:
                    # Initialize count for hub track to 0
                    trackCountsHubs[hubId][db][track] = 0
                    trackHubUsage = 0
                    for hgsid in trackUsersHubs[hubId][db][track]:
                        # Filter out those hgsids who only used track once - likely to be bots
                        if trackUsersHubs[hubId][db][track][hgsid] != 1:
                            trackCountsHubs[hubId][db][track] += 1
                            # Output information on how much each user used each track
                            count = trackUsersHubs[hubId][db][track][hgsid]
                            trackHubUsage += count
                            if args.trackHubUsers == True:
                                trackUsersHubsFile.write(hubLabel + "\t" + db + "\t" + track + "\t" + hgsid +\
                                "\t" + str(count) + "\n")
                    if trackCountsHubs[hubId][db][track] != 0:
                        if args.trackHubCounts == True:
                            # Output counts of total users for each hub track
                            count = trackCountsHubs[hubId][db][track]
                            trackCountsHubsFile.write(hubLabel + "\t" + db + "\t" + track + "\t" +\
                                str(count) + "\n")
                        if args.trackHubUsage == True:
                            trackUsageHubsFile.write(hubLabel + "\t" + db + "\t" + track + "\t" +\
                                str(trackHubUsage) + "\n")
    # Close our output files
    if args.trackHubUsers == True:
        print("Finished outputting trackHubUsers.tsv")
        trackUsersHubsFile.close()
    if args.trackHubCounts == True:
        print("Finished outputting trackHubCounts.tsv")
        trackCountsHubsFile.close()
    if args.trackHubUsage == True:
        print("Finished outputting trackHubUsage.tsv")
        trackUsageHubsFile.close()

    # Output file containing info on month/years covered by stats if indicated
    if args.monthYear == True:
        monthYearFile = open(os.path.join(outDir, "monthYear.tsv"), "w")
        for pair in monthYearSet:
            monthYearFile.write(pair + "\n")
        print("Finished outputting monthYear.tsv")
        monthYearFile.close()

    ##### Output data per month when indicated
    if args.perMonth == True:
        if args.dbUsers == True:
            dbUsersMonthFile = open(os.path.join(outDir, "dbUsers.perMonth.tsv"), "w")
        if args.dbCounts == True:
            dbCountsMonthFile = open(os.path.join(outDir, "dbCounts.perMonth.tsv"), "w")
        if args.dbUsage == True:
            dbUsageMonthFile = open(os.path.join(outDir, "dbUsage.perMonth.tsv"), "w")
        # Set of nested for loops to go through dictionary level by level and
        # output and summarize results into appropriate counts dictionary
        if any([args.dbUsage, args.dbUsers, args.dbCounts]):
            for db in dbUsersMonth:
                for year in dbUsersMonth[db]:
                    for month in dbUsersMonth[db][year]:
                        dbCountsMonth[db][year][month] = 0
                        dbUsageMonth = 0
                        for hgsid in dbUsersMonth[db][year][month]:
                             if dbUsersMonth[db][year][month][hgsid] != 1:
                                 dbCountsMonth[db][year][month] += 1
                                 # Output dbUsersMonth info to a file
                                 count = dbUsersMonth[db][year][month][hgsid]
                                 dbUsageMonth += count
                                 if args.dbUsers == True:
                                     dbUsersMonthFile.write(db + "\t" + year + "\t" + month +\
                                         "\t" + hgsid + "\t" + str(count) + "\n")
                        # Output dbCounts to a file
                        if dbCountsMonth[db][year][month] != 0:
                            if args.dbCounts == True:
                                count = dbCountsMonth[db][year][month]
                                dbCountsMonthFile.write(db + "\t" + year + "\t" + month +\
                                    "\t" + str(count) + "\n")
                            if args.dbUsage == True:
                                dbUsageMonthFile.write(db + "\t" + year + "\t" + month +\
                                    "\t" + str(dbUsageMonth) + "\n")
        if args.dbUsers == True:
            print("Finished outputting dbUsers.perMonth.tsv")
            dbUsersMonthFile.close()
        if args.dbCounts == True:
            print("Finished outputting dbCounts.perMonth.tsv")
            dbCountsMonthFile.close()
        if args.dbUsage == True:
            print("Finished outputting dbUsage.perMonth.tsv")
            dbUsageMonthFile.close()

        # Summarize user dictionaries to create counts per db/track/hub track
        if args.trackUsers == True:
            trackUsersMonthFile = open(os.path.join(outDir, "trackUsers.perMonth.tsv"), "w")
        if args.trackCounts == True:
            trackCountsMonthFile = open(os.path.join(outDir, "trackCounts.perMonth.tsv"), "w")
        if args.trackUsage == True:
            trackUsageMonthFile = open(os.path.join(outDir, "trackUsage.perMonth.tsv"), "w")
        # Set of nested for loops to go through dictionary level by level and
        # output and summarize results into appropriate counts dictionary
        if any([args.trackUsage, args.trackUsers, args.trackCounts]):
            for db in trackUsersMonth:
                for year in trackUsersMonth[db]:
                    for month in trackUsersMonth[db][year]:
                        for track in trackUsersMonth[db][year][month]:
                            trackCountsMonth[db][year][month][track] = 0
                            trackUsageMonth = 0
                            for hgsid in trackUsersMonth[db][year][month][track]:
                                 if trackUsersMonth[db][year][month][track][hgsid] != 1:
                                     trackCountsMonth[db][year][month][track] += 1
                                     count = trackUsersMonth[db][year][month][track][hgsid]
                                     trackUsageMonth += count
                                     if args.trackUsers == True:
                                         trackUsersMonthFile.write(db + "\t" + year + "\t" + month + "\t" +\
                                             hgsid + "\t" + track + "\t" + str(count) + "\n")
                            if trackCountsMonth[db][year][month][track] != 0:
                                if args.trackCounts == True:
                                    count = trackCountsMonth[db][year][month][track]
                                    trackCountsMonthFile.write(db + "\t" + year + "\t" + month + "\t" +\
                                        track + "\t" + str(count) + "\n")
                                if args.trackUsage == True:
                                    trackUsageMonthFile.write(db + "\t" + year + "\t" + month + "\t" +\
                                        track + "\t" + str(trackUsageMonth) + "\n")
        if args.trackUsers == True:
            print("Finished outputting trackUsers.perMonth.tsv")
            trackUsersMonthFile.close()
        if args.trackCounts == True:
            print("Finished outputting trackUsers.perMonth.tsv")
            trackCountsMonthFile.close()
        if args.trackUsage == True:
            print("Finished outputting trackUsage.perMonth.tsv")
            trackUsageMonthFile.close()

        # Summarize user dictionaries to create counts per db/track/hub track
        if args.trackHubUsers == True:
            trackUsersHubsMonthFile = open(os.path.join(outDir, "trackUsersHubs.perMonth.tsv"), "w")
        if args.trackHubCounts == True:
            trackCountsHubsMonthFile = open(os.path.join(outDir, "trackCountsHubs.perMonth.tsv"), "w")
        if args.trackHubUsage == True:
            trackUsageHubsMonthFile = open(os.path.join(outDir, "trackUsageHubs.perMonth.tsv"), "w")
        # Set of nested for loops to go through dictionary level by level and
        # output and summarize results into appropriate counts dictionary
        if any([args.trackHubUsage, args.trackHubUsers, args.trackHubCounts]):
            for hubId in trackUsersHubsMonth:
                hubLabel = publicHubs[hubId][1]
                for db in trackUsersHubsMonth[hubId]:
                    for year in trackUsersHubsMonth[hubId][db]:
                        for month in trackUsersHubsMonth[hubId][db][year]:
                            for track in trackUsersHubsMonth[hubId][db][year][month]:
                                trackCountsHubsMonth[hubId][db][year][month][track] = 0
                                trackHubUsageMonth = 0
                                for hgsid in trackUsersHubsMonth[hubId][db][year][month][track]:
                                     if trackUsersHubsMonth[hubId][db][year][month][track][hgsid] != 1:
                                         trackCountsHubsMonth[hubId][db][year][month][track] += 1
                                         count = trackUsersHubsMonth[hubId][db][year][month][track][hgsid]
                                         trackHubUsageMonth += count
                                         if args.trackHubUsers == True:
                                            trackUsersHubsMonthFile.write(hubLabel + "\t" + db + "\t" + year +\
                                                "\t" + month + "\t" + track + "\t" + hgsid + "\t" + str(count) + "\n")
                                if trackCountsHubsMonth[hubId][db][year][month][track] != 0:
                                    if args.trackHubCounts == True:
                                        count = trackCountsHubsMonth[hubId][db][year][month][track]
                                        trackCountsHubsMonthFile.write(hubLabel + "\t" + db + "\t" + year +\
                                            "\t" + month + "\t" + track + "\t" +\
                                            str(count) + "\n")
                                    if args.trackHubUsage == True:
                                        trackUsageHubsMonthFile.write(hubLabel + "\t" + db + "\t" + year +\
                                            "\t" + month + "\t" + track + "\t" +\
                                            str(trackHubUsageMonth) + "\n")
        if args.trackHubUsers == True:
            print("Finished outputting trackUsersHubs.perMonth.tsv")
            trackUsersHubsMonthFile.close()
        if args.trackHubCounts == True:
            print("Finished outputting trackCountsHubs.perMonth.tsv")
            trackCountsHubsMonthFile.close()
        if args.trackHubUsage == True:
            print("Finished outputting trackUsageHubs.perMonth.tsv")
            trackUsageHubsMonthFile.close()

    #####

    ##### Output json files if indicated #####

    if args.jsonOut == True:
        dumpToJson(dbCounts, "dbCounts.json", outDir)
        dumpToJson(trackCounts, "trackCounts.json", outDir)
        dumpToJson(trackCountsHubs, "trackCountsHubs.json", outDir)

        if args.perMonth == True:
            dumpToJson(dbCountsMonth, "dbCounts.perMonth.json", outDir)
            dumpToJson(trackCountsMonth, "trackCounts.perMonth.json", outDir)
            dumpToJson(trackCountsHubsMonth, "trackCountsHubs.perMonth.json", outDir)

        #if args.monthYear == True:
        #    dumpToJson(monthYearSet, "monthYearSet.json")

    #####

    ##### Output information on default track usage if indicated #####
    if args.outputDefaults == True and all([args.dbCounts, args.trackCounts]):

        # Sort dbs by most popular
        dbCountsSorted = sorted(dbCounts.items(), key=operator.itemgetter(1))
        dbCountsSorted.reverse()

        defaultCountsFile = open(os.path.join(outDir, "defaultCounts.tsv"), "w")
        for x in range(0, 15): # Will only output the default track stats for the 15 most popular assemblies
            db = dbCountsSorted[x][0]
            dbOpt = "db=" + db
            # HGDB_CONF must be set here so that we use default tracks from beta, not dev
            # Dev can contain staged tracks that don't exist on RR, leading to errors later in script
            cmd = ["cd /usr/local/apache/cgi-bin && HGDB_CONF=$HOME/.hg.conf.beta ./hgTracks " + dbOpt]
            p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            cmdout, cmderr = p.communicate()
            errText = cmderr.decode("ASCII") # Convert binary output into ACSII for processing
            # Process stderr output as that's what contains the trackLog lines
            splitErrText = errText.split("\n")
            trackLog = splitErrText[0] # First element is trackLog line, second is CGI_TIME; only want trackLog
            splitLine = trackLog.split(" ")

            # Build list of tracks
            tracks = splitLine[4]
            tracks = tracks.split(",")

            dbUse = dbCounts[db]
            # output list to file that contains column headings
            defaultCountsFile.write("#db\ttrackName\ttrackUse\t% using\t% turning off\n#" + db + "\t" + str(dbUse) + "\n")
            defaultCounts = []
            for track in tracks:
                if track == "":
                    continue

                # Remove trailing characters
                track = track[:-2]
               
                try: 
                    trackUse = trackCounts[db][track]
                    relUse = (trackUse/dbUse)*100
                    relOff = ((dbUse - trackUse)/dbUse)*100
                    # Store all this info a in a list so that we can sort my most used tracks later
                    defaultCounts.append([track, trackUse, relUse, relOff])
                except KeyError:
                    continue

            # Sort defaultCounts for current assembly by most used track first
            defaultCountsSorted = sorted(defaultCounts, key=operator.itemgetter(2))
            defaultCountsSorted.reverse()

            # Output sorted defaultCounts to a file
            for line in defaultCountsSorted:
                track = line[0]
                use = line[1]
                on = line[2]
                off = line[3]

                output = "{}\t{}\t{:d}\t{:3.2f}\t{:3.2f}\n".format(db, track, use, on, off)
                defaultCountsFile.write(output)

        print("Finished outputting defaultCounts.tsv")
        defaultCountsFile.close()
    elif args.outputDefaults == True:
        if not any([args.dbCounts, args.trackCounts]):
            # Need both dbCounts and trackCounts to actually be populated with data to be able to do the default counts
            print("\nCannot output default tracks if either --trackCounts or --dbCounts is not set. Set both of these options and re-run")
    ####

    # Print output directory name, particularly useful if using default output directory names
    # based on day/time
    print("\nYour output is in", outDir)
if __name__ == "__main__":
    main()

