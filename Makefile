
ifeq ($(OS),Windows_NT)
	target = windows
    ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
    	outname = libfibre-windows-amd64.dll
    else ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
    	outname = libfibre-windows-amd64.dll
    else
        $(error unsupported platform ${PROCESSOR_ARCHITECTURE})
    endif
else ifeq ($(shell uname -s),Linux)
	target = linux
	ifeq ($(shell uname -m),x86_64)
		outname = libfibre-linux-amd64.so
	else ifeq (($(shell uname -m),armv7l)
		outname = libfibre-linux-armhf.so
	else
		$(error unsupported platform)
	endif
else ifeq ($(shell uname -s),Darwin)
	target = macos
	outname = libfibre-macos-multiarch.dylib
endif


FILES=libfibre.cpp \
	platform_support/libusb_transport.cpp \
	legacy_protocol.cpp \
	legacy_object_client.cpp \
	logging.cpp

# libusb-1.0 is in /opt/homebrew/Cellar/libusb/1.0.27
mac:
	clang++ -o $(outname) -shared -fPIC -std=c++11 -I/opt/homebrew/Cellar/libusb/1.0.27/include/libusb-1.0 -I./include -DFIBRE_COMPILE -DFIBRE_ENABLE_CLIENT \
		$(FILES) \
		/opt/homebrew/Cellar/libusb/1.0.27/lib/libusb-1.0.a -framework CoreFoundation -framework IOKit -framework Security -framework CoreServices

linux:
	g++ -shared -o $(outname) -fPIC -std=c++11 -I/usr/include/libusb-1.0 -I./include -DFIBRE_COMPILE -DFIBRE_ENABLE_CLIENT \
		$(FILES) \
		-lusb-1.0
windows:
	g++ -shared -o $(outname) -fPIC -std=c++11 -I./third_party/libusb-windows/libusb-1.0.23/include/libusb-1.0 -I./include -DFIBRE_COMPILE -DFIBRE_ENABLE_CLIENT \
		$(FILES) \
		-static-libgcc -Wl,-Bstatic -lstdc++ ./third_party/libusb-windows/libusb-1.0.23/MinGW64/static/libusb-1.0.a -Wl,-Bdynamic

# To check exported symbols run:
#   nm -D libfibre.so | grep ' T '
