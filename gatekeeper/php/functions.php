<?
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
	    $auth_array = file ($this->auth_file);
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
	$dir_handle = opendir($this->db_dir);
	$collections = array();

	$dirents = $this->list_dir($dir_handle);
	foreach ($dirents as $entry) {
	    $query = "select distinct collection from metadata;";

	    $db_file = $this->db_dir . '/' . $entry;
	    $db_handle =  sqlite3_open($db_file);
	    $query_handle = sqlite3_query($db_handle, $query);
	    if($query_handle == FALSE) {
		echo "sqlite3 error opening $db_file: " . sqlite3_error($db_handle);
		echo "<br />";
		sqlite3_close($db_handle);
		continue;
	    }

	    while($row = sqlite3_fetch_array($query_handle)) {
		$keys = array_keys($row);
		$coll = $row['collection'];
		$collections[$coll] = $db_file;
	    }
	}

	return $collections;
    }


    /**
     *  Get the name to group ID info from the db files
     *  @param collection
     *  @return array with name_map info
     */
    function get_namemap_entry ($db_file, $collection) {
      $query = "select distinct collection,groupid from metadata where collection = '$collection';";

      $db_handle =  sqlite3_open($db_file);
      $query_handle = sqlite3_query($db_handle, $query);
      $result = sqlite3_fetch_array ($query_handle);

      return $result;
    }


    /**
     *  Get the group ID to server info from the db files
     *  @param collection
     *  @param groupid
     *  @return array with gid_map info
     */
    function get_gidmap_entry ($db_file, $collection, $groupid) {
      $query = "select distinct server from metadata where groupid='$groupid';";

      $db_handle = sqlite3_open($db_file);
      $query_handle = sqlite3_query($db_handle, $query);

      $result = array();
      while($myarray = sqlite3_fetch_array ($query_handle)) {
	  $result[] = $myarray['server'];
      }

      return $result;
    }


    /**
     *  Read ACL file
     *  @return array with key=users and value=array of collections
     */
    function read_acl_file () {
	$acl_file = file($this->acl_file);

	//  Read acl file and populate users array
	$users = array();
	for ($i = 0; $i < count($acl_file); $i++) {
	    $val = explode ("|", $acl_file[$i]);
	    $collection = $val[0];
	    $names = trim($val[1]);
	    if ($names) {
		$names = explode (",", $names);
		foreach($names as $user) {
		    if (!array_key_exists($user, $users)) {
			$users[$user] = array();
		    }
		    $users[$user][] = $collection;
		}
	    }
	}
	return $users;
    }


    /**
     *  Write ACL file
     *  @param array with key=users and value=array of collections
     */
    function write_acl_file($users) {
	/* map collections-by-user to users-by-collection */
	$collections = array();
	foreach ($users as $user => $cols) {
	    foreach ($cols as $col) {
		if (!array_key_exists($col, $collections)) {
		    $collections[$col] = array();
		}
		$collections[$col][] = $user;
	    }
	}
	/*  Write the file out */
	$contents = "";
	foreach($collections as $col => $users) {
	    $users = array_unique($users);
	    $contents .= $col . '|' . implode(",", $users) . "\n";
	}
	file_put_contents($this->acl_file, $contents);
    }

    /**
     *  Manage which collections a user can see
     *  @param username
     *  @param list of collections
     *  @return
     */
    function manage_user ($user, $collections) {
	$users = $this->read_acl_file();

	/* replace the set of collections for the specified user */
	if (!$collections) $collections = array();
	$users[$user] = $collections;

	$this->write_acl_file($users);
    }


    /**
     *  list_users
     *  @return array with users
     */
    function list_users () {
	$users = $this->read_acl_file();
	return array_keys($users);
    }


    /**
     *  is_member
     *  @param username
     *  @return 0/1
     */
    function is_member ($username, $collection) {
	$users = $this->read_acl_file();
	return in_array($collection, $users[$username]);
    }


    /**
     *  member_of
     *  Check to see if a user is a member of a collection
     *  @username  username
     *  @return array of collections user is a member of
     */
    function member_of ($username) {
	$users = $this->read_acl_file();
	return $users[$username];
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
