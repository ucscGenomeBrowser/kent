#!/usr/bin/python3

import os, requests, subprocess, sys, argparse

def parseArgs():
    """
    Parse the command line arguments.
    """
    parser = argparse.ArgumentParser(description = __doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    optional = parser._action_groups.pop()

    required = parser.add_argument_group('required arguments')

    required.add_argument ("saveDir",
        help = "Server from which to query the hubPublic table. Can be dev, hgwbeta, or rr.")
    if (len(sys.argv) == 1):
        parser.print_usage()
        print("\nFetches thumbnails and descriptions of public sessions from the RR and saves/creates\n" + \
              "an html page that can be included in the homepage. Requires the directory in which to save\n" + \
              "the pages to be declared.\n\n" + \
              "Example run:\n" + \
              "    buildPublicSessionThumbnailsIndexPage /cluster/home/lrnassar/kent/src/hg/htdocs/\n")
        exit(0)
    parser._action_groups.append(optional)
    options = parser.parse_args()
    return  options

def queryHgPublicSessAndAssignOutputFileAndDir(saveDir):
    """Curl hgPublicSession and assign output file name/directory"""
    hgPublicSessionOutPut = requests.get('https://genome.ucsc.edu/cgi-bin/hgPublicSessions')
    outputFile = open(saveDir+'thumbNailLinks.html', 'w')
    return(hgPublicSessionOutPut, outputFile)

def parseHgPublicSessPageAndWriteOut(hgPublicSessionOutPut, saveDir, outputFile):
    """Parse hgPublicSession curled page, extract thumbnail/url/description and write out to file"""
    sessionThumbNailsWritten = 1
    for line in hgPublicSessionOutPut.text.split('\n'):
        if "trash" in line and sessionThumbNailsWritten < 4:
            sessionUrl = "https://genome.ucsc.edu/cgi-bin/"+line.split('"')[1].split("/")[2]
            trashDirUrl = line.split('"')[3]
            downloadThumbNailUrl = "https://genome.ucsc.edu/trash/hgPS/"+trashDirUrl.split("/")[3]
            currentImageFileName = 'sessionThumbNail'+str(sessionThumbNailsWritten)+'.png'
            img_data = requests.get(downloadThumbNailUrl).content
            with open(saveDir+currentImageFileName, 'wb') as handler:
                handler.write(img_data)
            os.chmod(saveDir+currentImageFileName, 0o664)
        if "Description:" in line and sessionThumbNailsWritten < 4:
            sessionThumbNailsWritten+=1
            sessionDescription = line.split('</b> ')[1].split('<br>')[0]
            outputFile.write('''<a href="'''+sessionUrl+'''">\n<img src="/'''+currentImageFileName+'''" title="'''
                         +sessionDescription+'''" class="sessionThumbnail" style="vertical-align: top;" width="30%"
                         height=40%"></img></a>\n''')
    outputFile.close()
    os.chmod(saveDir+'thumbNailLinks.html', 0o775)

def main():
    options = parseArgs()
    saveDir = options.saveDir
    hgPublicSessionOutPut, outputFile = queryHgPublicSessAndAssignOutputFileAndDir(saveDir)
    parseHgPublicSessPageAndWriteOut(hgPublicSessionOutPut, saveDir, outputFile)

main()
