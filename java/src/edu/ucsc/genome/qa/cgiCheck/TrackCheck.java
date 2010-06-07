package edu.ucsc.genome.qa.cgiCheck;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.net.*;
import java.sql.*;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.Properties;

/** 
 *  Loop over assemblies defined in dbDb.
 *  Loop over valid tracks in that assembly.
 *  Set to default position.
 *  Set track control to full.
 *  View image, click into MAXLINK details.
 *  Zoom out 10x, view image, click into MAXLINK details.
 *  Zoom out 10x again, view image, click into MAXLINK details.
 *  For all pages viewed, check for non-200 return code.
 *  For hgc pages, check for HGERROR.  
 */

public class TrackCheck {

 static void usage() {
    System.out.println(
      "TrackCheck - check hgTrack cgi by iterating over tracks.\n" +
      "Enable each track in full mode, check MAXLINK details.\n" +
      "Check for non-200 return code and HGERROR.\n" +
      "Perform zoom out 10x, twice.\n" +
      "Uses defaultPos.\n" +
      "Uses all assemblies.\n" +
      
      "usage:\n" +
      "   java TrackCheck propertiesFile\n" +
      "where properties files may contain database, table." +
      "   java TrackCheck default\n" +
      "This will use the default properties\n"
      );
    System.exit(-1);
 }

 /** 
  *  Runs all assemblies.  Checks zoom out at default position.
  */
  public static void main(String[] args) {

    boolean debug = false;

    /* Process command line properties, and load them
     * into machine and table. */
    if (args.length != 1)
        usage();
    TestTarget target = new TestTarget(args[0]);
    System.out.println("\nTarget is:\n" + target);

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
    
    // get metadata
    HGDBInfo metadbinfo; 
    try {
      metadbinfo = new HGDBInfo(target.machine, "hgcentralbeta");
    } catch (Exception e) {
      System.out.println(e.toString());
      return;
    }
    if (!metadbinfo.validate()) return;

    ArrayList assemblyList = QADBLibrary.getDatabaseOrAll(metadbinfo, target.dbSpec);

    // iterate over assembly list
    Iterator assemblyIter = assemblyList.iterator();
    while (assemblyIter.hasNext()) {
      String assembly = (String) assemblyIter.next();
      System.out.println("Assembly = " + assembly);

      try {
	// create HGDBInfo for this assembly
	HGDBInfo dbinfo = new HGDBInfo(target.machine, assembly);
	if (!dbinfo.validate()) {
	  System.out.println("Cannot connect to database for " + assembly);
	} else { // find default pos for this assembly
	  String defaultPos = QADBLibrary.getDefaultPosition(metadbinfo,assembly);

	  // get chromInfo for this assembly
	  ArrayList chroms = QADBLibrary.getChromInfo(dbinfo);
	  if (debug) {
	    int count1 = chroms.size();
	    System.out.println("Count of chroms found = " + count1);
	  }

	  // smallest will typically be chrM or a chrN_random
	  // currently not using this
	  ChromInfo smallChrom = ChromLibrary.getSmallestChrom(chroms, false);
	  if (debug) {
	    System.out.println("Smallest chrom is " + smallChrom.chrom);
	  }

	  // get tracks for this assembly (read track controls from web)
	  String hgtracksURL = "http://" + target.server + "/cgi-bin/hgTracks?db=";
	  hgtracksURL = hgtracksURL + assembly;
          if (debug) {
            System.out.println("HgTracks.getTrackControlOrAll(url=" + hgtracksURL +", pos="+defaultPos+", table=" + target.table+", debug="+debug+")");
          }

	  ArrayList trackList = 
	    HgTracks.getTrackControlOrAll(hgtracksURL, defaultPos, target.table, debug);
	  if (debug) {
	    int count2 = trackList.size();
	    System.out.println("Count of tracks found = " + count2);
	  }
	  // iterate through tracks
	  Iterator trackIter = trackList.iterator();
	  int checkedCount=0;
	  while (trackIter.hasNext()) {
	    String track = (String) trackIter.next();
	    String mode = "default";
	    HgTracks.exerciseTrack(target.server, assembly, chroms, 
				       track, mode, defaultPos, "full", target.zoomCount);
	    checkedCount++;
	  }
          System.out.println("checked " + checkedCount + " of " + trackList.size() + " tracks");
	}
      } catch (Exception e) {
        System.out.println(e.getMessage());
      }
      System.out.println();
    }
  }
}
