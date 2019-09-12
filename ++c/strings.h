#pragma once

// users may directly compare nts pointers to see if they are equal.
struct str
{
	const char* nts;
	int len;
};

str strings_insert(const char* start, const char* end);
str strings_insert_nts(const char* nts); //nts=null-terminated string