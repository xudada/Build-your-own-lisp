/*
 * main.c
 *
 *  Created on: 2017年3月5日
 *      Author: wengle
 */

#include "parser.h"

#ifdef _WIN32
static char input[SIZE];
char* readline(char *prompt){
	fputs(prompt, stdout);
	fgets(input, SIZE, stdin);
	char *cp = malloc(strlen(input)+1);
	strcpy(cp, input);
	cp[strlen(input)-1] = '\0';
	return cp;
}
void add_history(char *unused){}

#else

#include <editline/readline.h>
#include <editline/history.h>

#endif

mpc_parser_t* Number;
mpc_parser_t* Symbol;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr;
mpc_parser_t* Wengle;

int main(int argc, char **argv) {
	/* create parsers */
	Number = mpc_new("number");
	Symbol = mpc_new("symbol");
	String = mpc_new("string");
	Comment = mpc_new("comment");
	Sexpr = mpc_new("sexpr");
	Qexpr = mpc_new("qexpr");
	Expr = mpc_new("expr");
	Wengle = mpc_new("wengle");

	/* define parser with the following rule */
	mpca_lang(MPCA_LANG_DEFAULT,
			"\
			number: /-?[0-9]+(\\.?[0-9]+)?/;	\
			symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/;	\
			string  : /\"(\\\\.|[^\"])*\"/ ;             \
			comment : /;[^\\r\\n]*/ ;                    \
			sexpr: '('<expr>*')';	\
			qexpr: '{'<expr>*'}';	\
			expr: <number>|<symbol>|<string>|<comment>|<sexpr>|<qexpr>;	\
			wengle: /^/ <expr>* /$/;	\
			", Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Wengle);
	puts("Wengle Version 1.0");
	puts("Press Ctrl+c to Exit\n");

	lenv *e = lenv_new();
	lenv_add_builtins(e);

	if (argc == 1) {

		puts("Lispy Version 0.0.0.1.0");
		puts("Press Ctrl+c to Exit\n");

		while (1) {

			char* input = readline("wengle> ");
			add_history(input);

			if(strcmp(input, "exit") == 0){
				mpc_cleanup(8, Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Wengle);
				lval_exit(input, e);
			}

			mpc_result_t r;
			if (mpc_parse("<stdin>", input, Wengle, &r)) {

				lval* x = lval_eval(e, lval_read(r.output));
				lval_println(x);
				lval_del(x);

				mpc_ast_delete(r.output);
			} else {
				mpc_err_print(r.error);
				mpc_err_delete(r.error);
			}

			free(input);

		}
	}

	if (argc >= 2) {

		/* loop over each supplied filename (starting from 1) */
		for (int i = 1; i < argc; i++) {

			/* Argument list with a single argument, the filename */
			lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));

			/* Pass to builtin load and get the result */
			lval* x = builtin_load(e, args);

			/* If the result is an error be sure to print it */
			if (x->type == LVAL_ERR) {
				lval_println(x);
			}
			lval_del(x);
		}
	}
	lenv_del(e);
	mpc_cleanup(8, Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Wengle);
	return 0;
}

lval* lval_eval_sexpr(lenv *e, lval *v){
	for(int i = 0; i < v->count; i++){
		v->cell[i] = lval_eval(e, v->cell[i]);
	}

	for(int i = 0; i < v->count; i++){
		if(v->cell[i]->type == LVAL_ERR) {
			return lval_take(v, i);
		}
	}

	if(v->count == 0){
		return v;
	}
	if(v->count == 1){
		return lval_take(v, 0);
	}

	lval *f = lval_pop(v, 0);
	if(f->type != LVAL_FUN){
		lval* err = lval_err("S-Expression starts with incorrect type. "
		    "Got %s, Expected %s.",
		    ltype_name(f->type), ltype_name(LVAL_FUN));
		lval_del(v);
		lval_del(f);
		return err;
	}
	lval *result = lval_call(e, f, v);
	lval_del(f);
	return result;
}

lval* lval_eval(lenv *e, lval *v){
	if(v->type == LVAL_SYM){
		lval *x = lenv_get(e, v);
		lval_del(v);
		return x;
	}
	if(v->type == LVAL_SEXPR){
		return lval_eval_sexpr(e, v);
	}
	return v;
}

