sparkler: main.o json.o fetchnparse.o monitor
		gcc -o $@ main.o json.o fetchnparse.o -lcurl -lm

main.o: main.c
		gcc -c $<

json.o: json.c json.h
		gcc -c $<

fetchnparse.o: fetchnparse.c fetchnparse.h
		gcc -c $<

monitor: monitor.asm
		nasm -f bin $<

.PHONY: clean

clean:
	rm -f sparkler json.o fetchnparse.o main.o monitor
