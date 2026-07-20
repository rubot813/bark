#include <stdio.h>
#include <stdlib.h>
#include "bark.h"

/*
	Bark language bootstrap.
	Compiling:
	gcc -std=c11 bark.c bark_ext.c main.c -o bark
	clang -std=c11 bark.c bark_ext.c main.c -o bark
*/

int main(int argc, char *argv[]) {
	// Проверка аргументов командной строки.
	if (argc < 2) {
		printf("Usage: %s <source file path> <optional bark param>\r\n", argv[0]);
		return EXIT_FAILURE;
	}	// if argc

	// Открытие файла.
	FILE *file = fopen(argv[1], "rb");
	if (file == NULL) {
		printf("Error: could not open file %s\r\n", argv[1]);
		return EXIT_FAILURE;
	}	// if file

	// Определить размер файла.
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Выделение памяти под буфер файла.
    char *buffer = malloc(file_size + 1);
    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';
    fclose(file);

    // Инициализация ROM и компиляция программы.
    rom_t *rom = bark_rom_init();
    word_t status = bark_compile(buffer, rom);

    // Если компиляция успешна
    if (status == cst_success) {
		// Инициализация машины.
		vm_t *vm = bark_vm_init(rom);
		word_t ex_status = est_work;

		// Обработка параметров запуска программы.
		word_t arg_count = 0;
		for (int arg = 2; arg < argc; arg++) {
			word_t value = atoi(argv[arg]);
			bark_vm_push(vm, value);
			arg_count++;
		}	// for arg

		// Соглашение о вызове VA bark:
		// на вершине стека остается общее число аргументов.
		if (arg_count)
			bark_vm_push(vm, arg_count);

		// Выполнение программы до ее завершения.
		while (ex_status < est_halt)
			ex_status = bark_exec(vm, 1024);

		// Программа завершена успешно.
		printf("Program finished with status: %d, sp: %d\r\n", (int)(ex_status), vm->sp);
		
		// Очистка памяти, выделенной под машину.
		bark_vm_free(vm);
    } else
		// Компиляция завершена с ошибкой.
		printf("Compilation error with code: %d\r\n", (int)(status));
		
	// Очистка памяти и выход.
	bark_rom_free(rom);
	free(buffer);
    return EXIT_SUCCESS;
}	// main
