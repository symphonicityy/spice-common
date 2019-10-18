/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2019 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <glib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <poll.h>

#include <common/agent_interface.h>

typedef struct sockaddr SA;

static GThread *recorder_comm_thr;
static bool agent_terminated = false;
static int terminate_efd = -1;
static FILE *communication_f = NULL;

#define NB_MAX_RECORDERS 16
static recorder_info *recorders[NB_MAX_RECORDERS];
static uint32_t nb_recorders = 0;

static forward_quality_cb_t forward_quality_cb;
static void *forward_quality_cb_data;

static on_connect_cb_t on_connect_cb;
static void *on_connect_cb_data;

static uintptr_t recorder_tick(void);

#ifndef RECORDER_HZ
#define RECORDER_HZ     1000000
#endif // RECORDER_HZ

static GMutex mutex_socket;

static int agent_initialize_communication(int socket)
{
    uint32_t i;
    int ret = -1;
    FILE *socket_f;

    g_mutex_lock(&mutex_socket);

    if (communication_f != NULL) {
        g_warning("A client is already connected, rejecting the connection.");

        goto unlock;
    }

    socket_f = fdopen(socket, "w+b");

    fprintf(socket_f, "Recorders: ");
    for (i = 0; i < nb_recorders; i++) {
        g_debug("Sending %s", recorders[i]->name);
        fprintf(socket_f, "%s;", recorders[i]->name);
    }
    fprintf(socket_f, "\n");
    fflush(socket_f);

    for (i = 0; i < nb_recorders; i++) {
        char enable;

        if (read(socket, &enable, sizeof(enable)) != sizeof(enable)) {
            g_warning("Invalid read on the client socket");

            goto unlock;
        }
        if (enable != '0' && enable != '1') {
            g_critical("Invalid enable-value received for recorder '%s': %u",
                       recorders[i]->name, enable);

            goto unlock;
        }

        if (enable == '0') {
            continue;
        }

        recorders[i]->trace = 1;
        g_info("Enable recorder '%s'", recorders[i]->name);
    }

    if (on_connect_cb && on_connect_cb(on_connect_cb_data)) {
        goto unlock;
    }

    communication_f = socket_f;
    ret = 0;

unlock:
    g_mutex_unlock(&mutex_socket);

    return ret;
}

static void agent_finalize_communication(int socket)
{
    uint32_t i;
    g_info("Communication socket closed.");

    g_mutex_lock(&mutex_socket);
    g_assert(socket == fileno(communication_f));

    fclose(communication_f);
    communication_f = NULL;

    for (i = 0; i < nb_recorders; i++) {
        recorders[i]->trace = 0;
    }
    g_mutex_unlock(&mutex_socket);
}

static void forward_quality(const char *quality)
{
    if (!forward_quality_cb) {
        g_warning("Quality: No callback set, dropping the message (%s).", quality);
        return;
    }

    g_info("Quality: Forwarding '%s'", quality);

    forward_quality_cb(forward_quality_cb_data, quality);
}

static int agent_process_communication(int socket)
{
    static char msg_in[128];

    static long unsigned int len = 0;

    g_assert(socket == fileno(communication_f));

    int nbytes = read(socket, msg_in + len, 1);

    if (nbytes < 0 && errno == EINTR) {
       return 0;
    }

    if (nbytes <= 0) {
        agent_finalize_communication(socket);
        return -1; // socket closed
    }

    if (msg_in[len] == '\0') {
        // process quality indicator
        forward_quality(msg_in);
        len = 0;
        return 0;
    }

    len += nbytes;

    if (len >= sizeof(msg_in) - 1) {
        msg_in[sizeof(msg_in) - 1] = '\0';
        g_warning("Invalid message received (too long?): %s", msg_in);
        len = 0;
    }

    return 0;
}

