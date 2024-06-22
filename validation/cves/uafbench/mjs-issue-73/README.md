# MJS ISSUE-73

[related Document](https://github.com/cesanta/mjs/issues/73)

## Use After Free (UAF)
UAF occurs at `mjs.c:14085`

```c
MJS_PRIVATE void embed_string(struct mbuf *m, size_t offset, const char *p, size_t len, uint8_t /*enum embstr_flags*/ flags) {
    // ...
  /* Write string */
  if (p != 0) {
    if (flags & EMBSTR_UNESCAPE) {
      unescape(p, len, m->buf + offset + k);
    } else {
      memcpy(m->buf + offset + k, p, len); // This Point!
    }
  }
    // ...
}
```