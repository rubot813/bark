#ifndef BARK_TYPES_H_INCLUDED
#define BARK_TYPES_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

/*
	The Bark programming language
	developed by Alexander Soganov, 2026
*/

#define LANG_VERSION		0x05	// Версия языка.
#define ROM_MAGIC			0x032D	// Идентификатор ROM.
#define CODE_SIZE			2048	// Количество слов в массиве code.
#define DATA_SIZE			512		// Количество слов в массиве data.
#define STACK_SIZE			16		// Глубина стека вычислений и стека вызовов.
#define OP_STACK_SIZE		16		// Глубина стека операторов.
#define MAX_TOKEN_COUNT		2048	// Максимальное количество токенов.
#define MAX_SYMBOL_COUNT	128		// Максимальное количество символов (переменные var и процедуры proc).
#define MAX_NAME_LEN		16		// Максимальная длина имени символа.

// Макросы взаимодействия со стеком VM.
#define STACK_PUSH(word) vm->stack[vm->sp++] = word		// Положить слово на стек.
#define STACK_POP vm->stack[--vm->sp]					// Взять слово из стека.
#define STACK_TOP vm->stack[vm->sp-1]					// Получить слово на вершине стека.

// Вспомогательный макрос сранвнения строки.
#define TOKEN_MATCH(str) strcmp(token, str) == 0

// Макрос формирования prebuilt-переменной.
#define MAKE_PREBUILT(name, value)				\
address = lookup_var(name, ctx);				\
if (address != -1) rom->data[address] = value;

// Макросы проверки стека на отсутствия элемента/переполнение.
#define EXEC_CHECK_SP_UFW(min_sp) if (vm->sp < min_sp) { status = est_stack_underflow; goto lb_exec_end; }
#define EXEC_CHECK_CSP_UFW(min_csp) if (vm->csp < min_csp) { status = est_cstack_underflow; goto lb_exec_end; }
#define EXEC_CHECK_SP_OFW if (vm->sp == STACK_SIZE) { status = est_stack_overflow; goto lb_exec_end; }
#define EXEC_CHECK_CSP_OFW if (vm->csp == STACK_SIZE) { status = est_cstack_overflow; goto lb_exec_end; }

// Типы знакового и беззнакового машинного слова.
typedef intptr_t word_t;
typedef uintptr_t uword_t;

// Перечисление статусов комплияции.
typedef enum {
	cst_success = 0,		// Успех.
	cst_out_of_memory,		// Закончилась память.
	cst_no_entry_point,		// Нет точки входа в программу (процедуры main).
	cst_token_overflow,		// Переполнение массива токенов.
	cst_op_stack_overflow,	// Переполнение стека операторов.
	cst_token_drop,			// Токен отброшен при обработке определения.
	cst_code_overflow,		// Переполнение секции code ROM.
	cst_data_overflow,		// Переполнение секции data ROM.
	cst_number_conversion,	// Ошибка преобразования строки в число.
	cst_token_mismatch,		// Несовпадение токена с ожидаемым.
	cst_var_overflow,		// Переполнение переменных.
	cst_proc_overflow,		// Переполнение процедур.
	cst_proc_not_found,		// Процедура не найдена.
	cst_ext_not_found		// Внешняя функция не найдена.
} cmp_status_t;

// Перечисление статусов работы.
typedef enum {
	est_work = 0,			// Машина в процессе выполнения.
	est_int,				// Программа прервана (interrupt).
	est_halt,				// Программа завершена (halt).
	est_inc_opcode,			// Ошибка: некорретный опкод.
	est_div_by_zero,		// Ошибка: деление на ноль.
	est_stack_underflow,	// Ошибка отсутствия элементов на стеке вычислений.
	est_stack_overflow,		// Ошибка переполнения стека вычислений.
	est_cstack_underflow,	// Ошибка отсутствия элементов на стеке вызовов.
	est_cstack_overflow,	// Ошибка переполнения стека вызовов.
	est_ext_stack_error,	// Ошибка стека при обработке внешней функции.
	est_ext_arg_overflow,	// Ошибка: превышено количество аргументов внешней функции.
	est_ext_not_found		// Ошибка: внешняя функция не найдена.
} ex_status_t;

