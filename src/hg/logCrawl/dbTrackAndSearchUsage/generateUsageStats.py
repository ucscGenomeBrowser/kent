#!/usr/bin/env python3

import subprocess, os, gzip, argparse, sys, json, operator
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
cmd = ["hgsql", "hgcentral", "-h", "genome-centdb", "-Ne", "select s.id,p.hubUrl,p.shortLabel\
       from hubPublic p, hubStatus s where s.hubUrl=p.hubUrl", ]
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
    # [Sun Mar 05 04:11:27 2017] [error] [client ###.###.###.##] trackLog 0 hg38 hgsid_### cytoBandIdeo:1,cloneEndCTD:2
    ####
    
    splitLine = line.strip().split('trackLog')
    prefix = splitLine[0].split()
    month = prefix[1]
    year = prefix[4].replace("]","")
    suffix = splitLine[1].split()
    db = suffix[1]
    hgsid = suffix[2]
    if len(suffix) > 3:
        activeTracks = suffix[3]
        tracks = activeTracks.split(",")
    else:
        tracks = []
    
    return db, year, month, hgsid, tracks

def modDicts(db, year, month, hgsid, tracks, perMonth=False):
    """Modify global dictionaries to store information on 
       db, track, and hub track usage"""
 
    ##### Process info about db usage
    # Count up number of times each hgsid shows up
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

        ##### Process hub tracks
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

def processFile(fileName, gzipped=False, perMonth=False):
    """Process a file line by line using the function parseTrackLog and record usage information using
       the function modDicts"""
    if gzipped == True:
        for line in gzip.open(fileName, "r"):
            # convert lines from binary into ASCII for processing, only needed for gzip
            line = line.decode("ASCII")
            # Process "trackLog" lines
            if "trackLog" in line:
                db, year, month, hgsid, tracks = parseTrackLog(line)
                modDicts(db, year, month, hgsid, tracks, perMonth)
                # Keep track of month/years covered
                monthYear = month + " " + year
                monthYearSet.add(monthYear)
        
    else:
        for line in open(fileName, "r"):
            # Process "trackLog" lines
            if "trackLog" in line:
                db, year, month, hgsid, tracks = parseTrackLog(line)
                # record information from trackLog line
                modDicts(db, year, month, hgsid, tracks, perMonth)
                # Keep track of month/years covered
                monthYear = month + " " + year
                monthYearSet.add(monthYear)

def processDir(dirName, gzipped=False, perMonth=False):
    """Process files in a directory using processFile function"""
    fileNames = os.listdir(dirName)
    for log in fileNames:
        processFile(dirName + log, gzipped, perMonth)

def dumpToJson(data, outputFile):
    """output data to named outputFile"""
    jsonOut = open(outputFile, "w")
    json.dump(data, jsonOut)
    jsonOut.close()

