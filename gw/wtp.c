/*
 * wtp.c - WTP implementation
 *
 *Implementation is for now very straigthforward, WTP state machines are stored
 *in an unordered linked list (this fact will change, naturally).
 *
 *By Aarno Syv�nen for WapIT Ltd.
 */

#include "wtp.h"

/*
 * Note that the outer {} for the struct definition come from the MACHINE
 * macro call in wtp_machine-decl.h and that a preprocessor option HAVE_THREADS
 * can be enabled or disabled 
 */
struct WTPMachine
        #define INTEGER(name) long name
        #define OCTSTR(name) Octstr *name
        #define QUEUE(name) /* XXX event queue to be implemented later */
	#define TIMER(name) WTPTimer *name
#if HAVE_THREADS
        #define MUTEX(name) pthread_mutex_t name
#else
        #define MUTEX(name) int name
#endif
        #define NEXT(name) struct WTPMachine *name
        #define MACHINE(field) field
        #include "wtp_machine-decl.h"
;


struct WTPEvent {
    enum event_name type;

    #define INTEGER(name) long name
    #define OCTSTR(name) Octstr *name
    #define EVENT(name, field) struct name field name;
    #include "wtp_events-decl.h" 
};

static WTPMachine *list = NULL;

/*****************************************************************************
 *
 *Prototypes of internal functions:
 */

char *name_event(enum event_name name);

/******************************************************************************
 *
 *EXTERNAL FUNCTIONS:
 */

WTPEvent *wtp_event_create(enum event_name type) {
	WTPEvent *event;
	
	event = malloc(sizeof(WTPEvent));
	if (event == NULL)
		goto error;

	event->type = type;
	
	#define INTEGER(name) p->name=0
	#define OCTSTR(name) p->name=octstr_create_empty();\
                             if (p->name == NULL)\
                                goto error
	#define EVENT(type, field) { struct type *p = &event->type; field } 
	#include "wtp_events-decl.h"
        return event;
/*
 *TBD: Send Abort(CAPTEMPEXCEEDED)
 */
error:
        #define INTEGER(name) p->name=0
        #define OCTSTR(name) if (p->name != NULL)\
                                octstr_destroy(p->name)
        #define EVENT(type, field) { struct type *p = &event->type; field }
        #include "wtp_events-decl.h"
        free(event);
	error(errno, "Out of memory.");
	return NULL;
}
/*
 *Note: We must use p everywhere (including events having only integer 
 *fields), otherwise we get a compiler warning.
 */

void wtp_event_destroy(WTPEvent *event) {
	#define INTEGER(name) p->name = 0
        #define OCTSTR(name) octstr_destroy(p->name)
        #define EVENT(type, field) { struct type *p = &event->type; field } 
        #include "wtp_events-decl.h"

	free(event);
}

void wtp_event_dump(WTPEvent *event) {
  	debug(0, "Event %p:", (void *) event); 
	debug(0, " type = %s", name_event(event->type));
	#define INTEGER(name) debug(0, "Integer field #name %ld:", p->name); 
	#define OCTSTR(name)  debug(0, "Octstr field #name :");\
                              octstr_dump(p->name)
	#define EVENT(type, stmt) { struct type *p = &event->type; stmt } 
	#include "wtp_events-decl.h"
}

