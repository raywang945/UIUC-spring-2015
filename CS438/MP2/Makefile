all: ls manager_send

ls: main.c monitor_neighbors.c monitor_neighbors.h linkstate.c linkstate.h
	gcc -pthread -o ls_router main.c monitor_neighbors.c linkstate.c

manager_send: manager_send.c
	gcc -o manager_send manager_send.c

.PHONY: clean
clean:
	rm ls_router manager_send
