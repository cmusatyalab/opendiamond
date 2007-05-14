package edu.cmu.cs.diamond.opendiamond;

import edu.cmu.cs.diamond.opendiamond.glue.OpenDiamond;
import edu.cmu.cs.diamond.opendiamond.glue.SWIGTYPE_p_p_void;

public class Result {
    final private SWIGTYPE_p_p_void obj_handle;

    Result(SWIGTYPE_p_p_void obj_handle) {
        this.obj_handle = obj_handle;
    }

    public byte[] getData() {

    }

    public byte[] getValue(byte[] key) {

    }

    public byte[][] getKeys() {

    }

    @Override
    protected void finalize() throws Throwable {
        OpenDiamond.ls_release_object(null, OpenDiamond
                .deref_void_cookie(obj_handle));
        OpenDiamond.delete_void_cookie(obj_handle);
    }
}
