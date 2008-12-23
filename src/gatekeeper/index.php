<?php
/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
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

    //  Check if we are using https
    if ((!isset($_SERVER['HTTPS'])) || $_SERVER["HTTPS"] != "on") {
	$bname = dirname($_SERVER['SCRIPT_NAME']);
	$secure_url = "https://" . $_SERVER["HTTP_HOST"] .  $bname;
	header("Location: $secure_url");
	exit;
    }
    include ("gatekeeper.conf");
    $G = init_globals();

    include ("php/functions.php");

    //  Get current logged in user
    if($G['AUTH_METHOD'] == "webiso") {
        $me = $_SERVER['REMOTE_USER'];
    }
    else if($G['AUTH_METHOD'] == "htaccess") {
        $me = $_SERVER['PHP_AUTH_USER'];
    }


    //  Layout the page
    $left_menu = "html/left_menu.html";
    $main_content = "html/scope.html";

    $a = new admin($G);

    $is_admin = $a->is_admin($me);
    //  Get POST vars

    $action = (isset($_GET['action'])) ?  $_GET['action'] : "front_page";
    $username = (isset($_POST['username'])) ?  $_POST['username'] : "";
    $collections = (isset($_POST['coll'])) ?  $_POST['coll'] : "";

    // Update set of accessible collections for a user
    if ($action == "manage" && $is_admin == 1) {
	if ($username) {
	    $a->manage_user($username, $collections);
	    if (!$collections) { $username = ""; }
	}
	$main_content = "html/manage.html";
    }

    //  Define the scope
    if ($action == "define_scope") {

	$col_array = $a->get_collections();
	$col_list = array_keys($col_array);

	$main_content = "html/scope.html";

	if ($collections && ($me != "")) {

	    $content = "1\n";
	    $gids = 0;

	    $content .= "collection ";

	    foreach ($collections as $coll) {
		$namemap_info = $a->get_namemap_entry($col_array[$coll], $coll);
		$content .= $namemap_info['groupid'] . " ";
		$gids = $gids + 1;
	    }

	    $content .= "\n" . $gids . "\n";

	    foreach ($collections as $coll) {
		$dbfile = $col_array[(string)$coll];
		$namemap_info = $a->get_namemap_entry($dbfile, $coll);
		$gid = $namemap_info['groupid'];
		$servers = $a->get_gidmap_entry($dbfile, $coll, $gid);
		$content .= $gid . " ";
		$content .= $servers . "\n";
	    }
	    file_put_contents ("testfile.txt", $content);
	    $filename = "diamond_config_scope";

	    header('Content-type: application/x-diamond-scope');
	    header('Content-Disposition: attachment; filename="'.$filename.'"');
	    echo $content;
	    exit;
	}
    }
    include ("html/page_layout.html");
?>
