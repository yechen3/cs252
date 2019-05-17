CXX = g++ -fPIC
NETLIBS= -lnsl
CC= gcc

all: git-commit myhttpd daytime-server use-dlopen jj-mod util jj-mod.so hello.so

daytime-server : daytime-server.o
	$(CXX) -o $@ $@.o $(NETLIBS)

myhttpd : myhttpd.o
	$(CXX) -pthread -o $@ $@.o $(NETLIBS) -ldl

use-dlopen: use-dlopen.o
	$(CXX) -o $@ $@.o $(NETLIBS) -ldl

jj-mod: jj-mod.c 
	$(CC) -c -fPIC -o $@.o jj-mod.c

util: util.c 
	$(CC) -c -fPIC -o $@.o util.c

jj-mod.so: jj-mod.o util.o 
	ld -G -o ./http-root-dir/cgi-bin/jj-mod.so jj-mod.o util.o

hello.so: hello.o
	ld -G -o ./http-root-dir/cgi-bin/hello.so hello.o

%.o: %.cc
	@echo 'Building $@ from $<'
	$(CXX) -o $@ -c -I. $<

.PHONY: git-commit
git-commit:
	git checkout
	git add *.cc *.h Makefile >> .local.git.out  || echo
	git commit -a -m 'Commit' >> .local.git.out || echo
	git push origin master 

.PHONY: clean
clean:
	rm -f *.o use-dlopen hello.so
	rm -f *.o daytime-server myhttpd

