// SPDX-License-Identifier: GPL-2.0

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>

#include "expr.h"
#include "cf_utils.h"
#include "lkc_proto.h"
#include "list_types.h"
#include "list.h"
#include "lkc.h"
#include "cf_defs.h"
#include "picosat_functions.h"
#include "configfix.h"
#include "xalloc.h"
#include "cf_fixgen.h"

#define fatal(...)                            \
	do {                                  \
		fprintf(stderr, __VA_ARGS__); \
		exit(EXIT_FAILURE);           \
	} while (0)

static char *conf_filename;
static const char *kconfig_name;
static struct sdv_list *conflict;
static struct sfl_list *fixes;
static volatile sig_atomic_t interrupted;
static volatile sig_atomic_t running_cf;

struct string_list {
	struct list_head list;
};

struct string_node {
	const char *elem;
	struct list_head node;
};

static const char *symbol_value_to_str(struct symbol *sym)
{
	if (sym_is_boolean(sym))
		return tristate_get_char(sym->curr.tri);
	return sym->curr.val;
}

/**
 * Returned string has static life-time or that of fix->nb_val
 */
static const char *symbol_fix_to_str(struct symbol_fix *fix)
{
	switch (fix->type) {
	case SF_BOOLEAN:
		return tristate_get_char(fix->tri);
	case SF_NONBOOLEAN:
		return str_get(&fix->nb_val);
	default:
		assert(false);
	}
}

static struct gstr table_str(struct string_list **columns,
			     size_t num_columns, bool vert_separator)
{
	struct gstr ret = str_new();
	unsigned int *max_lens;
	size_t num_rows;
	const char **entries;

	if (num_columns == 0)
		return ret;
	// max_lens[j] = max length of an entry in column j
	max_lens = xcalloc(num_columns, sizeof(*max_lens));
	num_rows = list_count_nodes(&columns[0]->list);
	// entries[i * num_columns + j] = row i, column j
	entries = xmalloc(num_columns * num_rows * sizeof(*entries));
	for (size_t col = 0; col < num_columns; ++col) {
		struct string_node *entry;
		size_t row = 0;

		CF_LIST_FOR_EACH(entry, columns[col], string) {
			size_t len = strlen(entry->elem);

			if (len > max_lens[col])
				max_lens[col] = len;
			entries[row * num_columns + col] = entry->elem;
			++row;
		}
	}

	for (size_t row = 0; row < num_rows; ++row) {
		if (row > 0)
			str_append(&ret, "\n");
		for (size_t col = 0; col < num_columns; ++col) {
			const char *entry = entries[row * num_columns + col];
			size_t len = strlen(entry);

			if (col > 0)
				str_append(&ret, "|");
			str_append(&ret, " ");
			str_append(&ret, entry);
			str_append(&ret, " ");
			for (size_t i = len; i < max_lens[col]; ++i)
				str_append(&ret, " ");
		}
		if (vert_separator && row == 0) {
			str_append(&ret, "\n");
			for (size_t col = 0; col < num_columns; ++col) {
				if (col > 0)
					str_append(&ret, "+");
				for (size_t i = 0; i < max_lens[col] + 2; ++i)
					str_append(&ret, "-");
			}
		}
	}

	free(max_lens);
	free(entries);
	return ret;
}

static void usage(void)
{
	const char *msg = "\
  Usage:\n\
      ./cfixconf [<Kconfig>]\n\
      where <Kconfig> is the root file of the Kconfig model. If not specified,\n\
      <Kconfig> is \"Kconfig\".\n\
\n\
";
	fprintf(stderr, "%s", msg);
}

/**
 * Allocates copy of str where each upper case letter is replaced by its upper
 * case equivalent.
 */
static char *to_upper(const char *str)
{
	const size_t len = strlen(str);
	char *cpy = xmalloc(len + 1);

	for (size_t i = 0; i < len; ++i)
		cpy[i] = (char) toupper(str[i]);
	cpy[len] = '\0';
	return cpy;
}

static void add_conflict_symbol(struct symbol *sym, tristate val)
{
	struct symbol_dvalue *conflict_entry = xmalloc(sizeof(*conflict_entry));
	struct sdv_node *entry, *entry2;

	conflict_entry->type = SDV_BOOLEAN;
	conflict_entry->sym = sym;
	conflict_entry->tri = val;
	list_for_each_entry_safe(entry, entry2, &conflict->list, node) {
		if (entry->elem->sym != sym)
			continue;

		if (entry->elem->tri != val)
			printf("Overwriting previous symbol value \"%s\"\n",
			       tristate_get_char(entry->elem->tri));
		list_del(&entry->node);
		free(entry);
	}
	CF_PUSH_BACK(conflict, conflict_entry, sdv);
	sym_calc_value(sym);
	printf("Added conflict symbol %s: %s -> %s\n", sym->name,
	       tristate_get_char(sym->curr.tri), tristate_get_char(val));
}

