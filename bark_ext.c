#include "bark.h"

// C stdlib.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// va_list.
#include <stdarg.h>

// Макросы максимального количества аргументов внешних функций.
#define EXT_MAX_ARG_COUNT 8

// Макрос определения соглашения о вызовах System V ABI (аргументы идут через регистры).
#if (defined(__linux__) || defined(__ANDROID__) || defined(__APPLE__)) && (defined(__x86_64__) || defined(__aarch64__))
#define SYSTEM_V_ABI
#endif	// if

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

// Внутренняя встроенная функция инициализации VA.
static inline va_list ext_make_va(void *ptr, ...) {
	// Инициализация va.
    va_list args;
    va_start(args, ptr);

// Выбор по типу ABI.
#if defined(__x86_64__)
	// AMD64 ABI.
	((struct { unsigned int gp; unsigned int fp; void *mem; void *reg; }*)&(args))->mem = ptr;
#elif defined(__aarch64__)
	// AArch64 ABI.
	((struct { void *stack; void *gr_top; void *vr_top; int gr_offs; int vr_offs; }*)&(args))->stack = ptr;
#endif	// if cpu
    return args;
}	// ext_make_va

// Макрос EXT_VA_CAST для правильного разворачивания va_list для разных ABI.
#if (defined(SYSTEM_V_ABI))
#define EXT_VA_CAST(ptr) ext_make_va((void*)(ptr))
#else
// MinGW/GCC/Clang 32-bit: va_list это сырой указатель.
#define EXT_VA_CAST(ptr) (va_list)(ptr)
#endif // SYSTEM_V_ABI

// Редефайн vsnprintf для MSVCRT.
#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
#define vsnprintf _vsnprintf
#endif // if mingw32

// Массив имен внешних функций по ANSI C (C89).
const char* ext_name[] = {
	// = = = = stdio.h = = = =
	"printf", "sprintf", "snprintf", "fprintf",
	"scanf", "fscanf", "fopen", "fclose",
	"fread", "fwrite", "fgets", "fputs",
	"fseek", "ftell",

	// = = = = string.h = = = =
	"strcpy", "strncpy", "strcat", "strncat",
	"strcmp", "strncmp", "strchr", "strstr",
	"strlen", "strtok", "memcpy", "memcmp",
	"memchr", "memset",

	// = = = = stdlib.h = = = =
	"atoi", "rand", "srand", "calloc",
	"malloc", "realloc", "free", "system",

	// = = = = bark = = = =
	"rom_init", "compile", "vm_init", "vm_reset",
	"exec", "vm_push", "vm_store", "rom_free", "vm_free"
};	// ext_name

