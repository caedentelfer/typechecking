/**
 * @file    amplc.c
 *
 * A recursive-descent compiler for the AMPL-2023 language.
 *
 * All scanning errors are handled in the scanner.  Parser errors MUST be
 * handled by the <code>abort_c</code> function.  System and environment errors
 * -- for example, running out of memory -- MUST be handled in the unit in which
 * they occur.  Transient errors -- for example, nonexistent files, MUST be
 * reported where they occur.  There are no warnings, which is to say, all
 * errors are fatal and MUST cause compilation to terminate with an abnormal
 * error code.
 *
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @author  C.J Telfer    (25526693@sun.ac.za)
 * @date    2023-07-04
 */

#include "boolean.h"
#include "errmsg.h"
#include "error.h"
#include "hashtable.h"
#include "scanner.h"
#include "stdarg.h"
#include "symboltable.h"
#include "token.h"
#include "valtypes.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* --- type definitions ----------------------------------------------------- */

/* TODO: Uncomment the following for use during type checking. */

#if 1
typedef struct variable_s Variable;
struct variable_s {
	char *id;       /**< variable identifier                                */
	ValType type;   /**< variable type                                      */
	SourcePos pos;  /**< position of the variable in the source             */
	Variable *next; /**< pointer to the next variable in the list           */
};
#endif

/* --- debugging ------------------------------------------------------------ */

#ifdef DEBUG_PARSER
void debug_start(const char *fmt, ...);
void debug_end(const char *fmt, ...);
void debug_info(const char *fmt, ...);
#define DBG_start(...) debug_start(__VA_ARGS__)
#define DBG_end(...)   debug_end(__VA_ARGS__)
#define DBG_info(...)  debug_info(__VA_ARGS__)
#else
#define DBG_start(...)
#define DBG_end(...)
#define DBG_info(...)
#endif /* DEBUG_PARSER */

/* --- global variables ----------------------------------------------------- */

Token token; /**< the lookahead token type                           */
#if 1
ValType return_type; /**< the return type of the current subroutine          */
#endif

/* TODO: Uncomment the previous definition for use during type checking. */

/* --- helper macros -------------------------------------------------------- */

#define STARTS_FACTOR(toktype)                                                 \
	(toktype == TOK_ID || toktype == TOK_NUM || toktype == TOK_LPAREN ||       \
	 toktype == TOK_NOT || toktype == TOK_TRUE || toktype == TOK_FALSE)

#define STARTS_EXPR(toktype)                                                   \
	(toktype == TOK_MINUS || toktype == TOK_ID || toktype == TOK_NUM ||        \
	 toktype == TOK_LPAREN || toktype == TOK_NOT || toktype == TOK_TRUE ||     \
	 toktype == TOK_FALSE)

#define IS_ADDOP(toktype) (toktype >= TOK_MINUS && toktype <= TOK_PLUS)

#define IS_MULOP(toktype) (toktype >= TOK_AND && toktype <= TOK_REM)

#define IS_ORDOP(toktype)                                                      \
	(toktype == TOK_GT || toktype == TOK_GE || toktype == TOK_LT ||            \
	 toktype == TOK_LE)

#define IS_RELOP(toktype) (toktype >= TOK_EQ && toktype <= TOK_NE)

#define IS_TYPE(toktype)  (toktype = TOK_BOOL || toktype == TOK_INT)

/* --- function prototypes: parser routines --------------------------------- */

void parse_program(void);
void parse_subdef(void);
void parse_body(void);
void parse_type(ValType *t0);
void parse_vardef(void);
void parse_statements(void);
void parse_statement(void);
void parse_assign(void);
void parse_call(void);
void parse_if(void);
void parse_input(void);
void parse_output(void);
void parse_return(void);
void parse_while(void);
void parse_arglist(char *id, SourcePos idpos);
void parse_index(char *id);
void parse_expr(ValType *t0);
void parse_relop(void);
void parse_simple(ValType *t0);
void parse_addop(void);
void parse_term(ValType *t0);
void parse_mulop(void);
void parse_factor(ValType *t0);
void parse_id(void);
void parse_num(void);
void parse_string(void);
void parse_letter(void);
void parse_digit(void);

/* --- function prototypes: helper routines --------------------------------- */

/* TODO: Uncomment the following commented-out prototypes for use during type
 * checking.
 */

#if 1
void chktypes(ValType found, ValType expected, SourcePos *pos, ...);
#endif
void expect(TokenType type);
void expect_id(char **id);

/* --- function prototypes: constructors ------------------------------------ */

#if 1
IDPropt *idpropt(ValType type,
                 unsigned int offset,
                 unsigned int nparams,
                 ValType *params);
Variable *variable(char *id, ValType type, SourcePos pos);
#endif

/* --- function prototypes: error reporting --------------------------------- */

void abort_c(Error err, ...);
void abort_cp(SourcePos *posp, Error err, ...);

/* --- main routine --------------------------------------------------------- */

