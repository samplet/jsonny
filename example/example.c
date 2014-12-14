#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "jsonny.h"


struct hello_t {
  char *hello;
  double pop;
  struct hello_t *next;
};


void
free_hellos(struct hello_t *h) {
  struct hello_t *tmp;
  while(h != NULL) {
    tmp = h;
    h = h->next;
    free(tmp);
  }
}


struct hello_t *
read_hello(struct jsonny_scanner_t *js, FILE *stream) {
  struct hello_t *hobj = NULL;
  int has_hello = 0, has_pop = 0;
  enum jsonny_type_t token;
  char *hello, *endptr;
  double pop;
  if ((token = jsonny_scan(js, stream)) != JSONNY_OBJECT_START)
    return NULL;
  while (token != JSONNY_OBJECT_END && token != JSONNY_ERROR) {
    if (token == JSONNY_KEY && !has_hello
	&& strcmp(js->js_str, "hello") == 0) {
      token = jsonny_scan(js, stream);
      if (token == JSONNY_STRING) {
	hello = (char *)calloc(js->js_strlen + 1, sizeof(char));
	if (hello != NULL) {
	  strncpy(hello, js->js_str, js->js_strlen + 1);
	  has_hello = 1;
	}
      }
    } else if (token == JSONNY_KEY && !has_pop
	       && strcmp(js->js_str, "pop") == 0) {
      token = jsonny_scan(js, stream);
      if (token == JSONNY_NUMBER) {
	pop = strtod(js->js_str, &endptr);
	if (pop != 0 || endptr != js->js_str)
	  has_pop = 1;
      }
    }
    token = jsonny_scan(js, stream);
  }
  if (token == JSONNY_OBJECT_END && has_hello && has_pop) {
    hobj = (struct hello_t *)malloc(sizeof(struct hello_t));
    if (hobj != NULL) {
      hobj->hello = hello;
      hobj->pop = pop;
      hobj->next = NULL;
    }
  }
  if (hobj == NULL)
    free(hello);
  return hobj;
}


struct hello_t *
read_hellos(struct jsonny_scanner_t *js, FILE *stream) {
  enum jsonny_type_t token;
  struct hello_t *head = NULL, *tail, *current;
  if ((token = jsonny_scan(js, stream)) != JSONNY_ARRAY_START)
    return NULL;
  while (token != JSONNY_ARRAY_END && token != JSONNY_ERROR) {
    current = read_hello(js, stream);
    if (current != NULL) {
      if (head == NULL)
	head = tail = current;
      else {
	tail->next = current;
	tail = current;
      }
    } else {
      free_hellos(head);
      return NULL;
    }
    token = jsonny_scan(js, stream);
  }
  if (token != JSONNY_ARRAY_END) {
    free_hellos(head);
    head = NULL;
  }
  return head;
}


int
main(int argc, char *argv[]) {
  struct jsonny_scanner_t js;
  enum jsonny_type_t token;
  struct hello_t *head, *h;
  if (jsonny_init_scanner(&js) != 0)
    return 1;
  head = read_hellos(&js, stdin);
  if (head != NULL) {
    h = head;
    while (h != NULL) {
      printf("Hello %s -- all %.0f of ya!\n", h->hello, h->pop);
      h = h->next;
    }
  }
  free_hellos(head);
  jsonny_free_scanner(&js);
  return 0;
}
