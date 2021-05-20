#include "GBuild.h"
#include <G64/G64.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

LexState **lexp = NULL;

typedef struct
{
	char  *vars[128];
	size_t var_count;
} Scope;

static HashMap *variables = NULL;

static Scope scopes[256] = { 0 };

static size_t scope_top = 0;

void ScopePush()
{
	if(variables == NULL)
		variables = HashMapNew(1024, HashDefaultFunction);
	scopes[scope_top++] = (Scope) { { NULL }, 0 };
}

void ScopePop()
{
	Scope sc = scopes[--scope_top];

	for(size_t i = 0; i < sc.var_count; i++) {
		Variable *var = HashFind(variables, (uint8_t*) sc.vars[i], strlen(sc.vars[i]));
		if(var == NULL) continue;

		HashDelete(variables, (uint8_t*) sc.vars[i], strlen(sc.vars[i]));
	}
}

void VariableNew(Variable *var)
{
	Scope *sc = &scopes[scope_top - 1];

	sc->vars[sc->var_count++] = var->name;
	HashPut(variables, (uint8_t*) var->name, strlen(var->name), var);
}

Variable *VariableGet(const char *name)
{
	return HashFind(variables, (uint8_t*) name, strlen(name));
}


static uint8_t ws_stack[128] = { 0 };

static uint8_t ws_top = 0;

void WSPush()
{
	ws_stack[ws_top++] = lex->skip_ws;
}

void WSPop()
{
	lex->skip_ws = ws_stack[--ws_top];
}

static Value val_stack[1024] = { 0 };

static size_t val_top = 0;

void PushVal(Value *val)
{
	if(val_top == 1024) {
		printf("gbuild: fatal error: Stack size exceeded\n");
		exit(1);
	}
	val_stack[val_top++] = *val;
}

Value PopVal()
{
	if(val_top == 0) {
		printf("gbuild: fatal error: Stack underflow\n");
		exit(1);
	}
	return val_stack[--val_top];
}

Value PeekVal()
{
	return val_stack[val_top - 1];
}

double ValueNum(Value *val)
{
	if(val->type == VT_STRING) return 0;
	return val->type == VT_INT ? (double) val->cur_int : val->cur_float;
}

void PushInt(int64_t num)
{
	PushVal(&(Value) { .type = VT_INT, .cur_int = num });
}

int64_t PopInt()
{
	Value v = PopVal();
	if(v.type != VT_INT) {
		printf("gbuild: fatal error: Expected integer on the stack\n");
		exit(1);
	}

	return v.cur_int;
}

void PushFloat(double num)
{
	PushVal(&(Value) { .type = VT_FLOAT, .cur_float = num });
}

double PopFloat()
{
	Value v = PopVal();
	if(v.type != VT_FLOAT) {
		printf("gbuild: fatal error: Expected float on the stack\n");
		exit(1);
	}

	return v.cur_float;
}

void PushString(char *str)
{
	PushVal(&(Value) { .type = VT_STRING, .cur_str = str, .str_len = strlen(str) });
}

char *PopString()
{
	Value v = PopVal();
	if(v.type != VT_STRING) {
		printf("gbuild: fatal error: Expected string on the stack\n");
		exit(1);
	}

	return v.cur_str;
}

void ClearVal()
{
	val_top = 0;
}

void Expect(int64_t token)
{
	int64_t tok = LexPush(lexp);
	if(tok != token) {
		const char *n1 = GetTokenName(token);
		const char *n2 = GetTokenName(tok);
		printf("GBuildFile:%d: error: Expected %s, got %s\n", lex->line, n1, n2);
		exit(1);
	}
}

void ExpectIdent(const char *str)
{
	Expect(TK_IDENT);
	if(strcmp(str, lexl->cur_str) != 0) {
		printf("GBuildFile:%d: error: Expected identifier '%s', got '%s'\n", lex->line, str, lexl->cur_str);
		exit(1);
	}
}

int Accept(int64_t token)
{
	int64_t tok = LexPush(lexp);

	lex = lexl;
	LexStateDelete(lex->next);

	return tok == token;
}

int AcceptB(int64_t token)
{
	int64_t tok = LexPush(lexp);

	if(tok != token) {
		lex = lexl;
		LexStateDelete(lex->next);
	}

	return tok == token;
}

int AcceptIdent(const char *str)
{
	int r = AcceptB(TK_IDENT);
	if(r == 0) return 0;

	r = strcmp(str, lexl->cur_str) == 0;

	lex = lexl;
	LexStateDelete(lex->next);

	return r;
}

int AcceptIdentB(const char *str)
{
	int r = AcceptB(TK_IDENT);
	if(r == 0) return 0;

	r = strcmp(str, lexl->cur_str) == 0;

	if(r == 0) {
		lex = lexl;
		LexStateDelete(lex->next);
	}

	return r;
}
