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
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	char *cwd, *cp;
	gid_t egid, gid;
	uid_t euid, uid;
	pid_t pgrp, pid, ppid;
	char buf [128];
	long hid;

	cwd = getcwd (buf, 128);
	printf ("cwd = %s\n", cwd);
	egid = getegid ();
	euid = geteuid ();
	gid = getgid ();
	hid = gethostid ();
	printf ("egid = %d, euid = %d, gid = %d, hostid = %d\n", egid, euid, gid, hid);
	gethostname (buf, 128);
	printf ("hostname = %s\n", buf);
	cp = getlogin ();
	printf ("login = %s\n", cp);

	pgrp = getpgrp ();
	pid = getpid ();
	ppid = getppid ();
	uid = getuid ();
	printf ("pgrp = %d, pid = %d, ppid = %d, uid = %d\n", pgrp, pid, ppid, uid);
	return 0;
}
