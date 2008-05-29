package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.net.*;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.regex.*;
import java.util.Random;

import com.meterware.httpunit.*;

import org.xml.sax.*;

/**
 * Utilities for getting genome database info 
 *
 */
public class HgTracks {

 /**
  * Zooms out (for now only out) and submits
  *
  * @param page     The active page
  * @param factor   The level of zooming
  */
  // factors: 1 = 1.5x zoom, 2 = 3x zoom, 3 = 10x zoom
  // factor must be 1, 2 or 3
  // use 1 by default
  public static void zoom(WebResponse page, int factor) throws Exception {

    if (factor < 1 || factor > 3) factor = 1;
    String buttonName = "hgt.out" + factor;
    WebForm form = page.getFormWithName("TrackHeaderForm");

    // System.out.println("asking form for submitbutton " + buttonName);
    SubmitButton zoomButton = form.getSubmitButton(buttonName);
    if (zoomButton == null) {
      System.out.println("Error getting zoom button");
      // throw Exception
      return;
    }
    // System.out.println("About to clickzoom button");
    zoomButton.click();
    // System.out.println("Done clicking zoom button");
  }

  /**
   * Refreshes browser screen
   * 
   * @param page     The active page
   */
   public static void refreshHGTracks(WebResponse page) throws Exception {
    // System.out.println("Clicking refresh button");
    WebForm form = page.getFormWithName("TrackForm");
    SubmitButton refreshButton = form.getSubmitButton("submit", "refresh");
    if (refreshButton == null) {
      System.err.println("Error getting refresh button");
      // throw Exception
      return;
    }
    refreshButton.click();
  }

  /**
   * Turns off base-position ruler
   * @param page  The active page
   * @return      True if Base Position has been turned off
   */
   public static boolean turnOffBasePositionHGTracks(WebResponse page) {
    try {
      // System.out.println("Turning off base position");
      WebForm form = page.getFormWithName("TrackForm");
      form.setParameter("ruler", "off");
    } catch (SAXException e) {
      System.err.println(e.getMessage());
      return false;
    }
    return true;
  }

  /**
   * Gets all links on page except track controls
   * Includes Blue Bar links
   *
   * @param page The active page
   */
   public static void printHGTracksImage(WebResponse page) {
  // gets all links except track controls
  // includes blue bar links 
  // (these are not set to true in for-loop
  // because only links from form are checked for "hide")

    try {
      WebForm form = page.getFormWithName("TrackForm");
      // get the links
      WebLink linkarray[] = page.getLinks();

      // review the links
      // screen out track controls
      for (int j = 0; j < linkarray.length; j++) {
        boolean isTrackControl = false;
        String paramarray[] = linkarray[j].getParameterNames();
        for (int l = 0; l < paramarray.length; l++) {
          // get the values for this parameter
          // why make array at all?  why not:
          /*  
            if (form.getParameterValues(paramarray[l].equals("hide")) {
              isTrackControl = true;
              continue;
            }
            */
          String valuearray[] = form.getParameterValues(paramarray[l]);
          for (int m = 0; m < valuearray.length; m++) {
            if (valuearray[m].equals("hide")) {
              isTrackControl = true;
              continue;
            }
          }
        }

        if (!isTrackControl) {
          String text = linkarray[j].asText();
          System.out.println(text);
          // String text2 = linkarray[j].getURLString();
          // System.out.println(text2);
        }
      }
    } catch (Exception e) {}
  }

  /**
   * Get random list of genes. 
   */
  public static ArrayList randomGenes(HGDBInfo dbinfo, String track, 
  	boolean isPb, boolean isShort) {
    Random random = new Random();
    ArrayList genes = QADBLibrary.getGenes(dbinfo, track, isPb);
/**
    if (isShort)
       genes = QALibrary.randomSubArray(genes, random, 5, true);
    else
       genes = QALibrary.randomSubArray(genes, random, 1000, true);
 */
    return genes;
  }
 


