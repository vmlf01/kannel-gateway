/*
 * wsp_headers.c - Implement WSP PDU headers
 *
 * Kalle Marjola <rpr@wapit.com>
 */

#include "gwlib.h"
#include "wsp.h"

/*
 * some predefined well-known assignments
 */

static char *WSPHeaderFieldNameAssignment[] = {
    "Accept",			/* 0x00 */
    "Accept-Charset",
    "Accept-Encoding",
    "Accept-Language",
    "Accept-Ranges",
    "Age", 
    "Allow",
    "Authorization",
    "Cache-Control",		/* 0x08 */
    "Connection",
    "Content-Base",
    "Content-Encoding",
    "Content-Language",
    "Content-Length",
    "Content-Location",
    "Content-MD5",
    "Content-Range",		/* 0x10 */
    "Content-Type",
    "Date",
    "Etag",
    "Expires",
    "From",
    "Host",
    "If-Modified-Since",
    "If-Match",			/* 0x18 */
    "If-None-Match",
    "If-Range",
    "If-Unmodified-Since",
    "Location",
    "Last-Modified",
    "Max-Forwards",
    "Pragma",
    "Proxy-Authenticate",	/* 0x20 */
    "Proxy-Authorization",
    "Public",
    "Range",
    "Referer",
    "Retry-After",
    "Server",
    "Transfer-Encoding",
    "Upgrade",			/* 0x28 */
    "User-Agent",
    "Vary",
    "Via",
    "Warning",
    "WWW_Authenticate",
    "Content-Disposition"	/* 0x2E */
};
#define WSP_PREDEFINED_LAST_FIELDNAME	0x2F


static char *WSPContentTypeAssignment[] = {
    "*/*",			/* 0x00 */
    "text/*",
    "text/html",
    "text/plain",
    "text/x-hdml",
    "text/x-ttml",
    "text/x-vCalendar",
    "text/x-vCard",
    "text/vnd.wap.wml",		/* 0x08 */
    "text/vnd.wap.wmlscript",
    "application/vnd.wap.catc",
    "Multipart/*",
    "Multipart/mixed",
    "Multipart/form-data",
    "Multipart/byteranges",
    "multipart/alternative",
    "application/*",		/* 0x10 */
    "application/java-wm",
    "application/x-www-form-urlencoded",
    "application/x-hdmlc",
    "application/vnd.wap.wmlc",
    "application/vnd.wap.wmlscriptc",
    "application/vnd.wap.wsic",
    "application/vnd.wap.uaprof",
    "application/vnd.wap.wtls-ca-certificate",
    "application/vnd.wap.wtls-user-certificate",
    "application/x-x509-ca-cert",
    "application/x-x509-user-cert",
    "image/*",
    "image/gif",
    "image/jpeg",
    "image/tiff",
    "image/png",			/* 0x20 */
    "image/vnd.wap.wbmp",
    "application/vnd.wap.multipart.*",
    "application/vnd.wap.multipart.mixed",
    "application/vnd.wap.multipart.form-data",
    "application/vnd.wap.multipart.byteranges",
    "application/vnd.wap.multipart.alternative",
    "application/xml",
    "text/xml",
    "application/vnd.wap.wbxml",
    ""					/* 0x2A - */
};
#define WSP_PREDEFINED_LAST_CONTENTTYPE	0x29


static char *WSPCharacterSetAssignment[] = {
    "0x00",
    "0x01",
    "0x02",
    "us-ascii",
    "iso-8859-1",
    "iso-8859-2",
    "iso-8859-3",
    "iso-8859-4",
    "iso-8859-5",
    "iso-8859-6",
    "iso-8859-7",	/* 0x0A */
    "iso-8859-8",
    "iso-8859-9",
    ""
};
#define WSP_PREDEFINED_LAST_CHARSET 	0x0C

/* hard-coded into header parsing:
 *  0x11, 0x6A, 0x03E8, 0x07EA
 */



#define WSP_FIELD_VALUE_NUL_STRING	1
#define WSP_FIELD_VALUE_ENCODED 	2
#define WSP_FIELD_VALUE_DATA		3

