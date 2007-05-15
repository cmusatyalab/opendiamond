package edu.cmu.cs.diamond.opendiamond;

import java.io.File;
import java.io.IOException;

import edu.cmu.cs.diamond.opendiamond.glue.OpenDiamond;
import edu.cmu.cs.diamond.opendiamond.glue.SWIGTYPE_p_p_void;
import edu.cmu.cs.diamond.opendiamond.glue.SWIGTYPE_p_void;
import edu.cmu.cs.diamond.opendiamond.glue.device_isa_t;

public class Search {
    final private SWIGTYPE_p_void handle;

    private boolean closed;

    private Searchlet searchlet;

    private Scope scope;

    public Search() {
        handle = OpenDiamond.ls_init_search();
    }

    public void setScope(Scope scope) {
        if (closed) {
            throw new ClosedSearchException();
        }
        this.scope = scope;
    }

    public void setSearchlet(Searchlet searchlet) {
        if (closed) {
            throw new ClosedSearchException();
        }
        this.searchlet = searchlet;
    }

    public void startSearch() {
        if (closed) {
            throw new ClosedSearchException();
        }

        // set scope
        OpenDiamond.ls_set_searchlist(handle, scope.getGidsSize(), scope
                .getGids());

        // prepare searchlet
        File filterspec;
        try {
            filterspec = searchlet.createFilterSpecFile();
            File filters[] = searchlet.createFilterFiles();
            OpenDiamond.ls_set_searchlet(handle, device_isa_t.DEV_ISA_IA32,
                    filters[0].getAbsolutePath(), filterspec.getAbsolutePath());
            for (int i = 1; i < filters.length; i++) {
                OpenDiamond.ls_add_filter_file(handle, device_isa_t.DEV_ISA_IA32,
                        filters[i].getAbsolutePath());
            }

        } catch (IOException e) {
            e.printStackTrace();
            return;
        }
        
        // begin
        OpenDiamond.ls_start_search(handle);
    }

    public void stopSearch() {
        if (closed) {
            throw new ClosedSearchException();
        }

        OpenDiamond.ls_terminate_search(handle);
    }

    public Result getNextResult() {
        if (closed) {
            throw new ClosedSearchException();
        }

        SWIGTYPE_p_p_void obj_handle = OpenDiamond.create_void_cookie();
        Result r = new Result(obj_handle); // Result will free the void cookie
        if (OpenDiamond.ls_next_object(handle, obj_handle, 0) == 0) {
            return r;
        } else {
            // no more objects
            return null;
        }
    }

    public void close() {
        if (closed) {
            return;
        }
        closed = true;
        OpenDiamond.ls_abort_search(handle);
    }

    @Override
    protected void finalize() throws Throwable {
        close();
    }
}
