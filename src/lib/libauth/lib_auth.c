/*
 *      Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2006, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * authentication and encryption using Kerberos
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>

#include <krb5.h>
#include <et/com_err.h>

#include "lib_log.h"
#include "lib_auth.h"
#include "lib_auth_priv.h"

static char const cvsid[] ="$Header$";

auth_handle_t auth_conn_server(int sockfd) 
{
    krb5_error_code retval;
    krb5_ticket *ticket;
    krb5_principal server;
    char *cname;
    char *service = DIAMOND_SERVICE;
    krb5_keytab keytab = NULL;	/* Allow specification on command line */
    auth_context_t *k;
 	
 	k = (auth_context_t *) malloc(sizeof(auth_context_t));
	assert(k != NULL);

	retval = krb5_init_context(&k->context);
    if (retval) {
    	log_message(LOGT_NET, LOGL_ERR, 
    				"Error %d while initializing krb5", retval);
	    free(k);
	    return (NULL);
    }

    retval = krb5_sname_to_principal(k->context, NULL, service, 
				     KRB5_NT_SRV_HST, &server);
    if (retval) {
		log_message(LOGT_NET, LOGL_ERR, 
					"Error while generating service name (%s): %s\n",
	       			service, error_message(retval));
	    krb5_free_context(k->context);
	    free(k);
		return(NULL);
    }
    
 	retval = krb5_auth_con_init(k->context, &k->auth_context);
    if (retval) {
	    log_message(LOGT_NET, LOGL_ERR, 
	    			"Error %d while initializing auth context", 	
	    			retval);
	    krb5_free_principal(k->context, server);
	    krb5_free_context(k->context);	  
	    free(k);
	    return (NULL);
    }
    
    retval = krb5_recvauth(k->context, &k->auth_context, 
    		   (krb5_pointer)&sockfd,
			   DIAMOND_VERSION, server, 
			   0,	/* no flags */
			   keytab,	/* default keytab is NULL */
			   &ticket);

   	krb5_free_principal(k->context, server);	/* done using it */

   	if (retval) {
		log_message(LOGT_NET, LOGL_ERR, 
					"recvauth failed--error %s\n", 
					error_message(retval));
		krb5_auth_con_free(k->context, k->auth_context);
		krb5_free_context(k->context);
		free(k);
		return(NULL);
    }
  
    /* Get client name */
    retval = krb5_unparse_name(k->context, tkt_client(ticket), &cname);
    if (retval){
		log_message(LOGT_NET, LOGL_ERR, 
					"Error unparsing principal name: %s\n", 
					error_message(retval));
    } else {
  		log_message(LOGT_NET, LOGL_INFO, 
 					"Authenticated %s to %s service\n", 	
 					cname, service);
 		free(cname);
    }
    krb5_free_ticket(k->context, ticket);	/* done using it */
    
    /* set auth context for encrypted messaging */
    retval = krb5_auth_con_genaddrs(k->context, k->auth_context, sockfd,
			     KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR |
			     KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR);
  	if (retval) {
      	log_message(LOGT_NET, LOGL_ERR, 
				"Error generating addrs for auth_context: %s\n",
      			error_message(retval));
      	krb5_auth_con_free(k->context, k->auth_context);
        krb5_free_context(k->context);
        free(k);
      	return(NULL);
    }

    retval = krb5_auth_con_setflags(k->context, k->auth_context,
				      KRB5_AUTH_CONTEXT_DO_SEQUENCE);
	if (retval) {
	 	log_message(LOGT_NET, LOGL_ERR, 
				"Error setting auth context flags: %s\n", 
	 			error_message(retval));
	 	krb5_auth_con_free(k->context, k->auth_context);
        krb5_free_context(k->context);
        free(k);
		return(NULL);
	}

    return ((auth_handle_t) k);
}


