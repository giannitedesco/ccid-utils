/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#include <ccid.h>

#include <stdio.h>
#include <ctype.h>

void hex_dump(void *t, size_t len, size_t llen)
{
	uint8_t *tmp = t;
	size_t i, j;
	size_t line;

	for(j=0; j < len; j += line, tmp += line) {
		if ( j + llen > len ) {
			line = len - j;
		}else{
			line = llen;
		}

		printf(" | %05X : ", j);

		for(i=0; i < line; i++) {
			if ( isprint(tmp[i]) ) {
				printf("%c", tmp[i]);
			}else{
				printf(".");
			}
		}

		for(; i < llen; i++)
			printf(" ");

		for(i = 0; i < line; i++)
			printf(" %02X", tmp[i]);

		printf("\n");
	}
	printf("\n");
}