lval* lval_call(lenv *e, lval *f, lval *a) {
	if(f->builtin) return f->builtin(e, a);

	int given = a->count;
	int total = f->formals->count;

	while(a->count){
		if(f->formals->count == 0){
			lval_del(a);
			return lval_err("Function passed too many arguments. "
			        "Got %i, Expected %i.", given, total);
		}
		lval *sym = lval_pop(f->formals, 0);
		if(strcmp(sym->sym, "&") == 0){
			if(f->formals->count != 1){
				lval_del(a);
				return lval_err("Function format invalid. "
					      "Symbol '&' not followed by single symbol.");
			}
			lval* nsym = lval_pop(f->formals, 0);
			lenv_put(f->env, nsym, builtin_list(e, a));
			lval_del(sym); lval_del(nsym);
			break;
		}
		lval *val = lval_pop(a, 0);
		lenv_put(f->env, sym, val);
		lval_del(sym);
		lval_del(val);
	}
	lval_del(a);

	if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {
		/* Check to ensure that & is not passed invalidly. */
		if (f->formals->count != 2) {
			return lval_err("Function format invalid. "
					"Symbol '&' not followed by single symbol.");
		}
		/* Pop and delete '&' symbol */
		lval_del(lval_pop(f->formals, 0));
		/* Pop next symbol and create empty list */
		lval* sym = lval_pop(f->formals, 0);
		lval* val = lval_qexpr();
		/* Bind to environment and delete */
		lenv_put(f->env, sym, val);
		lval_del(sym);
		lval_del(val);
	}

	if(f->formals->count == 0){
		f->env->par = e;
		return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
	}else{
		return lval_copy(f);
	}
}

int lval_eq(lval *x, lval *y) {
	/* Different Types are always unequal */
	if (x->type != y->type) {
		return 0;
	}

	/* Compare Based upon type */
	switch (x->type) {
		/* Compare Number Value */
		case LVAL_NUM:
			return ((int)(x->num) == (int)(y->num));

			/* Compare String Values */
		case LVAL_ERR:
			return (strcmp(x->err, y->err) == 0);
		case LVAL_SYM:
			return (strcmp(x->sym, y->sym) == 0);
		case LVAL_STR:
			return (strcmp(x->str, y->str) == 0);
			/* If builtin compare, otherwise compare formals and body */
		case LVAL_FUN:
			if (x->builtin || y->builtin) {
				return x->builtin == y->builtin;
			} else {
				return lval_eq(x->formals, y->formals)
						&& lval_eq(x->body, y->body);
			}

			/* If list compare every individual element */
		case LVAL_QEXPR:
		case LVAL_SEXPR:
			if (x->count != y->count) {
				return 0;
			}
			for (int i = 0; i < x->count; i++) {
				/* If any element not equal then whole list not equal */
				if (!lval_eq(x->cell[i], y->cell[i])) {
					return 0;
				}
			}
			/* Otherwise lists must be equal */
			return 1;
			break;
	}
	return 0;
}

lval* lval_copy(lval *v) {
	lval *x = malloc(sizeof(lval));
	x->type = v->type;
	switch(v->type){
		case LVAL_FUN:
			if(v->builtin){
				x->builtin = v->builtin;
			}else{
				x->builtin = NULL;
				x->env = lenv_copy(v->env);
				x->formals = lval_copy(v->formals);
				x->body = lval_copy(v->body);
			}
			break;
		case LVAL_NUM:
			x->num = v->num;
			break;
		case LVAL_ERR:
			x->err = malloc(strlen(v->err)+1);
			strcpy(x->err, v->err);
			break;
		case LVAL_SYM:
			x->sym = malloc(strlen(v->sym)+1);
			strcpy(x->sym, v->sym);
			break;
		case LVAL_STR:
			x->str = malloc(strlen(v->str) + 1);
			strcpy(x->str, v->str);
			break;
		case LVAL_SEXPR:
		case LVAL_QEXPR:
			x->count = v->count;
			x->cell = malloc(sizeof(lval*)*(x->count));
			for(int i = 0; i < x->count; i++){
				x->cell[i] = lval_copy(v->cell[i]);
			}
			break;
	}
	return x;
}

lval* lval_pop(lval *v, int i){
	lval *x = v->cell[i];
	memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*)*(v->count-i-1));
	v->count--;
	v->cell = realloc(v->cell, sizeof(lval*)*v->count);
	return x;
}

lval* lval_take(lval *v, int i){
	lval *x = lval_pop(v, i);
	lval_del(v);
	return x;
}

