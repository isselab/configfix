#define _GNU_SOURCE
#include <assert.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "satconf.h"

static void print_expr_util(struct expr *e, int prevtoken);

/*
 * print all symbols, just for debugging
 */
void print_all_symbols(void)
{
	unsigned int i;
	struct symbol *sym;
	
	printf("\n");
	
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;
		
		print_symbol(sym);

		if (sym->constraints->arr->len > 0) {
			printf("Constraints:\n");
			print_sym_constraint(sym);
			printf("\n");
		}
	}
}

/*
 * print the symbol
 */
void print_symbol(struct symbol *sym)
{
	printf("Symbol: ");
	struct property *p;

	if (sym_is_boolean(sym)) {
		printf("name %s, type %s, fexpr_y %d", sym->name, sym_type_name(sym->type), sym->fexpr_y->satval);
		if (sym->type == S_TRISTATE)
			printf(", fexpr_m %d", sym->fexpr_m->satval);
	} else {
		printf("name %s, type %s", sym->name, sym_type_name(sym->type));
	}
	printf("\n");

	printf("\t.config: %s\n", sym_get_string_value(sym));
	

	/* print default values */
	for_all_defaults(sym, p)
		print_default(p);

	/* print select statements */
	for_all_properties(sym, p, P_SELECT)
		print_select(p);

	/* print reverse dependencies */
	if (sym->rev_dep.expr) {
		printf("\tselected if ");
		print_expr_util(sym->rev_dep.expr, E_NONE);
		printf("  (reverse dep.)\n");
	}

	/* print imply statements */
	for_all_properties(sym, p, P_IMPLY)
		print_imply(p);

	/* print weak reverse denpencies */
	if (sym->implied.expr) {
		printf("\timplied if ");
		print_expr_util(sym->implied.expr, E_NONE);
		printf("  (weak reverse dep.)\n");
	}

	/* print dependencies */
	if (sym->dir_dep.expr) {
		printf("\tdepends on ");
		print_expr_util(sym->dir_dep.expr, E_NONE);
		printf("\n");
		struct property *prompt = sym_get_prompt(sym);
		if (prompt != NULL) {
// 			printf("\tpromptCond: ");
			print_expr("\tpromptCond:", prompt->visible.expr, E_NONE);
		}
		
	}

	printf("\n");

}

/*
 * print a symbol's name
 */
void print_sym_name(struct symbol *sym)
{
	printf("Symbol: ");
	if (sym_is_choice(sym)) {
		struct property *prompt = sym_get_prompt(sym);
		printf("%s (Choice)", prompt->text);
	} else  {
		printf("%s", sym->name);
	}
	printf("\n");
}


/*
 * print a default value for a property
 */
void print_default(struct property *p)
{
	assert(p->type == P_DEFAULT);
	printf("\tdefault %u", p->expr->left.sym->curr.tri);
	if (p->visible.expr) {
		printf(" if ");
		print_expr_util(p->visible.expr, E_NONE);
	}
	printf("\n");
}

/*
 * print a select statement for a property
 */
void print_select(struct property *p)
{
	assert(p->type == P_SELECT);
	struct expr *e = p->expr;

	printf("\tselect ");
	print_expr_util(e, E_NONE);
// 	print_expr("\tselect", e, E_NONE);
	if (p->visible.expr)
		print_expr(" if", p->visible.expr, E_NONE);
	
	printf("\n");
}

/*
 * print an imply statement for a property
 */
void print_imply(struct property *p)
{
	assert(p->type == P_IMPLY);
	struct expr *e = p->expr;

	printf("\timply ");
	print_expr_util(e, E_NONE);
	if (p->visible.expr) {
		printf(" if ");
		print_expr_util(p->visible.expr, E_NONE);
	}
	printf("\n");
}

/*
 * print an expr
 */
