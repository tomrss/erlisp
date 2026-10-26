#include "test_port.h"
#include "../src/alloc.h"
#include "../src/lisp.h"
#include "../src/port.h"
#include "test_lib.h"

// test cases
static TestResult test_port_input_string_read ();
static TestResult test_port_input_string_peek ();
static TestResult test_port_input_string_empty ();
static TestResult test_port_output_string_write ();
static TestResult test_port_output_string_grow ();
static TestResult test_port_direction ();
static TestResult test_port_type ();
static TestResult test_port_close ();
static TestResult test_port_file_roundtrip ();
static TestResult test_port_stderr ();

static TestCase test_port_cases[] = {
  { .skip = 0, .name = "input_string_read", .run = &test_port_input_string_read },
  { .skip = 0, .name = "input_string_peek", .run = &test_port_input_string_peek },
  { .skip = 0, .name = "input_string_empty", .run = &test_port_input_string_empty },
  { .skip = 0, .name = "output_string_write", .run = &test_port_output_string_write },
  { .skip = 0, .name = "output_string_grow", .run = &test_port_output_string_grow },
  { .skip = 0, .name = "direction", .run = &test_port_direction },
  { .skip = 0, .name = "type", .run = &test_port_type },
  { .skip = 0, .name = "close", .run = &test_port_close },
  { .skip = 0, .name = "file_roundtrip", .run = &test_port_file_roundtrip },
  { .skip = 0, .name = "stderr", .run = &test_port_stderr },
  {}, // terminator
};

TestSuite *
test_suite_port ()
{
  return test_suite_init ("port", test_port_cases);
}

// helpers

static Lisp_Port *
input_string_port (const char *s)
{
  return unbox_port (make_input_string_port (make_string (s)));
}

static Lisp_Port *
output_string_port ()
{
  return unbox_port (make_output_string_port ());
}

// test cases implementation

