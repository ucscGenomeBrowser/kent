import java.io.*;
import java.net.*;
import java.sql.*;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.Calendar;
import java.util.Date;
import java.text.DateFormat;
import java.text.SimpleDateFormat;

/** 
 *  Loop over assemblies defined in dbDb.
 *  Loop over valid tracks in that assembly.
 *  Loop over 6 different positions.
 *  Loop over 4 density views.
 *  View image, click into details.
 */
public class MGCCheck {

 /** 
  *  Runs program to check mgc machine : all assemblies,
  *    all tracks, multiple positions and density views.
  */
  public static void main(String[] args) {

    boolean debug = false;

    String machine = "mgc.cse.ucsc.edu";

    SimpleDateFormat dateFormat = new SimpleDateFormat();

    // make these command line arguments!
    // or read from .hg.conf!!
    String user = "hgcat";
    String password = "XXXXXXX";

    if (!QADBLibrary.checkDriver()) return;
    
    // get list of assemblies

    DBInfo metadbinfo = 
      new DBInfo("genome-testdb", "hgcentralbeta", user, password);

    if (!metadbinfo.validate()) return;


    ArrayList assemblyList = 
      QADBLibrary.getColumn(metadbinfo, "dbDb", "name", debug);


    // iterate over assembly list
    System.out.println("iterate over assembly list");
    Iterator assemblyIter = assemblyList.iterator();
    while (assemblyIter.hasNext()) {

      String assembly = (String) assemblyIter.next();
      if (!assembly.equals("rn3")) continue;
      System.out.println("Assembly = " + assembly);

      // Start time
      Date startDate = new Date();
      String startDateString = dateFormat.format(startDate);
      System.out.println("Start time = " + startDateString);

      // create DBInfo for this assembly
      DBInfo dbinfo = new DBInfo("localhost", assembly, user, password);
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
          QAWebLibrary.getTrackControls(hgtracksURL, defaultPos, debug);
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
          QAWebLibrary.exerciseTrack(machine, assembly, chroms, 
                                     track, mode, defaultPos, "full");
          System.out.println();
        }
      } catch (Exception e) {
        System.out.println(e.getMessage());
      }
      System.out.println();
      System.out.println();
      // End time
      Date endDate = new Date();
      String endDateString = dateFormat.format(endDate);
      System.out.println("End time = " + endDateString);
    }
  }
}