static int make_socket(guint port)
{
    struct sockaddr_in servaddr;
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_socket == -1) {
        g_critical("socket creation failed");
        return -1;
    }

    int enable = 1;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        g_critical("setsockopt(SO_REUSEADDR) failed");
        close(listen_socket);
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(listen_socket, (SA *) &servaddr, sizeof(servaddr)) != 0) {
        g_critical("socket bind failed");
        close(listen_socket);
        return -1;
    }

    return listen_socket;
}

static gpointer handle_communications(gpointer user_data)
{
    struct pollfd fds[3];
    int nb_fd = 0;
    int listen_socket;
    int i;
    guint port = GPOINTER_TO_UINT(user_data);

    listen_socket = make_socket(port);
    if (listen_socket < 0) {
        return NULL;
    }

    g_debug("Listening!");

    if ((listen(listen_socket, 1)) != 0) {
        g_critical("listen failed: %m");
        return NULL;
    }

    fds[0].fd = terminate_efd;
    fds[0].events = POLLIN;
    fds[1].fd = listen_socket;
    fds[1].events = POLLIN;
    nb_fd = 2;

    while (!agent_terminated) {

        /* Block until input arrives on one or more active sockets. */
        int ret = poll(fds, nb_fd, -1);

        if (ret < 0) {
            g_critical("poll failed: %m");
            break;
        }

        /* Service all the sockets with input pending. */
        for (i = 0; i < nb_fd; i++) {
            int fd = fds[i].fd;
            if (fd == terminate_efd) {
                if (fds[i].revents & POLLIN) {
                    g_assert(agent_terminated);
                    break;
                }
            } else if (fd == listen_socket) {
                if (fds[i].revents & ~POLLIN) {
                    g_critical("server socket closed");
                    break;
                }
                if (!(fds[i].revents & POLLIN)) {
                    continue;
                }

                /* Connection request on original socket. */
                int new_fd = accept(listen_socket, NULL, NULL);

                if (new_fd < 0) {
                    g_critical("accept failed: %m");
                    break;
                }

                if (nb_fd == 3) {
                    close(new_fd);
                    g_warning("Too many clients accepted ...");
                    continue;
                }

                g_debug("Agent Interface: client connected!");

                if (agent_initialize_communication(new_fd)) {
                    close(new_fd);
                    g_warning("Initialization failed ...");
                    continue;
                }

                fds[nb_fd].fd = new_fd;
                fds[nb_fd].events = POLLIN;
                nb_fd++;

                /* fds array modified, restart the poll. */
                break;
            } else {
                if (!(fds[i].revents & POLLIN)) {
                    continue;
                }

                /* Data arriving on an already-connected socket. */
                if (agent_process_communication(fd) < 0) {
                    nb_fd--;
                }
            }
        }
    }

    close(terminate_efd);
    close(listen_socket);

    g_info("Agent interface thread: bye!");
    return NULL;
}

static void recorder_deregister(void);

static void recorder_initialization(unsigned int port)
{
    GError *error = NULL;

    terminate_efd = eventfd(0, 0);
    if (terminate_efd == -1) {
        g_critical("eventfd failed: %m");
        return;
    }

    recorder_comm_thr = g_thread_try_new("smart_agent_interface",
                                         handle_communications,
                                         GUINT_TO_POINTER((guint) port), &error);
    if (error) {
        g_assert(!recorder_comm_thr);
        g_critical("Error: Could not start the agent interface thread: %s", error->message);
        g_error_free(error);
        return;
    }

    atexit(recorder_deregister);
}

static void recorder_interrupt_communications(void)
{
    agent_terminated = true;

    uint64_t msg = 1;
    ssize_t s = write(terminate_efd, &msg, sizeof(uint64_t));

    if (s != sizeof(uint64_t)) {
        g_warning("failed to send recorder thread termination event: %m");
    }
}


static void recorder_deregister(void)
{
    if (recorder_comm_thr) {
        recorder_interrupt_communications();
        g_thread_join(recorder_comm_thr);
        recorder_comm_thr = NULL;
    }
}

