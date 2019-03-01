make:
	cc hello.c -o hello `pkgconf fuse --cflags --libs`

clean:
	rm hello
