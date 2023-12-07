/* SPDX-License-Identifier: MPL-2.0 */

#include "testutil.hpp"
#include "testutil_unity.hpp"

#include <stdlib.h>
#include <string.h>

#define CONTENT_SIZE 13
#define CONTENT_SIZE_MAX 32
#define ROUTING_ID_SIZE 10
#define ROUTING_ID_SIZE_MAX 32
#define QT_WORKERS 5
#define QT_CLIENTS 3
#define is_verbose 0

const char *proxy_control_address = "inproc://proxy_control";

struct thread_data
{
    int id;
};

void *g_clients_pkts_out = NULL;
void *g_workers_pkts_out = NULL;
void *control_context = NULL; // worker control, not proxy control

void setUp ()
{
    setup_test_context ();
}


// Asynchronous client-to-server (DEALER to ROUTER) - pure libzmq
//
// While this example runs in a single process, that is to make
// it easier to start and stop the example. Each task may have its own
// context and conceptually acts as a separate process. To have this
// behaviour, it is necessary to replace the inproc transport of the
// control socket by a tcp transport.

// This is our client task
// It connects to the server, and then sends a request once per second
// It collects responses as they arrive, and it prints them out. We will
// run several client tasks in parallel, each with a different random ID.

static void client_task (void *db_)
{
    const thread_data *const databag = static_cast<const thread_data *> (db_);
    // Endpoint socket gets random port to avoid test failing when port in use
    void *endpoint = zmq_socket (get_test_context (), ZMQ_PAIR);
    TEST_ASSERT_NOT_NULL (endpoint);
    int linger = 0;
    TEST_ASSERT_SUCCESS_ERRNO (
      zmq_setsockopt (endpoint, ZMQ_LINGER, &linger, sizeof (linger)));
    char endpoint_source[256];
    snprintf (endpoint_source, 256 * sizeof (char), "inproc://endpoint%d",
              databag->id);
    TEST_ASSERT_SUCCESS_ERRNO (zmq_connect (endpoint, endpoint_source));
    char *my_endpoint = s_recv (endpoint);
    TEST_ASSERT_NOT_NULL (my_endpoint);

    void *client = zmq_socket (get_test_context (), ZMQ_DEALER);
    TEST_ASSERT_NOT_NULL (client);

    // Control socket receives terminate command from main over inproc
    void *control = zmq_socket (control_context, ZMQ_SUB);
    TEST_ASSERT_NOT_NULL (control);
    TEST_ASSERT_SUCCESS_ERRNO (zmq_setsockopt (control, ZMQ_SUBSCRIBE, "", 0));
    linger = 0;
    TEST_ASSERT_SUCCESS_ERRNO (
      zmq_setsockopt (control, ZMQ_LINGER, &linger, sizeof (linger)));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_connect (control, "inproc://control"));

    char content[CONTENT_SIZE_MAX] = {};
    // Set random routing id to make tracing easier
    char routing_id[ROUTING_ID_SIZE] = {};
    snprintf (routing_id, ROUTING_ID_SIZE * sizeof (char), "%04X-%04X",
              rand () % 0xFFFF, rand () % 0xFFFF);
    TEST_ASSERT_SUCCESS_ERRNO (zmq_setsockopt (
      client, ZMQ_ROUTING_ID, routing_id,
      ROUTING_ID_SIZE)); // includes '\0' as an helper for printf
    linger = 0;
    TEST_ASSERT_SUCCESS_ERRNO (
      zmq_setsockopt (client, ZMQ_LINGER, &linger, sizeof (linger)));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_connect (client, my_endpoint));

    zmq_pollitem_t items[] = {{client, 0, ZMQ_POLLIN, 0},
                              {control, 0, ZMQ_POLLIN, 0}};

    int request_nbr = 0;
    bool run = true;
    bool keep_sending = true;
    while (run) {
        // Tick once per 200 ms, pulling in arriving messages
        int centitick;
        for (centitick = 0; centitick < 20; centitick++) {
            zmq_poll (items, 2, 10);
            if (items[0].revents & ZMQ_POLLIN) {
                int rcvmore;
                size_t sz = sizeof (rcvmore);
                int rc = TEST_ASSERT_SUCCESS_ERRNO (
                  zmq_recv (client, content, CONTENT_SIZE_MAX, 0));
                TEST_ASSERT_EQUAL_INT (CONTENT_SIZE, rc);
                if (is_verbose)
                    printf (
                      "client receive - routing_id = %s    content = %s\n",
                      routing_id, content);
                //  Check that message is still the same
                TEST_ASSERT_EQUAL_STRING_LEN ("request #", content, 9);
                TEST_ASSERT_SUCCESS_ERRNO (
                  zmq_getsockopt (client, ZMQ_RCVMORE, &rcvmore, &sz));
                TEST_ASSERT_FALSE (rcvmore);
            }
            if (items[1].revents & ZMQ_POLLIN) {
                int rc = zmq_recv (control, content, CONTENT_SIZE_MAX, 0);

                if (rc > 0) {
                    content[rc] = 0; // NULL-terminate the command string
                    if (is_verbose)
                        printf (
                          "client receive - routing_id = %s    command = %s\n",
                          routing_id, content);
                    if (memcmp (content, "TERMINATE", 9) == 0) {
                        run = false;
                        break;
                    }
                    if (memcmp (content, "STOP", 4) == 0) {
                        keep_sending = false;
                        break;
                    }
                }
            }
        }

        if (keep_sending) {
            snprintf (content, CONTENT_SIZE_MAX * sizeof (char),
                      "request #%03d", ++request_nbr); // CONTENT_SIZE
            if (is_verbose)
                printf ("client send - routing_id = %s    request #%03d\n",
                        routing_id, request_nbr);
            zmq_atomic_counter_inc (g_clients_pkts_out);

            TEST_ASSERT_EQUAL_INT (CONTENT_SIZE,
                                   zmq_send (client, content, CONTENT_SIZE, 0));
        }
    }

    TEST_ASSERT_SUCCESS_ERRNO (zmq_close (client));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_close (control));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_close (endpoint));
    free (my_endpoint);
}

