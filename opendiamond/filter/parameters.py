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

class Parameters(object):
    '''A list of formal parameters accepted by a Filter.'''
    def __init__(self, *params):
        self.params = params

    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__,
                        ', '.join(repr(p) for p in self.params))

    def describe(self):
        '''Return a dict describing the parameter list, suitable for
        opendiamond-manifest.txt.'''
        ret = {}
        for i in range(len(self.params)):
            info = self.params[i].describe()
            ret.update(('%s-%d' % (k, i), v) for k, v in info.iteritems())
        return ret

    def parse(self, args):
        '''Parse the specified argument list and return a list of parsed
        arguments.'''
        if len(self.params) != len(args):
            raise ValueError('Incorrect argument list length')
        return [self.params[i].parse(args[i]) for i in range(len(args))]


class BaseParameter(object):
    '''The base type for a formal parameter.'''
    type = 'unknown'

    def __init__(self, label, default=None, disabled_value=None,
                            initially_enabled=True):
        self.label = label
        self.default = default
        self.disabled_value = disabled_value
        self.initially_enabled = initially_enabled

    def __repr__(self):
        return '%s(%s, %s, %s, %s)' % (self.__class__.__name__,
                            repr(self.label), repr(self.default),
                            repr(self.disabled_value),
                            repr(self.initially_enabled))

    def describe(self):
        ret = {
            'Label': self.label,
            'Type': self.type,
            'Initially-Enabled': self.initially_enabled and 'true' or 'false',
        }
        if self.default is not None:
            ret['Default'] = self.default
        if self.disabled_value is not None:
            ret['Disabled-Value'] = self.disabled_value
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
        self.min = min
        self.max = max
        self.increment = increment

    def __repr__(self):
        return '%s(%s, %s, %s, %s, %s, %s, %s)' % (self.__class__.__name__,
                                repr(self.label), repr(self.default),
                                repr(self.min), repr(self.max),
                                repr(self.increment),
                                repr(self.disabled_value),
                                repr(self.initially_enabled))

    def describe(self):
        ret = BaseParameter.describe(self)
        if self.min is not None:
            ret['Minimum'] = self.min
        if self.max is not None:
            ret['Maximum'] = self.max
        if self.increment is not None:
            ret['Increment'] = self.increment
        return ret

    def parse(self, str):
        val = float(str)
        if val == self.disabled_value:
            # Bypass min/max checks
            return val
        if self.min is not None and val < self.min:
            raise ValueError('Argument too small')
        if self.max is not None and val > self.max:
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
        self.disabled_name = disabled_value
        if disabled_value is not None:
            disabled_value = -1
        BaseParameter.__init__(self, label, default, disabled_value,
                                initially_enabled)
        self.choices = tuple(choices)

    def __repr__(self):
        return '%s(%s, %s, %s, %s, %s)' % (self.__class__.__name__,
                                repr(self.label), repr(self.choices),
                                repr(self.default),
                                repr(self.disabled_name),
                                repr(self.initially_enabled))

    def describe(self):
        ret = BaseParameter.describe(self)
        for i in range(len(self.choices)):
            ret['Choice-%d' % i] = self.choices[i][1]
        return ret

    def parse(self, str):
        index = int(str)
        if index == -1 and self.disabled_name is not None:
            return self.disabled_name
        if index < 0 or index >= len(self.choices):
            raise ValueError('Selection out of range')
        return self.choices[index][0]
