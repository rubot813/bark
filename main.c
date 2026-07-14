#include <stdio.h>
#include <stdlib.h>
#include "bark.h"

int main() {
	FILE *f = fopen("H:\\bark_v1\\bark\\sample\\bb.txt", "rb");
	if (!f)
		return 1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    rom_t *rom = bark_rom_init();
    word_t status = bark_compile(buf, rom);
    if (status == cst_success) {
		vm_t *vm = bark_vm_init(rom);
		word_t ex_status = est_work;

		// Параметры запуска.
		//char *str = "file path!";
		//bark_vm_push(vm, (word_t)(str));
		// bark_vm_push(vm, (word_t)(1));
		//bark_vm_push(vm, (word_t)(0));

		while (ex_status < est_halt)
			ex_status = bark_exec(vm);
		printf("#done. status: %d, sp: %d\r\n", ex_status, vm->sp);
    } else
		printf("#compile error: %d\r\n", status);
    return 0;
}
