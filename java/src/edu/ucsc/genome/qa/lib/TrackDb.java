package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.sql.*;
import java.util.*;

/**
 *  This container will hold a row from the trackDb table
 */
public class TrackDb {

  // data -- matches column names except where noted
  public String tableName;
  public String shortLabel;
  public String type;
  public String longLabel;
  public int visibility;
  public float priority;
  public int colorR;
  public int colorG;
  public int colorB;
  public int altColorR;
  public int altColorG;
  public int altColorB;
  public int useScore;
  // the column is called "private" but this is a reserved word
  // in Java
  public int privateCol;
  public int restrictCount;
  public Blob restrictList;
  public Blob url;
  public Blob html;
  public String grp;
  public int canPack;
  public Blob settings;

   // constructors
 /**
  *  This constructor creates an object that holds only names from
  *     trackDb table.
  *
  *  @param   tableNameVar    The name of the table.
  */
   TrackDb(String tableNameVar) {
     tableName = tableNameVar;
   }
 
 /**
  *  This constructor creates an object that is a row for the trackDb table
  *
  *  @param   tableNameVar    The name of the table.
  *  @param   shortLabelVar   The short label for the left side of gif display.
  *  @param   typeVar         The data format of the track (e.g. bed4).
  *  @param   longLabelVar    The long label for the gif display.
  *  @param   visibilityVar   The default visibility for the track.
  *  @param   priorityVar     The priority level (display order) for the track.
  *  @param   colorRVar
  *  @param   colorGVar
  *  @param   colorBVar
  *  @param   altColorRVar
  *  @param   altColorGVar
  *  @param   altColorBVar
  *  @param   useScoreVar
  *  @param   privateVar
  *  @param   restrictCountVar
  *  @param   restrictListVar
  *  @param   urlVar
  *  @param   htmlVar         The long description for the details page.
  *  @param   grpVar          The display group for the track (mRNA, etc).
  *  @param   canPackVar
  *  @param   settingsVar
  */
  public TrackDb(String tableNameVar, String shortLabelVar,
          String typeVar, String longLabelVar,
          int visibilityVar, float priorityVar,
          int colorRVar, int colorGVar, int colorBVar,
          int altColorRVar, int altColorGVar, int altColorBVar,
          int useScoreVar, int privateVar, int restrictCountVar,
          Blob restrictListVar, Blob urlVar, Blob htmlVar,
          String grpVar, int canPackVar, Blob settingsVar) {

    tableName = tableNameVar;
    shortLabel = shortLabelVar;
    type = typeVar;
    longLabel = longLabelVar;
    visibility = visibilityVar;
    priority = priorityVar;
    colorR = colorRVar;
    colorG = colorGVar;
    colorB = colorBVar;
    altColorR = altColorRVar;
    altColorG = altColorGVar;
    altColorB = altColorBVar;
    useScore = useScoreVar;
    privateCol = privateVar;
    restrictCount = restrictCountVar;
    restrictList = restrictListVar;
    url = urlVar;
    html = htmlVar;
    grp = grpVar;
    canPack = canPackVar;
    settings = settingsVar;

  }

 /**
  *  Prints out parameters to command line.
  */
  void print() {
    System.out.println("tableName = " + tableName);
    System.out.println("shortLabel = " + shortLabel);
    System.out.println("type = " + type);
    System.out.println("longLabel = " + longLabel);
    System.out.println("visibility = " + visibility);
    System.out.println("priority = " + priority);
    System.out.println("colors = " + colorR + ", " + colorG + ", " + colorB);
    System.out.println("altcolors = " + altColorR + ", " + altColorG + ", " + altColorB);
    System.out.println("useScore = " + useScore);
    System.out.println("private = " + privateCol);
    System.out.println("restrictCount = " + restrictCount);
    System.out.println("grp = " + grp);
    System.out.println("canPack = " + canPack);
  }
}
