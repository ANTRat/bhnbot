#!/usr/bin/env python
# -*- coding: UTF-8 -*-
import hashlib

class cache(object):
    """Decorator that caches a function's return value each time it is called.
    If called later with the same arguments, the cached value is returned, and
    not re-evaluated.
    """
    def __init__(self, seconds=None):
        self.timeout = seconds
        self.cache = {}
     
    def __call__(self, func):
        self.func = func
        def _func(*args, **kwargs):
            try:
                return self.cache[args]
            except KeyError:
                value = self.func(*args, **kwargs)
                self.cache[args] = value
                return value
            except TypeError:
                # uncachable -- for instance, passing a list as an argument.
                # Better to not cache than to blow up entirely.
                return self.func(*args, **kwargs)
        return _func

    def __repr__(self):
        """Return the function's docstring."""
        return self.func.__doc__

    def __get__(self, obj, objtype):
        """Support instance methods."""
        return functools.partial(self.__call__, obj)

@cache()
def fibonacci(n):
    "Return the nth fibonacci number."
    if n in (0, 1):
        return n
    return fibonacci(n-1) + fibonacci(n-2)

if __name__ == "__main__":
    print fibonacci(120)
