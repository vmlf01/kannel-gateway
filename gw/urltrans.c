/*
 * urltrans.c - URL translations
 *
 * Lars Wirzenius
 */


#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "urltrans.h"
#include "gwlib/gwlib.h"
#include "gw/sms.h"


/***********************************************************************
 * Definitions of data structures. These are not visible to the external
 * world -- they may be accessed only via the functions declared in
 * urltrans.h.
 */


/*
 * Hold one keyword/options entity
 */
struct URLTranslation {
    Octstr *keyword;	/* keyword in SMS (similar) query */
    List *aliases;	/* keyword aliases, List of Octstr */
    int type;		/* see enumeration in header file */
    Octstr *pattern;	/* url, text or file-name pattern */
    Octstr *prefix;	/* for prefix-cut */
    Octstr *suffix;	/* for suffix-cut */
    Octstr *faked_sender;/* works only with certain services */
    Octstr *default_sender;/* Default sender to sendsms-user */
    long max_messages;	/* absolute limit of reply messages */
    int concatenation;	/* send long messages as concatenated SMS's if true */
    Octstr *split_chars;/* allowed chars to be used to split message */
    Octstr *split_suffix;/* chars added to end after each split (not last) */
    int omit_empty;	/* if the reply is empty, is notification send */
    Octstr *header;	/* string to be inserted to each SMS */
    Octstr *footer;	/* string to be appended to each SMS */
    List *accepted_smsc; /* smsc id's allowed to use this service. If not set,
			    all messages can use this service */
    
    Octstr *name;	/* Translation name */
    Octstr *username;	/* send sms username */
    Octstr *password;	/* password associated */
    Octstr *forced_smsc;/* if smsc id is forcet to certain for this user */
    Octstr *default_smsc; /* smsc id if none given in http send-sms request */
    Octstr *allow_ip;	/* allowed IPs to request send-sms with this 
    	    	    	   account */
    Octstr *deny_ip;	/* denied IPs to request send-sms with this account */
    Octstr *allowed_prefix;	/* Prefixes (of sender) allowed in this translation, or... */
    Octstr *denied_prefix;	/* ...denied prefixes */
    Octstr *allowed_recv_prefix; /* Prefixes (of receiver) allowed in this translation, or... */
    Octstr *denied_recv_prefix;	/* ...denied prefixes */
    Numhash *white_list;	/* To numbers allowed, or ... */
    Numhash *black_list; /* ...denied numbers */

    int assume_plain_text; /* for type: octet-stream */
    int accept_x_kannel_headers; /* do we accept special headers in reply */
    int strip_keyword;	/* POST body */
    int send_sender;	/* POST headers */
    
    int args;
    int has_catchall_arg;
    int catch_all;
    Octstr *dlr_url;	/* Url to call for delivery reports */
};


/*
 * Hold the list of all translations.
 */
struct URLTranslationList {
    List *list;
    Dict *dict;		/* Dict of lowercase Octstr keywords*/
    Dict *names;	/* Dict of lowercase Octstr names */
};


/***********************************************************************
 * Declarations of internal functions. These are defined at the end of
 * the file.
 */

static long count_occurences(Octstr *str, Octstr *pat);
static URLTranslation *create_onetrans(CfgGroup *grp);
static void destroy_onetrans(void *ot);
static URLTranslation *find_translation(URLTranslationList *trans, 
					List *words, Octstr *smsc,
					Octstr *sender, Octstr *receiver, int *reject);
static URLTranslation *find_default_translation(URLTranslationList *trans,
						Octstr *smsc, Octstr *sender, Octstr *receiver,
						int *reject);
static URLTranslation *find_black_list_translation(URLTranslationList *trans,
						Octstr *smsc);


/***********************************************************************
 * Implementations of the functions declared in urltrans.h. See the
 * header for explanations of what they should do.
 */


static void destroy_keyword_list(void *list)
{
    list_destroy(list, NULL);
}


URLTranslationList *urltrans_create(void) 
{
    URLTranslationList *trans;
    
    trans = gw_malloc(sizeof(URLTranslationList));
    trans->list = list_create();
    trans->dict = dict_create(1024, destroy_keyword_list);
    trans->names = dict_create(1024, destroy_keyword_list);
    return trans;
}


void urltrans_destroy(URLTranslationList *trans) 
{
    list_destroy(trans->list, destroy_onetrans);
    dict_destroy(trans->names);
    dict_destroy(trans->dict);
    gw_free(trans);
}


