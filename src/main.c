#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "vcl.h"
#include "vec.h"
#include "registry.h"
#include "scanner.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: llfpl1 <source.lp>\n");
        return 1;
    }

    printf("LLFPL1 BOOTING...\n");

    // Initialize VCL and VEC
    HardwareProfile hp = vcl_discover();
    VEC* v = vec_init(hp);
    Registry* reg = registry_init();

    // Map source file via POSIX mmap
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("[ERROR] Failed to open source file");
        return 1;
    }

    struct stat st;
    fstat(fd, &st);

    const char* file_content = mmap(NULL, st.size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_content == MAP_FAILED) {
        perror("[ERROR] mmap failure");
        close(fd);
        return 1;
    }

    Scanner s;
    scanner_init(&s, file_content);

    printf("[SCANNER] Tokenizing Source Pipeline:\n");
    Atom a;
    do {
        a = scanner_next(&s);
        if (a.type != ATOM_EOF && a.type != ATOM_ERROR) {
            printf("  [L:%d] ATOM TYPE: %d | COORD: %p | VAL: %.*s\n", 
                    a.line, a.type, (void*)a.start, (int)a.len, a.start);
        }
    } while (a.type != ATOM_EOF);

    // Clean up
    munmap((void*)file_content, st.size);
    close(fd);
    registry_shutdown(reg);
    vec_shutdown(v);
    return 0;
}
