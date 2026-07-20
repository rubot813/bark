#include <stdio.h>	// stdin, stdout, stderr.
#include <stdlib.h>
#include <string.h>
#include "bark.h"

// Разрешение символов внешних функций (ссылки на bark_ext.o).
extern const char *ext_name[];		// Массив названий функций.
extern const word_t ext_count;		// Количество функций.
extern word_t ext(word_t, vm_t *);	// Функция обработки внешних функций.

// = = = = Общие функции = = = =

// Внутренняя функция получения токена.
static inline char* token_peek(ctx_t *ctx) {
	return (ctx->token_id < ctx->token_count ? ctx->token[ctx->token_id] : "");
}	// token_peek

// Внутренняя функция получения токена с переходом к следующему.
static inline char* token_next(ctx_t *ctx) {
	return (ctx->token_id < ctx->token_count ? ctx->token[ctx->token_id++] : "");
}	// token_next

// Внутренняя функция проверки токена на соответствие заданному.
// Возвращает признак соответствия: true - токен совпадает, false - токен не совпадает.
static bool token_match(const char *mean_token, ctx_t *ctx) {
	char *token = token_next(ctx);
	return (strcmp(token, mean_token) == 0);
}	// token_match

// Внутренняя функция разрешения символа для переменной.
// Возвращает индекс символа если он задан, либо занимает под него адрес.
static word_t lookup_var(const char *name, ctx_t *ctx) {
	word_t index = -1;

	// Поиск переменной по названию.
	for (word_t id = 0; id < ctx->var_count; id++)
		if (strcmp(ctx->var[id].name, name) == 0) {
			index = ctx->var[id].addr;
			break;
		}	// if strcmp

	// Если переменная не найдена и под нее есть память.
	if ((index == -1) && (ctx->var_count < MAX_SYMBOL_COUNT)) {
		// Сохранение индекса новой переменной.
		index = ctx->var_idx;

		// Создание новой переменной.
		strncpy(ctx->var[ctx->var_count].name, name, (MAX_NAME_LEN - 1));
		ctx->var[ctx->var_count].name[MAX_NAME_LEN-1] = '\0';
		ctx->var[ctx->var_count].addr = ctx->var_idx++;	// Резервирование ячейки под переменную.
		ctx->var_count++;
	}	// if index
	return index;
}	// lookup_var

// Внутренняя функция создания символа для процедуры.
// Возвращает признак успеха создания процедуры: true - успешно, false - переполнение.
static bool make_proc(const char *name, ctx_t *ctx) {
	// Результат создания процедуры.
	bool result = false;

	// Проверка на переполнение процедур.
	if (ctx->proc_count < MAX_SYMBOL_COUNT) {
		strncpy(ctx->proc[ctx->proc_count].name, name, (MAX_NAME_LEN - 1));
		ctx->proc[ctx->proc_count].name[MAX_NAME_LEN-1] = '\0';
		ctx->proc[ctx->proc_count].addr = ctx->cp;
		ctx->proc_count++;
		result = true;
	}	// if proc_count
	return result;
}	//make_proc

// Внутренняя функция разрешения символа для процедуры.
// Возвращает индекс символа если он задан, либо занимает под него адрес.
static word_t search_proc(const char *name, ctx_t *ctx) {
	word_t index = -1;
	// Поиск процедуры по названию.
	for (uword_t id = 0; id < ctx->proc_count; id++ )
		if (strcmp(ctx->proc[id].name, name) == 0) {
			index = ctx->proc[id].addr;
			break;
		}	// if strcmp
	return index;
}	// search_proc

// Внутренняя функция получения приоритета оператора.
// Принимает токен, возвращает приоритет. 0 - токен не является оператором.
static word_t get_op_priority(const char *token) {
	// Итоговый приоритет оператора.
	word_t priority = 0;

	// Ленивая инициализация массива операторов.
	static operator_t op_array[] = {
		{"~", 14},
		{"*", 13},	{"/", 13}, {"%", 13},
		{"+", 12},	{"-", 12},
		{">>", 11},	{"<<", 11},
		{"<", 10},	{">", 10}, {"<=", 10}, {">=", 10},
		{"==", 9},	{"!=", 9},
		{"&", 8},
		{"^", 7},
		{"|", 6},
		{"&&", 5},
		{"||", 4},
		{NULL, 0}
	};	// op_array

	// Поиск приоритета по массиву операторов.
	for (uword_t id = 0; op_array[id].op; id++)
        if (strcmp(op_array[id].op, token) == 0) {
			priority = op_array[id].priority;
			break;
        }	// if
	return priority;
}	// get_op_priotiry

// Внутренняя функция получения признака, что токен - команда.
static inline bool token_is_cmd(const char *token) {
	return (TOKEN_MATCH("read") || TOKEN_MATCH("bread"));
}	// token_is_cmd