int urltrans_add_one(URLTranslationList *trans, CfgGroup *grp)
{
    URLTranslation *ot;
    long i;
    List *list, *list2;
    Octstr *alias;
    
    ot = create_onetrans(grp);
    if (ot == NULL)
	return -1;
		
    list_append(trans->list, ot);
    
    list2 = dict_get(trans->names, ot->name);
    if (list2 == NULL) {
    	list2 = list_create();
	dict_put(trans->names, ot->name, list2);
    }
    list_append(list2, ot);

    if (ot->keyword == NULL || ot->type == TRANSTYPE_SENDSMS)
    	return 0;

    list = dict_get(trans->dict, ot->keyword);
    if (list == NULL) {
    	list = list_create();
	dict_put(trans->dict, ot->keyword, list);
    }
    list_append(list, ot);

    for (i = 0; i < list_len(ot->aliases); ++i) {
	alias = list_get(ot->aliases, i);
	list = dict_get(trans->dict, alias);
	if (list == NULL) {
	    list = list_create();
	    dict_put(trans->dict, alias, list);
	}
	list_append(list, ot);
    }


    return 0;
}


int urltrans_add_cfg(URLTranslationList *trans, Cfg *cfg) 
{
    CfgGroup *grp;
    List *list;
    
    list = cfg_get_multi_group(cfg, octstr_imm("sms-service"));
    while (list && (grp = list_extract_first(list)) != NULL) {
	if (urltrans_add_one(trans, grp) == -1) {
	    list_destroy(list, NULL);
	    return -1;
	}
    }
    list_destroy(list, NULL);
    
    list = cfg_get_multi_group(cfg, octstr_imm("sendsms-user"));
    while (list && (grp = list_extract_first(list)) != NULL) {
	if (urltrans_add_one(trans, grp) == -1) {
	    list_destroy(list, NULL);
	    return -1;
	}
    }
    list_destroy(list, NULL);

    return 0;
}


URLTranslation *urltrans_find(URLTranslationList *trans, Octstr *text,
			      Octstr *smsc, Octstr *sender, Octstr *receiver) 
{
    List *words;
    URLTranslation *t;
    int reject = 0;
    
    /* do not panic if text == NULL */
    if (text != NULL)
        words = octstr_split_words(text);
    else
        words = list_create();
    
    t = find_translation(trans, words, smsc, sender, receiver, &reject);
    list_destroy(words, octstr_destroy_item);
    if (reject)
	t = find_black_list_translation(trans, smsc);
    if (t == NULL) {
	t = find_default_translation(trans, smsc, sender, receiver, &reject);
	if (reject)
	    t = find_black_list_translation(trans, smsc);
    }
    return t;
}


URLTranslation *urltrans_find_service(URLTranslationList *trans, Msg *msg)
{
    URLTranslation *t;
    List *list;
    
    list = dict_get(trans->names, msg->sms.service);
    if (list != NULL) {
       t = list_get(list, 0);
    } else  {
       t = NULL;
    }
    return t;
}



URLTranslation *urltrans_find_username(URLTranslationList *trans, 
				       Octstr *name)
{
    URLTranslation *t;
    int i;

    gw_assert(name != NULL);
    for (i = 0; i < list_len(trans->list); ++i) {
	t = list_get(trans->list, i);
	if (t->type == TRANSTYPE_SENDSMS) {
	    if (octstr_compare(name, t->username) == 0)
		return t;
	}
    }
    return NULL;
}

/*
 * Remove the first word and the whitespace that follows it from
 * the start of the message data.
 */
static void strip_keyword(Msg *request)
{          
    int ch;
    long pos;

    pos = 0;

    for (; (ch = octstr_get_char(request->sms.msgdata, pos)) >= 0; pos++)
        if (isspace(ch))
            break;

    for (; (ch = octstr_get_char(request->sms.msgdata, pos)) >= 0; pos++)
        if (!isspace(ch))
            break;

    octstr_delete(request->sms.msgdata, 0, pos);
}


/*
 * Trans being NULL means that we are servicing ppg (doing dlr, but this does not
 * concern us here).
 */
