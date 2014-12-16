inline void putch (int, char);
int init_sock (int port);
void non_block (int fd);
void epoll_add (int epollfd, int fd);
int epoll();
