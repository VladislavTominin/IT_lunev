#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include "read_int.h"


int read_int(int argc, char *argv[])
{
	int base;
    char *endptr, *str;
    int val;

    if (argc != 2) {
		fprintf(stderr, "Usage: %s str [base]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
	int i = 1;


		str = argv[i];
		base = 10;
		errno = 0;    /* To distinguish success/failure after call */

		val = strtol(str, &endptr, base);

		/* Check for various possible errors */

		if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
                   || (errno != 0 && val == 0)) {

			perror("strtol");
			exit(EXIT_FAILURE);
		}

       if (endptr == str) {

               fprintf(stderr, "No digits were found %s\n", str);
               exit(EXIT_FAILURE);
        }

       /* If we got here, strtol() successfully parsed a number */



           if (*endptr != '\0')        /* Not necessarily an error... */{

               printf("Further characters after number: %s\n", endptr);
               exit(EXIT_FAILURE);
              }


	return val;
}
