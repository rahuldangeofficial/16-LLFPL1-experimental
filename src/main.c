#include <stdio.h>
#include "vcl.h"
#include "vec.h"

int main() {
    printf("LLFPL1 BOOTING...\n");

    HardwareProfile hp = vcl_discover();
    VEC* v = vec_init(hp);

    if (v) {
        printf("[VEC] Virtual Silicon Ready at %p\n", (void*)v->r);
        vec_shutdown(v);
    }

    return 0;
}
