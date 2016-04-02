/*
	Copyright (C) 2016 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "snis_nl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "arraysize.h"
#include "string-utils.h"

static const char * const part_of_speech[] = {
	"unknown",
	"noun",
	"verb",
	"article",
	"preposition",
	"separator",
	"adjective",
	"adverb",
	"number",
	"name",
	"pronoun",
	"externalnoun",
};

#define MAX_SYNONYMS 100
static int nsynonyms = 0;
static struct synonym_entry {
	char *syn, *canonical;
} synonym[MAX_SYNONYMS + 1] = { { 0 } }; 

#define MAX_DICTIONARY_ENTRIES 1000
static int ndictionary_entries = 0;
static struct dictionary_entry {
	char *word;
	char *canonical_word;
	int p_o_s; /* part of speech */
	__extension__ union {
		struct snis_nl_noun_data noun;
		struct snis_nl_verb_data verb;
		struct snis_nl_article_data article;
		struct snis_nl_preposition_data preposition;
		struct snis_nl_separator_data separator;
		struct snis_nl_adjective_data adjective;
		struct snis_nl_adverb_data adverb;
		struct snis_nl_number_data number;
		struct snis_nl_pronoun_data pronoun;
		struct snis_nl_external_noun_data external_noun;
	};
} dictionary[MAX_DICTIONARY_ENTRIES + 1] = { { 0 } };

static snis_nl_external_noun_lookup external_lookup = NULL;

#define MAX_MEANINGS 10
struct nl_token {
	char *word;
	int pos[MAX_MEANINGS];
	int meaning[MAX_MEANINGS];
	int npos; /* number of possible parts of speech */
	struct snis_nl_number_data number;
	struct snis_nl_external_noun_data external_noun;
};

static char *fixup_punctuation(char *s)
{
	char *src, *dest;
	char *n = malloc(3 * strlen(s) + 1);

	dest = n;
	for (src = s; *src; src++) {
		char *x = index(",:;.!?", *src);
		if (!x) {
			*dest = *src;
			dest++;
			continue;
		}
		*dest = ' '; dest++;
		*dest = *src; dest++;
		*dest = ' '; dest++;
	}
	*dest = '\0';
	return n;
}

static struct nl_token **tokenize(char *txt, int *nwords)
{
	char *s = fixup_punctuation(txt);
	char *t;
	char *w[100];
	int i, count;
	struct nl_token **word = NULL;

	count = 0;
	do {
		t = strtok(s, " ");
		s = NULL;
		if (!t)
			break;
		w[count] = strdup(t);
		count++;
		if (count >= 100)
			break;
	} while (1);
	word = malloc(sizeof(*word) * count);
	for (i = 0; i < count; i++) {
		word[i] = malloc(sizeof(*word[0]));
		memset(word[i], 0, sizeof(*word[0]));
		word[i]->word = w[i];
		word[i]->npos = 0;
	}
	*nwords = count;
	if (s)
		free(s);
	return word;
}

static void free_tokens(struct nl_token *word[], int nwords)
{
	int i;

	for (i = 0; i < nwords; i++)
		free(word[i]);
}

static void classify_token(struct nl_token *t)
{
	int i, j;
	float x, rc;
	char c;
	uint32_t handle;

	if (t->npos >= MAX_MEANINGS)
		return;

	/* Find all the dictionary meanings */
	for (i = 0; dictionary[i].word != NULL; i++) {
		if (strcmp(dictionary[i].word, t->word) != 0)
			continue;
		t->pos[t->npos] = dictionary[i].p_o_s;
		t->meaning[t->npos] = i;
		t->npos++;
		if (t->npos >= MAX_MEANINGS)
			break;
	}
	if (t->npos >= MAX_MEANINGS)
		return;

	/* Find all synonym meanings */
	for (i = 0; synonym[i].syn != NULL; i++) {
		if (strcmp(synonym[i].syn, t->word) != 0)
			continue;
		for (j = 0; dictionary[j].word != NULL; j++) {
			if (strcmp(dictionary[j].word, synonym[i].canonical) != 0)
				continue;
			t->pos[t->npos] = dictionary[j].p_o_s;
			t->meaning[t->npos] = j;
			t->npos++;
			if (t->npos >= MAX_MEANINGS)
				break;
		}
	}

	/* See if it's a percent */
	rc = sscanf(t->word, "%f%c", &x, &c);
	if (rc == 2 && c == '%') {
		t->pos[t->npos] = POS_NUMBER;
		t->meaning[t->npos] = -1;
		t->number.value = x / 100.0;
		t->npos++;
		if (t->npos >= MAX_MEANINGS)
			return;
	} else  {
		/* See if it's a number */
		rc = sscanf(t->word, "%f", &x);
		if (rc == 1) {
			t->pos[t->npos] = POS_NUMBER;
			t->meaning[t->npos] = -1;
			t->number.value = x;
			t->npos++;
		}
		if (t->npos >= MAX_MEANINGS)
			return;
	}

	if (external_lookup) {
		handle = external_lookup(t->word);
		if (handle != 0xffffffff) {
			t->pos[t->npos] = POS_EXTERNAL_NOUN;
			t->meaning[t->npos] = -1;
			t->external_noun.handle = handle;
			t->npos++;
		}
		if (t->npos >= MAX_MEANINGS)
			return;
	}
}