// This is our server task.
// It uses the multithreaded server model to deal requests out to a pool
// of workers and route replies back to clients. One worker can handle
// one request at a time but one client can talk to multiple workers at
// once.

static void server_worker (void * /*unused_*/);

void server_task (void * /*unused_*/)
{
    // Frontend socket talks to clients over TCP
    char my_endpoint[MAX_SOCKET_STRING];
    void *frontend = zmq_socket (get_test_context (), ZMQ_ROUTER);
    TEST_ASSERT_NOT_NULL (frontend);
    int linger = 0;
    TEST_ASSERT_SUCCESS_ERRNO (
      zmq_setsockopt (frontend, ZMQ_LINGER, &linger, sizeof (linger)));
    bind_loopback_ipv4 (frontend, my_endpoint, sizeof my_endpoint);

    // Backend socket talks to workers over inproc
    void *backend = zmq_socket (get_test_context (), ZMQ_DEALER);
    TEST_ASSERT_NOT_NULL (backend);
    TEST_ASSERT_SUCCESS_ERRNO (
      zmq_setsockopt (backend, ZMQ_LINGER, &linger, sizeof (linger)));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_bind (backend, "inproc://backend"));

    // Launch pool of worker threads, precise number is not critical
    int thread_nbr;
    void *threads[5];
    for (thread_nbr = 0; thread_nbr < QT_WORKERS; thread_nbr++)
        threads[thread_nbr] = zmq_threadstart (&server_worker, NULL);

    // Endpoint socket sends random port to avoid test failing when port in use
    void *endpoint_receivers[QT_CLIENTS];
    char endpoint_source[256];
    for (int i = 0; i < QT_CLIENTS; ++i) {
        endpoint_receivers[i] = zmq_socket (get_test_context (), ZMQ_PAIR);
        TEST_ASSERT_NOT_NULL (endpoint_receivers[i]);
        TEST_ASSERT_SUCCESS_ERRNO (zmq_setsockopt (
          endpoint_receivers[i], ZMQ_LINGER, &linger, sizeof (linger)));
        snprintf (endpoint_source, 256 * sizeof (char), "inproc://endpoint%d",
                  i);
        TEST_ASSERT_SUCCESS_ERRNO (
          zmq_bind (endpoint_receivers[i], endpoint_source));
    }

    for (int i = 0; i < QT_CLIENTS; ++i) {
        send_string_expect_success (endpoint_receivers[i], my_endpoint, 0);
    }

    // Proxy control socket
    void *proxy_control = zmq_socket (get_test_context (), ZMQ_REP);
    TEST_ASSERT_NOT_NULL (proxy_control);
    TEST_ASSERT_SUCCESS_ERRNO (zmq_bind (proxy_control, proxy_control_address));

    // Connect backend to frontend via a steerable proxy
    int rc = zmq_proxy_steerable (frontend, backend, NULL, proxy_control);
    TEST_ASSERT_EQUAL_INT (0, rc);

    for (thread_nbr = 0; thread_nbr < QT_WORKERS; thread_nbr++) {
        zmq_threadclose (threads[thread_nbr]);
    }

    TEST_ASSERT_SUCCESS_ERRNO (zmq_close (frontend));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_close (backend));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_close (proxy_control));
    for (int i = 0; i < QT_CLIENTS; ++i) {
        TEST_ASSERT_SUCCESS_ERRNO (zmq_close (endpoint_receivers[i]));
    }
}

