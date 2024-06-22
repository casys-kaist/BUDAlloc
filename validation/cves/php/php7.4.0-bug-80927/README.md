# PHP BUG-80927

[related Document](https://bugs.php.net/bug.php?id=80927)

## ffmalloc
When running a program with glibc, it outputs random bytes. However, ffmalloc prevents the attack and outputs `fake__nstest` as output.

## Use After Free (UAF)
UAF occurs at `ext/dom/node.c:644`

```c
int dom_node_namespace_uri_read(dom_object *obj, zval *retval)
{
	xmlNode *nodep = dom_object_get_node(obj);
	char *str = NULL;

	if (nodep == NULL) {
		php_dom_throw_error(INVALID_STATE_ERR, 0);
		return FAILURE;
	}

	switch (nodep->type) {
		case XML_ELEMENT_NODE:
		case XML_ATTRIBUTE_NODE:
		case XML_NAMESPACE_DECL:
			if (nodep->ns != NULL) {
				str = (char *) nodep->ns->href; // This Point!
			}
			break;
		default:
			str = NULL;
			break;
	}

	if (str != NULL) {
		ZVAL_STRING(retval, str);
	} else {
		ZVAL_NULL(retval);
	}

	return SUCCESS;
}

```