lval* builtin_load(lenv* e, lval* a) {
	LASSERT_NUM("load", a, 1);
	LASSERT_TYPE("load", a, 0, LVAL_STR);

	/* Parse File given by string name */
	mpc_result_t r;
	if (mpc_parse_contents(a->cell[0]->str, Wengle, &r)) {

		/* Read contents */
		lval* expr = lval_read(r.output);
		mpc_ast_delete(r.output);

		/* Evaluate each Expression */
		while (expr->count) {
			lval* x = lval_eval(e, lval_pop(expr, 0));
			/* If Evaluation leads to error print it */
			if (x->type == LVAL_ERR) {
				lval_println(x);
			}
			lval_del(x);
		}

		/* Delete expressions and arguments */
		lval_del(expr);
		lval_del(a);

		/* Return empty list */
		return lval_sexpr();

	} else {
		/* Get Parse Error as String */
		char* err_msg = mpc_err_string(r.error);
		mpc_err_delete(r.error);

		/* Create new error message using it */
		lval* err = lval_err("Could not load Library %s", err_msg);
		free(err_msg);
		lval_del(a);

		/* Cleanup and return error */
		return err;
	}
}


lval* builtin_add(lenv *e, lval *a) {
	return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv *e, lval *a) {
	return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv *e, lval *a) {
	return builtin_op(e, a, "*");
}

lval* builtin_div(lenv *e, lval *a) {
	return builtin_op(e, a, "/");
}

lval* builtin_mod(lenv *e, lval *a) {
	return builtin_op(e, a, "%");
}

lval* builtin_pow(lenv *e, lval *a) {
	return builtin_op(e, a, "^");
}

lval* builtin_max(lenv *e, lval *a) {
	return builtin_op(e, a, "max");
}

lval* builtin_min(lenv *e, lval *a) {
	return builtin_op(e, a, "min");
}

lval* builtin_op(lenv *e, lval *a, char *op){
	for(int i = 0; i < a->count; i++){
		if(a->cell[i]->type != LVAL_NUM){
			lval_del(a);
			return lval_err("Cannot operate on non-number!");
		}
	}

	lval *x = lval_pop(a, 0);
	if((strcmp(op, "-") == 0) && a->count == 0){
		x->num = -x->num;
	}
	while(a->count > 0){
		lval *y = lval_pop(a, 0);
		if(strcmp(op, "max") == 0) { x->num = x->num > y->num ? x->num : y->num;}
		if(strcmp(op, "min") == 0) { x->num = x->num < y->num ? x->num : y->num;}
		if(strcmp(op, "+") == 0) { x->num += y->num; }
		if(strcmp(op, "-") == 0) { x->num -= y->num; }
		if(strcmp(op, "*") == 0) { x->num *= y->num; }
		if(strcmp(op, "/") == 0) {
			if((int)(y->num) == 0){
				lval_del(x);
				lval_del(y);
				x = lval_err("Division By Zero!");
				break;
			}
			x->num /= y->num;
		}
		if(strcmp(op, "%") == 0) {
			if((int)(y->num) == 0){
				lval_del(x);
				lval_del(y);
				x = lval_err("Division By Zero!");
				break;
			}
			x->num = fmod(x->num, y->num);
		}
		if(strcmp(op, "^") == 0) {
			if((int)(x->num) <= 0) {
				x = lval_err("a ^ x--a can't less equal than zero!");
				break;
			}
			x->num = pow(x->num, y->num);
		}
		lval_del(y);
	}
	lval_del(a);
	return x;
}

lval* builtin_gt(lenv *e, lval *a) {
	return builtin_ord(e, a, ">");
}

lval* builtin_lt(lenv* e, lval* a) {
	return builtin_ord(e, a, "<");
}

lval* builtin_ge(lenv* e, lval* a) {
	return builtin_ord(e, a, ">=");
}

lval* builtin_le(lenv* e, lval* a) {
	return builtin_ord(e, a, "<=");
}

