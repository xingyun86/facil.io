// #include <errno.h>
// #include "libreact.h"
// #include "libsock.h"
// // old packet struct
// struct Packet_old {
//   ssize_t length;
//   struct Packet* next;
//   void* data;
//   char mem[BUFFER_PACKET_SIZE];
//   struct {
//     unsigned can_interrupt : 1;
//     unsigned close_after : 1;
//     unsigned rsrv : 6;
//   } metadata;
// };
// // The buffer structure
// struct Buffer {
//   void* id;
//   // pointer to the actual data.
//   struct Packet_old* packet;
//   // the amount of data sent from the first packet.
//   size_t sent;
//   // a mutex preventing buffer corruption.
//   pthread_mutex_t lock;
//   // a writing hook, allowing for SSL sockets or other extensions.
//   ssize_t (*writing_hook)(void* srv, int fd, void* data, size_t len);
//   // the buffer's owner
//   void* owner;
// };
//
// int srv = 0;
//
// void on_data(struct Reactor* r, int fd) {
//   if (fd == srv) {
//     while (sock_accept(r, fd) > 0)
//       ;
//     fprintf(stderr, "Accepted a new connection from srv %d\n", fd);
//     return;
//   }
//   char buff[1024];
//   ssize_t i = 0;
//   while ((i = sock_read(fd, buff, 1024)) > 0) {
//     if (buff[0] == '!')
//       fprintf(stderr, "goodbye %d\n", fd), sock_write(fd, buff, i),
//           sock_close(r, fd);
//     else
//       sock_write(fd, buff, i);
//     fprintf(stderr, "sending echo back\n");
//   }
// }
//
// int main(int argc, char const* argv[]) {
//   fprintf(stderr,
//           "buffer old size per connection: %lu bytes (i.e. %lu bytes for "
//           "10,000 connections).\n",
//           sizeof(struct Buffer), sizeof(struct Buffer) * 10000);
//   fprintf(stderr, "old packet size %lu vs. new packet size %lu (bytes)\n",
//           sizeof(struct Packet_old), sizeof(struct Packet));
//
//   init_socklib(sock_max_capacity());
//   if ((srv = sock_listen(NULL, "3000")) < 0)
//     perror("caanot initiate server socket"), exit(9);
//   errno = 0;
//   struct Reactor reactor = {.on_data = on_data, .maxfd =
//   sock_max_capacity()};
//   if (reactor_init(&reactor))
//     perror("cannot initiate reactor"), exit(1);
//   reactor_add(&reactor, srv);
//   while (reactor_review(&reactor) >= 0)
//     ;
//   reactor_stop(&reactor);
//   return 0;
// }

/////////////////////////////
// paste your favorite example code here, and run:
//
//       $ make run
//
// The *.o files are the binary saved in the tmp folder.

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

#define THREAD_COUNT 1

#include "websockets.h"

/*****************************
The Websocket echo implementation
*/

void ws_open(ws_s* ws) {
  fprintf(stderr, "Opened a new websocket connection (%p)\n", ws);
}

void ws_echo(ws_s* ws, char* data, size_t size, int is_text) {
  fprintf(stderr, "Got %lu bytes (text: %d): %s\n", size, is_text, data);
  // echos the data to the current websocket
  Websocket.write(ws, data, size, is_text);
}

void ws_shutdown(ws_s* ws) {
  Websocket.write(ws, "Shutting Down", 13, 1);
}

void ws_close(ws_s* ws) {
  fprintf(stderr, "Closed websocket connection (%p)\n", ws);
}

/*****************************
The Websocket Broadcast implementation
*/

/* websocket broadcast data */
struct ws_data {
  size_t size;
  char data[];
};
/* free the websocket broadcast data */
void free_wsdata(ws_s* ws, void* arg) {
  free(arg);
}
/* the broadcast "task" performed by `Websocket.each` */
void ws_get_broadcast(ws_s* ws, void* _data) {
  struct ws_data* data = _data;
  Websocket.write(ws, data->data, data->size, 1);  // echo
}
/* The websocket broadcast server's `on_message` callback */

