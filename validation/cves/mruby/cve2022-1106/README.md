# CVE2022-1106

[related Document](https://huntr.com/bounties/16b9d0ea-71ed-41bc-8a88-2deb4c20be8f)

## Use After Free (UAF)
UAF occurs at `src/vm.c:2822`

```c
MRB_API mrb_value
mrb_vm_exec(mrb_state *mrb, const struct RProc *proc, const mrb_code *pc)
{
    // ...
        CASE(OP_BLOCK, BB) {
      c = OP_L_BLOCK;
      goto L_MAKE_LAMBDA;
    }
    CASE(OP_METHOD, BB) {
      c = OP_L_METHOD;
      goto L_MAKE_LAMBDA;
    }

    CASE(OP_RANGE_INC, B) {
      regs[a] = mrb_range_new(mrb, regs[a], regs[a+1], FALSE); // This point! 
      mrb_gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_RANGE_EXC, B) {
      regs[a] = mrb_range_new(mrb, regs[a], regs[a+1], TRUE);
      mrb_gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_OCLASS, B) {
      regs[a] = mrb_obj_value(mrb->object_class);
      NEXT;
    }
    // ...
}
```