// Внутренняя функция получения признака, что токен - число.
static bool token_is_number(const char *token) {
	// Если токен начинается с цифры либо знака "минус" и после него - число
	return (((token[0] >= '0') && (token[0] <= '9')) ||
			((token[0] == '-') && (token[1] >= '0') && (token[1] <= '9')));
}	// token_is_number

// Внутренняя функция поиска индекса внешней функции по ее имегни.
// Принимает строку с именем  внешней функции, возвращает ее id.
static word_t search_ext(const char *name) {
	word_t ext_id = -1;
	for (word_t id = 0; id < ext_count; id++)
		if (strcmp(ext_name[id], name) == 0) {
			ext_id = id;
			break;
		}	// if
	return ext_id;
}	// search_ext

// Внутренняя функция преобразования строки в число (через strtol).
// Принммавет указатель на строку и указатель для записи числа.
// Возвращает результата преобразования: true - успешно, false - ошибка.
static bool str_to_number(char *str, word_t *word) {
	// Преобразовать строку в число.
	char *end_ptr = NULL;
	*word = (word_t)(strtol(str, &end_ptr, 0));	// Обрезать до word_t.
	return (str != end_ptr);
}	// str_to_number

// Внутренняя функция записи слова в секцию code ROM.
static inline void emit(word_t word, ctx_t *ctx, rom_t *rom) {
	rom->code[ctx->cp++] = word;
}	// emit

// Внутренняя функция записи числа в секцию code ROM.
// Возвращает успех преобразования строки в число: true - успешно, false - ошибка.
static bool emit_number(char *str, ctx_t *ctx, rom_t *rom) {
	bool result = false;
	word_t value;
	// Преобразовать строку в число.
	if (str_to_number(str, &value)) {
		// Поместить число на вершину стека.
		emit(op_push, ctx, rom);
		emit(value, ctx, rom);
		result = true;
	} // if result
	return result;
}	// emit_number

// Внутренняя функция записи строки в секцию code ROM.
static void emit_str(char *str, ctx_t *ctx, rom_t *rom) {
	char *src = (str + 1);	// Пропуск открывающих двойных кавычек.
	char *dst = (char *)(&rom->data[ctx->var_idx]);
	char *start_dst = dst;

	// Пока не встречен нуль-терминатор или закрывающая двойная кавычка
	while ((*src != '\0') && !((*src == '"') && *(src + 1) == '\0')) {
		// Если строка - спецсимвол
		if ((*src == '\\') && (*(src + 1) != '\0')) {
			// Выбор по типу символа.
			switch (*(src + 1)) {
				case ('n'): *dst++ = '\n'; break;
				case ('r'): *dst++ = '\r'; break;
				case ('t'): *dst++ = '\t'; break;
				case ('/'): *dst++ = '/'; break;
				case ('"'): *dst++ = '"'; break;
				default: *dst++ = *src++; src--; break;
			}	// switch
			src += 2;
		} else
			*dst++ = *src++;
	}	// while

	// Добавление нуль-терминатора и обновление количества зарезервированных слов в data.
	*dst++ = '\0';
	ctx->var_idx += ((dst - start_dst) + (sizeof(word_t) - 1)) / sizeof(word_t);
}	// emit_str

// Внутренняя функция записи массива в секцию code ROM.
// Возвращает успех преобразования строки в число: true - успешно, false - ошибка.
static bool emit_array(ctx_t *ctx, rom_t *rom) {
	bool result = true;
	word_t word;
	char *token = token_next(ctx);
	while (*token != ']') {
		if (str_to_number(token, &word))
			rom->data[ctx->var_idx++] = word;
		else {
			result = false;
			break;
		}	// if str_to_number
		token = token_next(ctx);
	}	// while token
	return result;
}	// emit_array

// Внутренняя функция патчинга ячейки code ROM. Принимает адрес для патчинга.
static inline void patch(word_t address, ctx_t *ctx, rom_t *rom) {
	rom->code[address] = ctx->cp;
}	// patch

// = = = = Функции токенизатора = = = =

