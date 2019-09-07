#include "strings.h"
#include "debug.h"
#include "memory.h"
#include <cinttypes>

char g_nts_strings[1024];
char* g_nts_strings_end = g_nts_strings;
char* g_nts_strings_cap = g_nts_strings + sizeof(g_nts_strings);
str g_table[32];
str* g_table_end = g_table;
str* g_table_cap = g_table + sizeof(g_table);

str strings_insert(const char* start, const char* end)
{
	int len = int(end - start);

	// Search table for entry
	str* iter = g_table;
	while (iter < g_table_end)
	{
		if (iter->len != len)
		{
			++iter;
			continue;
		}

		if (0 != memcmp(iter->nts, start, len))
		{
			++iter;
			continue;
		}

		return *iter;
	}

	// check for room in nts buffer
	if (g_nts_strings_end + len + 1 >= g_nts_strings_cap)
	{
		debug_break();
		return str();
	}

	// check for room in table
	if (iter + 1 >= g_table_cap)
	{
		debug_break();
		return str();
	}

	iter->nts = g_nts_strings_end;
	iter->len = len;

	// insert
	// NOTE: no need to null-terminate because g_table is memset to 0 by being in global namespace
	memcpy(g_nts_strings_end, start, len);
	g_nts_strings_end += len + 1;
	++g_table_end;

	return *iter;
}

str strings_insert_nts(const char* nts)
{
	const char* end = nts;
	while (*end) ++end;

	return strings_insert(nts, end);
}