void print_expr_util(struct expr *e, int prevtoken)
{
	if (!e) return;

	switch (e->type) {
	case E_SYMBOL:
		if (sym_get_name(e->left.sym) != NULL)
			printf("%s", sym_get_name(e->left.sym));
		else
			printf("left was null\n");
		break;
	case E_NOT:
		printf("!");
		print_expr_util(e->left.expr, E_NOT);
		break;
	case E_AND:
		if (prevtoken != E_AND && prevtoken != 0)
			printf("(");
		print_expr_util(e->left.expr, E_AND);
		printf(" && ");
		print_expr_util(e->right.expr, E_AND);
		if (prevtoken != E_AND && prevtoken != 0)
			printf(")");
		break;
	case E_OR:
		if (prevtoken != E_OR && prevtoken != 0)
			printf("(");
		print_expr_util(e->left.expr, E_OR);
		printf(" || ");
		print_expr_util(e->right.expr, E_OR);
		if (prevtoken != E_OR && prevtoken != 0)
			printf(")");
		break;
	case E_EQUAL:
	case E_UNEQUAL:
		if (e->left.sym->name)
			printf("%s", e->left.sym->name);
		else
			printf("left was null\n");
		printf("%s", e->type == E_EQUAL ? "=" : "!=");
		printf("%s", e->right.sym->name);
		break;
	case E_LEQ:
	case E_LTH:
		if (e->left.sym->name)
			printf("%s", e->left.sym->name);
		else
			printf("left was null\n");
		printf("%s", e->type == E_LEQ ? "<=" : "<");
		printf("%s", e->right.sym->name);
		break;
	case E_GEQ:
	case E_GTH:
		if (e->left.sym->name)
			printf("%s", e->left.sym->name);
		else
			printf("left was null\n");
		printf("%s", e->type == E_GEQ ? ">=" : ">");
		printf("%s", e->right.sym->name);
		break;
	case E_RANGE:
		printf("[");
		printf("%s", e->left.sym->name);
		printf(" ");
		printf("%s", e->right.sym->name);
		printf("]");
		break;
	default:
		break;
	}
}
void print_expr(char *tag, struct expr *e, int prevtoken)
{
	printf("%s ", tag);
	print_expr_util(e, prevtoken);
	printf("\n");
}


/*
 * print a kexpr
 */
static void print_kexpr_util(struct k_expr *e)
{
	if (!e) return;

	switch (e->type) {
	case KE_SYMBOL:
		printf("%s", sym_get_name(e->sym));
		if (e->tri == mod)
			printf("_m");
		break;
	case KE_AND:
		printf("(");
		print_kexpr_util(e->left);
		printf(" && ");
		print_kexpr_util(e->right);
		printf(")");
		break;
	case KE_OR:
		printf("(");
		print_kexpr_util(e->left);
		printf(" || ");
		print_kexpr_util(e->right);
		printf(")");
		break;
	case KE_NOT:
		printf("!");
		print_kexpr_util(e->left);
		break;
	case KE_EQUAL:
	case KE_UNEQUAL:
		printf("%s", e->eqsym->name);
		printf("%s", e->type == KE_EQUAL ? "=" : "!=");
		printf("%s", e->eqvalue->name);
		break;
	case KE_CONST_FALSE:
		printf("0");
		break;
	case KE_CONST_TRUE:
		printf("1");
		break;
	}
}
void print_kexpr(char *tag, struct k_expr *e)
{
	printf("%s ", tag);
	print_kexpr_util(e);
	printf("\n");
}


/*
 * print a fexpr
 */
static void print_fexpr_util(struct fexpr *e, int parent)
{
	if (!e) return;
	
	switch (e->type) {
	case FE_SYMBOL:
	case FE_CHOICE:
	case FE_SELECT:
	case FE_NONBOOL:
	case FE_TMPSATVAR:
		printf("%s", str_get(&e->name));
		break;
	case FE_AND:
		if (parent != FE_AND && parent != -1)
			printf("(");
		print_fexpr_util(e->left, FE_AND);
		printf(" && ");
		print_fexpr_util(e->right, FE_AND);
		if (parent != FE_AND && parent != -1)
			printf(")");
		break;
	case FE_OR:
		if (parent != FE_OR && parent != -1)
			printf("(");
		print_fexpr_util(e->left, FE_OR);
		printf(" || ");
		print_fexpr_util(e->right, FE_OR);
		if (parent != FE_OR && parent != -1)
			printf(")");
		break;
	case FE_NOT:
		printf("!");
		print_fexpr_util(e->left, FE_NOT);
		break;
	case FE_EQUALS:
		printf("%s=%s", e->sym->name, e->eqvalue->name);
		break;
	case FE_FALSE:
		printf("0");
		break;
	case FE_TRUE:
		printf("1");
		break;
	}
}
void print_fexpr(char *tag, struct fexpr *e, int parent)
{
	printf("%s ", tag);
	print_fexpr_util(e, parent);
	printf("\n");
}