  /**
   * Checks each record in a table to see if browser will
   *   give a good page (200 code).
   * Looks for a specified message string on good pages
   *   (code 200) and saves those.
   * Prints to .err, .msg and .ok files
   *
   * @param dbinfo   the host, assembly, user and password object
   * @param machine  the machine on which to run the check
   * @param assembly the genome to check
   * @param table    the table to check
   */
   public static void hggene(HGDBInfo dbinfo, String machine, String assembly,                            String table, boolean quickOn) {

    WebConversation wc = new WebConversation();
    Random random = new Random();

    ArrayList kglist = randomGenes(dbinfo, table, false, quickOn);
    Iterator kgiter  = kglist.iterator();

    // open outfiles to separate OK from errors
    String errorFile = new String(assembly + "." + table + ".errors");
    String outFile   = new String(assembly + "." + table + ".ok");
    String msgFile   = new String(assembly + "." + table + ".msg");
    try {
      FileWriter     fwOk   = new FileWriter(outFile);
      BufferedWriter bwOk   = new BufferedWriter(fwOk);
      PrintWriter    outOk  = new PrintWriter(bwOk);

      FileWriter     fwErr  = new FileWriter(errorFile);
      BufferedWriter bwErr  = new BufferedWriter(fwErr);
      PrintWriter    outErr = new PrintWriter(bwErr);

      FileWriter     fwMsg  = new FileWriter(msgFile);
      BufferedWriter bwMsg  = new BufferedWriter(fwMsg);
      PrintWriter    outMsg = new PrintWriter(bwMsg);

      // check each table entry for a returned page
      while (kgiter.hasNext()) {
        // not using all of the elements
        KnownGene kg = (KnownGene) kgiter.next();
        String url = "http://" + machine + "/cgi-bin/hgGene?db=" + assembly +
                     "&hgg_gene="  + kg.name +
                     "&hgg_chrom=" + kg.chrom +
                     "&hgg_start=" + kg.txStart +
                     "&hgg_end="   + kg.txEnd;

        System.out.println(url);

        WebRequest req = new GetMethodWebRequest(url);
        try {
          // System.out.print("trying to get response");
          WebResponse page = wc.getResponse(req);
          int    code = page.getResponseCode();
          String text = page.getText();

          // check for error message
          String msgString = "does not have Exon info";
          Pattern p = Pattern.compile(msgString);
          Matcher m = p.matcher(text);
          boolean b = m.find();

          System.out.println(" code = " + code);
          if (code != 200) {
             // write to errorfile
             outErr.println(url + ":\n Unexpected response code " + code);
             outErr.println("------------------------------------");
          } else if (b == true) {    
             outMsg.println(url + ":\n found: " + msgString);
             outMsg.println("------------------------------------");
          } else {
             outOk.println(url);
          }
        } catch (Exception e) {
          System.out.println("Unexpected error getting response code");
        }
      }
      outOk.close();
      outErr.close();
      outMsg.close();
    } catch (IOException e) {
        System.out.println("Couldn't open output file:" + errorFile);
        System.out.println("   or                     " +   outFile);
    }
  }

