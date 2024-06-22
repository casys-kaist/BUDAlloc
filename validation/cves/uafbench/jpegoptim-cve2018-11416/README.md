# CVE2018-11416

[related Document](https://github.com/tjko/jpegoptim/issues/57)

## DF
DF occurs at `jpegoptim.c:899`

```c
int main(int argc, char **argv) 
{
    // ...
  if (totals_mode && !quiet_mode)
    fprintf(LOG_FH,"Average ""compression"" (%ld files): %0.2f%% (%0.0fk)\n",
	    average_count, average_rate/average_count, total_save);
  jpeg_destroy_decompress(&dinfo);
  jpeg_destroy_compress(&cinfo);
  free (outbuffer); // This Point!

  return (decompress_err_count > 0 || compress_err_count > 0 ? 1 : 0);;
}
```