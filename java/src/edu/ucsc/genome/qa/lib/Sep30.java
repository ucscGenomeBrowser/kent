import java.io.*;
import java.net.*;

import com.meterware.httpunit.*;
import org.xml.sax.*;

import java.util.ArrayList;

class Sep30 {
  public static void main(String[] args) {

    String url1, url2, url3;

    String machine = "hgw7.cse.ucsc.edu";
    String assembly = "hg16";
    String defaultPos = "chr4:60000001-90000000";
    String track = "cytoBand";

    WebConversation wc = new WebConversation();

    url1 = "http://" + machine;
    url1 = url1 + "/cgi-bin/hgTracks?db=" + assembly;
    url2 = url1 + "&position=" + defaultPos;

    // hide all and turn off ruler
    url3 = url2 + "&hgt.hideAll=yes&ruler=off";

    WebRequest req = new GetMethodWebRequest(url3);

    try {
      System.out.println("Get page");
      WebResponse page = wc.getResponse(req);
      System.out.println("Get form");
      WebForm form = page.getForms()[0];
      System.out.println("Set " + track + " to full");
      form.setParameter(track, "full");
      // HGWebLibrary.refreshHGTracks(page);
      System.out.println("Get refresh button");
      SubmitButton refreshButton = 
        form.getSubmitButton("submit", "refresh");
      System.out.println("Click refresh button");
      refreshButton.click();
      System.out.println("Get current page");
      page = wc.getCurrentPage();
      System.out.println("Get matching links");
      ArrayList links1 =
        HGWebLibrary.getMatchingLinks(page, "hgc", "DNA");
      System.out.println("Check links");
      HGWebLibrary.checkLinks(wc, links1);

      // System.out.println("Get current page");
      // page = wc.getCurrentPage();
      // System.out.println("Get form");
      // form = page.getForms()[0];

      System.out.println("Get zoom button");
      SubmitButton zoomButton = form.getSubmitButton("hgt.out3");
      System.out.println("Click zoom button");
      zoomButton.click();
      page = wc.getCurrentPage();
      System.out.println("Get matching links");
      ArrayList links2 = 
        HGWebLibrary.getMatchingLinks(page, "hgc", "DNA");
      System.out.println("Check links");
      HGWebLibrary.checkLinks(wc, links2);
    } catch (Exception e) {
      System.out.println(e.getMessage());
    }
      
  }
}
