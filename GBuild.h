#pragma once

#include <G64/G64.h>

extern LexState **lexp;

#define lex (*lexp)

#define lexl (lex->last)

#define VT_INT    0
#define VT_FLOAT  1
#define VT_STRING 2

typedef struct
{
	int type;

	int64_t  cur_int;
	double cur_float;
	char    *cur_str;
	size_t   str_len;
} Value;

typedef struct
{
	char  *name;
	Value value;
} Variable;

void ScopePush();

void ScopePop();

void VariableNew(Variable *var);

Variable *VariableGet(const char *name);

void WSPush();

void WSPop();

void PushVal(Value *val);

Value PopVal();

Value PeekVal();

double ValueNum(Value *val);

void PushInt(int64_t num);

int64_t PopInt();

void PushFloat(double num);

double PopFloat();

void PushString(char *str);

char *PopString();

void ClearVal();

void Expect(int64_t token);

void ExpectIdent(const char *str);

int Accept(int64_t token);

int AcceptB(int64_t token);

int AcceptIdent(const char *str);

int AcceptIdentB(const char *str);
