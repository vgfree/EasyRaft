LIBUV_BRANCH=v1.20.3
LIBH2O_BRANCH=v2.2.4
LIBROCKSDB_BRANCH=v5.13.1

letsbuildthis:
	python waf configure
	python waf build

clean:
	python waf clean




libh2o_build:
	cd deps/h2o && cmake . -DCMAKE_INCLUDE_PATH=../libuv/include -DLIBUV_LIBRARIES=1 -DOPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include
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