static void remove_conflict_symbol(struct symbol *sym)
{
	struct sdv_node *entry, *entry2;
	bool deleted = false;

	list_for_each_entry_safe(entry, entry2, &conflict->list, node) {
		if (entry->elem->sym == sym) {
			list_del(&entry->node);
			printf("Deleted conflict symbol %s\n", sym->name);
			free(entry);
			deleted = true;
		}
	}
	if (!deleted)
		printf("Symbol not in conflict\n");
}

/*
 * Parses an add command and sets *sym and *val to the parsed values.
 * Returns whether the line could be parsed.
 */
static bool parse_add(struct string_list *tokens, struct symbol **sym,
		      tristate *val)
{
	struct string_node *entry;
	const char *const err_msg = "%s, expected: add <symbol> <value>\n";
	const char *sym_name = NULL, *val_name = NULL;
	char *sym_name_upper;
	int i = 0;

	CF_LIST_FOR_EACH(entry, tokens, string) {
		switch (i) {
		case 0:
			break;
		case 1:
			sym_name = entry->elem;
			break;
		case 2:
			val_name = entry->elem;
			break;
		default:
			printf(err_msg, "Too many arguments");
			return false;
		}
		++i;
	}
	if (!sym_name || !val_name) {
		printf(err_msg, "Too few arguments");
		return false;
	}
	sym_name_upper = to_upper(sym_name);
	*sym = sym_find(sym_name_upper);
	if (!*sym) {
		printf("No such symbol \"%s\"\n", sym_name_upper);
		free(sym_name_upper);
		return false;
	}
	free(sym_name_upper);
	if (sym_is_nonboolean(*sym)) {
		printf("Only symbols of type tristate and bool are supported; symbol %s has type %s\n",
		       (*sym)->name, sym_type_name((*sym)->type));
		return false;
	}
	if (!strcasecmp(val_name, "yes") | !strcasecmp(val_name, "y")) {
		*val = yes;
	} else if (!strcasecmp(val_name, "mod") | !strcasecmp(val_name, "m")) {
		if ((*sym)->type == S_BOOLEAN) {
			printf("Cannot assign mod to symbol of type bool\n");
			return false;
		}
		*val = mod;
	} else if (!strcasecmp(val_name, "no") | !strcasecmp(val_name, "n")) {
		*val = no;
	} else {
		printf("Invalid value \"%s\", expected \"yes\", \"mod\" or \"no\"\n",
		       val_name);
		return false;
	}
	return true;
}

/*
 * tokens must not be empty
 */
static void handle_add(struct string_list *tokens)
{
	struct symbol *sym;
	tristate val;
	bool parse_succ;

	parse_succ = parse_add(tokens, &sym, &val);
	if (!parse_succ)
		return;
	add_conflict_symbol(sym, val);
}

/*
 * Parses a rm command and sets *sym to the parsed value.
 * Returns whether the line could be parsed.
 */
static bool parse_rm(struct string_list *tokens, struct symbol **sym)
{
	struct string_node *entry;
	const char *const err_msg = "%s, expected: rm <symbol>\n";
	const char *sym_name = NULL;
	char *sym_name_upper;
	int i = 0;

	CF_LIST_FOR_EACH(entry, tokens, string) {
		switch (i) {
		case 0:
			break;
		case 1:
			sym_name = entry->elem;
			break;
		default:
			printf(err_msg, "Too many arguments");
			return false;
		}
		++i;
	}
	if (!sym_name) {
		printf(err_msg, "Too few arguments");
		return false;
	}
	sym_name_upper = to_upper(sym_name);
	*sym = sym_find(sym_name_upper);
	if (!*sym) {
		printf("No such symbol \"%s\"\n", sym_name_upper);
		free(sym_name_upper);
		return false;
	}
	free(sym_name_upper);
	return true;
}

static void handle_rm(struct string_list *tokens)
{
	struct symbol *sym;
	bool parse_succ;

	parse_succ = parse_rm(tokens, &sym);
	if (!parse_succ)
		return;
	remove_conflict_symbol(sym);
}

static void handle_clear(struct string_list *tokens)
{
	if (list_count_nodes(&tokens->list) != 1) {
		printf("Too many arguments, expected: clear\n");
		return;
	}
	if (list_empty(&conflict->list)) {
		printf("Conflict already empty\n");
		return;
	}
	CF_LIST_FREE(conflict, sdv);
	conflict = CF_LIST_INIT(sdv);
	printf("Cleared conflict\n");
}

