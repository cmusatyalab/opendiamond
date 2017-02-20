import os
from setuptools import setup, find_packages, Extension
from setuptools.command.egg_info import egg_info
from opendiamond import (__version__, PROJECT_NAME, PROJECT_URL,
    PROJECT_LICENSE, PROJECT_AUTHOR, PROJECT_EMAIL, PROJECT_DESCRIPTION)

PACKAGES = find_packages(exclude=['tests', 'tests.*'])
REQUIRES = [
    'pip>=1.5.6',
    'M2Crypto>=0.25.1',
    'Pillow>=4.0.0',
    'lxml>=3.7.3',
    'python-dateutil>=2.6.0',
    'six>=1.10.0',
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
    'Paste>=2.0.3',
]
REQUIRES_DIAMONDD = [
    'redis>=2.10.5',
]
SRC_PATH = os.path.relpath(os.path.dirname(__file__) or '.')

hashmodule = Extension("opendiamond.hash",
    sources = [ "opendiamond/hashmodule.c" ],
)

class EggInfoCommand(egg_info):
    def run(self):
        if "build" in self.distribution.command_obj:
            build_command = self.distribution.command_obj["build"]
            self.egg_base = build_command.build_base
            self.egg_info = os.path.join(self.egg_base,
                                         os.path.basename(self.egg_info))
        egg_info.run(self)

setup(
    name=PROJECT_NAME,
    version=__version__,
    license=PROJECT_LICENSE,
    url=PROJECT_URL,
    description=PROJECT_DESCRIPTION,
    author=PROJECT_AUTHOR,
    author_email=PROJECT_EMAIL,
    packages=PACKAGES,
    ext_modules = [ hashmodule ],
    zip_safe=False,
    install_requires=REQUIRES,
    extra_requires={
        'BLASTER': REQUIRES_BLASTER,
        'DATARETRIEVER': REQUIRES_DATARETRIEVER,
        'DIAMONDD': REQUIRES_DIAMONDD,
    },
    test_suite='tests',
    include_package_data=True,
    package_dir={
        "": SRC_PATH,
    },
    package_data={
        "opendiamond": [
            "blaster/static/*.js",
            "blaster/*/testui/*",
            "scopeserver/*/static/*/*",
            "scopeserver/*/templates/*/*",
        ],
    },
    data_files=[
        ('opendiamond', ['bundle/bundle.xsd']),
    ],
    entry_points={
        'console_scripts': [
            'blaster = opendiamond.blaster.__main__:run [BLASTER]',
            'diamondd = opendiamond.server.__main__:run [DIAMONDD]',
            'dataretriever = opendiamond.dataretriever.__main__:run'
            ' [DATARETRIEVER]',
        ]
    },
    scripts=[
        'tools/cookiecutter',
        'tools/diamond-bundle-predicate',
        'tools/volcano',
    ],
    cmdclass={
        "egg_info": EggInfoCommand,
    },
)
