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
        '''For the object type, strict indicates whether unknown
        object properties are accepted.'''

        if type == 'object' and 'additionalProperties' not in kwargs:
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


class _SingleEvent(_JSONSchema):
    '''A SockJS event message.'''

    def __init__(self, title, event, data, strict=False, **kwargs):
        if data is None:
            data = _JSONSchema(
                'Null or an empty object',
                [
                    'null',
                    _JSONSchema(
                        'An empty object',
                        'object',
                        strict=strict,
                    ),
                ],
            )
        _JSONSchema.__init__(self,
            title,
            'object',
            strict=strict,
            properties=dict(
                event=_JSONSchema(
                    'The event type',
                    'string',
                    required=True,
                    enum=[event],
                ),
                data=data,
            ),
            **kwargs
        )


class _StartEvent(_SingleEvent):
    def __init__(self, strict=False):
        _SingleEvent.__init__(self,
            'Start the search',
            'start',
            _JSONSchema(
                'Arguments to start event',
                'object',
                strict=strict,
                required=True,
                properties=dict(
                    search_key=_JSONSchema(
                        'The search key',
                        'string',
                        required=True,
                    ),
                ),
            ),
            strict=strict,
        )


class _PongEvent(_SingleEvent):
    def __init__(self, strict=False):
        _SingleEvent.__init__(self,
            'A response to a ping message',
            'pong',
            None,
            strict=strict,
        )


class _PauseEvent(_SingleEvent):
    def __init__(self, strict=False):
        _SingleEvent.__init__(self,
            'Pause the search',
            'pause',
            None,
            strict=strict,
            description='''Pause each Diamond server after it
produces the next result.  Note that results may continue to arrive at the
client for an indefinite period.''',
        )


class _ResumeEvent(_SingleEvent):
    def __init__(self, strict=False):
        _SingleEvent.__init__(self,
            'Resume the search after it is paused',
            'resume',
            None,
            strict=strict,
        )


class ClientToServerEvent(_JSONSchema):
    '''A client-to-server SockJS event.'''

    def __init__(self, strict=False):
        _JSONSchema.__init__(self,
            'A client-to-server SockJS event',
            [
                _StartEvent(strict),
                _PongEvent(strict),
                _PauseEvent(strict),
                _ResumeEvent(strict),
            ],
        )


class _SearchStartedEvent(_SingleEvent):
    def __init__(self, strict=False):
        _SingleEvent.__init__(self,
            'Search has been started',
            'search_started',
            _JSONSchema(
                'Information about the search',
                'object',
                strict=strict,
                required=True,
                properties=dict(
                    search_id=_JSONSchema(
                        'The search ID',
                        'number',
                        required=True,
                    ),
                ),
            ),
            strict=strict,
        )


class _PingEvent(_SingleEvent):
    def __init__(self, strict=False):
        _SingleEvent.__init__(self,
            'A keepalive event',
            'ping',
            None,
            strict=strict,
            description='The client must respond with a pong event.',
        )


class _AttributeValue(_JSONSchema):
    def __init__(self, strict=False):
        _JSONSchema.__init__(self,
            'The value of an object attribute',
            'object',
            strict=strict,
            properties=dict(
                data=_JSONSchema(
                    'The decoded attribute data',
                    ['string', 'number', 'object'],
                ),
                image_url=_JSONSchema(
                    'The location of the attribute rendered as an image',
                    'string',
                    format='uri',
                ),
                raw_url=_JSONSchema(
                    'The location of the attribute raw data',
                    'string',
                    format='uri',
                ),
            ),
        )


class _ResultEvent(_SingleEvent):
    def __init__(self, strict=False):
        _SingleEvent.__init__(self,
            'A search result',
            'result',
            _JSONSchema(
                'The attributes of the result object',
                'object',
                strict=strict,
                required=True,
                properties=dict(
                    _ResultURL=_JSONSchema(
                        'The location of this search result',
                        'object',
                        strict=strict,
                        properties=dict(
                            data=_JSONSchema(
                                'The location URL',
                                'string',
                                required=True,
                            ),
                        ),
                        required=True,
                    ),
                ),
                additionalProperties=_AttributeValue(strict),
            ),
            strict=strict,
        )


class _Statistic(_JSONSchema):
    def __init__(self):
        _JSONSchema.__init__(self,
            'A statistic',
            'number',
        )


class _StatisticsEvent(_SingleEvent):
    def __init__(self, strict=False):
        _SingleEvent.__init__(self,
            'A statistics update',
            'statistics',
            _JSONSchema(
                'Statistics about the entire search',
                'object',
                required=True,
                properties=dict(
                    filters=_JSONSchema(
                        'Statistics blocks for individual filters',
                        'object',
                        required=True,
                        additionalProperties=_JSONSchema(
                            'Statistics about an individual filter',
                            'object',
                            additionalProperties=_Statistic(),
                        ),
                    ),
                ),
                additionalProperties=_Statistic(),
            ),
            strict=strict,
        )


class _SearchCompleteEvent(_SingleEvent):
    def __init__(self, strict=False):
        _SingleEvent.__init__(self,
            'A notification of search completion',
            'search_complete',
            None,
            strict=strict,
        )


class _ErrorEvent(_SingleEvent):
    def __init__(self, strict=False):
        _SingleEvent.__init__(self,
            'A failure report',
            'error',
            _JSONSchema(
                'The error detail',
                'object',
                strict=strict,
                required=True,
                properties=dict(
                    message=_JSONSchema(
                        'The error message',
                        'string',
                        required=True,
                    ),
                ),
            ),
            strict=strict,
        )


class ServerToClientEvent(_JSONSchema):
    '''A server-to-client SockJS event.'''

    def __init__(self, strict=False):
        _JSONSchema.__init__(self,
            'A server-to-client SockJS event',
            [
                _SearchStartedEvent(strict),
                _PingEvent(strict),
                _ResultEvent(strict),
                _StatisticsEvent(strict),
                _SearchCompleteEvent(strict),
                _ErrorEvent(strict),
            ],
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
