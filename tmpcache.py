#!/usr/bin/env python
# -*- coding: UTF-8 -*-
import time
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
            m = hashlib.md5()
            m.update(repr([args, kwargs]))
            digest = m.digest()
            try:
                if(self.cache[digest]['timeout'] > time.time()):
                    # if the timeout has not been reached
                    return self.cache[digest]['value']
                raise KeyError
            except KeyError:
                value = self.func(*args, **kwargs)
                self.cache[digest] = dict(value=value, timeout=(time.time() + self.timeout))
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

@cache(seconds=1)
def fibonacci(n):
    "Return the nth fibonacci number."
    if n in (0, 1):
        return n
    return fibonacci(n-1) + fibonacci(n-2)

if __name__ == "__main__":
    print fibonacci(120)
