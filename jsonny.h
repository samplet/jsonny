/*-
 * jsonny.h
 *
 * Copyright (c) 2014 Timothy Sample
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _JSONNY_H_
#define _JSONNY_H_


#include <stdio.h>
#include <stdlib.h>


/* Memory error. */
#define JSONNY_ERR_MEM 9
/* Something other than a hex digit in escaped UTF-16. */
#define JSONNY_ERR_UBADC 10
/* Invalid escaped UTF-16. */
#define JSONNY_ERR_UENC 11
/* Bad escape sequence. */
#define JSONNY_ERR_ESC 12
/* String format error. */
#define JSONNY_ERR_STR 13
/* Number format error. */
#define JSONNY_ERR_NUM 14
/* Primitive format error. */
#define JSONNY_ERR_PRIM 15
/* Key format error. */
#define JSONNY_ERR_KEY 16
/* Encountered ',' outside of object or array. */
#define JSONNY_ERR_SEP 17
/* Mismatched delimiters. */
#define JSONNY_ERR_DLM 18
/* Unexpected EOF. */
#define JSONNY_ERR_EOF 19
/* Unexpected character. */
#define JSONNY_ERR_BADC 20


enum jsonny_type_t {
  JSONNY_ERROR,
  JSONNY_EOF,
  JSONNY_STRING,
  JSONNY_NUMBER,
  JSONNY_OBJECT_START,
  JSONNY_KEY,
  JSONNY_OBJECT_END,
  JSONNY_ARRAY_START,
  JSONNY_SEP,
  JSONNY_ARRAY_END,
  JSONNY_TRUE,
  JSONNY_FALSE,
  JSONNY_NULL
};


struct jsonny_scanner_t {
  int js_state;
  size_t js_pos;
  size_t js_strsize;
  size_t js_strlen;
  char *js_str;
  size_t js_stksize;
  size_t js_stklen;
  char *js_stk;
};


int
jsonny_init_scanner(struct jsonny_scanner_t *s);


int
jsonny_init_scanner_s(struct jsonny_scanner_t *s,
		      size_t strsize,
		      size_t stksize);


void
jsonny_reset_scanner(struct jsonny_scanner_t *s);


enum jsonny_type_t
jsonny_scan(struct jsonny_scanner_t *s,
	    FILE *stream);


void
jsonny_free_scanner(struct jsonny_scanner_t *s);


#endif