WTPMachine *wtp_machine_create(void){

        WTPMachine *machine;
        int dummy, ret;

        machine=malloc(sizeof(WTPMachine));
        if (machine == NULL)
           goto error;
        
        #define INTEGER(name) machine->name=0
        #define OCTSTR(name) machine->name=octstr_create_empty();\
                             if (machine->name == NULL)\
                                goto error
        #define QUEUE(name) /*Queue will be implemented later*/
#if HAVE_THREADS
        #define MUTEX(name) dummy=pthread_mutex_init(&machine->name, NULL)
#else
        #define MUTEX(name) machine->name=0
#endif                
        #define TIMER(name) machine->name=wtp_timer_create();\
                            if (machine->name == NULL)\
                               goto error
        #define NEXT(name) machine->name=NULL
        #define MACHINE(field) field
        #include "wtp_machine-decl.h"

        machine->in_use=1;

        ret=pthread_mutex_lock(&list->mutex);
        machine->next=list;
        list=machine;
/*
 *List was not empty
 */
        if (ret != EINVAL)
           ret=pthread_mutex_unlock(&list->mutex);

        return machine;
/*
 *Message Abort(CAPTEMPEXCEEDED), to be added later. 
 *Thou shalt not leak memory... Note, that a macro could be called many times.
 *So it is possible one call to succeed and another to fail. 
 */
 error:  if (machine != NULL) {
            #define INTEGER(name)
            #define OCTSTR(name) if (machine->name != NULL)\
                                    octstr_destroy(machine->name)
            #define QUEUE(name)  /*to be implemented later*/
#if HAVE_THREADS
            #define MUTEX(name)  ret=pthread_mutex_lock(&machine->name);\
                                 ret=pthread_mutex_destroy(&machine->name)
#else
            #define MUTEX(name)
#endif
            #define TIMER(name) if (machine->name != NULL)\
                                   wtp_timer_destroy(machine->name)
            #define NEXT(name)
            #define MACHINE(field) field
            #include "wtp_machine-decl.h"
        }
        free(machine);
        error(errno, "Out of memory");
        return NULL;
}

/*
 *Mark a WTP state machine unused. Normal functions do not remove machines.
 */
void wtp_machine_mark_unused(WTPMachine *machine){

        WTPMachine *temp;
        int ret;

        ret=pthread_mutex_lock(&list->mutex);
        if (ret == EINVAL){
           error(errno, "Mutex not iniatilized. (List probably empty.)");
           return;
        }

        temp=list;
        ret=pthread_mutex_lock(&temp->next->mutex);

        while (temp != NULL && temp->next != machine){
            ret=pthread_mutex_unlock(&temp->mutex);
            temp=temp->next;
            if (temp != NULL)
               ret=pthread_mutex_lock(&temp->next->mutex);
        }

        if (temp == NULL){
            if (ret != EINVAL)
               ret=pthread_mutex_unlock(&temp->mutex);
            debug(0, "Machine unknown");
            return;
	}
       
        temp->in_use=0;
        if (ret != EINVAL)
           ret=pthread_mutex_unlock(&temp->mutex);
        return;
}

/*
 *Really removes a WTP state machine. Used only by the garbage collection.
 */
void wtp_machine_destroy(WTPMachine *machine){

        WTPMachine *temp;
        int ret, d_ret;

        ret=pthread_mutex_lock(&list->mutex);
        if (ret == EINVAL){
           error(errno, "Empty list (mutex not iniatilized)");
           return;
        }
        ret=pthread_mutex_lock(&list->next->mutex);

        if (list == machine) {
           list=machine->next;         
           if (ret != EINVAL)
              ret=pthread_mutex_unlock(&list->next->mutex);
           ret=pthread_mutex_unlock(&list->mutex);

        } else {
          temp=list;

          while (temp != NULL && temp->next != machine){ 
                ret=pthread_mutex_unlock(&temp->mutex);
                temp=temp->next;
                if (temp != 0)
                    ret=pthread_mutex_lock(&temp->next->mutex);
          }

          if (temp == NULL){
              
              if (ret != EINVAL)
                  ret=pthread_mutex_unlock(&temp->next->mutex);
              ret=pthread_mutex_unlock(&temp->mutex);
              debug(0, "Machine unknown");
              return;
	  }
          temp->next=machine->next;
	}

        #define INTEGER(name)        
        #define OCTSTR(name) octstr_destroy(temp->name)
        #define TIMER(name) wtp_timer_destroy(temp->name)
        #define QUEUE(name) /*queue to be implemented later*/
#if HAVE_THREADS
        #define MUTEX(name) d_ret=pthread_mutex_destroy(&temp->name)
#else
        #define MUTEX(name)
#endif
        #define NEXT(name)
        #define MACHINE(field) field
        #include "wtp_machine-decl.h"

        free(temp);
        if (ret != EINVAL)
           ret=pthread_mutex_unlock(&machine->next->mutex);
        ret=pthread_mutex_unlock(&machine->mutex);
        
        return;
}

/*
 *Write state machine fields, using debug function from a project library 
 *wapitlib.c.
 */
