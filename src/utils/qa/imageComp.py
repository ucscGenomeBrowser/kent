#!/usr/bin/python
#/cluster/software/bin/python3

import subprocess, os, sys, requests
from PIL import Image, ImageChops
from datetime import date
import getpass
import subprocess

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

# Notes
# Currently this only supports a single session comparison
# But the logic is there to support any number if the
# Output and comparison functions are updated

### Functions

# def createImageFromSessions(sessionData,saveDir,serverUrl):
#     '''Function used to query all sessions on hgcentral'''
#     processedSessionData = sessionData[1].split("\n")[:-1]
#     imageFiles = []
#     for session in processedSessionData[2:]:
#         userName = session.split('\t')[0]
#         sessionName = session.split('\t')[1]
#         run(["wget","--output-file=/dev/null", "--output-document="+saveDir+"/"+userName+"."+sessionName+"-"+str(date.today())+".png", serverUrl+"/cgi-bin/hgRenderTracks?hgS_doOtherUser=submit&hgS_otherUserName="+userName+"&hgS_otherUserSessionName="+sessionName])
#         imageFiles.append(userName+"."+sessionName+"-"+str(date.today())+".png")
#     return(imageFiles)

def createImageFromSession(userName, sessionName, saveDir, serverUrl):
    '''Function for extracting only a single session'''
    output_file = "{}/{}.{}-{}.png".format(saveDir, userName, sessionName, date.today())
    url = "{}cgi-bin/hgRenderTracks?hgS_doOtherUser=submit&hgS_otherUserName={}&hgS_otherUserSessionName={}".format(
        serverUrl, userName, sessionName
    )

#     print(url, output_file)  #for debug
#     urllib.urlretrieve(url, output_file)
    with open(output_file,'wb') as f:
        f.write(requests.get(url).content)
    
    return "{}.{}-{}.png".format(userName, sessionName, date.today())

def populateServerDir1checkExist(userName,sessionName,serverDir1,serverUrl1):
    '''Creates all the image files for server 1 sessions if they do not exist in directory'''
    if os.path.exists(serverDir1) and os.path.isdir(serverDir1):
        if not os.listdir(serverDir1):
            print("No server one images found, populating directory")
            createImageFromSession(userName,sessionName,serverDir1,serverUrl1)
        else:
            print("Archive images found")
    else:
        sys.exit("Sever one directory given does not exist")

def populateServerDir1(userName,sessionName,serverDir1,serverUrl1):
    '''Creates all the image files for server 1 regardless of existence or not'''
    if os.path.exists(serverDir1) and os.path.isdir(serverDir1):
        createImageFromSession(userName,sessionName,serverDir1,serverUrl1)
    else:
        sys.exit("Sever one directory given does not exist")

def imageCompare(imageFiles,serverDir2,serverDir1,diffImagesDir,user1,sessName1):
    '''Compare server1 and server2 generated images'''
    firstImageName = serverDir1+"/"+user1+"."+sessName1+"-"+str(date.today())+".png"
    emptyFiles = 0
    imagesCompared = 0
    differentImages = 0
    diffImages = []
    noDiffImages = []
    for image in imageFiles:
        if os.path.getsize(serverDir2+"/"+image) == 0:
            emptyFiles+=1
        elif os.path.getsize(firstImageName) == 0:
            emptyFiles+=1
        else:
            imagesCompared+=1
            previousImage = Image.open(firstImageName).convert('RGB')
            newImage = Image.open(serverDir2+"/"+image).convert('RGB')
            diff = ImageChops.difference(previousImage, newImage)
            if diff.getbbox():
                differentImages+=1
                diff.save(diffImagesDir+image)
                diffImages.append(image)
            else:
                noDiffImages.append(image)
    return(emptyFiles,imagesCompared,differentImages,diffImages,noDiffImages)

def reportOutput(emptyFiles,imagesCompared,differentImages,diffImages,noDiffImages,\
                 diffImagesDir,publicHtmlDirToSave,publicHtmlDirToView,serverUrl1,\
                 sessionUser1,sessionName1,serverUrl2,sessionUser2,sessionName2):
    '''Report findings, if differences found create symlinks to public html'''
    if noDiffImages != []:
        print("No differences seen in the following session(s):")
        for image in noDiffImages:
            print(image)
    if diffImages != []:
        print("\nDifferences were observed in the following session(s):")
        for image in diffImages:
            print(image)
            bash("ln -sf "+diffImagesDir+image+" "+publicHtmlDirToSave+image)
            print("Link: "+publicHtmlDirToView+image)
            print("session 1: %s/cgi-bin/hgTracks?hgS_doOtherUser=submit&hgS_otherUserName=%s&hgS_otherUserSessionName=%s"
                   %(serverUrl1,sessionUser1,sessionName1))
            print("session 2: %s/cgi-bin/hgTracks?hgS_doOtherUser=submit&hgS_otherUserName=%s&hgS_otherUserSessionName=%s"
                   %(serverUrl2,sessionUser2,sessionName2))
    print("\nNumber of empty session files created: %s" % emptyFiles)
    print("Total number of images compared: %s" % imagesCompared)
    print("Different images found: %s" % differentImages)
    
###########
#To be used when extracting all session data
#sessionData = run(["hgsql", "-e", "select userName,sessionName from namedSessionDb", "hgcentraltest"], stdout=subprocess.PIPE)
##########

def main():
    user = getpass.getuser()
    #Define cars
    serverUrl1 = "http://hgwdev.gi.ucsc.edu/"
    serverUrl2 = "http://hgwbeta.soe.ucsc.edu/"
    # If you want to add more sessions, do so in the next 4 lines. Add the user and session name to the list
    sessionUser1 = ["lou","lou","lou","lou"]
    sessionUser2 = ["lou","lou","lou","lou"]
    sessionName1 = ["imageCompCron1.2","bigLollyAndTrioDisplayExample","logoDisplayWithWeirdConfigurations","snakesDisplayExample"]
    sessionName2 = ["imageCompCron1.2","bigLollyAndTrioDisplayExample","logoDisplayWithWeirdConfigurations","snakesDisplayExample"]
    serverDir1 = "/hive/users/"+user+"/imageTest/cronImages/server1Images"
    serverDir2 = "/hive/users/"+user+"/imageTest/cronImages/server2Images"
    diffImagesDir = "/hive/users/"+user+"/imageTest/cronImages/diffImages/"
    publicHtmlDirToSave = "/cluster/home/"+user+"/public_html/images/"
    publicHtmlDirToView = "https://hgwdev.gi.ucsc.edu/~"+user+"/images/"
    for (user1, sessName1, user2, sessName2) in zip(sessionUser1, sessionName1, sessionUser2, sessionName2):
        #Create first set of image file(s)
        populateServerDir1(user1,sessName1,serverDir1,serverUrl1)
        #Create second set of image file(s)
        imageFiles = []
        imageFiles.append(createImageFromSession(user2,sessName2,serverDir2,serverUrl2))
        #Compare images
        emptyFiles,imagesCompared,differentImages,diffImages,noDiffImages = imageCompare(imageFiles,serverDir2,serverDir1,diffImagesDir,user1,sessName1)
        #Report findings
        if diffImages == []: #Check if there is anything to report - that way cron does not output
            pass
        else:
            reportOutput(emptyFiles,imagesCompared,differentImages,diffImages,noDiffImages,\
                     diffImagesDir,publicHtmlDirToSave,publicHtmlDirToView,serverUrl1,\
                     user1,sessName1,serverUrl2,user2,sessName2)

main()
