#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#


from builtins import object
class Parameter(object):
    '''The base class for a parameter.'''

    def __init__(self, name):
        self._name = name

    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__, repr(self._name))

    def __str__(self):
        '''Return the name of the attribute to hold the parsed argument.'''
        return self._name

    def parse(self, str):
        raise NotImplementedError()


class BooleanParameter(Parameter):
    '''A boolean parameter.'''

    def parse(self, str):
        if str == 'true':
            return True
        elif str == 'false':
            return False
        else:
            raise ValueError('Argument must be true or false')


class StringParameter(Parameter):
    '''A string parameter.'''

    def parse(self, str):
        return str


class NumberParameter(Parameter):
    '''A number parameter.'''

    def parse(self, str):
        return float(str)