Octstr *urltrans_get_pattern(URLTranslation *t, Msg *request)
{
    Octstr *enc;
    int nextarg, j;
    struct tm tm;
    int num_words;
    List *word_list;
    Octstr *result, *pattern;
    long pattern_len;
    long pos;
    int c;
    long i;
    Octstr *temp;
    Octstr *url, *reply; /* For and If delivery report */

    url = reply = NULL;
    
    if (request->sms.sms_type != report && t->type == TRANSTYPE_SENDSMS)
        return octstr_create("");

    if (request->sms.msgdata) {
        word_list = octstr_split_words(request->sms.msgdata);
        num_words = list_len(word_list);
    } else {
    	word_list = list_create();
        num_words = 0;
    }

    result = octstr_create("");

    /* check if this is a delivery report message or not */
    if (request->sms.sms_type != report) {
        pattern = t->pattern;
    } else {

        /* this is a DLR message */
        reply = octstr_duplicate(request->sms.msgdata);
        url = octstr_duplicate(request->sms.dlr_url);

        pattern = url;
        if (octstr_len(pattern) == 0) {
            if (t && octstr_len(t->dlr_url)) {
                pattern = t->dlr_url;
            } else {
                list_destroy(word_list, octstr_destroy_item);
                return octstr_create("");
            }
        }
    }

    pattern_len = octstr_len(pattern);
    nextarg = 1;
    pos = 0;
    for (;;) {
        while (pos < pattern_len) {
            c = octstr_get_char(pattern, pos);
            if (c == '%' && pos + 1 < pattern_len)
                break;
            octstr_append_char(result, c);
            ++pos;
        }

        if (pos == pattern_len)
            break;

    switch (octstr_get_char(pattern, pos + 1)) {
	case 'k':
	    if (num_words <= 0)
		break;
	    enc = octstr_duplicate(list_get(word_list, 0));
	    octstr_url_encode(enc);
	    octstr_append(result, enc);
	    octstr_destroy(enc);
	    break;

	case 's':
	    if (nextarg >= num_words)
		break;
	    enc = octstr_duplicate(list_get(word_list, nextarg));
	    octstr_url_encode(enc);
	    octstr_append(result, enc);
	    octstr_destroy(enc);
	    ++nextarg;
	    break;

	case 'S':
	    if (nextarg >= num_words)
		break;
	    temp = list_get(word_list, nextarg);
	    for (i = 0; i < octstr_len(temp); ++i) {
		if (octstr_get_char(temp, i) == '*')
		    octstr_append_char(result, '~');
		else
		    octstr_append_char(result, octstr_get_char(temp, i));
	    }
	    ++nextarg;
	    break;

	case 'r':
	    for (j = nextarg; j < num_words; ++j) {
		enc = octstr_duplicate(list_get(word_list, j));
		octstr_url_encode(enc);
		if (j != nextarg)
		    octstr_append_char(result, '+');
		octstr_append(result, enc);
		octstr_destroy(enc);
	    }
	    break;
    
	/* NOTE: the sender and receiver is already switched in
	 *    message, so that's why we must use 'sender' when
	 *    we want original receiver and vice versa
	 */
	case 'P':
	    enc = octstr_duplicate(request->sms.sender);
    	    octstr_url_encode(enc);
	    octstr_append(result, enc);
	    octstr_destroy(enc);
	    break;

	case 'p':
	    enc = octstr_duplicate(request->sms.receiver);
	    octstr_url_encode(enc);
	    octstr_append(result, enc);
	    octstr_destroy(enc);
	    break;

	case 'Q':
	    if (strncmp(octstr_get_cstr(request->sms.sender), "00", 2) == 0) {
		enc = octstr_copy(request->sms.sender, 2, 
		    	    	  octstr_len(request->sms.sender));
		octstr_url_encode(enc);
		octstr_format_append(result, "%%2B%S", enc);
		octstr_destroy(enc);
	    } else {
		enc = octstr_duplicate(request->sms.sender);
    	    	octstr_url_encode(enc);
		octstr_append(result, enc);
		octstr_destroy(enc);
	    }
	    break;

	case 'q':
	    if (strncmp(octstr_get_cstr(request->sms.receiver),"00",2)==0) {
		enc = octstr_copy(request->sms.receiver, 2, 
		    	    	  octstr_len(request->sms.receiver));
		octstr_url_encode(enc);
		octstr_format_append(result, "%%2B%S", enc);
		octstr_destroy(enc);
	    } else {
		enc = octstr_duplicate(request->sms.receiver);
		octstr_url_encode(enc);
		octstr_append(result, enc);
		octstr_destroy(enc);
	    }
	    break;

	case 'a':
	    for (j = 0; j < num_words; ++j) {
		enc = octstr_duplicate(list_get(word_list, j));
		octstr_url_encode(enc);
		if (j > 0)
		    octstr_append_char(result, '+');
		octstr_append(result, enc);
		octstr_destroy(enc);
	    }
	    break;

	case 'b':
	    enc = octstr_duplicate(request->sms.msgdata);
	    octstr_url_encode(enc);
	    octstr_append(result, enc);
	    octstr_destroy(enc);
	    break;

	case 't':
	    tm = gw_gmtime(request->sms.time);
	    octstr_format_append(result, "%04d-%02d-%02d+%02d:%02d:%02d",
				 tm.tm_year + 1900,
				 tm.tm_mon + 1,
				 tm.tm_mday,
				 tm.tm_hour,
				 tm.tm_min,
				 tm.tm_sec);
	    break;

	case 'T':
	    if (request->sms.time == -1)
		break;
	    octstr_format_append(result, "%ld", request->sms.time);
	    break;

	case 'i':
	    if (request->sms.smsc_id == NULL)
		break;
	    enc = octstr_duplicate(request->sms.smsc_id);
	    octstr_url_encode(enc);
	    octstr_append(result, enc);
	    octstr_destroy(enc);
	    break;

	case 'I':
	    if (request->sms.id == -1)
		break;
	    octstr_format_append(result, "%ld", request->sms.id);
	    break;

	case 'n':
	    if (request->sms.service == NULL)
		break;
	    enc = octstr_duplicate(request->sms.service);
	    octstr_url_encode(enc);
	    octstr_append(result, enc);
	    octstr_destroy(enc);
	    break;

	case 'd':
	    enc = octstr_create("");
	    octstr_append_decimal(enc, request->sms.dlr_mask);
	    octstr_url_encode(enc);
	    octstr_append(result, enc);
	    octstr_destroy(enc);
	    break;

	case 'A':
	    enc = octstr_duplicate(reply);
	    octstr_url_encode(enc);
	    octstr_append(result, enc);
	    octstr_destroy(enc);
	    break;

	case 'c':
	    octstr_append_decimal(result, request->sms.coding);
	    break;

	case 'C':
	    if(octstr_len(request->sms.charset)) {
		octstr_append(result, request->sms.charset);
	    } else {
		switch (request->sms.coding) {
		    case DC_UNDEF:
		    case DC_7BIT:
			octstr_append(result, octstr_imm("ISO-8859-1"));
			break;
		    case DC_8BIT:
			octstr_append(result, octstr_imm("8-BIT"));
			break;
		    case DC_UCS2:
			octstr_append(result, octstr_imm("UTF16-BE"));
			break;
		}
	    }
	    break;

	case 'u':
	    if(octstr_len(request->sms.udhdata)) {
		enc = octstr_duplicate(request->sms.udhdata);
		octstr_url_encode(enc);
		octstr_append(result, enc);
		octstr_destroy(enc);
	    }
	    break;

	case 'B':  /* billing identifier/information */
	    if (octstr_len(request->sms.binfo)) {
            enc = octstr_duplicate(request->sms.binfo);
            octstr_url_encode(enc);
            octstr_append(result, enc);
            octstr_destroy(enc);
        }
        break;

	/* XXX sms.parameters not present in here:
	 *   * pid - will we receive this ? 
	 *   * mwi,mclass - will we receive these bits from smsc ?
	 *   * alt-dcs - shouldn't be required unless we want to inform 
	 *               which alt-dcs external server should use back
	 *   * compress - if we use compression, probably kannel would 
	 *                decompress and reset this to 0. not required
	 *   * validity, deferred, rpi - we don't receive these from smsc
	 *   * username, password, dlr-url, account - nonsense to send
	 */

	case '%':
	    octstr_format_append(result, "%%");
	    break;

	default:
	    octstr_format_append(result, "%%%c",
	    	    	    	 octstr_get_char(pattern, pos + 1));
	    break;
	}

	pos += 2;
    }
    /*
     * this SHOULD be done in smsbox, not here, but well,
     * much easier to do here
     */
    if (t && (t->type == TRANSTYPE_POST_URL || t->type == TRANSTYPE_POST_XML)
		    && t->strip_keyword)
	strip_keyword(request);

    octstr_destroy(url);
    octstr_destroy(reply);

    list_destroy(word_list, octstr_destroy_item);
    return result;
}


