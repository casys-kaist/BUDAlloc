# CVE2019-20633

[related Document](https://savannah.gnu.org/bugs/index.php?56683)

## Double Free(DF)
DF occurs at `src/pch.c:1184`

```c
int
another_hunk (enum diff difftype, bool rev)
{
    // ...
    while (p_end >= 0) {
        if (p_end == p_efake)
            p_end = p_bfake;		/* don't free twice */
        else
            free(p_line[p_end]); // This Point!
        p_end--;
        }
    // ...
}
```