void wtp_machine_dump(WTPMachine  *machine){

        int ret;

        debug(0, "Machine %p:", (void *) machine); 
	#define INTEGER(name) debug(0, "Integer field #name %ld:",machine->name)
	#define OCTSTR(name)  debug(0, "Octstr field #name :");\
                              octstr_dump(machine->name)
        #define TIMER(name)   debug(0, "Machine timer %p:", (void *) \
                              machine->name)
        #define QUEUE(name)   /*to be implemented later*/
#if HAVE_THREADS
        #define MUTEX(name)   ret=pthread_mutex_trylock(&machine->name);\
                              if (ret == EBUSY)\
                                  debug(0, "Machine locked");\
                              else {\
                                  debug(0, "Machine unlocked");\
                                  ret=pthread_mutex_unlock(&machine->name);\
                              }
#else
        #define MUTEX(name)
#endif
        #define NEXT(name)
	#define MACHINE(field) field
	#include "wtp_machine-decl.h"
}

WTPMachine *wtp_machine_find(Octstr *source_address, long source_port,
	   Octstr *destination_address, long destination_port, long tid){

           WTPMachine *temp;
           int ret;

/*
 *We are interested only machines in use, it is, having in_use-flag 1.
 */
           ret=pthread_mutex_lock(&list->mutex);
           if (ret == EINVAL){
             error(errno, "Empty list (mutex not iniatilized)");
           return NULL;
           }
           
           temp=list;

           while (temp != NULL){
   
                if ((temp->source_address != source_address &&
                   temp->source_port != source_port &&
                   temp->destination_address != destination_address &&
                   temp->destination_port != destination_port &&
		   temp->tid != tid) || temp->in_use == 0){

                   pthread_mutex_unlock(&temp->mutex);
                   temp=temp->next;
                   pthread_mutex_lock(&temp->mutex);

                } else {
                   pthread_mutex_unlock(&temp->mutex);
                   debug(0, "Machine %p found", (void *) temp);
                   return temp;
               }              
           }
           pthread_mutex_unlock(&temp->mutex);
           debug(0, "Machine %p not found", (void *) temp);
           return temp;
}

/*
 *Transfers data from fields of a message to fields of WTP event. User data has
 *the host byte order. Updates the log and sends protocol error messages.
 */

