package edu.cmu.cs.diamond.opendiamond;

import java.io.*;
import java.util.ArrayList;
import java.util.List;

public class Searchlet {
    final private List<Filter> filters = new ArrayList<Filter>();
    private String[] dependencies;
    
    public void addFilter(Filter f) {
        filters.add(f);
    }
    
    public void setApplicationDependencies(String dependencies[]) {
        this.dependencies = new String[dependencies.length];
        System.arraycopy(dependencies, 0, this.dependencies, 0, dependencies.length);
    }
    
    File createFilterSpecFile() throws IOException {
        File out = File.createTempFile("filterspec", ".txt");
        out.deleteOnExit();

        Writer w = new FileWriter(out);
        for (Filter f : filters) {
            w.write(f.toString());
        }
        
        if (dependencies != null) {
            w.write("FILTER APPLICATION\n");
            for (String d : dependencies) {
                w.write("REQUIRES " + d + "\n");
            }
        }
        
        w.close();
        return out;
    }
    
    File[] createFilterFiles() throws IOException {
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
