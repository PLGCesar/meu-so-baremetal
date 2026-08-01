#include "../include/elf.h"
#include "../include/serial.h"
#include "../include/memory.h"
#include "../include/task.h"
#include "../include/util.h"

int elf_validate(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(Elf64_Ehdr)) return 0;

    Elf64_Ehdr* header = (Elf64_Ehdr*)data;

    if (header->e_ident[0] != 0x7F ||
        header->e_ident[1] != 'E'  ||
        header->e_ident[2] != 'L'  ||
        header->e_ident[3] != 'F') {
        serial_write("[ELF] Erro: Cabecalho nao possui magica \\x7fELF!\n");
        return 0;
    }

    if (header->e_ident[4] != 2) {
        serial_write("[ELF] Erro: O arquivo nao eh ELF64!\n");
        return 0;
    }

    return 1;
}

int elf_load_and_run(const uint8_t* data, size_t size, const char* name) {
    if (!elf_validate(data, size)) return 0;

    Elf64_Ehdr* header = (Elf64_Ehdr*)data;
    serial_write("[ELF] Executavel ELF64 Valido Encontrado!\n");

    uint64_t entry_point = header->e_entry;

    Elf64_Phdr* phdr = (Elf64_Phdr*)(data + header->e_phoff);
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == 1) { // PT_LOAD
            void* target = (void*)(uintptr_t)phdr[i].p_vaddr;
            if (target && phdr[i].p_memsz > 0) {
                kmemset(target, 0, phdr[i].p_memsz);
                kmemcpy(target, data + phdr[i].p_offset, phdr[i].p_filesz);
            }
        }
    }

    // INJETA O EXECUTAVEL PARA RODAR EM RING 3 NO ESCALONADOR!
    int pid = task_create_user((void (*)(void))(uintptr_t)entry_point, name, 2);
    if (pid >= 0) {
        serial_write("[ELF] Processo ELF64 de Ring 3 Carregado e Ativo!\n");
        return 1;
    }
    return 0;
}