// Внутренняя главная функция токенизации.
// Принимает указатель на строку для разбора и указатель на контекст.
// Возвращает статус компиляции.
static cmp_status_t token(char *data, ctx_t *ctx) {
	// Статус компиляции.
	cmp_status_t status = cst_success;

	// Токенизация.
	char *src = data;
	while (*src && ctx->token_count < MAX_TOKEN_COUNT) {
		// Пропуск пробелов, символов табуляции и переноса строк.
		while ((*src == ' ') || (*src == '\t') || (*src == '\n') || (*src == '\r'))
			src++;

		// Обработка конца файла.
		if (*src == '\0')
			break;

		// Обработка комментариев.
		if ((*src == '/') && (*(src + 1) == '/')) {
			while (*src && (*src != '\n'))
				src++;
			continue;
		}	// if

		// Обработка токена.
		char *token_start = src;

		// Обработка строк.
		if (*src == '"') {
			// Переход к следующему символу.
			src++;
			while (*src && (*src != '"'))
				src++;
			if (*src == '"')
				src++;
		} else
			// Обычный токен.
			while (*src && (*src != ' ') && (*src != '\t') && (*src != '\n') && (*src != '\r'))
				src++;

		// Проверка на переполнение массива токенов
		if (ctx->token_count < MAX_TOKEN_COUNT) {
			// Определение длины токена и выделение памяти.
			word_t token_len = (src - token_start);
			ctx->token[ctx->token_count] = malloc(token_len + 1);

			// Проверка на нехватку памяти.
			if (ctx->token[ctx->token_count]) {
				memcpy(ctx->token[ctx->token_count], token_start, token_len);
				ctx->token[ctx->token_count][token_len] = '\0';
				ctx->token_count++;
			} else {
				status = cst_out_of_memory;
				break;
			}	// if mem
		} else {
			status = cst_token_overflow;
			break;
		}	// if token_count
	}	// while src
	return status;
}	// token

// Внутренняя функция преобразования выражений из инфиксной в RPN.
// Осуществляет преобразование выражений в круглых скобках из инфиксной нотации в RPN.
// Возвращает статус компиляции.
static cmp_status_t token_rpn(ctx_t *ctx) {
	// Статус компиляции.
	cmp_status_t status = cst_success;

	// Уровень вложенности скобок.
	word_t level = 0;

	// Идентификаторы чтения и записи в token.
	word_t write_id = 0, read_id = 0;

	// Цикл по всем токенам.
	while (ctx->token[read_id]) {
		char *token = ctx->token[read_id];
		if (level == 0) {
			// Обработка вне круглых скобок.
			if (TOKEN_MATCH("(")) {
				ctx->token[write_id++] = token;	// Сохранить "("
				level = 1;	// Инициализация.
				ctx->op_sp = 0;
			} else
				// Сохранение токена на его месте.
				ctx->token[write_id++] = token;
		} else {
			// Обработка внутри круглых скобок.
			if (token_is_cmd(token))
				// Обработка команды.
				ctx->op_stack[ctx->op_sp++] = token;
			else if (TOKEN_MATCH("(")) {
				// Обработка открывающей скобки.
				level++;
				ctx->op_stack[ctx->op_sp++] = token;
			} else if (TOKEN_MATCH(")")) {
				// Обработка закрывающей скобки.
				level--;

				while ((ctx->op_sp > 0) && strcmp(ctx->op_stack[ctx->op_sp - 1], "(") != 0)
					ctx->token[write_id++] = ctx->op_stack[--ctx->op_sp];

				// Удаление внутренней "("
				if (ctx->op_sp)
					ctx->op_sp--;

				if (ctx->op_sp && token_is_cmd(ctx->op_stack[ctx->op_sp - 1]))
					ctx->token[write_id++] = ctx->op_stack[--ctx->op_sp];

				// Конец внешней группы скобок
				if (level == 0) {
					while (ctx->op_sp)
						ctx->token[write_id++] = ctx->op_stack[--ctx->op_sp];
					ctx->token[write_id++] = token;	// Сохранить внешнюю закрывающую скобку.
				}	// if level
			} else {
				// Обработка операторов и операндов.
				word_t op1_prio = get_op_priority(token);
				if (op1_prio) {
					// Оператор
					while (ctx->op_sp) {
						word_t op2_prio = get_op_priority(ctx->op_stack[ctx->op_sp - 1]);
						if (op2_prio && (op2_prio >= op1_prio))
							ctx->token[write_id++] = ctx->op_stack[--ctx->op_sp];
						else
							break;
					}	// while sp
					ctx->op_stack[ctx->op_sp++] = token;
				} else
					// Операнд.
					ctx->token[write_id++] = token;
			}	// if token

			// Проверка на переполнение стека операторов.
			if (ctx->op_sp == OP_STACK_SIZE) {
				status = cst_op_stack_overflow;
				break;
			}	// if op_sp
		}	// if level
		read_id++;
	}	// while

	// Сохранение общего количества токенов.
	ctx->token_count = write_id;
	return status;
}	// token_rpn

// = = = = Функции компилятора = = = =

