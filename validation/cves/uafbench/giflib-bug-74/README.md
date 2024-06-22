# GIFLIB BUG-74

[related Document](https://abi-laboratory.pro/index.php?view=changelog&l=giflib&v=5.1.6)

## Use After Free (UAF)
UAF occurs at `egif_lib.c:790`

```c
int
EGifCloseFile(GifFileType *GifFile, int *ErrorCode)
{
    // ...
    if (Private) {
        if (Private->HashTable) {
            free((char *) Private->HashTable); // This Point!
        }
	free((char *) Private);
    }

    if (File && fclose(File) != 0) {
	if (ErrorCode != NULL)
	    *ErrorCode = E_GIF_ERR_CLOSE_FAILED;
	free(GifFile);
        return GIF_ERROR;
    }

    free(GifFile);
    if (ErrorCode != NULL)
	*ErrorCode = E_GIF_SUCCEEDED;
    return GIF_OK;
}
```

## BUDALLOC, ffmalloc
BUDALLOC and ffmalloc both cause SEGFAULT. We tried to analyze the SEGFAULT by attaching gdb, but program execution stops in the middle if we attach gdb. Since we couldn't determine if it is `DETECT` or ``PREVENT` when segfault happens, we marked as `CANNOT_DETERMINE`.