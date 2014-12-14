/*-
 * jsonny.c
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

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "jsonny.h"


#define JSONNY_EXP_VALUE 1
#define JSONNY_EXP_KEY 2
#define JSONNY_EXP_SEP_OR_END 3


#define UTF8_LEN_MAX 4
#define UTF16_HSG_MIN 0xd800
#define UTF16_HSG_MAX 0xdbff
#define UTF16_LSG_MIN 0xdc00
#define UTF16_LSG_MAX 0xdfff


int
jsonny_push(char **s, size_t *size, size_t *len, char c) {
  char *newptr;
  size_t newsize;

  if (*len >= *size) {
    if (*size && *size > SIZE_MAX / 2) {
      errno = ENOMEM;
      return 1;
    }
    newsize = *size * 2;
    if (newsize && sizeof(char) > SIZE_MAX / newsize) {
      errno = ENOMEM;
      return 1;
    }
    if ((newptr = realloc(*s, newsize * sizeof(char))) == NULL)
      return 1;
    *s = newptr;
    *size = newsize;
  }

  (*s)[(*len)++] = c;
  (*s)[*len] = '\0';
  return 0;
}


char
jsonny_pop(char *s, size_t *len) {
  char c;
  if (*len > 0) {
    (*len)--;
    c = s[*len];
    s[*len] = '\0';
    return c;
  }
}


char
jsonny_peek(const char *s, size_t len) {
  if (len > 0)
    return s[len - 1];
}


void
jsonny_clear(char *s, size_t *len) {
  *len = 0;
  s[0] = '\0';
}


bool
issurrogate(uint16_t c) {
  return (c >= UTF16_HSG_MIN && c <= UTF16_LSG_MAX);
}


bool
ishsurrogate(uint16_t c) {
  return (c >= UTF16_HSG_MIN && c <= UTF16_HSG_MAX);
}


bool
islsurrogate(uint16_t c) {
  return (c >= UTF16_LSG_MIN && c <= UTF16_LSG_MAX);
}


size_t u16tou8(uint8_t *buf, uint16_t c) {
  if (c < 0x0080) {
    buf[0] = c;
    return 1;
  } else if (c < 0x0800) {
    buf[0] = 0xc0 | ((c >> 6) & 0x1f);
    buf[1] = 0x80 | (c & 0x3f);
    return 2;
  } else if (!issurrogate(c)) {
    buf[0] = 0xe0 | ((c >> 12) & 0xf);
    buf[1] = 0x80 | ((c >> 6) & 0x3f);
    buf[2] = 0x80 | (c & 0x3f);
    return 3;
  } else {
    return 0;
  }
}


size_t u16stou8(uint8_t *buf, uint16_t hsg, uint16_t lsg) {
  if (ishsurrogate(hsg) && islsurrogate(lsg)) {
    hsg = hsg - UTF16_HSG_MIN + 0x40;
    lsg -= UTF16_LSG_MIN;
    buf[0] = 0xf0 | ((hsg >> 8) & 0x3);
    buf[1] = 0x80 | ((hsg >> 2) & 0x3f);
    buf[2] = 0x80 | (((hsg << 4) | ((lsg >> 6) & 0xf)) & 0x3f);
    buf[3] = 0x80 | (lsg & 0x3f);
    return 4;
  } else {
    return 0;
  }
}

size_t
jsonny_scan_utf16_escape(struct jsonny_scanner_t *s, FILE *stream) {
  int res = 0, c;
  size_t size = 0;
  char hex[] = "0000";
  uint8_t u8buf[UTF8_LEN_MAX];
  uint16_t c1, c2;
  for (int i = 0; i < 4; i++) {
    c = fgetc(stream);
    s->js_pos++;
    if (isxdigit(c))
      hex[i] = c;
    else {
      s->js_state = JSONNY_ERR_UBADC;
      return 0;
    }
  }
  c1 = (uint16_t)strtol(hex, NULL, 16);
  if (issurrogate(c1)) {
    c = fgetc(stream);
    s->js_pos++;
    if (c != '\\') {
      s->js_state = JSONNY_ERR_UENC;
      return 0;
    }
    c = fgetc(stream);
    s->js_pos++;
    if (c != 'u') {
      s->js_state = JSONNY_ERR_UENC;
      return 0;
    }
    for (int i = 0; i < 4; i++) {
      c = fgetc(stream);
      s->js_pos++;
      if (isxdigit(c))
        hex[i] = c;
      else {
        s->js_state = JSONNY_ERR_UBADC;
        return 0;
      }
    }
    c2 = (uint16_t)strtol(hex, NULL, 16);
    size = u16stou8(u8buf, c1, c2);
  } else
    size = u16tou8(u8buf, c1);
  for (size_t i = 0; i < size && res == 0; i++)
    if (jsonny_push(&s->js_str, &s->js_strsize,
                    &s->js_strlen, u8buf[i]) != 0) {
      s->js_state = JSONNY_ERR_MEM;
      return 0;
    }
  if (size == 0)
    s->js_state = JSONNY_ERR_UENC;
  return size;
}


enum jsonny_type_t
jsonny_scan_string(struct jsonny_scanner_t *s, FILE *stream) {
  int res, c;
  
  jsonny_clear(s->js_str, &s->js_strlen);

  c = fgetc(stream);
  s->js_pos++;
  if (c == EOF || c != '"') {
    s->js_state = JSONNY_ERR_STR;
    return JSONNY_ERROR;
  }

  while ((c = fgetc(stream)) != EOF && c != '"') {
    s->js_pos++;
    res = 0;
    if (c == '\\') {
      c = fgetc(stream);
      s->js_pos++;
      switch (c) {
      case '"':
        res = jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, '"');
        break;
      case '\\':
        res = jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, '\\');
        break;
      case '/':
        res = jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, '/');
        break;
      case 'b':
        res = jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, '\b');
        break;
      case 'f':
        res = jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, '\f');
        break;
      case 'n':
        res = jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, '\n');
        break;
      case 'r':
        res = jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, '\r');
        break;
      case 't':
        res = jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, '\t');
        break;
      case 'u':
        if (jsonny_scan_utf16_escape(s, stream) == 0)
          return JSONNY_ERROR;
        break;
      default:
        s->js_state = JSONNY_ERR_ESC;
        return JSONNY_ERROR;
      }
    } else
      res = jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c);
    if (res != 0) {
      s->js_state = JSONNY_ERR_MEM;
      return JSONNY_ERROR;
    }
  }

  if (c == '"')
    return JSONNY_STRING;
  else {
    s->js_state = JSONNY_ERR_STR;
    return JSONNY_ERROR;
  }
}


enum jsonny_type_t
jsonny_scan_number(struct jsonny_scanner_t *s, FILE *stream) {
  int c;
  
  jsonny_clear(s->js_str, &s->js_strlen);

  c = fgetc(stream);
  s->js_pos++;
  if (c == '-') {
    if (jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c) != 0) {
      s->js_state = JSONNY_ERR_MEM;
      return JSONNY_ERROR;
    }
    c = fgetc(stream);
    s->js_pos;
  }

  if (c == '0') {
    if (jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c) != 0) {
      s->js_state = JSONNY_ERR_MEM;
      return JSONNY_ERROR;
    }
    c = fgetc(stream);
    s->js_pos++;
  } else if (isdigit(c))
    do {
      if (jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c) != 0) {
        s->js_state = JSONNY_ERR_MEM;
        return JSONNY_ERROR;
      }
      c = fgetc(stream);
      s->js_pos++;
    } while (isdigit(c));
  else {
    s->js_state = JSONNY_ERR_NUM;
    return JSONNY_ERROR;
  }

  if (c == '.') {
    if (jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c) != 0) {
      s->js_state = JSONNY_ERR_MEM;
      return JSONNY_ERROR;
    }
    c = fgetc(stream);
    s->js_pos++;
    if (isdigit(c)) {
      if (jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c) != 0) {
        s->js_state = JSONNY_ERR_MEM;
        return JSONNY_ERROR;
      }
      c = fgetc(stream);
      s->js_pos++;
    } else {
      s->js_state = JSONNY_ERR_NUM;
      return JSONNY_ERROR;
    }
    while (isdigit(c)) {
      if (jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c) != 0) {
        s->js_state = JSONNY_ERR_MEM;
        return JSONNY_ERROR;
      }
      c = fgetc(stream);
      s->js_pos++;
    }
  }

  if (c == 'e' || c == 'E') {
    if (jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c) != 0) {
      s->js_state = JSONNY_ERR_MEM;
      return JSONNY_ERROR;
    }
    c = fgetc(stream);
    s->js_pos++;
    if (c == '+' || c == '-') {
      if (jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c) != 0) {
        s->js_state = JSONNY_ERR_MEM;
        return JSONNY_ERROR;
      }
      c = fgetc(stream);
      s->js_pos++;
    }
    if (isdigit(c)) {
      if (jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c) != 0) {
        s->js_state = JSONNY_ERR_MEM;
        return JSONNY_ERROR;
      }
      c = fgetc(stream);
      s->js_pos++;
    } else {
      s->js_state = JSONNY_ERR_NUM;
      return JSONNY_ERROR;
    }
    while (isdigit(c)) {
      if (jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c) != 0) {
        s->js_state = JSONNY_ERR_MEM;
        return JSONNY_ERROR;
      }
      c = fgetc(stream);
      s->js_pos++;
    }
  }

  if (c != EOF) {
    ungetc(c, stream);
    s->js_pos--;
  }

  return JSONNY_NUMBER;
}


enum jsonny_type_t
jsonny_scan_primitive(struct jsonny_scanner_t *s, FILE *stream,
                      const char *target, size_t m, enum jsonny_type_t t) {
  int c;

  jsonny_clear(s->js_str, &s->js_strlen);

  for (size_t i = 0; i < m; i++) {
    c = fgetc(stream);
    s->js_pos++;
    if (c == EOF) {
      s->js_state = JSONNY_ERR_PRIM;
      return JSONNY_ERROR;
    }
    if (jsonny_push(&s->js_str, &s->js_strsize, &s->js_strlen, c) != 0) {
      s->js_state = JSONNY_ERR_MEM;
      return JSONNY_ERROR;
    }
  }

  if (strncmp(s->js_str, target, m) == 0)
    return t;
  else {
    s->js_state = JSONNY_ERR_PRIM;
    return JSONNY_ERROR;
  }
}


enum jsonny_type_t
jsonny_scan_true(struct jsonny_scanner_t *s, FILE *stream) {
  return jsonny_scan_primitive(s, stream, "true", 4, JSONNY_TRUE);
}


enum jsonny_type_t
jsonny_scan_false(struct jsonny_scanner_t *s, FILE *stream) {
  return jsonny_scan_primitive(s, stream, "false", 5, JSONNY_FALSE);
}


enum jsonny_type_t
jsonny_scan_null(struct jsonny_scanner_t *s, FILE *stream) {
  return jsonny_scan_primitive(s, stream, "null", 4, JSONNY_NULL);
}


int
jsonny_init_scanner_s(struct jsonny_scanner_t *s,
                      size_t strsize,
                      size_t stksize) {
  s->js_str = calloc(strsize, sizeof(char));
  if (s->js_str == NULL)
    return 1;
  s->js_stk = calloc(stksize, sizeof(char));
  if (s->js_stk == NULL) {
    free(s->js_str);
    return 1;
  }
  s->js_strsize = strsize;
  s->js_stksize = stksize;
  jsonny_reset_scanner(s);
  return 0;
}


int
jsonny_init_scanner(struct jsonny_scanner_t *s) {
  return jsonny_init_scanner_s(s, 8196, 256);
}


void
jsonny_reset_scanner(struct jsonny_scanner_t *s) {
  s->js_state = JSONNY_EXP_VALUE;
  s->js_pos = 0;
  jsonny_clear(s->js_str, &s->js_strlen);
  jsonny_clear(s->js_stk, &s->js_stklen);
}


void
jsonny_free_scanner(struct jsonny_scanner_t *s) {
  if (s->js_str != NULL)
    free(s->js_str);
  if (s->js_stk != NULL)
    free(s->js_stk);
  s->js_str = NULL;
  s->js_stk = NULL;
}


enum jsonny_type_t
jsonny_scan(struct jsonny_scanner_t *s, FILE *stream) {
  int c;
  char d;

  do {
    c = fgetc(stream);
    s->js_pos++;
  } while (isspace(c));
  ungetc(c, stream);
  s->js_pos--;

  if (s->js_state == JSONNY_EXP_KEY) {
    if (jsonny_scan_string(s, stream) == JSONNY_ERROR)
      return JSONNY_ERROR;
    do {
      c = fgetc(stream);
      s->js_pos++;
    } while (isspace(c));
    if (c != ':') {
      s->js_state = JSONNY_ERR_KEY;
      return JSONNY_ERROR;
    }
    s->js_state = JSONNY_EXP_VALUE;
    return JSONNY_KEY;
  } else if (s->js_state == JSONNY_EXP_SEP_OR_END) {
    c = fgetc(stream);
    s->js_pos++;
    if (c == ',') {
      if (s->js_stklen > 0) {
	if (s->js_stk[s->js_stklen - 1] == '[')
	  s->js_state = JSONNY_EXP_VALUE;
	else /* s->js_stk[s->js_stklen - 1] == '{' */
	  s->js_state = JSONNY_EXP_KEY;
	return JSONNY_SEP;
      } else {
	s->js_state = JSONNY_ERR_SEP;
	return JSONNY_ERROR;
      }
    } else if (c == ']' || c == '}') {
      if (s->js_stklen == 0) {
        s->js_state = JSONNY_ERR_DLM;
        return JSONNY_ERROR;
      }
      d = jsonny_pop(s->js_stk, &s->js_stklen);
      if ((c == ']' && d != '[') || (c == '}' && d != '{')) {
        s->js_state = JSONNY_ERR_DLM;
        return JSONNY_ERROR;
      }
      if (c == ']')
        return JSONNY_ARRAY_END;
      else /* c == '}' */
        return JSONNY_OBJECT_END;
    } else if (feof(stream)) {
      if (s->js_stklen == 0)
        return JSONNY_EOF;
      else {
        s->js_state = JSONNY_ERR_EOF;
        return JSONNY_ERROR;
      }
    } else {
      s->js_state = JSONNY_ERR_BADC;
      return JSONNY_ERROR;
    }
  } else if (s->js_state == JSONNY_EXP_VALUE) {
    s->js_state = JSONNY_EXP_SEP_OR_END;
    switch (c) {
    case 't':
      return jsonny_scan_true(s, stream);      
      break;
    case 'f':
      return jsonny_scan_false(s, stream);
      break;
    case 'n':
      return jsonny_scan_null(s, stream);
      break;
    case '"':
      return jsonny_scan_string(s, stream);
      break;
    case '[':
    case '{':
      c = fgetc(stream);
      s->js_pos++;
      if (jsonny_push(&s->js_stk, &s->js_stksize, &s->js_stklen, c) == 0) {
        if (c == '[') {
          s->js_state = JSONNY_EXP_VALUE;
          return JSONNY_ARRAY_START;
        } else { /* c == '{' */
          s->js_state = JSONNY_EXP_KEY;
          return JSONNY_OBJECT_START;
        }
      } else {
        s->js_state = JSONNY_ERR_MEM;
        return JSONNY_ERROR;
      }
      break;
    default:
      if (c == '-' || isdigit(c))
        return jsonny_scan_number(s, stream);
      else {
        s->js_state = JSONNY_ERR_BADC;
        return JSONNY_ERROR;
      }
    }
  }
}
