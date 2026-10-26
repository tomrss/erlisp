#include "port.h"
#include "alloc.h"
#include "error.h"
#include "lisp.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define PORTOUTSTRINCR 1024

static int file_port_get_char (Lisp_Port *p);
static int file_port_put_char (int c, Lisp_Port *p);
static int file_port_flush (Lisp_Port *p);
static int file_port_close (Lisp_Port *p);
static int string_port_get_char (Lisp_Port *p);
static int string_port_put_char (int c, Lisp_Port *p);
static int string_port_flush (Lisp_Port *p);
static int string_port_close (Lisp_Port *p);
static void snprintf_addr (struct sockaddr *addr, char *buf, size_t size);
static int port_unsafe_write_char (int c, Lisp_Port *p);
static Lisp_Object connect_socket (int fd, struct sockaddr *addr,
                                   socklen_t len);

static const struct lisp_port_ops file_ops = {
  .name = "file",
  .get_char = &file_port_get_char,
  .put_char = &file_port_put_char,
  .flush = &file_port_flush,
  .close = &file_port_close,
};

static const struct lisp_port_ops string_ops = {
  .name = "string",
  .get_char = &string_port_get_char,
  .put_char = &string_port_put_char,
  .flush = &string_port_flush,
  .close = &string_port_close,
};

// used by print_error, not in lisp
static Lisp_Port stderr_port = {
  .header = { .type = LISP_PORT },
  .direction = PORT_OUT,
  .current = PORT_NOPEEK,
  .closed = 0,
  .ops = &file_ops,
};

static int
file_port_get_char (Lisp_Port *p)
{
  return fgetc (p->backend.file);
}

static int
file_port_put_char (int c, Lisp_Port *p)
{
  return fputc (c, p->backend.file);
}

static int
file_port_flush (Lisp_Port *p)
{
  fflush (p->backend.file);
  return 1;
}

static int
file_port_close (Lisp_Port *p)
{
  if (p->closed)
    return 0;
  fclose (p->backend.file);
  p->closed = 1;
  return 1;
}

static int
string_port_get_char (Lisp_Port *p)
{
  if (p->backend.string.pos >= p->backend.string.size)
    return EOF;
  return (unsigned char)p->backend.string.data[p->backend.string.pos++];
}

static int
string_port_put_char (int c, Lisp_Port *p)
{
  if (c == EOF)
    return EOF;
  if (p->backend.string.pos >= p->backend.string.size)
    {
      // not enough space, expand buffer
      p->backend.string.size += PORTOUTSTRINCR;
      char *buf = realloc (p->backend.string.data, p->backend.string.size);
      if (buf == NULL)
        // todo this is bad!
        f_signal (q_error, f_cons (make_string ("not enough memory"), q_nil));
      p->backend.string.data = buf;
    }

  p->backend.string.data[p->backend.string.pos++] = c;
  return c;
}

static int
string_port_flush (UNUSED Lisp_Port *p)
{
  // nothing to do here
  return 1;
}

static int
string_port_close (Lisp_Port *p)
{
  if (p->closed)
    return 0;
  free (p->backend.string.data);
  p->closed = 1;
  return 1;
}

Lisp_Object
make_file_port (FILE *f, Lisp_Port_Direction direction)
{
  Lisp_Port *p = make_uninit_port ();
  p->direction = direction;
  p->backend.file = f;
  p->ops = &file_ops;
  return box_port (p);
}

Lisp_Object
make_input_string_port (Lisp_Object string)
{
  Lisp_Port *p = make_uninit_port ();
  p->direction = PORT_IN;
  // TODO this is tmp: we are copying but we shouldn't. Why? gc will
  // free the input string.  probably the correct thing to do would be
  // to keep the Lisp_Object string and gc mark it with the port.
  p->backend.string.data = malloc (unbox_string (string)->size);
  memcpy (p->backend.string.data, unbox_string (string)->data,
          unbox_string (string)->size);

  p->backend.string.size = unbox_string (string)->size;
  p->backend.string.pos = 0;
  p->ops = &string_ops;
  return box_port (p);
}

Lisp_Object
make_output_string_port ()
{
  Lisp_Port *p = make_uninit_port ();
  p->direction = PORT_OUT;
  p->backend.string.data = malloc (PORTOUTSTRINCR);
  p->backend.string.size = PORTOUTSTRINCR;
  p->backend.string.pos = 0;
  p->ops = &string_ops;
  return box_port (p);
}

static Lisp_Object
connect_socket (int fd, struct sockaddr *addr, socklen_t len)
{
  if (connect (fd, addr, len) < 0)
    {
      int err = errno;
      close (fd);
      char buf[256] = { 0 };
      snprintf_addr (addr, buf, sizeof (buf));
      portio_error (buf, err);
    }

  // we open a couple of FILEs that target the same socket underneath.
  // this is because r+ file on socket is not doable. this will be ditched once
  // we have non-blocking sockets
  FILE *in = fdopen (fd, "r");
  FILE *out = fdopen (dup (fd), "w");

  return f_cons (make_file_port (in, PORT_IN), make_file_port (out, PORT_OUT));
}

