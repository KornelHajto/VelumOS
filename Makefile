all:
	gcc -o os kernel/main.c hal/linux/udp_mock.c -Ihal

clean:
	rm os