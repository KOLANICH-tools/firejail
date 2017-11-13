/*
 * Copyright (C) 2014-2017 Firejail Authors
 *
 * This file is part of firejail project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "firejail.h"
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>


void check_netfilter_file(const char *fname) {
	EUID_ASSERT();

	char *tmp = strdup(fname);
	if (!tmp)
		errExit("strdup");
	char *ptr = strchr(tmp, ',');
	if (ptr)
		*ptr = '\0';

	invalid_filename(tmp, 0); // no globbing

	if (is_dir(tmp) || is_link(tmp) || strstr(tmp, "..") || access(tmp, R_OK )) {
		fprintf(stderr, "Error: invalid network filter file %s\n", tmp);
		exit(1);
	}
	free(tmp);
}


void netfilter(const char *fname) {
	// find iptables command
	struct stat s;
	char *iptables = NULL;
	char *iptables_restore = NULL;
	if (stat("/sbin/iptables", &s) == 0) {
		iptables = "/sbin/iptables";
		iptables_restore = "/sbin/iptables-restore";
	}
	else if (stat("/usr/sbin/iptables", &s) == 0) {
		iptables = "/usr/sbin/iptables";
		iptables_restore = "/usr/sbin/iptables-restore";
	}
	if (iptables == NULL || iptables_restore == NULL) {
		fprintf(stderr, "Error: iptables command not found, netfilter not configured\n");
		return;
	}

	// create an empty user-owned SBOX_STDIN_FILE
	create_empty_file_as_root(SBOX_STDIN_FILE, 0644);
	if (set_perms(SBOX_STDIN_FILE, getuid(), getgid(), 0644))
		errExit("set_perms");

	if (fname == NULL)
		sbox_run(SBOX_USER| SBOX_CAPS_NONE | SBOX_SECCOMP, 2, PATH_FNETFILTER, SBOX_STDIN_FILE);
	else
		sbox_run(SBOX_USER| SBOX_CAPS_NONE | SBOX_SECCOMP, 3, PATH_FNETFILTER, fname, SBOX_STDIN_FILE);

	// first run of iptables on this platform installs a number of kernel modules such as ip_tables, x_tables, iptable_filter
	// we run this command with caps and seccomp disabled in order to allow the loading of these modules
	sbox_run(SBOX_ROOT | SBOX_STDIN_FROM_FILE, 1, iptables_restore);
	unlink(SBOX_STDIN_FILE);

	// debug
	if (arg_debug)
		sbox_run(SBOX_ROOT | SBOX_CAPS_NETWORK | SBOX_SECCOMP, 2, iptables, "-vL");

	return;
}

void netfilter6(const char *fname) {
	if (fname == NULL)
		return;

	// find iptables command
	char *ip6tables = NULL;
	char *ip6tables_restore = NULL;
	struct stat s;
	if (stat("/sbin/ip6tables", &s) == 0) {
		ip6tables = "/sbin/ip6tables";
		ip6tables_restore = "/sbin/ip6tables-restore";
	}
	else if (stat("/usr/sbin/ip6tables", &s) == 0) {
		ip6tables = "/usr/sbin/ip6tables";
		ip6tables_restore = "/usr/sbin/ip6tables-restore";
	}
	if (ip6tables == NULL || ip6tables_restore == NULL) {
		fprintf(stderr, "Error: ip6tables command not found, netfilter6 not configured\n");
		return;
	}

	// create the filter file
	char *filter = read_text_file_or_exit(fname);
	FILE *fp = fopen(SBOX_STDIN_FILE, "w");
	if (!fp) {
		fprintf(stderr, "Error: cannot open %s\n", SBOX_STDIN_FILE);
		exit(1);
	}
	fprintf(fp, "%s\n", filter);
	fclose(fp);

	// push filter
	if (arg_debug)
		printf("Installing network filter:\n%s\n", filter);

	// first run of iptables on this platform installs a number of kernel modules such as ip_tables, x_tables, iptable_filter
	// we run this command with caps and seccomp disabled in order to allow the loading of these modules
	sbox_run(SBOX_ROOT | SBOX_STDIN_FROM_FILE, 1, ip6tables_restore);
	unlink(SBOX_STDIN_FILE);

	// debug
	if (arg_debug)
		sbox_run(SBOX_ROOT | SBOX_CAPS_NETWORK | SBOX_SECCOMP, 2, ip6tables, "-vL");

	free(filter);
	return;
}