// Определение общего количества внешних функций в ext_name.
const word_t ext_count = 45;

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
		case (0) : EXT_DEFINE_VA(1, EXT_DEFINE_FUNC((arg_count + 1), STACK_PUSH(vprintf((const char *)(arg[0]), EXT_VA_CAST(arg + 1))))); break;
		case (1) : EXT_DEFINE_VA(2, EXT_DEFINE_FUNC((arg_count + 2), STACK_PUSH(vsprintf((char *)(arg[0]), (const char *)(arg[1]), EXT_VA_CAST(arg + 2))))); break;
		case (2) : EXT_DEFINE_VA(3, EXT_DEFINE_FUNC((arg_count + 3), STACK_PUSH(vsnprintf((char *)(arg[0]), (size_t)(arg[1]), (const char *)(arg[2]), EXT_VA_CAST(arg + 3))))); break;
		case (3) : EXT_DEFINE_VA(1, EXT_DEFINE_FUNC((arg_count + 2), STACK_PUSH(vfprintf((FILE *)(arg[0]), (const char *)(arg[1]), EXT_VA_CAST(arg + 2))))); break;
		case (4) : EXT_DEFINE_VA(1, EXT_DEFINE_FUNC((arg_count + 1), STACK_PUSH(vscanf((char *)(arg[0]), EXT_VA_CAST(arg + 1))))); break;
		case (5) : EXT_DEFINE_VA(2, EXT_DEFINE_FUNC((arg_count + 2), STACK_PUSH(vfscanf((FILE *)(arg[0]), (const char *)(arg[1]), EXT_VA_CAST(arg + 2))))); break;

		case (6) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(fopen((const char *)(arg[0]), (const char *)(arg[1]))))); break;
		case (7) : EXT_DEFINE_FUNC(1, STACK_PUSH(fclose((FILE *)(arg[0])))); break;
		case (8) : EXT_DEFINE_FUNC(4, STACK_PUSH(fread((void *)(arg[0]), arg[1], arg[2], (FILE *)(arg[3])))); break;
		case (9) : EXT_DEFINE_FUNC(4, STACK_PUSH((word_t)(fwrite((const void *)(arg[0]), arg[1], arg[2], (FILE *)(arg[3]))))); break;
		case (10) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(fgets((char *)(arg[0]), arg[1], (FILE *)(arg[2]))))); break;
		case (11) : EXT_DEFINE_FUNC(2, STACK_PUSH(fputs((const char *)(arg[0]), (FILE *)(arg[1])))); break;
		case (12) : EXT_DEFINE_FUNC(3, STACK_PUSH(fseek((FILE *)(arg[0]), (long)(arg[1]), arg[2]))); break;
		case (13) : EXT_DEFINE_FUNC(1, STACK_PUSH((word_t)(ftell((FILE *)(arg[0]))))); break;

		// = = = = string.h = = = =
		case (14) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(strcpy((char *)(arg[0]), (const char *)(arg[1]))))); break;
		case (15) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(strncpy((char *)(arg[0]), (const char *)(arg[1]), arg[2])))); break;
		case (16) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(strcat((char *)(arg[0]), (const char *)(arg[1]))))); break;
		case (17) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(strncat((char *)(arg[0]), (const char *)(arg[1]), arg[2])))); break;
		case (18) : EXT_DEFINE_FUNC(2, STACK_PUSH(strcmp((const char *)(arg[0]), (const char *)(arg[1])))); break;
		case (19) : EXT_DEFINE_FUNC(3, STACK_PUSH(strncmp((const char *)(arg[0]), (const char *)(arg[1]), arg[2]))); break;
		case (20) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(strchr((const char *)(arg[0]), arg[1])))); break;
		case (21) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(strstr((const char *)(arg[0]), (const char *)(arg[1]))))); break;
		case (22) : EXT_DEFINE_FUNC(1, STACK_PUSH((word_t)(strlen((const char *)(arg[0]))))); break;
		case (23) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(strtok((char *)(arg[0]), (const char *)(arg[1]))))); break;
		case (24) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(memcpy((void *)(arg[0]), (const void *)(arg[1]), arg[2])))); break;
		case (25) : EXT_DEFINE_FUNC(3, STACK_PUSH(memcmp((const void *)(arg[0]), (const void *)(arg[1]), arg[2]))); break;
		case (26) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(memchr((const void *)(arg[0]), arg[1], arg[2])))); break;
		case (27) : EXT_DEFINE_FUNC(3, STACK_PUSH((word_t)(memset((void *)(arg[0]), arg[1], arg[2])))); break;

		// = = = = stdlib.h = = = =
		case (28) : EXT_DEFINE_FUNC(1, STACK_PUSH(atoi((void *)(arg[0])))); break;
		case (29) : STACK_PUSH(rand()); break;
		case (30) : EXT_DEFINE_FUNC(1, srand(arg[0])); break;
		case (31) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(calloc(arg[0], arg[1])))); break;
		case (32) : EXT_DEFINE_FUNC(1, STACK_PUSH((word_t)(malloc(arg[0])))); break;
		case (33) : EXT_DEFINE_FUNC(2, STACK_PUSH((word_t)(realloc((void *)(arg[0]), arg[1])))); break;
		case (34) : EXT_DEFINE_FUNC(1, free((void *)(arg[0]))); break;
		case (35) : EXT_DEFINE_FUNC(1, STACK_PUSH(system((const char *)(arg[0])))); break;

		// = = = = bark = = = =
		case (36) : STACK_PUSH((word_t)(bark_rom_init())); break;
		case (37) : EXT_DEFINE_FUNC(2, STACK_PUSH(bark_compile((const char *)(arg[0]), (rom_t *)(arg[1])))); break;
		case (38) : EXT_DEFINE_FUNC(1, STACK_PUSH((word_t)(bark_vm_init((rom_t *)(arg[0]))))); break;
		case (39) : EXT_DEFINE_FUNC(1, bark_vm_reset((vm_t *)(arg[0]))); break;
		case (40) : EXT_DEFINE_FUNC(2, STACK_PUSH(bark_exec((vm_t *)(arg[0]), arg[1]))); break;
		case (41) : EXT_DEFINE_FUNC(2, bark_vm_push((vm_t *)(arg[0]), arg[1])); break;
		case (42) : EXT_DEFINE_FUNC(1, STACK_PUSH(bark_vm_store((vm_t *)(arg[0])))); break;
		case (43) : EXT_DEFINE_FUNC(1, bark_rom_free((rom_t *)(arg[0]))); break;
		case (44) : EXT_DEFINE_FUNC(1, bark_vm_free((vm_t *)(arg[0]))); break;

		// Внешняя функция не найдена.
		default: status = est_ext_not_found;
	}	// switch id
	return status;
}	// ext