  /**
   * Checks each record in table to see if pbTracks (proteome) will
   *   give a good page (200 code).
   * Prints to .err, .ok and .msg  files
   *
   * @param dbinfo   The host, assembly, user and password object
   * @param server  The server on which to run the check
   * @param assembly The genome to check
   * @param table    The table to check
   */
   public static void pbgene(HGDBInfo dbinfo, String server, String assembly, String table, boolean quickOn) {

    WebConversation wc = new WebConversation();
    Random random = new Random();

    ArrayList kglist = randomGenes(dbinfo, table, true, quickOn);
    Iterator kgiter  = kglist.iterator();

    // open outfiles to separate OK from errors
    String errorFile = new String(assembly + ".proteome.errors");
    String outFile   = new String(assembly + ".proteome.ok");
    String msgFile   = new String(assembly + ".proteome.msg");
    String msg2File  = new String(assembly + ".proteome.msg2");
    try {
      FileWriter     fwOk  = new FileWriter(outFile);
      BufferedWriter bwOk  = new BufferedWriter(fwOk);
      PrintWriter    outOk = new PrintWriter(bwOk);

      FileWriter     fwErr  = new FileWriter(errorFile);
      BufferedWriter bwErr  = new BufferedWriter(fwErr);
      PrintWriter    outErr = new PrintWriter(bwErr);

      FileWriter     fwMsg  = new FileWriter(msgFile);
      BufferedWriter bwMsg  = new BufferedWriter(fwMsg);
      PrintWriter    outMsg = new PrintWriter(bwMsg);

      FileWriter     fwMsg2  = new FileWriter(msg2File);
      BufferedWriter bwMsg2  = new BufferedWriter(fwMsg2);
      PrintWriter    outMsg2 = new PrintWriter(bwMsg2);
             
      // check each table entry for a returned page
      int code  = 0;
      int count = 0;
      while (kgiter.hasNext()) {
        // not using all of the elements
        KnownGene kg = (KnownGene) kgiter.next();
        String url = "http://" + server + "/cgi-bin/pbTracks?db=" + assembly +
                     "&proteinID="  + kg.proteinID;
        //           "&proteinID=HXA7_HUMAN";
        //           "&proteinID=OMD_HUMAN";
        // System.out.println("url = " + url);

        WebRequest req = new GetMethodWebRequest(url);
        try {
          // System.out.print("trying to get response");
          WebResponse page = wc.getResponse(req);
          code = page.getResponseCode();
          String text = page.getText();
          String firstText = text.substring(0, 1000);

          // check for error message
          // String msgString = "does not have";
          String msgString = "Sorry, cannot display";
          Pattern p = Pattern.compile(msgString);
          Matcher m = p.matcher(text);
          boolean b = m.find();
          
          count++;
       
       
          // System.out.println(" code = " + code);
          System.out.println(" URL = " + url);
          // System.out.println(" firstText = " + firstText);
          // System.out.println(" text = " + text);
          System.out.println(" ----------------------");
          System.out.println(" boolean b = " + b);
          if (b) System.out.println(" means '" + msgString + "'");
          System.out.println(" count = " + count);
          System.out.println("-----------------------------------------------");
        

          if (code != 200) {
             // write to errorfile
             outErr.println(url + "\n Unexpected response code " + code);
             outErr.println("------------------------------------");
          } else if (b) {    
             outMsg.println(url + "\n found: " + msgString);
             outMsg.println("count = " + count);
             outMsg.println("------------------------------------");
             outMsg2.println(kg.proteinID);
             outMsg2.println("count = " + count);
          } else {
             outOk.println(url);
          }
        } catch (Exception e) {
          System.out.println("Unexpected error getting response code");
          outErr.println(url + "\n Unexpected response code " + code);
          outErr.println(count);
          outErr.println("------------------------------------");
        }
      }
      outOk.close();
      outErr.close();
      outMsg.close();
      outMsg2.close();
    } catch (IOException e) {
        System.out.println("Couldn't open output file:" + errorFile);
        System.out.println("   or                     " +   outFile);
    }
  }

