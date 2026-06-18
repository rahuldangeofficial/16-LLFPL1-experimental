#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "vcl.h"
#include "vec.h"
#include "registry.h"
#include "scanner.h"
#include "resolver.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: llfpl1 <source.LLFPL1>\n");
        return 1;
    }

    /* Enforce the strict .LLFPL1 extension rule */
    const char* filename = argv[1];
    size_t len = strlen(filename);
    const char* ext = ".LLFPL1";
    size_t ext_len = strlen(ext);

    if (len < ext_len || strcmp(filename + len - ext_len, ext) != 0) {
        printf("[ERROR] Invalid file format. System only accepts native '.LLFPL1' source files.\n");
        return 1;
    }

    printf("LLFPL1 BOOTING...\n");

    /* Initialize hardware pipeline */
    HardwareProfile hp = vcl_discover();
    VEC* v = vec_init(hp);
    Registry* reg = registry_init();

    /* Map source file via POSIX mmap */
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("[ERROR] Failed to open source file");
        registry_shutdown(reg);
        vec_shutdown(v);
        return 1;
    }

    struct stat st;
    fstat(fd, &st);

    const char* file_content = mmap(NULL, st.size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_content == MAP_FAILED) {
        perror("[ERROR] mmap failure");
        close(fd);
        registry_shutdown(reg);
        vec_shutdown(v);
        return 1;
    }

    Scanner* s = scanner_create(file_content);
    if (!s) {
        printf("[ERROR] Failed to allocate Scanner.\n");
        munmap((void*)file_content, st.size);
        close(fd);
        registry_shutdown(reg);
        vec_shutdown(v);
        return 1;
    }

    /* Pre-bind test identity */
    registry_bind(reg, "Base_Offset", TYPE_F64, (ValueData){.f64 = 100.0});

    printf("LLFPL1 INDUCTION ENGINE ACTIVE...\n");

    /* Execute the Structural Tree, collapsing into VEC Register 0 */
    double final_state = resolver_evaluate(s, reg, v, 0, 1);
    (void)final_state;

    printf("[VEC FINAL] Cycle Cost: %llu | Target R[0]: %.2f\n",
           vec_get_clock(v), vec_read_register(v, 0));

    /* Clean up */
    scanner_destroy(s);
    munmap((void*)file_content, st.size);
    close(fd);
    registry_shutdown(reg);
    vec_shutdown(v);
    return 0;
}
