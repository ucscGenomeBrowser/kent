import java.io.*;
import java.util.*;

/**
 *  Genome Library Tools. 
 */
public class QALibrary {

 /**
  *  Checks for existence of file.
  *
  *  @param  filename  The name of the file.
  *
  *  @returns  True if filename exists.
  */
  static public boolean checkFile(String filename) {

    try {
      File fin = new File(filename);
      if (fin.exists()) {
        return (true);
      }
      return (false);
    } catch (Exception e) {
      return (false);
    }
  }

 /**
  *  Checks for existence of directory.
  *
  *  @param  dirname  The name of the directory.
  *
  *  @returns  True if directory name exists.
  */
  static public boolean checkDir(String dirname) {
  
    try {
      File fin = new File(dirname);
      if (fin.exists() && fin.isDirectory()) {
        return (true);
      }
      return (false);
    } catch (Exception e) {
      return (false);
    }
  }

 /**
  *  Reads a file into an ArrayList.
  *
  *  @param  inputfile  The name of the input file.
  *
  *  @returns  List of Strings representing lines in the file.
  */
  static public ArrayList readFile(String inputfile) {

    ArrayList newList = new ArrayList();
    String thisLine;
    BufferedReader br;

    try {
      FileReader fin = new FileReader(inputfile);
      br = new BufferedReader(fin);
      while ((thisLine = br.readLine()) != null) {
        newList.add(thisLine);
      }
      br.close();
      fin.close();
    } catch (Exception e) {
      System.out.println("Exception reading file " + inputfile);
      System.out.println(e.getMessage());
    }
    return (newList);
  }
      
  // read properties
 /**
  *  Reads a property list into a Properties object.
  *
  *  @param  filename  The file containing the properties.
  *
  *  @returns  The Properties listed in the file.
  */
  public static Properties readProps(String filename) {
    FileInputStream fin = null;
    Properties proplist = new Properties();
    try {
      fin = new FileInputStream(filename);
      proplist.load(fin);
      fin.close();
    } catch (FileNotFoundException e) {
      System.out.println("Can't find properties file");
      // proplist.put("prop0", "??");
    } catch (IOException e) {
      System.out.println("Error reading properties file");
    }
    return proplist;
  }

 /**
  *  Counts the hnubmer of characters in a file.
  *
  *  @param  filename  The file.
  *
  *  @returns  The number of characters in the file.
  */
  static public int numCharsInFile(String filename) {

    FileReader fin;
    BufferedReader br;
    int inputChar;
    int inputlen=0; 

    try {
      fin = new FileReader(filename);
      br = new BufferedReader(fin);
      while (true) {
        inputChar = br.read();
        if (inputChar == -1) break;
        inputlen++;
      }
      br.close();
      fin.close();
    } catch (Exception e) {
      System.out.println("Error counting chars in " + filename);
      System.out.println(e.getMessage());
    }
    return (inputlen);
  }
}