/**
 * Main method for compiling ampl
 */
int main(int argc, char *argv[])
{
#if 0
	char *jasmin_path;
#endif
	FILE *src_file;

	/* TODO: Uncomment the previous definition for code generation. */

	/* set up global variables */
	setprogname(argv[0]);

	/* check command-line arguments and environment */
	if (argc != 2) {
		eprintf("usage: %s <filename>", getprogname());
	}

	/* TODO: Uncomment the following code for code generation:
	if ((jasmin_path = getenv("JASMIN_JAR")) == NULL) {
	    eprintf("JASMIN_JAR environment variable not set");
	}
	*/

	setsrcname(argv[1]);

	/* open the source file, and report an error if it cannot be opened */
	if ((src_file = fopen(argv[1], "r")) == NULL) {
		eprintf("file '%s' could not be opened:", argv[1]);
	}

	/* initialise all compiler units */
	init_scanner(src_file);
	init_symbol_table();

	/* compile */
	get_token(&token);
	parse_program();

	/* produce the object code, and assemble */
	/* TODO: Add calls for code generation. */

	/* release all allocated resources */
	/* TODO: Release the resources of the symbol table and code generation. */
	fclose(src_file);
	freeprogname();
	freesrcname();
	release_symbol_table();

#ifdef DEBUG_PARSER
	printf("Success!\n");
#endif

	return EXIT_SUCCESS;
}

/* --- parser routines ------------------------------------------------------ */

/*
 * program = "program" id ":" { subdef } "main" ":" body .
 */
void parse_program(void)
{
	char *class_name;
	SourcePos origin;

	DBG_start("<program>");

	/* TODO: For code generation, set the class name inside this function, and
	 * also handle initialising and closing the "main" function.  But from the
	 * perspective of simple parsing, this function is complete.
	 */

	origin.line = 1;
	origin.col = 0;
	if (token.type == TOK_EOF) {
		abort_cp(&origin, ERR_EXPECT, TOK_PROGRAM);
	}
	expect(TOK_PROGRAM);

	expect_id(&class_name);
	expect(TOK_COLON);

	while (token.type == TOK_ID) {
		parse_subdef();
	}

	expect(TOK_MAIN);
	expect(TOK_COLON);

	parse_body();

	if (token.type != TOK_EOF) {
		abort_c(ERR_UNREACHABLE, get_token_string(token.type));
	}

	free(class_name);

	DBG_end("</program>");
}

/**
 * subdef = id "(" type id {"," type id} ")" ["->" type] ":" body -$
 */
void parse_subdef(void)
{
	DBG_start("<subdef>");

	char *id, *subid;
	SourcePos subpos, pos;
	ValType t1, *params;
	Variable *head, *temp, *newvar;
	unsigned int count, i, width;
	IDPropt *prop;

	subpos = position;
	count = 0;
	head = NULL;
	t1 = 0;
	id = NULL;
	return_type = TYPE_NONE;

	expect_id(&subid);
	expect(TOK_LPAREN);

	parse_type(&t1);
	pos = position;
	expect_id(&id);
	head = variable(id, t1, pos);
	head->next = NULL;
	count = 1;
	temp = head;

	while (token.type == TOK_COMMA) {
		get_token(&token);
		t1 = 0;
		parse_type(&t1);
		pos = position;
		expect_id(&id);
		newvar = variable(id, t1, pos);
		newvar->next = NULL;
		temp->next = newvar;
		temp = newvar;
		count++;
	}

	expect(TOK_RPAREN);
	params = emalloc(count * sizeof(Variable));
	temp = head;
	for (i = 0; i < count; i++) {
		params[i] = temp->type;
		temp = temp->next;
	}
	t1 = TYPE_CALLABLE;

	if (token.type == TOK_ARROW) {
		get_token(&token);
		parse_type(&t1);
	}
	return_type = t1;
	width = get_variables_width();
	prop = idpropt(t1, width, count, params);

	if (open_subroutine(subid, prop)) {
		while (head != NULL) {
			temp = head;
			prop = NULL;
			if (find_name(temp->id, &prop)) {
				position = temp->pos;
				abort_c(ERR_MULTIPLE_DEFINITION, temp->id);
			}
			prop = NULL;
			width = get_variables_width();
			prop = idpropt(temp->type, width, 0, NULL);
			if (!insert_name(temp->id, prop)) {
				position = temp->pos;
				abort_c(ERR_MULTIPLE_DEFINITION, temp->id);
			}
			head = head->next;
			free(temp);
			temp = NULL;
		}
		expect(TOK_COLON);
		parse_body();
		close_subroutine();
		return_type = TYPE_NONE;
	} else {
		position = subpos;
		abort_c(ERR_MULTIPLE_DEFINITION, subid);
	}

	DBG_end("</subdef>");
}

/**
 * body = {vardef} statements -$
 */
