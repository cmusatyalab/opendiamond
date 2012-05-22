#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011-2012 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

import simplejson as json
import validictory

class _JSONSchema(dict):
    '''A JSON schema.'''

    def __init__(self, title, type, strict=None, **kwargs):
        if type == 'object':
            if strict is None:
                raise RuntimeError('strict not specified')
            kwargs['additionalProperties'] = not strict
        dict.__init__(self,
                title=title,
                type=type,
                **kwargs)

    def validate(self, obj):
        # required_by_default=False and blank_by_default=True for
        # JSON Schema draft 3 semantics
        validictory.validate(obj, self, required_by_default=False,
                blank_by_default=True)

    def dump(self, file):
        json.dump(self, file, indent=4)


class _SearchBlob(_JSONSchema):
    '''A blob specification.'''

    def __init__(self, title, strict=False, **kwargs):
        _JSONSchema.__init__(self,
            title,
            'object',
            strict=strict,
            properties=dict(
                uri=_JSONSchema(
                    'The data URI',
                    'string',
                    required=True,
                    format='uri',
                ),
                sha256=_JSONSchema(
                    'Optional SHA-256 to verify',
                    'string',
                    minLength=64,
                    maxLength=64,
                    pattern='^[0-9a-f]{64}$',
                ),
            ),
            **kwargs
        )


class SearchConfig(_JSONSchema):
    '''A search specification.'''

    def __init__(self, title=None, strict=False):
        _JSONSchema.__init__(self,
            title or 'Configuration for a Diamond search',
            'object',
            strict=strict,
            properties=dict(
                cookies=_JSONSchema(
                    'List of scope cookies',
                    'array',
                    required=True,
                    minItems=1,
                    uniqueItems=True,
                    items=_JSONSchema(
                        'A scope cookie',
                        'string'
                    ),
                ),
                filters=_JSONSchema(
                    'List of configured filters',
                    'array',
                    required=True,
                    minItems=1,
                    uniqueItems=True,
                    items=_JSONSchema(
                        'A filter',
                        'object',
                        strict=strict,
                        properties=dict(
                            name=_JSONSchema(
                                'Unique filter name',
                                'string',
                                required=True,
                                minLength=1,
                            ),
                            code=_SearchBlob(
                                'Pointer to executable code',
                                strict=strict,
                                required=True,
                            ),
                            blob=_SearchBlob(
                                'Pointer to data for the blob argument',
                                strict=strict,
                            ),
                            arguments=_JSONSchema(
                                'List of string arguments',
                                'array',
                                items=_JSONSchema(
                                    'A string argument',
                                    'string',
                                ),
                            ),
                            dependencies=_JSONSchema(
                                'List of names of depended-on filters',
                                'array',
                                uniqueItems=True,
                                items=_JSONSchema(
                                    'A filter name',
                                    'string',
                                    minLength=1,
                                ),
                            ),
                            min_score=_JSONSchema(
                                'Minimum score passing the filter',
                                'number',
                            ),
                            max_score=_JSONSchema(
                                'Maximum score passing the filter',
                                'number',
                            ),
                        ),
                    ),
                ),
            ),
        )


class SocketEvent(_JSONSchema):
    '''A SockJS event.'''

    def __init__(self, strict=False):
        _JSONSchema.__init__(self,
            'A SockJS event',
            'object',
            strict=strict,
            properties=dict(
                event=_JSONSchema(
                    'The name of the event',
                    'string',
                    required=True,
                ),
                data=_JSONSchema(
                    'Event data',
                    ['null', 'object'],
                ),
            ),
        )


def _main():
    import sys

    if (len(sys.argv) != 3
            or sys.argv[1] not in globals()
            or sys.argv[1][0] == '_'
            or sys.argv[2] not in ('strict', 'permissive')):
        print >> sys.stderr, 'Arguments: SchemaClass {strict|permissive}'
        sys.exit(1)

    globals()[sys.argv[1]](sys.argv[2] == 'strict').dump(sys.stdout)
    sys.stdout.write('\n')


if __name__ == '__main__':
    _main()
