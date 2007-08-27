<?php
/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2007 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */
/*  $Id:$  */


    function simple_header($G)
    {
        $style = $G['WEBROOT'] . "/css/gatekeeper_main.css";
        $pagetitle = $G['PAGE_TITLE'];

        $h = "<html><head>\n";
        $h .= "<title>$pagetitle</title>\n";
        $h .= "<link rel=\"stylesheet\" type=\"text/css\" name=\"ISADS\"";
        $h .= " href=\"$style\">\n";
        $h .= "<link rel=\"shortcut icon\" href=\"images/favicon.ico\">\n";
        $h .= "<meta http-equiv=\"Content-Type\" content=\"text/html; ";
        $h .= "charset=iso-8859-1\">\n";
        $h .= "<meta name=description content=\"ISADS\">\n";
        $h .= "</head>\n";
        //$h .= "<script language=\"JavaScript\" ";
        //$h .= "src=\"{$G['WEBROOT']}/js/utils.js\"> \n";
        $h .= "</script>\n";


        return $h;

    }

    function simple_footer ($G) {
        $webroot = $G['WEBROOT'];
        $imagelib = $G['IMAGE_LIB'];
        $f = "";
        $f .= "<br><br><br><br>";
        $f .= "<hr>";
        $f .= "<center>";

        $f .= "<table class=footer>";
        $f .= "    <tr>";
        for ($i = 0; $i < count($G['LOGO']); $i++) {
            $val = explode ("|", $G['LOGO'][$i]);
            $imagename = $val[0];
            $url = $val[1];
        $f .= "        <td>\n";
        $f .= "        <a href=$url>\n";
        $f .= "            <img src=$imagelib/$imagename>\n";
        $f .= "        </a>\n";
        $f .= "        </td>\n";
        }
        $f .= "    </tr>";
        $f .= "</table>";




        return $f;
    }
?>
