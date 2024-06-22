# CVE2015-3205

[related Document](https://packetstormsecurity.com/files/132257/Libmimedir-VCF-Memory-Corruption-Proof-Of-Concept.html)

## Double Free(DF)
DF occurs at `memmem.c:116`

```c
_mdir_mem_forget2(struct mdir_memchunk *mdm, int freeit) {
	if(!mdm)
		return;

	if(freeit && mdm->p) {
		free(mdm->p); // This point!
	}

	/* One test or every chunk test? To be, or not to be? */
	_mdir_mem_forget2(mdm->next, freeit);

	free(mdm);
};


```