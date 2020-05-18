#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <locale.h>
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include "satconf.h"

static void sym_set_changed(struct symbol *sym)
{
	struct property *prop;

	sym->flags |= SYMBOL_CHANGED;
	for (prop = sym->prop; prop; prop = prop->next) {
		if (prop->menu)
			prop->menu->flags |= MENU_CHANGED;
	}
}

bool sym_set_tristate_value_mod(struct symbol *sym, tristate val)
{
	tristate oldval = sym_get_tristate_value(sym);

	// TODO decide, what to do here
	// maybe do some checks
// 	if (oldval != val && !sym_tristate_within_range_mod(sym, val))
// 		return false;

	if (!(sym->flags & SYMBOL_DEF_USER)) {
		sym->flags |= SYMBOL_DEF_USER;
		sym_set_changed(sym);
	}
	/*
	 * setting a choice value also resets the new flag of the choice
	 * symbol and all other choice values.
	 */
	if (sym_is_choice_value(sym) && val == yes) {
		struct symbol *cs = prop_get_symbol(sym_get_choice_prop(sym));
		struct property *prop;
		struct expr *e;

		cs->def[S_DEF_USER].val = sym;
		cs->flags |= SYMBOL_DEF_USER;
		prop = sym_get_choice_prop(cs);
		for (e = prop->expr; e; e = e->left.expr) {
			if (e->right.sym->visible != no)
				e->right.sym->flags |= SYMBOL_DEF_USER;
		}
	}

	sym->def[S_DEF_USER].tri = val;
	if (oldval != val)
		sym_clear_all_valid();

	return true;
}