/*
 * get field value and return irs type as predefined data types
 * Modify all arguments except for 'str'
 *
 * offset: set to end of unpacked data
 * well_known_value: set to well-known-value, if present (_VALUE_ENCODED)
 * data_offset: start of the data, unless well-known
 * data_len: total length of the data in data_offset location. Includes
 *         terminating NUL if data type is VALUE_NUL_STRING
 */
static int field_value(Octstr *str, int	*offset, int *well_known_value,
		       int *data_offset, unsigned long *data_len)
{
    int val;
    val = octstr_get_char(str, *offset);
    if (val < 31) {
	*well_known_value = -1;
	*data_offset = *offset + 1;
	*data_len = val;
	*offset += 1+val;
	return WSP_FIELD_VALUE_DATA;
    }
    else if (val == 31) {
	int len;
	*well_known_value = -1;
	(*offset)++;
	*data_len = get_variable_value(octstr_get_cstr(str)+ *offset + 1, &len);
	*data_offset = *offset + 1 + len;
	*offset = *data_offset + *data_len;
	return WSP_FIELD_VALUE_DATA;
    }
    else if (val > 127) {
	*well_known_value = val - 0x80;
	*data_offset = *offset;
	*data_len = 1;
	(*offset)++;
	return WSP_FIELD_VALUE_ENCODED;
    }
    else {
	*well_known_value = -1;
	*data_offset = *offset;
	*data_len = strlen(octstr_get_cstr(str)+*offset) + 1;
	*offset += *data_len;
	return WSP_FIELD_VALUE_NUL_STRING;
    }
}


static char *encoded_language(int val, int val2)
{
    char *ch;

    if (val2 >= 0) {
	if (val==0x07 && val2==0xEA)
	    ch = "big5";
	else if (val==0x03 && val2==0xE8)
	    ch = "iso-10646-ucs-2";
	else
	    ch = "unknown";
    } else {
	if (val <= WSP_PREDEFINED_LAST_CHARSET)
	    ch = WSPCharacterSetAssignment[val];
	else if (val == 0x6A)
	    ch = "utf-8";
	else
	    ch = "non-assigned";
    }
    return ch;
}
    

static char *encode_language_str(Octstr *str, char *buf, int off, int data_len)
{
    char *ch;
    unsigned long len;
    int ret, val, data_off, end;
    end = off;
    
    ret = field_value(str, &end, &val, &data_off, &len);
    if (ret == WSP_FIELD_VALUE_ENCODED) {
	ch = encoded_language(val, -1);
    }
    else if (ret == WSP_FIELD_VALUE_DATA) {
	ch = encoded_language(octstr_get_char(str, data_off),
			      octstr_get_char(str, data_off+1));
    } else {
	ch = "Unknown";	/* should not happen */
    }
    if (data_off+len+1 == off+data_len) {
	val = octstr_get_char(str, data_off+len);
	sprintf(buf, "%s;q=%0.2f", ch, (float)((val-1)/100.0));
	ch = buf;
    } else if (data_off+len+2 == off+data_len) {
	sprintf(buf, "%s;q=?", ch);
	ch = buf;
    }
    return ch;
}


