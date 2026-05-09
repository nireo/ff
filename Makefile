run: a.out
	./a.out < ff.c

a.out: ff.c
	gcc ff.c
