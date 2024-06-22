# CVE2022-1934

[related Document](https://huntr.com/bounties/99e6df06-b9f7-4c53-a722-6bb89fbfb51f)

## ffmalloc
Execution output of ffmalloc is same with glibc. We couldn't determine if ffmalloc prevent the UAF bug. So we marked as `CANNOT_DETERMINE`.

## Use After Free (UAF)
UAF occurs at `mruby/src/vm.c:1167`

```c
static mrb_value
hash_new_from_values(mrb_state *mrb, mrb_int argc, mrb_value *regs)
{
  mrb_value hash = mrb_hash_new_capa(mrb, argc);
  while (argc--) {
    mrb_hash_set(mrb, hash, regs[0], regs[1]); // This point!
    regs += 2;
  }
  return hash;
}
```
