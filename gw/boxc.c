/*
 * BOX Connection
 *
 * Kalle Marjola for Wapit ltd.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "config.h"

#include "gwlib/gwlib.h"
#include "boxc.h"
#include "bb_msg.h"
#include "msg.h"
#include "smsbox_req.h"


BOXC *boxc_open(int fd, char *allow_ip, char *deny_ip)
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    BOXC *nb;
    char accept_ip[NI_MAXHOST];
    int ret;

    nb = gw_malloc(sizeof(BOXC));

    if (fd < 0) {
	debug("bb", 0, "BOXC: Started an internal SMS BOX Thread");
	nb->fd = BOXC_THREAD;
	nb->client_ip = NULL;
    } else {
	debug("bb", 0, "BOXC: Accepting a new client...");

	client_addr_len = sizeof(client_addr);
	nb->fd = accept(fd, (struct sockaddr *)&client_addr, &client_addr_len);
	if (nb->fd < 0)
	    goto error;

	memset(accept_ip, 0, sizeof(accept_ip));
        getnameinfo((struct sockaddr *)&client_addr, client_addr_len,
		    accept_ip, sizeof(accept_ip), 
		    NULL, 0, NI_NUMERICHOST);

	ret = 0;
	if (allow_ip != NULL)
	    ret = check_ip(allow_ip, accept_ip, NULL);
	if (ret < 1 && deny_ip != NULL)
	    if (check_ip(deny_ip, accept_ip, NULL) == 1) {
		warning(0, "Non-allowed connect tried from <%s>, disconnected",
			accept_ip);
		gw_free(nb);
		return NULL;
	    }
	
        nb->client_ip = gw_strdup(accept_ip);
	info(0, "BOXC: Client connected from <%s>", accept_ip);

	/* TODO: do the hand-shake, baby, yeah-yeah! */
    }
    nb->load = 0;
    nb->box_heartbeat = time(NULL);
    return nb;
    
error:
    error(errno, "BOXC: Failed to create and open Box connection");
    gw_free(nb);
    return NULL;

}


int boxc_close(BOXC *boxc)
{
    if (boxc == NULL)
	return 0;
    gw_free(boxc->client_ip);
    if (boxc->fd >= 0)
	close(boxc->fd);

    gw_free(boxc);
    return 0;
}


int boxc_send_message(BOXC *boxc, RQueueItem *msg, RQueue *reply_queue)
{
    int ack = 0;

    gw_assert(msg != NULL);
    
    if (msg->msg_type == R_MSG_TYPE_ACK ||
	msg->msg_type == R_MSG_TYPE_NACK) {

	rqi_delete(msg);
	return 0;
    }
    if (boxc->fd == BOXC_THREAD) {

	debug("bb", 0, "BOXC: starting a new thread to handle request");
	
	(void)start_thread(1, smsbox_req_thread, msg->msg, 0);

	msg->msg = NULL;	/* it has been taken... */
    } else {
	Octstr *pack;

	pack = msg_pack(msg->msg);
	if (pack == NULL) goto error;
	    
	octstr_send(boxc->fd, pack);
	octstr_destroy(pack);

	if (msg->msg_class == R_MSG_CLASS_SMS) {

	    if(msg_type(msg->msg) == smart_sms) {
#if 0
		debug("bb", 0, "BOXC: Write < %s >", 
		      octstr_get_cstr(msg->msg->smart_sms.msgdata));
#endif
	    }
	} else {
#if 0
	    debug("bb", 0, "BOXC: Write < WAP >");
#endif
	    }
	ack = 1;
    }

    /* ACK/NACK later.. now we ignore...
     * 
    if (msg->msg_type == R_MSG_TYPE_MO) {
	if (ack) {
	    msg->msg_type = R_MSG_TYPE_ACK;   
	} else {
	    msg->msg_type = R_MSG_TYPE_NACK;
	}
 	rq_push_msg_ack(reply_queue, msg);
     } else
     *
     */
    rqi_delete(msg);	/* delete message */
    
	
    return 0;
error:
    error(0, "BOXC: Send message failed");
    return -1;
}


int boxc_get_message(BOXC *boxc, RQueueItem **rmsg)
{
    RQueueItem *msg;
    int ret;

    gw_assert(boxc != NULL);
    
    msg = NULL;
    *rmsg = NULL;
    if (boxc->fd == BOXC_THREAD)

	/*
	 * thread connection adds directly into queue, so
	 * this function is redundant with it
	 */
	return 0;	
    else {
	if (read_available(boxc->fd, 0) > 0) {
	    Msg *pmsg;
	    Octstr *os;
	    
	    boxc->box_heartbeat = time(NULL);	/* update heartbeat */
	    
	    /*
	     * Note: the following blocks the connection if there is
	     * partial data. But that's life, smsbox would not
	     * accept our data neither if it has blocked while writing,
	     * or would it?
	     */
	    /* XXX os is not freed in error sitatations. memory leak. --liw */
	    ret = octstr_recv(boxc->fd, &os);
	    if (ret < 1) {
		if (ret == -1)
		    error(0, "BOXC: Socket error, closing...");
		return -1;	/* time to die */
	    }
	    pmsg = msg_unpack(os);
	    if (pmsg == NULL)
		goto error;

	    if (msg_type(pmsg) == heartbeat) {
		boxc->load = pmsg->heartbeat.load;
/*		if (boxc->load > 0)*/
		    debug("bb", 0, "BOXC: Load factor %d received", boxc->load);

		octstr_destroy(os);
		msg_destroy(pmsg);
		return 0;
	    }
	    else if (msg_type(pmsg) == smart_sms) {
		
		msg = rqi_new(R_MSG_CLASS_SMS, R_MSG_TYPE_MT);
		if (msg == NULL) {
		    error(0, "Failed to create new message, killing thread");
		    return -1;
		}
		msg->msg = pmsg;
#if 0
		debug("bb", 0, "BOXC: Read < %s >", octstr_get_cstr(pmsg->smart_sms.msgdata));
#endif
	    }
	    else if (msg_type(pmsg) == wdp_datagram) {
		
		msg = rqi_new(R_MSG_CLASS_WAP, R_MSG_TYPE_MT);
		if (msg == NULL) {
		    error(0, "Failed to create new message, killing thread");
		    return -1;
		}
		msg->msg = pmsg;
#if 0
		debug("bb", 0, "BOXC: Read < WAP >");
#endif
	    }
	    octstr_destroy(os);
	}
    }
    if (msg) {
	*rmsg = msg;
	return 1;
    }
    return 0;

error:
    rqi_delete(msg);
    return -1;
}
 