 /**
  *  Steps through all chromosomes to see if zoom-out will work
  *    and give a good page (200 code) for all items in track.
  *  Zooms out 10x twice
  *
  * @param machine     The machine on which to run the check
  * @param assembly    The genome to check
  * @param chroms      The chromosomes to check
  * @param track       The name of the track control
  * @param mode        The chromosome view (all, default) under scrutiny
  * @param defaultPos  The default position for the assembly
  * @param displayMode The view level of the track (hide, squish, etc)
  */
  // track is the name of the track control   
  public static void exerciseTrack(String machine, String assembly,
                            ArrayList chroms, String track, 
                            String mode, String defaultPos,
                            String displayMode, int zoomCount) throws Exception {

    String url1, url2, url3;
    PositionIterator mypi;

    // create the WebConversation (httpunit container for session 
    // context)

    WebConversation wc = new WebConversation();

    // System.out.println("Entering exercise track for " + track);

    if (mode.equals("all")) {
      mypi = new PositionIterator(chroms);
    } else if (mode.equals("default")) {
      mypi = new PositionIterator(defaultPos);
    } else {
      System.out.println("mode not supported: " + mode);
      return;
    }

    while (mypi.hasNext()) {

      // System.out.println("Getting next position");
      // currently assuming first 10kb
      // also need to implement middle and last 10kb
      // also full chrom view
      Position mypos = mypi.getNextPosition();

      url1 = "http://" + machine;
      url1 = url1 + "/cgi-bin/hgTracks?db=" + assembly;

      // set position
      String format = mypos.format;
      if (format.equals("full")) {
        // full = position fully defined with start and end coords
        url2 = url1 + "&position=" + mypos.chromName;
        url2 = url2 + ":" + mypos.startPos;
        url2 = url2 + "-" + mypos.endPos;
      } else {
        url2 = url1 + "&position=" + mypos.stringVal;
      }

      // hide all and turn off ruler
      url3 = url2 + "&hgt.hideAll=yes&ruler=off";

      // System.out.println();
      System.out.println(url3);

      WebRequest req = new GetMethodWebRequest(url3);

      WebResponse page = wc.getResponse(req);
      int code = page.getResponseCode();
      if (code != 200) {
        System.out.println("Unexpected response code " + code);
      }
      WebForm form = page.getFormWithName("TrackForm");
      // this will fail gracefully if we attempt an unsupported
      // display mode
      // That is, it returns, throwing an exception
      form.setParameter(track, displayMode);
      HgTracks.refreshHGTracks(page);

      for (int zooms = 1; zooms <= zoomCount; zooms++) {
        page = wc.getCurrentPage();
        ArrayList links1 = 
          HgTracks.getMatchingLinks(page, "hgc", "DNA");
        HgTracks.checkLinks(wc, links1);
	
        if (zooms == zoomCount) break;
        if (links1.size() >= 10000) {
            // Stop zooming if there's a lot of links on this page (b/c that means there will be even more when we zoom out).
            // This is to fix the problem with running out of memory when loading pages with lots of links; e.g. ce4.nucleosome,
            // where we were running out of memory after taking at least an hour of processing on a page with 139308 links.
            System.err.println("Breaking out early in exerciseTrack because of excessive number of links; url1: " + url1 + "; links1.size(): " + links1.size());
            break;
        }
        HgTracks.zoom(page, 3);
      }
    }
  }

 /**
  * Check first lines of WebLink objects,
  *   and report if response code is other than 200
  *   or report if "HGERROR"
  *
  * @param wc          The open web conversation
  * @param links       An ArrayList of links to check
  */
  // given a ArrayList of WebLink objects,
  // follow up to MAXLINKS of them
  // report if response code is other than 200
  // report if "HGERROR"
  public static void checkLinks(WebConversation wc, ArrayList links) 
                         throws Exception {
    
    final int MAXLINKS = 4;
    int count = 0;
    Iterator iter = links.iterator();
    while (iter.hasNext()) {
      WebLink link = (WebLink) iter.next();
      link.click();
      WebResponse page = wc.getCurrentPage();
      int code = page.getResponseCode();
      if (code != 200) {
        System.out.println("Unexpected response code " + code);
      }
      String text = page.getText();
      int index = text.indexOf("HGERROR");
      if (index > 0) {
        System.out.println("Error detected:");
        String errortext = text.substring(index);
        System.out.println(errortext);
      }
      count++;
      if (count > MAXLINKS) return;
    }
  }

