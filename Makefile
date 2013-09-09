ifndef CONFIG
CONFIG=Release
endif

.PHONY: all clean config distclean sln dsw

all:
	make -C build/$(CONFIG) all
config:
	mkdir -p build/Release
	cd build/Release && cmake -G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Release  ../.. && cd ../..
	mkdir -p build/Debug
	cd build/Debug && cmake -G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Debug  ../.. && cd ../..
	mkdir -p build/package
clean:
	make -C build/$(CONFIG) clean
sln:
	mkdir -p build
	cd build && cmake .. && cd ..
dsw:
	mkdir -p build
	cd build && cmake -G "Visual Studio 6" .. && cd ..
distclean:
	rm -rf build
%:
	make -C build/$(CONFIG) $@
