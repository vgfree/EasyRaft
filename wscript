# -*- mode: python -*-
# vi: set ft=python :

import sys
import os


def options(opt):
        opt.load('compiler_c')


def configure(conf):
    conf.load('compiler_c')
    conf.load('clib')


def build(bld):
    bld.load('clib')

    includes = []

    cflags = """
        -Werror=int-to-pointer-cast
        -g
        -O0
        -Werror=unused-variable
        -Werror=return-type
        -Werror=uninitialized
        -Werror=pointer-to-int-cast
    """.split()

    lib = ['uv', 'h2o', 'ssl', 'crypto', 'rocksdb', 'snappy', 'z', 'bz2', 'lz4', 'db']

    libpath = [os.getcwd()]
    libpath.append('/usr/lib/x86_64-linux-gnu')

    if sys.platform == 'darwin':
        cflags.extend("""
            -fcolor-diagnostics
            -fdiagnostics-color
            """.split())

        # Added due to El Capitan changes
        includes.append('/usr/local/opt/openssl/include')
        libpath.append('/usr/local/opt/openssl/lib')

    elif sys.platform.startswith('linux'):
        cflags.extend("""
            -DLINUX
            """.split())
        lib.append('pthread')
        lib.append('stdc++')
        lib.append('rt')
        lib.append('dl')
        lib.append('m')

    clibs = """
        h2o_helpers
        lmdb
        lmdb_helpers
        raft
        eraft
        carg_parser
        uv_helpers
        uv_multiplex
        """.split()

    h2o_includes = """
        ./deps/h2o/include
        ./deps/picohttpparser
        ./deps/klib
        """.split()

    uv_includes = """
        ./deps/libuv/include
        """.split()

    rocksdb_includes = """
        ./deps/rocksdb/include
        """.split()

    libdb_includes = """
        ./deps/libdb/build_unix
        """.split()

    bld.program(
        source="""
        example/ticketd/main.c
        example/ticketd/usage.c
        example/ticketd/http_context.c
        """.split() + bld.clib_c_files(clibs),
        includes=['./include'] + includes + bld.clib_h_paths(clibs) + h2o_includes + uv_includes + rocksdb_includes + libdb_includes,
        target='ticketd',
        stlibpath=['.'],
        libpath=libpath,
        lib=lib,
        cflags=cflags)

    bld.program(
        source="""
        example/analyse/main.c
        example/analyse/usage.c
        """.split() + bld.clib_c_files(clibs),
        includes=['./include'] + includes + bld.clib_h_paths(clibs) + h2o_includes + uv_includes + rocksdb_includes + libdb_includes,
        target='analyse',
        stlibpath=['.'],
        libpath=libpath,
        lib=lib,
        cflags=cflags)