int urltrans_type(URLTranslation *t) 
{
    return t->type;
}

Octstr *urltrans_prefix(URLTranslation *t) 
{
    return t->prefix;
}

Octstr *urltrans_suffix(URLTranslation *t) 
{
    return t->suffix;
}

Octstr *urltrans_default_sender(URLTranslation *t) 
{
    return t->default_sender;
}

Octstr *urltrans_faked_sender(URLTranslation *t) 
{
    return t->faked_sender;
}

int urltrans_max_messages(URLTranslation *t) 
{
    return t->max_messages;
}

int urltrans_concatenation(URLTranslation *t) 
{
    return t->concatenation;
}

Octstr *urltrans_split_chars(URLTranslation *t) 
{
    return t->split_chars;
}

Octstr *urltrans_split_suffix(URLTranslation *t) 
{
    return t->split_suffix;
}

int urltrans_omit_empty(URLTranslation *t) 
{
    return t->omit_empty;
}

Octstr *urltrans_header(URLTranslation *t) 
{
    return t->header;
}

Octstr *urltrans_footer(URLTranslation *t) 
{
    return t->footer;
}

Octstr *urltrans_name(URLTranslation *t) 
{
    return t->name;
}

Octstr *urltrans_username(URLTranslation *t) 
{
    return t->username;
}

