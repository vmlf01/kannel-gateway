/*
 * xml_shared.h - Common xml tokenizer interface
 * This file contains mainly character set functions.
 *
 * Tuomas Luttinen for Wapit Ltd.
 */

#ifndef XML_SHARED_H
#define XML_SHARED_H

typedef struct charset_t charset_t;

#include "gwlib/gwlib.h"

/*
 * Prototypes of common functions
 *
 * set_charset - set the charset of the http headers into the document, if 
 * it has no encoding set.
 */
void set_charset(Octstr *document, Octstr *charset);

/*
 * element_check_content - a helper function for checking if an element has 
 * content or attributes. Returns status bit for attributes (0x80) and another
 * for content (0x40) added into one octet.
 */
unsigned char element_check_content(xmlNodePtr node);

/*
 * only_blanks - checks if a text node contains only white space, when it can 
 * be left out as a element content.
 */

int only_blanks(const char *text);

/*
 * Parses the character set of the document. 
 */
int parse_charset(Octstr *charset);

List *wml_charsets(void);

/*
 * Macro for creating an octet string from a node content. This has two 
 * versions for different libxml node content implementation methods. 
 */

#ifdef XML_USE_BUFFER_CONTENT
#define create_octstr_from_node(node) (octstr_create(node->content->content))
#else
#define create_octstr_from_node(node) (octstr_create(node->content))
#endif

#endif








