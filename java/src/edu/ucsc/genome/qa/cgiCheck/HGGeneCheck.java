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
      "HGGeneCheck - do some basic automatic tests on hgGene cgi\n" +
      "usage:\n" +
      "   java HGGeneCheck propertiesFile\n" +
      "where properties files may contain database, table." +
      "   java HGGeneCheck default\n" +
      "This will use the default properties\n"
      );
    System.exit(0);
 }

 /** 
  *  Runs the program to check all Known Genes,
  *  looping over all assemblies, looking for non-200 return code.
  */
  public static void main(String[] args) {

    boolean debug = false;

    /* Process command line properties, and load them
     * into machine and table. */
    if (args.length != 1)
        usage();
    Properties mainProps;
    if (!args[0].equals("default"))
         mainProps = QALibrary.readProps(args[0]); 
    else
         mainProps = new Properties();
    String machine;
    machine = mainProps.getProperty("machine", "hgwbeta.cse.ucsc.edu");
    String table;
    table = mainProps.getProperty("table", "knownGene");

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
    
    // get read access to database
    Properties dbloginread;
    try {
      dbloginread = QALibrary.readProps("javadbconf.read");
    } catch (Exception e) {
      System.out.println("Cannot read javadbconf.read");
      return;
    }
    String userRead = dbloginread.getProperty("login");
    String passwordRead = dbloginread.getProperty("password");

    // get list of assemblies

    HGDBInfo metadbinfo = 
      new HGDBInfo("genome-testdb", "hgcentralbeta", userRead, passwordRead);

    if (!metadbinfo.validate()) return;

    ArrayList assemblyList = 
      QADBLibrary.getColumn(metadbinfo, "dbDb", "name", debug);

    // iterate over assembly list
    // System.out.println("iterate over assembly list");
    Iterator assemblyIter = assemblyList.iterator();
    while (assemblyIter.hasNext()) {
      String assembly = (String) assemblyIter.next();
      if (!assembly.equals("mm4")) continue;
      // System.out.println("Assembly = " + assembly);
      // create HGDBInfo for this assembly
      HGDBInfo dbinfo = new HGDBInfo("localhost", assembly, userRead, passwordRead);
      if (!dbinfo.validate()) {
        System.out.println("Cannot connect to database for " + assembly);
        continue;
      }

      try {
        // does this assembly have knownGene track?
        // could write a helper routine to do this

        // get tracks for this assembly (read track controls from web)
        String hgtracksURL = "http://" + machine + "/cgi-bin/hgTracks?db=";
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
          if (!track.equals(table)) continue;
          System.out.println(track);
          HgTracks.hggene(dbinfo, machine, assembly, track, table);
          System.out.println();
        }
      } catch (Exception e) {
        System.out.println(e.getMessage());
      }
      System.out.println();
      System.out.println();
    }
  }
}
