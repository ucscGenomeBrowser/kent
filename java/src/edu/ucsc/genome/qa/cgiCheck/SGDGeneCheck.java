import java.io.*;
import java.net.*;
import java.sql.*;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.Properties;


/** 
 *  For all rows in sgdGene, view details page.
 *  For all pages viewed, check for non-200 return code.
 *  Doesn't click into any links.
 *  Doesn't check for HGERROR.  
 */
public class SGDGeneCheck {

 /** 
  *  Runs program looking for a non-200 code return from sgdGene.
  *    Prints to Command line.
  */
  public static void main(String[] args) {

    boolean debug = false;

    String machine = "hgwbeta.cse.ucsc.edu";
    String assembly = "sacCer1";
    String table = "sgdGene";

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
    
    // get read access to database
    HGDBInfo metadbinfo, dbinfo; 
    try {
      metadbinfo = new HGDBInfo("hgwbeta", "hgcentraltest");
      dbinfo = new HGDBInfo("localhost", assembly);
    } catch (Exception e) {
      System.out.println(e.toString());
      return;
    }
    if (!metadbinfo.validate()) return;

    if (!dbinfo.validate()) {
      System.out.println("Cannot connect to database for " + assembly);
      return;
    }

    try {
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
        HgTracks.hggene(dbinfo, machine, assembly, track, table, false);
        System.out.println();
      }
    } catch (Exception e) {
      System.out.println(e.getMessage());
    }
  }
}