// Перечисление опкодов.
typedef enum {
	// Общие инструкции.
	op_halt = 0,	// Завершение работы машины.
	op_int,			// Прерывание машины.
	op_ext,			// Вызов внешней функции. Вершина стека: ID функции.

	// Работа со стеком.
	op_push,		// Положить слово на стек. Слово берется из code (константа).
	op_drop,		// Сбросить слово с вершины стека.
	op_ld,			// Положить слово стек. Вершина стека: индекс ячейки data.
	op_bld,			// Положить байт на стека Вершина стека: индекс ячейки data.
	op_st,			// Извлечь из стека значение (слово) и индекс. Записать в data значение по индексу.
	op_bst,			// Извлечь из стека значение (байт) и индекс. Записать в data значение по индексу.

	// Адресация.
	op_addr,		// Получить адрес переменной.
	op_iaddr,		// Получить адрес переменной по индексу в переменной.
	op_ild,			// Чтение слова по адресу на вершине стека.
	op_ibld,		// Чтение байта по адресу на вершине стека.
	op_ist,			// Запись слова по адресу на вершине стека.
	op_ibst,		// Запись байта по адресу на вершине стека.

	// Ветвление.
	op_jump,		// Безусловный прыжок по адресу.
	op_jz,			// Прыжок по адресу, если на вершине стека 0.
	op_call,		// Вызов функции по адресу.
	op_ret,			// Возврат из функции по значению на вершине стека вызовов.

	// Арифметика.
	op_add,			// Сложение (+).
	op_sub,			// Вычитание (-).
	op_mul,			// Умножение (*).
	op_mod,			// Остаток от деления (%).
	op_div,			// Деление (/).

	// Логические операции.
	op_and,			// Логическое И (&&).
	op_or,			// Логическое ИЛИ (||).

	// Битовые операции.
	op_band,		// Битовое И (&).
	op_bor,			// Битовое ИЛИ (|).
	op_xor,			// Битовое исключающее ИЛИ (^).
	op_inv,			// Битовое НЕ (~).
	op_lsh,			// Побитовый сдвиг влево (<<).
	op_rsh,			// Побитовый сдвиг вправо (>>).

	// Сравнение.
	op_eq,			// Равно (==).
	op_neq,			// Не равно (!=).
	op_lt,			// Меньше (<).
	op_gt			// Больше (>).
} op_type_t;

// Структура символа компилятора.
typedef struct {
	char	name[MAX_NAME_LEN];	// Имя символа.
	uword_t	addr;				// Адрес/индекс символа.
} symbol_t;

// Структура оператора.
typedef struct {
	char	*op;		// Строка оператора.
	uword_t	priority;	// Приоритет оператора.
} operator_t;

// Структура контекста компилятора.
typedef struct {
	// Текущая позиция записи байт-кода.
	uword_t cp;

	// Стек операторов.
	char		*op_stack[OP_STACK_SIZE];
	uword_t		op_sp; // Указатель стека операторов.

	// Переменные var.
	symbol_t	var[MAX_SYMBOL_COUNT];
	uint8_t 	var_count;	// Общее количество переменных в var
	uword_t		var_idx;	// Индекс следующего свободного слова в data.

	// Переменные proc.
	symbol_t	proc[MAX_SYMBOL_COUNT];
	uint8_t		proc_count;

	// Переменные токенизатора.
	char 		*token[MAX_TOKEN_COUNT];
	uword_t		token_count, token_id;
} ctx_t;

// Структура образа виртуальной машины.
typedef struct {
	uword_t	magic;				// Иденетификатор ROM.
	uword_t entry_point;		// Точка входа в программу.
	word_t	code[CODE_SIZE];	// Машинный код.
	word_t	data[DATA_SIZE];	// Данные.
} rom_t;

// Структура виртуальной машины.
typedef struct {
	uword_t		pc;						// Программный счетчик.
	rom_t		*rom;					// Указатель на образ виртуальной машины.
	word_t 		stack[STACK_SIZE];		// Стек вычислений.
	uint8_t		sp;
	uword_t 	call_stack[STACK_SIZE];	// Стек вызовов.
	uint8_t		csp;
} vm_t;

#endif // BARK_TYPES_H_INCLUDED
