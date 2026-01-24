// SPDX-License-Identifier: GPL-2.0-only

#include <bobject.h>
#include <stdio.h>
#include <param.h>
#include <xfuncs.h>
#include <string.h>

static char *json_escape_string(const char *str)
{
	char *result = NULL;
	const char *p;

	if (!str)
		return xstrdup("");

	for (p = str; *p; p++) {
		switch (*p) {
		case '"':
			result = xrasprintf(result, "\\\"");
			break;
		case '\\':
			result = xrasprintf(result, "\\\\");
			break;
		case '\b':
			result = xrasprintf(result, "\\b");
			break;
		case '\f':
			result = xrasprintf(result, "\\f");
			break;
		case '\n':
			result = xrasprintf(result, "\\n");
			break;
		case '\r':
			result = xrasprintf(result, "\\r");
			break;
		case '\t':
			result = xrasprintf(result, "\\t");
			break;
		default:
			result = xrasprintf(result, "%c", *p);
			break;
		}
	}

	return result;
}

/**
 * bobject_set_name - set a barebox object's name
 * @bobj: barebox object or device
 * @fmt: format string for the object's name
 *
 * NOTE: This function expects bobj->name to be free()-able, so extra
 * precautions needs to be taken when mixing its usage with manual
 * assignement of bobject.name.
 */
int bobject_set_name(bobject_t bobj, const char *fmt, ...)
{
	va_list vargs;
	int err;
	/*
	 * Save old pointer in case we are overriding already set name
	 */
	char *oldname = bobj.bobj->name;

	va_start(vargs, fmt);
	err = vasprintf(&bobj.bobj->name, fmt, vargs);
	va_end(vargs);

	/*
	 * Free old pointer, we do this after vasprintf call in case
	 * old device name was in one of vargs
	 */
	free_const(oldname);

	return WARN_ON(err < 0) ? err : 0;
}
EXPORT_SYMBOL_GPL(bobject_set_name);

struct bobject *bobject_alloc(const char *name)
{
	struct bobject *bobj = xzalloc(sizeof(*bobj));

	bobject_init(bobj);
	bobject_set_name(bobj, "%s", name);

	return bobj;
}
EXPORT_SYMBOL_GPL(bobject_alloc);

void bobject_free(struct bobject *bobj)
{
	if (!bobj)
		return;

	bobject_del(bobj);
	free(bobj);
}
EXPORT_SYMBOL_GPL(bobject_free);

/**
 * bobject_del - remove all parameters from a bobject and free their
 * memory
 * @param bobject	The barebox object
 */
void bobject_del(struct bobject *bobj)
{
	struct param_d *p, *n;

	list_for_each_entry_safe(p, n, &bobj->parameters, list)
		param_remove(p);

	free_const(bobj->name);
}
EXPORT_SYMBOL(bobject_del);

/**
 * bobject_format_json - format a barebox object as JSON string
 * @bobj: barebox object to format
 *
 * Returns an allocated JSON string representation of the bobject and its
 * parameters. The caller is responsible for freeing the returned string.
 * Returns "{}" for NULL bobject.
 */
char *bobject_format_json(struct bobject *bobj)
{
	char *json = NULL;
	char *esc_name, *esc_value;
	struct param_d *p;
	const char *value;
	bool first = true;

	if (!bobj)
		return xstrdup("{}");

	/* Escape and add object name */
	esc_name = json_escape_string(bobj->name);
	json = xrasprintf(json, "{ \"%s\": {", esc_name);
	free(esc_name);

	/* Iterate over parameters */
	list_for_each_entry(p, &bobj->parameters, list) {
		/* Add comma separator (except for first parameter) */
		if (!first)
			json = xrasprintf(json, ",");
		first = false;

		/* Escape parameter name */
		esc_name = json_escape_string(p->name);

		/* Get parameter value */
		value = p->get(p->bobj, p);
		if (!value)
			value = "";

		/* Escape parameter value */
		esc_value = json_escape_string(value);

		/* Append parameter to JSON */
		json = xrasprintf(json, " \"%s\": \"%s\"", esc_name, esc_value);

		/* Free escaped strings */
		free(esc_name);
		free(esc_value);
	}

	/* Close JSON object */
	json = xrasprintf(json, " } }");

	return json;
}
EXPORT_SYMBOL_GPL(bobject_format_json);

static bool is_json_literal(const char *value)
{
	if (!value)
		return false;

	/* Check for boolean literals */
	if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0)
		return true;

	/* Check for null */
	if (strcmp(value, "null") == 0)
		return true;

	/* Check for JSON objects and arrays */
	if (value[0] == '{' || value[0] == '[')
		return true;

	/* Could add numeric check here if needed */
	return false;
}

