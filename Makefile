all:
	gcc -pthread split_router.c -o split_router

clean:
	rm -vf split_router
