/*
 * wap_push_ppg_pushuser.h: Header of push user authentication module.
 *
 * Only WAP-165-PushArchOverview-19991108-a, an informal document, mentions
 * pi authentication. (See chapter 13.) So this is definitely left for 
 * implementors.
 * Basic authentication is defined in rfc 2617. Note that https connections
 * are handled by our http module.
 *
 * By Aarno Syv�nen for Wiral Ltd
 */

#ifndef WAP_PUSH_PPG_PUSHUSER_H
#define WAP_PUSH_PPG_PUSHUSER_H

#include "gwlib/gwlib.h"

/*
 * This function initializes the module and push users data stucture, contain-
 * ing authentication data for all push user accounts. This function MUST be
 * called before any other functions of this module.
 */
int wap_push_ppg_pushuser_list_add(List *l, long number_of_pushes, 
                                   long number_of_users);

/*
 * This function does clean up for module shutdown. This module MUST be called
 * when the caller of this module is shut down.
 */
void wap_push_ppg_pushuser_list_destroy(void);

/*
 * This function does authentication possible before compiling the control 
 * document. This means:
 *           a) password authentication by url or by headers (it is, by basic
 *              authentication response, see rfc 2617, chapter 2) 
 *           b) if this does not work, basic authentication by challenge - 
 *              response 
 *           c) enforcing various ip lists
 *
 * Try to find username and password first from the url, then form headers. If
 * both fails, try basic authentication.
 * Then check does this user allow a push from this ip, then check the pass-
 * word.
 *
 * For protection against brute force and partial protection for denial of 
 * service attacks, an exponential backup algorithm is used. Time when a 
 * specific ip  is allowed to reconnect, is stored in Dict next_try. If an ip 
 * tries to reconnect before this (because first periods are small, this means
 * the attempt cannot be manual) we drop the connection.
 *
 * Rfc 2617, chapter 1 states that if we do not accept credentials, we must 
 * send  a new challenge.
 *
 * Output an authenticated username.
 * This function should be called only when there are a push users list; the 
 * caller is responsible for this.
 */
int wap_push_ppg_pushuser_authenticate(HTTPClient *client, List *cgivars, 
                                       Octstr *ip, List *headers, 
                                       Octstr **username);

/*
 * This function checks phone number for allowed prefixes, black lists and 
 * white lists. Note that the phone number necessarily follows the interna-
 * tional format (this is checked by our pap compiler).
 */
int wap_push_ppg_pushuser_client_phone_number_acceptable(Octstr *username, 
    Octstr *number);

int wap_push_ppg_pushuser_search_ip_from_wildcarded_list(Octstr *haystack, 
    Octstr *needle, Octstr *list_sep, Octstr *ip_sep);

#endif