Octstr *urltrans_password(URLTranslation *t) 
{
    return t->password;
}

Octstr *urltrans_forced_smsc(URLTranslation *t) 
{
    return t->forced_smsc;
}

Octstr *urltrans_default_smsc(URLTranslation *t) 
{
    return t->default_smsc;
}

Octstr *urltrans_allow_ip(URLTranslation *t) 
{
    return t->allow_ip;
}

Octstr *urltrans_deny_ip(URLTranslation *t) 
{
    return t->deny_ip;
}

Octstr *urltrans_allowed_prefix(URLTranslation *t) 
{
    return t->allowed_prefix;
}

Octstr *urltrans_denied_prefix(URLTranslation *t) 
{
    return t->denied_prefix;
}

Octstr *urltrans_allowed_recv_prefix(URLTranslation *t) 
{
    return t->allowed_recv_prefix;
}

Octstr *urltrans_denied_recv_prefix(URLTranslation *t) 
{
    return t->denied_recv_prefix;
}

Numhash *urltrans_white_list(URLTranslation *t)
{
    return t->white_list;
}

Numhash *urltrans_black_list(URLTranslation *t)
{
    return t->black_list;
}

int urltrans_assume_plain_text(URLTranslation *t) 
{
    return t->assume_plain_text;
}

int urltrans_accept_x_kannel_headers(URLTranslation *t) 
{
    return t->accept_x_kannel_headers;
}

int urltrans_strip_keyword(URLTranslation *t) 
{
    return t->strip_keyword;
}

int urltrans_send_sender(URLTranslation *t) 
{
    return t->send_sender;
}



/***********************************************************************
 * Internal functions.
 */


/*
 * Create one URLTranslation. Return NULL for failure, pointer to it for OK.
 */
