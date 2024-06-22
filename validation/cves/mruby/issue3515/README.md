# MJS ISSUE-3515

[related Document](https://github.com/mruby/mruby/issues/3515)

## Use After Free (UAF)
UAF occurs at `vm.c:1684`
UAF occurs at mrb_vm_exec(). 

```c
MRB_API mrb_value
mrb_vm_exec(mrb_state *mrb, struct RProc *proc, mrb_code *pc)
{
            // ...
          /* call ensure only when we skip this callinfo */
          if (ci[0].ridx == ci[-1].ridx) {
            while (eidx > ci[-1].eidx) {
              ecall(mrb, --eidx); // This point!
            }
          }
            // ...
}
```