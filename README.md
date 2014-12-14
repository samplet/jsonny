jsonny
======

The jsonny JSON scanner is intended to be a simple JSON scanner.  All it
does is give you tokens and values from the scanning process, not unlike
SAX for XML. For example, to parse a single JSON string, you would write:

```C
if (jsonny_scan(js, stream) == JSONNY_STRING)
  printf("%s\n", js->js_str);
else
  printf("error\n");
```

The value in `js->js_str` will be the JSON string with no quotes and all
escapes processed to their UTF-8 equivalents (including Unicode code point
escapes).

The scanner-only model makes for an easy implementation with minimal
requirements. The `jsonny_scanner_t` struct has two buffers: one for the
current string value, and one for a delimiter stack. These buffers will
expand as necessary to accomodate anything that can fit into memory (note
that this means receiving a gigantic string could exaust the memory on
your system).

The intended usage model is where you have a struct that you want to
populate with JSON data. You simply write a loop that loops over object
keys, and copies the scanner's value into your struct. For example:

```C
struct foo {
  int a,
  double b
} f;

if ((token = jsonny_scan(js, stream)) == JSONNY_OBJECT_START) {
  token = jsonny_scan(js, stream);
  while (token != JSONNY_OBJECT_END && token != JSONNY_ERROR) {
    if (token == JSONNY_KEY
	&& strcmp("a", js->js_str) == 0
	&& json_scan(js, stream) == JSONNY_NUMBER)
      f.a = strtol(js->js_str, NULL, 10);
    else if (token == JSONNY_KEY
	     && strcmp("b", js->js_str) == 0
	     && json_scan(js, stream) == JSONNY_NUMBER)
      j.b = strtod(js->js_str, NULL);
    token = jsonny_scan(js, stream);
  }
}
```