static URLTranslation *create_onetrans(CfgGroup *grp)
{
    URLTranslation *ot;
    Octstr *aliases, *url, *post_url, *post_xml, *text, *file, *exec;
    Octstr *accepted_smsc, *forced_smsc, *default_smsc;
    Octstr *grpname, *sendsms_user, *sms_service;
    int is_sms_service;
    
    grpname = cfg_get_group_name(grp);
    if (grpname == NULL)
    	return NULL;

    sms_service = octstr_imm("sms-service");
    sendsms_user = octstr_imm("sendsms-user");
    if (octstr_compare(grpname, sms_service) == 0)
    	is_sms_service = 1;
    else if (octstr_compare(grpname, sendsms_user) == 0)
    	is_sms_service = 0;
    else {
	octstr_destroy(grpname);
	return NULL;
    }
    octstr_destroy(grpname);

    ot = gw_malloc(sizeof(URLTranslation));

    ot->keyword = NULL;
    ot->aliases = NULL;
    ot->pattern = NULL;
    ot->prefix = NULL;
    ot->suffix = NULL;
    ot->faked_sender = NULL;
    ot->default_sender = NULL;
    ot->split_chars = NULL;
    ot->split_suffix = NULL;
    ot->footer = NULL;
    ot->header = NULL;
    ot->name = NULL;
    ot->username = NULL;
    ot->password = NULL;
    ot->omit_empty = 0;
    ot->accepted_smsc = NULL;
    ot->forced_smsc = NULL;
    ot->default_smsc = NULL;
    ot->allow_ip = NULL;
    ot->deny_ip = NULL;
    ot->allowed_prefix = NULL;
    ot->denied_prefix = NULL;
    ot->allowed_recv_prefix = NULL;
    ot->denied_recv_prefix = NULL;
    ot->white_list = NULL;
    ot->black_list = NULL;
    
    if (is_sms_service) {
	cfg_get_bool(&ot->catch_all, grp, octstr_imm("catch-all"));

	ot->dlr_url = cfg_get(grp, octstr_imm("dlr-url"));
	    
	url = cfg_get(grp, octstr_imm("get-url"));
	if (url == NULL)
	    url = cfg_get(grp, octstr_imm("url"));
	    
	post_url = cfg_get(grp, octstr_imm("post-url"));
	post_xml = cfg_get(grp, octstr_imm("post-xml"));
	file = cfg_get(grp, octstr_imm("file"));
	text = cfg_get(grp, octstr_imm("text"));
	exec = cfg_get(grp, octstr_imm("exec"));
	if (url != NULL) {
	    ot->type = TRANSTYPE_GET_URL;
	    ot->pattern = url;
	} else if (post_url != NULL) {
	    ot->type = TRANSTYPE_POST_URL;
	    ot->pattern = post_url;
	    ot->catch_all = 1;
	} else if (post_xml != NULL) {
	    ot->type = TRANSTYPE_POST_XML;
	    ot->pattern = post_xml;
	    ot->catch_all = 1;
	} else if (file != NULL) {
	    ot->type = TRANSTYPE_FILE;
	    ot->pattern = file;
	} else if (text != NULL) {
	    ot->type = TRANSTYPE_TEXT;
	    ot->pattern = text;
	} else if (exec != NULL) {
	    ot->type = TRANSTYPE_EXECUTE;
	    ot->pattern = exec;
	} else {
	    error(0, "Configuration group `sms-service' "
	    	     "did not specify get-url, post-url, post-xml, file or text.");
    	    goto error;
	}

	ot->keyword = cfg_get(grp, octstr_imm("keyword"));
	if (ot->keyword == NULL) {
	    error(0, "Group 'sms-service' must include 'keyword'.");
	    goto error;
	}
	octstr_convert_range(ot->keyword, 0, octstr_len(ot->keyword), 
	    	    	     tolower);

	ot->name = cfg_get(grp, octstr_imm("name"));
	if (ot->name == NULL)
	    ot->name = octstr_duplicate(ot->keyword);

	aliases = cfg_get(grp, octstr_imm("aliases"));
	if (aliases == NULL)
	    ot->aliases = list_create();
	else {
	    long i;
	    Octstr *os;

	    ot->aliases = octstr_split(aliases, octstr_imm(";"));
	    octstr_destroy(aliases);
	    for (i = 0; i < list_len(ot->aliases); ++i) {
		os = list_get(ot->aliases, i);
	    	octstr_convert_range(os, 0, octstr_len(os), tolower);
	    }
	}

	accepted_smsc = cfg_get(grp, octstr_imm("accepted-smsc"));
	if (accepted_smsc != NULL) {
	    ot->accepted_smsc = octstr_split(accepted_smsc, octstr_imm(";"));
	    octstr_destroy(accepted_smsc);
	}

	cfg_get_bool(&ot->assume_plain_text, grp, 
		     octstr_imm("assume-plain-text"));
	cfg_get_bool(&ot->accept_x_kannel_headers, grp, 
		     octstr_imm("accept-x-kannel-headers"));
	cfg_get_bool(&ot->strip_keyword, grp, octstr_imm("strip-keyword"));
	cfg_get_bool(&ot->send_sender, grp, octstr_imm("send-sender"));
	
	ot->prefix = cfg_get(grp, octstr_imm("prefix"));
	ot->suffix = cfg_get(grp, octstr_imm("suffix"));

	ot->allowed_recv_prefix = cfg_get(grp, octstr_imm("allowed-receiver-prefix"));
    ot->denied_recv_prefix = cfg_get(grp, octstr_imm("denied-receiver-prefix"));

	ot->args = count_occurences(ot->pattern, octstr_imm("%s"));
	ot->args += count_occurences(ot->pattern, octstr_imm("%S"));
	ot->has_catchall_arg = 
	    (count_occurences(ot->pattern, octstr_imm("%r")) > 0) ||
	    (count_occurences(ot->pattern, octstr_imm("%a")) > 0);

    } else {
	ot->type = TRANSTYPE_SENDSMS;
	ot->pattern = octstr_create("");
	ot->args = 0;
	ot->has_catchall_arg = 0;
	ot->catch_all = 1;
	ot->username = cfg_get(grp, octstr_imm("username"));
	ot->password = cfg_get(grp, octstr_imm("password"));
	ot->dlr_url = cfg_get(grp, octstr_imm("dlr-url"));
	if (ot->password == NULL) {
	    error(0, "Password required for send-sms user");
	    goto error;
	}
	ot->name = cfg_get(grp, octstr_imm("name"));
	if (ot->name == NULL)
	    ot->name = octstr_duplicate(ot->username);

	forced_smsc = cfg_get(grp, octstr_imm("forced-smsc"));
	default_smsc = cfg_get(grp, octstr_imm("default-smsc"));
	if (forced_smsc != NULL) {
	    if (default_smsc != NULL) {
		info(0, "Redundant default-smsc for send-sms user %s", 
		     octstr_get_cstr(ot->username));
	    }
	    ot->forced_smsc = forced_smsc;
	    octstr_destroy(default_smsc);
	} else  if (default_smsc != NULL)
	    ot->default_smsc = default_smsc;

	ot->deny_ip = cfg_get(grp, octstr_imm("user-deny-ip"));
	ot->allow_ip = cfg_get(grp, octstr_imm("user-allow-ip"));
	ot->default_sender = cfg_get(grp, octstr_imm("default-sender"));

    }
    ot->allowed_prefix = cfg_get(grp, octstr_imm("allowed-prefix"));
    ot->denied_prefix = cfg_get(grp, octstr_imm("denied-prefix"));
    {
	Octstr *os;
	os = cfg_get(grp, octstr_imm("white-list"));
	if (os != NULL) {
	    ot->white_list = numhash_create(octstr_get_cstr(os));
	    octstr_destroy(os);
	}
	os = cfg_get(grp, octstr_imm("black-list"));
	if (os != NULL) {
	    ot->black_list = numhash_create(octstr_get_cstr(os));
	    octstr_destroy(os);
	}
    }

    if (cfg_get_integer(&ot->max_messages, grp, 
    	    	    	octstr_imm("max-messages")) == -1)
	ot->max_messages = 1;
    cfg_get_bool(&ot->concatenation, grp, 
		 octstr_imm("concatenation"));
    cfg_get_bool(&ot->omit_empty, grp, 
		 octstr_imm("omit-empty"));
    
    ot->header = cfg_get(grp, octstr_imm("header"));
    ot->footer = cfg_get(grp, octstr_imm("footer"));
    ot->faked_sender = cfg_get(grp, octstr_imm("faked-sender"));
    ot->split_chars = cfg_get(grp, octstr_imm("split-chars"));
    ot->split_suffix = cfg_get(grp, octstr_imm("split-suffix"));

    if ( (ot->prefix == NULL && ot->suffix != NULL) ||
	 (ot->prefix != NULL && ot->suffix == NULL) ) {
	warning(0, "Service <%s>: suffix and prefix are only used"
		   " if both are set.", octstr_get_cstr(ot->keyword));
    }
    if ((ot->prefix != NULL || ot->suffix != NULL) &&
        ot->type != TRANSTYPE_GET_URL) {
	warning(0, "Service <%s>: suffix and prefix are only used"
                   " if type is 'get-url'.", octstr_get_cstr(ot->keyword));
    }
    
    return ot;

error:
    error(0, "Couldn't create a URLTranslation.");
    destroy_onetrans(ot);
    return NULL;
}


