import java.io.*;
import java.net.*;
import java.sql.*;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.Properties;


 // modified from SGDGeneCheck to print to files
 // 01-21-04 -- kuhn

/** 
 *  For all rows in knownGene, view details page.
 *  For all pages viewed, check for non-200 return code
 *    and for any message, msgError.
 *  Doesn't click into any links.
 *  Doesn't check for HGERROR.  
 *  Prints to .ok, .error, .msg files 
 *                  via HgTracks.hggene.
 */
public class KGGeneCheck {

 /** 
  *  Runs the program to check all Known Genes for good return code.
  *    
  *  Loop over all assemblies.
  */
  public static void main(String[] args) {

    boolean debug = false;

    String machine = "hgwbeta.cse.ucsc.edu";
    String assembly = "hg16";
    String table = "knownGene";

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
    
    // get read access to database
    HGDBInfo metadbinfo; 
    try {
      metadbinfo = new HGDBInfo("hgwbeta", "hgcentraltest");
    } catch (Exception e) {
      System.out.println(e.toString());
      return;
    }
    if (!metadbinfo.validate()) return;


    try {
      HGDBInfo dbinfo = new HGDBInfo("localhost", assembly);
      if (!dbinfo.validate()) {
	System.out.println("Cannot connect to database for " + assembly);
	return;
      }
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
        HgTracks.hggene(dbinfo, machine, assembly, table);
        System.out.println();
      }
    } catch (Exception e) {
      System.out.println(e.getMessage());
    }
  }
}
