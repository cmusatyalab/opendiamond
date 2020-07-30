import os
from setuptools import setup, find_packages, Extension
from setuptools.command.egg_info import egg_info
from opendiamond import (
    __version__, PROJECT_NAME, PROJECT_URL, PROJECT_LICENSE, PROJECT_AUTHOR,
    PROJECT_EMAIL, PROJECT_DESCRIPTION)

with open('README.md', 'rt') as f:
    LONG_DESCRIPTION = f.read()

PACKAGES = find_packages(exclude=['tests', 'tests.*'])
REQUIRES = [
    'future>=0.18.0',
    'lxml>=3.2.1',
    'M2Crypto>=0.21.1',
    'pip>=1.5.6',
    'Pillow>=6.2.0',
    'python-dateutil>=1.5',
    'pyyaml>=5.1.2',
    'six>=1.9.0',
]
REQUIRES_BLASTER = [
    'file-magic>=0.3.0',
    'pycurl>=7.43.0',
    'simplejson>=3.10.0',
    'sockjs-tornado>=1.0.3',
    'tornado>=4.4.2',
    'validictory>=1.1.0',
]
REQUIRES_DATARETRIEVER = [
    'flask>=0.12.4',
]
REQUIRES_DIAMONDD = [
    'docker>=3.7.0',
    'psutil>=5.6.6',
    'PyYAML>=4.2b1',
    'raven>=6.6.0',
    'redis>=2.10.3',
    'xmltodict>=0.11.0',
]
REQUIRES_SCOPESERVER = [
    'Django>=1.11.29',
]
SRC_PATH = os.path.relpath(os.path.dirname(__file__) or '.')


setup(
    classifiers=[
        'License :: OSI Approved :: Eclipse Public License 1.0 (EPL-1.0)',
        'Programming Language :: Python :: 3',
    ],
    name=PROJECT_NAME,
    version=__version__,
    license=PROJECT_LICENSE,
    url=PROJECT_URL,
    description=PROJECT_DESCRIPTION,
    author=PROJECT_AUTHOR,
    author_email=PROJECT_EMAIL,
    long_description=LONG_DESCRIPTION,
    long_description_content_type='text/markdown',
    packages=PACKAGES,
    ext_modules=[],
    zip_safe=False,
    install_requires=REQUIRES,
    extras_require={
        'BLASTER': REQUIRES_BLASTER,
        'DATARETRIEVER': REQUIRES_DATARETRIEVER,
        'DIAMONDD': REQUIRES_DIAMONDD,
        'SCOPESERVER': REQUIRES_SCOPESERVER,
    },
    test_suite='tests',
    include_package_data=True,
    package_dir={
        "": SRC_PATH,
    },
    package_data={
        "opendiamond": [
            "*.xsd",
            "*.xslt",
            "blaster/static/*.js",
            "blaster/*/testui/*",
            "scopeserver/*/static/*/*",
            "scopeserver/*/templates/*/*",
        ],
    },
    entry_points={
        'console_scripts': [
            'blaster = opendiamond.blaster.__main__:run [BLASTER]',
            'diamondd = opendiamond.server.__main__:run [DIAMONDD]',
            'dataretriever = opendiamond.dataretriever.__main__:run'
            ' [DATARETRIEVER]',
        ]
    },
    scripts = [
        'tools/cookiecutter',
        'tools/diamond-bundle-predicate',
        'tools/extract-filters',
    ],
)
