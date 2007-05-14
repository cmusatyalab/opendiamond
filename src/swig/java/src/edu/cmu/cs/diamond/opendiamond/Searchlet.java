package edu.cmu.cs.diamond.opendiamond;

import java.io.*;
import java.util.ArrayList;
import java.util.List;

public class Searchlet {
    private List<Filter> filters = new ArrayList<Filter>();
    
    public void addFilter(Filter f) {
        filters.add(f);
    }
    
    public File createFilterSpecFile() throws IOException {
        File out = File.createTempFile("filterspec", ".txt");
        out.deleteOnExit();

        Writer w = new FileWriter(out);
        for (Filter f : filters) {
            w.write(f.toString());
        }
        
        w.close();
        return out;
    }
    
    public File[] createFilterFiles() throws IOException {
        File result[] = new File[filters.size()];

        int i = 0;
        for (Filter f : filters) {
            File file = File.createTempFile("filter", ".bin");
            file.deleteOnExit();
            
            DataOutputStream out = new DataOutputStream(new FileOutputStream(file));
            out.write(f.getFilterCode().getBytes());
            out.close();
            
            result[i++] = file;
        }
        
        return result;
    }
}
