#include "stdio.h"
#include "unistd.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

int g_id = 0;

typedef struct	s_list {
	int id;
	int fd;
	struct s_list *next;
}				c_list;

int add_client(c_list **head, int fd) {
	c_list *track = *head;
	c_list *new_c;

	new_c = (c_list*)malloc(sizeof(c_list));
	if (!new_c)
		return (0);
	new_c->fd = fd;
	new_c->id = g_id;

	if (!track) {
		*head = new_c;
	} else {
		while (track->next)
			track = track->next;
		track->next = new_c;
	}
	g_id++;
	return (g_id - 1);
}

int rm_client(c_list **head, int fd) {
	int id = -1;
	c_list *track = *head;
	c_list *prev = NULL;
	
	if (track) {
		while (track && track->fd != fd) {
			prev = track;
			track = track->next;
		}
		if (track)
			id = track->id;
		if (prev && track && track->next)
			prev->next = track->next;
		if (!prev && track && track->next)
			*head = track->next;
		else
			*head = prev;
		free(track);
	}
	return (id);
}

int get_client_id(c_list *head, int fd) {
	while(head && head->fd != fd)
		head = head->next;
	if (!head)
		return (-1);
	return (head->id);
}

int setup_server(int port) {
	int fd;
	struct sockaddr_in addr;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		printf("socket error: %s", strerror(errno));
		return (0);
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(2130706433);
	addr.sin_port = htons(port);

	if ((bind(fd, (const struct sockaddr*)&addr, sizeof(addr))) != 0) {
		printf("bind error: %s", strerror(errno));
		return (0);
	}

	if (listen(fd, 10) != 0) {
		printf("listen error: %s", strerror(errno));
		return (0);
	}

	return (fd);
}

void write_error(char *str) {
	size_t size = strlen(str);
	write(STDERR_FILENO, str, size);
	write(STDERR_FILENO, "\n", 1);
}

void send_all(int highest_fd, fd_set fd_master, int server_fd, char *str, int fd_sender) {
	int fd_it = 0;
	
	while (fd_it < highest_fd + 1) {
		if (FD_ISSET(fd_it, &fd_master) && fd_it != server_fd && fd_it != fd_sender)
			send(fd_it, str, strlen(str), 0);		
		fd_it++;
	}
}

int main(int argc, char **argv) {
	int server_fd;
	c_list *head;
	int highest_fd;
	fd_set fd_read;
	fd_set fd_master;
	char str[8192];
	int fd_it;
	int id_msg;
	
	if (argc != 2) {
		write_error("Wrong number of arguments");
		exit(1);
	}

	server_fd = setup_server(atoi(argv[1]));
	if (server_fd == 0) {
		write_error("Fatal error");
		exit(1);
	}

	FD_ZERO(&fd_read);
	FD_ZERO(&fd_master);
	FD_SET(server_fd, &fd_master);
	highest_fd = server_fd;
	head = NULL;

	while (1) {
		fd_read = fd_master;
		select(highest_fd + 1, &fd_read, NULL, NULL, NULL);
		fd_it = 0;
		while (fd_it < highest_fd + 1) {
			bzero(str, 8192);
			if (FD_ISSET(fd_it, &fd_read)) {
				struct sockaddr_in c_addr;
				socklen_t len;
				char buffer[4096];
				bzero(&buffer, 4096);
				
				if (fd_it == server_fd) {
					int new_fd;
					len = sizeof(c_addr);
					new_fd = accept(server_fd, (struct sockaddr*)&c_addr, &len);
					if (new_fd > highest_fd)
						highest_fd = new_fd;
					FD_SET(new_fd, &fd_master);
					id_msg = add_client(&head, new_fd);
					sprintf(str, "client %d has joined\n", id_msg);
					send_all(highest_fd, fd_master, server_fd, str, -1);
				} else {
					if (recv(fd_it, buffer, 4096, 0) <= 0) {
						id_msg = rm_client(&head, fd_it);
						if (id_msg >= 0) {
							sprintf(str, "client %d has left\n", id_msg);
							send_all(highest_fd, fd_master, server_fd, str, fd_it);
						}
						close(fd_it);
						FD_CLR(fd_it, &fd_master);
					} else {
						id_msg = get_client_id(head, fd_it);
						sprintf(str, "client %d: %s", id_msg, buffer);
						send_all(highest_fd, fd_master, server_fd, str, fd_it);
					}	
				}
			}
			fd_it++;
		}
	}

	return (0);
}

