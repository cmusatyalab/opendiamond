#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2017-2018 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from future import standard_library
standard_library.install_aliases()
from builtins import map
from builtins import str
import io
import os
from zipfile import ZipFile

from opendiamond.scope import ScopeCookie

from opendiamond.client.search import Blob, FilterSpec


def create_blob_argument(*paths):
    """
    Create a Blob containing the zip file of examples.
    :param paths: paths of example files to include in the zip.
    :return: A Blob.
    """
    sio = io.BytesIO()
    zf = ZipFile(sio, 'w')
    for i, f in enumerate(paths):
        zf.write(filename=f, arcname=os.path.join("examples", str(i) + os.path.splitext(f)[1]))
    zf.close()
    return Blob(data=sio.getvalue())


def create_filter_from_files(filter_name,
                             code_path,
                             args=(),
                             example_paths=None,
                             dependencies=(),
                             min_score=float('-inf'),
                             max_score=float('inf'),
                             blob_zip_path=None
                             ):
    """
    Create a FilterSpec using code file and example files on disk.
    Provide sanity check and convenient conversion of several parameters.
    :param filter_name: Filter name. Will be converted to string.
    :param code_path: Path of the code file.
    :param args: Filter arguments. Will be converted to strings.
    :param example_paths: List of paths of example files. Will create zip from them.
    :param dependencies: List of dependent filter names. Will be converted to strings.
    :param min_score: float.
    :param max_score: float
    :param blob_zip_path: Alternative path of blob argument zip file.
    Cannot be specified with example_paths at the same time.
    :return: A FilterSpec.
    """
    assert not (bool(example_paths) and bool(blob_zip_path)), \
        "example_path and blob_zip_path should not be given at the same time."
    filter_name = str(filter_name)
    with open(code_path, 'rb') as f:
        code_blob = Blob(data=f.read())
    args = list(map(str, args))
    dependencies = list(map(str, dependencies))

    if example_paths:
        assert isinstance(example_paths, list) or isinstance(example_paths, tuple)
        example_blob = create_blob_argument(*example_paths)
    elif blob_zip_path:
        with open(blob_zip_path, 'rb') as f:
            example_blob = Blob(f.read())
    else:
        example_blob = Blob()

    return FilterSpec(name=filter_name, code=code_blob, arguments=args, blob_argument=example_blob,
                      dependencies=dependencies, min_score=min_score, max_score=max_score)


def get_default_rgb_filter():
    code_path = os.path.join(os.environ['HOME'], '.diamond', 'filters', 'fil_rgb')
    rgb = create_filter_from_files(filter_name='RGB', code_path=code_path, min_score=1.0)
    return rgb


def get_default_scopecookies():
    """
    Load and parse `$HOME/.diamond/NEWSCOPE`
    :return:
    """
    scope_file = os.path.join(os.environ['HOME'], '.diamond', 'NEWSCOPE')
    data = open(scope_file, 'rt').read()
    cookies = [ScopeCookie.parse(c) for c in ScopeCookie.split(data)]
    return cookies
