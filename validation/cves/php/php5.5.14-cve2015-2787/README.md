# CVE2015-2787

[related Document](https://security-tracker.debian.org/tracker/CVE-2015-2787)

## Use After Free (UAF)
UAF occurs at `ext/standard/var_unserializer.c:1243`
```c
PHPAPI int php_var_unserialize(UNSERIALIZE_PARAMETER)
{
    // ...
	if (id == -1 || var_access(var_hash, id, &rval_ref) != SUCCESS) {
		return 0;
	}

	if (*rval != NULL) {
		zval_ptr_dtor(rval);
	}
	*rval = *rval_ref;
	Z_ADDREF_PP(rval); // This point!
	Z_SET_ISREF_PP(rval);
	
	return 1;
}
    // ...
}
```