static void handle_show(struct string_list *tokens)
{
	struct sdv_node *entry;
	int conflict_len = 0;
	struct string_list **columns;
	struct gstr out;

	if (list_count_nodes(&tokens->list) != 1) {
		printf("Too many arguments, expected: show\n");
		return;
	}
	columns = xcalloc(3, sizeof(*columns));
	columns[0] = CF_LIST_INIT(string);
	CF_PUSH_BACK(columns[0], "Symbol", string);
	columns[1] = CF_LIST_INIT(string);
	CF_PUSH_BACK(columns[1], "Current", string);
	columns[2] = CF_LIST_INIT(string);
	CF_PUSH_BACK(columns[2], "Target", string);
	CF_LIST_FOR_EACH(entry, conflict, sdv) {
		const struct symbol_dvalue *sdv = entry->elem;

		sym_calc_value(sdv->sym);
		CF_PUSH_BACK(columns[0], sdv->sym->name, string);
		CF_PUSH_BACK(columns[1], tristate_get_char(sdv->sym->curr.tri), string);
		CF_PUSH_BACK(columns[2], tristate_get_char(sdv->tri), string);
		++conflict_len;
	}
	out = table_str(columns, 3, true);
	if (conflict_len == 0)
		printf("No symbols in conflict\n");
	else
		printf("%s\n", str_get(&out));
	str_free(&out);
	for (size_t i = 0; i < 3; ++i)
		CF_LIST_FREE(columns[i], string);
	free(columns);
}

static void handle_solve(struct string_list *tokens)
{
	struct sfl_list *new_fixes;
	struct sfl_node *fix;
	struct sdv_node *entry;
	int i = 0;
	bool first, trivial;
	enum fixgen_exit_status fixgen_status;

	if (list_count_nodes(&tokens->list) != 1) {
		printf("Too many arguments, expected: show\n");
		return;
	}
	if (list_empty(&conflict->list)) {
		printf("No symbols in conflict\n");
		return;
	}
	printf("Solving for: ");
	first = true;
	CF_LIST_FOR_EACH(entry, conflict, sdv) {
		if (first)
			first = false;
		else
			printf("; ");
		printf("%s=%s", entry->elem->sym->name,
		       tristate_get_char(entry->elem->tri));
	}
	printf("\n");
	stop_fixgen = false;
	running_cf = true;
	new_fixes = run_satconf_list(conflict, &trivial, &fixgen_status);
	running_cf = false;
	if (interrupted || fixgen_status == CFGEN_STATUS_CANCELED) {
		interrupted = false;
		CF_LIST_FOR_EACH(fix, new_fixes, sfl) {
			CF_LIST_FREE(fix->elem, sfix);
		}
		CF_LIST_FREE(new_fixes, sfl);
		return;
	}

	CF_LIST_FOR_EACH(fix, new_fixes, sfl) {
		struct sfix_node *entry;
		struct string_list **columns;
		struct gstr table;

		if (i > 0)
			printf("\n");
		printf("Fix %d:\n", i + 1);
		columns = xcalloc(3, sizeof(*columns));
		columns[0] = CF_LIST_INIT(string);
		CF_PUSH_BACK(columns[0], "Symbol", string);
		columns[1] = CF_LIST_INIT(string);
		CF_PUSH_BACK(columns[1], "Current", string);
		columns[2] = CF_LIST_INIT(string);
		CF_PUSH_BACK(columns[2], "New", string);
		CF_LIST_FOR_EACH(entry, fix->elem, sfix) {
			struct symbol *sym = entry->elem->sym;

			sym_calc_value(sym);
			CF_PUSH_BACK(columns[0], entry->elem->sym->name,
				     string);
			CF_PUSH_BACK(columns[1], symbol_value_to_str(sym),
				     string);
			CF_PUSH_BACK(columns[2], symbol_fix_to_str(entry->elem),
				     string);
		}
		table = table_str(columns, 3, true);
		printf("%s\n", str_get(&table));

		for (int i = 0; i < 3; ++i)
			CF_LIST_FREE(columns[i], string);
		free(columns);
		str_free(&table);
		++i;
	}
	if (i == 0)
		printf("No fixes found\n");
	if (trivial)
		printf("(All changes can already be made manually)\n");
	if (fixgen_status == CFGEN_STATUS_TIMEOUT)
		printf("(Fix generation stopped due to timeout)\n");
	if (fixes) {
		CF_LIST_FOR_EACH(fix, fixes, sfl)
		{
			CF_LIST_FREE(fix->elem, sfix);
		}
		CF_LIST_FREE(fixes, sfl);
	}
	fixes = new_fixes;
}

