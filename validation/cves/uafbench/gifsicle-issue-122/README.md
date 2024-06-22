# GIFSICLE ISSUE-122

[related Document](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=881119)

## Double Free(DF)
DF occurs at `src/giffunc.c:529`

```c
void
Gif_DeleteImage(Gif_Image *gfi)
{
  Gif_DeletionHook *hook;
  if (!gfi || --gfi->refcount > 0)
    return;

  for (hook = all_hooks; hook; hook = hook->next)
    if (hook->kind == GIF_T_IMAGE)
      (*hook->func)(GIF_T_IMAGE, gfi, hook->callback_data);

  Gif_DeleteArray(gfi->identifier); // This Point!
  Gif_DeleteComment(gfi->comment);
  while (gfi->extension_list)
      Gif_DeleteExtension(gfi->extension_list);
  Gif_DeleteColormap(gfi->local);
  if (gfi->image_data && gfi->free_image_data)
    (*gfi->free_image_data)((void *)gfi->image_data);
  Gif_DeleteArray(gfi->img);
  if (gfi->compressed && gfi->free_compressed)
    (*gfi->free_compressed)((void *)gfi->compressed);
  if (gfi->user_data && gfi->free_user_data)
    (*gfi->free_user_data)(gfi->user_data);
  Gif_Delete(gfi);
}
```