static HTTPHeader *decode_well_known_field(int field_type,
					   Octstr *headers, int *off)
{
    unsigned long len;
    int data_off, ret, val;
    char *ch, tmpbuf[1024];
    HTTPHeader *newhdr;
    ch = "";
    
    ret = field_value(headers, off, &val, &data_off, &len);

    /* We may take the NUL string immediately out of sequence
     * as it is easily decoded...
     */
    if (ret == WSP_FIELD_VALUE_NUL_STRING) {
	ch = octstr_get_cstr(headers)+data_off;
    } else {
	switch (field_type) {

	    /* new well-known-field-names are added to this list
	     */
	
	case 0x00:		/* Accept */ 
	    if (ret == WSP_FIELD_VALUE_ENCODED &&
		val <= WSP_PREDEFINED_LAST_CONTENTTYPE)

		ch = WSPContentTypeAssignment[val];
		  
	    else if (ret == WSP_FIELD_VALUE_DATA) {
		debug(0, "%s: accept-general-form not supported",
		      WSPHeaderFieldNameAssignment[field_type]);
		return NULL;
	    } else
		goto error;
	    break;
	case 0x01:		/* Accept-Charset */ 
	    if (ret == WSP_FIELD_VALUE_ENCODED) {
		ch = encoded_language(val, -1);
	    }
	    else if (ret == WSP_FIELD_VALUE_DATA) {
		ch = encode_language_str(headers, tmpbuf, data_off, len);
	    }
	    else
		ch = "?";
	    break;

	    /* case 0x02: accept-encoding not yet supported */
	
	case 0x03:		/* Accept-language */
	    if (ret == WSP_FIELD_VALUE_ENCODED) {
		if (val == 0x00) 	/* actually 0x80 */
		    ch = "*";	/* is this how it is marked? */
		else if (val == 0x16) ch = "de";
		else if (val == 0x19) ch = "en";
		else if (val == 0x1F) ch = "fi";
		else if (val == 0x70) ch = "sv";
		else {
		    debug(0, "Nonsupported language '0x%x'", val);
		    return NULL;
		}
	    } else
		ch = "Unsupported";

	    break;

	default:
	    if (field_type <= WSP_PREDEFINED_LAST_FIELDNAME) {
		debug(0, "Nonsupported field '0x%x'", field_type);
		return NULL;
	    } else
		goto error;
	}
    }
    newhdr = gw_malloc(sizeof(HTTPHeader));
    newhdr->next = NULL;
    newhdr->key = gw_strdup(WSPHeaderFieldNameAssignment[field_type]);
    newhdr->value = gw_strdup(ch);
    
    return newhdr;

error:
    warning(0, "Faulty header!");
    return NULL;
}


static HTTPHeader *decode_app_header(Octstr *str, int *off)
{
    int len;
    HTTPHeader *newhdr;
    newhdr = gw_malloc(sizeof(HTTPHeader));
    newhdr->next = NULL;
    
    len = strlen(octstr_get_cstr(str) + *off);

    newhdr->key = gw_strdup(octstr_get_cstr(str)+ *off);
    newhdr->value = gw_strdup(octstr_get_cstr(str)+ *off+len+1);
    
    *off += len + 2 + strlen(octstr_get_cstr(str)+*off+len+1);

    return newhdr;
}


HTTPHeader *unpack_headers(Octstr *headers)
{
    int off, byte;
    HTTPHeader *first, *prev, *val;
    
    off = 0;
    first = NULL;
    
    while(off < octstr_len(headers)) {
	byte = octstr_get_char(headers, off);
	val = NULL;
	
	if (byte == -1) {
	    warning(0, "read past header octet!");
	    break;
	} else if (byte == 127) {
	    debug(0, "Shift-delimiter encountered, IGNORED");
	    off += 2;	/* ignore page-identity */
	} else if (byte >= 1 && byte <= 31) {
	    debug(0, "Short-cut-shift-delimiter %d encountered, IGNORED", byte);
	    off++;
	}
	else if (byte >= 128) {  /* well-known-header */
	    off++;
	    val = decode_well_known_field(byte-0x80, headers, &off);
	} else if (byte > 31 && byte < 127) {
	    val = decode_app_header(headers, &off);
	} else {
	    warning(0, "Unsupported token/whatever header (start 0x%x)", byte);
	    break;
	}
	/*
	 * append new header
	 */
	if (val != NULL) {
	    if (first == NULL) first = prev = val;
	    else {
		prev->next = val;
		prev = val;
	    }
	}
    }
    return first;
}	


Octstr *output_headers(OctstrList *uhdrs)
{
    char buf[2*1024];
    char *pstr, *nstr;
    Octstr *ostr;
    int i;

    pstr = NULL;
    *buf = '\0';
    
    for(i=0; ; i+=2) {
	ostr = octstr_list_get(uhdrs, i);
	if (ostr == NULL)
	    break;
	
	nstr = octstr_get_cstr(ostr);
	if (pstr != NULL && strcmp(pstr, nstr) == 0) {
	    strcat(buf, ", ");
	} else {
	    if (pstr != NULL)
		strcat(buf, "\r\n");
	    strcat(buf, nstr);
	    strcat(buf, ": ");
	    pstr = nstr;
	}
	strcat(buf, octstr_get_cstr(octstr_list_get(uhdrs, i+1)));
    }
    strcat(buf, "\r\n");
    return octstr_create(buf);
}


