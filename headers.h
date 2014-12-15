int headers_consume (int length, char *buffer);
void headers_cleanup ();
char *get_request (int part);
char *get_header (int header, int part);
char *headers_get_error (int error);
char *headers_get_current_error ();
int headers_has_version ();
int headers_has_request ();
