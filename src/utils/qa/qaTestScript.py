#!/usr/bin/env python3

# Program Header
# Name:   Gerardo Perez
# Description: A program that runs Selenium in either headless mode or local mode (window popup)
# to run test cases for a machine (hgwdev, hgwbeta, and the RR). 
#
# qaTestScript.py
#
#
# Development Environment: VIM - Vi IMproved version 7.4.629
# Version: Python 3.6.5 
#
#
# To run local mode (window popup), you will need to install chromedriver locally.
# Ex. /Users/gerardoperez/Downloads/chromedriver 
# You can download chromedriver from https://chromedriver.chromium.org/downloads
#
# IGNORE the following error notes: 
# For error 
# (The process started from chrome location 
# /hive/users/lrnassar/selenium/chrome/opt/google/chrome/google-chrome is no longer running,
# so ChromeDriver is assuming that Chrome has crashed.)
# import os
# os.environ['TMPDIR'] = "/hive/users/lrnassar/selenium/chrom113/TMP"
# ^IGNORE

# Imports module
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.common.by import By
from selenium.webdriver.common.action_chains import ActionChains
from selenium import webdriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.support.ui import Select
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.common.exceptions import NoSuchElementException
from selenium.common.exceptions import NoAlertPresentException
import unittest, time, re, sys, argparse
import getpass
import os

user = getpass.getuser()


#Make tmpdir for the cron
try:
   os.makedirs("/hive/users/"+user+"/selenium/chrom113/cron/TMP", exist_ok=True)
except OSError as e:
    print(f"Error creating directory: {e}")
os.environ['TMPDIR'] = "/hive/users/"+user+"/selenium/chrom113/cron/TMP"

def initialize_driver(headless):
    """Initiates which webdriver to run for headless mode or local mode"""
    options = Options()
    if headless:
        options.add_argument('--headless')
        options.add_argument('--no-sandbox')
        options.add_argument("--headless")
        options.add_argument('--disable-dev-shm-usage')
        options.add_argument("--start-maximized")
        options.add_argument("--window-size=1920,1080")
        options.binary_location = '/hive/users/lrnassar/selenium/chrom113/chromeInstall/opt/google/chrome/google-chrome'
        driver = webdriver.Chrome('/hive/users/lrnassar/selenium/chrom113/chromedriver', options = options)
    else:
        # Asks user for the WebDriver location
        webdriver_location = input("Enter the WebDriver location: ")
        driver = webdriver.Chrome(executable_path=webdriver_location)
    return driver

# Creates the command-line argument parser
parser = argparse.ArgumentParser(description='Selenium script with headless/local mode.')
parser.add_argument('--headless', action='store_true', help='Activate headless mode')
parser.add_argument('--machine', type=str, help='Specify the machine URL')

# Parses the command-line arguments
args = parser.parse_args()

# Asks the user for the website URL if not provided as a command-line argument
if not args.machine:
    args.machine = input("Enter the machine URL (Ex. https://hgwbeta.soe.ucsc.edu):")

# Initializes the driver based on the chosen mode
driver = initialize_driver(args.headless)

# Loads the machine website
machine= args.machine

def cartReset():
    """The function does a cart reset"""
    a = ActionChains(driver)
    #identify element
    m = driver.find_element_by_link_text("Genome Browser")
    #hover over element
    a.move_to_element(m).perform()
    #identify sub menu element
    n = driver.find_element_by_link_text("Reset All User Settings")
    # hover over element and click
    a.move_to_element(n).click().perform()
    driver.implicitly_wait(2)

# Tests the Gateway page and home page
driver.get(machine + "/cgi-bin/hgGateway")
driver.get(machine + "/index.html")
driver.find_element_by_link_text("Home").click()
driver.find_element_by_link_text("Genomes").click()
driver.get(machine + "/cgi-bin/hgGateway")
driver.find_element_by_xpath("//div[@id='selectSpeciesSection']/div[2]/div[2]/div[2]/div").click()


# Tests mm39 hgTracks
driver.get(machine + "/cgi-bin/hgTracks?db=mm39")
cartReset()


# Tests hg38 hgGene
driver.get(machine + "/cgi-bin/hgTracks?db=hg38")
driver.find_element_by_xpath("//td[@id='td_data_knownGene']/div[2]/map/area[5]").click()

