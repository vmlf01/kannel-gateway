/*
 * wap-events-def.h - definitions for wapbox events
 *
 * This file uses a pre-processor trick to define the structure of
 * structures. See the documentation.
 *
 * Aarno Syv�nen
 * Lars Wirzenius
 */


WAPEVENT(TR_Invoke_Ind,
          INTEGER(ack_type)
          OCTSTR(user_data)
          INTEGER(tcl)
          INTEGER(wsp_pdu)
          INTEGER(wsp_tid)
	  INTEGER(tid)
	  INTEGER(mid)
	  ADDRTUPLE(addr_tuple)
	  )

WAPEVENT(TR_Invoke_Cnf,
          OCTSTR(exit_info)
          INTEGER(exit_info_present)
          INTEGER(wsp_pdu)
          INTEGER(wsp_tid)
	  ADDRTUPLE(addr_tuple);
          )

WAPEVENT(TR_Result_Cnf,
          OCTSTR(exit_info)
          INTEGER(exit_info_present)
          INTEGER(wsp_pdu)
          INTEGER(wsp_tid)
	  ADDRTUPLE(addr_tuple)
          )

WAPEVENT(TR_Abort_Ind,
          INTEGER(abort_code)
          INTEGER(wsp_pdu)
          INTEGER(wsp_tid)
	  INTEGER(tid)
	  INTEGER(mid)
	  ADDRTUPLE(addr_tuple)
          )

WAPEVENT(S_Connect_Res,
	  INTEGER(tid)
	  INTEGER(mid)
	  OCTSTR(server_headers)
	  OCTSTR(negotiated_capabilities)
	  )

WAPEVENT(Release,
	  INTEGER(tid)
	  INTEGER(mid)
	  OCTSTR(url)
	  HTTPHEADER(http_headers)
	  )

WAPEVENT(S_MethodInvoke_Ind,
	  INTEGER(tid)
	  INTEGER(mid)
	  OCTSTR(url)
	  INTEGER(method)
	  HTTPHEADER(http_headers)
	  INTEGER(server_transaction_id)
	  SESSION_MACHINE(session)
	  )

WAPEVENT(S_MethodInvoke_Res,
	  INTEGER(tid)
	  INTEGER(mid)
	  )

WAPEVENT(S_MethodResult_Req,
	  INTEGER(server_transaction_id)
	  INTEGER(status)
	  INTEGER(response_type)
	  OCTSTR(response_body)
	  INTEGER(tid)
	  INTEGER(mid)
	  )

WAPEVENT(RcvInvoke,
      OCTSTR(user_data)
      OCTSTR(exit_info)
      INTEGER(tcl)
      INTEGER(tid)
      INTEGER(tid_new)
      INTEGER(rid)
      INTEGER(up_flag)
      INTEGER(exit_info_present)
      INTEGER(no_cache_supported)
      ADDRTUPLE(addr_tuple)
      )

WAPEVENT(RcvAbort,
      INTEGER(tid)
      INTEGER(abort_type)
      INTEGER(abort_reason)
      ADDRTUPLE(addr_tuple)
      )

WAPEVENT(RcvAck,
      INTEGER(tid)
      INTEGER(tid_ok)
      INTEGER(rid)
      ADDRTUPLE(addr_tuple)
      )

WAPEVENT(TR_Invoke_Req,
      ADDRTUPLE(addr_tuple)
      INTEGER(ack_type)
      INTEGER(tcl)
      OCTSTR(user_data)
      )

WAPEVENT(TR_Invoke_Res,
      INTEGER(tid)
      OCTSTR(exit_info)
      INTEGER(exit_info_present)
      INTEGER(mid);
      )

WAPEVENT(TR_Result_Req,
      INTEGER(tid)
      OCTSTR(user_data)
      INTEGER(mid)
      )

WAPEVENT(TR_Abort_Req,
     INTEGER(tid)
     INTEGER(abort_type)
     INTEGER(abort_reason)
     INTEGER(mid)
     ) 

WAPEVENT(TimerTO_A,
     INTEGER(dummy)
     )

WAPEVENT(TimerTO_R,
     INTEGER(dummy)
     )

WAPEVENT(TimerTO_W,
     INTEGER(dummy)
     )

WAPEVENT(RcvErrorPDU,
     INTEGER(tid)
     ADDRTUPLE(addr_tuple)
     )


#undef WAPEVENT
#undef OCTSTR
#undef INTEGER
#undef SESSION_MACHINE
#undef HTTPHEADER
#undef ADDRTUPLE
