# CVE2018-20623

[related Document](https://sourceware.org/bugzilla/show_bug.cgi?id=24049)

## Use After Free (UAF)
UAF occurs at `binutils/readelf.c:19092`
UAF occurs at process_archive(). 

```c
static bfd_boolean
process_archive (Filedata * filedata, bfd_boolean is_thin_archive)
{
    // ...
        /* This is a proxy for an external member of a thin archive.  */
        Filedata * member_filedata;
        char * member_file_name = adjust_relative_path // This Point!
    (filedata->file_name, name, namelen);

        if (member_file_name == NULL)
        {
            ret = FALSE;
            break;
        }
    // ...
}
```