/*
 * print some debug info about the tree structure of k_expr
 */
void debug_print_kexpr(struct k_expr *e)
{
	if (!e) return;
	
	#if DEBUG_KEXPR
		printf("e-type: %s", kexpr_type[e->type]);
		if (e->parent)
			printf(", parent %s", kexpr_type[e->parent->type]);
		printf("\n");
		switch (e->type) {
		case KE_SYMBOL:
			printf("name %s\n", e->sym->name);
			break;
		case KE_AND:
		case KE_OR:
			printf("left child: %s\n", kexpr_type[e->left->type]);
			printf("right child: %s\n", kexpr_type[e->right->type]);
			debug_print_kexpr(e->left);
			debug_print_kexpr(e->right);
			break;
		case KE_NOT:
		default:
			printf("child: %s\n", kexpr_type[e->left->type]);
			debug_print_kexpr(e->left);
			break;
		}
		
	#endif
}

/*
 * write a kexpr into a string
 */
void kexpr_as_char(struct k_expr *e, struct gstr *s)
{
	if (!e) return;
	
	switch (e->type) {
	case KE_SYMBOL:
		str_append(s, e->sym->name);
		return;
	case KE_AND:
		str_append(s, "(");
		kexpr_as_char(e->left, s);
		str_append(s, " && ");
		kexpr_as_char(e->right, s);
		str_append(s, ")");
		return;
	case KE_OR:
		str_append(s, "(");
		kexpr_as_char(e->left, s);
		str_append(s, " || ");
		kexpr_as_char(e->right, s);
		str_append(s, ")");
		return;
	case KE_NOT:
		str_append(s, "!");
		kexpr_as_char(e->left, s);
		return;
	case KE_EQUAL:
	case KE_UNEQUAL:
		str_append(s, e->eqsym->name);
		str_append(s, e->type == KE_EQUAL ? "=" : "!=");
		str_append(s, e->eqvalue->name);
		break;
	case KE_CONST_FALSE:
		str_append(s, "0");
		break;
	case KE_CONST_TRUE:
		str_append(s, "1");
		break;
	}
}

/*
 * write an fexpr into a string (format needed for testing)
 */
void fexpr_as_char(struct fexpr *e, struct gstr *s, int parent)
{
	if (!e) return;
	
	switch (e->type) {
	case FE_SYMBOL:
	case FE_CHOICE:
	case FE_SELECT:
	case FE_NONBOOL:
		str_append(s, "definedEx(");
		str_append(s, str_get(&e->name));
		str_append(s, ")");
		return;
	case FE_AND:
		/* need this hack for the FeatureExpr parser */
		if (parent != FE_AND)
			str_append(s, "(");
		fexpr_as_char(e->left, s, FE_AND);
		str_append(s, " && ");
		fexpr_as_char(e->right, s, FE_AND);
		if (parent != FE_AND)
			str_append(s, ")");
		return;
	case FE_OR:
		if (parent != FE_OR)
			str_append(s, "(");
		fexpr_as_char(e->left, s, FE_OR);
		str_append(s, " || ");
		fexpr_as_char(e->right, s, FE_OR);
		if (parent != FE_OR)
			str_append(s, ")");
		return;
	case FE_NOT:
		str_append(s, "!");
		fexpr_as_char(e->left, s, FE_NOT);
		return;
	case FE_FALSE:
		str_append(s, "0");
		return;
	case FE_TRUE:
		str_append(s, "1");
		return;
	default:
		return;
	}
}

/*
 * write an fexpr into a string
 */