// Each worker task works on one request at a time and sends a random number
// of replies back, with random delays between replies:
// The comments in the first column, if suppressed, makes it a poller version

static void server_worker (void * /*unused_*/)
{
    void *worker = zmq_socket (get_test_context (), ZMQ_DEALER);
    TEST_ASSERT_NOT_NULL (worker);
    int linger = 0;
    TEST_ASSERT_SUCCESS_ERRNO (
      zmq_setsockopt (worker, ZMQ_LINGER, &linger, sizeof (linger)));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_connect (worker, "inproc://backend"));

    // Control socket receives terminate command from main over inproc
    void *control = zmq_socket (control_context, ZMQ_SUB);
    TEST_ASSERT_NOT_NULL (control);
    TEST_ASSERT_SUCCESS_ERRNO (zmq_setsockopt (control, ZMQ_SUBSCRIBE, "", 0));
    TEST_ASSERT_SUCCESS_ERRNO (
      zmq_setsockopt (control, ZMQ_LINGER, &linger, sizeof (linger)));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_connect (control, "inproc://control"));

    char content[CONTENT_SIZE_MAX] =
      {}; // bigger than what we need to check that
    char routing_id[ROUTING_ID_SIZE_MAX] =
      {}; // the size received is the size sent

    bool run = true;
    bool keep_sending = true;
    while (run) {
        int rc = zmq_recv (control, content, CONTENT_SIZE_MAX,
                           ZMQ_DONTWAIT); // usually, rc == -1 (no message)
        if (rc > 0) {
            content[rc] = 0; // NULL-terminate the command string
            if (is_verbose)
                printf ("server_worker receives command = %s\n", content);
            if (memcmp (content, "TERMINATE", 9) == 0)
                run = false;
            if (memcmp (content, "STOP", 4) == 0)
                keep_sending = false;
        }
        // The DEALER socket gives us the reply envelope and message
        // if we don't poll, we have to use ZMQ_DONTWAIT, if we poll, we can block-receive with 0
        rc = zmq_recv (worker, routing_id, ROUTING_ID_SIZE_MAX, ZMQ_DONTWAIT);
        if (rc == ROUTING_ID_SIZE) {
            rc = zmq_recv (worker, content, CONTENT_SIZE_MAX, 0);
            TEST_ASSERT_EQUAL_INT (CONTENT_SIZE, rc);
            if (is_verbose)
                printf ("server receive - routing_id = %s    content = %s\n",
                        routing_id, content);

            // Send 0..4 replies back
            if (keep_sending) {
                int reply, replies = rand () % 5;
                for (reply = 0; reply < replies; reply++) {
                    // Sleep for some fraction of a second
                    msleep (rand () % 10 + 1);

                    //  Send message from server to client
                    if (is_verbose)
                        printf ("server send - routing_id = %s    reply\n",
                                routing_id);
                    zmq_atomic_counter_inc (g_workers_pkts_out);

                    rc = zmq_send (worker, routing_id, ROUTING_ID_SIZE,
                                   ZMQ_SNDMORE);
                    TEST_ASSERT_EQUAL_INT (ROUTING_ID_SIZE, rc);
                    rc = zmq_send (worker, content, CONTENT_SIZE, 0);
                    TEST_ASSERT_EQUAL_INT (CONTENT_SIZE, rc);
                }
            }
        }
    }
    TEST_ASSERT_SUCCESS_ERRNO (zmq_close (worker));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_close (control));
}

// If STATISTICS is received, the proxy will reply on the control socket
// sending a multipart message with 8 frames, each with an unsigned integer
// 64-bit wide that provide in the following order:
//
// - 0/frn: number of messages received by the frontend socket
//
// - 1/frb: number of bytes received by the frontend socket
//
// - 2/fsn: number of messages sent out the frontend socket
//
// - 3/fsb: number of bytes sent out the frontend socket
//
// - 4/brn: number of messages received by the backend socket
//
// - 5/brb: number of bytes received by the backend socket
//
// - 6/bsn: number of messages sent out the backend socket
//
// - 7/bsb: number of bytes sent out the backend socket

