/*
 * urltrans.h - URL translations
 *
 * The SMS gateway receives service requests sent as SMS messages and uses
 * a web server to actually perform the requests. The first word of the
 * SMS message usually specifies the service, and for each service there is
 * a URL that specifies the web page or cgi-bin that performs the service. 
 * Thus, in effect, the gateway `translates' SMS messages to URLs.
 *
 * urltrans.h and urltrans.c implement a data structure for holding a list
 * of translations and formatting a SMS request into a URL. It is used as
 * follows:
 *
 * 1. Create a URLTranslation object with urltrans_create.
 * 2. Add translations into it with urltrans_add_one or urltrans_add_cfg.
 * 3. Receive SMS messages, and translate them into URLs with urltrans_get_url.
 * 4. When you are done, free the object with urltrans_destroy.
 *
 * See below for more detailed instructions for using the functions.
 *
 * Lars Wirzenius for WapIT Ltd.
 */

#ifndef URLTRANS_H
#define URLTRANS_H

#include "gwlib/gwlib.h"
#include "msg.h"

/*
 * This is the data structure that holds the list of translations. It is
 * opaque and is defined in and usable only within urltrans.c.
 */
typedef struct URLTranslationList URLTranslationList;


/*
 * This is the data structure that holds one translation. It is also
 * opaque, and is accessed via some of the functions below.
 */
typedef struct URLTranslation URLTranslation;

enum {
    TRANSTYPE_URL = 0,
    TRANSTYPE_TEXT = 1,
    TRANSTYPE_FILE = 2,
    TRANSTYPE_SENDSMS =3
};

/*
 * Create a new URLTranslationList object. Return NULL if the creation failed,
 * or a pointer to the object if it succeded.
 *
 * The object is empty: it contains no translations.
 */
URLTranslationList *urltrans_create(void);


/*
 * Destroy a URLTranslationList object.
 */
void urltrans_destroy(URLTranslationList *list);


/*
 * Add a translation to the object. The group is parsed internally.
 *
 * There can be several patterns for the same keyword, but with different
 * patterns. urltrans_get_url will pick the pattern that best matches the
 * actual SMS message. (See urltrans_get_pattern for a description of the
 * algorithm.)
 *
 * There can only be one pattern with keyword "default", however.
 *
 * Sendsms-translations do not use keyword. Instead they use username and
 * password
 *
 * Return -1 for error, or 0 for OK.
 */
int urltrans_add_one(URLTranslationList *trans, ConfigGroup *grp);

/*
 * Add translations to a URLTranslation object from a Config object
 * (see config.h). Translations are added from groups in `cfg' that
 * contain variables called "keyword" and "url". For each such group,
 * urltrans_add_one is called.
 *
 * Return -1 for error, 0 for OK. If -1 is returned, the URLTranslation
 * object may have been partially modified.
 */
int urltrans_add_cfg(URLTranslationList *trans, Config *cfg);


/*
 * Find the translation that corresponds to a given text string
 *
 * Use the translation with pattern whose keyword is the same as the first
 * word of the text and that has the number of `%s' fields as the text
 * has words after the first one. If no such pattern exists, use the
 * pattern whose keyword is "default". If there is no such pattern, either,
 * return NULL.
 *
 * If 'smsc' is set, only accept translation with no 'accepted-smsc' set or
 * with matching smsc in that list.
 */
URLTranslation *urltrans_find(URLTranslationList *trans, Octstr *text, Octstr *smsc);

/*
 * find matching URLTranslation for the given 'username', or NULL
 * if not found. Password must be checked afterwards
 */
URLTranslation *urltrans_find_username(URLTranslationList *trans, char *name);

/*
 * Return a pattern given contents of an SMS message. Find the appropriate
 * translation pattern and fill in the missing parts from the contents of
 * the SMS message.
 *
 * `sms' is the SMS message that is being translated.
 *
 * Return NULL if there is a failure. Otherwise, return a pointer to the
 * pattern, which is stored in dynamically allocated memory that the
 * caller should free when the pattern is no longer needed.
 *
 * The pattern is URL, fixed text or file name according to type of urltrans
 */
char *urltrans_get_pattern(URLTranslation *t, Msg *sms);

/*
 * Return the type of the translation, see enumeration above
 */
int urltrans_type(URLTranslation *t);

/*
 * Return prefix and suffix of translations, if they have been set.
 */
char *urltrans_prefix(URLTranslation *t);
char *urltrans_suffix(URLTranslation *t);

/*
 * Return (a recommended) faked sender number, or NULL if not set.
 */
char *urltrans_faked_sender(URLTranslation *t);

/*
 * Return maximum number of SMS messages that should be generated from
 * the web page directed by the URL translation.
 */
int urltrans_max_messages(URLTranslation *t);

/*
 * Return the concatenation status for SMS messages that should be generated from
 * the web page directed by the URL translation. (1=enabled)
 */
int urltrans_concatenation(URLTranslation *t);

/*
 * Return (recommended) delimiter characters when splitting long
 * replies into several messages
 */
char *urltrans_split_chars(URLTranslation *t);

/*
 * return a string that should be added after each sms message if it is
 * except for the last one.
 */
char *urltrans_split_suffix(URLTranslation *t);

/*
 * Return if set that should not send 'empty reply' messages
 */
int urltrans_omit_empty(URLTranslation *t);

/*
 * return a string that should be inserted to each SMS, if any
 */
char *urltrans_header(URLTranslation *t);

/*
 * return a string that should be appended to each SMS, if any
 */
char *urltrans_footer(URLTranslation *t);

/*
 * return the username or password string, or NULL if not set
 * (used only with TRANSTYPE_SENDSMS)
 */
char *urltrans_username(URLTranslation *t);
char *urltrans_password(URLTranslation *t);

/* Return forced smsc ID for send-sms user, if set */
char *urltrans_forced_smsc(URLTranslation *t);

/* Return default smsc ID for send-sms user, if set */
char *urltrans_default_smsc(URLTranslation *t);

/* Return list of accepted SMSC IDs, if set */
char *urltrans_accepted_smsc(URLTranslation *t);

/* Return allow and deny IP strings, if set. */
char *urltrans_allow_ip(URLTranslation *t);
char *urltrans_deny_ip(URLTranslation *t);

#endif
