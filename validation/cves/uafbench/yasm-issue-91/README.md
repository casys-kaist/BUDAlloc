# YASM ISSUE-91

[related Document](https://github.com/yasm/yasm/issues/91)

## Use After Free (UAF)
UAF occurs at `libyasm/intnum.c:415`

```c
void
yasm_intnum_destroy(yasm_intnum *intn)
{
    if (intn->type == INTNUM_BV) // This Point!
        BitVector_Destroy(intn->val.bv);
    yasm_xfree(intn);
}
```