// Внутренняя функция парсера выражения в круглых скобках.
// Принимает указатель на контекст и ROM.
// Возвращает статус компиляции.
static cmp_status_t expr_paren(ctx_t *ctx, rom_t *rom) {
	// Статус компиляции.
	cmp_status_t status = cst_success;

	// Валидация токена (открывающая выражение круглая скобка).
	if (!token_match("(", ctx)) {
		status = cst_token_mismatch;
		goto lb_expr_paren_end;
	}	// if !token_match

	// Пока не встречена закрывающая выражение скобка
	while (strcmp(token_peek(ctx), ")") != 0) {
		// Чтение следующего токена.
		char *token = token_next(ctx);

		// Выбор по типу токена.
		// Арифметика.
		if (TOKEN_MATCH("+")) emit(op_add, ctx, rom);
		else if (TOKEN_MATCH("-")) emit(op_sub, ctx, rom);
		else if (TOKEN_MATCH("*")) emit(op_mul, ctx, rom);
		else if (TOKEN_MATCH("/")) emit(op_div, ctx, rom);
		else if (TOKEN_MATCH("%")) emit(op_mod, ctx, rom);

		// Логические операции.
		else if (TOKEN_MATCH("&&")) emit(op_and, ctx, rom);
		else if (TOKEN_MATCH("||")) emit(op_or, ctx, rom);

		// Битовые операции.
		else if (TOKEN_MATCH("&")) emit(op_band, ctx, rom);
		else if (TOKEN_MATCH("|")) emit(op_bor, ctx, rom);
		else if (TOKEN_MATCH("^")) emit(op_xor, ctx, rom);
		else if (TOKEN_MATCH("~")) emit(op_inv, ctx, rom);
		else if (TOKEN_MATCH("<<")) emit(op_lsh, ctx, rom);
		else if (TOKEN_MATCH(">>")) emit(op_rsh, ctx, rom);

		// Сравнение.
		else if (TOKEN_MATCH("==")) emit(op_eq, ctx, rom);
		else if (TOKEN_MATCH("!=")) emit(op_neq, ctx, rom);
		else if (TOKEN_MATCH("<")) emit(op_lt, ctx, rom);
		else if (TOKEN_MATCH(">")) emit(op_gt, ctx, rom);

		else if (TOKEN_MATCH("read")) emit(op_ild, ctx, rom);
		else if (TOKEN_MATCH("bread")) emit(op_ibld, ctx, rom);
		else if (TOKEN_MATCH("addrof")) {
			emit(op_addr, ctx, rom);
			word_t address = lookup_var(token_next(ctx), ctx);
			if (address != -1)
				emit(address, ctx, rom);
			else {
				status = cst_var_overflow;
				goto lb_expr_paren_end;
			}	// if address
		} else if (TOKEN_MATCH("iaddrof")) {
			emit(op_iaddr, ctx, rom);
			word_t address = lookup_var(token_next(ctx), ctx);
			if (address != -1)
				emit(address, ctx, rom);
			else {
				status = cst_var_overflow;
				goto lb_expr_paren_end;
			}	// if address
		} else if (token[0] == '"') {
			// Положить на вершину стека строку.
			emit(op_push, ctx, rom);
			emit(ctx->var_idx, ctx, rom);
			emit_str(token, ctx, rom);
		} else if (token[0] == '[') {
			emit(op_push, ctx, rom);
			emit(ctx->var_idx, ctx, rom);
			// Положить массив в data.
			if (emit_array(ctx, rom) == false) {
				status = cst_number_conversion;
				goto lb_expr_paren_end;
			}	// if emit_array
			// Если токен - число
		} else if (token_is_number(token)) {
			if (emit_number(token, ctx, rom) == false) {
				status = cst_number_conversion;
				goto lb_expr_paren_end;
			}	// if emit_number
		} else {
			// Токен - имя переменной.
			emit(op_push, ctx, rom);
			word_t address = lookup_var(token, ctx);
			if (address != -1) {
				emit(address, ctx, rom);
				emit(op_ld, ctx, rom);
			} else {
				status = cst_var_overflow;
				goto lb_expr_paren_end;
			}	// if address
		}	// if
	}	// while

	// Валидация токена (закрывающая выражение круглая скобка).
	if (!token_match(")", ctx))
		status = cst_token_mismatch;

lb_expr_paren_end:
	// Обработка переполнения секций code и data ROM.
	if (ctx->cp >= CODE_SIZE) status = cst_code_overflow;
	if (ctx->var_idx >= DATA_SIZE) status = cst_data_overflow;
	return status;
}	// expr_paren

// Внутренняя функция парсера одиночного выражения.
// Возвращает статус компиляции.
static cmp_status_t expr_single(ctx_t *ctx, rom_t *rom) {
	// Статус компиляции.
	cmp_status_t status = cst_success;

	// Чтение следующего токена.
	char *token = token_next(ctx);

	// Выбор по типу токена.
	if (token[0] == '"') {
		// Положить на стек индекс строки в data.
		emit(op_push, ctx, rom);
		emit(ctx->var_idx, ctx, rom);
		emit_str(token, ctx, rom);		// Поместить строку в data.
	} else if (token[0] == '[') {
		emit(op_push, ctx, rom);
		emit(ctx->var_idx, ctx, rom);
		// Положить массив в data.
		if (emit_array(ctx, rom) == false)
			status = cst_number_conversion;
		// Если токен - число
	} else if (token_is_number(token)) {
		if (emit_number(token, ctx, rom) == false)
			status = cst_number_conversion;
	} else {
		// Токен - имя переменной.
		emit(op_push, ctx, rom);
		word_t address = lookup_var(token, ctx);
		if (address != -1) {
			emit(address, ctx, rom);
			emit(op_ld, ctx, rom);
		} else
			status = cst_var_overflow;
	}	// if

	// Обработка переполнения секций code и data ROM.
	if (ctx->cp >= CODE_SIZE) status = cst_code_overflow;
	if (ctx->var_idx >= DATA_SIZE) status = cst_data_overflow;
	return status;
}	// expr_single

