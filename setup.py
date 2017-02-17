import os
from setuptools import setup, find_packages, Extension
from setuptools.command.egg_info import egg_info
from opendiamond import (__version__, PROJECT_NAME, PROJECT_URL,
    PROJECT_LICENSE, PROJECT_AUTHOR, PROJECT_EMAIL, PROJECT_DESCRIPTION)

PACKAGES = find_packages(exclude=['tests', 'tests.*'])
REQUIRES = [
    'M2Crypto>=0.25.1',
    'Pillow>=4.0.0',
    'file-magic>=0.3.0',
    'pip>=1.5.6',
    'pycurl>=7.43.0',
    'python-dateutil>=2.6.0',
    'simplejson>=3.10.0',
    'six>=1.10.0',
    'sockjs-tornado>=1.0.3',
    'tornado>=4.4.2',
    'validictory>=1.1.0',
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
            'blaster = opendiamond.blaster.__main__:main',
        ]
    },
    cmdclass={
        "egg_info": EggInfoCommand,
    },
)