auth_handle_t auth_conn_client_ext(int sockfd, char *service) 
{
    krb5_data cksum_data;
    krb5_error_code retval;
    krb5_ccache ccdef;
    krb5_principal client, server;
    krb5_error *err_ret = NULL;
 	char            node_name[128];
 	char		   *clientstr = NULL;
 	struct hostent *hent;
 	int len;
 	struct sockaddr_in addr;
	unsigned int addr_len;
	auth_context_t *k;
	
	addr_len = sizeof(addr);
	retval = getpeername(sockfd, &addr, &addr_len);
	if (retval) {
 	    log_message(LOGT_NET, LOGL_ERR, 
					"Error %d while calling getpeername", retval);
	    return(NULL);
	}
 
    signal(SIGPIPE, SIG_IGN);
 
    hent = gethostbyaddr(&addr.sin_addr, sizeof(addr.sin_addr), AF_INET);
	if (hent == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
					"failed to get hostname for %s\n",
					inet_ntoa(addr.sin_addr));
		return(NULL);
	} else {
		len = strlen(hent->h_name);
		strncpy(node_name, hent->h_name, len);
		node_name[len] = 0;
	}

 	k = (auth_context_t *) malloc(sizeof(auth_context_t));
	assert(k != NULL);
 
    retval = krb5_init_context(&k->context);
    if (retval) {
	    log_message(LOGT_NET, LOGL_ERR,
	    			"Error while initializing krb5: %s", 
	    			error_message(retval));
	    free(k);
	    return(NULL);
    }
    
	log_message(LOGT_NET, LOGL_INFO,
				"creating service principal %s at %s\n", 
				service, node_name);
    
    retval = krb5_sname_to_principal(k->context, node_name, service,
				     KRB5_NT_SRV_HST, &server);
    if (retval) {
		log_message(LOGT_NET, LOGL_ERR,
					"Error %d while creating service principal for %s at %s",
					retval, service, node_name);
	    krb5_free_context(k->context);
        free(k);
		return(NULL);
    }

    cksum_data.data = node_name;
    cksum_data.length = strlen(node_name);

    retval = krb5_cc_default(k->context, &ccdef);
    if (retval) {
		log_message(LOGT_NET, LOGL_ERR,
					"Error while getting default ccache--%s", 
					error_message(retval));
		krb5_free_principal(k->context, server);
		krb5_free_context(k->context);
        free(k);
		return(NULL);
    }

    retval = krb5_cc_get_principal(k->context, ccdef, &client);
    if (retval) {
		log_message(LOGT_NET, LOGL_ERR,
					"Error while getting client principal name: %s", 
					error_message(retval));
		krb5_cc_close(k->context, ccdef);
		krb5_free_principal(k->context, server);
		krb5_free_context(k->context);
        free(k);
		return(NULL);
    }
    
    retval = krb5_unparse_name(k->context, client, &clientstr);
    if (retval) {
		log_message(LOGT_NET, LOGL_ERR,
					"Error while unparsing client principal name: %s", 
					error_message(retval));
		krb5_cc_close(k->context, ccdef);
		krb5_free_principal(k->context, server);
		krb5_free_context(k->context);
        free(k);
		return(NULL);
    } else {
	    log_message(LOGT_NET, LOGL_INFO,
	    			"Sending authorization for %s", 
	    			clientstr);
    }

    retval = krb5_auth_con_init(k->context, &k->auth_context);
    if (retval) {
	    log_message(LOGT_NET, LOGL_ERR,
	    			"Error while initializing auth context: %s\n", 
	    			error_message(retval));
	    krb5_free_principal(k->context, client);
	    krb5_cc_close(k->context, ccdef);
	    krb5_free_principal(k->context, server);
	    krb5_free_context(k->context);
        free(k);
	    return(NULL);
    }
 
    retval = krb5_sendauth(k->context, &k->auth_context, 
    		   (krb5_pointer) &sockfd,
			   DIAMOND_VERSION, client, server,
			   0,
			   &cksum_data,
			   0,		/* no creds, use ccache instead */
			   ccdef, &err_ret, NULL, NULL);

    krb5_free_principal(k->context, server);	/* finished using it */
    krb5_free_principal(k->context, client);      
    krb5_cc_close(k->context, ccdef);

    signal(SIGPIPE, SIG_DFL);
    if (retval && retval != KRB5_SENDAUTH_REJECTED) {
		log_message(LOGT_NET, LOGL_ERR, 
					"Error from sendauth: %s\n", 
					error_message(retval));
		if (err_ret) krb5_free_error(k->context, err_ret);
		krb5_auth_con_free(k->context, k->auth_context);
		krb5_free_context(k->context);
        free(k);
		return(NULL);
    }
    if (retval == KRB5_SENDAUTH_REJECTED) {
		/* got an error */
		log_message(LOGT_NET, LOGL_ERR,
					"sendauth rejected, error reply is:\n\t\"%*s\"\n",
					err_length(err_ret), err_text(err_ret));
	    krb5_free_error(k->context, err_ret);
	    krb5_auth_con_free(k->context, k->auth_context);
	    krb5_free_context(k->context);
        free(k);
	    return(NULL);
    }
    
    /* set auth context for encrypted messaging */
 	retval = krb5_auth_con_genaddrs(k->context, k->auth_context, sockfd,
			     KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR |
			     KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR);
  	if (retval) {
      	log_message(LOGT_NET, LOGL_ERR,
      				"Error generating addrs for auth_context: %s\n",
      				error_message(retval));
      	krb5_auth_con_free(k->context, k->auth_context);
      	krb5_free_context(k->context);
        free(k);
      	return(NULL);
    }

    retval = krb5_auth_con_setflags(k->context, k->auth_context,
				      KRB5_AUTH_CONTEXT_DO_SEQUENCE);
	if (retval) {
	 	log_message(LOGT_NET, LOGL_ERR,
	 				"Failed encryption settings: %s\n", 
	 				error_message(retval));
	 	krb5_auth_con_free(k->context, k->auth_context);
	 	krb5_free_context(k->context);
        free(k);
		return(NULL);
	}
 	
	return ((auth_handle_t) k);
}

