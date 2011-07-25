#
#  The OpenDiamond Platform for Interactive Search
#  Version 6
#
#  Copyright (c) 2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

import math

class Parameters(object):
    '''A list of formal parameters accepted by a Filter.'''
    def __init__(self, *params):
        self._params = params

    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__,
                        ', '.join(repr(p) for p in self._params))

    def describe(self):
        '''Return a dict describing the parameter list, suitable for
        opendiamond-manifest.txt.'''
        ret = {}
        for i in range(len(self._params)):
            info = self._params[i].describe()
            ret.update(('%s-%d' % (k, i), v) for k, v in info.iteritems())
        return ret

    def parse(self, args):
        '''Parse the specified argument list and return a list of parsed
        arguments.'''
        if len(self._params) != len(args):
            raise ValueError('Incorrect argument list length')
        return [self._params[i].parse(args[i]) for i in range(len(args))]


class BaseParameter(object):
    '''The base type for a formal parameter.'''
    type = 'unknown'

    def __init__(self, label, default=None, disabled_value=None,
                            initially_enabled=True):
        self._label = label
        self._default = default
        self._disabled_value = disabled_value
        self._initially_enabled = initially_enabled

    def __repr__(self):
        return '%s(%s, %s, %s, %s)' % (self.__class__.__name__,
                            repr(self._label), repr(self._default),
                            repr(self._disabled_value),
                            repr(self._initially_enabled))

    def describe(self):
        ret = {
            'Label': self._label,
            'Type': self.type,
            'Initially-Enabled': self._initially_enabled and 'true' or 'false',
        }
        if self._default is not None:
            ret['Default'] = self._default
        if self._disabled_value is not None:
            ret['Disabled-Value'] = self._disabled_value
        return ret

    def parse(self, str):
        raise NotImplementedError()


class BooleanParameter(BaseParameter):
    '''A boolean formal parameter.'''
    type = 'boolean'

    def __init__(self, label, default=None):
        if default is not None:
            if default:
                default = 'true'
            else:
                default = 'false'
        BaseParameter.__init__(self, label, default)

    def parse(self, str):
        if str == 'true':
            return True
        elif str == 'false':
            return False
        else:
            raise ValueError('Argument must be true or false')


class StringParameter(BaseParameter):
    '''A string formal parameter.'''
    type = 'string'

    def parse(self, str):
        return str


class NumberParameter(BaseParameter):
    '''A number formal parameter.'''
    type = 'number'

    def __init__(self, label, default=None, min=None, max=None,
                        increment=None, disabled_value=None,
                        initially_enabled=True):
        BaseParameter.__init__(self, label, default, disabled_value,
                            initially_enabled)
        self._min = min
        self._max = max
        self._increment = increment

    def __repr__(self):
        return '%s(%s, %s, %s, %s, %s, %s, %s)' % (self.__class__.__name__,
                                repr(self._label), repr(self._default),
                                repr(self._min), repr(self._max),
                                repr(self._increment),
                                repr(self._disabled_value),
                                repr(self._initially_enabled))

    def describe(self):
        ret = BaseParameter.describe(self)
        if self._min is not None:
            ret['Minimum'] = self._min
        if self._max is not None:
            ret['Maximum'] = self._max
        if self._increment is not None:
            ret['Increment'] = self._increment
        return ret

    def parse(self, str):
        val = float(str)
        if (val == self._disabled_value or math.isnan(val) and
                                math.isnan(self._disabled_value)):
            # Bypass min/max checks
            return val
        if self._min is not None and val < self._min:
            raise ValueError('Argument too small')
        if self._max is not None and val > self._max:
            raise ValueError('Argument too large')
        return val


class ChoiceParameter(BaseParameter):
    '''A multiple-choice formal parameter.'''
    type = 'choice'

    def __init__(self, label, choices, default=None, disabled_value=None,
                                initially_enabled=True):
        '''choices is a tuple of (parsed-value, label) pairs'''
        if default is not None:
            for i, tag in enumerate(zip(*choices)[0]):
                if tag == default:
                    default = i
                    break
            else:
                raise ValueError('Default is not one of the choices')
        self._disabled_name = disabled_value
        if disabled_value is not None:
            disabled_value = -1
        BaseParameter.__init__(self, label, default, disabled_value,
                                initially_enabled)
        self._choices = tuple(choices)

    def __repr__(self):
        return '%s(%s, %s, %s, %s, %s)' % (self.__class__.__name__,
                                repr(self._label), repr(self._choices),
                                repr(self._default),
                                repr(self._disabled_name),
                                repr(self._initially_enabled))

    def describe(self):
        ret = BaseParameter.describe(self)
        for i in range(len(self._choices)):
            ret['Choice-%d' % i] = self._choices[i][1]
        return ret

    def parse(self, str):
        index = int(str)
        if index == -1 and self._disabled_name is not None:
            return self._disabled_name
        if index < 0 or index >= len(self._choices):
            raise ValueError('Selection out of range')
        return self._choices[index][0]