/*
 * Free one URLTranslation.
 */
static void destroy_onetrans(void *p) 
{
    URLTranslation *ot;
    
    ot = p;
    if (ot != NULL) {
	octstr_destroy(ot->keyword);
	list_destroy(ot->aliases, octstr_destroy_item);
	octstr_destroy(ot->dlr_url);
	octstr_destroy(ot->pattern);
	octstr_destroy(ot->prefix);
	octstr_destroy(ot->suffix);
	octstr_destroy(ot->default_sender);
	octstr_destroy(ot->faked_sender);
	octstr_destroy(ot->split_chars);
	octstr_destroy(ot->split_suffix);
	octstr_destroy(ot->header);
	octstr_destroy(ot->footer);
	list_destroy(ot->accepted_smsc, octstr_destroy_item);
	octstr_destroy(ot->name);
	octstr_destroy(ot->username);
	octstr_destroy(ot->password);
	octstr_destroy(ot->forced_smsc);
	octstr_destroy(ot->default_smsc);
	octstr_destroy(ot->allow_ip);
	octstr_destroy(ot->deny_ip);
	octstr_destroy(ot->allowed_prefix);
	octstr_destroy(ot->denied_prefix);
	octstr_destroy(ot->allowed_recv_prefix);
	octstr_destroy(ot->denied_recv_prefix);
	numhash_destroy(ot->white_list);
	numhash_destroy(ot->black_list);
	gw_free(ot);
    }
}


/*
 * Find the appropriate translation 
 */