 /**
  * Creates an ArrayList of WebLink objects
  * with the property that matchString is in the URL
  * and excludeString is not the ALT tag
  *
  * @param page           The active page
  * @param matchString    The string that must be in the URL
  * @param excludeString  The string that must not be in the URL
  * @return               List of links that meet the criteria
  */
  // creates an ArrayList of WebLink objects
  // with the property that matchString is in the URL
  // and excludeString is not the ALT tag
  public static ArrayList getMatchingLinks(WebResponse page, 
                                    String matchString,
                                    String excludeString) throws Exception {

    ArrayList matchLinks = new ArrayList();
    int count = 0;

    // System.out.println("Get matching links");

    WebLink links[] = page.getLinks();
    for (int i=0; i < links.length; i++) {
      String url = links[i].getURLString();
      int index = url.indexOf(matchString);
      String text = links[i].asText();
      // System.out.println("linktext = " + text);
      if (index != -1 && !text.equals(excludeString)) {
        matchLinks.add(links[i]);
        count++;
        // System.out.println(links[i].asText());
      }
    }
    System.out.println("Links found = " + count);
    return (matchLinks);
  }

 /**
  * Makes ArrayList of all tracks in the assembly name in myURL
  *
  * @param myURL       The URL of the query, contains assembly name
  * @param defaultPos  The default position of the assembly
  * @param debug       Whether debug mode is on
  * @return            List of tracks in currrent assembly
  */ 
  public static ArrayList getTrackControls(String myURL, String defaultPos, 
                                          boolean debug) {

    ArrayList paramlist = new ArrayList();

    WebConversation wc = new WebConversation();

    myURL = myURL + "&position=" + defaultPos;
    myURL = myURL + "&hgt.hideAll=yes";

    if (debug) System.out.println(myURL);
    WebRequest req = new GetMethodWebRequest(myURL);

    try {
      WebResponse page = wc.getResponse(req);
      int code = page.getResponseCode();
      if (code != 200) {
        System.out.println("Unexpected response code = " + code);
      }
      page = wc.getCurrentPage();
      WebForm form = page.getFormWithName("TrackForm");
      String paramarray[] = form.getParameterNames();
      for (int i=0; i < paramarray.length; i++) {
        if (debug) System.out.println(paramarray[i]);
        // get the values for this parameter
        String valuearray[] = form.getParameterValues(paramarray[i]);
        for (int j=0; j < valuearray.length; j++) {
          // check if hide is value
          if (valuearray[j].equals("hide")) {
            paramlist.add(paramarray[i]);
            break;  
          }
        }
      }
    } catch (Exception e) {
      System.out.println(e.getMessage());
    }
    return (paramlist);
  }

  
  /**
   * If 'track' is all, then get list of all tracks from
   * calling and parsing myURL.  Otherwise just return track.
   */    
  public static ArrayList getTrackControlOrAll(String myURL, String defaultPos, 
                                          String track, boolean debug) {
    if (track.equals("all"))
        return getTrackControls(myURL, defaultPos, debug);
    ArrayList a = new ArrayList();
    a.add(track);
    return a;
  }

 /**
  * Sets all track controls to "hide"
  *
  * @param page  The active page
  * @return      True if successful
  */
  public static boolean hideAllHGTracks(WebResponse page) {

    try {
      // only 1 form on hgTracks
      WebForm form = page.getFormWithName("TrackHeaderForm");
      SubmitButton hideButton = form.getSubmitButton("hgt.hideAll");
      if (hideButton == null) {
        System.err.println("Error getting hide button");
	return false;
      }
      // System.out.println("Clicking hide all");
      hideButton.click();
    } catch (MalformedURLException e) {
      System.err.println(e.getMessage());
      return false;
    } catch (SAXException e) {
      System.err.println(e.getMessage());
      return false;
    } catch (IOException e) {
      System.err.println(e.getMessage());
      return false;
    }
    return true;
  }

