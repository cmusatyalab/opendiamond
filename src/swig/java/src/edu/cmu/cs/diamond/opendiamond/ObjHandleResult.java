package edu.cmu.cs.diamond.opendiamond;

import java.util.ArrayList;
import java.util.List;

import edu.cmu.cs.diamond.opendiamond.glue.*;

public class ObjHandleResult extends Result {
    final private SWIGTYPE_p_void obj_handle;

    ObjHandleResult(SWIGTYPE_p_void obj_handle) {
        this.obj_handle = obj_handle;
    }

    private byte[] savedData;
    /* (non-Javadoc)
     * @see edu.cmu.cs.diamond.opendiamond.IResult#getData()
     */
    @Override
    public byte[] getData() {
        if (savedData == null) {
            savedData = getValue(null);
        }
        return savedData;
    }

    /* (non-Javadoc)
     * @see edu.cmu.cs.diamond.opendiamond.IResult#getValue(java.lang.String)
     */
    @Override
    public byte[] getValue(String key) {
        byte result[];
        long lenp[] = { 0 };

        SWIGTYPE_p_p_unsigned_char data = OpenDiamond.create_data_cookie();
        try {
            data = OpenDiamond.create_data_cookie();

            if (key == null) {
                OpenDiamond.lf_next_block(obj_handle, Integer.MAX_VALUE, lenp,
                        data);
            } else {
                if (OpenDiamond.lf_ref_attr(obj_handle, key, lenp, data) != 0) {
                    // no such key
                    return null;
                }
            }

            // copy
            result = extractData((int) lenp[0], data);
        } finally {
            OpenDiamond.delete_data_cookie(data);
        }
        return result;
    }

    private byte[] extractData(int len, SWIGTYPE_p_p_unsigned_char data) {
        byte[] result;
        result = new byte[len];

        byteArray d = OpenDiamond.deref_data_cookie(data);
        // XXX slow
        for (int i = 0; i < result.length; i++) {
            result[i] = (byte) d.getitem(i);
        }
        return result;
    }

    /* (non-Javadoc)
     * @see edu.cmu.cs.diamond.opendiamond.IResult#getKeys()
     */
    @Override
    public List<String> getKeys() {
        List<String> result = new ArrayList<String>();

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
            int err = OpenDiamond.lf_first_attr(obj_handle, name, len, data,
                    cookie);
            while (err == 0) {
                result.add(OpenDiamond.deref_char_cookie(name));
                err = OpenDiamond.lf_next_attr(obj_handle, name, len, data,
                        cookie);
            }
        } finally {
            OpenDiamond.delete_void_cookie(cookie);
            OpenDiamond.delete_char_cookie(name);
            OpenDiamond.delete_data_cookie(data);
        }

        return result;
    }

    @Override
    protected void finalize() throws Throwable {
        OpenDiamond.ls_release_object(null, obj_handle);
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("Result [");

        for (String name : getKeys()) {
            sb.append(" '" + name + "'");
            if (name.endsWith(".int")) {
                sb.append(":" + Util.extractInt(getValue(name)));
            } else if (name.endsWith("-Name")) {
                sb.append(":'" + new String(getValue(name)) + "'");
            } else if (name.endsWith(".time")) {
                sb.append(":" + Util.extractLong(getValue(name)));
            }
        }
        sb.append(" ]");
        return sb.toString();
    }

    public static Result getEmptyResult() {
        // TODO Auto-generated method stub
        return null;
    }
}
