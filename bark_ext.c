#include "bark.h"

// C stdlib.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Макросы максимального количества аргументов внешних функций.
#define EXT_MAX_ARG_COUNT 8

// Макрос определения простой внешней функции.
#define EXT_DEFINE_FUNC(arg_count, func)	\
if (arg_read(arg_count, &arg, vm)) {		\
	func;									\
} else										\
	status = est_ext_stack_error;

// Макрос определения va_args внешней функции.
#define EXT_DEFINE_VA(sp_count, func) 	\
if (vm->sp >= sp_count) {				\
	word_t arg_count = STACK_POP;		\
	func;								\
} else									\
	status = est_ext_stack_error;

// Массив имен внешних функций по ANSI C (C89).
const char* ext_name[] = {
	// = = = = stdio.h = = = =
	"printf", "sprintf", "fprintf", "scanf",
	"fscanf", "fopen", "fclose", "fread",
	"fwrite", "fgets", "fputs", "fseek",
	"ftell",

	// = = = = string.h = = = =
	"strcpy", "strcat", "strcmp", "strchr",
	"strstr", "strlen", "memcpy", "memcmp",
	"memchr", "memset",

	// = = = = stdlib.h = = = =
	"atoi", "itoa", "rand", "srand",
	"calloc", "malloc", "realloc", "free",
	"system",

	// = = = = bark = = = =
	"rom_init", "compile", "vm_init", "vm_reset",
	"exec", "vm_push", "vm_store", "rom_free", "vm_free"
};	// ext_name

// Определение общего количества внешних функций в ext_name.
const word_t ext_count = 41;

// Тип массива аргументов внешней функции.
typedef word_t arg_t[EXT_MAX_ARG_COUNT];

// Внутренняя функция чтения аргументов внешней функции из стека.
// Осуществляет проверку количества аргументов и их наличия на стеке.
// Принимает количество аргументов для чтения, указатель на массив для записи аргументов, указатель на ВМ.
// Возвращает признак успеха чтения аргументов (true - успешно, false - не хватает элементов на стеке).
static bool arg_read(word_t count, arg_t *arg, vm_t *vm) {
	bool read_flag = false;
	if (vm->sp >= count) {
		while (count--)
			(*arg)[count] = STACK_POP;
		read_flag = true;
	}	// if count
	return read_flag;
}	// arg_read