/**
 * bobject_format_json_params - format barebox object parameters as JSON object
 * @bobj: barebox object to format
 *
 * Returns an allocated JSON object containing only the parameters, without
 * the outer name wrapper. Format: { "param1": "value1", "param2": "value2" }
 * Boolean values "true"/"false" and "null" are output as unquoted literals.
 * The caller is responsible for freeing the returned string.
 * Returns "{}" for NULL bobject or bobject with no parameters.
 */
char *bobject_format_json_params(struct bobject *bobj)
{
	char *json = NULL;
	char *esc_name, *esc_value;
	struct param_d *p;
	const char *value;
	bool first = true;

	json = xrasprintf(json, "{");

	if (!bobj) {
		json = xrasprintf(json, " }");
		return json;
	}

	/* Iterate over parameters */
	list_for_each_entry(p, &bobj->parameters, list) {
		/* Add comma separator (except for first parameter) */
		if (!first)
			json = xrasprintf(json, ",");
		first = false;

		/* Escape parameter name */
		esc_name = json_escape_string(p->name);

		/* Get parameter value */
		value = p->get(p->bobj, p);
		if (!value)
			value = "";

		/* Check if value should be output as unquoted literal */
		if (is_json_literal(value)) {
			json = xrasprintf(json, " \"%s\": %s", esc_name, value);
		} else {
			/* Escape parameter value and quote it */
			esc_value = json_escape_string(value);
			json = xrasprintf(json, " \"%s\": \"%s\"", esc_name, esc_value);
			free(esc_value);
		}

		free(esc_name);
	}

	/* Close JSON object */
	json = xrasprintf(json, " }");

	return json;
}
EXPORT_SYMBOL_GPL(bobject_format_json_params);

/**
 * bobject_format_json_append - append bobject parameters to existing JSON object
 * @json: existing JSON object string
 * @bobj: barebox object whose parameters to append
 *
 * Takes a previously generated JSON object string, walks back whitespace at the
 * end, replaces the final '}', adds a comma, formats the new bobject's parameters,
 * and closes it again.
 *
 * Returns an allocated string with the combined JSON object.
 * The caller is responsible for freeing the returned string.
 */
char *bobject_format_json_append(const char *json, struct bobject *bobj)
{
	char *result = NULL;
	const char *end;
	size_t len;
	char *esc_name, *esc_value;
	struct param_d *p;
	const char *value;
	bool first;

	if (!json)
		return bobject_format_json_params(bobj);

	if (!bobj || list_empty(&bobj->parameters))
		return xstrdup(json);

	/* Find the closing '}' by walking back from end */
	len = strlen(json);
	end = json + len - 1;

	/* Skip trailing whitespace */
	while (end > json && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
		end--;

	/* Verify it's a '}' */
	if (*end != '}')
		return xstrdup(json); /* Invalid JSON, return as-is */

	/* Copy everything up to (but not including) the '}' */
	result = xstrndup(json, end - json);

	/* Check if there were already parameters (look for content before }) */
	first = false;
	if (end > json) {
		const char *check = end - 1;
		/* Walk back to see if we have content before the } */
		while (check > json && (*check == ' ' || *check == '\t' || *check == '\n' || *check == '\r'))
			check--;
		/* If we're at the opening {, this is an empty object */
		first = (*check == '{');
	}

	/* Add parameters from bobj */
	list_for_each_entry(p, &bobj->parameters, list) {
		/* Add comma separator if not first */
		if (!first)
			result = xrasprintf(result, ",");
		first = false;

		/* Escape parameter name */
		esc_name = json_escape_string(p->name);

		/* Get parameter value */
		value = p->get(p->bobj, p);
		if (!value)
			value = "";

		/* Check if value should be output as unquoted literal */
		if (is_json_literal(value)) {
			result = xrasprintf(result, " \"%s\": %s", esc_name, value);
		} else {
			esc_value = json_escape_string(value);
			result = xrasprintf(result, " \"%s\": \"%s\"", esc_name, esc_value);
			free(esc_value);
		}

		free(esc_name);
	}

	/* Close with } */
	result = xrasprintf(result, " }");

	return result;
}
EXPORT_SYMBOL_GPL(bobject_format_json_append);

/**
 * bobject_print_json - print a barebox object as JSON to stdout
 * @bobj: barebox object to print
 *
 * Formats the bobject as JSON and prints it to stdout with a newline.
 */
void bobject_print_json(struct bobject *bobj)
{
	char *json = bobject_format_json(bobj);

	if (json) {
		printf("%s\n", json);
		free(json);
	}
}
EXPORT_SYMBOL_GPL(bobject_print_json);
