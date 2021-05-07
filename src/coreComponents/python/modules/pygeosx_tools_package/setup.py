from distutils.core import setup

setup(name='pygeosx_tools',
      version='0.1.0',
      description='Tools for interacting with pygeosx',
      author='Chris Sherman',
      author_email='sherman27@llnl.gov',
      packages=['pygeosx_tools',
                'pygeosx_tools.geophysics'],
      install_requires=['matplotlib', 'pyevtk'])