 /**
  * Prints all links to command line
  *
  * @param page  The active page
  */
  public static void printLinks(WebResponse page) {
    try {
      WebLink linkarray[] = page.getLinks();
      System.out.println("List of links:");
      for (int i=0; i< linkarray.length; i++) {
        System.out.println(linkarray[i].asText());
        // System.out.println(linkarray[i].getURLString());
      }
    } catch (SAXException e) {
      System.err.println(e.getMessage());
    }
  }

 /**
  * Checks if link is http or https and clicks it.
  * Prints URL or "skipping" to command line.
  *
  * @param link  The link being checked
  * @return      True if is correctly formed, regardless of type 
  */
  public static boolean checkLink(WebLink link) {
    try {
      System.out.println("Link = " + link.asText());
      WebRequest detailReq = link.getRequest();
      URL myURL = detailReq.getURL();
      String myProtocol = myURL.getProtocol();
      if (myProtocol.equals("http") | myProtocol.equals("https")) {
        System.out.println("URL = " + myURL.toString() + "\n");
        link.click();
      } else {
        System.out.println("Skipping protocol " + myProtocol);
      }
    } catch (Exception e) {
      System.err.println(e.getMessage());
      return false;
    }
    return true;
  }

 /**
  * Gets list of links from image in browser
  *
  * @param trackContainer  The track display properties: position coords
  * @param baseURL         The http, machine name and assembly info
  * @param skiplinks       The number of initial links not to check
  * @param tc1             Text that prevents a link from being added to list 
  * @param debug           Whether debug mode is on
  * @return                The list of links
  */
  public static ArrayList getListFromImage(TrackContainer trackContainer,
                                    String baseURL,
                                    int skiplinks, String tc1,
                                    boolean debug) {

    ArrayList weblist = new ArrayList();

    if (debug) {
      System.out.println("Web values");
    }

    WebConversation wc = new WebConversation();

    String webURL = baseURL + "&position=";
    webURL = webURL + trackContainer.chrom + ":";
    webURL = webURL + trackContainer.startpos + "-";
    webURL = webURL + trackContainer.endpos;

    String hideURL = webURL + "&hgt.hideAll=yes";

    WebRequest req = new GetMethodWebRequest(hideURL);

    try {

      // hide all
      WebResponse page = wc.getResponse(req);

      // get the page again
      page = wc.getCurrentPage();

      // req = new GetMethodWebRequest(webURL);

      // page = wc.getResponse(req);

      // switch to "hide all" mode
      // HgTracks.hideAllHGTracks(page);
    
      // get the page again
      // page = wc.getCurrentPage();

      // turn off base position track control 
      HgTracks.turnOffBasePositionHGTracks(page);
      HgTracks.refreshHGTracks(page);

      // get the page again
      page = wc.getCurrentPage();

      WebForm form = page.getFormWithName("TrackForm");
      String paramarray[] = form.getParameterNames();

      // find the track control
      // could I assume that it is there?
      for (int i = 0; i < paramarray.length; i++) {
        if(paramarray[i].equals(trackContainer.trackcontrol)) {
          form.setParameter(trackContainer.trackcontrol, "full");
          HgTracks.refreshHGTracks(page);
          // get the page again
          page = wc.getCurrentPage();
          // get the links
          WebLink linkarray[] = page.getLinks();
          // print the links
          // skip past skiplinks, and past the track control
          for (int j = skiplinks + 2; j < linkarray.length; j++) {
            String text = linkarray[j].asText();
            if (text.equals(tc1)) break;
            if (debug) {
              System.out.println(text);
            }
            weblist.add(text);
          }
          break;
        } // end if
      } // end for
    
    } catch (Exception e) {
      System.err.println(e.getMessage());
    }
    return (weblist);
  }
}