// Внутренняя функция комбинированного парсера выражений.
// Возвращает статус компиляции.
static cmp_status_t expression(ctx_t *ctx, rom_t *rom) {
	// Статус компиляции.
	cmp_status_t status = cst_success;

	// Если токен - открывающая круглая скобка
	if (strcmp(token_peek(ctx), "(") == 0)
		// Обработка выражения в круглых скобках (RPN).
		status = expr_paren(ctx, rom);
	else
		// Обработка одиночного выражения.
		status = expr_single(ctx, rom);
	return status;
}	// expression

// Внутренняя рекурсивная функция обработки определений (вне круглых скобок).
// Возвращает статус компиляции.
static cmp_status_t statement(ctx_t *ctx, rom_t *rom) {
	// Статус компиляции.
	cmp_status_t status = cst_success;

	// Получение следующего токена.
	char *token = token_next(ctx);

	// Если EOF - выход.
	if ((*token == '\0') || (TOKEN_MATCH("")))
		goto lb_statement_end;

	// Обработка команды push (положить значение на вершину стека).
	if (TOKEN_MATCH("push")) {
		status = expression(ctx, rom);
		goto lb_statement_end;
	}	// push

	// Обработка команды drop (сбросить слово с вершины стека).
	if (TOKEN_MATCH("drop")) {
		emit(op_drop, ctx, rom);
		goto lb_statement_end;
	}	// drop

	// Обработка команды store (прочитать значение с вершины стека в переменную).
	if (TOKEN_MATCH("store")) {
		// Далее ожидается имя переменной.
		word_t address = lookup_var(token_next(ctx), ctx);
		if (address != -1) {
			// Формирование store.
			emit(op_push, ctx, rom);
			emit(address, ctx, rom);
			emit(op_st, ctx, rom);
		} else
			status = cst_var_overflow;
		goto lb_statement_end;
	}	// store

	// Обработка if.
	if (TOKEN_MATCH("if")) {
		// Обработка выражения внутри круглых скобок.
		status = expression(ctx, rom);

		// Валидация токена (открывающая определение фигурная скобка).
		if (token_match("{", ctx)) {
			// Генерация опкода прыжка и сохранение адреса патчинга.
			emit(op_jz, ctx, rom);
			word_t patch_if_addr = ctx->cp++;

			// Обработка определений внутри фигурных скобок.
			while (strcmp(token_peek(ctx), "}") != 0)
				status = statement(ctx, rom);

			// Валидация токена (закрывающая определение фигурная скобка).
			if (!token_match("}", ctx))
				status = cst_token_mismatch;

			// Генерация безусловного прыжка в обход возможного else.
			emit(op_jump, ctx, rom);
			word_t patch_else_addr = ctx->cp++;

			// Патчинг if.
			patch(patch_if_addr, ctx, rom);

			// Проверка наличия опционального else.
			if (strcmp(token_peek(ctx), "else") == 0) {
				// Сброс текущего токена.
				token_next(ctx);

				// Валидация токена (открывающая определение фигурная скобка).
				if (token_match("{", ctx)) {
					// Обработка определений внутри фигурных скобок.
					while (strcmp(token_peek(ctx), "}") != 0)
						status = statement(ctx, rom);

					// Валидация токена (закрывающая определение фигурная скобка).
					if (!token_match("}", ctx))
						status = cst_token_mismatch;
				} else
					status = cst_token_mismatch;
			}	// else

			// Патчинг else.
			patch(patch_else_addr, ctx, rom);
		} else
			status = cst_token_mismatch;
		goto lb_statement_end;
	}	// if

	// Обработка while.
	if (TOKEN_MATCH("while")) {
		// Обработка выражения внутри круглых скобок.
		word_t start_addr = ctx->cp;
		expression(ctx, rom);

		// Валидация токена (открывающая выражение фигурная скобка).
		if (token_match("{", ctx)) {
			// Генерация опкода прыжка и сохранение адреса патчинга.
			emit(op_jz, ctx, rom);
			word_t patch_addr = ctx->cp++;

			// Обработка определений внутри фигурных скобок.
			while (strcmp(token_peek(ctx), "}") != 0)
				status = statement(ctx, rom);

			// Валидация токена (закрывающая выражение фигурная скобка).
			if (!token_match("}", ctx))
				status = cst_token_mismatch;

			// Генерация jump и патчинг.
			emit(op_jump, ctx, rom);
			emit(start_addr, ctx, rom);
			patch(patch_addr, ctx, rom);
		} else
			status = cst_token_mismatch;
		goto lb_statement_end;
	}	// if while

	// Обработка объявления процедуры.
	if (TOKEN_MATCH("proc")) {
		// Формирование процедуры.
		if (make_proc(token_next(ctx), ctx)) {
			// Валидация токена (открывающая выражение фигурная скобка).
			if (token_match("{", ctx)) {
				// Обработка тела процедуры.
				while (strcmp(token_peek(ctx), "}") != 0)
					status = statement(ctx, rom);

				// Валидация токена (закрывающая выражение фигурная скобка).
				if (!token_match("}", ctx))
					status = cst_token_mismatch;
			} else
				status = cst_token_mismatch;

			// Генерация опкода возврата.
			emit(op_ret, ctx, rom);
		} else
			status = cst_proc_overflow;
		goto lb_statement_end;
	}	// if proc

	// Обработка вызова процедуры.
	if (TOKEN_MATCH("call")) {
		// Получение имени и адреса процедуры.
		char *name = token_next(ctx);
		word_t address = search_proc(name, ctx);
		if (address != -1) {
			// Если адрес процедуры успешно определен.
			emit(op_call, ctx, rom);
			emit(address, ctx, rom);
		} else
			status = cst_proc_not_found;
		goto lb_statement_end;
	}	// if call

	// Обработка вызова внешней функции.
	if (TOKEN_MATCH("ext")) {
		// Получение имени и поиск ID внешней функции.
		char *name = token_next(ctx);
		word_t ext_id = search_ext(name);
		if (ext_id != -1) {
			// Обработка аргументов внешней функции.
			status = expression(ctx, rom);
			emit(op_ext, ctx, rom);
			emit(ext_id, ctx, rom);
		} else
			status = cst_ext_not_found;
		goto lb_statement_end;
	}	// if ext

	// Обработка команды записи слова.
	if (TOKEN_MATCH("write")) {
		// Обработка выражения до '='.
		status = expression(ctx, rom);

		// Валидация токена.
		if ((status == cst_success) && token_match("=", ctx)) {
			// Обработка выражения после '='.
			status = expression(ctx, rom);

			// Генерация опкода indirect store.
			emit(op_ist, ctx, rom);
		} else
			status = cst_token_mismatch;
		goto lb_statement_end;
	}	// if write

	// Обработка команды записи байта.
	if (TOKEN_MATCH("bwrite")) {
		// Обработка выражения до '='.
		status = expression(ctx, rom);

		// Валидация токена.
		if ((status == cst_success) && token_match("=", ctx)) {
			// Обработка выражения после '='.
			status = expression(ctx, rom);

			// Генерация опкода indirect byte store.
			emit(op_ibst, ctx, rom);
		} else
			status = cst_token_mismatch;
		goto lb_statement_end;
	}	// if bwrite

	// Обработка команды прерывания работы машины.
	if (TOKEN_MATCH("int")) {
		emit(op_int, ctx, rom);
		goto lb_statement_end;
	}	// if int

	// Обработка команды завершения работы машины.
	if (TOKEN_MATCH("halt")) {
		emit(op_halt, ctx, rom);
		goto lb_statement_end;
	}	// if halt

	// Обработка присвоения.
	if (strcmp(ctx->token[ctx->token_id], "=") == 0) {
		// Получение адреса переменной.
		word_t address = lookup_var(token, ctx);
		if (address != -1) {
			// Валидация токена.
			if (token_match("=", ctx)) {
				// Обработка выражения после '='.
				status = expression(ctx, rom);
				emit(op_push, ctx, rom);
				emit(address, ctx, rom);
				emit(op_st, ctx, rom);
			} else
				status = cst_token_mismatch;
		} else
			status = cst_var_overflow;
		goto lb_statement_end;
	}	// if token_id

	// Токен отброшен при обработке определения.
	status = cst_token_drop;

lb_statement_end:
	// Обработка переполнения секций code и data ROM.
	if (ctx->cp >= CODE_SIZE) status = cst_code_overflow;
	if (ctx->var_idx >= DATA_SIZE) status = cst_data_overflow;
	return status;
}	// statement

