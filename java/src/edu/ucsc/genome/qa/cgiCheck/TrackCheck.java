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

 /** 
  *  Runs all assemblies.  Checks zoom out at default position.
  */
  public static void main(String[] args) {

    boolean debug = false;

    // String machine = "dhcp-55-107.cse.ucsc.edu";
    String machine = "hgwbeta.cse.ucsc.edu";

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
       new HGDBInfo("hgwbeta", "hgcentralbeta", userRead, passwordRead);

    if (!metadbinfo.validate()) return;

    ArrayList assemblyList = 
      QADBLibrary.getColumn(metadbinfo, "dbDb", "name", debug);

    // iterate over assembly list
    System.out.println("iterate over assembly list");
    Iterator assemblyIter = assemblyList.iterator();
    while (assemblyIter.hasNext()) {
      String assembly = (String) assemblyIter.next();
      if(!assembly.equals("panTro1")) continue;
      System.out.println("Assembly = " + assembly);
      // create HGDBInfo for this assembly
      HGDBInfo dbinfo = new HGDBInfo("localhost", assembly, userRead, passwordRead);
      if (!dbinfo.validate()) {
        System.out.println("Cannot connect to database for " + assembly);
        continue;
      }

      // find default pos for this assembly
      try {
        String defaultPos = QADBLibrary.getDefaultPosition(metadbinfo,assembly);
        System.out.println("defaultPos = " + defaultPos);
        System.out.println();

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
        String hgtracksURL = "http://" + machine + "/cgi-bin/hgTracks?db=";
        hgtracksURL = hgtracksURL + assembly;
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
          System.out.println(track);
          String mode = "default";
          // String mode = "all";
          HgTracks.exerciseTrack(machine, assembly, chroms, 
                                     track, mode, defaultPos, "full");
          // HgTracks.exerciseTrack(machine, assembly, chroms, 
                                     // track, mode, defaultPos, "dense");
          // HgTracks.exerciseTrack(machine, assembly, chroms, 
                                     // track, mode, defaultPos, "pack");
          // HgTracks.exerciseTrack(machine, assembly, chroms, 
                                     // track, mode, defaultPos, "squish");
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