static void classify_tokens(struct nl_token *t[], int ntokens)
{
	int i;

	for (i = 0; i < ntokens; i++)
		classify_token(t[i]);
}

static void print_token_instance(struct nl_token *t, int i)
{
	printf("%s[%d]:", t->word, i);
	switch (t->pos[i]) {
	case POS_NUMBER:
		printf(" (%s: %f)\n", part_of_speech[t->pos[i]], t->number.value);
		break;
	case -1:
		printf("(not parsed)\n");
		break;
	case POS_UNKNOWN:
	case POS_NAME:
		printf("(%s)\n", part_of_speech[t->pos[i]]);
		break;
	case POS_VERB:
		printf("(%s %s (%s))\n", dictionary[t->meaning[i]].verb.syntax,
			part_of_speech[t->pos[i]],
			dictionary[t->meaning[i]].canonical_word);
		break;
	default:
		printf("(%s (%s))\n", part_of_speech[t->pos[i]],
			dictionary[t->meaning[i]].canonical_word);
		break;
	}
}

static void print_token(struct nl_token *t)
{
	int i;

	if (t->npos == 0)
		printf("%s:(unknown)\n", t->word);
	for (i = 0; i < t->npos; i++)
		print_token_instance(t, i);
}

static void print_tokens(struct nl_token *t[], int ntokens)
{
	int i;
	for (i = 0; i < ntokens; i++)
		print_token(t[i]);
}

#define MAX_MEANINGS 10
#define MAX_WORDS 100

static char *starting_syntax = "v";

/*
 * This method of parsing is something I came up with in 1984 or 1985 while trying
 * to write a game similar to Infocom's Zork.
 *
 * The idea is we have a series of state machines parsing a command concurrently.
 * Each state machine has a "expected syntax".
 * As we come to a word (token) that has multiple interpretations, we replicate
 * the state machine once for each interpretation, and let each state machine
 * move forward.  At the end, hopefully only one state machine -- the one with
 * the correct interpretation -- is left alive.  (otherwise there is an ambiguity)
 */
struct nl_parse_machine {
	struct nl_parse_machine *next, *prev;
	char *syntax;				/* expected syntax for this machine, see struct snis_nl_verb_data, above
						 * This starts out as "v" (verb), and when a verb is encounted, it
						 * changes to the syntax for that verb, and the syntax_pos is reset to 0
						 */
	int syntax_pos;				/* This state machine's current position in the syntax */
	int current_token;			/* This machine's current position in the token[] array */
	int meaning[MAX_WORDS];			/* This machine's chosen interpretation of meaning of token[x].
						 * meaning[x] stores an index into token[x].meaning[] and token[x].pos[]
						 */
	int state;				/* State of this state machine: Start-> Running-> other-states */
#define NL_STATE_START 0
#define NL_STATE_RUNNING 1
#define NL_STATE_FAILED 2
#define NL_STATE_SUCCESS 3
#define NL_STATE_DONE 4
	float score;				/* Measure of how well this SUCCESSFUL parsing matched the input.
						 * Not _every_ word has to match to be considered successful,
						 * The score is essentially (count of tokens matched / total tokens)
						 */
};