# Tests hg19 hgGene
driver.get(machine + "/cgi-bin/hgTracks?db=hg19")
driver.find_element_by_xpath("//td[@id='td_data_knownGene']/div[2]/map/area[5]").click()

# Tests mm10 hgGene
driver.get(machine + "/cgi-bin/hgTracks?db=mm10")
driver.find_element_by_xpath("//td[@id='td_data_knownGene']/div[2]/map/area[5]").click()

# Tests multi-region for hg38
cartReset()
driver.get(machine + "/cgi-bin/hgTracks?db=hg38")
driver.find_element_by_id("positionInput").clear()
driver.find_element_by_id("positionInput").send_keys("chr7 192500 727300")
driver.find_element_by_id("goButton").click()
driver.find_element_by_name("hgTracksConfigMultiRegionPage").click()
driver.find_element_by_xpath("(//input[@id='virtModeType'])[4]").click()
driver.find_element_by_id("multiRegionsBedInput").send_keys("chr7    192570  260772  NM_020223.4\nchr7    290169  291488  NM_001374838.1\nchr7    497257  519846  NM_033023.5\nchr7    549197  727281  NM_001164760.2")
driver.find_element_by_name("topSubmit").click()
driver.find_element_by_xpath("//td[@id='td_data_ncbiRefSeqCurated']/div[2]/map/area").click()
driver.find_element_by_xpath("//td[@id='td_data_ncbiRefSeqCurated']/div[2]/map/area[2]").click()


# Tests hgGeneGraph 
driver.get(machine + "/cgi-bin/hgGeneGraph")
driver.find_element_by_name("gene").clear()
driver.find_element_by_name("gene").send_keys("sirt1")
driver.find_element_by_name("1").click()
driver.find_element_by_id("dropdownMenu1").click()
driver.find_element_by_link_text("GNF2 Expression").click()
driver.find_element_by_id("edge7").click()
driver.find_element_by_xpath("(.//*[normalize-space(text()) and normalize-space(.)='Gene interactions and pathways from curated databases and text-mining'])[1]/following::a[2]")
cartReset()

# Tests hgVai
driver.get(machine + "/cgi-bin/hgVai?hgva_agreedToDisclaimer=1")
driver.find_element_by_id("subDisclmAgrd").click()

# Tests hgc/hgGene for hg38
driver.get(machine + "/cgi-bin/cartReset")
driver.get(machine + "/cgi-bin/hgGateway?db=hg38")
driver.get(machine + "/cgi-bin/hgGene?hgg_gene=ENST00000370314.9&hgg_chrom=chrX&hgg_start=152166233&hgg_end=152451315&hgg_type=knownGene&db=hg38")
driver.find_element_by_link_text("Sequence and Links").click()
driver.find_element_by_link_text("Genomic Sequence (chrX:152,166,234-152,451,315)").click()
driver.find_element_by_name("submit").click()

# Tests hub on canFam3
cartReset()
driver.get(machine + "/cgi-bin/hgGateway?db=canFam3")
driver.get(machine + "/cgi-bin/hgTracks?hubUrl=https://data.broadinstitute.org/vgb/dog/dog/hub.txt&genome=canFam3&position=lastDbPos")

