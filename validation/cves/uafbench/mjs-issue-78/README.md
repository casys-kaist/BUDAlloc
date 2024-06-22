# MJS ISSUE-78

[related Document](https://github.com/cesanta/mjs/issues/78)

## Use After Free (UAF)
UAF occurs at `mjs.c:5790`
```c
static void skip_whitespaces(struct frozen *f) {
  while (f->cur < f->end && is_space(*f->cur)) f->cur++; // This Point!
}
```

## ffmalloc
Program execution doesn't have any output. So we couldn't determine if the ffmalloc has prevent the UAF bug. So we marked as `CANNOT_DETERMINE`.