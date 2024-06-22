# CVE2018-11496

[related Document](https://github.com/ckolivas/lrzip/issues/95)

## Use After Free (UAF)
UAF occurs at `stream:1756`

```c
/* read some data from a stream. Return number of bytes read, or -1
   on failure */
i64 read_stream(rzip_control *control, void *ss, int streamno, uchar *p, i64 len)
{
	struct stream_info *sinfo = ss;
	struct stream *s = &sinfo->s[streamno];
	i64 ret = 0;

	while (len) {
		i64 n;

		n = MIN(s->buflen - s->bufp, len);

		if (n > 0) {
			memcpy(p, s->buf + s->bufp, n); // This Point!
			s->bufp += n;
			p += n;
			len -= n;
			ret += n;
		}

		if (len && s->bufp == s->buflen) {
			if (unlikely(fill_buffer(control, sinfo, s, streamno)))
				return -1;
			if (s->bufp == s->buflen)
				break;
		}
	}

	return ret;
}
```