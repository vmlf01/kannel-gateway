#ifndef SMSCCONN_P_H
#define SMSCCONN_P_H

/* SMSC Connection private header
 *
 * Defines internal private structure
 *
 * Kalle Marjola 2000 for project Kannel
 *

 ADDING AND WORKING OF NEW SMS CENTER CONNECTIONS:

 These are guidelines and rules for adding new SMSC Connections to
 Kannel. See file bb_smscconn.h for callback function prototypes.

 New SMSC Connection handler is free-formed module which only have following
 rules:
 
 1) Each new SMSC Connection MUST implement function
    smsc_xxx_create(SMSCConn *conn, ConfigGrp *cfg), which:

    a) SHOULD NOT block   (XXX)
    b) MUST warn about any configuration group variables it does
       not support    (XXX)
    c) MUST set up send_msg dynamic function to handle messages
       to-be-sent. This function MAY NOT block.
    d) CAN set up private shutdown function, which MAY NOT block
    e) SHOULD set private function to return number of queued messages
       to-be-sent inside the driver
    f) MUST set SMSCConn->name
       
 2) Each SMSC Connection MUST call certain BB callback functions when
    certain things occur:

    a) Each SMSC Connection MUST call callback function
       bb_smscconn_killed when it dies because it was put down earlier
       with bb_smscconn_shutdown or it simply cannot keep the connection
       up (wrong password etc. When killed,
       SMSC Connection MUST release all memory it has taken EXCEPT for
       the basic SMSCConn struct, which is laterwards released by the
       bearerbox. 

    b) When SMSC Connection receives a message from SMSC, it must
       create a new Msg from it and call bb_smscconn_received

    c) When SMSC Connection has sent a message to SMSC, it MUST call
       callback function bb_smscconn_sent

    d) When SMSC Connection has failed to send a message to SMSC, it
       MUST call callback function bb_smscconn_send_failed with appropriate
       reason

 3) SMSC Connection MUST fill up SMSCConn structure as needed to, and is
    responsible for any concurrency timings. SMSCConn->status MAY NOT be
    set to KILLED until the connection is really that. Use why_killed to
    make internally dead, supplied with reason.

 4) When SMSC Connection shuts down (shutdown called), it MUST try to send
    all messages so-far relied to it to be sent if 'finish_sending' is set
    to non-zero. If set to 0, it MUST call bb_smscconn_send_failed
    for each message not yet sent.

    After everything is ready (it can happen in different thread), before
    calling callback function bb_smscconn_killed it MUST release all memory it
    has taken except for basic SMSCConn structure, and set status to
    SMSCCONN_KILLED so it can be finally deleted.

 5) Callback bb_smscconn_ready is automatically called by main
    smscconn_create. New implementation MAY NOT call it directly

 6) SMSC Connection driver must obey is_stopped/stopped variable to
    suspend receiving (it can still send/re-connect), or must set
    appropriate function calls. When connection is stopped, it is not
    allowed to receive any new messages
*/
 
#include <signal.h>
#include "gwlib/gwlib.h"

struct smscconn {
    /* variables set by appropriate SMSCConn driver */
    int		status;		/* see smscconn.h */
    int 	load;	       	/* load factor, 0 = no load */
    int		why_killed;	/* time to die with reason */
    time_t 	connect_time;	/* When connection to SMSC was established */

    /* connection specific counters (created in smscconn.c, updated
     *  by specific driver) */
    Counter *received;
    Counter *sent;
    Counter *failed;

    Mutex 	*flow_mutex;	/* used to lock SMSCConn structure (both
				 *  in smscconn.c and specific driver) */

    /* SMSCConn variables set in smscconn.c */
    int 	is_stopped;

    Octstr *name;		/* Descriptive name filled from connection info */
    Octstr *id;			/* Abstract name spesified in configuration and
				   used for logging and routing */
    Octstr *allowed_smsc_id;
    Octstr *denied_smsc_id;
    Octstr *preferred_smsc_id;

    Octstr *denied_prefix;
    Octstr *preferred_prefix;
    
    
    /* XXX: move rest global data from Smsc here
     */

    /* pointers set by specific driver, but initiated to NULL by smscconn */
    
    /* pointer to function called when smscconn_shutdown called.
       Note that this function is not needed always. */
    int (*shutdown) (SMSCConn *conn, int finish_sending);

    /* pointer to function called when a new message is needed to be sent.
       MAY NOT block */
    int (*send_msg) (SMSCConn *conn, Msg *msg);

    /* pointer to function which returns current number of queued
     * messages to-be-sent. The function CAN also set load factor directly
     * to SMSCConn structure (above)
     */
    long (*queued) (SMSCConn *conn);

    /* pointers to functions called when connection started/stopped
     * (suspend/resume), if not NULL */
    
    void (*start_conn) (SMSCConn *conn);
    void (*stop_conn) (SMSCConn *conn);
    
    void *data;			/* SMSC specific stuff */

};

/*
 * Initializers for various SMSC connection implementations,
 * each should take same arguments and return an int,
 * which is 0 for okay and -1 for error.
 *
 * Each function is responsible for setting up all dynamic
 * function pointers at SMSCConn structure and starting up any
 * threads it might need.
 */

/* generic wrapper for old SMSC implementations (uses old smsc.h).
 * Responsible file: smsc_wrapper.c */
int smsc_wrapper_create(SMSCConn *conn, ConfigGroup *cfg);

/* ADD NEW CREATE FUNCTIONS HERE
 *
 * int smsc_xxx_create(SMSCConn *conn, ConfigGroup *cfg);
 */


#endif
