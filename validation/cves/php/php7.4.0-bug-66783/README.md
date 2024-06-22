# PHP BUG-66783

[related Document](https://bugs.php.net/bug.php?id=66783)

## Double Free(DF)
DF occurs at `ext/libxml/libxml.c:1265 `

DF occurs at php_libxml_decrement_doc_ref(). 

```c
PHP_LIBXML_API int php_libxml_decrement_doc_ref(php_libxml_node_object *object)
{
	int ret_refcount = -1;

	if (object != NULL && object->document != NULL) {
		ret_refcount = --object->document->refcount;
		if (ret_refcount == 0) {
			if (object->document->ptr != NULL) {
				xmlFreeDoc((xmlDoc *) object->document->ptr); // This Point!
			}
			if (object->document->doc_props != NULL) {
				if (object->document->doc_props->classmap) {
					zend_hash_destroy(object->document->doc_props->classmap);
					FREE_HASHTABLE(object->document->doc_props->classmap);
				}
				efree(object->document->doc_props);
			}
			efree(object->document);
		}
		object->document = NULL;
	}

	return ret_refcount;
}
```