void parse_body(void)
{
	DBG_start("<body>");

	while (token.type == TOK_BOOL || token.type == TOK_INT) {
		parse_vardef();
	}

	parse_statements();

	DBG_end("</body>");
}

/**
 * type = ("bool" | "int") ["array"] - $
 */
void parse_type(ValType *t0)
{
	DBG_start("<type>");

	if (token.type == TOK_BOOL || token.type == TOK_INT) {
		if (token.type == TOK_BOOL) {
			*t0 |= TYPE_BOOLEAN;
		} else {
			*t0 |= TYPE_INTEGER;
		}
		get_token(&token);
		if (token.type == TOK_ARRAY) {
			get_token(&token);
			*t0 |= TYPE_ARRAY;
		}

	} else {
		abort_c(ERR_EXPECTED_TYPE_SPECIFIER);
	}

	DBG_end("</type>");
}

/**
 * vardef = type id {"," id} ";" -$
 */
void parse_vardef(void)
{
	char *id;
	ValType t1;
	IDPropt *prop;
	SourcePos pos;
	unsigned int width;

	DBG_start("<vardef>");

	t1 = 0;
	parse_type(&t1);
	pos = position;
	expect_id(&id);

	if (find_name(id, &prop)) {
		position = pos;
		abort_c(ERR_MULTIPLE_DEFINITION, id);
	}
	width = get_variables_width();
	prop = idpropt(t1, width, 0, NULL);

	if (!insert_name(id, prop)) {
		position = pos;
		abort_c(ERR_MULTIPLE_DEFINITION, id);
	}

	while (token.type == TOK_COMMA) {
		get_token(&token);
		pos = position;
		expect_id(&id);

		if (find_name(id, &prop)) {
			position = pos;
			abort_c(ERR_MULTIPLE_DEFINITION, id);
		}

		width = get_variables_width();
		prop = idpropt(t1, width, 0, NULL);
		if (!insert_name(id, prop)) {
			position = pos;
			abort_c(ERR_MULTIPLE_DEFINITION, id);
		}
	}

	expect(TOK_SEMICOLON);

	DBG_end("</vardef>");
}

/**
 * statements = "chillax" | statement {";" statement} -$
 */
void parse_statements(void)
{
	DBG_start("<statements>");

	if (token.type == TOK_CHILLAX) {
		get_token(&token);
	} else {
		parse_statement();
		while (token.type == TOK_SEMICOLON) {
			get_token(&token);
			parse_statement();
		}
	}

	DBG_end("</statements>");
}

/**
 * statement = assign | call | if | input | output | return | while -$
 */
void parse_statement(void)
{
	DBG_start("<statement>");

	switch (token.type) {
		case TOK_LET:
			parse_assign();
			break;
		case TOK_ID:
			parse_call();
			break;
		case TOK_IF:
			parse_if();
			break;
		case TOK_INPUT:
			parse_input();
			break;
		case TOK_OUTPUT:
			parse_output();
			break;
		case TOK_RETURN:
			parse_return();
			break;
		case TOK_WHILE:
			parse_while();
			break;
		default:
			abort_c(ERR_EXPECTED_STATEMENT);
	}

	DBG_end("</statement>");
}

/**
 * assign = "let" id [index] "=" (expr | "array" simple) -$
 */
void parse_assign(void)
{
	char *id;
	ValType t1, proptype, original_proptype;
	IDPropt *prop;
	bool indexed;
	SourcePos idpos, pos;

	DBG_start("<assign>");

	expect(TOK_LET);
	idpos = position;
	expect_id(&id);
	indexed = false;

	if (!find_name(id, &prop)) {
		position = idpos;
		abort_c(ERR_UNKNOWN_IDENTIFIER, id);
	}
	proptype = prop->type;
	original_proptype = proptype;

	if (IS_CALLABLE_TYPE(prop->type)) {
		position = idpos;
		abort_c(ERR_NOT_A_VARIABLE, id);
	}

	if (token.type == TOK_LBRACK) {
		if (!IS_ARRAY_TYPE(prop->type)) {
			position = idpos;
			abort_c(ERR_NOT_AN_ARRAY, id);
		}
		proptype ^= TYPE_ARRAY;

		indexed = true;
		parse_index(id);
	}

	expect(TOK_EQ);
	pos = position;
	t1 = proptype;

	if (STARTS_EXPR(token.type)) {
		parse_expr(&t1);

		if (indexed) {
			if (!IS_ARRAY(t1)) {
				chktypes(t1, proptype, &pos,
				         "for allocation to indexed array '%s'", id);
			}
		} else {
			if (IS_ARRAY(t1) != IS_ARRAY(original_proptype)) {
				chktypes(t1, original_proptype, &pos, "for assignment to '%s'",
				         id);
			}
		}

		if (IS_INTEGER_TYPE(proptype) && !IS_INTEGER_TYPE(t1)) {
			chktypes(t1, proptype, &pos, "for assignment to '%s'", id);
		} else if (IS_BOOLEAN_TYPE(proptype) && !IS_BOOLEAN_TYPE(t1)) {
			chktypes(t1, proptype, &pos, "for assignment to '%s'", id);
		}
	} else if (token.type == TOK_ARRAY) {

		if (!IS_ARRAY(original_proptype)) {
			position = idpos;
			abort_c(ERR_NOT_AN_ARRAY, id);
		}

		get_token(&token);
		pos = position;
		parse_simple(&t1);
		chktypes(t1, TYPE_INTEGER, &pos, "for array size of '%s'", id);
	} else {
		abort_c(ERR_EXPECTED_EXPRESSION_OR_ARRAY_ALLOCATION);
	}

	DBG_end("</assign>");
}