// The main thread simply starts several clients and a server, and then
// waits for the server to finish.

void steer (void *proxy_control, const char *command, const char *runctx)
{
    if (is_verbose) {
        printf ("steer: sending %s - %s\n", command, runctx);
    }

    // Start with proxy paused
    TEST_ASSERT_SUCCESS_ERRNO (
      zmq_send (proxy_control, command, strlen (command), 0));

    zmq_msg_t stats_msg;
    int count = -1;
    while (1) {
        count = count + 1;
        TEST_ASSERT_SUCCESS_ERRNO (zmq_msg_init (&stats_msg));
        TEST_ASSERT_SUCCESS_ERRNO (zmq_msg_recv (&stats_msg, proxy_control, 0));


        if (is_verbose && zmq_msg_size (&stats_msg)) {
            if (count == 0) {
                printf ("steer:");
            }
            printf (" %lu", *(unsigned long int *) zmq_msg_data (&stats_msg));
            if (count == 7) {
                printf ("\n");
            }
        }
        if (!zmq_msg_get (&stats_msg, ZMQ_MORE))
            break;
        TEST_ASSERT_SUCCESS_ERRNO (zmq_msg_close (&stats_msg));
    }
    TEST_ASSERT_SUCCESS_ERRNO (zmq_msg_close (&stats_msg));
}

void test_proxy_steerable ()
{
    int linger = 0;
    void *threads[QT_CLIENTS + 1];

    g_clients_pkts_out = zmq_atomic_counter_new ();
    g_workers_pkts_out = zmq_atomic_counter_new ();
    control_context = zmq_ctx_new ();
    TEST_ASSERT_NOT_NULL (control_context);

    // Worker control socket receives terminate command from main over inproc
    void *control = zmq_socket (control_context, ZMQ_PUB);
    linger = 0;
    TEST_ASSERT_SUCCESS_ERRNO (
      zmq_setsockopt (control, ZMQ_LINGER, &linger, sizeof (linger)));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_bind (control, "inproc://control"));

    struct thread_data databags[QT_CLIENTS + 1];
    for (int i = 0; i < QT_CLIENTS; i++) {
        databags[i].id = i;
        threads[i] = zmq_threadstart (&client_task, &databags[i]);
    }
    threads[QT_CLIENTS] = zmq_threadstart (&server_task, NULL);
    msleep (500); // Run for 500 ms then quit

    // Proxy control socket
    void *proxy_control = zmq_socket (get_test_context (), ZMQ_REQ);
    TEST_ASSERT_NOT_NULL (proxy_control);
    linger = 0;
    TEST_ASSERT_SUCCESS_ERRNO (
      zmq_setsockopt (proxy_control, ZMQ_LINGER, &linger, sizeof (linger)));
    TEST_ASSERT_SUCCESS_ERRNO (
      zmq_connect (proxy_control, proxy_control_address));
    msleep (500); // Run for 500 ms then quit

    steer (proxy_control, "STATISTICS", "started clients");
    steer (proxy_control, "PAUSE", "started server");

    msleep (500); // Run for 500 ms then quit

    steer (proxy_control, "RESUME", "started clients");

    msleep (500); // Run for 500 ms then quit

    steer (proxy_control, "STATISTICS", "ran for a while");

    if (is_verbose)
        printf ("stopping all clients and server workers\n");
    send_string_expect_success (control, "STOP", 0);

    steer (proxy_control, "STATISTICS", "stopped clients and workers");

    msleep (500); // Wait for all clients and workers to STOP

    if (is_verbose)
        printf ("shutting down all clients and server workers\n");
    send_string_expect_success (control, "TERMINATE", 0);

    msleep (500);
    steer (proxy_control, "STATISTICS", "terminate clients and server workers");

    msleep (500); // Wait for all clients and workers to terminate
    steer (proxy_control, "TERMINATE", "terminate proxy");

    for (int i = 0; i < QT_CLIENTS + 1; i++)
        zmq_threadclose (threads[i]);

    TEST_ASSERT_SUCCESS_ERRNO (zmq_close (control));
    TEST_ASSERT_SUCCESS_ERRNO (zmq_ctx_destroy (control_context));

    TEST_ASSERT_SUCCESS_ERRNO (zmq_close (proxy_control));

    teardown_test_context ();
}

int main (void)
{
    setup_test_environment (360);

    UNITY_BEGIN ();
    RUN_TEST (test_proxy_steerable);
    return UNITY_END ();
}
