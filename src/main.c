#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "vcl.h"
#include "vec.h"
#include "registry.h"
#include "scanner.h"
#include "resolver.h"
#include "template.h"

/* ── Private Helpers ───────────────────────────────────────────── */

static void atom_to_str(Atom a, char* dest, size_t dest_size) {
    size_t copy_len = (a.len < dest_size - 1) ? a.len : dest_size - 1;
    memcpy(dest, a.start, copy_len);
    dest[copy_len] = '\0';
}

/* ── Entry Point ───────────────────────────────────────────────── */

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
    CodeSegment* cs = cs_init();

    /* Map source file via POSIX mmap */
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("[ERROR] Failed to open source file");
        cs_shutdown(cs);
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
        cs_shutdown(cs);
        registry_shutdown(reg);
        vec_shutdown(v);
        return 1;
    }

    Scanner* s = scanner_create(file_content);
    if (!s) {
        printf("[ERROR] Failed to allocate Scanner.\n");
        munmap((void*)file_content, st.size);
        close(fd);
        cs_shutdown(cs);
        registry_shutdown(reg);
        vec_shutdown(v);
        return 1;
    }

    printf("LLFPL1 GLOBAL ROUTER ACTIVE...\n");

    /* ── Top-level dispatch loop ───────────────────────────────── */
    Atom a;
    while ((a = scanner_next(s)).type != ATOM_EOF) {
        if (a.type != ATOM_VERB) continue;

        char keyword[32];
        atom_to_str(a, keyword, sizeof(keyword));

        if (strcmp(keyword, "Identity") == 0) {
            scanner_next(s);  /* '(' */
            Atom name = scanner_next(s);
            scanner_next(s);  /* ',' */
            Atom val  = scanner_next(s);
            scanner_next(s);  /* ')' */

            char n_buf[32], v_buf[32];
            atom_to_str(name, n_buf, sizeof(n_buf));
            atom_to_str(val, v_buf, sizeof(v_buf));

            registry_bind(reg, n_buf, TYPE_F64, (ValueData){.f64 = atof(v_buf)});
            printf("[GLOBAL] Locked Identity: %s = %s\n", n_buf, v_buf);
        }
        else if (strcmp(keyword, "Map") == 0) {
            cs_define_map(cs, s);
            printf("[GLOBAL] Mapped Template structure to CodeSegment.\n");
        }
        else if (strcmp(keyword, "Commit") == 0) {
            scanner_next(s);  /* '(' */
            double final_state = resolver_evaluate(s, reg, cs, v, NULL, 0, 1);
            scanner_next(s);  /* ')' */

            printf("[VEC COMMIT] Cycle Cost: %llu | Hardware State R[0]: %.2f\n",
                   vec_get_clock(v), final_state);
        }
    }

    /* Clean up */
    scanner_destroy(s);
    munmap((void*)file_content, st.size);
    close(fd);
    cs_shutdown(cs);
    registry_shutdown(reg);
    vec_shutdown(v);
    return 0;
}
