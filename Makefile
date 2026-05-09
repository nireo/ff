run: a.out
	./a.out < ff.c > out.s
	cc out.s -o out
	./out

a.out: ff.c
	gcc ff.c
