# PYTHON ISSUE-24613

[related Document](https://bugs.python.org/issue24613)

## ffmalloc
We found out that ffmalloc occurs SEGFAULT at UAF position. 
So we marked ffmalloc as `DETECT`.

## Use After Free (UAF)
UAF occurs at `Modules/arraymodule.c:1403`
UAF occurs at array.fromstring(). 

```c

static PyObject *
array_fromstring(arrayobject *self, PyObject *args)
{
    char *str;
    Py_ssize_t n;
    int itemsize = self->ob_descr->itemsize;
    if (!PyArg_ParseTuple(args, "s#:fromstring", &str, &n))
        return NULL;
    if (n % itemsize != 0) {
        PyErr_SetString(PyExc_ValueError,
                   "string length not a multiple of item size");
        return NULL;
    }
    n = n / itemsize;
    if (n > 0) {
        char *item = self->ob_item;
        if ((n > PY_SSIZE_T_MAX - Py_SIZE(self)) ||
            ((Py_SIZE(self) + n) > PY_SSIZE_T_MAX / itemsize)) {
                return PyErr_NoMemory();
        }
        PyMem_RESIZE(item, char, (Py_SIZE(self) + n) * itemsize);
        if (item == NULL) {
            PyErr_NoMemory();
            return NULL;
        }
        self->ob_item = item;
        Py_SIZE(self) += n;
        self->allocated = Py_SIZE(self);
        memcpy(item + (Py_SIZE(self) - n) * itemsize, // This point!
               str, itemsize*n);
    }
    Py_INCREF(Py_None);
    return Py_None;
}
```