/**
 * call = id arglist -$
 */
void parse_call(void)
{
	ValType type;
	IDPropt *prop;
	char *id;
	SourcePos idpos;

	DBG_start("<call>");

	idpos = position;
	expect_id(&id);

	if (!find_name(id, &prop)) {
		position = idpos;
		abort_c(ERR_UNKNOWN_IDENTIFIER, id);
	}

	if (IS_FUNCTION(prop->type)) {
		position = idpos;
		abort_c(ERR_NOT_A_PROCEDURE);
	}

	type = prop->type;
	if (!IS_CALLABLE_TYPE(type)) {
		position = idpos;
		if (IS_FUNCTION(type)) {
			abort_c(ERR_NOT_A_FUNCTION, id);
		} else {
			abort_c(ERR_NOT_A_PROCEDURE, id);
		}
	}

	parse_arglist(id, idpos);

	DBG_end("</call>");
}

/**
 * id = "if" expr ":" statements {"elif" expr ":" statements}
 * 		["else" ":" statements] "end" -$
 */
void parse_if(void)
{
	ValType t1;
	SourcePos pos;

	DBG_start("<if>");

	expect(TOK_IF);
	pos = position;
	parse_expr(&t1);
	chktypes(t1, TYPE_BOOLEAN, &pos, "for 'if' guard");
	expect(TOK_COLON);
	parse_statements();

	while (token.type == TOK_ELIF) {
		get_token(&token);
		pos = position;
		parse_expr(&t1);
		chktypes(t1, TYPE_BOOLEAN, &pos, "for 'elif' guard");
		expect(TOK_COLON);
		parse_statements();
	}

	if (token.type == TOK_ELSE) {
		DBG_start("<else>");
		get_token(&token);
		expect(TOK_COLON);
		parse_statements();
		DBG_end("</else>");
	}

	expect(TOK_END);

	DBG_end("</if>");
}

/**
 * input = "input" "(" id [index] ")" -$
 */
void parse_input(void)
{
	DBG_start("<input>");

	char *id;
	IDPropt *prop;
	SourcePos pos;

	expect(TOK_INPUT);
	expect(TOK_LPAREN);
	pos = position;
	expect_id(&id);

	if (!find_name(id, &prop)) {
		position = pos;
		abort_c(ERR_UNKNOWN_IDENTIFIER, id);
	}

	if (token.type == TOK_LBRACK) {
		if (!IS_ARRAY(prop->type)) {
			position = pos;
			abort_c(ERR_NOT_AN_ARRAY, id);
		}
		parse_index(id);
	} else if (IS_ARRAY(prop->type)) {
		position = pos;
		abort_c(ERR_EXPECTED_SCALAR);
	}

	expect(TOK_RPAREN);

	DBG_end("</input>");
}

/**
 * output = "output" "(" (string | expr) {".." (string | expr)} ")" -$
 */
void parse_output(void)
{
	ValType t1;
	SourcePos pos;

	DBG_start("<output>");

	pos = position;
	expect(TOK_OUTPUT);
	expect(TOK_LPAREN);

	if (token.type == TOK_STR) {
		parse_string();
	} else if (STARTS_EXPR(token.type)) {
		parse_expr(&t1);
		if (IS_ARRAY(t1)) {
			position = pos;
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION, "'output'");
		}
	} else {
		abort_c(ERR_EXPECTED_EXPRESSION_OR_STRING);
	}

	while (token.type == TOK_DOTDOT) {
		pos = position;
		get_token(&token);
		if (token.type == TOK_STR) {
			parse_string();
		} else if (STARTS_EXPR(token.type)) {
			parse_expr(&t1);
			if (IS_ARRAY(t1)) {
				position = pos;
				abort_c(ERR_ILLEGAL_ARRAY_OPERATION, "'output'");
			}
		} else {
			abort_c(ERR_EXPECTED_EXPRESSION_OR_STRING);
		}
	}

	expect(TOK_RPAREN);

	DBG_end("</output>");
}

/**
 * return = "return" [expr] -$
 */