static void nl_parse_machine_init(struct nl_parse_machine *p,
		char *syntax, int syntax_pos, int current_token, int *meaning)
{
	p->next = NULL;
	p->prev = NULL;
	p->syntax = syntax;
	p->syntax_pos = syntax_pos;
	p->current_token = current_token;
	memset(p->meaning, -1, sizeof(p->meaning));
	if (meaning && current_token > 0) {
		memcpy(p->meaning, meaning, sizeof(*meaning) * current_token);
		/* printf("----- init, last meaning: %d\n", p->meaning[current_token - 1]); */
	}
	p->state = NL_STATE_RUNNING;
	p->score = 0.0;
}

static void insert_parse_machine_before(struct nl_parse_machine **list, struct nl_parse_machine *entry)
{
	if (*list == NULL) {
		*list = entry;
		(*list)->prev = NULL;
		(*list)->next = NULL;
		return;
	}
	entry->prev = (*list)->prev;
	entry->next = *list;
	if (entry->next)
		entry->next->prev = entry;
	if (entry->prev)
		entry->prev->next = entry;
	*list = entry;
}

static void nl_parse_machine_list_remove(struct nl_parse_machine **list, struct nl_parse_machine *p)
{
	if (*list == NULL)
		return;

	if (p == *list) {
		*list = (*list)->next;
		if (*list)
			(*list)->prev = NULL;
		free(p);
		return;
	}
	if (p->prev)
		p->prev->next = p->next;
	if (p->next)
		p->next->prev = p->prev;
	free(p);
	return;
}

static void nl_parse_machine_list_prune(struct nl_parse_machine **list)
{
	struct nl_parse_machine *next, *p = *list;

	while (p) {
		switch (p->state) {
		case NL_STATE_RUNNING:
		case NL_STATE_SUCCESS:
			p = p->next;
			break;
		default:
			next = p->next;
			nl_parse_machine_list_remove(list, p);
			p = next;
			break;
		}
	}
}

static struct nl_parse_machine *nl_parse_machines_find_highest_score(struct nl_parse_machine **list)
{
	float highest_score;
	struct nl_parse_machine *highest, *p = *list;
	highest = NULL;

	while (p) {
		if (!highest || highest_score < p->score) {
			highest = p;
			highest_score = p->score;
		}
		p = p->next;
	}
	return highest;
}

static void parse_machine_free_list(struct nl_parse_machine *list)
{
	struct nl_parse_machine *next;

	while (list) {
		next = list->next;
		free(list);
		list = next;
	}
}

static void nl_parse_machine_process_token(struct nl_parse_machine **list, struct nl_parse_machine *p,
				struct nl_token **token, int ntokens, int looking_for_pos)
{
	struct nl_parse_machine *new_parse_machine;
	int i, found = 0;
	int p_o_s;

	for (i = 0; i < token[p->current_token]->npos; i++) {
		p_o_s = token[p->current_token]->pos[i]; /* part of speech */
		if (p_o_s == looking_for_pos || (p_o_s == POS_EXTERNAL_NOUN && looking_for_pos == POS_NOUN)) {
			p->meaning[p->current_token] = i;
			if (found == 0) {
				p->syntax_pos++;
				p->current_token++;
			} else {
				new_parse_machine = malloc(sizeof(*new_parse_machine));
				nl_parse_machine_init(new_parse_machine, p->syntax,
						p->syntax_pos + 1, p->current_token + 1, p->meaning);
				insert_parse_machine_before(list, new_parse_machine);
			}
			found++;
			break;
		} else {
			if (looking_for_pos == POS_NOUN && (p_o_s == POS_ARTICLE || p_o_s == POS_ADJECTIVE)) {
				/* Don't advance syntax_pos, but advance to next token */
				p->meaning[p->current_token] = i;
				if (found == 0) {
					p->current_token++;
				} else {
					new_parse_machine = malloc(sizeof(*new_parse_machine));
					nl_parse_machine_init(new_parse_machine, p->syntax,
							p->syntax_pos, p->current_token + 1, p->meaning);
					insert_parse_machine_before(list, new_parse_machine);
				}
				found++;
			}
		}
	}
	if (!found) {
		/* didn't find a required meaning for this token, we're done */
		p->state = NL_STATE_FAILED;
	}
}

static void nl_parse_machine_step(struct nl_parse_machine **list,
			struct nl_parse_machine *p, struct nl_token **token, int ntokens)
{
	int i, de, p_o_s;
	int found;
	struct nl_parse_machine *new_parse_machine;

