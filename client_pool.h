/*
 * client_pool.h
 *
 *      Copyright (c) <2013> <Fahad Islam>
 */
#ifndef _CLIENT_POOL_H_
#define _CLIENT_POOL_H_

#include "main.h"

typedef struct {
	client **clients;
	int num_clients;
} client_pool;

client_pool *client_pool_create();
client *new_client(client_pool *p, int sock);
void delete_client(client_pool *p, int sock);


#endif /* _CLIENT_POOL_H_ */