lval* builtin_ord(lenv* e, lval* a, char* op) {
	LASSERT_NUM(op, a, 2);
	LASSERT_TYPE(op, a, 0, LVAL_NUM);
	LASSERT_TYPE(op, a, 1, LVAL_NUM);

	int r;
	if (strcmp(op, ">") == 0) {
		r = ((int)(a->cell[0]->num) > (int)(a->cell[1]->num));
	}
	if (strcmp(op, "<") == 0) {
		r = ((int)(a->cell[0]->num) < (int)(a->cell[1]->num));
	}
	if (strcmp(op, ">=") == 0) {
		r = ((int)(a->cell[0]->num) >= (int)(a->cell[1]->num));
	}
	if (strcmp(op, "<=") == 0) {
		r = ((int)(a->cell[0]->num) <= (int)(a->cell[1]->num));
	}
	lval_del(a);
	return lval_num(r);
}

lval* builtin_if(lenv* e, lval* a) {
	LASSERT_NUM("if", a, 3);
	LASSERT_TYPE("if", a, 0, LVAL_NUM);
	LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
	LASSERT_TYPE("if", a, 2, LVAL_QEXPR);

	/* Mark Both Expressions as evaluable */
	lval* x;
	a->cell[1]->type = LVAL_SEXPR;
	a->cell[2]->type = LVAL_SEXPR;

	if (a->cell[0]->num) {
		/* If condition is true evaluate first expression */
		x = lval_eval(e, lval_pop(a, 1));
	} else {
		/* Otherwise evaluate second expression */
		x = lval_eval(e, lval_pop(a, 2));
	}

	/* Delete argument list and return */
	lval_del(a);
	return x;
}

lval* builtin_cmp(lenv* e, lval* a, char* op) {
	LASSERT_NUM(op, a, 2);
	int r;
	if (strcmp(op, "==") == 0) {
		r = lval_eq(a->cell[0], a->cell[1]);
	}
	if (strcmp(op, "!=") == 0) {
		r = !lval_eq(a->cell[0], a->cell[1]);
	}
	lval_del(a);
	return lval_num(r);
}

lval* builtin_eq(lenv* e, lval* a) {
	return builtin_cmp(e, a, "==");
}

lval* builtin_ne(lenv* e, lval* a) {
	return builtin_cmp(e, a, "!=");
}

char* ltype_name(int t) {
	switch (t) {
		case LVAL_FUN:
			return "Function";
		case LVAL_NUM:
			return "Number";
		case LVAL_ERR:
			return "Error";
		case LVAL_SYM:
			return "Symbol";
		case LVAL_STR:
			return "String";
		case LVAL_SEXPR:
			return "S-Expression";
		case LVAL_QEXPR:
			return "Q-Expression";
		default:
			return "Unknown";
	}
}

lval* builtin_head(lenv *e, lval *a){
	LASSERT(a, a->count == 1, "Function 'head' passed too many arguments."
			"Got %i, Expected %i.", a->count, 1);
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
			"Function 'head' passed incorrect types."
			"Got %s, Expected %s.",
			ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
	LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}!");

	lval *v = lval_take(a, 0);
	while(v->count > 1) {
		lval_del(lval_pop(v, 1));
	}
	return v;
}

lval* builtin_tail(lenv *e, lval *a){
	LASSERT(a, a->count == 1, "Function 'tail' passed too many arguments!");
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
			"Function 'tail' passed incorrect types!");
	LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed {}!");

	lval *v = lval_take(a, 0);
	lval_del(lval_pop(v, 0));
	return v;
}

lval* builtin_list(lenv *e, lval *a){
	a->type = LVAL_QEXPR;
	return a;
}

lval* builtin_eval(lenv *e, lval *a){
	LASSERT(a, a->count == 1, "Function 'eval' passed too many arguments!");
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
			"Function 'eval' passed incorrect type!");

	lval *x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval(e, x);
}

lval* builtin_join(lenv *e, lval *a){
	for(int i = 0; i < a->count; i++){
		LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
				"Function 'join' passed incorrect type.");
	}
	lval *x = lval_pop(a, 0);
	while(a->count){
		x = lval_join(x, lval_pop(a, 0));
	}
	lval_del(a);
	return x;
}

lval* lval_join(lval *x, lval *y){
	while(y->count){
		x = lval_add(x, lval_pop(y, 0));
	}
	lval_del(y);
	return x;
}

lval* builtin_cons(lenv *e, lval *a){
	LASSERT(a, a->count == 2,
			"Function 'cons' should passed two arguments!");
	LASSERT(a, a->cell[0]->type == LVAL_NUM && a->cell[1]->type == LVAL_QEXPR,
			"Function 'cons' passed incorrect type.");
	lval *val = lval_pop(a, 0);
	lval *x = lval_pop(a, 0);
	x->cell = realloc(x->cell, sizeof(lval*)*((x->count)+1));
	if(x->count != 0){
		memmove(&x->cell[1], &x->cell[0], sizeof(lval*)*(x->count));
	}
	x->cell[0] = val;
	x->count++;
	lval_del(a);
	return x;
}

