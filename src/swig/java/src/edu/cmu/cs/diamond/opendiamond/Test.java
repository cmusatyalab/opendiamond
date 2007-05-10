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
        Scope s = scopes.get(0);

        FilterCode c;
        try {
            c = new FilterCode(
                    new FileInputStream("/opt/diamond/lib/fil_rgb.a"));
            Filter f = new Filter("RGB", c, "f_eval_img2rgb", "f_init_img2rgb",
                    "f_fini_img2rgb", 1, new String[0], new String[0], 400);
            System.out.println(f);
        } catch (FileNotFoundException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
    }
}