// Функция обработки внешних функций.
// Принимает ID функции и указатель на виртуальную машину.
// Возвращает статус выполнения.
word_t ext(word_t id, vm_t *vm) {
	// Статус выполнения внешней функции.
	ex_status_t status = est_work;

	// Массив аргументов.
	arg_t arg = {0};

	// Выбор по идентификатору команды
	switch (id) {
		// = = = = stdio.h = = = =
		case (0) : EXT_DEFINE_VA(1, EXT_DEFINE_FUNC((arg_count + 1), STACK_PUSH(vprintf((const char *)(arg[0]), (va_list)(arg + 1))))); break;
		case (1) : EXT_DEFINE_VA(2, EXT_DEFINE_FUNC((arg_count + 2), STACK_PUSH(vsprintf((char *)(arg[0]), (const char *)(arg[1]), (va_list)(arg + 2))))); break;
		case (2) : EXT_DEFINE_VA(1, EXT_DEFINE_FUNC((arg_count + 2), STACK_PUSH(vfprintf((FILE *)(arg[0]), (const char *)(arg[1]), (va_list)(arg + 2))))); break;
		case (3) : EXT_DEFINE_VA(1, EXT_DEFINE_FUNC((arg_count + 1), STACK_PUSH(vscanf((char *)(arg[0]), (va_list)(arg + 1))))); break;
		case (4) : EXT_DEFINE_VA(2, EXT_DEFINE_FUNC((arg_count + 2), STACK_PUSH(vfscanf((FILE *)(arg[0]), (const char *)(arg[1]), (va_list)(arg + 2))))); break;

		case (5) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(fopen((const char *)(arg[0]), (const char *)(arg[1]))))); break;
		case (6) : EXT_DEFINE_FUNC(1, STACK_PUSH(fclose((FILE *)(arg[0])))); break;
		case (7) : EXT_DEFINE_FUNC(4, STACK_PUSH(fread((void *)(arg[0]), arg[1], arg[2], (FILE *)(arg[3])))); break;
		case (8) : EXT_DEFINE_FUNC(4, STACK_PUSH((word_t)(fwrite((const void *)(arg[0]), arg[1], arg[2], (FILE *)(arg[3]))))); break;
		case (9) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(fgets((char *)(arg[0]), arg[1], (FILE *)(arg[2]))))); break;
		case (10) : EXT_DEFINE_FUNC(2, STACK_PUSH(fputs((const char *)(arg[0]), (FILE *)(arg[1])))); break;
		case (11) : EXT_DEFINE_FUNC(3, STACK_PUSH(fseek((FILE *)(arg[0]), (long)(arg[1]), arg[2]))); break;
		case (12) : EXT_DEFINE_FUNC(1, STACK_PUSH((word_t)(ftell((FILE *)(arg[0]))))); break;

		// = = = = string.h = = = =
		case (13) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(strcpy((char *)(arg[0]), (const char *)(arg[1]))))); break;
		case (14) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(strcat((char *)(arg[0]), (const char *)(arg[1]))))); break;
		case (15) : EXT_DEFINE_FUNC(2, STACK_PUSH(strcmp((const char *)(arg[0]), (const char *)(arg[1])))); break;
		case (16) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(strchr((const char *)(arg[0]), arg[1])))); break;
		case (17) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(strstr((const char *)(arg[0]), (const char *)(arg[1]))))); break;
		case (18) : EXT_DEFINE_FUNC(1, STACK_PUSH(strlen((const char *)(arg[0])))); break;
		case (19) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(memcpy((void *)(arg[0]), (const void *)(arg[1]), arg[2])))); break;
		case (20) : EXT_DEFINE_FUNC(3, STACK_PUSH(memcmp((const void *)(arg[0]), (const void *)(arg[1]), arg[2]))); break;
		case (21) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(memchr((const void *)(arg[0]), arg[1], arg[2])))); break;
		case (22) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(memset((void *)(arg[0]), arg[1], arg[2])))); break;

		// = = = = stdlib.h = = = =
		case (23) : EXT_DEFINE_FUNC(1, STACK_PUSH(atoi((void *)(arg[0])))); break;
		case (24) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(itoa(arg[0], (char *)(arg[1]), arg[2])))); break;
		case (25) : STACK_PUSH(rand()); break;
		case (26) : EXT_DEFINE_FUNC(1, srand(arg[0])); break;
		case (27) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(calloc(arg[0], arg[1])))); break;
		case (28) : EXT_DEFINE_FUNC(1, STACK_PUSH((word_t)(malloc(arg[0])))); break;
		case (29) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(realloc((void *)(arg[0]), arg[1])))); break;
		case (30) : EXT_DEFINE_FUNC(1, free((void *)(arg[0]))); break;
		case (31) : EXT_DEFINE_FUNC(1, STACK_PUSH(system((const char *)(arg[0])))); break;

		// = = = = bark = = = =
		case (32) : STACK_PUSH((word_t)(bark_rom_init())); break;
		case (33) : EXT_DEFINE_FUNC(2, STACK_PUSH(bark_compile((const char *)(arg[0]), (rom_t *)(arg[1])))); break;
		case (34) : EXT_DEFINE_FUNC(1, STACK_PUSH((word_t)(bark_vm_init((rom_t *)(arg[0]))))); break;
		case (35) : EXT_DEFINE_FUNC(1, bark_vm_reset((vm_t *)(arg[0]))); break;
		case (36) : EXT_DEFINE_FUNC(1, STACK_PUSH(bark_exec((vm_t *)(arg[0])))); break;
		case (37) : EXT_DEFINE_FUNC(2, bark_vm_push((vm_t *)(arg[0]), arg[1])); break;
		case (38) : EXT_DEFINE_FUNC(1, STACK_PUSH(bark_vm_store((vm_t *)(arg[0])))); break;
		case (39) : EXT_DEFINE_FUNC(1, bark_rom_free((rom_t *)(arg[0]))); break;
		case (40) : EXT_DEFINE_FUNC(1, bark_vm_free((vm_t *)(arg[0]))); break;

		// Внешняя функция не найдена.
		default: status = est_ext_not_found;
	}	// switch id
	return status;
}	// ext