def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
	description="Generates usage statistics for dbs, tracks, and hubs \
tracks using Apache error_log files")
    parser.add_argument("-f","--fileName", type=str, help='input file name, \
must be space-separated Apache error_log file')
    parser.add_argument("-d","--dirName", type=str , help='input directory \
name, files must be space-separated error_log files. No other files should be \
present in this directory.')
    parser.add_argument("-g","--gzipped", action='store_true', help='indicates \
that input files are gzipped (.gz)')
    parser.add_argument("-p","--perMonth", action='store_true', help='output \
file containing info on db/track/hub track use per month')
    parser.add_argument("-m","--monthYear", action='store_true', help='output \
file containing month/year pairs (e.g. "Mar 2017")')
    parser.add_argument("-j","--jsonOut", action='store_true', help='output \
json files for summary dictionaries')
    parser.add_argument("-t","--outputDefaults", action='store_true',
help='output file containing info on default track usage for top 15 most used assemblies')
    args = parser.parse_args()

    # File and directory options can't be used together. Catch this and exit.
    if args.fileName != None and args.dirName != None:
        print("-f/--fileName and -d/--dirName cannot be used together. Choose one and re-run.")
        sys.exit(1)

    # Process input to create dictionaries containing info about users per db/track/hub track
    if args.fileName:
        processFile(args.fileName, args.gzipped, args.perMonth)
    elif args.dirName:
        processDir(args.dirName, args.gzipped, args.perMonth)

    ##### Output files of summaries of db/track/hub track usage information

    # Output files of db users and db use
    dbCountsFile = open("dbCounts.tsv", "w")
    dbUsersFile = open("dbUsers.tsv", "w")
    # Set of nested for loops to go through dictionary level by level and
    # output and summarize results into appropriate counts dictionary
    for db in dbUsers:
        dbCounts[db] = 0
        for hgsid in dbUsers[db]:
            # Ignore those dbs only used once
            if dbUsers[db][hgsid] != 1:
                dbCounts[db] += 1
                # Output dbUsers info to a file
                count = dbUsers[db][hgsid]
                dbUsersFile.write(db + "\t" + hgsid + "\t" + str(count) + "\n")
        # Output counts of total users for each db
        if dbCounts[db] != 0:
            dbCountsFile.write(db + "\t" + str(dbCounts[db]) + "\n")
    # Close our output files
    dbCountsFile.close()
    dbUsersFile.close()

    # Output files of track users and track use
    trackCountsFile = open("trackCounts.tsv", "w")
    trackUsersFile = open("trackUsers.tsv", "w")
    # Set of nested for loops to go through dictionary level by level and
    # output and summarize results into appropriate counts dictionary
    for db in trackUsers:
        for track in trackUsers[db]:
            # Initialize count for track to 0
            trackCounts[db][track] = 0
            for hgsid in trackUsers[db][track]:
                # Filter out those hgsids who only used track once - likely to be bots
                if trackUsers[db][track][hgsid] != 1:
                    trackCounts[db][track] += 1
                    # Output information on how much each user used each track
                    count = trackUsers[db][track][hgsid]
                    trackUsersFile.write(db + "\t" + track + "\t" + hgsid + "\t" + str(count) + "\n")
            if trackCounts[db][track] != 0:
                # Output counts of total users for each track
                count = trackCounts[db][track] 
                trackCountsFile.write(db + "\t" + track + "\t" + str(count) + "\n")
    # Close our output files
    trackCountsFile.close()
    trackUsersFile.close()

    # Output files of hub track users and hub track use
    trackUsersHubsFile = open("trackUsersHubs.tsv", "w")
    trackCountsHubsFile = open("trackCountsHubs.tsv", "w")
    # Set of nested for loops to go through dictionary level by level and
    # output and summarize results into appropriate counts dictionary
    for hubId in trackUsersHubs:
        hubLabel = publicHubs[hubId][1]
        for db in trackUsersHubs[hubId]:
            for track in trackUsersHubs[hubId][db]:
                # Initialize count for hub track to 0
                trackCountsHubs[hubId][db][track] = 0
                for hgsid in trackUsersHubs[hubId][db][track]:
                    # Filter out those hgsids who only used track once - likely to be bots
                    if trackUsersHubs[hubId][db][track][hgsid] != 1:
                        trackCountsHubs[hubId][db][track] += 1
                        # Output information on how much each user used each track
                        count = trackUsersHubs[hubId][db][track][hgsid]
                        trackUsersHubsFile.write(hubLabel + "\t" + db + "\t" + track + "\t" + hgsid +\
                        "\t" + str(count) + "\n")
                if trackCountsHubs[hubId][db][track] != 0:
                    # Output counts of total users for each hub track
                    count = trackCountsHubs[hubId][db][track] 
                    trackCountsHubsFile.write(hubLabel + "\t" + db + "\t" + track + "\t" +\
                        str(count) + "\n")
    # Close our output files
    trackUsersHubsFile.close()
    trackCountsHubsFile.close()

    # Output file containing info on month/years covered by stats if indicated
    if args.monthYear == True:
        monthYearFile = open("monthYear.tsv", "w")
        for pair in monthYearSet:
            monthYearFile.write(pair + "\n")
        monthYearFile.close()

    ##### Output data per month when indicated
    if args.perMonth == True:
        dbUsersMonthFile = open("dbUsers.perMonth.tsv", "w")
        dbCountsMonthFile = open("dbCounts.perMonth.tsv", "w")
        # Set of nested for loops to go through dictionary level by level and
        # output and summarize results into appropriate counts dictionary
        for db in dbUsersMonth:
            for year in dbUsersMonth[db]:
                for month in dbUsersMonth[db][year]:
                    dbCountsMonth[db][year][month] = 0
                    for hgsid in dbUsersMonth[db][year][month]:
                         if dbUsersMonth[db][year][month][hgsid] != 1:
                             dbCountsMonth[db][year][month] += 1
                             # Output dbUsersMonth info to a file
                             count = dbUsersMonth[db][year][month][hgsid]
                             dbUsersMonthFile.write(db + "\t" + year + "\t" + month +\
                                 "\t" + hgsid + "\t" + str(count) + "\n")
                    # Output dbCounts to a file
                    if dbCountsMonth[db][year][month] != 0:
                        count = dbCountsMonth[db][year][month]
                        dbCountsMonthFile.write(db + "\t" + year + "\t" + month +\
                            "\t" + str(count) + "\n")
        dbUsersMonthFile.close()
        dbCountsMonthFile.close()

        # Summarize user dictionaries to create counts per db/track/hub track
        trackUsersMonthFile = open("trackUsers.perMonth.tsv", "w")
        trackCountsMonthFile = open("trackCounts.perMonth.tsv", "w")
        # Set of nested for loops to go through dictionary level by level and
        # output and summarize results into appropriate counts dictionary
        for db in trackUsersMonth:
            for year in trackUsersMonth[db]:
                for month in trackUsersMonth[db][year]:
                    for track in trackUsersMonth[db][year][month]:
                        trackCountsMonth[db][year][month][track] = 0
                        for hgsid in trackUsersMonth[db][year][month][track]:
                             if trackUsersMonth[db][year][month][track][hgsid] != 1:
                                 trackCountsMonth[db][year][month][track] += 1
                                 count = trackUsersMonth[db][year][month][track][hgsid]
                                 trackUsersMonthFile.write(db + "\t" + year + "\t" + month + "\t" +\
                                     hgsid + "\t" + track + "\t" + str(count) + "\n")
                        if trackCountsMonth[db][year][month][track] != 0:
                            count = trackCountsMonth[db][year][month][track]
                            trackCountsMonthFile.write(db + "\t" + year + "\t" + month + "\t" +\
                                track + "\t" + str(count) + "\n")
        trackUsersMonthFile.close()
        trackCountsMonthFile.close()

        # Summarize user dictionaries to create counts per db/track/hub track
        trackUsersHubsMonthFile = open("trackUsersHubs.perMonth.tsv", "w")
        trackCountsHubsMonthFile = open("trackCountsHubs.perMonth.tsv", "w")
        # Set of nested for loops to go through dictionary level by level and
        # output and summarize results into appropriate counts dictionary
        for hubId in trackUsersHubsMonth:
            hubLabel = publicHubs[hubId][1]
            for db in trackUsersHubsMonth[hubId]:
                for year in trackUsersHubsMonth[hubId][db]:
                    for month in trackUsersHubsMonth[hubId][db][year]:
                        for track in trackUsersHubsMonth[hubId][db][year][month]:
                            trackCountsHubsMonth[hubId][db][year][month][track] = 0
                            for hgsid in trackUsersHubsMonth[hubId][db][year][month][track]:
                                 if trackUsersHubsMonth[hubId][db][year][month][track][hgsid] != 1:
                                     trackCountsHubsMonth[hubId][db][year][month][track] += 1
                                     count = trackUsersHubsMonth[hubId][db][year][month][track][hgsid]
                                     trackUsersHubsMonthFile.write(hubLabel + "\t" + db + "\t" + year +\
                                         "\t" + month + "\t" + track + "\t" + hgsid + "\t" + str(count) + "\n")
                            if trackCountsHubsMonth[hubId][db][year][month][track] != 0:
                                count = trackCountsHubsMonth[hubId][db][year][month][track] 
                                trackCountsHubsMonthFile.write(hubLabel + "\t" + db + "\t" + year +\
                                    "\t" + month + "\t" + track + "\t" +\
                                    str(count) + "\n")
        trackUsersHubsMonthFile.close()
        trackCountsHubsMonthFile.close()

    #####

    ##### Output json files if indicated #####

    if args.jsonOut == True:
        dumpToJson(dbCounts, "dbCounts.json")
        dumpToJson(trackCounts, "trackCounts.json")
        dumpToJson(trackCountsHubs, "trackCountsHubs.json")

        if args.perMonth == True:
            dumpToJson(dbCountsMonth, "dbCounts.perMonth.json")
            dumpToJson(trackCountsMonth, "trackCounts.perMonth.json")
            dumpToJson(trackCountsHubsMonth, "trackCountsHubs.perMonth.json")

        #if args.monthYear == True:
        #    dumpToJson(monthYearSet, "monthYearSet.json")

    #####

    ##### Output information on default track usage if indicated #####
    if args.outputDefaults == True:

        # Sort dbs by most popular
        dbCountsSorted = sorted(dbCounts.items(), key=operator.itemgetter(1))
        dbCountsSorted.reverse()

        defaultCountsFile = open("defaultCounts.tsv", "w")
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

        defaultCountsFile.close()

if __name__ == "__main__":
    main()