WTPEvent *wtp_unpack_wdp_datagram(Msg *msg){

         WTPEvent *event;
         int octet,
             this_octet,

             con,
             pdu_type,
             gtr,
             ttr,
	     first_tid,  /*first octet of the tid, in the network order*/ 
	     last_tid,   /*second octet of the tid, in the network order*/
             tid,
             rcv_tid,
             version,
             tcl,
             abort_type,
             tpi_length_type,
             tpi_length;

/*
 *every message type uses the second and the third octets for tid. Bytes are 
 *already in host order.
 */
         first_tid=octstr_get_char(msg->wdp_datagram.user_data,1);
         last_tid=octstr_get_char(msg->wdp_datagram.user_data,2);
         rcv_tid=first_tid;
         rcv_tid=(rcv_tid << 8) + last_tid;
/*
 *Toggle first bit of the tid field. It tells whether the packet is sended by
 *the iniator or by the responder. So in the message the bit in question has
 *the opposite value.
 */
         tid = rcv_tid^0x8000;
         debug(0, "first_tid=%d last_tid=%d tid=%d rcv_tid=%d", first_tid, 
               last_tid, tid, rcv_tid);

         this_octet=octet=octstr_get_char(msg->wdp_datagram.user_data, 0);
         if (octet == -1)
            goto no_datagram;

         con=this_octet>>7; 
         if (con == 0){
            this_octet=octet;
            pdu_type=this_octet>>3&15;
            this_octet=octet;

            if (pdu_type == 0){
               goto no_segmentation;
            }
/*
 *Message type was invoke
 */
            if (pdu_type == 1){

               event=wtp_event_create(RcvInvoke);
               if (event == NULL)
                  goto cap_error;
               event->RcvInvoke.tid=tid;

               gtr=this_octet>>2&1;
               this_octet=octet;
               ttr=this_octet>>1&1;
               if (gtr == 0 || ttr == 0){
		  goto no_segmentation;
               }
               this_octet=octet;
               event->RcvInvoke.rid=this_octet&1; 

               this_octet=octet=octstr_get_char(
                          msg->wdp_datagram.user_data, 3);
               version=this_octet>>6&3;
               if (version != 0){
                  goto wrong_version;
               } 
               this_octet=octet;
               event->RcvInvoke.tid_new=this_octet>>5&1;
               this_octet=octet;
               event->RcvInvoke.up_flag=this_octet>>4&1;
               this_octet=octet;
               tcl=this_octet&3; 
               if (tcl > 2)
                  goto illegal_header;
               event->RcvInvoke.tcl=tcl; 
 
/*
 *At last, the message itself. We remove the header.
 */
               octstr_delete(msg->wdp_datagram.user_data, 0, 4);
               event->RcvInvoke.user_data=msg->wdp_datagram.user_data; 
               info(0, "Invoke event packed"); 
               wtp_event_dump(event);       
            }
/*
 *Message type is supposed to be result. This is impossible, so we have an
 *illegal header.
 */
            if (pdu_type == 2){
               goto illegal_header;
            }
/*
 *Message type was ack.
 */
            if (pdu_type == 3){
               event=wtp_event_create(RcvAck);
               if (event == NULL)
                  goto cap_error;
               event->RcvAck.tid=tid;

               this_octet=octet=octstr_get_char(
                          msg->wdp_datagram.user_data, 0);
               event->RcvAck.tid_ok=this_octet>>2&1;
               this_octet=octet;
               event->RcvAck.rid=this_octet&1;
               info(0, "Ack event packed");
               wtp_event_dump(event);
            }

/*
 *Message type was abort.
 */
	    if (pdu_type == 4){
                event=wtp_event_create(RcvAbort);
                if (event == NULL)
                    goto cap_error;
                event->RcvAbort.tid=tid;
                
               octet=octstr_get_char(msg->wdp_datagram.user_data, 0);
               abort_type=octet&7;
               if (abort_type > 1)
                  goto illegal_header;
               event->RcvAbort.abort_type=abort_type;   

               octet=octstr_get_char(msg->wdp_datagram.user_data, 3);
               if (octet > NUMBER_OF_ABORT_REASONS)
                  goto illegal_header;
               event->RcvAbort.abort_reason=octet;
               info(0, "abort event packed");
            }

/*
 *WDP does the segmentation.
 */
            if (pdu_type > 4 || pdu_type < 8){
               goto no_segmentation;
            }
            if (pdu_type >= 8){
               goto illegal_header;
            } 

/*
 *Message is of variable length. This is possible only when we are receiving
 *an invoke message.(For now, only info tpis are supported.)
 */
         } else {
           this_octet=octet=octstr_get_char(msg->wdp_datagram.user_data, 4);
           tpi_length_type=this_octet>>2&1;
/*
 *TPI can be long
 */
           if (tpi_length_type == 1){
               
               tpi_length=1;
           } else {
/*or short*/
           }
         }      
         return event;
/*
 *Send Abort(WTPVERSIONZERO)
 */
wrong_version:
         wtp_event_destroy(event);
         error(0, "Version not supported");
         return NULL;
/*
 *Send Abort(NOTIMPLEMENTEDSAR)
 */
no_segmentation:
         wtp_event_destroy(event);
         error(errno, "No segmentation implemented");
         return NULL;
/*
 *TBD: Send Abort(CAPTEMPEXCEEDED), too.
 */
cap_error:
         free(event);
         error(errno, "Out of memory");
         return NULL;
/*
 *Send Abort(PROTOERR)
 */
illegal_header:
         wtp_event_destroy(event);
         error(errno, "Illegal header structure");
         return NULL;
/*
 *TBD: Another error message. Or panic?
 */
no_datagram:   
         free(event);
         error(errno, "No datagram received");
         return NULL;
}

/*****************************************************************************
 *
 *INTERNAL FUNCTIONS:
 */
char *name_event(enum event_name type){

       switch (type){
              #define EVENT(type, field) case type: return #type;
              #include "wtp_events-decl.h"
              default:
                      return "unknown name";
       }
 }

/*****************************************************************************/










