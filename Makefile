LIBUV_BRANCH=v1.20.3
LIBEV_BRANCH=master
LIBEVCORO_BRANCH=master
LIBCOMM_BRANCH=master
LIBH2O_BRANCH=v2.2.4
LIBROCKSDB_BRANCH=v5.13.1

letsbuildthis:
	python waf configure
	python waf build

clean:
	python waf clean


PWD=$(shell pwd)


libh2o_build:
	cd deps/h2o && cmake . -DLIBUV_INCLUDE_DIRS=$(PWD)/deps/libuv/include -DLIBUV_LIBRARY_DIRS=$(PWD)/deps/libuv/.libs -DH2O_USE_LIBUV=1 -DLIBUV_FOUND=1 -DOPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include
	cd deps/h2o && make libh2o
	cp deps/h2o/libh2o.a .
.PHONY : libh2o_build

libh2o_fetch:
	if test -e deps/h2o; then \
		cd deps/h2o && rm -f CMakeCache.txt ; \
	else \
		git clone https://github.com/h2o/h2o deps/h2o && cd deps/h2o && git pull origin $(LIBH2O_BRANCH); \
	fi
	cd deps/h2o && git checkout $(LIBH2O_BRANCH)
.PHONY : libh2o_fetch

libh2o: libh2o_fetch libh2o_build
.PHONY : libh2o




libuv_build:
	cd deps/libuv && sh autogen.sh
	cd deps/libuv && ./configure
	cd deps/libuv && make
	cp deps/libuv/.libs/libuv.a .
.PHONY : libuv_build

libuv_fetch:
	if test -e deps/libuv; then \
		cd deps/libuv ; \
	else \
		git clone https://github.com/libuv/libuv deps/libuv && cd deps/libuv && git pull origin $(LIBUV_BRANCH) ; \
	fi
	cd deps/libuv && git checkout $(LIBUV_BRANCH)
.PHONY : libuv_fetch

libuv: libuv_fetch libuv_build
.PHONY : libuv





libev_build:
	cd deps/libev && sh build.sh
	cp deps/libev/linux/lib/libev.a .
.PHONY : libev_build

libev_fetch:
	if test -e deps/libev; then \
		cd deps/libev ; \
	else \
		git clone https://github.com/vgfree/libev deps/libev && cd deps/libev && git pull origin $(LIBEV_BRANCH) ; \
	fi
	cd deps/libev && git checkout $(LIBEV_BRANCH)
.PHONY : libev_fetch

libev: libev_fetch libev_build
.PHONY : libev





libevcoro_build:
	#-DCORO_UCONTEXT"
	#if you want use libevcoro to operate a file-descriptor of libzmq, then you need to define a macro like "FEED_EVENT"
	$(MAKE) -C deps/libevcoro CFLAGS="-g -O1 -DFEED_EVENT"
	cp deps/libevcoro/lib/libevcoro.a .
.PHONY : libevcoro_build

libevcoro_fetch:
	if test -e deps/libevcoro; then \
		cd deps/libevcoro ; \
	else \
		git clone http://gitlab.sihuatech.com/common/libevcoro.git deps/libevcoro && cd deps/libevcoro && git pull origin $(LIBEVCORO_BRANCH) ; \
	fi
	cd deps/libevcoro && git checkout $(LIBEVCORO_BRANCH)
.PHONY : libevcoro_fetch

libevcoro: libevcoro_fetch libevcoro_build
.PHONY : libevcoro





libcomm_build:
	cd deps/libcomm && make lib
	cp deps/libcomm/lib/libcomm.a .
.PHONY : libcomm_build

libcomm_fetch:
	if test -e deps/libcomm; then \
		cd deps/libcomm ; \
	else \
		git clone http://gitlab.sihuatech.com/network/libcomm.git deps/libcomm && cd deps/libcomm && git pull origin $(LIBCOMM_BRANCH) ; \
	fi
	cd deps/libcomm && git checkout $(LIBCOMM_BRANCH)
.PHONY : libcomm_fetch

libcomm: libcomm_fetch libcomm_build
.PHONY : libcomm





librocksdb_build:
	cd deps/rocksdb && make static_lib
	cp deps/rocksdb/librocksdb.a ./librocksdb.a
.PHONY : librocksdb_build

librocksdb_fetch:
	if test -e deps/rocksdb; then \
		cd deps/rocksdb ; \
	else \
		git clone https://github.com/facebook/rocksdb deps/rocksdb && cd deps/rocksdb && git pull origin $(LIBROCKSDB_BRANCH) ; \
	fi
	cd deps/rocksdb && git checkout $(LIBROCKSDB_BRANCH)
.PHONY : librocksdb_fetch

librocksdb: librocksdb_fetch librocksdb_build
.PHONY : librocksdb




libdb_build:
	cd deps/libdb/build_unix/ && sh ../dist/configure && make
	cp deps/libdb/build_unix/.libs/libdb-6.2.a ./libdb.a
.PHONY : libdb_build

libdb_fetch:
	if test -e deps/libdb; then \
		cd deps/libdb ; \
	else \
		git clone https://github.com/vgfree/libdb.git deps/libdb ; \
	fi
.PHONY : libdb_fetch

libdb: libdb_fetch libdb_build
.PHONY : libdb
