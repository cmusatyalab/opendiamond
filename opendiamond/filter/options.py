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
from opendiamond.bundle import element

class OptionList(object):
    '''A list of options to be displayed in the user interface for a search.'''

    def __init__(self, options):
        self._options = tuple(options)
        self._option_map = dict()
        for option in options:
            self._option_map.update(option.get_name_map())

    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__,
                        ', '.join(repr(p) for p in self._options))

    def get_names(self):
        return self._option_map.keys()

    def describe(self):
        '''Return an XML element describing the option list.'''
        return element('options', *[opt.describe() for opt in self._options])

    def parse(self, optnames, args):
        '''Parse the specified argument list composed of the specified option
        names and return a dict mapping option names to parsed arguments.'''
        if len(optnames) != len(args):
            raise ValueError('Incorrect argument list length')
        ret = dict()
        for name, value in zip(optnames, args):
            if name in self._option_map:
                ret[name] = self._option_map[name].parse(value)
            else:
                raise ValueError('Unknown option "%s"' % name)
        return ret


class _BaseOption(object):
    '''The base class for a option.'''

    def __init__(self, name):
        self._name = name

    def get_name_map(self):
        return {self._name: self}

    def describe(self):
        raise NotImplementedError()

    def parse(self, str):
        raise NotImplementedError()


class BooleanOption(_BaseOption):
    '''A boolean option.'''

    def __init__(self, name, display_name, default=None):
        _BaseOption.__init__(self, name)
        self._display_name = display_name
        if default:
            self._default = True
        else:
            self._default = None

    def __repr__(self):
        return '%s(%s, %s, %s)' % (self.__class__.__name__,
                            repr(self._name), repr(self._display_name),
                            repr(self._default))

    def describe(self):
        return element('booleanOption', displayName=self._display_name,
                            name=self._name, default=self._default)

    def parse(self, str):
        if str == 'true':
            return True
        elif str == 'false':
            return False
        else:
            raise ValueError('Argument must be true or false')


class StringOption(_BaseOption):
    '''A string option.'''

    def __init__(self, name, display_name, default=None,
                    initially_enabled=None, disabled_value=None):
        _BaseOption.__init__(self, name)
        self._display_name = display_name
        self._default = default
        self._initially_enabled = initially_enabled
        self._disabled_value = disabled_value

    def __repr__(self):
        return '%s(%s, %s, %s, %s, %s)' % (self.__class__.__name__,
                            repr(self._name), repr(self._display_name),
                            repr(self._default), repr(self._initially_enabled),
                            repr(self._disabled_value))

    def describe(self):
        return element('stringOption', displayName=self._display_name,
                            name=self._name, default=self._default,
                            initiallyEnabled=self._initially_enabled,
                            disabledValue=self._disabled_value)

    def parse(self, str):
        return str


class NumberOption(_BaseOption):
    '''A number option.'''

    def __init__(self, name, display_name, default=None, min=None, max=None,
                        step=None, initially_enabled=None,
                        disabled_value=None):
        _BaseOption.__init__(self, name)
        self._display_name = display_name
        self._default = default
        self._min = min
        self._max = max
        self._step = step
        self._initially_enabled = initially_enabled
        self._disabled_value = disabled_value

    def __repr__(self):
        return '%s(%s, %s, %s, %s, %s, %s, %s, %s)' % (self.__class__.__name__,
                                repr(self._name), repr(self._display_name),
                                repr(self._default), repr(self._min),
                                repr(self._max), repr(self._step),
                                repr(self._initially_enabled),
                                repr(self._disabled_value))

    def describe(self):
        return element('numberOption', displayName=self._display_name,
                                name=self._name, default=self._default,
                                min=self._min, max=self._max, step=self._step,
                                initiallyEnabled=self._initially_enabled,
                                disabledValue=self._disabled_value)

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


class ChoiceOption(_BaseOption):
    '''A multiple-choice option.'''

    def __init__(self, name, display_name, choices, default=None,
                                initially_enabled=None, disabled_value=None):
        '''choices is a tuple of (value, display_name) pairs'''
        _BaseOption.__init__(self, name)
        self._display_name = display_name
        self._choices = tuple(choices)
        self._default = default
        self._initially_enabled = initially_enabled
        self._disabled_value = disabled_value
        if len(choices) == 0:
            raise ValueError('No choices specified')
        self._choice_set = set(zip(*choices)[0])
        if default is not None and default not in self._choice_set:
            raise ValueError('Default is not one of the choices')
        if initially_enabled is not None:
            if disabled_value is None:
                disabled_value = ''
            self._choice_set.add(disabled_value)

    def __repr__(self):
        return '%s(%s, %s, %s, %s, %s, %s)' % (self.__class__.__name__,
                                repr(self._name), repr(self._display_name),
                                repr(self._choices), repr(self._default),
                                repr(self._initially_enabled),
                                repr(self._disabled_value))

    def describe(self):
        el = element('choiceOption', displayName=self._display_name,
                                name=self._name,
                                initiallyEnabled=self._initially_enabled,
                                disabledValue=self._disabled_value)
        for value, display_name in self._choices:
            attrs = {
                'displayName': display_name,
                'value': value,
            }
            if value == self._default:
                attrs['default'] = True
            el.append(element('choice', **attrs))
        return el

    def parse(self, str):
        if str not in self._choice_set:
            raise ValueError('Choice not one of available alternatives')
        return str
