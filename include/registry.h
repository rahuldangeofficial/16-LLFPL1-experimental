#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdio.h>
#include <stdint.h>

#define MAX_IDENTITIES 1024

typedef struct{
	char name[20];
	double value;
	uint8_t active;
	uint8_t padding[3];
}Identity;

typedef struct{
	Identity*entries;
	uint32_t count;
}Registry;

Registry* registry_init();
void registry_bind(Registry* reg,const char* name,double value);
double registry_resolve(Registry* reg,const char* name);
void registry_shutdown(Registry* reg);

#endif
