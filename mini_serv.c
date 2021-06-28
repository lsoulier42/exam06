#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>

typedef struct	s_list {
	int fd;
	int id;
	struct s_list *next;
}		t_list;

t_list *head = NULL;
int server_fd, g_id = 0;
fd_set curr_sock, cpy_read, cpy_write;
char msg[42], str[42*4096], tmp[42*4096], buf[42*4096 + 42];

void write_error(char *error_str) {
	char *to_write = error_str ? error_str : "Fatal error\n";
	write(2, to_write, strlen(to_write));
	if (server_fd > 0)
		close(server_fd);
	exit(1);
}

int get_id(int fd) {
	int id = -1;
	t_list *track = head;
	while (track && track->fd != fd)
		track = track->next;
	if (track)
		id = track->id;
	return (id);
}

int get_max_fd() {
	int max = server_fd;
	t_list *track = head;

	while (track) {
		if (track->fd > max)
			max = track->fd;
		track = track->next;
	}
	return (max);
}

void send_all(int fd, char *send_str) {
	t_list *track = head;

	while(track) {
		if (track->fd != fd && FD_ISSET(track->fd, &cpy_write))
			if(send(track->fd, send_str, strlen(send_str), 0) < 0)
				write_error(NULL);
		track = track->next;
	}
}

int add_client(int fd) {
	t_list *new_el;
	t_list *track = head;
	
	new_el = (t_list*)malloc(sizeof(t_list));
	if (!new_el)
		write_error(NULL);
	new_el->id = g_id++;
	new_el->fd = fd;
	new_el->next = NULL;
	if(!track)
		head = new_el;
	else {
		while(track->next)
			track = track->next;
		track->next = new_el;
	}
	return (new_el->id);
}

int rm_client(int fd) {
	t_list *prev = NULL;
	t_list *track = head;
	int id = -1;

	while (track && track->fd != fd) {
		prev = track;
		track = track->next;
	}
	if (track) {
		id = track->id;
		if (!prev)
			head = track->next;
		else
			prev->next = track->next;
		free(track);
	}
	return (id);
}

int setup_server(int port) {
	int fd;
	struct sockaddr_in addr;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return (-1);
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(2130706433);
	addr.sin_port = htons(port);
	if (bind(fd, (const struct sockaddr*)&addr, sizeof(addr)) < 0)
		return (-1);
	if (listen(fd, 0) < 0)
		return (-1);
	return (fd);
}

int accept_client() {
	int client_fd;
	struct sockaddr_in c_addr;
	socklen_t len = sizeof(c_addr);

	client_fd = accept(server_fd, (struct sockaddr*)&c_addr, &len);
	if (client_fd < 0)
		write_error(NULL);
	sprintf(msg, "server: client %d just arrived\n", add_client(client_fd));
	send_all(client_fd, msg);
	FD_SET(client_fd, &curr_sock);
}

int ex_msg(int fd) {
	int i = 0;
	int j = 0;
	
	while(str[i]) {
		tmp[j] = str[i];
		j++;
		if (str[i] == '\n') {
			sprintf(buf, "client %d: %s", get_id(fd), tmp);
			send_all(fd, buf);
			j = 0;
			bzero(&tmp, strlen(tmp));
			bzero(&buf, strlen(buf));
		}
		i++;
	}
	bzero(&str, strlen(str));
}

int main (int argc, char **argv) {
	int max_fd;
	
	if (argc != 2)
		write_error("Wrong number of arguments\n");
	server_fd = setup_server(atoi(argv[1]));
	if (server_fd == -1)
		write_error(NULL);

	FD_ZERO(&curr_sock);
	FD_SET(server_fd, &curr_sock);
	bzero(&tmp, sizeof(tmp));
	bzero(&buf, sizeof(buf));
	bzero(&str, sizeof(str));

	while(1) {
		cpy_write = cpy_read = curr_sock;
		max_fd = get_max_fd();
		if (select(max_fd + 1, &cpy_read, &cpy_write, NULL, NULL) < 0)
			continue;
		for(int fd = 0; fd < max_fd + 1; fd++) {
			if (FD_ISSET(fd, &cpy_read)) {
				if (fd == server_fd) {
					bzero(&msg, sizeof(msg));
					accept_client();
					break;
				}
				else {
					if (recv(fd, str, sizeof(str), 0) <= 0) {
						bzero(&msg, sizeof(msg));
						sprintf(msg, "server: client %d just left\n", rm_client(fd));
						send_all(fd, msg);
						FD_CLR(fd, &curr_sock);
						close(fd);
						break;
					} else {
						ex_msg(fd);
					}
				}
			}
		}
	}
	return (0);
}
	