auth_handle_t auth_conn_client(int sockfd) {
	return auth_conn_client_ext(sockfd, DIAMOND_SERVICE);
}

/*
 * wrappers for message encryption and decryption
 */
int auth_msg_encrypt(auth_handle_t handle, char *inbuf, int ilen, 
					 char *outbuf, int olen) 
{
	krb5_data clear;
	krb5_data coded;
	krb5_replay_data rdata;
	krb5_error_code retval;
	auth_context_t *c;
	int len;
	
	c = (auth_context_t *) handle;
	clear.length = ilen;
    clear.data = inbuf;
    
  	retval = krb5_mk_priv(c->context, c->auth_context, &clear,
  						  &coded, &rdata);
	if (retval) {
	 	log_message(LOGT_NET, LOGL_ERR,
	 				"Failed encrypting reply: %s\n", 
	 				error_message(retval));
		return(-1);
	} 
	memcpy(outbuf, coded.data, coded.length);
	len = coded.length;
	krb5_free_data_contents(c->context, &coded);
	
	return(len);
}

int auth_msg_decrypt(auth_handle_t handle, char *inbuf, int ilen, 
					char *outbuf, int olen) 
{
	krb5_data clear;
	krb5_data coded;
	krb5_replay_data rdata;
	krb5_error_code retval;
	auth_context_t *c;
	int len;
	
	c = (auth_context_t *) handle;	
    coded.length = ilen;
    coded.data = inbuf;
    
 	retval = krb5_rd_priv(c->context, c->auth_context, &coded,
						  &clear, &rdata);
	if (retval) {
	 	log_message(LOGT_NET, LOGL_ERR,
	 				"Failed decrypting reply: %s\n",
	 				error_message(retval));
		return(-1);
	} 

	memcpy(outbuf, clear.data, clear.length);
	len = clear.length;
	krb5_free_data_contents(c->context, &clear);

	return(len);
}

