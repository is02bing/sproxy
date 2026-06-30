#include "filemacro.h"
// “/a/b/c.c”  -> b/c.c
const char* mystrrchr2(const char *cp, char ch)
{
	char *save;
	char *prev;
	char c;

	for (save = (char *) 0, prev = (char*)0; (c = *cp); cp++) {
		if (c == ch) {
			if (save != 0) {
				prev = save;
			}
			save = (char *) cp;
		}
	}

	return prev != 0 ? prev : save;
}