void parse_return(void)
{
	ValType t1, t2;
	SourcePos pos;

	DBG_start("<return>");

	t1 = 0;
	pos = position;
	expect(TOK_RETURN);

	if (IS_PROCEDURE(return_type)) {
		abort_c(ERR_RETURN_EXPRESSION_NOT_ALLOWED);
	}

	if (STARTS_EXPR(token.type)) {
		if (IS_PROCEDURE(return_type)) {
			abort_c(ERR_RETURN_EXPRESSION_NOT_ALLOWED);
		} else if (IS_FUNCTION(return_type)) {
			pos = position;
			parse_expr(&t1);
			t2 = return_type;
			SET_RETURN_TYPE(t2);
			SET_RETURN_TYPE(t1);
			chktypes(t1, t2, &pos, "for 'return' statement");
		}
	} else if (IS_FUNCTION(return_type)) {
		position = pos;
		abort_c(ERR_MISSING_RETURN_EXPRESSION);
	} else if (IS_PROCEDURE(return_type)) {
		abort_c(ERR_RETURN_EXPRESSION_NOT_ALLOWED);
	}

	DBG_end("</return>");
}

/**
 * while = "while" expr ":" statements "end" -$
 */
void parse_while(void)
{
	ValType t1;
	SourcePos pos;

	DBG_start("<while>");

	expect(TOK_WHILE);
	pos = position;
	parse_expr(&t1);
	chktypes(t1, TYPE_BOOLEAN, &pos, "for 'while' guard");
	expect(TOK_COLON);
	parse_statements();
	expect(TOK_END);

	DBG_end("</while>");
}

/**
 * arglist = "(" expr {"," expr} ")" -$
 */
void parse_arglist(char *id, SourcePos idpos)
{
	ValType t1;
	unsigned int i;
	IDPropt *prop;
	SourcePos pos;

	DBG_start("<arglist>");

	if (!find_name(id, &prop)) {
		position = idpos;
		abort_c(ERR_UNKNOWN_IDENTIFIER, id);
	}
	i = 0;

	expect(TOK_LPAREN);
	if (STARTS_EXPR(token.type)) {
		pos = position;
		parse_expr(&t1);
		if (!IS_ARRAY_TYPE(t1) && !IS_ARRAY_TYPE(prop->params[i])) {
			if (!((IS_INTEGER_TYPE(t1) && IS_INTEGER_TYPE(prop->params[i])) ||
			      (IS_BOOLEAN_TYPE(t1) && IS_BOOLEAN_TYPE(prop->params[i])) ||
			      (IS_CALLABLE_TYPE(t1) &&
			       IS_CALLABLE_TYPE(prop->params[i])))) {
				chktypes(t1, prop->params[i], &pos,
				         "for argument %d of call to '%s'", i + 1, id);
			}
		} else {
			chktypes(t1, prop->params[i], &pos,
			         "for argument %d of call to '%s'", i + 1, id);
		}
		i++;

		while (token.type == TOK_COMMA) {
			if (i >= prop->nparams) {
				abort_c(ERR_TOO_MANY_ARGUMENTS, id);
			}
			get_token(&token);
			pos = position;
			parse_expr(&t1);
			if (!IS_ARRAY_TYPE(t1) && !IS_ARRAY_TYPE(prop->params[i])) {
				if (!((IS_INTEGER_TYPE(t1) &&
				       IS_INTEGER_TYPE(prop->params[i])) ||
				      (IS_BOOLEAN_TYPE(t1) &&
				       IS_BOOLEAN_TYPE(prop->params[i])) ||
				      (IS_CALLABLE_TYPE(t1) &&
				       IS_CALLABLE_TYPE(prop->params[i])))) {
					chktypes(t1, prop->params[i], &pos,
					         "for argument %d of call to '%s'", i + 1, id);
				}
			} else {
				chktypes(t1, prop->params[i], &pos,
				         "for argument %d of call to '%s'", i + 1, id);
			}
			i++;
		}
		if (i < prop->nparams) {
			abort_c(ERR_TOO_FEW_ARGUMENTS, id);
		}
	}

	expect(TOK_RPAREN);

	DBG_end("</arglist>");
}

/**
 * index = "[" simple "]" -$
 */
void parse_index(char *id)
{
	ValType t1;
	IDPropt *prop;
	SourcePos pos;

	DBG_start("<index>");

	find_name(id, &prop);
	expect(TOK_LBRACK);
	pos = position;
	parse_simple(&t1);
	chktypes(t1, TYPE_INTEGER, &pos, "for array index of '%s'", id);
	expect(TOK_RBRACK);

	DBG_end("</index>");
}

/**
 * expr = simple [relop simple] -$
 */