static bool parse_apply(struct string_list *tokens, long *fix_no)
{
	struct string_node *entry;
	const char *const err_msg = "%s, expected: apply <fix-no>\n";
	const char *fix_no_str = NULL;
	char *endptr;
	int i = 0;

	CF_LIST_FOR_EACH(entry, tokens, string) {
		switch (i) {
		case 0:
			break;
		case 1:
			fix_no_str = entry->elem;
			break;
		default:
			printf(err_msg, "Too many arguments");
			return false;
		}
		++i;
	}
	if (!fix_no_str) {
		printf(err_msg, "Too few arguments");
		return false;
	}
	assert(fix_no_str[0] != '\0');
	errno = 0;
	*fix_no = strtol(fix_no_str, &endptr, 10);
	if (errno == ERANGE) {
		printf("Number \"%s\" out of range\n", fix_no_str);
		return false;
	}
	if (*endptr != '\0') {
		printf("Invalid number \"%s\"\n", fix_no_str);
		return false;
	}
	if (*fix_no <= 0) {
		printf("The fix number must be postive\n");
		return false;
	}
	return true;
}

static void handle_apply(struct string_list *tokens)
{
	long fix_no;
	bool parse_succ;
	int num_fixes;
	int i;
	struct sfl_node *fix;
	struct sfix_node *entry;
	struct string_list **columns;
	struct gstr table;

	parse_succ = parse_apply(tokens, &fix_no);
	if (!parse_succ)
		return;
	if (!fixes) {
		printf("No fixes have been computed\n");
		return;
	}
	num_fixes = list_count_nodes(&fixes->list);
	if (fix_no > num_fixes) {
		printf("Invalid fix number %ld (max.: %d)\n", fix_no, num_fixes);
		return;
	}
	i = 1;
	CF_LIST_FOR_EACH(fix, fixes, sfl) {
		if (i == fix_no)
			break;
		++i;
	}
	apply_fix(fix->elem);
	columns = xcalloc(2, sizeof(*columns));
	columns[0] = CF_LIST_INIT(string);
	CF_PUSH_BACK(columns[0], xstrdup("Symbol"), string);
	columns[1] = CF_LIST_INIT(string);
	CF_PUSH_BACK(columns[1], xstrdup("New"), string);
	CF_LIST_FOR_EACH(entry, fix->elem, sfix) {
		struct symbol *sym = entry->elem->sym;
		const char *const FAILURE_STR = " (failed)";
		struct gstr s;

		CF_PUSH_BACK(columns[0], xstrdup(sym->name), string);

		s = str_new();
		sym_calc_value(sym);
		str_append(&s, symbol_value_to_str(sym));
		if (sym_is_boolean(sym)) {
			if (sym->curr.tri != entry->elem->tri)
				str_append(&s, FAILURE_STR);
		} else {
			if (strcmp(sym->curr.val,
				   str_get(&entry->elem->nb_val)))
				str_append(&s, FAILURE_STR);
		}
		CF_PUSH_BACK(columns[1], str_get(&s), string);
	}
	table = table_str(columns, 2, true);
	printf("Updated values:\n%s\n", str_get(&table));

	for (int i = 0; i < 2; ++i) {
		struct string_node *str_entry;

		CF_LIST_FOR_EACH(str_entry, columns[i], string)
			free((char *) str_entry->elem);
		CF_LIST_FREE(columns[i], string);
	}
	free(columns);
	str_free(&table);
}

static void handle_open(struct string_list *tokens)
{
	struct string_node *entry;
	const char *const err_msg = "%s, expected: open [config-file]\n";
	int i = 0;
	bool succ, reload = true;
	const char *verb;

	CF_LIST_FOR_EACH(entry, tokens, string)
	{
		switch (i) {
		case 0:
			break;
		case 1:
			reload = conf_filename &&
				 !strcmp(entry->elem, conf_filename);
			free(conf_filename);
			conf_filename = xstrdup(entry->elem);
			break;
		default:
			printf(err_msg, "Too many arguments");
			return;
		}
		++i;
	}

	succ = conf_read(conf_filename) == 0;
	verb = reload ? "Reloaded" : "Opened";
	if (succ) {
		if (conf_filename)
			printf("%s configuration file (%s)\n", verb, conf_filename);
		else
			printf("%s configuration file\n", verb);
	} else
		printf("Could not open configuration file\n");
}

