/*
	Copyright (C) 2018 Stephen M. Cameron
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "snis_tweak.h"
#include "arraysize.h"

int find_tweakable_var_descriptor(struct tweakable_var_descriptor *desc, int count, char *name)
{
	int i;

	for (i = 0; i < count; i++) {
		if (!desc[i].name)
			return -1;
		if (strcmp(name, desc[i].name) == 0)
			return i;
	}
	return -1;
}

int tweak_variable(struct tweakable_var_descriptor *tweak, int count, char *cmd,
			char *msg, int msgsize)
{
	int rc;
	char variable[255], valuestr[255];
	struct tweakable_var_descriptor *v;
	float f;
	int i;
	uint8_t b;

	rc = sscanf(cmd, "SET %s%*[ ]=%*[ ]%s", variable, valuestr);
	if (rc != 2) {
		if (msg)
			snprintf(msg, msgsize, "SET: INVALID SET COMMAND: %s", cmd);
		return TWEAK_BAD_SYNTAX;
	}
	rc = find_tweakable_var_descriptor(tweak, count, variable);
	if (rc < 0) {
		if (msg)
			snprintf(msg, msgsize, "SET: INVALID VARIABLE: %s", variable);
		return TWEAK_UNKNOWN_VARIABLE;
	}
	v = &tweak[rc];
	switch (v->type) {
	case 'f':
		if (strcmp(valuestr, "DEFAULT") == 0) {
			f = v->defaultf;
		} else {
			rc = sscanf(valuestr, "%f", &f);
			if (rc != 1) {
				if (msg)
					snprintf(msg, msgsize, "SET: UNPARSEABLE FLOAT VALUE");
				return TWEAK_BAD_SYNTAX;
			}
		}
		if (f < v->minf || f > v->maxf) {
			if (msg)
				snprintf(msg, msgsize, "SET: FLOAT VALUE OUT OF RANGE");
			return TWEAK_OUT_OF_RANGE;
		}
		*((float *) v->address) = f;
		if (msg)
			snprintf(msg, msgsize, "%s SET TO %f", variable, f);
		break;
	case 'b':
		if (strcmp(valuestr, "DEFAULT") == 0) {
			b = v->defaulti;
		} else {
			rc = sscanf(valuestr, "%hhu", &b);
			if (rc != 1) {
				snprintf(msg, msgsize, "SET: UNPARSEABLE BYTE VALUE");
				return TWEAK_BAD_SYNTAX;
			}
		}
		if (b < v->mini || b > v->maxi) {
			if (msg)
				snprintf(msg, msgsize, "SET: BYTE VALUE OUT OF RANGE");
			return TWEAK_OUT_OF_RANGE;
		}
		*((uint8_t *) v->address) = b;
		if (msg)
			snprintf(msg, msgsize, "%s SET TO %hhu", variable, b);
		break;
	case 'i':
		if (strcmp(valuestr, "DEFAULT") == 0) {
			i = v->defaulti;
		} else {
			rc = sscanf(valuestr, "%d", &i);
			if (rc != 1) {
				if (msg)
					snprintf(msg, msgsize, "SET: UNPARSEABLE INT VALUE");
				return TWEAK_BAD_SYNTAX;
			}
		}
		if (i < v->mini || i > v->maxi) {
			if (msg)
				snprintf(msg, msgsize, "SET: INT VALUE OUT OF RANGE");
			return TWEAK_OUT_OF_RANGE;
		}
		*((int *) v->address) = i;
		if (msg)
			snprintf(msg, msgsize, "%s SET TO %d", variable, i);
		break;
	default:
		if (msg)
			snprintf(msg, msgsize, "VARIABLE NOT SET, UNKNOWN TYPE.");
		return TWEAK_UNKNOWN_TYPE;
	}
	return 0;
}

void tweakable_vars_list(struct tweakable_var_descriptor *tweak, int count, void (*printfn)(const char *, ...))
{
	int i;
	struct tweakable_var_descriptor *v;

	for (i = 0; i < count; i++) {
		v = &tweak[i];
		if (!v->name)
			break;
		switch (v->type) {
		case 'f':
			printfn("%s = %.2f (D=%.2f, MN=%.2f, MX=%.2f)", v->name,
					*((float *) v->address), v->defaultf, v->minf, v->maxf);
			break;
		case 'b':
			printfn("%s = %hhu (D=%d, MN=%d, MX=%d)", v->name,
					*((uint8_t *) v->address), v->defaulti, v->mini, v->maxi);
			break;
		case 'i':
			printfn("%s = %d (D=%d,MN=%d,MX=%d)", v->name,
					*((int32_t *) v->address), v->defaulti, v->mini, v->maxi);
			break;
		default:
			printfn("%s = ? (unknown type '%c')", v->name, v->type);
			break;
		}
	}
}

int tweakable_var_describe(struct tweakable_var_descriptor *tweak, int count, char *cmd,
				void (*printfn)(const char *, ...), int suppress_unknown_var)
{
	int rc;
	char variable[255];
	struct tweakable_var_descriptor *v;

	rc = sscanf(cmd, "DESCRIBE%*[ ]%s", variable);
	if (rc != 1) {
		printfn("DESCRIBE: INVALID USAGE. USE E.G., DESCRIBE MAX_PLAYER_VELOCITY");
		return TWEAK_BAD_SYNTAX;
	}
	rc = find_tweakable_var_descriptor(tweak, count, variable);
	if (rc < 0) {
		if (!suppress_unknown_var)
			printfn("DESCRIBE: INVALID VARIABLE NAME");
		return TWEAK_UNKNOWN_VARIABLE;
	}
	v = &tweak[rc];
	printfn(variable);
	printfn("   DESC: %s", v->description);
	printfn("   TYPE: %c", toupper(v->type));
	switch (v->type) {
	case 'b':
		printfn("   CURRENT VALUE: %hhu", *((uint8_t *) v->address));
		printfn("   DEFAULT VALUE: %d", v->defaulti);
		printfn("   MINIMUM VALUE: %d", v->mini);
		printfn("   MAXIMUM VALUE: %d", v->maxi);
		break;
	case 'i':
		printfn("   CURRENT VALUE: %d", *((int *) v->address));
		printfn("   DEFAULT VALUE: %d", v->defaulti);
		printfn("   MINIMUM VALUE: %d", v->mini);
		printfn("   MAXIMUM VALUE: %d", v->maxi);
		break;
	case 'f':
		printfn("   CURRENT VALUE: %f", *((float *) v->address));
		printfn("   DEFAULT VALUE: %f", v->defaultf);
		printfn("   MINIMUM VALUE: %f", v->minf);
		printfn("   MAXIMUM VALUE: %f", v->maxf);
		break;
	default:
		printfn("   VARIABLE HAS UNKNOWN TYPE");
		return TWEAK_UNKNOWN_TYPE;
	}
	return 0;
}