static TestResult
test_port_input_string_read ()
{
  Lisp_Port *p = input_string_port ("ab");

  int c = port_read_char (p);
  TEST_ASSERT (c == 'a', "first read: expected 'a', got %d", c);
  c = port_read_char (p);
  TEST_ASSERT (c == 'b', "second read: expected 'b', got %d", c);
  c = port_read_char (p);
  TEST_ASSERT (c == EOF, "read at end: expected EOF, got %d", c);
  // eof is sticky
  c = port_read_char (p);
  TEST_ASSERT (c == EOF, "read after EOF: expected EOF, got %d", c);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_port_input_string_peek ()
{
  Lisp_Port *p = input_string_port ("ab");

  // peek doesn't consume
  int c = port_peek_char (p);
  TEST_ASSERT (c == 'a', "first peek: expected 'a', got %d", c);
  c = port_peek_char (p);
  TEST_ASSERT (c == 'a', "second peek: expected 'a', got %d", c);
  TEST_ASSERT (p->current == 'a', "peeked char not stored: current is %d",
               p->current);

  // read returns the peeked char and clears it
  c = port_read_char (p);
  TEST_ASSERT (c == 'a', "read after peek: expected 'a', got %d", c);
  TEST_ASSERT (p->current == PORT_NOPEEK,
               "peeked char not cleared after read: current is %d",
               p->current);

  c = port_read_char (p);
  TEST_ASSERT (c == 'b', "read: expected 'b', got %d", c);

  // peek at the end gives EOF, and so does the following read
  c = port_peek_char (p);
  TEST_ASSERT (c == EOF, "peek at end: expected EOF, got %d", c);
  c = port_read_char (p);
  TEST_ASSERT (c == EOF, "read after peeked EOF: expected EOF, got %d", c);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_port_input_string_empty ()
{
  Lisp_Port *p = input_string_port ("");

  int c = port_peek_char (p);
  TEST_ASSERT (c == EOF, "peek on empty: expected EOF, got %d", c);
  c = port_read_char (p);
  TEST_ASSERT (c == EOF, "read on empty: expected EOF, got %d", c);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_port_output_string_write ()
{
  Lisp_Port *p = output_string_port ();
  TEST_ASSERT (p->backend.string.pos == 0,
               "new output port not empty: pos is %zu",
               p->backend.string.pos);

  int w = port_write_char ('a', p);
  TEST_ASSERT (w == 'a', "write_char: expected 'a' returned, got %d", w);
  port_write_string ("bc", p);
  port_write_nstring ("dexx", 2, p);

  const char *expected = "abcde";
  size_t len = strlen (expected);
  TEST_ASSERT (p->backend.string.pos == len, "expected %zu chars, got %zu",
               len, p->backend.string.pos);
  TEST_ASSERT (memcmp (p->backend.string.data, expected, len) == 0,
               "expected \"%s\", got \"%.*s\"", expected,
               (int)p->backend.string.pos, p->backend.string.data);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_port_output_string_grow ()
{
  Lisp_Port *p = output_string_port ();
  size_t initsize = p->backend.string.size;

  // go well past the initial buffer, growing it more than once
  size_t n = initsize * 3 + 1;
  for (size_t i = 0; i < n; i++)
    port_write_char ('a' + i % 26, p);

  TEST_ASSERT (p->backend.string.pos == n, "expected %zu chars, got %zu", n,
               p->backend.string.pos);
  TEST_ASSERT (p->backend.string.size >= n,
               "buffer size %zu smaller than written chars %zu",
               p->backend.string.size, n);

  // what was written before each growth is still there
  for (size_t i = 0; i < n; i++)
    TEST_ASSERT (p->backend.string.data[i] == 'a' + (int)(i % 26),
                 "char %zu: expected '%c', got '%c'", i, 'a' + (int)(i % 26),
                 p->backend.string.data[i]);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_port_direction ()
{
  Lisp_Port *in = input_string_port ("a");
  TEST_ASSERT (in->direction == PORT_IN, "input string port is not PORT_IN");

  Lisp_Port *out = output_string_port ();
  TEST_ASSERT (out->direction == PORT_OUT,
               "output string port is not PORT_OUT");

  // never stdin/stdout: the gc finalizer of the port would close them
  Lisp_Port *fin = unbox_port (make_file_port (tmpfile (), PORT_IN));
  int dir = fin->direction;
  port_close (fin);
  TEST_ASSERT (dir == PORT_IN, "file port in is not PORT_IN");

  Lisp_Port *fout = unbox_port (make_file_port (tmpfile (), PORT_OUT));
  dir = fout->direction;
  port_close (fout);
  TEST_ASSERT (dir == PORT_OUT, "file port out is not PORT_OUT");

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_port_type ()
{
  Lisp_Port *s = input_string_port ("a");
  TEST_ASSERT (port_is_string (s), "input string port is not string");
  TEST_ASSERT (!port_is_file (s), "input string port is file");
  TEST_ASSERT (strcmp (port_type_name (s), "string") == 0,
               "input string port name: expected \"string\", got \"%s\"",
               port_type_name (s));

  Lisp_Port *o = output_string_port ();
  TEST_ASSERT (port_is_string (o), "output string port is not string");
  TEST_ASSERT (strcmp (port_type_name (o), "string") == 0,
               "output string port name: expected \"string\", got \"%s\"",
               port_type_name (o));

  // never stdout: the gc finalizer of the port would close it
  Lisp_Port *f = unbox_port (make_file_port (tmpfile (), PORT_OUT));
  int isfile = port_is_file (f);
  int isstring = port_is_string (f);
  const char *name = port_type_name (f);
  port_close (f);
  TEST_ASSERT (isfile, "file port is not file");
  TEST_ASSERT (!isstring, "file port is string");
  TEST_ASSERT (strcmp (name, "file") == 0,
               "file port name: expected \"file\", got \"%s\"", name);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_port_close ()
{
  Lisp_Port *in = input_string_port ("ab");
  TEST_ASSERT (!in->closed, "new port is closed");

  // closing twice is allowed, only the first time does something
  int r = port_close (in);
  TEST_ASSERT (r == 1, "first close: expected 1, got %d", r);
  TEST_ASSERT (in->closed, "port not closed after close");
  r = port_close (in);
  TEST_ASSERT (r == 0, "second close: expected 0, got %d", r);
  TEST_ASSERT (in->closed, "port not closed after second close");

  Lisp_Port *out = output_string_port ();
  r = port_close (out);
  TEST_ASSERT (r == 1, "close output: expected 1, got %d", r);
  TEST_ASSERT (out->closed, "output port not closed after close");

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_port_file_roundtrip ()
{
  // obj/ is created by the build and ignored by git
  const char *path = "obj/test-port-c.txt";

  FILE *f = fopen (path, "w");
  TEST_ASSERT (f != NULL, "cannot open %s for writing", path);
  Lisp_Port *out = unbox_port (make_file_port (f, PORT_OUT));
  port_write_string ("hi", out);
  port_write_char ('!', out);
  port_flush (out);
  int r = port_close (out);
  TEST_ASSERT (r == 1, "close file out: expected 1, got %d", r);
  TEST_ASSERT (out->closed, "file out not closed after close");

  f = fopen (path, "r");
  TEST_ASSERT (f != NULL, "cannot open %s for reading", path);
  Lisp_Port *in = unbox_port (make_file_port (f, PORT_IN));
  int c = port_peek_char (in);
  TEST_ASSERT (c == 'h', "peek: expected 'h', got %d", c);
  c = port_read_char (in);
  TEST_ASSERT (c == 'h', "read: expected 'h', got %d", c);
  c = port_read_char (in);
  TEST_ASSERT (c == 'i', "read: expected 'i', got %d", c);
  c = port_read_char (in);
  TEST_ASSERT (c == '!', "read: expected '!', got %d", c);
  c = port_read_char (in);
  TEST_ASSERT (c == EOF, "read at end: expected EOF, got %d", c);
  r = port_close (in);
  TEST_ASSERT (r == 1, "close file in: expected 1, got %d", r);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_port_stderr ()
{
  Lisp_Port *p = port_stderr ();
  TEST_ASSERT (p != NULL, "stderr port is NULL");
  TEST_ASSERT (p == port_stderr (), "stderr port is not always the same");
  TEST_ASSERT (p->backend.file == stderr, "stderr port doesn't use stderr");
  TEST_ASSERT (p->direction == PORT_OUT, "stderr port is not PORT_OUT");
  TEST_ASSERT (port_is_file (p), "stderr port is not file");
  TEST_ASSERT (!p->closed, "stderr port is closed");
  TEST_ASSERT (p->current == PORT_NOPEEK, "stderr port has a peeked char");

  return TEST_RESULT_SUCCESS;
}
