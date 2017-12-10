import StringIO
import os
from zipfile import ZipFile

from opendiamond.scope import ScopeCookie

from opendiamond.client.search import EmptyBlob, BinaryBlob, FilterSpec


def create_blob_argument(*paths):
    """
    Create a Blob containing the zip file of examples.
    :param paths: paths of example files to include in the zip.
    :return: A Blob.
    """
    sio = StringIO.StringIO()
    zf = ZipFile(sio, 'w')
    for i, f in enumerate(paths):
        zf.write(filename=f, arcname=os.path.join("examples", str(i) + os.path.splitext(f)[1]))
    zf.close()
    return BinaryBlob(data=sio.getvalue())


def create_filter_from_files(filter_name,
                             code_path,
                             args=tuple([]),
                             example_paths=None,
                             dependencies=tuple([]),
                             min_score=float('-inf'),
                             max_score=float('inf'),
                             ):
    """
    Create a FilterSpec using code file and example files on disk.
    Provide sanity check and convenient conversion of several parameters.
    :param filter_name: Filter name. Will be converted to string.
    :param code_path: Path of the code file.
    :param args: Filter arguments. Will be converted to strings.
    :param example_paths: List of paths of example files.
    :param dependencies: List of dependent filter names. Will be converted to strings.
    :param min_score: float.
    :param max_score: float
    :return: A FilterSpec.
    """
    filter_name = str(filter_name)
    code_blob = BinaryBlob(data=open(code_path).read())
    args = map(str, args)
    dependencies = map(str, dependencies)

    if example_paths:
        assert isinstance(example_paths, list) or isinstance(example_paths, tuple)
        example_blob = create_blob_argument(*example_paths)
    else:
        example_blob = EmptyBlob()

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