void parse_expr(ValType *t0)
{
	ValType t1, t2;
	TokenType toktype;
	SourcePos pos;

	DBG_start("<expr>");

	parse_simple(&t1);
	if (IS_RELOP(token.type)) {
		toktype = token.type;

		if (IS_ARRAY(t1)) {
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION, get_token_string(toktype));
		}
		pos = position;
		parse_relop();
		parse_simple(&t2);

		if (IS_ARRAY(t2)) {
			position = pos;
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION, get_token_string(toktype));
		}

		if (toktype == TOK_EQ || toktype == TOK_NE) {
			chktypes(t1, t2, &pos, "for operator %s",
			         get_token_string(toktype));
			*t0 = TYPE_BOOLEAN;
		} else {
			chktypes(t1, TYPE_INTEGER, &pos, "for operator %s",
			         get_token_string(toktype));
			chktypes(t2, TYPE_INTEGER, &pos, "for operator %s",
			         get_token_string(toktype));
			*t0 = TYPE_BOOLEAN;
		}
	} else {
		*t0 = t1;
	}

	DBG_end("</expr>");
}

/**
 * relop = "=" | ">=" | ">" | "<=" | "<" | "/="
 */
void parse_relop(void)
{
	get_token(&token);
}

/**
 * simple = ["-"] term {addop term}
 */
void parse_simple(ValType *t0)
{
	ValType t1;
	SourcePos pos, pos2;
	TokenType toktype;

	DBG_start("<simple>");

	t1 = 0;
	if (token.type == TOK_MINUS) {
		pos = position;
		get_token(&token);
		pos2 = pos;
		parse_term(t0);
		if (IS_ARRAY(*t0)) {
			position = pos;
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION, "unary minus");
		}
		pos = position;
		pos2.col++;
		chktypes(*t0, TYPE_INTEGER, &pos2, "for unary minus");
	} else {
		parse_term(t0);
		if (IS_ADDOP(token.type)) {
			if (IS_ARRAY(*t0)) {
				abort_c(ERR_ILLEGAL_ARRAY_OPERATION,
				        get_token_string(token.type));
			}
		}

		while (IS_ADDOP(token.type)) {
			toktype = token.type;
			pos = position;
			get_token(&token);
			parse_term(&t1);
			if (IS_ARRAY(t1)) {
				position = pos;
				abort_c(ERR_ILLEGAL_ARRAY_OPERATION, get_token_string(toktype));
			}
			if (toktype == TOK_OR) {
				chktypes(*t0, TYPE_BOOLEAN, &pos, "for operator %s",
				         get_token_string(toktype));
				chktypes(t1, TYPE_BOOLEAN, &pos, "for operator %s",
				         get_token_string(toktype));
			} else {
				if (!IS_INTEGER_TYPE(*t0)) {
					chktypes(*t0, TYPE_INTEGER, &pos, "for operator %s",
					         get_token_string(toktype));
				}
				if (!IS_INTEGER_TYPE(t1)) {
					chktypes(t1, TYPE_INTEGER, &pos, "for operator %s",
					         get_token_string(toktype));
				}
			}
		}
	}
	DBG_end("</simple>");
}

/**
 * addop = "-" | "or" | "+"
 */
void parse_addop(void)
{
	DBG_start("<addop>");

	get_token(&token);

	DBG_end("</addop>");
}

/**
 * term = factor {mulop factor} -$ ?
 */
void parse_term(ValType *t0)
{
	ValType t1;
	SourcePos pos;
	TokenType toktype;

	DBG_start("<term>");

	parse_factor(t0);
	if (IS_MULOP(token.type)) {
		if (IS_ARRAY(*t0)) {
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION, get_token_string(token.type));
		}
	}

	while (IS_MULOP(token.type)) {
		toktype = token.type;
		pos = position;
		parse_mulop();
		parse_factor(&t1);

		if (IS_ARRAY(t1)) {
			position = pos;
			abort_c(ERR_ILLEGAL_ARRAY_OPERATION, get_token_string(toktype));
		}
		if (toktype == TOK_AND) {
			chktypes(*t0, TYPE_BOOLEAN, &pos, "for operator %s",
			         get_token_string(toktype));
			chktypes(t1, TYPE_BOOLEAN, &pos, "for operator %s",
			         get_token_string(toktype));
		} else {
			chktypes(*t0, TYPE_INTEGER, &pos, "for operator %s",
			         get_token_string(toktype));
			chktypes(t1, TYPE_INTEGER, &pos, "for operator %s",
			         get_token_string(toktype));
		}
	}

	DBG_end("</term>");
}

/**
 * mulop = "and" | "/" | "*" | "rem"
 */
void parse_mulop(void)
{
	get_token(&token);
}

/**
 * factor = id [index | arglist] | num | "(" expr ")" | "not" factor
 * 			| "true" | "false" --$
 */
