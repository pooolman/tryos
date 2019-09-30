#include <stdint.h>

#include "sys.h"
#include "ulib.h"

int32_t main(int32_t argc, char * argv[])
{
	for(int32_t i = 1; i < argc; i++)
		printf("%s ", argv[i]);
	printf("\n");

	return 0;
}
