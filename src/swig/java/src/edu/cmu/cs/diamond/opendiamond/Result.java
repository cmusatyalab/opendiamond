package edu.cmu.cs.diamond.opendiamond;

import java.util.ArrayList;
import java.util.List;

import edu.cmu.cs.diamond.opendiamond.glue.*;

public class Result {
    final private SWIGTYPE_p_p_void obj_handle;

    Result(SWIGTYPE_p_p_void obj_handle) {
        this.obj_handle = obj_handle;
    }

    public byte[] getData() {
        return getValue(null);
    }

    public byte[] getValue(byte[] key) {
        byte result[];
        long lenp[] = { 0 };

        SWIGTYPE_p_p_unsigned_char data = OpenDiamond.create_data_cookie();
        try {
            data = OpenDiamond.create_data_cookie();

            if (key == null) {
                OpenDiamond.lf_next_block(OpenDiamond
                        .deref_void_cookie(obj_handle), Integer.MAX_VALUE,
                        lenp, data);
            } else {
                OpenDiamond.lf_ref_attr(OpenDiamond
                        .deref_void_cookie(obj_handle), new String(key), lenp,
                        data);
            }

            // copy
            result = extractData(lenp, data);
        } finally {
            OpenDiamond.delete_data_cookie(data);
        }
        return result;
    }

    private byte[] extractData(long[] len, SWIGTYPE_p_p_unsigned_char data) {
        byte[] result;
        result = new byte[(int) len[0]];
        
        byteArray d = OpenDiamond.deref_data_cookie(data);
        for (int i = 0; i < result.length; i++) {
            result[i] = (byte) d.getitem(i);
        }
        return result;
    }

    public List<byte[]> getKeys() {
        List<byte[]> result = new ArrayList<byte[]>();

        // make cookies
        SWIGTYPE_p_p_void cookie = null;
        SWIGTYPE_p_p_char name = null;
        SWIGTYPE_p_p_unsigned_char data = null;

        try {
            cookie = OpenDiamond.create_void_cookie();
            name = OpenDiamond.create_char_cookie();
            data = OpenDiamond.create_data_cookie();
            long len[] = { 0 };

            // first
            int err = OpenDiamond.lf_first_attr(OpenDiamond
                    .deref_void_cookie(obj_handle), name, len, data, cookie);
            while (err == 0) {
                result.add(extractData(len, name));
                err = OpenDiamond
                        .lf_next_attr(
                                OpenDiamond.deref_void_cookie(obj_handle),
                                name, len, data, cookie);
            }
        } finally {
            OpenDiamond.delete_void_cookie(cookie);
            OpenDiamond.delete_char_cookie(name);
            OpenDiamond.delete_data_cookie(data);
        }

        return result;
    }

    private byte[] extractData(long[] len, SWIGTYPE_p_p_char name) {
        String d = OpenDiamond.deref_char_cookie(name);
        return d.getBytes();
    }

    @Override
    protected void finalize() throws Throwable {
        OpenDiamond.ls_release_object(null, OpenDiamond
                .deref_void_cookie(obj_handle));
        OpenDiamond.delete_void_cookie(obj_handle);
    }
    
    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("Result [");
        
        for (byte[] data : getKeys()) {
            sb.append(" '" + new String(data) + "'");
        }
        sb.append(" ]");
        return sb.toString();
    }
}
