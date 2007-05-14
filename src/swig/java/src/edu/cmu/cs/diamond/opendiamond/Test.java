package edu.cmu.cs.diamond.opendiamond;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.List;

public class Test {

    /**
     * @param args
     */
    public static void main(String[] args) {
        // get scopes
        List<Scope> scopes = ScopeSource.getPredefinedScopeList();
        for (Scope scope : scopes) {
            System.out.println(scope);
        }

        // use first scope
        Scope scope = scopes.get(0);

        // set up the rgb filter
        Filter rgb = null;
        try {
            FilterCode c = new FilterCode(new FileInputStream(
                    "/opt/diamond/lib/fil_rgb.a"));
            rgb = new Filter("RGB", c, "f_eval_img2rgb", "f_init_img2rgb",
                    "f_fini_img2rgb", 1, new String[0], new String[0], 400);
            System.out.println(rgb);
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }

        // make a new search
        Search search = new Search();
        search.setScope(scope);

        // make a new searchlet
        Searchlet searchlet = new Searchlet();
        searchlet.addFilter(rgb);
        searchlet.setApplicationDependencies(new String[] { "RGB" });
        try {
            search.setSearchlet(searchlet);
        } catch (IOException e) {
            e.printStackTrace();
        }

        // begin search
        search.startSearch();

        // read 10 results
        Result r;
        int count = 0;
        while ((r = search.getNextResult()) != null && count < 10) {
            System.out.println(r);
            count++;
        }

        // disconnect
        search.close();
    }
}