void parse_factor(ValType *t0)
{
	char *id;
	IDPropt *prop;
	SourcePos pos, pos_not;

	DBG_start("<factor>");

	id = NULL;

	switch (token.type) {
		case TOK_ID:
			pos = position;
			expect_id(&id);
			if (!find_name(id, &prop)) {
				position = pos;
				abort_c(ERR_UNKNOWN_IDENTIFIER, id);
			}

			if (token.type == TOK_LBRACK) {
				if (!IS_ARRAY_TYPE(prop->type)) {
					position = pos;
					abort_c(ERR_NOT_AN_ARRAY, id);
				}
				*t0 = prop->type & 6;
				parse_index(id);
			} else if (token.type == TOK_LPAREN) {
				if (!IS_FUNCTION(prop->type)) {
					position = pos;
					abort_c(ERR_NOT_A_FUNCTION, id);
				}
				*t0 = (prop->type & 6) ^ TYPE_CALLABLE;
				parse_arglist(id, pos);
			} else {
				*t0 = prop->type;
			}
			break;
		case TOK_NUM:
			*t0 = TYPE_INTEGER;
			get_token(&token);
			break;
		case TOK_LPAREN:
			expect(TOK_LPAREN);
			parse_expr(t0);
			expect(TOK_RPAREN);
			break;
		case TOK_NOT:
			pos_not = position;
			expect(TOK_NOT);
			pos = position;
			parse_factor(t0);
			if (IS_ARRAY_TYPE(*t0)) {
				position = pos_not;
				abort_c(ERR_ILLEGAL_ARRAY_OPERATION, "'not'");
			}
			chktypes(*t0, TYPE_BOOLEAN, &pos, "for 'not'");
			break;
		case TOK_TRUE:
			*t0 = TYPE_BOOLEAN;
			expect(TOK_TRUE);
			break;
		case TOK_FALSE:
			*t0 = TYPE_BOOLEAN;
			expect(TOK_FALSE);
			break;
		default:
			abort_c(ERR_EXPECTED_FACTOR);
	}

	DBG_end("</factor>");
}

/**
 * string = """ {printable ASCII} """
 */
void parse_string(void)
{
	get_token(&token);
}

/* TODO: Turn the EBNF into a program by writing one parse function for those
 * productions as instructed in the specification.  I suggest you use the
 * production as comment to the function.  Also, you may only report errors
 * through the `abort_c` and `abort_cp` functions.  You must figure out what
 * arguments you should pass for each particular error.  Keep to the *exact*
 * error messages given in the specification.
 */

/* --- helper routines ------------------------------------------------------ */

#define MAX_MSG_LEN 256

/* TODO: Uncomment the following function for use during type checking. */

#if 1
void chktypes(ValType found, ValType expected, SourcePos *pos, ...)
{
	char buf[MAX_MSG_LEN], *s;
	va_list ap;

	if (found != expected) {
		buf[0] = '\0';
		va_start(ap, pos);
		s = va_arg(ap, char *);
		vsnprintf(buf, MAX_MSG_LEN, s, ap);
		va_end(ap);
		if (pos) {
			position = *pos;
		}
		leprintf("incompatible types (expected %s, found %s) %s",
		         get_valtype_string(expected), get_valtype_string(found), buf);
	}
}
#endif

/**
 * Compares expected token to the current token
 *
 * @param[in] TokenType type
 * 			The expected token
 */
void expect(TokenType type)
{
	if (token.type == type) {
		get_token(&token);
	} else {
		abort_c(ERR_EXPECT, type);
	}
}

/**
 * Checks for valid id
 *
 * @param[in] char **id
 * 			pointer to the pointer of the id to be inspected
 */
void expect_id(char **id)
{
	if (token.type == TOK_ID) {
		*id = estrdup(token.lexeme);
		get_token(&token);
	} else {
		abort_c(ERR_EXPECT, TOK_ID);
	}
}

/* --- constructors --------------------------------------------------------- */

/* TODO: Uncomment the following two functions for use during type checking. */

#if 1
IDPropt *idpropt(ValType type,
                 unsigned int offset,
                 unsigned int nparams,
                 ValType *params)
{
	IDPropt *ip = emalloc(sizeof(*ip));

	ip->type = type;
	ip->offset = offset;
	ip->nparams = nparams;
	ip->params = params;

	return ip;
}

Variable *variable(char *id, ValType type, SourcePos pos)
{
	Variable *vp = emalloc(sizeof(*vp));

	vp->id = id;
	vp->type = type;
	vp->pos = pos;
	vp->next = NULL;

	return vp;
}
#endif

/* --- error handling routines ---------------------------------------------- */

/**
 * handles all errors for parsing and type checking
 */