	if (p->state != NL_STATE_RUNNING)
		return;

	/* We ran out of syntax */
	if (p->syntax_pos >= strlen(p->syntax)) {
		if (p->current_token >= ntokens) {
			p->state = NL_STATE_SUCCESS;
			return;
		} else {
			p->syntax = starting_syntax;
			p->syntax_pos = 0;
		}
	}

	/* We ran out of tokens */
	if (p->current_token >= ntokens) {
		/* printf("Ran out tokens, but only %d/%d through syntax '%s'.\n",
				p->syntax_pos, (int) strlen(p->syntax), p->syntax); */
		p->state = NL_STATE_DONE;
		return;
	}

#if 0
	printf("Parsing: token = %d(%s)syntax = '%c'[%s] pos(", p->current_token,
		token[p->current_token]->word, p->syntax[p->syntax_pos], p->syntax);
	for (i = 0; i < token[p->current_token]->npos; i++) {
		p_o_s = token[p->current_token]->pos[i]; /* part of speech */
		printf("%s", part_of_speech[p_o_s]);
		if (p_o_s == POS_VERB)
			printf("[%s]", dictionary[token[p->current_token]->meaning[i]].verb.syntax);
		printf(" ");
	}
	printf(")\n");
#endif
	switch (p->syntax[p->syntax_pos]) {
	case 'v': /* starting state, just looking for verbs... */
		found = 0;
		for (i = 0; i < token[p->current_token]->npos; i++) {
			p_o_s = token[p->current_token]->pos[i]; /* part of speech */
			de = token[p->current_token]->meaning[i]; /* dictionary entry */
			switch (p_o_s) {
			case POS_VERB:
				new_parse_machine = malloc(sizeof(*new_parse_machine));
				p->meaning[p->current_token] = i;
				nl_parse_machine_init(new_parse_machine, dictionary[de].verb.syntax,
							0, p->current_token + 1, p->meaning);
				insert_parse_machine_before(list, new_parse_machine);
				found++;
				break;
			default: /* not a verb, ignore */
				break;
			}
		}
		if (found) {
			p->state = NL_STATE_DONE;
		} else {
			/* didn't find a verb meaning for this token, move to next token */
			p->current_token++;
			if (p->current_token >= ntokens)
				p->state = NL_STATE_DONE;
		}
		break;
	case 'p':
		nl_parse_machine_process_token(list, p, token, ntokens, POS_PREPOSITION);
		break;
	case 'n':
		nl_parse_machine_process_token(list, p, token, ntokens, POS_NOUN);
		break;
	case 'q':
		nl_parse_machine_process_token(list, p, token, ntokens, POS_NUMBER);
		break;
	case 'a':
		nl_parse_machine_process_token(list, p, token, ntokens, POS_ADJECTIVE);
		break;
	case 's':
		nl_parse_machine_process_token(list, p, token, ntokens, POS_SEPARATOR);
		break;
	case '\0': /* We reached the end of the syntax, success */
		if (p->current_token >= ntokens) {
			p->state = NL_STATE_SUCCESS;
		} else {
			p->syntax = starting_syntax;
			p->syntax_pos = 0;
		}
		break;
	default:
		fprintf(stderr, "Unexpected syntax entry '%c'\n", p->syntax[p->syntax_pos]);
		break;
	}
}

static int nl_parse_machines_still_running(struct nl_parse_machine **list)
{
	struct nl_parse_machine *i;

	for (i = *list; i != NULL; i = i->next)
		if (i->state == NL_STATE_RUNNING)
			return 1;
	return 0;
}

static void do_action(struct nl_parse_machine *p, struct nl_token **token, int ntokens)
{
	int argc;
	char *argv[MAX_WORDS];
	int pos[MAX_WORDS];
	union snis_nl_extra_data extra_data[MAX_WORDS] = { { { 0 } } };
	int i, w = 0, de;
	snis_nl_verb_function vf = NULL;

	argc = 0;
	for (i = 0; i < ntokens; i++) {
		struct nl_token *t = token[i];
		if (t->pos[p->meaning[i]] == POS_VERB) {
			if (vf != NULL) {
				vf(argc, argv, pos, extra_data);
				vf = NULL;
			}
			argc = 1;
			w = 0;
			argv[w] = t->word;
			pos[w] = POS_VERB;
			extra_data[w].number.value = 0.0;
			de = t->meaning[p->meaning[i]];
			vf = dictionary[de].verb.fn;
			w++;
		} else {
			if (argc > 0) {
				argv[w] = t->word;
				pos[w] = t->pos[p->meaning[i]];
				if (pos[w] == POS_NUMBER) {
					extra_data[w].number.value = t->number.value;
				} else if (pos[w] == POS_EXTERNAL_NOUN) {
					extra_data[w].external_noun.handle = t->external_noun.handle;
				} else {
					extra_data[w].number.value = 0.0;
				}
				w++;
				argc++;
			}
		}
	}
	if (vf != NULL) {
		vf(argc, argv, pos, extra_data);
		vf = NULL;
	}
}

