#include <stdio.h>
#include "vcl.h"
#include "vec.h"
#include "registry.h"

int main() {
    printf("LLFPL1 BOOTING...\n");

    HardwareProfile hp = vcl_discover();
    VEC* v = vec_init(hp);
    Registry* reg=registry_init();

    registry_bind(reg,"Gravity",9.81);
    registry_bind(reg,"PI",3.14159);

    double g=registry_resolve(reg,"Gravity");
    printf("[REGISTRY] Resolved 'Gravity' as: %.2f\n",g);
    registry_shutdown(reg);

    if (v) {
        printf("[VEC] Virtual Silicon Ready at %p\n", (void*)v->r);
        vec_shutdown(v);
    }

    return 0;
}