// Внутренняя функция формирования prebuilt-переменных.
// prebuilt-переменные содержат префикс '_'.
// Возвращает адрес последней созданной переменной (-1 - ошибка).
static word_t make_prebuilt(ctx_t *ctx, rom_t *rom) {
	word_t address;	// Адрес переменной для макроса MAKE_PREBUILT.
	MAKE_PREBUILT("_lang_ver",  LANG_VERSION)
	MAKE_PREBUILT("_word_size", sizeof(word_t))
	MAKE_PREBUILT("_rom_size",  sizeof(rom_t))
	MAKE_PREBUILT("_stdin",  	(word_t)(stdin))
	MAKE_PREBUILT("_stdout", 	(word_t)(stdout))
	MAKE_PREBUILT("_stderr",  	(word_t)(stderr))
	return address;
}	// make_prebuilt

// = = = = Функции виртуальной машины = = = =
// Внутренняя функция выполнения математических, логических и битовых операций виртуальной машины.
// Принимает указатель на виртуальную машину.
// Возвращает статус выполнения программы.
static ex_status_t exec_op(vm_t *vm) {
	// Статус выполнения программы.
	ex_status_t status = est_work;

	// Результат операции.
	word_t result = 0;

	// Получение кода операции.
	op_type_t op = vm->rom->code[vm->pc - 1];

	// Проверка наличия элементов на стеке.
	// Для op_inv - один элемент (унарная операция), для других два (бинарные операции).
	if (((op == op_inv) && (vm->sp < 1)) || ((op != op_inv) && (vm->sp < 2))) {
		status = est_stack_underflow;
		goto lb_exec_op_end;
	}	// if sp

	// Получение элементов стека.
	word_t tos = STACK_POP;		// Top of stack.
	word_t nos = STACK_TOP;		// Next of stack.

	// Выбор по типу операции.
	switch (op) {
		// Арифметика.
		case (op_add):	result = (nos + tos); break;
		case (op_sub):	result = (nos - tos); break;
		case (op_mul):	result = (nos * tos); break;
		case (op_mod):	{ if (tos) result = (nos % tos); else { status = est_div_by_zero; goto lb_exec_op_end; } break; }
		case (op_div):	{ if (tos) result = (nos / tos); else { status = est_div_by_zero; goto lb_exec_op_end; } break; }

		// Логические операции.
		case (op_and):	result = (nos && tos); break;
		case (op_or):	result = (nos || tos); break;

		// Битовые операции.
		case (op_band):	result = (nos & tos); break;
		case (op_bor):	result = (nos | tos); break;
		case (op_xor):	result = (nos ^ tos); break;
		case (op_inv):	result = ~(nos); break;
		case (op_lsh):	result = (nos << tos); break;
		case (op_rsh):	result = (nos >> tos); break;

		// Сравнение.
		case (op_eq):	result = (nos == tos); break;
		case (op_neq):	result = (nos != tos); break;
		case (op_lt):	result = (nos < tos); break;
		case (op_gt):	result = (nos > tos); break;

		// Некорректный опкод.
		default: status = est_inc_opcode; goto lb_exec_op_end;
	}	// switch op

	// Сохранение на вершине стека результата выполнения операции.
	vm->stack[vm->sp - 1] = result;

lb_exec_op_end:
	return status;
}	// exec_op