static void nl_parse_machine_print(struct nl_parse_machine *p, struct nl_token **token, int ntokens, int number)
{
	int i;

	printf("State machine %d ('%s,%d, ', ", number, p->syntax, p->syntax_pos);
	switch (p->state) {
	case NL_STATE_START:
		printf("START)");
		break;
	case NL_STATE_RUNNING:
		printf("RUNNING)");
		break;
	case NL_STATE_FAILED:
		printf("FAILED)");
		break;
	case NL_STATE_SUCCESS:
		printf("SUCCESS)");
		break;
	case NL_STATE_DONE:
		printf("DONE)");
		break;
	default:
		printf("UNKNOWN (%d))", p->state);
		break;
	}
	printf(" -- score = %f\n", p->score);
	for (i = 0; i < ntokens; i++) {
		print_token_instance(token[i], p->meaning[i]);
	}
}

static void nl_parse_machines_print(struct nl_parse_machine **list, struct nl_token **token, int ntokens)
{
	int x;
	struct nl_parse_machine *i;

	x = 0;
	for (i = *list; i != NULL; i = i->next)
		nl_parse_machine_print(i, token, ntokens, x++);
}

static void nl_parse_machines_run(struct nl_parse_machine **list, struct nl_token **tokens, int ntokens)
{
	int iteration = 0;
	struct nl_parse_machine *i;

	do {
		/* printf("--- iteration %d ---\n", iteration);
		nl_parse_machines_print(list, tokens, ntokens); */
		for (i = *list; i != NULL; i = i->next)
			nl_parse_machine_step(list, i, tokens, ntokens);
		iteration++;
		nl_parse_machine_list_prune(list);
	} while (nl_parse_machines_still_running(list));
	/* printf("--- iteration %d ---\n", iteration);
	nl_parse_machines_print(list, tokens, ntokens); */
}

static void nl_parse_machine_score(struct nl_parse_machine *p, int ntokens)
{
	int i;

	float score = 0.0;

	if (p->state != NL_STATE_SUCCESS || ntokens == 0) {
		p->score = 0.0;
		return;
	}

	for (i = 0; i < p->current_token; i++) {
		if (p->meaning[i] != -1)
			score += 1.0;
	}
	p->score = score / (float) ntokens;
}

static void nl_parse_machines_score(struct nl_parse_machine **list, int ntokens)
{
	struct nl_parse_machine *p = *list;

	for (p = *list; p; p = p->next)
		nl_parse_machine_score(p, ntokens);
}

static void extract_meaning(char *original_text, struct nl_token *token[], int ntokens)
{
	struct nl_parse_machine *list, *p;

	list = NULL;
	p = malloc(sizeof(*p));
	nl_parse_machine_init(p, starting_syntax, 0, 0, NULL);
	insert_parse_machine_before(&list, p);
	nl_parse_machines_run(&list, token, ntokens);
	nl_parse_machine_list_prune(&list);
	nl_parse_machines_score(&list, ntokens);
	p = nl_parse_machines_find_highest_score(&list);
	if (p) {
		printf("-------- Final interpretation: ----------\n");
		nl_parse_machine_print(p, token, ntokens, 0);
	} else {
		printf("Failure to comprehend '%s'\n", original_text);
	}
	do_action(p, token, ntokens);
}

void snis_nl_parse_natural_language_request(char *txt)
{
	int ntokens;
	struct nl_token **token = NULL;
	char *original = strdup(txt);

	lowercase(txt);
	token = tokenize(txt, &ntokens);
	classify_tokens(token, ntokens);
	// print_tokens(token, ntokens);
	extract_meaning(original, token, ntokens);
	free_tokens(token, ntokens);
	if (token)
		free(token);
	free(original);
}

