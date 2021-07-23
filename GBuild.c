#include "GBuild.h"
#include <G64/G64.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _BSD_SOURCE
#include <dirent.h>

const char *reserved[] = {"let", "if", "else", "cut", "lengthof"};

int IsReserved(const char *str)
{
	for(size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
		if(strcmp(str, reserved[i]) == 0) return 1;

	return 0;
}

void ErrorHandler(LexState *l, int64_t tok)
{
	printf("GBuildFile:%d: error: Can't analyze %s token\n", l->line, GetTokenName(tok));
	exit(1);
}

void ErrorHandle(LexState *l, const char *cause)
{
	printf("GBuildFile:%d: error: %s\n", l->line, cause);
	exit(1);
}

void PrsFactor();

void PrsTerm();

void PrsExpression();

void PrsStatement();

void PrsBody();

void PrsShell()
{
	Expect(TK_DOLLAR);

	PrsExpression();

	Value v = PopVal();

	if(v.type != VT_STRING)
		ErrorHandle(lex, "Can't execute a non-string value");

	if(!AcceptB(TK_AND))
		printf("%s\n", v.cur_str);

	int ret = system(v.cur_str);
	if(ret == -1)
		ret = 1;

	PushInt(ret);
}

void PrsLengthof()
{
	Expect(TK_LEFT_PHAR);

	PrsExpression();

	Value str = PopVal();

	if(str.type != VT_STRING)
		ErrorHandle(lex, "Can't get the length of a non-string value");

	Expect(TK_RIGHT_PHAR);

	PushInt(str.str_len);
}

void PrsCut()
{
	Expect(TK_LEFT_PHAR);

	PrsExpression();

	Value str = PopVal();

	if(str.type != VT_STRING)
		ErrorHandle(lex, "Can't cut a non-string value");

	Expect(TK_COMMA);

	PrsExpression();

	Value tmp   = PopVal();
	int64_t low = tmp.cur_int;

	if(tmp.type != VT_INT)
		ErrorHandle(lex, "Can't cut a string with non-integer lower bound");
	if(low < 0)
		ErrorHandle(lex, "Can't cut a string with negative lower bound");


	Expect(TK_COMMA);

	PrsExpression();

	tmp = PopVal();
	int64_t high = tmp.cur_int;

	if(tmp.type != VT_INT)
		ErrorHandle(lex, "Can't cut a string with non-integer upper bound");
	if(high < 0)
		ErrorHandle(lex, "Can't cut a string with negative upper bound");

	Expect(TK_RIGHT_PHAR);

	int64_t len = str.str_len - (low + high);

	if(len <= 0)
		ErrorHandle(lex, "Can't cut an entire string");

	char *nstr = calloc(len + 1, 1);

	memcpy(nstr, &str.cur_str[low], str.str_len - high - low);

	PushString(nstr);
}

void PrsFactor()
{
	if(Accept(TK_DOLLAR)) {
		PrsShell();
		return;
	}
	int64_t token = LexPush(lexp);

	switch(token)
	{
	case TK_LEFT_PHAR:
		PrsExpression();
		Expect(TK_RIGHT_PHAR);
		break;
	case TK_STRING:
		if(lexl->str_len == 0)
			ErrorHandle(lex, "Can't have zero length strings");
		PushString(lexl->cur_str);
		break;
	case TK_FLOAT:
		PushFloat(lexl->cur_float);
		break;
	case TK_INT:
		PushInt(lexl->cur_int);
		break;
	case TK_LOGICAL_NOT: {
		PrsExpression();

		Value val = PopVal();

		if(val.type == VT_STRING)
			ErrorHandle(lex, "Can't get the logical not of a string");

		if(val.type == VT_INT)
			PushInt(val.cur_int == 0);

		if(val.type == VT_FLOAT)
			PushInt(val.cur_float == 0);

		break;
	  }
	case TK_IDENT: {
		if(strcmp(lexl->cur_str, "cut") == 0) {
			PrsCut();
			break;
		} else if(strcmp(lexl->cur_str, "lengthof") == 0) {
			PrsLengthof();
			break;
		}

		Variable *var = VariableGet(lexl->cur_str);
		if(var == NULL) {
			printf("GBuildFile:%d: error: Can't find variable '%s'\n", lex->line, lexl->cur_str);
			exit(1);
		}

		LexState *saved = lex;

		if(AcceptB(TK_EQUALS)) {
			if(Accept(TK_EQUALS)) {
				lex = saved;
				PushVal(&var->value);
				break;
			}

			PrsExpression();
			var->value = PopVal();
		}

		PushVal(&var->value);
		break;
	  }
	default:
		ErrorHandle(lex, "Expected factor");
	}
}

void PrsTerm()
{
	PrsFactor();

	while(Accept(TK_STAR) || Accept(TK_SLASH)) {
		int64_t tok = LexPush(lexp);
		PrsFactor();

		Value v2 = PopVal();
		Value v1 = PopVal();

		if(tok == TK_STAR) {
			if(v1.type == VT_STRING || v2.type == VT_STRING) {
				Value str   = v1.type == VT_STRING ? v1 : v2;
				Value other = v1.type == VT_STRING ? v2 : v1;

				if(other.type != VT_INT)
					ErrorHandle(lex, "Can't multiply a string with a non-integer value");

				if(other.cur_int < 0)
					ErrorHandle(lex, "Can't multiply a string with a negative value");

				StringBuilder *builder = StringBuilderNew();

				for(int64_t i = 0; i < other.cur_int; i++)
					StringBuilderAppend(builder, "%s", str.cur_str);

				char *built_str = StringBuild(builder);

				StringBuilderDelete(builder);

				PushString(built_str);
			} else if(v1.type == VT_FLOAT || v2.type == VT_FLOAT) {
				PushFloat(ValueNum(&v1) * ValueNum(&v2));
			} else if(v1.type == VT_INT && v2.type == VT_INT) {
				PushInt(v1.cur_int * v2.cur_int); 
			}
		} else {
			if(v1.type == VT_STRING || v2.type == VT_STRING)
				ErrorHandle(lex, "Can't divide strings");

			if((v2.type == VT_INT ? v2.cur_int : v2.cur_float) == 0)
				ErrorHandle(lex, "Can't divide number by 0");

			PushFloat((double) (v1.type == VT_FLOAT ? v1.cur_float : v1.cur_int) /
				(double) (v2.type == VT_FLOAT ? v2.cur_float : v2.cur_int));
		}
	}
}

void PrsExpression0()
{
	PrsTerm();

	while(Accept(TK_PLUS) || Accept(TK_MINUS)) {
		int64_t tok = LexPush(lexp);
		PrsTerm();

		Value v2 = PopVal();
		Value v1 = PopVal();

		if(tok == TK_PLUS) {
			if(v1.type == VT_STRING || v2.type == VT_STRING) {

				StringBuilder *builder = StringBuilderNew();

				switch(v1.type)
				{
				case VT_INT: StringBuilderAppend(builder,    "%ld", v1.cur_int);  break;
				case VT_FLOAT: StringBuilderAppend(builder,  "%f", v1.cur_float); break;
				case VT_STRING: StringBuilderAppend(builder, "%s", v1.cur_str);   break;
				}

				switch(v2.type)
				{
				case VT_INT: StringBuilderAppend(builder,    "%ld", v2.cur_int);  break;
				case VT_FLOAT: StringBuilderAppend(builder,  "%f", v2.cur_float); break;
				case VT_STRING: StringBuilderAppend(builder, "%s", v2.cur_str);   break;
				}

				char *built_str = StringBuild(builder);

				StringBuilderDelete(builder);

				PushString(built_str);
			} else if(v1.type == VT_FLOAT || v2.type == VT_FLOAT) {

				PushFloat(ValueNum(&v1) + ValueNum(&v2));

			} else if(v1.type == VT_INT && v2.type == VT_INT) {
				PushInt(v1.cur_int + v2.cur_int); 
			}
		} else {
			if(v1.type == VT_STRING || v2.type == VT_STRING) {
				ErrorHandle(lex, "Can't subtract from strings");
			} else if(v1.type == VT_FLOAT || v2.type == VT_FLOAT) {
				PushFloat(ValueNum(&v1) - ValueNum(&v2));
			} else if(v1.type == VT_INT && v2.type == VT_INT) {
				PushInt(v1.cur_int - v2.cur_int); 
			}
		}
	}
}

void PrsExpression()
{
	PrsExpression0();

	while(Accept(TK_GREATER) || Accept(TK_LESSER) || Accept(TK_EQUALS)) {
		int64_t tok = LexPush(lexp);
		int aequ = 0;

		if(tok == TK_EQUALS)
			Expect(TK_EQUALS);

		if(tok != TK_EQUALS) {
			if(AcceptB(TK_EQUALS)) {
				aequ = 1;
			}
		}

		PrsExpression0();

		Value v2 = PopVal();
		Value v1 = PopVal();

		if(tok == TK_GREATER || tok == TK_LESSER) {
			if(v2.type == VT_STRING || v1.type == VT_STRING)
				ErrorHandle(lex, "Can't compare strings with greater / lesser signs");

			if(tok == TK_GREATER) {
				if(aequ)
					PushInt(ValueNum(&v1) >= ValueNum(&v2));
				else
					PushInt(ValueNum(&v1) > ValueNum(&v2));
			} else {
				if(aequ)
					PushInt(ValueNum(&v1) <= ValueNum(&v2));
				else
					PushInt(ValueNum(&v1) < ValueNum(&v2));
			}
		} else if(tok == TK_EQUALS) {
			if((v1.type == VT_STRING || v2.type == VT_STRING)) {
				if(v1.type != VT_STRING || v2.type != VT_STRING)
					ErrorHandle(lex, "Can't compare string with a non-string value");

				size_t len = v1.str_len > v2.str_len ? v1.str_len : v2.str_len;

				PushInt(strncmp(v1.cur_str, v2.cur_str, len) == 0);
			} else {
				PushInt(ValueNum(&v1) == ValueNum(&v2));
			}
		}
	}
}

void PrsVarDecl()
{
	ExpectIdent("let");

	Expect(TK_IDENT);


	if(IsReserved(lexl->cur_str)) {
		printf("GBuildFile:%d: error: Variable name reserved\n", lex->line);
		exit(1);
	}

	if(VariableGet(lexl->cur_str) != NULL) {
		printf("GBuildFile:%d: error: Variable '%s' already exists\n", lex->line, lexl->cur_str);
		exit(1);
	}

	Variable *var = calloc(1, sizeof(Variable));

	var->name = lexl->cur_str;

	if(AcceptB(TK_EQUALS)) {
		PrsExpression();

		var->value = PopVal();
	}

	VariableNew(var);
}

void PrsBody()
{
	Expect(TK_LEFT_CURLY);

	ScopePush();

	while(!Accept(TK_RIGHT_CURLY)) {
		PrsStatement();
		ClearVal();

		if(Accept(TK_EOF))
			ErrorHandle(lex, "Can't find matching '}'");
	}

	ScopePop();
	Expect(TK_RIGHT_CURLY);
}

void SkipBody()
{
	Expect(TK_LEFT_CURLY);
	int depth = 1;

	while(depth) {
		if(Accept(TK_LEFT_CURLY))
			depth++;
		else if(Accept(TK_RIGHT_CURLY))
			depth--;
		else if(Accept(TK_EOF))
			ErrorHandle(lex, "Can't find matching '}'");

		LexPush(lexp);
	}
}

void PrsIf()
{
	ExpectIdent("if");

	Expect(TK_LEFT_PHAR);

	int reverse = 0;

	if(AcceptB(TK_LOGICAL_NOT))
		reverse = 1;

	PrsExpression();

	Value val = PopVal();

	if(val.type == VT_STRING)
		ErrorHandle(lex, "A string can't be true / false");

	int is_true = val.type == VT_FLOAT ? (val.cur_float != 0) : (val.cur_int != 0);

	if(reverse)
		is_true = !is_true;

	Expect(TK_RIGHT_PHAR);

	if(is_true) {
		PrsBody();
		if(AcceptIdentB("else"))
			SkipBody();
	} else {
		SkipBody();

		if(AcceptIdentB("else")) {
			PrsBody();
		}
	}
}

void ExecuteForEach(char *target_ext, char *cur_dir, LexState *state)
{
	DIR *dir = opendir(cur_dir);
	if(dir == NULL) return;

	struct dirent *ent = readdir(dir);
	while(ent != NULL) {
		if(ent->d_type == DT_REG) {
			size_t len = strlen(ent->d_name);
			char *ext  = calloc(len, 1);

			size_t i   = 0;
			int dot = 0;
			for(size_t j = 0; j < len; j++) {
				if(dot)
					ext[i++] = ent->d_name[j];

				if(ent->d_name[j] == '.') dot = 1;
			}

			if(strcmp(ext, target_ext) == 0) {
				Variable *var = VariableGet("file");

				if(var == NULL) {
					var = calloc(1, sizeof(Variable));
					var->name          = "file";
					var->value.type    = VT_STRING;
					var->value.cur_str = ent->d_name;
					var->value.str_len = len;

					VariableNew(var);
				} else {
					var->value.type    = VT_STRING;
					var->value.cur_str = ent->d_name;
					var->value.str_len = len;
				}

				var = VariableGet("dir");

				if(var == NULL) {
					var = calloc(1, sizeof(Variable));
					var->name          = "dir";
					var->value.type    = VT_STRING;
					var->value.cur_str = cur_dir;
					var->value.str_len = strlen(cur_dir);

					VariableNew(var);
				} else {
					var->value.type    = VT_STRING;
					var->value.cur_str = cur_dir;
					var->value.str_len = strlen(cur_dir);
				}


				lex = LexStateNew();
				memcpy(lex, state, sizeof(LexState));
				PrsBody();
			}
		}

		if(ent->d_type == DT_DIR) {
			if(ent->d_name[0] == '.')
				goto next;

			StringBuilder *builder = StringBuilderNew();

			StringBuilderAppend(builder, "%s/%s", cur_dir, ent->d_name);

			char *dir_name = StringBuild(builder);

			StringBuilderDelete(builder);
			ExecuteForEach(target_ext, dir_name, state);
		}

next:;
		ent = readdir(dir);
	}
}

void PrsBuiltin()
{
	Expect(TK_SQUARE);

	Expect(TK_IDENT);

	lex = lexl;
	LexStateDelete(lex->next);

	if(AcceptIdentB("exit")) {
		PrsExpression();

		Value val = PopVal();

		if(val.type != VT_INT)
			ErrorHandle(lex, "#exit expects integer value");

		if(val.cur_int < 0)
			val.cur_int = -val.cur_int;

		exit(val.cur_int % 256);
	} else if(AcceptIdentB("foreach")) {
		if(VariableGet("file") != NULL)
			ErrorHandle(lex, "The variable 'file' is used by #foreach");
		if(VariableGet("dir") != NULL)
			ErrorHandle(lex, "The variable 'dir' is used by #foreach");

		Expect(TK_LEFT_PHAR);
		Expect(TK_STRING);

		char *ext = lexl->cur_str;

		Expect(TK_RIGHT_PHAR);

		LexState *saved = lex;

		ScopePush();
		ExecuteForEach(ext, ".", saved);
		ScopePop();

		lex = saved;
		SkipBody();
		return;
	}

	ErrorHandle(lex, "Unknown builtin");
}

void PrsStatement()
{
	if(AcceptIdent("let")) {
		PrsVarDecl();
		Expect(TK_SEMICOLON);
	} else if(AcceptIdent("if")) {
		PrsIf();
 	} else if(Accept(TK_LEFT_CURLY)) {
		PrsBody();
	} else if(Accept(TK_SQUARE)) {
		PrsBuiltin();
	} else {
		PrsExpression();
		Expect(TK_SEMICOLON);
	}
}

void Parse()
{
	while(!Accept(TK_EOF)) {
		PrsStatement();
		ClearVal();
	}
}

int main(int argc, char **argv)
{
	const char *file_name = "GBuildFile";

	if(argc > 1) {
		const char *arg = argv[argc-1];

		if(strlen(arg) > 2) {
			if(memcmp(arg, "f:", 2) == 0)
				file_name = &arg[2];
		}
	}

	File *f = FileRead(file_name);

	if(f->data == NULL || f->size == 0) {
		printf("gbuild: fatal error: Can't read %s\n", file_name);
		return 1;
	}

	char *fbuf = calloc(f->size + 1, 1);
	memcpy(fbuf, f->data, f->size);
	fbuf[f->size] = '\0';


	LexState *l = LexStateNew();
	lexp = &l;

	lex->source  = fbuf;
	lex->skip_ws = 1;
	lex->buf     = calloc(f->size, 1);
	lex->error   = ErrorHandler;

	ScopePush();

	for(int i = 0; i < argc; i++) {
		Variable *var = calloc(1, sizeof(Variable));

		StringBuilder *builder = StringBuilderNew();

		StringBuilderAppend(builder, "arg%d", i);

		var->name = StringBuild(builder);

		StringBuilderDelete(builder);

		var->value.type    = VT_STRING;
		var->value.cur_str = argv[i];
		var->value.str_len = strlen(argv[i]);

		VariableNew(var);
	}

	Variable *var = calloc(1, sizeof(Variable));

	var->name          = strdup("argc");
	var->value.type    = VT_INT;
	var->value.cur_int = argc;

	VariableNew(var);

	Parse();
	ScopePop();
}