lval* builtin_len(lenv *e, lval *a) {
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
			"Function 'len' passed incorrect type.");
	lval *x = lval_num(a->cell[0]->count);
	lval_del(a);
	return x;
}

lval* builtin_init(lenv *e, lval *a) {
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
			"Function 'init' passed incorrect type.");
	LASSERT(a, a->cell[0]->count != 0,
			"Function 'init' passed {}!");
	lval_del(lval_pop(a->cell[0], a->cell[0]->count-1));
	return a;
}

lval* builtin_error(lenv* e, lval* a) {
	LASSERT_NUM("error", a, 1);
	LASSERT_TYPE("error", a, 0, LVAL_STR);

	/* Construct Error from first argument */
	lval* err = lval_err(a->cell[0]->str);

	/* Delete arguments and return */
	lval_del(a);
	return err;
}

lval* builtin_print(lenv* e, lval* a) {

	/* Print each argument followed by a space */
	for (int i = 0; i < a->count; i++) {
		lval_print(a->cell[i]);
		putchar(' ');
	}

	/* Print a newline and delete arguments */
	putchar('\n');
	lval_del(a);

	return lval_sexpr();
}

lval* builtin_def(lenv *e, lval *a) {
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
			"Function 'def' passed incorrect type!");
	lval *syms = a->cell[0];
	for(int i = 0; i < syms->count; i++){
		LASSERT(a, syms->cell[i]->type == LVAL_SYM,
				"Function 'def' cannot define non-symbol");
	}
	LASSERT(a, syms->count == a->count-1,
			"Function 'def' cannot define incorrect number of values to symbols");
	for(int i = 0; i < syms->count; i++){
		lenv_put(e, syms->cell[i], a->cell[i+1]);
	}
	lval_del(a);
	return lval_sexpr();
}

lval* builtin_put(lenv *e, lval *a) {
	return builtin_var(e, a, "=");
}

lval* builtin_var(lenv *e, lval *a, char *func) {
	LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

	lval *syms = a->cell[0];
	for(int i = 0; i < syms->count; i++){
		LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
				"Function '%s' cannot define non-symbol. "
				"Got %s, Expected %s.", func,
				ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
	}
	LASSERT(a, (syms->count == a->count-1),
			"Function '%s' passed too many arguments for symbols. "
			"Got %i, Expected %i.", func, syms->count, a->count-1);
	for(int i = 0; i < syms->count; i++) {
		if(strcmp(func, "def") == 0){
			lenv_def(e, syms->cell[i], a->cell[i+1]);
		}
		if(strcmp(func, "=") == 0){
			lenv_put(e, syms->cell[i], a->cell[i+1]);
		}
	}
	lval_del(a);
	return lval_sexpr();
}