void snis_nl_add_synonym(char *synonym_word, char *canonical_word)
{
	if (nsynonyms >= MAX_SYNONYMS) {
		fprintf(stderr, "Too many synonyms, discarding '%s/%s'\n", synonym_word, canonical_word);
		return;
	}
	synonym[nsynonyms].syn = strdup(synonym_word);
	synonym[nsynonyms].canonical = strdup(canonical_word);
	nsynonyms++;
}

static void generic_add_dictionary_word(char *word, char *canonical_word, int part_of_speech,
						char *syntax, snis_nl_verb_function action)
{
	struct dictionary_entry *de;

	if (ndictionary_entries >= MAX_DICTIONARY_ENTRIES) {
		fprintf(stderr, "Too many dictionary entries, discarding '%s/%s'\n", word, canonical_word);
		return;
	}

	de = &dictionary[ndictionary_entries];
	memset(de, 0, sizeof(*de));
	de->word = strdup(word);
	de->canonical_word = strdup(canonical_word);
	de->p_o_s = part_of_speech;
	if (de->p_o_s == POS_VERB) {
		de->verb.syntax = syntax;
		de->verb.fn = action;
	}
	ndictionary_entries++;
}

void snis_nl_add_dictionary_verb(char *word, char *canonical_word, char *syntax, snis_nl_verb_function action)
{
	generic_add_dictionary_word(word, canonical_word, POS_VERB, syntax, action);
}

void snis_nl_add_dictionary_word(char *word, char *canonical_word, int part_of_speech)
{
	generic_add_dictionary_word(word, canonical_word, part_of_speech, NULL, NULL);
}

void snis_nl_add_external_lookup(snis_nl_external_noun_lookup lookup)
{
	if (external_lookup) {
		fprintf(stderr, "Too many lookup functions added\n");
	}
	external_lookup = lookup;
}

#ifdef TEST_NL
static void init_synonyms(void)
{
	snis_nl_add_synonym("cut", "lower");
	snis_nl_add_synonym("decrease", "lower");
	snis_nl_add_synonym("boost", "raise");
	snis_nl_add_synonym("increase", "raise");
	snis_nl_add_synonym("calculate", "compute");
	snis_nl_add_synonym("figure", "compute");
	snis_nl_add_synonym("activate", "engage");
	snis_nl_add_synonym("actuate", "engage");
	snis_nl_add_synonym("start", "engage");
	snis_nl_add_synonym("energize", "engage");
	snis_nl_add_synonym("deactivate", "disengage");
	snis_nl_add_synonym("deenergize", "disengage");
	snis_nl_add_synonym("stop", "disengage");
	snis_nl_add_synonym("shutdown", "disengage");
	snis_nl_add_synonym("deploy", "launch");
}

static void generic_verb_action(int argc, char *argv[], int pos[],
		__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	int i;

	printf("generic_verb_action: ");
	for (i = 0; i < argc; i++) {
		printf("%s(%s) ", argv[i], part_of_speech[pos[i]]);
	}
	printf("\n");
}