void _abort_cp(SourcePos *posp, Error err, va_list args)
{
	char expstr[MAX_MSG_LEN], *s;
	int t;

	if (posp) {
		position = *posp;
	}

	snprintf(expstr, MAX_MSG_LEN, "expected %%s, but found %s",
	         get_token_string(token.type));

	switch (err) {
		case ERR_EXPECTED_SCALAR:
		case ERR_ILLEGAL_ARRAY_OPERATION:
		case ERR_MULTIPLE_DEFINITION:
		case ERR_NOT_AN_ARRAY:
		case ERR_NOT_A_FUNCTION:
		case ERR_NOT_A_PROCEDURE:
		case ERR_NOT_A_VARIABLE:
		case ERR_TOO_FEW_ARGUMENTS:
		case ERR_TOO_MANY_ARGUMENTS:
		case ERR_UNKNOWN_IDENTIFIER:
		case ERR_UNREACHABLE:
		case ERR_EXPECTED_TYPE_SPECIFIER:
		case ERR_EXPECTED_STATEMENT:
		case ERR_EXPECTED_FACTOR:
		case ERR_EXPECTED_EXPRESSION_OR_ARRAY_ALLOCATION:
		case ERR_EXPECTED_EXPRESSION_OR_STRING:
		case ERR_RETURN_EXPRESSION_NOT_ALLOWED:
		case ERR_MISSING_RETURN_EXPRESSION:
			s = va_arg(args, char *);
			break;
		case ERR_EXPECT:
			break;
		default:
			err = ERR_UNREACHABLE;
	}

	switch (err) {

			/* TODO: Add additional cases here as is necessary, referring to
			 * errmsg.h for all possible errors.  Some errors only become
			 * possible to recognise once we add type checking.  Until you get
			 * to type checking, you can handle such errors by adding the
			 * default case. However, your final submission *must* handle all
			 * cases explicitly.
			 */

		case ERR_EXPECT:
			t = va_arg(args, int);
			leprintf(expstr, get_token_string(t));
			break;
		case ERR_EXPECTED_FACTOR:
			leprintf(expstr, "factor");
			break;
		case ERR_UNREACHABLE:
			leprintf("unreachable: %s", s);
			break;
		case ERR_EXPECTED_TYPE_SPECIFIER:
			leprintf(expstr, "type specifier");
			break;
		case ERR_EXPECTED_STATEMENT:
			leprintf(expstr, "statement");
			break;
		case ERR_EXPECTED_EXPRESSION_OR_ARRAY_ALLOCATION:
			leprintf(expstr, "expression or array allocation");
			break;
		case ERR_EXPECTED_EXPRESSION_OR_STRING:
			leprintf(expstr, "expression or string");
			break;
		case ERR_MULTIPLE_DEFINITION:
			leprintf("multiple definition of '%s'", s);
			break;
		case ERR_UNKNOWN_IDENTIFIER:
			leprintf("unknown identifier '%s'", s);
			break;
		case ERR_NOT_A_VARIABLE:
			leprintf("'%s' is not a variable", s);
			break;
		case ERR_NOT_AN_ARRAY:
			leprintf("'%s' is not an array", s);
			break;
		case ERR_NOT_A_FUNCTION:
			leprintf("'%s' is not a function", s);
			break;
		case ERR_ILLEGAL_ARRAY_OPERATION:
			leprintf("%s is an illegal array operation", s);
			break;
		case ERR_MISSING_RETURN_EXPRESSION:
			leprintf("missing return expression for a function");
			break;
		case ERR_RETURN_EXPRESSION_NOT_ALLOWED:
			leprintf("a return expression is not allowed for a procedure");
			break;
		case ERR_TOO_FEW_ARGUMENTS:
			leprintf("too few arguments for call to '%s'", s);
			break;
		case ERR_TOO_MANY_ARGUMENTS:
			leprintf("too many arguments for call to '%s'", s);
			break;
		case ERR_NOT_A_PROCEDURE:
			leprintf("'%s' is not a procedure", s);
			break;
		case ERR_EXPECTED_SCALAR:
			leprintf("expected scalar variable instead of '%s'", s);
			break;
		default:
			leprintf("unreachable: %s", s);
	}
}

/**
 * handles errors
 */
void abort_c(Error err, ...)
{
	va_list args;

	va_start(args, err);
	_abort_cp(NULL, err, args);
	va_end(args);
}

/**
 * handles errors and appends position
 */
void abort_cp(SourcePos *posp, Error err, ...)
{
	va_list args;

	va_start(args, err);
	_abort_cp(posp, err, args);
	va_end(args);
}

/* --- debugging output routines -------------------------------------------- */

#ifdef DEBUG_PARSER

static int indent = 0;

void _debug_info(const char *fmt, va_list args)
{
	int i;
	char buf[MAX_MSG_LEN], *buf_ptr;

	buf_ptr = buf;

	for (i = 0; i < indent; i++) {
		*buf_ptr++ = ' ';
	}

	vsprintf(buf_ptr, fmt, args);
	buf_ptr += strlen(buf_ptr);
	snprintf(buf_ptr, MAX_MSG_LEN, " at %d:%d.\n", position.line, position.col);
	fflush(stdout);
	fputs(buf, stdout);
	fflush(NULL);
}

void debug_start(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	_debug_info(fmt, args);
	va_end(args);
	indent += 2;
}

void debug_end(const char *fmt, ...)
{
	va_list args;

	indent -= 2;
	va_start(args, fmt);
	_debug_info(fmt, args);
	va_end(args);
}

void debug_info(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	_debug_info(fmt, args);
	va_end(args);
}

#endif /* DEBUG_PARSER */