void fexpr_as_char_short(struct fexpr *e, struct gstr *s, int parent)
{
	if (!e) return;
	
	switch (e->type) {
	case FE_SYMBOL:
	case FE_NONBOOL:
		str_append(s, str_get(&e->name));
		return;
	case FE_AND:
		/* need this hack for the FeatureExpr parser */
		if (parent != FE_AND)
			str_append(s, "(");
		fexpr_as_char_short(e->left, s, FE_AND);
		str_append(s, " && ");
		fexpr_as_char_short(e->right, s, FE_AND);
		if (parent != FE_AND)
			str_append(s, ")");
		return;
	case FE_OR:
		if (parent != FE_OR)
			str_append(s, "(");
		fexpr_as_char_short(e->left, s, FE_OR);
		str_append(s, " || ");
		fexpr_as_char_short(e->right, s, FE_OR);
		if (parent != FE_OR)
			str_append(s, ")");
		return;
	case FE_NOT:
		str_append(s, "!");
		fexpr_as_char_short(e->left, s, FE_NOT);
		return;
	case FE_FALSE:
		str_append(s, "0");
		return;
	case FE_TRUE:
		str_append(s, "1");
		return;
	default:
		return;
	}
}

/*
 * print all constraints for a symbol
 */
void print_sym_constraint(struct symbol* sym)
{
	struct fexpr *e;
	int i;
	for (i = 0; i < sym->constraints->arr->len; i++) {
		e = g_array_index(sym->constraints->arr, struct fexpr *, i);
		print_fexpr_util(e, -1);
		
		#if PRINT_ALL_CNF
			printf("\nCNF: ");
			convert_fexpr_to_cnf(e);
			print_fexpr(e, -1);
		#endif
		printf("\n");
	}
}

/*
 * print the satmap
 */
void print_satmap(gpointer key, gpointer value, gpointer userData)
{
	struct fexpr *e = (struct fexpr *) value;
	printf( "%d => %s\n", *((int *) key), str_get(&e->name));
}

/*
 * print a default map
 */
void print_default_map(GArray *map)
{
	struct default_map *entry;
	unsigned int i;
	
	for (i = 0; i < map->len; i++) {
		entry = g_array_index(map, struct default_map *, i);
		struct gstr s = str_new();
		str_append(&s, "\t");
		str_append(&s, str_get(&entry->val->name));
		str_append(&s, " ->");
		print_fexpr(strdup(str_get(&s)), entry->e, -1);
	}
}


/*
 * print all variables from the satmap in DIMACS-format
 */
static void print_satmap_dimacs(gpointer key, gpointer value, gpointer fd)
{
	fprintf((FILE *) fd, "c %d %s\n", *((int *) key), (char *) value);
}


/*
 * write all CNF-clauses into a file in DIMACS-format
 */
void write_cnf_to_file(GArray *cnf_clauses, int sat_variable_nr, int nr_of_clauses)
{
	perror("Fix function write_cnf_to_file");
// 	FILE *fd = fopen(OUTFILE_DIMACS, "w");
// 
// 	/* symbols, constants & tmp vars */
// 	g_hash_table_foreach(satmap, print_satmap_dimacs, fd);
// 
// 	/* stats */
// 	fprintf(fd, "p cnf %d %d\n", sat_variable_nr - 1, nr_of_clauses);
// 	
// 	/* clauses */
// 	int i, j;
// 	struct cnf_clause *cl;
// 	struct cnf_literal *lit;
// 	for (i = 0; i < cnf_clauses->len; i++) {
// 		cl = g_array_index(cnf_clauses, struct cnf_clause *, i);
// 		for (j = 0; j < cl->lits->len; j++) {
// 			lit = g_array_index(cl->lits, struct cnf_literal *, j);
// 			fprintf(fd, "%d ", lit->val);
// 			if (j == cl->lits->len - 1)
// 				fprintf(fd, "0\n");
// 		}
// 	}
// 
// 	fclose(fd);
}

/*
 * write all constraints into a file for testing purposes
 */
void write_constraints_to_file(void)
{
	printf("Writing constraints to file...");
	
	FILE *fd = fopen(OUTFILE_FEXPR, "w");
	unsigned int i, j;
	struct symbol *sym;
	struct fexpr *e;

	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;
		
		for (j = 0; j < sym->constraints->arr->len; j++) {
			e = g_array_index(sym->constraints->arr, struct fexpr *, j);
			struct gstr s = str_new();
			fexpr_as_char(e, &s, -1);
			fprintf(fd, "%s\n", str_get(&s));
		}
	}
	fclose(fd);
	
	printf("done.\n");
}