lval *builtin_lambda(lenv *e, lval *a) {
	// \ {x y} {+ x y}
	LASSERT_NUM("\\", a, 2);
	LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
	LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

	for(int i = 0; i < a->cell[0]->count; i++){
		LASSERT(a, a->cell[0]->cell[i]->type == LVAL_SYM,
				"Cannot define non-symbol. Got %s, Expected %s.",
				ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
	}

	lval *formals = lval_pop(a, 0);
	lval *body = lval_pop(a, 0);
	lval_del(a);

	return lval_lambda(formals, body);
}

lval* lval_read_num(mpc_ast_t *t){
	errno = 0;
	double x = strtod(t->contents, NULL);
	return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read_str(mpc_ast_t* t) {
	/* Cut off the final quote character */
	t->contents[strlen(t->contents) - 1] = '\0';
	/* Copy the string missing out the first quote character */
	char* unescaped = malloc(strlen(t->contents + 1) + 1);
	strcpy(unescaped, t->contents + 1);
	/* Pass through the unescape function */
	unescaped = mpcf_unescape(unescaped);
	/* Construct a new lval using the string */
	lval* str = lval_str(unescaped);
	/* Free the string and return */
	free(unescaped);
	return str;
}

lval* lval_read(mpc_ast_t *t){
	if(strstr(t->tag, "number")) { return lval_read_num(t);}
	if(strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
	if(strstr(t->tag, "string")) { return lval_read_str(t); }

	lval *x = NULL;
	if(strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
	if(strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
	if(strstr(t->tag, "qexpr")) { x = lval_qexpr(); }
	int i = 0;
	for(i = 0; i < t->children_num; i++){
		if(strcmp(t->children[i]->contents, "(") == 0) { continue; }
		if(strcmp(t->children[i]->contents, ")") == 0) { continue; }
		if(strcmp(t->children[i]->contents, "{") == 0) { continue; }
		if(strcmp(t->children[i]->contents, "}") == 0) { continue; }
		if(strstr(t->children[i]->tag, "comment")) { continue; }
		if(strcmp(t->children[i]->tag, "regex") == 0) { continue; }
		x = lval_add(x, lval_read(t->children[i]));
	}
	return x;
}

lval* lval_add(lval *v, lval *x){
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval*) * (v->count));
	v->cell[v->count-1] = x;
	return v;
}

/* construct and deconstruct functions */
lval* lval_num(double x){
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

lval* lval_err(char *fmt, ...){
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_ERR;

	va_list va;
	va_start(va, fmt);
	v->err = malloc(512);
	vsnprintf(v->err, 511, fmt, va);

	v->err = realloc(v->err, strlen(v->err)+1);
	va_end(va);
	return v;
}

lval* lval_sym(char *sym){
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(sym)+1);
	strcpy(v->sym, sym);
	return v;
}

lval* lval_str(char *s){
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_STR;
	v->str = malloc(strlen(s)+1);
	strcpy(v->str, s);
	return v;
}

lval* lval_sexpr(void){
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_qexpr(void){
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_fun(lbuiltin func) {
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->builtin = func;
	return v;
}

lval* lval_lambda(lval *formals, lval *body){
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->builtin = NULL;
	v->env = lenv_new();
	v->formals = formals;
	v->body = body;
	return v;
}

void lval_del(lval *v){
	switch(v->type){
		case LVAL_NUM:
			break;
		case LVAL_ERR:
			free(v->err);
			break;
		case LVAL_SYM:
			free(v->sym);
			break;
		case LVAL_STR:
			free(v->str);
			break;
		case LVAL_FUN:
			if(!v->builtin){
				lenv_del(v->env);
				lval_del(v->formals);
				lval_del(v->body);
			}
			break;
		case LVAL_QEXPR:
		case LVAL_SEXPR:
			for(int i = 0; i < v->count; i++){
				lval_del(v->cell[i]);
			}
			free(v->cell);
			break;
	}
	free(v);
}

/* print functions */
void lval_expr_print(lval *v, char open, char close){
	putchar(open);
	int i = 0;
	for(i = 0; i < v->count; i++){
		lval_print(v->cell[i]);
		if(i == (v->count - 1)){
			putchar(' ');
		}
	}
	putchar(close);
}

void lval_print_str(lval* v) {
	/* Make a Copy of the string */
	char* escaped = malloc(strlen(v->str) + 1);
	strcpy(escaped, v->str);
	/* Pass it through the escape function */
	escaped = mpcf_escape(escaped);
	/* Print it between " characters */
	printf("\"%s\"", escaped);
	/* free the copied string */
	free(escaped);
}

void lval_print(lval *v){
	switch(v->type){
		case LVAL_NUM:
			printf("%.3f", v->num);
			break;
		case LVAL_ERR:
			printf("Error: %s", v->err);
			break;
		case LVAL_SYM:
			printf("%s", v->sym);
			break;
		case LVAL_STR:
			lval_print_str(v);
			break;
		case LVAL_FUN:
			if(v->builtin){
				printf("<builtin>");
			}else{
				printf("(\\");
				lval_print(v->formals);
				putchar(' ');
				lval_print(v->body);
				putchar(')');
			}
			break;
		case LVAL_SEXPR:
			lval_expr_print(v, '(', ')');
			break;
		case LVAL_QEXPR:
			lval_expr_print(v, '{', '}');
			break;
	}
}

void lval_println(lval *v){
	lval_print(v);
	putchar('\n');
}

lval* lval_exit(char *input, lenv *e){
	free(input);
	lenv_del(e);
	exit(0);
}

lenv* lenv_new(void) {
	lenv *e = malloc(sizeof(lenv));
	e->par = NULL;
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

lenv* lenv_copy(lenv *e) {
	lenv *n = malloc(sizeof(lenv));
	n->par = e->par;
	n->count = e->count;
	n->syms = malloc(sizeof(char*)*(n->count));
	n->vals = malloc(sizeof(char*)*(n->count));
	for(int i = 0; i < n->count; i++){
		n->syms[i] = malloc(strlen(e->syms[i])+1);
		strcpy(n->syms[i], e->syms[i]);
		n->vals[i] = lval_copy(e->vals[i]);
	}
	return n;
}

void lenv_del(lenv *e) {
	for(int i = 0; i < e->count; i++){
		free(e->syms[i]);
		lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

lval* lenv_get(lenv *e, lval *k) {
	for(int i = 0; i < e->count; i++){
		if(strcmp(e->syms[i], k->sym) == 0){
			return lval_copy(e->vals[i]);
		}
	}
	if(e->par){
		return lenv_get(e->par, k);
	}else{
		return lval_err("unbound symbol '%s'", k->sym);
	}
}

void lenv_def(lenv *e, lval *k, lval *v) {
	while(e->par) { e = e->par; }
	lenv_put(e, k, v);
}

void lenv_put(lenv *e, lval *k, lval *v){
	for(int i = 0; i < e->count; i++){
		if(strcmp(e->syms[i], k->sym) == 0){
			lval_del(e->vals[i]);
			e->vals[i] = lval_copy(v);
			return;
		}
	}
	e->count++;
	e->vals = realloc(e->vals, sizeof(lval*) * (e->count));
	e->syms = realloc(e->syms, sizeof(char*) * (e->count));
	e->vals[e->count-1] = lval_copy(v);
	e->syms[e->count-1] = malloc(strlen(k->sym)+1);
	strcpy(e->syms[e->count-1], k->sym);
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
	lval *k = lval_sym(name);
	lval *v = lval_fun(func);
	lenv_put(e, k, v);
	lval_del(k);
	lval_del(v);
}

void lenv_add_builtins(lenv *e) {
	/* List Functions */
	lenv_add_builtin(e, "list", builtin_list);
	lenv_add_builtin(e, "head", builtin_head);
	lenv_add_builtin(e, "tail", builtin_tail);
	lenv_add_builtin(e, "eval", builtin_eval);
	lenv_add_builtin(e, "join", builtin_join);
	lenv_add_builtin(e, "cons", builtin_cons);
	lenv_add_builtin(e, "len", builtin_len);
	lenv_add_builtin(e, "init", builtin_init);
	lenv_add_builtin(e, "def", builtin_def);
	lenv_add_builtin(e, "=", builtin_put);
	lenv_add_builtin(e, "\\",  builtin_lambda);

	/* Mathematical Functions */
	lenv_add_builtin(e, "+", builtin_add);
	lenv_add_builtin(e, "-", builtin_sub);
	lenv_add_builtin(e, "*", builtin_mul);
	lenv_add_builtin(e, "/", builtin_div);
	lenv_add_builtin(e, "%", builtin_mod);
	lenv_add_builtin(e, "^", builtin_pow);
	lenv_add_builtin(e, "max", builtin_max);
	lenv_add_builtin(e, "min", builtin_min);

	/* Comparison Functions */
	lenv_add_builtin(e, "if", builtin_if);
	lenv_add_builtin(e, "==", builtin_eq);
	lenv_add_builtin(e, "!=", builtin_ne);
	lenv_add_builtin(e, ">",  builtin_gt);
	lenv_add_builtin(e, "<",  builtin_lt);
	lenv_add_builtin(e, ">=", builtin_ge);
	lenv_add_builtin(e, "<=", builtin_le);

	lenv_add_builtin(e, "load",  builtin_load);
	lenv_add_builtin(e, "error", builtin_error);
	lenv_add_builtin(e, "print", builtin_print);
}

int countLeaf(mpc_ast_t *t){
	if(t->children_num == 0) return 1;
	int sum = 0;
	int i = 0;
	for(i = 0; i < t->children_num; i++){
		sum += countLeaf(t->children[i]);
	}
	return sum;
}

int countBranch(mpc_ast_t *t){
	if(t->children_num == 0) return 0;
	int sum = 0;
	int i = 0;
	sum = t->children_num;
	for(i = 0; i < t->children_num; i++){
		sum += countBranch(t->children[i]);
	}
	return sum;
}

