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

    include ("include/framing.php");
    $header = simple_header($G);
    $footer = simple_footer($G);

    $a = new admin($G);

    $is_admin = $a->is_admin($me);
    //  Get POST vars

    $action = (isset($_GET['action'])) ?  $_GET['action'] : "front_page";
    $collection = (isset($_GET['collection'])) ?  $_GET['collection'] : "";
    $username = (isset($_GET['username'])) ?  $_GET['username'] : "";
    $add_col = (isset($_GET['add_col'])) ?  $_GET['add_col'] : "";
    $scope = (isset($_POST['scope'])) ?  $_POST['scope'] : "";


    //  Default setting 
    if ($action == "front_page") {
        $main_content = "html/scope.html";
    }
    //  Get listing of all users
    $all_users = $a->list_users();
    //  Add the user to the selected collections
    if ($action == "add_user") {
        $col_array = $a->get_collections();
	$col_list = array_keys($col_array);
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

        $col_array = $a->get_collections();
        $col_list = array_keys($col_array);

        $main_content = "html/scope.html";

        if (($scope != "") && ($me != "")) {

	    $content = "1\n";
	    $gids = 0;


            $content .= "collection ";

            foreach ($scope as $coll) {
                $namemap_info = $a->get_namemap_entry($col_array[$coll], $coll);
                $content .= $namemap_info['groupid'] . " ";
		$gids = $gids + 1; 
            }

	    $content .= "\n" . $gids . "\n";

            foreach ($scope as $coll) {
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
