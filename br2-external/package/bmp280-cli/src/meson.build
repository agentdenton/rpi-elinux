project('bmp280-cli', 'c', version: '0.1', default_options: ['c_std=c17'])

debug_defs = [
  '-DDEBUG=1'
]

debug_args = [
  '-g2',
  '-O0'
] + debug_defs

release_args = [
  '-O2'
]

build_args = [
  '-Wall',
  '-Wextra',
]

if get_option('buildtype') == 'debug'
  build_args += debug_args
else
  build_args += release_args
endif

srclist = [
  'main.c'
]

executable('bmp280-cli',
  srclist,
  c_args: build_args,
)