// = = = = Внешние функции компилятора = = = =
rom_t* bark_rom_init(void) {
	return (rom_t*)(calloc(1, sizeof(rom_t)));
}	// bark_rom_init

word_t bark_compile(const char *data, rom_t *rom) {
	// Статус компиляции.
	cmp_status_t status = cst_success;

	// Инициализация контекста компилятора.
	ctx_t *ctx = calloc(1, sizeof(ctx_t));
	if (ctx == NULL) {
		status = cst_out_of_memory;
		goto lb_compile_end;	// Не вызывать free.
	}	// if ctx is null

	// Токенизация (разбиение исходного кода на токены).
	status = token((char *)(data), ctx);
	if (status) goto lb_compile_free;

	// Обработка выражений в круглых скобках (RPN).
	status = token_rpn(ctx);
	if (status) goto lb_compile_free;

	// Обработка определений (statement) и выражений (expression).
	while ((status == cst_success) && (ctx->token_id < ctx->token_count))
		status = statement(ctx, rom);
	if (status) goto lb_compile_free;

	// Формирование prebuilt переменных.
	if (make_prebuilt(ctx, rom) == -1) {
		status = cst_var_overflow;
		goto lb_compile_free;
	}	// if make_prebuilt

	// Поиск точки входа в программу (процедура main).
	word_t entry_point = search_proc("main", ctx);
	if (entry_point != -1)
		rom->entry_point = entry_point;
	else {
		status = cst_no_entry_point;
		goto lb_compile_free;
	}	// if entry_point

	// Инициализация идентификатора ROM.
	rom->magic = ROM_MAGIC;

lb_compile_free:
	// Очистка памяти, выделенной под токены.
	for (uword_t token_id = 0; token_id < ctx->token_count; token_id++)
		if (ctx->token[token_id]) free(ctx->token[token_id]);

	// Очистка контекста компиляции.
	free(ctx);

lb_compile_end:
	return status;
}	// bark_compile

