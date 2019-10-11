from __future__ import print_function
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

from contextlib import contextmanager
import threading

import simplejson as json
import validictory

_strict_config = threading.local()


@contextmanager
def _strictness(strict):
    '''Context manager to save the strictness setting in a thread-local
    so that we don't have to explicitly pass it to all of the constructors.'''
    if not hasattr(_strict_config, 'enabled'):
        _strict_config.enabled = None
    saved = _strict_config.enabled
    _strict_config.enabled = strict
    try:
        yield
    finally:
        _strict_config.enabled = saved


class _JSONSchema(dict):
    '''A JSON schema.'''

    def __init__(self, title, type, **kwargs):
        '''If strictness is enabled, the type is object, and kwargs does
        not include additionalProperties, we do not accept unknown object
        properties.'''

        if type == 'object' and 'additionalProperties' not in kwargs:
            if getattr(_strict_config, 'enabled', None) is None:
                raise RuntimeError('strictness not specified')
            kwargs['additionalProperties'] = not _strict_config.enabled
        dict.__init__(self, title=title, type=type, **kwargs)

    def validate(self, obj):
        # required_by_default=False and blank_by_default=True for
        # JSON Schema draft 3 semantics
        validictory.validate(obj, self, required_by_default=False,
                             blank_by_default=True)

    def dump(self, file):
        json.dump(self, file, indent=4)


