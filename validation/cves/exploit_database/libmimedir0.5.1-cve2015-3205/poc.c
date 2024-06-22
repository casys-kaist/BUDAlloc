#include "libmimedir.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	printf("target file = %s\n", "free.vcf");

	mdir_line **l = mdir_parse_file("free.vcf");
	printf("result=%p\n", l);
	return 0;
}
