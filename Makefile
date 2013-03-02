all:
	gcc -pthread split_router.c helper_methods.c -o split_router

clean:
	rm -vf split_router