class _SearchBlob(_JSONSchema):
    '''A blob specification.'''

    def __init__(self, title, **kwargs):
        _JSONSchema.__init__(
            self, title, 'object',
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

    def __init__(self, strict=False):
        with _strictness(strict):
            _JSONSchema.__init__(
                self, 'Configuration for a Diamond search', 'object',
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
                            properties=dict(
                                name=_JSONSchema(
                                    'Unique filter name',
                                    'string',
                                    required=True,
                                    minLength=1,
                                ),
                                code=_SearchBlob(
                                    'Pointer to executable code',
                                    required=True,
                                ),
                                blob=_SearchBlob(
                                    'Pointer to data for the blob argument',
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


class SearchConfigResult(_JSONSchema):
    '''The response to a search request.'''

    def __init__(self, strict=False):
        with _strictness(strict):
            _JSONSchema.__init__(
                self, 'Response to a Diamond search request', 'object',
                properties=dict(
                    socket_url=_JSONSchema(
                        'URL of the SockJS socket',
                        'string',
                        required=True,
                        format='uri',
                    ),
                    search_key=_JSONSchema(
                        'The search key to include in the start event',
                        'string',
                        required=True,
                    ),
                    evaluate_url=_JSONSchema(
                        'URL for evaluating the search on data',
                        'string',
                        format='uri',
                    ),
                ),
            )


class EvaluateRequest(_JSONSchema):
    '''A request to evaluate the search on data.'''

    def __init__(self, strict=False):
        with _strictness(strict):
            _JSONSchema.__init__(
                self,
                'A request to evaluate the search on some data', 'object',
                properties=dict(
                    object=_SearchBlob(
                        'The data to evaluate',
                        required=True,
                    ),
                ),
            )


class _SingleEvent(_JSONSchema):
    '''A SockJS event message.'''

    def __init__(self, title, event, data, **kwargs):
        if data is None:
            data = _JSONSchema(
                'Null or an empty object',
                [
                    'null',
                    _JSONSchema(
                        'An empty object',
                        'object',
                    ),
                ],
            )
        _JSONSchema.__init__(
            self, title, 'object',
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
    def __init__(self):
        _SingleEvent.__init__(
            self, 'Start the search', 'start',
            _JSONSchema(
                'Arguments to start event',
                'object',
                required=True,
                properties=dict(
                    search_key=_JSONSchema(
                        'The search key',
                        'string',
                        required=True,
                    ),
                ),
            ),
        )


class _PongEvent(_SingleEvent):
    def __init__(self):
        _SingleEvent.__init__(
            self, 'A response to a ping message', 'pong', None,
        )


class _PauseEvent(_SingleEvent):
    def __init__(self):
        _SingleEvent.__init__(
            self, 'Pause the search', 'pause', None,
            description='''Pause each Diamond server after it
produces the next result.  Note that results may continue to arrive at the
client for an indefinite period.''',
        )


class _ResumeEvent(_SingleEvent):
    def __init__(self):
        _SingleEvent.__init__(
            self, 'Resume the search after it is paused', 'resume', None,
        )


class ClientToServerEvent(_JSONSchema):
    '''A client-to-server SockJS event.'''

    def __init__(self, strict=False):
        with _strictness(strict):
            _JSONSchema.__init__(
                self, 'A client-to-server SockJS event',
                [
                    _StartEvent(),
                    _PongEvent(),
                    _PauseEvent(),
                    _ResumeEvent(),
                ],
            )


class _SearchStartedEvent(_SingleEvent):
    def __init__(self):
        _SingleEvent.__init__(
            self, 'Search has been started', 'search_started',
            _JSONSchema(
                'Information about the search',
                'object',
                required=True,
                properties=dict(
                    search_id=_JSONSchema(
                        'The search ID',
                        'string',
                        required=True,
                    ),
                ),
            ),
        )


class _PingEvent(_SingleEvent):
    def __init__(self):
        _SingleEvent.__init__(
            self, 'A keepalive event', 'ping', None,
            description='The client must respond with a pong event.',
        )


class _AttributeValue(_JSONSchema):
    def __init__(self):
        _JSONSchema.__init__(
            self, 'The value of an object attribute', 'object',
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


class _ResultObject(_JSONSchema):
    def __init__(self, **kwargs):
        _JSONSchema.__init__(
            self, 'The attributes of the result object', 'object',
            properties=dict(
                _ResultURL=_JSONSchema(
                    'The location of this search result',
                    'object',
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
            additionalProperties=_AttributeValue(),
            **kwargs
        )


class ResultObject(_ResultObject):
    '''A search result.'''
    # Specifically, a _ResultObject for external consumption.

    def __init__(self, strict=False):
        with _strictness(strict):
            _ResultObject.__init__(self)


class _ResultEvent(_SingleEvent):
    def __init__(self):
        _SingleEvent.__init__(
            self, 'A search result', 'result',
            _ResultObject(required=True),
        )


class _Statistic(_JSONSchema):
    def __init__(self):
        _JSONSchema.__init__(
            self, 'A statistic', 'number',
        )


class _StatisticsEvent(_SingleEvent):
    def __init__(self):
        _SingleEvent.__init__(
            self, 'A statistics update', 'statistics',
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
        )


class _SearchCompleteEvent(_SingleEvent):
    def __init__(self):
        _SingleEvent.__init__(
            self, 'A notification of search completion', 'search_complete',
            None,
        )


class _ErrorEvent(_SingleEvent):
    def __init__(self):
        _SingleEvent.__init__(
            self, 'A failure report', 'error',
            _JSONSchema(
                'The error detail',
                'object',
                required=True,
                properties=dict(
                    message=_JSONSchema(
                        'The error message',
                        'string',
                        required=True,
                    ),
                ),
            ),
        )


class ServerToClientEvent(_JSONSchema):
    '''A server-to-client SockJS event.'''

    def __init__(self, strict=False):
        with _strictness(strict):
            _JSONSchema.__init__(
                self, 'A server-to-client SockJS event',
                [
                    _SearchStartedEvent(),
                    _PingEvent(),
                    _ResultEvent(),
                    _StatisticsEvent(),
                    _SearchCompleteEvent(),
                    _ErrorEvent(),
                ],
            )


def _main():
    import sys

    if (len(sys.argv) != 3
            or sys.argv[1] not in globals()
            or sys.argv[1][0] == '_'
            or sys.argv[2] not in ('strict', 'permissive')):
        print('Arguments: SchemaClass {strict|permissive}', file=sys.stderr)
        print(file=sys.stderr)
        print('SchemaClass can be one of the following:', file=sys.stderr)
        for name, obj in sorted(globals().items()):
            if (isinstance(obj, type) and issubclass(obj, _JSONSchema)
                    and not name.startswith('_')):
                print('    %-25s: %s' % (name, getattr(obj, '__doc__',
                                         'Undocumented')))
        sys.exit(1)

    globals()[sys.argv[1]](sys.argv[2] == 'strict').dump(sys.stdout)
    sys.stdout.write('\n')


if __name__ == '__main__':
    _main()
