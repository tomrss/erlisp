#ifndef PORT_H
#define PORT_H

#include "lisp.h"
#include <stdio.h>

// value of lisp_port current when nothing was peeked, must differ from EOF
#define PORT_NOPEEK -2

typedef enum
{
  PORT_IN,
  PORT_OUT,
} Lisp_Port_Direction;

struct lisp_port_ops
{
  const char *name;
  int (*get_char) (struct lisp_port *p);
  int (*put_char) (int c, struct lisp_port *p);
  int (*flush) (struct lisp_port *p);
  int (*close) (struct lisp_port *p);
};

struct lisp_port
{
  struct lisp_cplx_header header;
  Lisp_Port_Direction direction;
  union
  {
    FILE *file;
    struct
    {
      char *data;
      size_t size;
      size_t pos;
    } string;
  } backend;
  int current; // peeked char or EOF, PORT_NOPEEK if nothing was peeked
  int closed;  // bool
  const struct lisp_port_ops *ops;
};

Lisp_Object make_file_port (FILE *f, Lisp_Port_Direction direction);
Lisp_Object make_input_string_port (Lisp_Object string);
Lisp_Object make_output_string_port ();
Lisp_Object tcp_connect (const char *host, int port);
Lisp_Object unix_connect (const char *path);

int port_peek_char (Lisp_Port *p);
int port_read_char (Lisp_Port *p);
int port_write_char (int c, Lisp_Port *p);
void port_write_nstring (const char *string, size_t size, Lisp_Port *p);
void port_write_string (const char *string, Lisp_Port *p);
int port_flush (Lisp_Port *p);
int port_close (Lisp_Port *p);
int port_is_file (Lisp_Port *p);
int port_is_string (Lisp_Port *p);
Lisp_Object port_get_output_string (Lisp_Port *p);
const char *port_type_name (Lisp_Port *p);
Lisp_Port *port_stderr ();

NORETURN void invalidporttype_error (Lisp_Object port, const char *expected);
NORETURN void invalidportdir_error (Lisp_Object port,
                                    Lisp_Port_Direction expected);

#endif /* PORT_H */