static void handle_write(struct string_list *tokens)
{
	struct string_node *entry;
	const char *const err_msg = "%s, expected: write [config-file]\n";
	const char *write_config_file = NULL;
	int i = 0;
	bool succ;

	CF_LIST_FOR_EACH(entry, tokens, string) {
		switch (i) {
		case 0:
			break;
		case 1:
			write_config_file = entry->elem;
			break;
		default:
			printf(err_msg, "Too many arguments");
			return;
		}
		++i;
	}

	if (write_config_file == NULL)
		write_config_file = conf_filename;
	succ = conf_write(write_config_file) == 0;
	if (succ) {
		if (!conf_filename)
			conf_filename = xstrdup(conf_get_configname());
	} else
		printf("Could not write configuration\n");
}

static void handle_help(void)
{
	const char *text = "\
Commands:\n\
    add <symbol> <value>  Add symbol with value to conflict.\n\
    show                  List all symbols in conflict.\n\
    rm <symbol>           Remove symbol from conflict.\n\
    clear                 Clear conflict.\n\
    solve                 Compute and propose fixes for conflict.\n\
    apply <fix-no>        Apply a previously computed fix.\n\
    open [config-file]    Open configuration file. If none given, reloads\n\
                          the currently opened configuration file.\n\
    write [config-file]   Write configuration to a file. If none given, writes\n\
                          to currently opened configuration file.\n\
    help                  Show this help text.\n\
";
	printf("%s", text);
}

static void handle_line(struct string_list *tokens)
{
	if (list_empty(&tokens->list))
		return;

	const char *cmd =
		list_first_entry(&tokens->list, struct string_node, node)->elem;
	if (!strcasecmp(cmd, "add"))
		handle_add(tokens);
	else if (!strcasecmp(cmd, "show"))
		handle_show(tokens);
	else if (!strcasecmp(cmd, "help"))
		handle_help();
	else if (!strcasecmp(cmd, "rm"))
		handle_rm(tokens);
	else if (!strcasecmp(cmd, "clear"))
		handle_clear(tokens);
	else if (!strcasecmp(cmd, "solve"))
		handle_solve(tokens);
	else if (!strcasecmp(cmd, "apply"))
		handle_apply(tokens);
	else if (!strcasecmp(cmd, "open"))
		handle_open(tokens);
	else if (!strcasecmp(cmd, "write"))
		handle_write(tokens);
	else
		printf("Unknown command \"%s\", type \"help\" for a list of commands\n",
		       cmd);
}

static struct string_list *tokenize_line(char *in)
{
	char *saveptr;
	char *str = in;
	CF_DEF_LIST(tokens, string);

	while (true) {
		char *token = strtok_r(str, " \t\n", &saveptr);

		str = NULL;
		if (!token)
			break;
		CF_PUSH_BACK(tokens, token, string);
	}

	return tokens;
}

static void read_loop(void)
{
	while (true) {
		struct gstr in = str_new();
		struct string_list *tokens;

		printf(">>> ");
		do {
			int next_char = fgetc(stdin);

			if (next_char == EOF) {
				if (ferror(stdin)) {
					if (interrupted) {
						interrupted = false;
						clearerr(stdin);
						printf("\n");
						break;
					}
					fatal("Error reading stdin\n");
				} else {
					assert(feof(stdin));
					printf("\n");
					return;
				}
			} else if (next_char == '\n') {
				break;
			} else {
				str_append(&in, (char[]){ next_char, '\0' });
			}
		} while (true);
		tokens = tokenize_line(str_get(&in));
		handle_line(tokens);
		CF_LIST_FREE(tokens, string);
		str_free(&in);
	}
}

static void parse_args(int argc, char *argv[])
{
	const char *arg;

	if (argc == 0) {
		kconfig_name = "Kconfig";
		return;
	}
	arg = argv[1];
	if (argc > 2) {
		fprintf(stderr, "Too many arguments\n");
		usage();
		exit(EXIT_FAILURE);
	}

	if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
		usage();
		exit(EXIT_SUCCESS);
	}

	kconfig_name = arg;
}

static void on_int(int signum)
{
	interrupted = true;
	if (running_cf) {
		printf("\nInterrupting...\n");
		interrupt_fix_generation();
	}
}

int main(int argc, char *argv[])
{
	parse_args(argc, argv);
	if (!load_picosat())
		fatal("Could not load PicoSAT\n");
	conf_parse(kconfig_name);
	conf_read(NULL);
	conflict = CF_LIST_INIT(sdv);
	sigaction(SIGINT, (struct sigaction[]){{ .sa_handler = on_int }}, NULL);
	read_loop();
	return EXIT_SUCCESS;
}
