#include "client_pool.h"

client *
new_client(client_pool *p, int sock)
{
	client *nc;
	assert(p->clients[sock] == NULL);

	nc = malloc(sizeof(client));
	memset(nc, 0, sizeof(client));
	nc->sock = sock;
	nc->inbuf_size = 0;
	nc->request_incomplete = 0;
	nc->close_connection = 0;
	nc->ssl_connection = 0;

	p->clients[sock] = nc;
	p->num_clients++;
	return p->clients[sock];
}

void
delete_client(client_pool *p, int sock)
{
	assert(p->clients[sock] != NULL);

	p->num_clients--;
	free(p->clients[sock]);
	p->clients[sock] = NULL;
}

client_pool *client_pool_create()
{
	client_pool *p;
	p = malloc(sizeof(*p));
	p->clients = malloc(sizeof(client *) * MAX_CLIENTS);
	memset(p->clients, 0, sizeof(client *) * MAX_CLIENTS);
	p->num_clients = 0;
	return p;
}
