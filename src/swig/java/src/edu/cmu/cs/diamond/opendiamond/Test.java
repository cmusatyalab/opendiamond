package edu.cmu.cs.diamond.opendiamond;

import java.awt.Image;
import java.io.ByteArrayInputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.List;

import javax.imageio.ImageIO;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JFrame;

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
        Scope scope = scopes.get(4);

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

            byte data[] = r.getData();
            // try {
            // DataOutputStream fout = new DataOutputStream(new
            // FileOutputStream("/tmp/zz.jpg"));
            // fout.write(data);
            // fout.close();
            // } catch (FileNotFoundException e1) {
            // e1.printStackTrace();
            // } catch (IOException e) {
            // // TODO Auto-generated catch block
            // e.printStackTrace();
            // }

            ByteArrayInputStream in = new ByteArrayInputStream(data);
            try {
                Image img = ImageIO.read(in);

                if (img != null) {
                    System.out.println(img);
                    JFrame j = new JFrame();
                    j.getContentPane().add(new JButton(new ImageIcon(img)));
                    j.pack();
                    j.setVisible(true);
                }
            } catch (IOException e) {
                e.printStackTrace();
            }

            count++;
        }

        // disconnect
        search.close();
    }
}
