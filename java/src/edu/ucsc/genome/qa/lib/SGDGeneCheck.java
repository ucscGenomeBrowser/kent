import java.io.*;
import java.net.*;
import java.sql.*;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.Properties;

import lib.*;

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
    Properties dbloginread;
    try {
      dbloginread = QALibrary.readProps("javadbconf.read");
    } catch (Exception e) {
      System.out.println("Cannot read javadbconf.read");
      return;
    }
    String userRead = dbloginread.getProperty("login");
    String passwordRead = dbloginread.getProperty("password");

    DBInfo metadbinfo = 
      new DBInfo("hgwbeta", "hgcentraltest", userRead, passwordRead);

    if (!metadbinfo.validate()) return;

    DBInfo dbinfo = new DBInfo("localhost", assembly, userRead, passwordRead);
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
        QAWebLibrary.getTrackControls(hgtracksURL, defaultPos, debug);
      if (debug) {
        int count2 = trackList.size();
        System.out.println("Count of tracks found = " + count2);
      }
      // iterate through tracks
      Iterator trackIter = trackList.iterator();
      while (trackIter.hasNext()) {
        String track = (String) trackIter.next();
        if (!track.equals(table)) continue;
        QAWebLibrary.hggene(dbinfo, machine, assembly, track, table);
        System.out.println();
      }
    } catch (Exception e) {
      System.out.println(e.getMessage());
    }
  }
}
