package edu.cmu.cs.diamond.opendiamond;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import edu.cmu.cs.diamond.opendiamond.glue.OpenDiamond;
import edu.cmu.cs.diamond.opendiamond.glue.SWIGTYPE_p_p_char;
import edu.cmu.cs.diamond.opendiamond.glue.SWIGTYPE_p_p_void;

public class ScopeSource {
    static public List<Scope> getPredefinedScopeList() {
        List<Scope> result = new ArrayList<Scope>();

        SWIGTYPE_p_p_char name = OpenDiamond.create_char_cookie();
        SWIGTYPE_p_p_void cookie = OpenDiamond.create_void_cookie();
        try {
            int val = OpenDiamond.nlkup_first_entry(name, cookie);
            while (val == 0) {
                result.add(new NameScope(OpenDiamond.deref_char_cookie(name)));
                val = OpenDiamond.nlkup_next_entry(name, cookie);
            }
        } finally {
            OpenDiamond.delete_char_cookie(name);
            OpenDiamond.delete_void_cookie(cookie);
        }
        
        Collections.reverse(result);
        return result;
    }
}
