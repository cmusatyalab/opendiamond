#
#  The OpenDiamond Platform for Interactive Search
#  Version 5
#
#  Copyright (c) 2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

import base64

class Parameters(object):
    '''A list of formal parameters accepted by a Filter.'''
    def __init__(self, *params):
        self.params = params

    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__,
                        ', '.join(map(lambda p: repr(p), self.params)))

    def describe(self):
        '''Return a dict describing the parameter list, suitable for
        opendiamond-manifest.txt.'''
        ret = {}
        for i in range(len(self.params)):
            info = self.params[i].describe()
            ret.update(map(lambda kv: ('%s-%d' % (kv[0], i), kv[1]),
                        info.items()))
        return ret

    def parse(self, args):
        '''Parse the specified argument list and return a list of parsed
        arguments.'''
        if len(self.params) != len(args):
            raise ValueError('Incorrect argument list length')
        return map(lambda i: self.params[i].parse(args[i]), range(len(args)))


class BaseParameter(object):
    '''The base type for a formal parameter.'''
    type = 'unknown'

    def __init__(self, label, default=None):
        self.label = label
        self.default = default

    def __repr__(self):
        return '%s(%s, %s)' % (self.__class__.__name__, repr(self.label),
                            repr(self.default))

    def describe(self):
        ret = {
            'Label': self.label,
            'Type': self.type,
        }
        if self.default is not None:
            ret['Default'] = self.default
        return ret

    def parse(self, str):
        raise NotImplemented()


class BooleanParameter(BaseParameter):
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
    type = 'string'

    def parse(self, str):
        if str == '*':
            return ''
        else:
            return base64.b64decode(str)


class NumberParameter(BaseParameter):
    type = 'number'

    def __init__(self, label, default=None, min=None, max=None,
                        increment=None):
        BaseParameter.__init__(self, label, default)
        self.min = min
        self.max = max
        self.increment = increment

    def __repr__(self):
        return '%s(%s, %s, %s, %s, %s)' % (self.__class__.__name__,
                                repr(self.label), repr(self.default),
                                repr(self.min), repr(self.max),
                                repr(self.increment))

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
        if self.min is not None and val < self.min:
            raise ValueError('Argument too small')
        if self.max is not None and val > self.max:
            raise ValueError('Argument too large')
        return val


class ChoiceParameter(BaseParameter):
    type = 'choice'

    def __init__(self, label, choices, default=None):
        '''choices is a tuple of (parsed-value, label) pairs'''
        if default is not None:
            for i, tag in enumerate(zip(*choices)[0]):
                if tag == default:
                    default = i
                    break
            else:
                raise ValueError('Default is not one of the choices')
        BaseParameter.__init__(self, label, default)
        self.choices = tuple(choices)

    def __repr__(self):
        return '%s(%s, %s, %s)' % (self.__class__.__name__,
                                repr(self.label), repr(self.choices),
                                repr(self.default))

    def describe(self):
        ret = BaseParameter.describe(self)
        for i in range(len(self.choices)):
            ret['Choice-%d' % i] = self.choices[i][1]
        return ret

    def parse(self, str):
        index = int(str)
        if index < 0 or index >= len(self.choices):
            raise ValueError('Selection out of range')
        return self.choices[index][0]


if __name__ == '__main__':
    params = Parameters(
        BooleanParameter('Boolean param'),
        StringParameter('Strn', default='xyzzy plugh'),
        NumberParameter('Even integer parameter with long label', max=10,
                    increment=2),
        ChoiceParameter('Choices', (
            ('foo', 'Do something with foo'),
            ('bar', 'Do something else with bar'),
        ))
    )
    print repr(params) + '\n'

    pdesc = params.describe()
    for k, v in sorted(pdesc.items()):
        print '%s: %s' % (k, v)

    b64str = base64.b64encode('twelve')
    if params.parse(['true', b64str, '6', '1']) != [True, 'twelve', 6.0,
                                        'bar']:
        raise Exception('Parse result mismatch')

    # Check parsing of zero-length string argument
    params.parse(['true', '*', '6', '1'])

    try:
        print params.parse(['true', b64str, '12', '1'])
    except ValueError:
        pass
    else:
        raise Exception('Failed to catch number maximum')

    try:
        print params.parse(['foo', b64str, '6', '1'])
    except ValueError:
        pass
    else:
        raise Exception('Failed to catch invalid boolean')
