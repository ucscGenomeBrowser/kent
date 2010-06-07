package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.util.*;

/**
 * This container holds all track properties 
 */
public class TrackContainer {

  // data
  public String chrom;
  // could store these as strings
  public int startpos;
  public int endpos;
  public String idcol;
  public String startcol;
  public String endcol;
  public String table;
  public String trackcontrol;
  public boolean split;

  // constructors

  // this constructor searches the properties object  
 /**
  * This constructor searches the properties object.
  * 
  * @param trackProps  The properties of the track 
  */
  public TrackContainer(Properties trackProps) {

    chrom = trackProps.getProperty("chrom");

    // use try/catch?
    startpos = 
      Integer.parseInt(trackProps.getProperty("startpos"));

    endpos = 
      Integer.parseInt(trackProps.getProperty("endpos"));

    idcol = trackProps.getProperty("idcol");

    startcol = trackProps.getProperty("startcol");

    endcol = trackProps.getProperty("endcol");

    table = trackProps.getProperty("table");

    trackcontrol = trackProps.getProperty("trackcontrol");

    String splitString = trackProps.getProperty("split");
    if (splitString.equals("yes")) {
      split = true;
    } else {
      split = false;
    }

  }

 /**
  * Checks properties for acceptable values
  *
  * @return  True if all properties are within bounds
  */
  public boolean validate() {
    if (chrom == null) return false;
    if (startpos < 0) return false;
    if (endpos < 0) return false;
    if (idcol == null) return false;
    if (startcol == null) return false;
    if (endcol == null) return false;
    if (table == null) return false;
    if (trackcontrol == null) return false;
    return true;
  }
}
