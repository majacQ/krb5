/*
 * $Source$
 * $Author$
 *
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 */

/* 
 * Sandia National Laboratories also makes no representations about the 
 * suitability of the modifications, or additions to this software for 
 * any purpose.  It is provided "as is" without express or implied warranty.
 */

#if !defined(lint) && !defined(SABER)
static char rcsid_kadmin_mod[] =
    "$Header$";
#endif	/* lint */

/*
 * kadmin_mod
 * Perform Remote Kerberos Administrative Functions
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#ifndef __convex__
#include <strings.h>
#endif
#include <com_err.h>

#include <krb5/adm_defs.h>

#include <krb5/krb5.h>
#include <krb5/ext-proto.h>
#include <krb5/los-proto.h>
#include <krb5/kdb.h>
#include <krb5/kdb_dbm.h>

krb5_error_code
kadm_mod_user(my_creds, rep_ret, local_addr, foreign_addr, 
	      local_socket, seqno, principal)
krb5_creds *my_creds;
krb5_ap_rep_enc_part *rep_ret;
krb5_address *local_addr, *foreign_addr;
int *local_socket;
krb5_int32 *seqno;
char *principal;
{
    krb5_data msg_data, inbuf;
    kadmin_requests rd_priv_resp;
    char username[755];
    int count;
    krb5_error_code retval;     /* return code */

    if ((inbuf.data = (char *) calloc(1, 3 + sizeof(username))) == (char *) 0) {        fprintf(stderr, "No memory for command!\n");
        exit(1);
    }

    inbuf.data[0] = KADMIN;
    inbuf.data[1] = MODOPER;
    inbuf.data[2] = SENDDATA2;

    if (principal && principal[0] != '\0')
	strcpy(username, principal);
    else {
	count = 0;
	do {
	    fprintf(stdout, "\nName of Principal to be Modified: ");
	    fgets(username, sizeof(username), stdin);
	    if (username[0] == '\n')
		fprintf(stderr, "Invalid Principal name!\n");
	    count++;
	}
	while (username[0] == '\n' && count < 3);

	if (username[0] == '\n') {
	    fprintf(stderr, "Aborting!!\n\n");
	    return(1);
	}

	username[strlen(username) -1] = '\0';
    }
    
    (void) memcpy( inbuf.data + 3, username, strlen(username));
    inbuf.length = strlen(username) + 3;
 
    if ((retval = krb5_mk_priv(&inbuf,
			ETYPE_DES_CBC_CRC,
			&my_creds->keyblock, 
			local_addr, 
			foreign_addr,
			*seqno,
			KRB5_PRIV_DOSEQUENCE|KRB5_PRIV_NOTIME,
			0,
			0,
			&msg_data))) {
        fprintf(stderr, "Error during Second Message Encoding: %s!\n",
			error_message(retval));
	free(inbuf.data);
        return(1);
    }
    free(inbuf.data);

    /* write private message to server */
    if (krb5_write_message(local_socket, &msg_data)){
        fprintf(stderr, "Write Error During Second Message Transmission!\n");
        return(1);
    } 
    free(msg_data.data);

    /* Ok Now let's get the private message */
    if (retval = krb5_read_message(local_socket, &inbuf)){
        fprintf(stderr, "Read Error During Second Reply: %s!\n",
			error_message(retval));
        return(1);
    }

    if ((retval = krb5_rd_priv(&inbuf,
			&my_creds->keyblock,
    			foreign_addr, 
			local_addr,
			rep_ret->seq_number, 
			KRB5_PRIV_DOSEQUENCE|KRB5_PRIV_NOTIME,
			0,
			0,
			&msg_data))) {
        fprintf(stderr, "Error during Second Read Decoding :%s!\n", 
			error_message(retval));
        free(inbuf.data);
        return(1);
    }
    free(inbuf.data);
    free(msg_data.data);

    if (msg_data.data[2] == KADMBAD) {
        fprintf(stderr, "Principal Does NOT Exist!\n\n");
        return(0);
    }

    kadm_snd_mod(my_creds, rep_ret, local_addr, 
			foreign_addr, local_socket, seqno);

    if ((inbuf.data = (char *) calloc(1, 2)) == (char *) 0) {
        fprintf(stderr, "No memory for command!\n");
        return(1);
    }


    inbuf.data[0] = KADMIN;
    inbuf.data[1] = KADMGOOD;
    inbuf.data[2] = SENDDATA3;
    inbuf.length = 3;
 
    if ((retval = krb5_mk_priv(&inbuf,
                        ETYPE_DES_CBC_CRC,
                        &my_creds->keyblock,
                        local_addr,
                        foreign_addr,
                        *seqno,
                        KRB5_PRIV_DOSEQUENCE|KRB5_PRIV_NOTIME,
                        0,
                        0,
                        &msg_data))) {
        fprintf(stderr, "Error during Second Message Encoding: %s!\n",
                        error_message(retval));
	free(inbuf.data);
        return(1);
    }
    free(inbuf.data);
     
         /* write private message to server */
    if (krb5_write_message(local_socket, &msg_data)){
        fprintf(stderr, "Write Error During Second Message Transmission!\n");
        return(1);
    }
    free(msg_data.data);
     
                /* Ok Now let's get the final private message */
    if (retval = krb5_read_message(local_socket, &inbuf)){
        fprintf(stderr, "Read Error During Final Reply: %s!\n",
                        error_message(retval));
        retval = 1;
    }

    if ((retval = krb5_rd_priv(&inbuf,
                        &my_creds->keyblock,
                        foreign_addr,
                        local_addr,
                        rep_ret->seq_number,
                        KRB5_PRIV_DOSEQUENCE|KRB5_PRIV_NOTIME,
                        0,
                        0,
                        &msg_data))) {
        fprintf(stderr, "Error during Final Read Decoding :%s!\n",
                        error_message(retval));
	free(inbuf.data);
        return(1);
    }
    free(inbuf.data);

    memcpy(&rd_priv_resp.appl_code, msg_data.data, 1);
    memcpy(&rd_priv_resp.oper_code, msg_data.data + 1, 1);
    memcpy(&rd_priv_resp.retn_code, msg_data.data + 2, 1);

    free(msg_data.data);

    if (!((rd_priv_resp.appl_code == KADMIN) &&
		(rd_priv_resp.retn_code == KADMGOOD))) {
	fprintf(stderr, "Error Performing kadmin service!\n");
	retval = 1;
    } else {
        fprintf(stderr, "\nDatabase Modification Successful.\n");
    }
    return(0);
}
