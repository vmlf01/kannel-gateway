/*
 * test_http.c - a simple program to test the new http library
 *
 * Lars Wirzenius
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "gwlib/gwlib.h"
#include "gwlib/http.h"

#define MAX_THREADS 1024

static Counter *counter = NULL;
static long max_requests = 1;
static char **urls = NULL;
static int num_urls = 0;
static int print_body = 1;

static void client_thread(void *arg) {
	int ret;
	Octstr *url, *final_url, *replyb, *os, *type, *charset;
	List *reqh, *replyh;
	long i, id, succeeded, failed;
	HTTPCaller *caller;
	char buf[1024];
	
    	caller = arg;
	succeeded = 0;
	failed = 0;
	reqh = list_create();
	sprintf(buf, "%ld", (long) gwthread_self());
	http_header_add(reqh, "X-Thread", buf);
	while ((i = counter_increase(counter)) < max_requests) {
		if ((i % 1000) == 0)
			info(0, "Starting fetch %ld", i);
		url = octstr_create(urls[i % num_urls]);
		id = http_start_request(caller, url, reqh, NULL, 0);
		debug("", 0, "Started request %ld", id);
		id = http_receive_result(caller, &ret, &final_url, 
		    	    	    	 &replyh, &replyb);
    	    	octstr_destroy(final_url);
		debug("", 0, "Done with reqest %ld", id);
		if (id == -1 || ret == -1) {
			++failed;
			error(0, "http GET failed");
		} else {
			++succeeded;
			http_header_get_content_type(replyh, &type, &charset);
			debug("", 0, "Content-type is <%s>, charset is <%s>",
			      octstr_get_cstr(type), octstr_get_cstr(charset));
			octstr_destroy(type);
			octstr_destroy(charset);
			debug("", 0, "Reply headers:");
			while ((os = list_extract_first(replyh)) != NULL) {
				octstr_dump(os, 1);
				octstr_destroy(os);
			}
			list_destroy(replyh, NULL);
			if (print_body)
				octstr_print(stdout, replyb);
			octstr_destroy(replyb);
		}
		octstr_destroy(url);
	}
	http_destroy_headers(reqh);
	http_caller_destroy(caller);
	info(0, "This thread: %ld succeeded, %ld failed.", succeeded, failed);
}

static void help(void) {
	info(0, "Usage: test_http [options] url ...");
	info(0, "where options are:");
	info(0, "-v number");
	info(0, "    set log level for stderr logging");
	info(0, "-q");
	info(0, "    don't print the body of the HTTP response");
	info(0, "-r number");
	info(0, "    make `number' requests, repeating URLs as necessary");
	info(0, "-p domain.name");
	info(0, "    use `domain.name' as a proxy");
	info(0, "-P portnumber");
	info(0, "    connect to proxy at port `portnumber'");
	info(0, "-e domain1:domain2:...");
	info(0, "    set exception list for proxy use");
}

int main(int argc, char **argv) {
	int i, opt, num_threads;
	Octstr *proxy;
	List *exceptions;
	long proxy_port;
	Octstr *proxy_username;
	Octstr *proxy_password;
	char *p;
	long threads[MAX_THREADS];
	time_t start, end;
	double run_time;
	
	gwlib_init();

	proxy = NULL;
	proxy_port = -1;
	exceptions = list_create();
	proxy_username = NULL;
	proxy_password = NULL;
	num_threads = 0;

	while ((opt = getopt(argc, argv, "hv:qr:p:P:e:t:")) != EOF) {
		switch (opt) {
		case 'v':
			set_output_level(atoi(optarg));
			break;
		
		case 'q':
		    	print_body = 0;
			break;

		case 'r':
			max_requests = atoi(optarg);
			break;

		case 't':
			num_threads = atoi(optarg);
			if (num_threads > MAX_THREADS)
				num_threads = MAX_THREADS;
			break;

		case 'h':
			help();
			exit(0);
		
		case 'p':
			proxy = octstr_create(optarg);
			break;
		
		case 'P':
			proxy_port = atoi(optarg);
			break;
		
		case 'e':
			p = strtok(optarg, ":");
			while (p != NULL) {
				list_append(exceptions, octstr_create(p));
				p = strtok(NULL, ":");
			}
			break;

		case '?':
		default:
			error(0, "Invalid option %c", opt);
			help();
			panic(0, "Stopping.");
		}
	}

    	if (optind == argc) {
	    help();
	    exit(0);
	}

	if (proxy != NULL && proxy_port > 0) {
		http_use_proxy(proxy, proxy_port, exceptions,
		    	       proxy_username, proxy_password);
	}
	octstr_destroy(proxy);
	octstr_destroy(proxy_username);
	octstr_destroy(proxy_password);
	list_destroy(exceptions, octstr_destroy_item);
	
	counter = counter_create();
	urls = argv + optind;
	num_urls = argc - optind;
	
	time(&start);
	if (num_threads == 0)
		client_thread(http_caller_create());
	else {
		for (i = 0; i < num_threads; ++i)
			threads[i] = gwthread_create(client_thread, 
			    	    	    	     http_caller_create());
		for (i = 0; i < num_threads; ++i)
			gwthread_join(threads[i]);
	}
	time(&end);

	counter_destroy(counter);
	
	run_time = difftime(end, start);
	info(0, "%ld requests in %f seconds, %f requests/s.",
		max_requests, run_time, max_requests / run_time);
	
	gwlib_shutdown();

	return 0;
}