Lisp_Object
tcp_connect (const char *host, int port)
{
  int fd = socket (AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    portio_error ("tcp-socket", errno);

  struct sockaddr_in addr;
  memset (&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons (port);
  if (inet_pton (AF_INET, host, &addr.sin_addr) < 1)
    {
      close (fd);
      f_signal (q_error_portio,
                f_cons (make_string (host), f_cons (box_int (port), q_nil)));
    }

  return connect_socket (fd, (struct sockaddr *)&addr,
                         sizeof (struct sockaddr_in));
}

Lisp_Object
unix_connect (const char *path)
{
  int fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    portio_error ("unix-socket", errno);

  struct sockaddr_un addr;
  memset (&addr, 0, sizeof addr);
  addr.sun_family = AF_UNIX;
  strncpy (addr.sun_path, path, sizeof addr.sun_path - 1);

  return connect_socket (fd, (struct sockaddr *)&addr,
                         sizeof (struct sockaddr_un));
}

int
port_peek_char (Lisp_Port *p)
{
  if (p->closed)
    closedport_error (box_port (p));

  if (p->current != PORT_NOPEEK)
    return p->current;

  int c = p->ops->get_char (p);
  p->current = c;
  return c;
}

int
port_read_char (Lisp_Port *p)
{
  if (p->closed)
    closedport_error (box_port (p));

  if (p->current != PORT_NOPEEK)
    {
      int res = p->current;
      p->current = PORT_NOPEEK;
      return res;
    }

  return p->ops->get_char (p);
}

int
port_write_char (int c, Lisp_Port *p)
{
  if (p->closed)
    closedport_error (box_port (p));

  return port_unsafe_write_char (c, p);
}

void
port_write_nstring (const char *string, size_t size, Lisp_Port *p)
{
  if (p->closed)
    closedport_error (box_port (p));

  for (size_t i = 0; i < size; i++)
    port_unsafe_write_char (string[i], p);
}

void
port_write_string (const char *string, Lisp_Port *p)
{
  if (p->closed)
    closedport_error (box_port (p));

  int i = 0;
  char c;
  while ((c = string[i++]) != '\0')
    port_unsafe_write_char (c, p);
}

int
port_flush (Lisp_Port *p)
{
  return p->ops->flush (p);
}

int
port_close (Lisp_Port *p)
{
  return p->ops->close (p);
}

int
port_is_file (Lisp_Port *p)
{
  return p->ops == &file_ops;
}

int
port_is_string (Lisp_Port *p)
{
  return p->ops == &string_ops;
}

Lisp_Object
port_get_output_string (Lisp_Port *p)
{
  if (!port_is_string (p))
    invalidporttype_error (box_port (p), "string");
  if (p->direction != PORT_OUT)
    invalidportdir_error (box_port (p), PORT_OUT);
  if (p->closed)
    closedport_error (box_port (p));

  // this will copy the buf
  return make_nstring (p->backend.string.data, p->backend.string.pos);
}

const char *
port_type_name (Lisp_Port *p)
{
  return p->ops->name;
}

Lisp_Port *
port_stderr ()
{
  stderr_port.backend.file = stderr;
  return &stderr_port;
}

static const char *
port_direction_name (Lisp_Port_Direction d)
{
  switch (d)
    {
    case PORT_IN:
      return "PORT_IN";
    case PORT_OUT:
      return "PORT_OUT";
    default:
      return "UNKNONW";
    }
}

NORETURN void
invalidporttype_error (Lisp_Object port, const char *expected)
{
  Lisp_Object data = f_cons (make_string (expected), q_nil);
  data = f_cons (make_string (port_type_name (unbox_port (port))), data);

  f_signal (q_error_portinvalid, data);
}

NORETURN void
invalidportdir_error (Lisp_Object port, Lisp_Port_Direction expected)
{
  Lisp_Object data
      = f_cons (make_string (port_direction_name (expected)), q_nil);
  data = f_cons (
      make_string (port_direction_name (unbox_port (port)->direction)), data);

  f_signal (q_error_portinvalid, data);
}

static void
snprintf_addr (struct sockaddr *addr, char *buf, size_t size)
{
  char ip6[INET6_ADDRSTRLEN]; // ip4 will fit here
  struct sockaddr_in *addr_in;
  struct sockaddr_in6 *addr_in6;
  struct sockaddr_un *addr_un;

  switch (addr->sa_family)
    {
    case AF_INET:
      addr_in = (struct sockaddr_in *)addr;
      inet_ntop (AF_INET, &addr_in->sin_addr, ip6, sizeof (ip6));
      snprintf (buf, size, "%s:%d", ip6, ntohs (addr_in->sin_port));
      break;
    case AF_INET6:
      addr_in6 = (struct sockaddr_in6 *)addr;
      inet_ntop (AF_INET6, &addr_in6->sin6_addr, ip6, sizeof (ip6));
      snprintf (buf, size, "[%s]:%d", ip6, ntohs (addr_in6->sin6_port));
      break;
    case AF_UNIX:
      addr_un = (struct sockaddr_un *)addr;
      snprintf (buf, size, "%s", addr_un->sun_path);
      break;
    default:
      snprintf (buf, size, "[unknown socket type");
    }
}

static int
port_unsafe_write_char (int c, Lisp_Port *p)
{
  int w = p->ops->put_char (c, p);
  if (w == EOF)
    // TODO this is not a file error
    file_error (make_string ("error writing to output port"));
  return w;
}
