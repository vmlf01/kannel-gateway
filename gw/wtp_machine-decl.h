
/*
 * wtp_machine-decl.h - macro call for generating WTP state machine. See the 
 * architecture document for guidance how to use and update it.
 *
 * By Aarno Syv�nen for WapIT Ltd.
 */

MACHINE(INTEGER(in_use);
        ENUM(state);
        INTEGER(tid);
        OCTSTR(source_address);
        INTEGER(source_port);
        OCTSTR(destination_address);
        INTEGER(destination_port);
        INTEGER(tcl);
        INTEGER(aec);
        INTEGER(rcr);
        INTEGER(u_ack);
        INTEGER(hold_on);
        INTEGER(rid);
        INTEGER(ack_pdu_sent);
        TIMER(timer_data);
        MUTEX(mutex);
	INTEGER(locker);
	MUTEX(queue_lock);
        QUEUE(event_queue_head);
        QUEUE(event_queue_tail);
        NEXT(next);
        )

#undef MACHINE
#undef INTEGER
#undef ENUM
#undef OCTSTR
#undef TIMER
#undef QUEUE
#undef MUTEX
#undef NEXT