vm_t* bark_vm_init(rom_t *rom) {
	vm_t *vm = NULL;
	// Проверка идентификатора ROM.
	if (rom->magic == ROM_MAGIC) {
		// Выделение памяти под ВМ.
		vm = calloc(1, sizeof(vm_t));
		if (vm) {
			// Инициаилизация ВМ.
			vm->rom	= rom;
			vm->pc	= rom->entry_point;
		}	// if vm
	}	// if rom magic
	return vm;
}	// bark_vm_init

void bark_vm_reset(vm_t *vm) {
	// Если указатель на ВМ валиден.
	if (vm) {
		rom_t *rom = vm->rom;
		memset(vm, 0x00, sizeof(vm_t));
		vm->rom	= rom;
		vm->pc	= rom->entry_point;
	}	// if vm
}	// bark_vm_reset

word_t bark_exec(vm_t *vm, word_t cycle) {
	// Статус выполнения программы.
	ex_status_t status = est_work;

	// Получение указателя на данные в виде массива байт.
	char *data_byte = (char *)(vm->rom->data);

	// Цикл выполнения опкодов.
	while ((status == est_work) && (cycle-- > 0)) {
		// Выбор по опкоду.
		switch (vm->rom->code[vm->pc++]) {
			// Общие инструкции.
			case (op_halt): status = est_halt; break;
			case (op_int): status = est_int; break;
			case (op_ext): status = ext(vm->rom->code[vm->pc++], vm); break;

			// Работа со стеком.
			case (op_push): STACK_PUSH(vm->rom->code[vm->pc++]); EXEC_CHECK_SP_OFW; break;
			case (op_drop): EXEC_CHECK_SP_UFW(1); vm->sp--; break;
			case (op_ld): EXEC_CHECK_SP_UFW(1); STACK_TOP = vm->rom->data[STACK_TOP]; break;
			case (op_bld): EXEC_CHECK_SP_UFW(1); STACK_TOP = data_byte[STACK_TOP]; break;
			case (op_st): { EXEC_CHECK_SP_UFW(2); word_t addr = STACK_POP; word_t value = STACK_POP; vm->rom->data[addr] = value; break; }
			case (op_bst): { EXEC_CHECK_SP_UFW(2); char value = (char)(STACK_POP); word_t addr = STACK_POP; data_byte[addr] = value; break; }

			// Адресация.
			case (op_addr): STACK_PUSH((word_t)&(vm->rom->data[vm->rom->data[vm->pc++]])); EXEC_CHECK_SP_OFW; break;
			case (op_iaddr) : STACK_PUSH((word_t)&(vm->rom->data[vm->rom->data[vm->rom->code[vm->pc++]]])); EXEC_CHECK_SP_OFW; break;
			case (op_ild): EXEC_CHECK_SP_UFW(1); STACK_TOP = *(word_t *)(STACK_TOP); break;
			case (op_ibld): EXEC_CHECK_SP_UFW(1); STACK_TOP = *(char *)(STACK_TOP); break;
			case (op_ist): { EXEC_CHECK_SP_UFW(2); word_t value = STACK_POP; word_t *addr = (word_t *)(STACK_POP); *addr = value; break; }
			case (op_ibst): { EXEC_CHECK_SP_UFW(2); char value = (char)(STACK_POP); char *addr = (char *)(STACK_POP); *addr = value; break; }

			// Ветвление.
			case (op_jump): vm->pc = vm->rom->code[vm->pc]; break;
			case (op_jz): EXEC_CHECK_SP_UFW(1); vm->pc = (STACK_POP == 0) ? vm->rom->code[vm->pc] : (word_t)(vm->pc + 1); break;
			case (op_call): vm->call_stack[vm->csp++] = (vm->pc + 1); vm->pc = vm->rom->code[vm->pc]; EXEC_CHECK_CSP_OFW; break;
			case (op_ret): EXEC_CHECK_CSP_UFW(1); vm->pc = vm->call_stack[--vm->csp]; break;

			// Арифметика, логические, битовые операции и сравнение.
			default: status = exec_op(vm); break;
		}	// switch opcode
	}	// while status
lb_exec_end:
	return status;
}	// bark_exec

void bark_vm_push(vm_t *vm, word_t word) {
	if (vm->sp < STACK_SIZE)
		STACK_PUSH(word);
}	// bark_vm_push

word_t bark_vm_store(vm_t *vm) {
	word_t word = 0;
	if (vm->sp) word = STACK_POP;
	return word;
}	// bark_vm_store

void bark_rom_free(rom_t *rom) {
	if (rom) free(rom);
}	// bark_rom_free

void bark_vm_free(vm_t *vm) {
	if (vm) free(vm);
}	// bark_vm_free
