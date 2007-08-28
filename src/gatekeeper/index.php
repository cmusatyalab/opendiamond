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

    //  Check if we are using https
    if ((!isset($_SERVER['HTTPS'])) || $_SERVER["HTTPS"] != "on") {
        $bname = dirname($_SERVER['SCRIPT_NAME']);
        $secure_url = "https://" . $_SERVER["HTTP_HOST"] .  $bname;
        header("Location: $secure_url");
        exit;
    }
    //  Get current logged in user;
    $me = (isset($_SERVER['PHP_AUTH_USER'])) ?  $_SERVER['PHP_AUTH_USER'] : NULL;

    include ("gatekeeper.conf");
    $G = init_globals();

    include ("php/functions.php");

    //  Layout the page
    $left_menu = "html/left_menu.html";
    $main_content = "html/front_page.html";

    include ("include/framing.php");
    $header = simple_header($G);
    $footer = simple_footer($G);

    $a = new admin($G);

    $col_list = $a->get_collections();

    $is_admin = $a->is_admin($me);
    //  Get POST vars

    $action = (isset($_GET['action'])) ?  $_GET['action'] : "front_page";
    $collection = (isset($_GET['collection'])) ?  $_GET['collection'] : "";
    $username = (isset($_GET['username'])) ?  $_GET['username'] : "";
    $add_col = (isset($_GET['add_col'])) ?  $_GET['add_col'] : "";
    $scope = (isset($_POST['scope'])) ?  $_POST['scope'] : "";


    //  Default setting 
    if ($action == "front_page") {
        $main_content = "html/front_page.html";
    }
    //  Get listing of all users
    $all_users = $a->list_users();
    //  Add the user to the selected collections
    if ($action == "add_user") {
        $main_content = "html/add_user.html";
        if (($add_col != "") && ($username != "")) {
            //  Print the add user admin stuff
            $a->add_user($username, $add_col);
            print_summary();
            //include ("add_user_to_group.html");
        }
    }

    //  Define the scope 
    if ($action == "define_scope") {
        $main_content = "html/scope.html";
        if (($scope != "") && ($me != "")) {

	    $content = sizeof($scope) . "\n";
	    $gids = 0;

            foreach ($scope as $coll) {
                $namemap_info = $a->get_namemap_entry($coll);
                $content .= $namemap_info['collection'] . " ";
                $content .= $namemap_info['groupid'] . "\n";
		$gids = $gids + 1; 
            }

	    $content .= $gids . "\n";

            foreach ($scope as $coll) {
                $namemap_info = $a->get_namemap_entry($coll);
		$gid = $namemap_info['groupid'];
                $gidmap_info = $a->get_gidmap_entry($coll, $gid);
                $content .= $gid . " ";
		$servers = $gidmap_info['server'];
		$content .= $servers . " ";
                $content .= "\n";
            }
            file_put_contents ("testfile.txt", $content);
            $filename = "diamond_config_scope";

            header('Content-type: application/x-diamond-scope');
            header('Content-Disposition: attachment; filename="'.$filename.'"');
            echo $content;
            exit; 
        }
    }

    //  Check if user is admin
    if ($is_admin == 1) {
        //  Delete a user
        if ($action == "delete_user") {
            $main_content = "html/mod_user.html";
            if ($username != "") {
                $a->delete_user($username);
                print_summary();
            }
        }
        //  Delete a user from a collection
        if ($action == "delete_user_from_collection") {
            $main_content = "html/mod_user.html";
            if (($collection != "") && ($username != "")) {
                $a->delete_user($username, $collection);
                print_summary();
            }
        }
        //  Print out the summary  
        if ($action == "summary") {
            //  List the users;
            //$member_list = $a->member_of("rgass");
            $main_content = "html/summary.html";
        }

    }

    include ("html/page_layout.html");


    function print_summary() {
        $bname = dirname($_SERVER['SCRIPT_NAME']);
        $secure_url = "https://" . $_SERVER["HTTP_HOST"] .  $bname;
        header("Location: $secure_url?action=summary");
        exit;
    }

?>
