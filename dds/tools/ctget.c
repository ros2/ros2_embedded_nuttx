/*
 * Copyright (c) 2014 - Qeo LLC
 *
 * The source code form of this Qeo Open Source Project component is subject
 * to the terms of the Clear BSD license.
 *
 * You can redistribute it and/or modify it under the terms of the Clear BSD
 * License (http://directory.fsf.org/wiki/License:ClearBSD). See LICENSE file
 * for more details.
 *
 * The Qeo Open Source Project also includes third party Open Source Software.
 * See LICENSE file for more details.
 */

#include <stdio.h>

/**********************************************************************/
/*   Function to read a file, ignoring blank lines and comments.      */
/**********************************************************************/
int read_file (char *filename, char *label, int y)
{
	char	lbuf [BUFSIZ];
	char	time [20];
	FILE	*fp;
	int	line_no = 0;
	int	len, i, j;
	char	*cp;

	if ((fp = fopen (filename, "r")) == (FILE *) NULL) {
		perror (filename);
		return -1;
	}
	while (!feof (fp)) {
		line_no++;
		if (fgets (lbuf, sizeof lbuf, fp) == (char *) NULL)
			break;

		if (line_no <= 3)
			continue;

		len = strlen (lbuf);
		while (len > 0 && (lbuf [len - 1] == '\r' ||
		                   lbuf [len - 1] == '\n'))
			lbuf [--len] = '\0';

		if (!len || lbuf [0] == '\t' || !isdigit (lbuf [0]))
			continue;

		i = 0;
		while (lbuf [i] == '0' && lbuf [i + 1] != '.')
			i++;
		j = 0;
		while (isdigit (lbuf [i]) || lbuf [i] == '.' || lbuf [i] == ',') {
			if (lbuf [i] != ',')
				time [j++] = lbuf [i];
			i++;
		}
		time [j] = '\0';
		for (cp = &lbuf [i]; isspace (*cp) || *cp == '-'; cp++)
			;
		j = 0;
		while (*cp == ':' || isalnum (*cp) || *cp == '_')
			if (label [j++] != *cp++)
				break;

		if (isspace (*cp) && !label [j])

		/***********************************************/
		/*   Process line here.                        */
		/***********************************************/

		printf ("%s  %d\r\n", time, y);
	}
	fclose (fp);
	return 0;
}

int main (int argc, char **argv)
{
	if (argc != 4) {
		printf ("%s -- data extractor tool for saved cyclic trace files.\n", argv [0]);
		printf ("Usage: %s <ctfile> <label> <y>\n", argv [0]);
		printf ("Where: <ctfile> is the name of a cyclic trace file and\r\n");
		printf ("       <label> is a trace label, and\n");
		printf ("       <y> is the y-coordinate of the output for that label.\n");
		printf ("The output is printed to stdout, so you probably need to redirect it.\n");
		exit (1);
	}
	read_file (argv [1], argv [2], atoi (argv [3]));
}
