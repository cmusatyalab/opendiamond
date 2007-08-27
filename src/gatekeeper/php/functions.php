<?
/*
 *  OpenDiamond Gatekeeper
 *  An OpenDiamond application the generation of scoping files
 *
 *  Copyright (c) 2007  Intel Corporation
 *  All Rights Reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */
/*  $Id: functions.php 287 2007-08-22 19:09:34Z rgass $  */
include ("compat.php");

class admin {
    var $db_dir;
    var $acl_file;

    function admin ($G) {
        $this->db_dir = $G['ABSROOT'] . "/" . $G['DB_DIR'];
        $this->acl_file = $G['ABSROOT'] . "/" . $G['ACL_FILE'];
        $this->auth_method = $G['AUTH_METHOD'];
        $this->auth_file = $G['AUTH_FILE'];
    }
 
    /**  
     *  authenticate
     *  @param username  Username that is being checked
     *  return 1 on success, 0 on fail
     */
    function is_admin($username) {
        if ($this->auth_method = "htaccess") {
            $auth_array = file ($this->auth_file);
        }
        if ($this->auth_method = "webiso") {
        }

        #if (in_array($username, $auth_array)) {
        $isauth = 0;
        for ($i = 0; $i < count($auth_array); $i++) {
            if (trim($username) == trim($auth_array[$i])) {
                $isauth = 1;
                break;
            }
        }
        return $isauth;
    }

    /**  
     *  Get the list of collections
     *  @return list of collection array
     */
    function get_collections () {
        $handle = opendir($this->db_dir);
        $collection = array();

        $results = $this->list_dir($handle);        
        foreach ($results as $val) {
            $n1 = explode("-", $val);
            $n = explode(".", $n1[1]);
            array_push($collection, $n[0]);
        }
        return $collection;
    }

    /**
     *  Get the Scope info from the db files
     *  @param collection
     *  @return array with scope info
     */
    function get_scope ($collection) {
        $query = "select * from metadata where collection = '$collection';";
        $collection_file = $this->db_dir . "/metadata-" . $collection . ".db";
       $handle =  sqlite3_open($collection_file);
       $result = sqlite3_query($handle, $query);

       $myarray = sqlite3_fetch_array ($result);
       return $myarray;
    }

    /**  
     *  Add a user
     *  @param  username to add
     *  @param  collection 
     *  @return 
     */
    function add_user ($username, $collection) {
        $acl_file = file($this->acl_file);
        $cur_array = array();
        //  Read acl file and put in cur_array
        for ($i = 0; $i < count($acl_file); $i++) {
            $val = explode ("|", $acl_file[$i]);
            $c = $val[0];
            $names = trim ($val[1]);

            if ($names != "") {
                $cur_array[$c] = $names;
            }
        }
        foreach($collection as $col) {
            if (!(array_key_exists($col, $cur_array))) {
                $cur_array[$col] = $username;
            } else {
                $cur_array[$col] = $cur_array[$col] . "," . $username;
            }
        }
        //  If there is a new db added 
        //  Write the file out
        $contents = "";
        foreach($cur_array as $col => $users) {
            $vals = explode (",", $users);
            $new_vals = array_unique($vals);
            $contents .= $col . "|";
            for ($i = 0; $i < count($new_vals); $i++) {
                $contents .= $new_vals[$i];
                if ($i < count($new_vals)-1) {
                    $contents .= ",";
                }
            }
            $contents .= "\n";
        }
        file_put_contents($this->acl_file, $contents);
    }

