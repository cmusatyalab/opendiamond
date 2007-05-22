package edu.cmu.cs.diamond.opendiamond;

import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;

public class Util {
    private Util() {
    }

    // XXX endian specific
    public static int extractInt(byte[] value) {
        return (value[3] & 0xFF) << 24 | (value[2] & 0xFF) << 16
                | (value[1] & 0xFF) << 8 | (value[0] & 0xFF);
    }

    public static long extractLong(byte[] value) {
        return ((long) (value[7] & 0xFF) << 56)
                | ((long) (value[6] & 0xFF) << 48)
                | ((long) (value[5] & 0xFF) << 40)
                | ((long) (value[4] & 0xFF) << 32)
                | ((long) (value[3] & 0xFF) << 24)
                | ((long) (value[2] & 0xFF) << 16)
                | ((long) (value[1] & 0xFF) << 8) | (value[0] & 0xFF);
    }

    public static double extractDouble(byte[] value) {
        return Double.longBitsToDouble(extractLong(value));
    }

    public static BufferedImage possiblyShrinkImage(BufferedImage img,
            int maxW, int maxH) {
        int h = img.getHeight(null);
        int w = img.getWidth(null);
    
        double scale = 1.0;
        
        double imgAspect = (double) w / h;
        double targetAspect = (double) maxW / maxH;

        if (imgAspect > targetAspect) {
            // more wide
            if (w > maxW) {
                scale = (double) maxW / w;
            }
        } else {
            // more tall
            if (h > maxH) {
                scale = (double) maxH / h;
            }
        }
    
        if (scale == 1.0) {
            return img;
        } else {
            BufferedImage newI = new BufferedImage((int) (w * scale),
                    (int) (h * scale), img.getType());
    
            Graphics2D g = newI.createGraphics();
            g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,
                    RenderingHints.VALUE_INTERPOLATION_BILINEAR);
            g.scale(scale, scale);
            g.drawImage(img, 0, 0, null);
            g.dispose();
    
            return newI;
        }
    }
}
