ifndef CONFIG
CONFIG=Release
endif

ifdef VERSION
CMAKE_VER=-D VERSION=$(VERSION)
endif

.PHONY: all clean config distclean sln

ifndef INSTALL_PREFIX
CMAKE_INSTALL_PREFIX=install
else
CMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX)
endif
all:
	make -C build/$(CONFIG) all
config:
	mkdir -p build/Release
	cd build/Release && cmake -G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=$(CMAKE_INSTALL_PREFIX) $(CMAKE_VER) ../.. && cd ../..
	mkdir -p build/Debug
	cd build/Debug && cmake -G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Debug -D CMAKE_INSTALL_PREFIX=$(CMAKE_INSTALL_PREFIX) $(CMAKE_VER) ../.. && cd ../..
	mkdir -p build/package
config_package:
ifndef INSTALL_PREFIX
	cd build/package && cmake -G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Release $(CMAKE_VER) ../.. && cd ../..
else
	cd build/package && cmake -G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) $(CMAKE_VER) ../.. && cd ../..
endif
all_package: config_package
	make -C build/package all
package: all_package
	make -C build/package package
clean_package:
	make -C build/package clean
clean:
	make -C build/$(CONFIG) clean
sln:
	mkdir -p build
	cd build && cmake $(CMAKE_VER) .. && cd ..
dsw:
	mkdir -p build
	cd build && cmake $(CMAKE_VER) -G "Visual Studio 6" .. && cd ..
distclean:
	rm -rf build
%:
	make -C build/$(CONFIG) $@
