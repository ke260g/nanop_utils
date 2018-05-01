.PHONY:
default:  test daemon

test: mvsdk_iface.c cv_demo.cpp
	g++ `pkg-config --cflags opencv` -o $@ $^ `pkg-config --libs opencv`

daemon: mvsdk_ctrld.c jpeg_simp.c
	gcc -o $@ $^ -lMVSDK -lpthread -ljpeg

clean:
	rm test daemon
