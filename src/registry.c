#define _POSIX_C_SOURCE 200112L
#include "registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uint32_t hash_identity(const char* str){
    uint32_t hash=0;
    while(*str){
        hash+=*str++;
        hash+=(hash<<10);
        hash^=(hash>>6);
    }
    hash+=(hash<<3);
    hash^=(hash>>11);
    hash+=(hash<<15);
    return hash%MAX_IDENTITIES;
}

Registry* registry_init(){
    Registry* reg=(Registry*)malloc(sizeof(Registry));
    if(!reg){
        return NULL;
    }

    void** entries_address=(void**)(&((*reg).entries));
    if(posix_memalign(entries_address, SYS_CACHE_LINE, sizeof(Identity)*MAX_IDENTITIES)!=0){
        free(reg);
        return NULL;
    }

    memset((*reg).entries, 0, sizeof(Identity)*MAX_IDENTITIES);
    (*reg).count = 0;
    return reg;
}

void registry_bind(Registry* reg, const char* name, double value){
    if((*reg).count>=MAX_IDENTITIES){
        printf("[FATAL] Registry Memory Slab Exhausted.\n");
        return; 
    }

    uint32_t slot=hash_identity(name);
    
    // Construct the incoming axiom
    Identity incoming;
    strncpy(incoming.name, name, 19);
    incoming.name[19]='\0';
    incoming.value=value;
    incoming.active=1;
    incoming.psl=0;

    while(1){
        // Condition 1: Slot is empty, drop it in.
        if(!(*((*reg).entries+slot)).active){
            (*((*reg).entries+slot))=incoming;
            (*reg).count++;
            return;
        }

        // Condition 2: Axioms are immutable. If it exists, ignore.
        if(strcmp((*((*reg).entries+slot)).name, incoming.name)==0){
            return;
        }

        // Condition 3: ROBIN HOOD SWAP
        // Steal from the rich (low PSL) to give to the poor (high PSL)
        if(incoming.psl>(*((*reg).entries+slot)).psl){
            Identity temp=(*((*reg).entries+slot));
            (*((*reg).entries+slot))=incoming;
            incoming=temp;
        }

        // Move to the next slot and increase the distance tracker
        slot=(slot+1)%MAX_IDENTITIES;
        incoming.psl++;
    }
}

double registry_resolve(Registry* reg, const char* name){
    uint32_t slot=hash_identity(name);
    uint8_t current_psl=0;

    while((*((*reg).entries+slot)).active){
        if(strcmp((*((*reg).entries+slot)).name, name)==0){
            return (*((*reg).entries+slot)).value; // Cache hit
        }
        
        // ROBIN HOOD EARLY EXIT
        // If the element we are looking at is closer to its target hash 
        // than our current search distance, our item cannot be in this table.
        if(current_psl>(*((*reg).entries+slot)).psl){
            return 0.0;
        }

        slot=(slot+1)%MAX_IDENTITIES;
        current_psl++;
    }
    return 0.0; // Default undefined state
}

void registry_shutdown(Registry* reg){
    if(reg){
        free((*reg).entries);
        free(reg);
    }
}
