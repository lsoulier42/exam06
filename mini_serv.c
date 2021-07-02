#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#define BUFFER_SIZE 1

typedef struct	s_list {
	int fd;
	int id;
	struct s_list *next;
}		t_list;

t_list *head = NULL;
int server_fd, g_id = 0;
fd_set curr_sock, cpy_read, cpy_write;
char buffer[BUFFER_SIZE + 1], msg[42];
char *str_joined, *for_extract, *for_sprintf;

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

void accept_client() {
	int client_fd;
	struct sockaddr_in c_addr;
	socklen_t len = sizeof(c_addr);

	bzero(&msg, sizeof(msg));
	client_fd = accept(server_fd, (struct sockaddr*)&c_addr, &len);
	if (client_fd < 0)
		write_error(NULL);
	sprintf(msg, "server: client %d just arrived\n", add_client(client_fd));
	send_all(client_fd, msg);
	FD_SET(client_fd, &curr_sock);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void client_leave(int fd) {
	bzero(&msg, sizeof(msg));
	sprintf(msg, "server: client %d just left\n", rm_client(fd));
	send_all(fd, msg);
	FD_CLR(fd, &curr_sock);
	close(fd);
}

void client_receive(int fd) {
	for_sprintf = (char*)calloc(42 + strlen(for_extract), sizeof(char));
	if (!for_sprintf)
		write_error(NULL);
	sprintf(for_sprintf, "client %d: %s", get_id(fd), for_extract);
	free(for_extract);
	send_all(fd, for_sprintf);
	free(for_sprintf);
}

void select_loop(int max_fd) {
	int ret;

	for(int fd = 0; fd < max_fd + 1; fd++) {	
		if (FD_ISSET(fd, &cpy_read)) {
			if (fd == server_fd) {
				accept_client();
				break;
			} else {
				if ((ret = recv(fd, buffer, BUFFER_SIZE, 0)) <= 0) {
					client_leave(fd);
					break;
				} else {
					buffer[ret] = '\0';
					str_joined = str_join(str_joined, buffer);
					if (extract_message(&str_joined, &for_extract) == 1) {
						client_receive(fd);	
						break;
					}
				}
			}
		}
	}
}

void init_server(int port) {
	server_fd = setup_server(port);
	if (server_fd == -1)
		write_error(NULL);

	FD_ZERO(&curr_sock);
	FD_SET(server_fd, &curr_sock);
	bzero(buffer, BUFFER_SIZE + 1);
	str_joined = (char*)calloc(1, sizeof(char));
	if (!str_joined)
		write_error(NULL);
}

int main (int argc, char **argv) {
	int max_fd;
	
	if (argc != 2)
		write_error("Wrong number of arguments\n");
	init_server(atoi(argv[1]));
	while(1) {
		cpy_write = cpy_read = curr_sock;
		max_fd = get_max_fd();
		if (select(max_fd + 1, &cpy_read, &cpy_write, NULL, NULL) < 0)
			continue;
		select_loop(max_fd);
	}
	free(str_joined);
	return (0);
}

