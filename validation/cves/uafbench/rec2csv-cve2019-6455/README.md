# CVE2019-6455

[related Document](https://github.com/ckolivas/lrzip/issues/95)

## Double Free(DF)
DF occurs at `src/rec-mset.c:905`

```c
static void
rec_mset_elem_destroy (rec_mset_elem_t elem)
{
  if (elem)
    {
      /* Dispose the data stored in the element if a disposal callback
         function was configured by the user.  The callback is never
         invoked if the stored data is NULL.  */
      
      if (elem->data && elem->mset->disp_fn[elem->type])
        {
          elem->mset->disp_fn[elem->type] (elem->data); // This Point!
        }
      
      free (elem);
    }
}
```