package edu.ucsc.genome.qa.cgiCheck;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.net.*;
import java.sql.*;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.Properties;

/** 
 *  For all rows in knownGene, view details page.
 *  Loops over all assemblies.
 *  For all pages viewed, check for non-200 return code.
 *  Doesn't click into any links.
 *  Doesn't check for HGERROR.  
 */
public class HGGeneCheck {

 /**
  * Print usage info and exit
  */
 static void usage() {
    System.out.println(
      "\nHGGeneCheck - do some basic automatic tests on hgGene cgi.\n" +
      "usage:\n" +
      "   HGGeneCheck propertiesFile\n" +
      "where properties files may contain machine, database, quick.\n" +
      "\n   HGGeneCheck default\n" +
      "will use the default properties (hg17)\n"
      );
    System.exit(-1);
 }

 /**
  * Return true if track has hgGene details page.
  */
 static boolean trackHasHgGene(String track) {
   if (track.equals("knownGene"))
     return true;
   if (track.equals("sangerGene"))
     return true;
   if (track.equals("bdgpGene"))
     return true;
   if (track.equals("sgdGene"))
     return true;
   if (track.equals("rgdGene2"))
     return true;
   return false;
 }

 /** 
  *  Runs the program to check all Known Genes,
  *  looping over all assemblies, looking for non-200 return code.
  */
  public static void main(String[] args) throws Exception {

    boolean debug = false;

    /* Process command line properties, and load them
     * into machine and table. */
    if (args.length < 1 || args.length > 2) {
        usage();
    }
    // for allowing full KG list search (not yet implemented).
    boolean full = false;
    if (args.length == 2) {
        full = true;
        usage();  // for now
    }
    TestTarget target = new TestTarget(args[0]);

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
    
    // get read access to database
    HGDBInfo metadbinfo = new HGDBInfo("hgwdev", "hgcentraltest");
    if (!metadbinfo.validate()) return;

    ArrayList assemblyList = QADBLibrary.getDatabaseOrAll(metadbinfo, target.dbSpec);

    // iterate over assembly list
    // System.out.println("iterate over assembly list");
    Iterator assemblyIter = assemblyList.iterator();
    while (assemblyIter.hasNext()) {
      String assembly = (String) assemblyIter.next();
      // System.out.println("Assembly = " + assembly);

      // create HGDBInfo for this assembly
      HGDBInfo dbinfo = new HGDBInfo(target.machine, assembly);
      if (!dbinfo.validate()) {
         System.err.println("Cannot connect to database for " + assembly);
         continue;
      }
      // does this assembly have knownGene track?
      // could write a helper routine to do this

      // get tracks for this assembly (read track controls from web)
      String hgtracksURL = "http://" + target.machine + "/cgi-bin/hgTracks?db=";
      hgtracksURL = hgtracksURL + assembly;
      String defaultPos = QADBLibrary.getDefaultPosition(metadbinfo,assembly);
      ArrayList trackList = 
         HgTracks.getTrackControls(hgtracksURL, defaultPos, debug);
      if (debug) {
         int count2 = trackList.size();
         System.out.println("Count of tracks found = " + count2);
      }
      // iterate through tracks
      Iterator trackIter = trackList.iterator();
      while (trackIter.hasNext()) {
         String track = (String) trackIter.next();
         if (!trackHasHgGene(track))
	    continue;
         System.out.println(track);
         HgTracks.hggene(dbinfo, target.machine, assembly, track, 
                         target.quickOn);
         System.out.println();
      }
      System.out.println();
      System.out.println();
    }
  }
}