void ws_broadcast(ws_s* ws, char* data, size_t size, int is_text) {
  // Copy the message to a broadcast data-packet
  struct ws_data* msg = malloc(sizeof(*msg) + size);
  msg->size = size;
  memcpy(msg->data, data, size);
  // Asynchronously calls `ws_get_broadcast` for each of the websockets
  // (except this one)
  // and calls `free_wsdata` once all the broadcasts were perfomed.
  Websocket.each(ws, ws_get_broadcast, msg, free_wsdata);
  // echos the data to the current websocket
  Websocket.write(ws, data, size, 1);
}

/*****************************
The HTTP implementation
*/

void on_request(struct HttpRequest* request) {
  if (!strcmp(request->path, "/echo")) {
    websocket_upgrade(.request = request, .on_message = ws_echo,
                      .on_open = ws_open, .on_close = ws_close,
                      .on_shutdown = ws_shutdown);
    return;
  }
  if (!strcmp(request->path, "/broadcast")) {
    websocket_upgrade(.request = request, .on_message = ws_broadcast,
                      .on_open = ws_open, .on_close = ws_close,
                      .on_shutdown = ws_shutdown);

    return;
  }
  struct HttpResponse* response = HttpResponse.create(request);
  HttpResponse.write_body(response, "Hello World!", 12);
  HttpResponse.destroy(response);
}

void on_request_f(struct HttpRequest* request) {
  static char reply[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n"
      "Connection: keep-alive\r\n"
      "Keep-Alive: timeout=2\r\n"
      "\r\n"
      "Hello World!";
  Server.write(request->server, request->sockfd, reply, sizeof(reply));
}

void on_init(server_pt srv) {
  Server.set_udata(srv, 0, (void*)7777);
}

/*****************************
The main function
*/

int main(int argc, char const* argv[]) {
  start_http_server(on_request, "~/Documents/Scratch", .threads = THREAD_COUNT,
                    .processes = 1, .on_init = on_init);
  return 0;
}
//
// /**************************************
// Lib Server "Hello World" (Http)
// */
// #include "lib-server.h"
// // #include "lib-tls-server.h"
//
// static void on_data(server_pt srv, uint64_t fd) {
//   static char reply[] =
//       "HTTP/1.1 200 OK\r\n"
//       "Content-Length: 12\r\n"
//       "Connection: keep-alive\r\n"
//       "Keep-Alive: timeout=2\r\n"
//       "\r\n"
//       "Hello World!";
//   char buff[1024];
//
//   if (Server.read(srv, fd, buff, 1024)) {
//     // write(server_uuid_to_fd(fd), reply, sizeof(reply));
//     Server.write(srv, fd, reply, sizeof(reply));
//   }
// }
//
// /**************************************
// initialization... how about timers?
// */
// void print_conn(server_pt srv, uint64_t fd, void* arg) {
//   printf("- Connection to FD: %llu\n", fd);
// }
// void done_printing(server_pt srv, uint64_t fd, void* arg) {
//   fprintf(stderr, "******* Total Clients: %lu\n", Server.count(srv, NULL));
// }
//
// void timer_task(server_pt srv) {
//   size_t count = Server.each(srv, 0, NULL, print_conn, NULL, done_printing);
//   fprintf(stderr, "Clients: %lu\n", count);
// }
// void on_init(server_pt srv) {
//   // TLSServer.init_server(srv);
//   // Server.run_every(srv, 1000, -1, (void*)timer_task, srv);
// }
//
// /**************************************
// Main function
// */
//
// int main(int argc, char const* argv[]) {
//   struct Protocol protocol = {.on_data = on_data};
//   start_server(.protocol = &protocol, .timeout = 2, .on_init = on_init,
//                .threads = THREAD_COUNT);
//   return 0;
// }
