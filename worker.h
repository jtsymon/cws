const int worker_count;

void worker_init (int port);
void check_workers();
void spawn_workers (int new_count);
void finish (int force);
