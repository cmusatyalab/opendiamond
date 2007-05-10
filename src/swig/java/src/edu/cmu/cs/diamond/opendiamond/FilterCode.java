package edu.cmu.cs.diamond.opendiamond;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class FilterCode {
    private byte[] code;

    public FilterCode(byte code[]) {
        this.code = new byte[code.length];
        System.arraycopy(code, 0, this.code, 0, code.length);
    }

    public FilterCode(InputStream in) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();

        byte bb[] = new byte[4096];
        
        int amount;
        while((amount = in.read(bb)) != -1) {
            out.write(bb, 0, amount);
        }

        code = out.toByteArray();
    }
}