void recorder_activate(recorder_info *recorder)
{
    if (nb_recorders >= NB_MAX_RECORDERS) {
        g_critical("Too many recorders configured (nb max: %d)", NB_MAX_RECORDERS);
        return;
    }

    recorders[nb_recorders] = recorder;
    nb_recorders++;
}

static void do_send_entry(FILE *dest, recorder_info *info, recorder_entry *entry, va_list args)
{
    fprintf(dest, "Name: %s\nFunction: %s\nTime: %lu\n",
            info->name, entry->where, entry->timestamp);

    vfprintf(dest, entry->format, args);
    fprintf(dest, "\n\n");

    fflush(dest);
}


static void recorder_trace_entry(recorder_info *info, recorder_entry *entry, ...)
// ----------------------------------------------------------------------------
//   Show a recorder entry when a trace is enabled
// ----------------------------------------------------------------------------
{
    va_list args;

    if (strchr(entry->format, '\n') != NULL) {
        g_critical("Agent records cannot contain '\n' char ... (%s)", entry->where);
        return;
    }

    // send info/entry to the socket
    g_mutex_lock(&mutex_socket);

    if (communication_f == NULL) {
        g_mutex_unlock(&mutex_socket);
        return;
    }

    va_start(args, entry);
    do_send_entry(communication_f, info, entry, args);
    va_end(args);

    if (g_strcmp0(g_getenv("SPICE_AGENT_LOG_RECORDS"), "1") == 0) {
        va_start(args, entry);
        do_send_entry(stderr, info, entry, args);
        va_end(args);
    }

    g_mutex_unlock(&mutex_socket);
}

void agent_interface_start(unsigned int port)
{
    g_info("Launch on port %u", port);
    recorder_initialization(port);
}

void agent_interface_set_forward_quality_cb(forward_quality_cb_t cb, void *data)
{
    g_debug("Received forward_quality callback");
    forward_quality_cb = cb;
    forward_quality_cb_data = data;
}

void agent_interface_set_on_connect_cb(on_connect_cb_t cb, void *data)
{
    g_debug("Received on_connect callback");
    on_connect_cb = cb;
    on_connect_cb_data = data;
}

void recorder_append(recorder_info *rec,
                     const char *where,
                     const char *format,
                     uintptr_t a0,
                     uintptr_t a1,
                     uintptr_t a2,
                     uintptr_t a3)
// ----------------------------------------------------------------------------
//  Enter a record entry in ring buffer with given set of args
// ----------------------------------------------------------------------------
{
    recorder_entry entry;

    if (!rec->trace) {
        return;
    }

    entry.format = format;
    entry.timestamp = recorder_tick();
    entry.where = where;

    recorder_trace_entry(rec, &entry, a0, a1, a2, a3);
}

void recorder_append2(recorder_info *rec,
                      const char *where,
                      const char *format,
                      uintptr_t a0,
                      uintptr_t a1,
                      uintptr_t a2,
                      uintptr_t a3,
                      uintptr_t a4,
                      uintptr_t a5,
                      uintptr_t a6,
                      uintptr_t a7)
// ----------------------------------------------------------------------------
//   Enter a double record (up to 8 args)
// ----------------------------------------------------------------------------
{
    recorder_entry entry;

    if (!rec->trace) {
        return;
    }

    entry.format = format;
    entry.timestamp = recorder_tick();
    entry.where = where;

    recorder_trace_entry(rec, &entry, a0, a1, a2, a3, a4, a5, a6, a7);
}

// ============================================================================
//
//    Support functions
//
// ============================================================================

static uintptr_t recorder_tick(void)
// ----------------------------------------------------------------------------
//   Return the "ticks" as stored in the recorder
// ----------------------------------------------------------------------------
{
    struct timeval t;

    gettimeofday(&t, NULL);

    return t.tv_sec * RECORDER_HZ + t.tv_usec / (1000000 / RECORDER_HZ);
}