static void init_dictionary(void)
{
	snis_nl_add_dictionary_verb("navigate",		"navigate",	"pn", generic_verb_action);
	snis_nl_add_dictionary_verb("set",		"set",		"npq", generic_verb_action);
	snis_nl_add_dictionary_verb("set",		"set",		"npa", generic_verb_action);
	snis_nl_add_dictionary_verb("lower",		"lower",	"npq", generic_verb_action);
	snis_nl_add_dictionary_verb("lower",		"lower",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb("raise",		"raise",	"nq", generic_verb_action);
	snis_nl_add_dictionary_verb("raise",		"raise",	"npq", generic_verb_action);
	snis_nl_add_dictionary_verb("raise",		"raise",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb("engage",		"engage",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb("disengage",	"disengage",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb("turn",		"turn",		"pn", generic_verb_action);
	snis_nl_add_dictionary_verb("turn",		"turn",		"aq", generic_verb_action);
	snis_nl_add_dictionary_verb("compute",		"compute",	"npn", generic_verb_action);
	snis_nl_add_dictionary_verb("report",		"report",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb("yaw",		"yaw",		"aq", generic_verb_action);
	snis_nl_add_dictionary_verb("pitch",		"pitch",	"aq", generic_verb_action);
	snis_nl_add_dictionary_verb("roll",		"roll",		"aq", generic_verb_action);
	snis_nl_add_dictionary_verb("zoom",		"zoom",		"p", generic_verb_action);
	snis_nl_add_dictionary_verb("zoom",		"zoom",		"pq", generic_verb_action);
	snis_nl_add_dictionary_verb("shut",		"shut",		"an", generic_verb_action);
	snis_nl_add_dictionary_verb("shut",		"shut",		"na", generic_verb_action);
	snis_nl_add_dictionary_verb("launch",		"launch",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb("eject",		"eject",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb("full",		"full",		"n", generic_verb_action);

	snis_nl_add_dictionary_word("drive",		"drive",	POS_NOUN);
	snis_nl_add_dictionary_word("system",		"system",	POS_NOUN);
	snis_nl_add_dictionary_word("starbase",		"starbase",	POS_NOUN);
	snis_nl_add_dictionary_word("base",		"starbase",	POS_NOUN);
	snis_nl_add_dictionary_word("planet",		"planet",	POS_NOUN);
	snis_nl_add_dictionary_word("ship",		"ship",		POS_NOUN);
	snis_nl_add_dictionary_word("bot",		"bot",		POS_NOUN);
	snis_nl_add_dictionary_word("shields",		"shields",	POS_NOUN);
	snis_nl_add_dictionary_word("throttle",		"throttle",	POS_NOUN);
	snis_nl_add_dictionary_word("factor",		"factor",	POS_NOUN);
	snis_nl_add_dictionary_word("coolant",		"coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("level",		"level",	POS_NOUN);
	snis_nl_add_dictionary_word("energy",		"energy",	POS_NOUN);
	snis_nl_add_dictionary_word("power",		"energy",	POS_NOUN);
	snis_nl_add_dictionary_word("asteroid",		"asteroid",	POS_NOUN);
	snis_nl_add_dictionary_word("nebula",		"nebula",	POS_NOUN);
	snis_nl_add_dictionary_word("star",		"star",		POS_NOUN);
	snis_nl_add_dictionary_word("range",		"range",	POS_NOUN);
	snis_nl_add_dictionary_word("distance",		"range",	POS_NOUN);
	snis_nl_add_dictionary_word("weapons",		"weapons",	POS_NOUN);
	snis_nl_add_dictionary_word("screen",		"screen",	POS_NOUN);
	snis_nl_add_dictionary_word("robot",		"robot",	POS_NOUN);
	snis_nl_add_dictionary_word("torpedo",		"torpedo",	POS_NOUN);
	snis_nl_add_dictionary_word("phasers",		"phasers",	POS_NOUN);
	snis_nl_add_dictionary_word("maneuvering",	"maneuvering",	POS_NOUN);
	snis_nl_add_dictionary_word("thruster",		"thrusters",	POS_NOUN);
	snis_nl_add_dictionary_word("thrusters",	"thrusters",	POS_NOUN);
	snis_nl_add_dictionary_word("sensor",		"sensors",	POS_NOUN);
	snis_nl_add_dictionary_word("science",		"science",	POS_NOUN);
	snis_nl_add_dictionary_word("comms",		"comms",	POS_NOUN);
	snis_nl_add_dictionary_word("enemy",		"enemy",	POS_NOUN);
	snis_nl_add_dictionary_word("derelict",		"derelict",	POS_NOUN);
	snis_nl_add_dictionary_word("computer",		"computer",	POS_NOUN);
	snis_nl_add_dictionary_word("fuel",		"fuel",		POS_NOUN);
	snis_nl_add_dictionary_word("radiation",	"radiation",	POS_NOUN);
	snis_nl_add_dictionary_word("wavelength",	"wavelength",	POS_NOUN);
	snis_nl_add_dictionary_word("charge",		"charge",	POS_NOUN);
	snis_nl_add_dictionary_word("magnets",		"magnets",	POS_NOUN);
	snis_nl_add_dictionary_word("gate",		"gate",		POS_NOUN);
	snis_nl_add_dictionary_word("percent",		"percent",	POS_NOUN);
	snis_nl_add_dictionary_word("sequence",		"sequence",	POS_NOUN);
	snis_nl_add_dictionary_word("core",		"core",		POS_NOUN);
	snis_nl_add_dictionary_word("code",		"code",		POS_NOUN);
	snis_nl_add_dictionary_word("hull",		"hull",		POS_NOUN);
	snis_nl_add_dictionary_word("scanner",		"scanner",	POS_NOUN);
	snis_nl_add_dictionary_word("scanners",		"scanners",	POS_NOUN);
	snis_nl_add_dictionary_word("detail",		"details",	POS_NOUN);
	snis_nl_add_dictionary_word("report",		"report",	POS_NOUN);
	snis_nl_add_dictionary_word("damage",		"damage",	POS_NOUN);
	snis_nl_add_dictionary_word("course",		"course",	POS_NOUN);


	snis_nl_add_dictionary_word("a",		"a",		POS_ARTICLE);
	snis_nl_add_dictionary_word("an",		"an",		POS_ARTICLE);
	snis_nl_add_dictionary_word("the",		"the",		POS_ARTICLE);

	snis_nl_add_dictionary_word("above",		"above",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("aboard",		"aboard",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("across",		"across",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("after",		"after",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("along",		"along",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("alongside",	"alongside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("at",		"at",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("atop",		"atop",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("around",		"around",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("before",		"before",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("behind",		"behind",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("beneath",		"beneath",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("below",		"below",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("beside",		"beside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("besides",		"besides",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("between",		"between",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("by",		"by",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("down",		"down",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("during",		"during",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("except",		"except",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("for",		"for",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("from",		"from",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("in",		"in",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("inside",		"inside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("of",		"of",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("off",		"off",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("on",		"on",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("onto",		"onto",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("out",		"out",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("outside",		"outside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("through",		"through",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("throughout",	"throughout",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("to",		"to",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("toward",		"toward",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("under",		"under",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("up",		"up",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("until",		"until",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("with",		"with",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("within",		"within",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("without",		"without",	POS_PREPOSITION);

	snis_nl_add_dictionary_word("or",		"or",		POS_SEPARATOR);
	snis_nl_add_dictionary_word("and",		"and",		POS_SEPARATOR);
	snis_nl_add_dictionary_word("then",		"then",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(",",		",",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(".",		".",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(";",		";",		POS_SEPARATOR);
	snis_nl_add_dictionary_word("!",		"!",		POS_SEPARATOR);
	snis_nl_add_dictionary_word("?",		"?",		POS_SEPARATOR);

	snis_nl_add_dictionary_word("damage",		"damage",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("status",		"status",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("warp",		"warp",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("impulse",		"impulse",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("docking",		"docking",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("star",		"star",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("space",		"space",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("mining",		"mining",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("energy",		"energy",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("main",		"main",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("navigation",	"navigation",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("comms",		"comms",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("engineering",	"engineering",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("science",		"science",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("enemy",		"enemy",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("derelict",		"derelict",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("solar",		"solar",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("nearest",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("closest",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("nearby",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("close",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("up",		"up",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("down",		"down",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("left",		"left",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("right",		"right",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("self",		"self",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("destruct",		"destruct",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("self-destruct",	"self-destruct",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("short",		"short",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("long",		"long",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("range",		"range",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("full",		"maximum",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("max",		"maximum",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("maximum",		"maximum",	POS_ADJECTIVE);

	snis_nl_add_dictionary_word("percent",		"percent",	POS_ADVERB);
	snis_nl_add_dictionary_word("quickly",		"quickly",	POS_ADVERB);
	snis_nl_add_dictionary_word("rapidly",		"quickly",	POS_ADVERB);
	snis_nl_add_dictionary_word("swiftly",		"quickly",	POS_ADVERB);
	snis_nl_add_dictionary_word("slowly",		"slowly",	POS_ADVERB);

	snis_nl_add_dictionary_word("it",		"it",		POS_PRONOUN);
	snis_nl_add_dictionary_word("me",		"me",		POS_PRONOUN);
	snis_nl_add_dictionary_word("them",		"them",		POS_PRONOUN);
	snis_nl_add_dictionary_word("all",		"all",		POS_PRONOUN);
	snis_nl_add_dictionary_word("everything",	"everything",	POS_PRONOUN);

}

int main(int argc, char *argv[])
{
	int i;

	init_synonyms();
	init_dictionary();

	for (i = 1; i < argc; i++)
		snis_nl_parse_natural_language_request(argv[i]);
	return 0;
}

#endif