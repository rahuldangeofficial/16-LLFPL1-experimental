#include <stdio.h>
#include <string.h>
#include "vcl.h"
#include "vec.h"
#include "registry.h"
#include "reduction.h"

int main(void) {
    printf("LLFPL1 BOOTING...\n");

    HardwareProfile hp = vcl_discover();
    VEC* v = vec_init(hp);
    Registry* reg = registry_init();

    /* 1. Bind an Identity (Testing Robin Hood lookup) */
    registry_bind(reg, "Base_Offset", TYPE_F64, (ValueData){.f64 = 100.0});
    
    ValueType out_type;
    ValueData base_data = registry_resolve(reg, "Base_Offset", &out_type);
    double base = (out_type == TYPE_F64) ? base_data.f64 : 0.0;

    /* 2. Resolve a primitive verb */
    PrimitiveOp op = reduction_resolve_op("plus");

    /* 3. Apply the reduction directly into Virtual Register 0 */
    if (op != OP_UNKNOWN) {
        double current_val = 50.5;
        printf("[REDUCTION] Collapsing %s(%.2f, %.2f) into VEC.r[0]\n", "plus", current_val, base);
        
        reduction_apply(v, op, current_val, base, 0);

        printf("[VEC STATE] Clock Cycles: %llu | Dirty Mask: 0x%X\n", vec_get_clock(v), vec_get_dirty_mask(v));
        printf("[VEC STATE] Register 0 Value: %.2f\n", vec_read_register(v, 0));
    }

    registry_shutdown(reg);
    vec_shutdown(v);
    return 0;
}