    /**
     *  list_users
     *  @return array with users
     */
    function list_users () {
        $acl_file = file($this->acl_file);
        $all_users = array();
        for ($i = 0; $i < count($acl_file); $i++) {
            $val = explode ("|", $acl_file[$i]);
            $collection = $val[0];
            $names = trim ($val[1]);
            $names_array = explode (",", $names);
            foreach($names_array as $key => $value) {
                array_push ($all_users, $value);
            }
             
        }
        return array_unique($all_users);
    }
    /**
     *  delete_users
     *  @param username
     *  @param collection Optional.  If specified, it only deletes a user from 
     *  a single collection
     */
    function delete_user ($username, $collection=NULL) {
        $cur_array = array();
        $acl_file = file($this->acl_file);
        for ($i = 0; $i < count($acl_file); $i++) {
            $val = explode ("|", $acl_file[$i]);
            $c = $val[0];
            $names = trim ($val[1]);

            if ($names != "") {
                $cur_array[$c] = $names;
            }
        }

        if ($collection != NULL) {
            $val = explode (",", $cur_array[$collection]);
            $tmp = "";
            foreach($val as $user) {
                if ($user != $username) {
                    $tmp .= $user . " ";
                }
            }
            $newval = ereg_replace(" ", ",", trim($tmp));
            $cur_array[$collection] = $newval;
        }


        $new_array = array();
        foreach($cur_array as $col => $users) {
            $val = explode (",", $users);
            for ($i = 0; $i < count($val); $i++) {
                if ($collection == NULL) {
                    if ($username != $val[$i]){
                        $new_array[$col][$i] = $val[$i];
                    }
                }else {
                    $new_array[$col][$i] = $val[$i];
                }
            }
        }
        file_put_contents($this->acl_file, $this->generate_acl($new_array));
    }

    /**
     *  delete_user_from_collection
     *  @param username
     *  @param collection name 
     */
    function delete_user_from_collection ($username, $collection) {
        $cur_array = array();
        $acl_file = file($this->acl_file);
        for ($i = 0; $i < count($acl_file); $i++) {
            $val = explode ("|", $acl_file[$i]);
            $c = $val[0];
            $names = trim ($val[1]);

            if ($names != "") {
                $cur_array[$c] = $names;
            }
        }
        $val = explode (",", $cur_array[$collection]);
        $tmp = "";
        foreach($val as $user) {
            if ($user != $username) {
                $tmp .= $user . " ";
            }
        }
        $newval = ereg_replace(" ", ",", trim($tmp));
        $cur_array[$collection] = $newval;

        //  Do this so I can call my function
        $new_array = array();
        foreach($cur_array as $col => $users) {
            $val = explode (",", $users);
            for ($i = 0; $i < count($val); $i++) {
                $new_array[$col][$i] = $val[$i];
            }
        }
        file_put_contents($this->acl_file, $this->generate_acl($new_array));

        
        

    } 
    /**
     *  is_member
     *  @param username
     *  @return 0/1
     */ 
    function is_member ($username, $collection) {
    }

    /**
     *  generate_acl
     *  @param 2D array 
     *  [collection] => Array ([0] => username); 
     *  @return content string to be written
     */
    function generate_acl($my_array) {
        $content = "";
        foreach($my_array as $col => $users) {
            $content .= $col . "|";
            $i = 0;
            foreach($users as $name) {
                $content .= $name;
                if ($i < count($users)-1) {
                    $content .= ",";
                }
                $i++;
            }
            $content .= "\n";
        }
        return $content;
    }

    /**
     *  member_of
     *  Check to see if a user is a member of a collection
     *  @username  username
     *  @return array of collections user is a member of 
     */
    function member_of ($username) {
        $member = array();
        $acl_file = file($this->acl_file);
        for ($i = 0; $i < count($acl_file); $i++) {
            $val = explode ("|", $acl_file[$i]);
            $collection = $val[0];
            $names = trim ($val[1]);
            if (strstr($names, $username)) {
                array_push ($member, $collection);
            }
        }
        return $member;
    }

    /**
     *  function to list the contents of a directory
     *  @param  opendir handle
     *  @return directory contents array
     */
    function list_dir ($handle)
    {
        $contents = array();
        $x = 0;
        while (false !== ($filez = readdir($handle))) {
           if ($filez!= "." && $filez!= ".."
               && ereg (".*\.[Dd][Bb]$", $filez)) {
                array_push ($contents , $filez);
            }
        }
        return $contents;
    }

}
?>
