run: a.out
	./a.out < ff.c > out.s
	cc out.s -o out
	./out

test: a.out
	@zsh tests/run.sh

a.out: ff.c
	gcc ff.c

.PHONY: run test
