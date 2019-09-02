#include "strings.h"
#include "debug.h"
#include "memory.h"
#include <cinttypes>

char g_table[1024];
char* g_table_end = g_table;
char* g_table_cap = g_table + sizeof(g_table);

str strings_insert(const char* start, const char* end)
{
	int len = int(end - start);

	// Search table for entry
	{
		char* iter = g_table;
		while (iter + len < g_table_end)
		{
			// check entry
			if (0 == memcmp(iter, start, len))
			{
				str s;
				s.nts = iter;
				s.len = len;
				return s;
			}

			// entry not found, advance
			while (*iter) ++iter;
			++iter; // advance past null-terminator
		}
	}

	// No entry found, insert
	if (g_table_end + len + 1 < g_table_cap)
	{
		str s;
		s.nts = g_table_end;
		s.len = len;

		// insert
		memcpy((void*)g_table_end, start, len);
		g_table_end += len + 1;
		// NOTE: no need to null-terminate because g_table is memset to 0 by being in global namespace

		return s;
	}

	// out of memory
	debug_break();
	return str();
}

str strings_insert_nts(const char* nts)
{
	const char* end = nts;
	while (*end) ++end;

	return strings_insert(nts, end);
}