# Tests non-human/mouse (oviAri4) on Table Browser
cartReset()
driver.get(machine + "/cgi-bin/hgGateway?db=oviAri4")
a = ActionChains(driver)
# identify element
m = driver.find_element_by_id("tools3") 
# hover over element
a.move_to_element(m).perform()
# identify sub menu element
n = driver.find_element_by_id("tableBrowserMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_id("hgta_doSchema").click()
driver.get(machine + "/cgi-bin/hgTables?db=oviAri4")
driver.find_element_by_name("hgta_doSummaryStats").click()

# Tests a session with custom tracks, multiRegion, and assembly hub
cartReset()
driver.get(machine + "/cgi-bin/hgTracks?hgS_doOtherUser=submit&hgS_otherUserName=brianlee&hgS_otherUserSessionName=Custom_Tracks_AssemblyHub_MultiRegion_TrackCollection_BigWigs")
driver.find_element_by_xpath("//td[@id='td_data_ct_UserTrack_3545']/div[2]/map/area[4]").click()
driver.find_element_by_link_text("chr1:33719895-33742564").click()

# Tests a DNA search on hgTracks
cartReset()
driver.get(machine + "/cgi-bin/hgTracks?db=hg38&hideTracks=1")
driver.get(machine + "/cgi-bin/hgTracks")
driver.find_element_by_id("positionInput").clear()
driver.find_element_by_id("positionInput").send_keys("GTATGTAGCCACGGAGCACCATTACCTGTCACCATTACCTGAATGGCTA")
driver.find_element_by_name("goButton").click()
driver.find_element_by_xpath("//a[contains(text(),'browser')]").click()

# Tests custom tracks on hg19
driver.get(machine + "/cgi-bin/hgGateway?db=hg19")
a = ActionChains(driver)
#identify element
m = driver.find_element_by_id("myData")
##hover over element
a.move_to_element(m).perform()
#identify sub menu element
n = driver.find_element_by_id("customTracksMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_name("hgct_customText").clear()
driver.find_element_by_name("hgct_customText").send_keys("https://hgwdev-gperez2.gi.ucsc.edu/~gperez2/testing/hgCustom_testing/examples.WITHOUT.FTPS.txt")
driver.find_element_by_name("Submit").click()
driver.find_element_by_name("submit").click()
driver.find_element_by_id("p_btn_ct_hicExampleTWO_9382").click()
driver.find_element_by_name("ct_hicExampleTWO_9382.color").click()

# Tests small custom track to click into hgTrackUi
cartReset()
driver.get(machine + "/cgi-bin/hgGateway?db=hg19")
a = ActionChains(driver)
#identify element
m = driver.find_element_by_id("myData")
##hover over element
a.move_to_element(m).perform()
#identify sub menu element
n = driver.find_element_by_id("customTracksMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_name("hgct_customText").clear()
driver.find_element_by_name("hgct_customText").send_keys("https://hgwdev.gi.ucsc.edu/~brianlee/examples/customTracks/newTypes.txt")
driver.find_element_by_name("Submit").click()
driver.find_element_by_name("submit").click()
#if 'barChart Example One' in driver.page_source:
#    print("Add Custom Tracks")

# click into hgTrackUi of customTrack
driver.find_element_by_xpath("//td[@id='td_data_ct_barChartExampleOne_4976']/div[2]/map/area[29]").click()
driver.find_element_by_xpath("//div[@id='firstSection']/table/tbody/tr/td/table/tbody/tr/td/table/tbody/tr[2]/td[2]/p[2]/a").click()
driver.find_element_by_xpath("//a[contains(text(),'Data schema/format description and download')]")
driver.get(machine + "/cgi-bin/hgTracks")
driver.find_element_by_xpath("//td[@id='td_data_ct_interactExample_4634']/div[2]/map/area[5]").click()
driver.find_element_by_xpath("//div[@id='firstSection']/table/tbody/tr/td/table/tbody/tr/td/table/tbody/tr[2]/td[2]/p[2]/a").click()
driver.find_element_by_xpath("//a[contains(text(),'Data schema/format description and download')]")

## Tests chromAlias hg38 custom track
cartReset()
driver.get(machine + "/cgi-bin/hgGateway?db=hg38")
a = ActionChains(driver)
#identify element
m = driver.find_element_by_id("myData")
##hover over element
a.move_to_element(m).perform()
#identify sub menu element
n = driver.find_element_by_id("customTracksMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_name("hgct_customText").clear()
driver.find_element_by_name("hgct_customText").send_keys("https://hgwdev-gperez2.gi.ucsc.edu/~gperez2/testing/selenium/chrmAliasTestHg38_track")
driver.find_element_by_name("Submit").click()
driver.find_element_by_name("submit").click()
# click into hgTrackUi of customTrack

driver.find_element_by_xpath("//td[@id='td_data_ct_chrmAliasTestHg38_4656']/div[2]/map/area").click()
driver.find_element_by_xpath("//div[@id='firstSection']/table/tbody/tr/td/table/tbody/tr/td/table/tbody/tr[2]/td[2]/p/a").click()
driver.find_element_by_xpath("//a[contains(text(),'Data schema/format description and download')]")

# Tests mm10 ENCODE hub
cartReset()
driver.get(machine + "/cgi-bin/hgTracks?db=mm10&hideTracks=1")
driver.get(machine + "/cgi-bin/hgGateway?db=mm10&hubUrl=https://www.encodeproject.org/experiments/ENCSR736GVO/@@hub/hub.txt")
driver.get(machine + "/cgi-bin/hgHubConnect?#unlistedHubs")
driver.find_element_by_link_text("Connected Hubs").click()
driver.get(machine + "/cgi-bin/hgTracks")
driver.find_element_by_name("hgt.positionInput").clear()
driver.find_element_by_name("hgt.positionInput").send_keys("chr12:56,694,976-56,714,605")
driver.find_element_by_name("goButton").click()
driver.find_element_by_name("hgt.out1").click()

# Tests Public Hub search 
driver.get(machine + "/cgi-bin/hgHubConnect?hubSearchTerms=wuhCor1")
driver.get(machine + "/cgi-bin/hgHubConnect?hubSearchTerms=methpipe")
driver.get(machine + "/cgi-bin/hgHubConnect?hubSearchTerms=GCF")

# Tests AssemblyHub search
cartReset()
driver.get(machine + "/cgi-bin/hgGateway?hubUrl=https://genome-test.gi.ucsc.edu/gbdb/hubs/genbank/vertebrate_mammalian/hub.ncbi.txt&genome=GCA_000493695.1_BalAcu1.0&position=lastDbPos")
driver.get(machine + "/cgi-bin/hgTracks")
driver.find_element_by_name("hgt.positionInput").clear()
driver.find_element_by_name("hgt.positionInput").send_keys("ATDI01079686")
driver.find_element_by_name("goButton").click()
time.sleep(5)
#driver.find_element_by_id("hgt.out1").click()
#time.sleep(5)
# Tests track hub annotation for specific machine
if 'hub_26485' in driver.page_source:
     driver.find_element_by_xpath("//td[@id='td_data_hub_26485_assembly']/div[2]/map/area[3]").click()
elif 'hub_11450' in driver.page_source:
     driver.find_element_by_xpath("//td[@id='td_data_hub_11450_assembly']/div[2]/map/area[3]").click()

else:
     driver.find_element_by_xpath("//td[@id='td_data_hub_4081003_assembly']/div[2]/map/area[3]").click()
time.sleep(2)

# Tests HGVS searches
cartReset()
driver.get(machine + "/cgi-bin/hgTracks?db=hg38")
driver.find_element_by_name("hgt.positionInput").clear()
driver.find_element_by_name("hgt.positionInput").send_keys("NM_000310.4(PPT1):c.271_287del17insTT")
driver.find_element_by_id("goButton").click()
time.sleep(2)
driver.find_element_by_xpath("//td[@id='td_data_ncbiRefSeqCurated']/div[2]/map/area[9]").click()
driver.get(machine + "/cgi-bin/hgTracks?db=hg38")
driver.find_element_by_name("hgt.positionInput").clear()
driver.find_element_by_name("hgt.positionInput").send_keys("NM_007262.5(PARK7):c.-24+75_-24+92dup")
driver.find_element_by_id("goButton").click()
time.sleep(2)
driver.find_element_by_name("hgt.positionInput").clear()
driver.find_element_by_name("hgt.positionInput").send_keys("NM_006172.4(NPPA):c.456_*1delAA")
driver.find_element_by_id("goButton").click()
time.sleep(2)
driver.find_element_by_name("hgt.positionInput").clear()
driver.find_element_by_name("hgt.positionInput").send_keys("MYH11:c.503-14_503-12del")
driver.find_element_by_id("goButton").click()
time.sleep(2)
driver.find_element_by_name("hgt.positionInput").clear()
driver.find_element_by_name("hgt.positionInput").send_keys("NM_198576.4(AGRN):c.1057C>T")
driver.find_element_by_id("goButton").click()
time.sleep(2)
driver.find_element_by_name("hgt.positionInput").clear()
driver.find_element_by_name("hgt.positionInput").send_keys("NM_198056.3:c.1654G>T")
driver.find_element_by_id("goButton").click()
time.sleep(2)
driver.find_element_by_name("hgt.positionInput").clear()
driver.find_element_by_name("hgt.positionInput").send_keys("NP_002993.1:p.Asp92Glu")
driver.find_element_by_id("goButton").click()
time.sleep(2)
driver.find_element_by_name("hgt.positionInput").clear()
driver.find_element_by_name("hgt.positionInput").send_keys("NP_002993.1:p.D92E")
driver.find_element_by_id("goButton").click()
time.sleep(2)
driver.find_element_by_name("hgt.positionInput").clear()
driver.find_element_by_name("hgt.positionInput").send_keys("BRCA1 Ala744Cys")
driver.find_element_by_id("goButton").click()
time.sleep(2)
driver.find_element_by_xpath("//td[@id='td_data_ncbiRefSeqCurated']/div[2]/map/area[3]").click()
driver.get(machine + "/cgi-bin/hgTracks?db=hg38") 
driver.find_element_by_name("hgt.positionInput").clear()
time.sleep(3)
driver.find_element_by_name("hgt.positionInput").send_keys("NM_000828.5:c.-2G>A")
driver.find_element_by_id("goButton").click()
time.sleep(3)

# Tests hgBlat
cartReset()
driver.get(machine + "/cgi-bin/hgGateway?db=hg19")
a = ActionChains(driver)
#identify element
m = driver.find_element_by_id("tools3")
#hover over element
a.move_to_element(m).perform()
#identify sub menu element
n = driver.find_element_by_id("blatMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_name("userSeq").clear()
driver.find_element_by_name("userSeq").send_keys("AACAAAATCAAACTGTTTTTGTTGGACAATTCTCTGTTAAGCAGCTATAA\\nGCTGAATGACATTAACCGCAAAATGTAACCATAAAGGCCATAAACCCGAC\\nATTGTTAATTAATTAAATGCCTCATTAACTTTTTTAAAAACATGATTTAT\\nTCGATTCATAGAAAACTTAACCATCACTACTAAATGCACACACATGCGGT\\nTCCACATTGGCATCTTAGCCTAAGAACAGACAGGTTCAACTGTAACTGGC\\nCTTTCAGGTGGTCTATTACAGATCTGAAGACAGAGGGTGTTTCTAAACCT\\nCAAGAACCAGATTAACAGAAAACAAAGCTTGAGCAGCCTTTTTATTGCAT\\nGTGGTATCTTTTTAGCTAAGCAGAAGACAATGATAAAGAGGGGTTTTGGG\\nAAACCTCTCCCAAAGCTGTGCATTCATACCGTACCTTATCCTGTTAAGCA\\nAACTGTTCTTTTATTTTAAAGGGTTTACACTGCCACATCTGAATGGACTA")
driver.find_element_by_name("Submit").click()
time.sleep(3)
driver.find_element_by_xpath("//div[@id='firstSection']/table/tbody/tr/td/table/tbody/tr/td/table/tbody/tr[2]/td[2]/div[2]/pre/a").click()
driver.find_element_by_xpath("//td[@id='td_data_hgUserPsl']/div[2]/map/area").click()
time.sleep(3)
driver.find_element_by_xpath("//div[@id='firstSection']/table/tbody/tr/td/table/tbody/tr/td/table/tbody/tr[2]/td[2]/a").click()
time.sleep(3)
driver.get(machine + "/cgi-bin/hgGateway?db=hg38")


# Tests hgBlat for alt patch sequence
driver.get(machine + "/cgi-bin/hgGateway?db=hg38")
a = ActionChains(driver)
#identify element
m = driver.find_element_by_id("tools3")
#hover over element
a.move_to_element(m).perform()
#identify sub menu element
n = driver.find_element_by_id("blatMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_name("userSeq").clear()
driver.find_element_by_name("userSeq").send_keys("CACACTGTGGATGACATCCAGCAGATCGCTGCTGCGCTGGCCCAGTGCATGGTAGGATGGCCCCACATGCTCTCCCCGCCCCGCATGCCTGCCAGGGTACTGGGTTCAGCCCCCCAGGGCAGACGGGCAGCTTGGCCGAGGAGCTGAGCCTCCAGCCTGGGC")
driver.find_element_by_name("Submit").click()
time.sleep(3)
driver.find_element_by_xpath("//div[@id='firstSection']/table/tbody/tr/td/table/tbody/tr/td/table/tbody/tr[2]/td[2]/div[2]/pre/a").click()
time.sleep(3)
driver.find_element_by_xpath("//td[@id='td_data_altSeqLiftOverPsl']/div[2]/map/area[3]").click()
time.sleep(3)
driver.find_element_by_link_text("Show chr16_KI270853v1_alt placed on its chromosome").click()
time.sleep(3)

# Tests hgBlat for fix patch sequence
cartReset()
driver.get(machine + "/cgi-bin/hgGateway?db=hg38")
a = ActionChains(driver)
#identify element
m = driver.find_element_by_id("tools3")
#hover over element
a.move_to_element(m).perform()
#identify sub menu element
n = driver.find_element_by_id("blatMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_name("userSeq").clear()
driver.find_element_by_name("userSeq").send_keys("GTTTTTTCTCCTATGGCATGCAGGCGACATGTTACTTCCTATTCCCATAAACCCTCCACTGTAGGATTAACACCTAAGACACCAACCAAGACAAAAAAGATATGACCCTTGGT")
driver.find_element_by_name("Submit").click()
time.sleep(3)
driver.find_element_by_xpath("//div[@id='firstSection']/table/tbody/tr/td/table/tbody/tr/td/table/tbody/tr[2]/td[2]/div[2]/pre/a").click()
time.sleep(3)
driver.find_element_by_xpath("//td[@id='td_data_fixSeqLiftOverPsl']/div[2]/map/area[3]").click()
driver.find_element_by_link_text("Show chr1_MU273333v1_fix placed on its chromosome").click()
time.sleep(3)

# Tests hgPcr for hg38
driver.get(machine + "/cgi-bin/hgGateway?db=hg38")
a = ActionChains(driver)
#identify element
m = driver.find_element_by_id("tools3")
#hover over element
a.move_to_element(m).perform()
#identify sub menu element
n = driver.find_element_by_id("ispMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_name("wp_f").clear()
driver.find_element_by_name("wp_f").send_keys("AACAAAATCAAACTGTTTTTGTTGGACAATTCTCTGTTAAGCAGCTATAA")
driver.find_element_by_name("wp_r").clear()
driver.find_element_by_name("wp_r").send_keys("AACTGTTCTTTTATTTTAAAGGGTTTACACTGCCACATCTGAATGGACTA")
driver.find_element_by_name("wp_flipReverse").click()
driver.find_element_by_name("Submit").click()
time.sleep(3)
driver.find_element_by_link_text("chrX:40059679+40060178").click()
time.sleep(3)

# Tests hgConvert
driver.get(machine + "/cgi-bin/hgTracks")
a = ActionChains(driver)
#identify element
m = driver.find_element_by_id("view")
#hover over element
a.move_to_element(m).perform()
#identify sub menu element
n = driver.find_element_by_id("convertMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_name("hglft_doConvert").click()
driver.find_element_by_link_text("chrX:39460925-39461424").click()
driver.find_element_by_css_selector("#tools3 > span").click()

# Tests hgLiftOver for mm39
cartReset()
driver.get(machine + "/cgi-bin/hgTracks?db=mm39")
a = ActionChains(driver)
#identify element
m = driver.find_element_by_id("tools3")
#hover over element
a.move_to_element(m).perform()
#identify sub menu element
n = driver.find_element_by_id("liftOverMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_name("hglft_userData").clear()
driver.find_element_by_name("hglft_userData").send_keys("chr11:101,379,590-101,442,705")
driver.find_element_by_name("Submit").click()
time.sleep(3)
driver.find_element_by_link_text("View Conversions")

# Tests hgPcr target Genes Track (data changes with data pushes)
cartReset()
driver.get(machine + "/cgi-bin/hgTracks?db=hg38&hideTracks=1")
driver.get(machine + "/cgi-bin/hgGateway?db=hg38&wp_target=hg38KgSeqV41") #will be hg38KgSeqV41
a = ActionChains(driver)
#identify element
m = driver.find_element_by_id("tools3")
#hover over element
a.move_to_element(m).perform()
#identify sub menu element
n = driver.find_element_by_id("ispMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_name("wp_f").clear()
driver.find_element_by_name("wp_f").clear()
driver.find_element_by_name("wp_f").send_keys("TTTTCCTAATAATGCTTGTCTTGGTCTTGTT")
driver.find_element_by_name("wp_r").clear()
driver.find_element_by_name("wp_r").send_keys("ACACACACAGAAAGACACACACAGACACAAAA")
driver.find_element_by_name("wp_flipReverse").click()
driver.find_element_by_name("wp_append").click()
driver.find_element_by_name("wp_size").clear()
driver.find_element_by_name("wp_size").send_keys("40000")
select = Select(driver.find_element_by_name("wp_target"))
select.select_by_visible_text("GENCODE Genes")
driver.find_element_by_name("Submit").click()
driver.find_element_by_link_text("ENST00000611156.4__ABO:90+1305").click()
time.sleep(3)
driver.find_element_by_xpath("//td[@id='td_data_hgPcrResult']/div[2]/map/area[2]").click()
cartReset()

# Tests GenArk Rabbit Hub
#cartReset()
driver.get(machine + "/cgi-bin/hgTracks?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/GCF/000/003/625/GCF_000003625.3/hub.txt&genome=GCF_000003625.3")
driver.get(machine + "/cgi-bin/hgTracks?hideTracks=1")
driver.find_element_by_id("positionInput").clear()
driver.find_element_by_id("positionInput").send_keys("HOPX")
driver.find_element_by_id("goButton").click()
time.sleep(3)
driver.find_element_by_link_text("HOPX").click()

# Tests Assembly Hubs at GitHub
cartReset()
driver.get(machine + "/cgi-bin/hgTracks?genome=daph&hubUrl=https://raw.githubusercontent.com/ucsc-browser/assemblyHubEx/master/Daphnia/hubExamples/hubAssembly/daph/hub.txt&position=scaffold_1%3A35591-35626")
driver.get(machine + "/cgi-bin/hgTracks")
time.sleep(3)
driver.find_element_by_id("positionInput").clear()
driver.find_element_by_id("positionInput").send_keys("scaffold_3:1,888,907-1,888,948")
driver.find_element_by_id("goButton").click()
time.sleep(3)
driver.find_element_by_id("hgt.out1").click()
time.sleep(3)
# Tests track hub annotation if it is on the RR  
if 'hub_129603_daph scaffold_3' in driver.page_source:
     driver.find_element_by_xpath("//td[@id='td_data_hub_129603_myTrack']/div[2]/map/area").click()
else:
     driver.find_element_by_xpath("//td[@id='td_data_hub_6872_myTrack']/div[2]/map/area").click() 


# Tests Mega Hub US 
cartReset()
driver.get(machine + "/cgi-bin/hgTracks?db=hg19&measureTiming=1&hubUrl=https://hgwdev.gi.ucsc.edu/~brianlee/hubTesting/manyMulitWigsENCODE/hub.txt")
driver.get(machine + "/cgi-bin/hgTracks")
driver.find_element_by_id("positionInput").clear()
driver.find_element_by_id("positionInput").send_keys("chr10:69,644,427-69,678,147")
driver.find_element_by_id("goButton").click()
driver.find_element_by_id("hgt.out1").click()
time.sleep(3)
# Tests track hub annotation if it is on the RR   
if 'hub_336627' in driver.page_source:
     driver.find_element_by_xpath("//td[@id='td_data_hub_336627_multiWig4']/div[2]/map/area").click()
else:
     driver.find_element_by_xpath("//td[@id='td_data_hub_9717_multiWig4']/div[2]/map/area").click()


# Tests hgBlat All and Monk Seal/Human MYLK Protein
cartReset()
driver.get(machine + "/cgi-bin/hgGateway?db=hg19")
a = ActionChains(driver)
#identify element
m = driver.find_element_by_id("tools3")
#hover over element
a.move_to_element(m).perform()
#identify sub menu element
n = driver.find_element_by_id("blatMenuLink")
# hover over element and click
a.move_to_element(n).click().perform()
driver.find_element_by_id("searchAllText").click()
driver.find_element_by_name("userSeq").clear()
driver.find_element_by_name("userSeq").send_keys("MIPDTDLQVQLASRNRVGECSCQVSLMLQSSPGRAPLRGREPVSCEGLCS\\nQGAGAHGAGGDCYGTLRPGWPARGQGWPEEEDGEDVRGLLKRRVETRQHT\\nEEAIRQQEVEQLDFRDLLGKKVSTKTVSEEDLKEIPAEQMDFRANLQRQV\\nKPKTVSEEERKVHSPQQVDFRSVLAKKGTPKTPVPEKAPLPKPATPDFRS\\nVLGSKKKLPAENGSNNAEALNAKAAESPKAVSNAQPLGSLKPLGNAKPAE\\nTLRPVGNAKPAEPTKPVDNTKLAETLKPIGNAKPAETPKPMGNA")
driver.find_element_by_name("Submit").click()
driver.find_element_by_id("res0").click()
time.sleep(3)

# Closes the current window on which Selenium is running
driver.close()