static URLTranslation *find_translation(URLTranslationList *trans, 
	List *words, Octstr *smsc, Octstr *sender, Octstr *receiver, int *reject)
{
    Octstr *keyword;
    int i, n;
    URLTranslation *t;
    List *list;
    
    n = list_len(words);
    if (n == 0)
	return NULL;
    keyword = list_get(words, 0);
    keyword = octstr_duplicate(keyword);
    octstr_convert_range(keyword, 0, octstr_len(keyword), tolower);
    
    list = dict_get(trans->dict, keyword);
    t = NULL;
    for (i = 0; i < list_len(list); ++i) {
	t = list_get(list, i);

	/* if smsc_id set and accepted_smsc exist, accept
	 * translation only if smsc id is in accept string
	 */
	if (smsc && t->accepted_smsc) {
	    if (!list_search(t->accepted_smsc, smsc, octstr_item_match)) {
		t = NULL;
		continue;
	    }
	}

	/* Have allowed for sender */
	if (t->allowed_prefix && ! t->denied_prefix &&
	   (does_prefix_match(t->allowed_prefix, sender) != 1)) {
	    t = NULL;
	    continue;
	}

	/* Have denied for sender */
	if (t->denied_prefix && ! t->allowed_prefix &&
	   (does_prefix_match(t->denied_prefix, sender) == 1)) {
	    t = NULL;
	    continue;
	}

	/* Have allowed for receiver */
	if (t->allowed_recv_prefix && ! t->denied_recv_prefix &&
	   (does_prefix_match(t->allowed_recv_prefix, receiver) != 1)) {
	    t = NULL;
	    continue;
	}

	/* Have denied for receiver */
	if (t->denied_recv_prefix && ! t->allowed_recv_prefix &&
	   (does_prefix_match(t->denied_recv_prefix, receiver) == 1)) {
	    t = NULL;
	    continue;
	}

	if (t->white_list &&
	    numhash_find_number(t->white_list, sender) < 1) {
	    t = NULL; *reject = 1;
	    continue;
	}   
	if (t->black_list &&
	    numhash_find_number(t->black_list, sender) == 1) {
	    t = NULL; *reject = 1;
	    continue;
	}   

	/* Have allowed and denied */
	if (t->denied_prefix && t->allowed_prefix &&
	   (does_prefix_match(t->allowed_prefix, sender) != 1) &&
	   (does_prefix_match(t->denied_prefix, sender) == 1) ) {
	    t = NULL;
	    continue;
	}

	if (t->catch_all)
	    break;

	if (n - 1 == t->args)
	    break;
	if (t->has_catchall_arg && n - 1 >= t->args)
	    break;
	t = NULL;
    }

    /* Only return reject if there's only blacklisted smsc's */
    if(t != NULL)
	*reject = 0;

    octstr_destroy(keyword);    
    return t;
}


static URLTranslation *find_default_translation(URLTranslationList *trans,
						Octstr *smsc, Octstr *sender, Octstr *receiver,
						int *reject)
{
    URLTranslation *t;
    int i;
    List *list;

    *reject = 0;

    list = dict_get(trans->dict, octstr_imm("default"));
    t = NULL;
    for (i = 0; i < list_len(list); ++i) {
	t = list_get(list, i);
	if (smsc && t->accepted_smsc) {
	    if (!list_search(t->accepted_smsc, smsc, octstr_item_match)) {
		t = NULL;
		continue;
	    }
	}

	/* Have allowed sender */
	if (t->allowed_prefix && ! t->denied_prefix &&
		       	(does_prefix_match(t->allowed_prefix, sender) != 1)) {
	    t = NULL;
	    continue;
	}

	/* Have denied sender */
	if (t->denied_prefix && ! t->allowed_prefix &&
		       	(does_prefix_match(t->denied_prefix, sender) == 1)) {
	    t = NULL;
	    continue;
	}

	/* Have allowed receiver */
	if (t->allowed_recv_prefix && ! t->denied_recv_prefix &&
		       	(does_prefix_match(t->allowed_recv_prefix, receiver) != 1)) {
	    t = NULL;
	    continue;
	}

	/* Have denied receiver */
	if (t->denied_recv_prefix && ! t->allowed_recv_prefix &&
		       	(does_prefix_match(t->denied_recv_prefix, receiver) == 1)) {
	    t = NULL;
	    continue;
	}

	if (t->white_list && numhash_find_number(t->white_list, sender) < 1) {
	    t = NULL; *reject = 1;
	    continue;
	}
	if (t->black_list && numhash_find_number(t->black_list, sender) == 1) {
	    t = NULL; *reject = 1;
	    continue;
										        }

	/* Have allowed and denied */
	if (t->denied_prefix && t->allowed_prefix &&
		       	(does_prefix_match(t->allowed_prefix, sender) != 1) &&
		       	(does_prefix_match(t->denied_prefix, sender) == 1) ) {
	    t = NULL;
	    continue;
	}
	break;
    }

    /* Only return reject if there's only blacklisted smsc's */
    if(t != NULL)
	*reject = 0;
    return t;
}

static URLTranslation *find_black_list_translation(URLTranslationList *trans,
						Octstr *smsc)
{
    URLTranslation *t;
    int i;
    List *list;

    list = dict_get(trans->dict, octstr_imm("black-list"));
    t = NULL;
    for (i = 0; i < list_len(list); ++i) {
	t = list_get(list, i);
	if (smsc && t->accepted_smsc) {
	    if (!list_search(t->accepted_smsc, smsc, octstr_item_match)) {
		t = NULL;
		continue;
	    }
	}
	break;
    }
    return t;
}


/*
 * Count the number of times `pat' occurs in `str'.
 */
static long count_occurences(Octstr *str, Octstr *pat)
{
    long count;
    long pos;
    long len;
    
    count = 0;
    pos = 0;
    len = octstr_len(pat);
    while ((pos = octstr_search(str, pat, pos)) != -1) {
    	++count;
	pos += len;
    }
    return count;
}

