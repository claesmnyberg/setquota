/*
 * setquota.c 
 *
 * Commandline interface to disk quota on Solaris
 * If no options are given the current quota is printed.
 *
 *  Copyright (c) 2004 Claes M. Nyberg <md0claes@mdstud.chalmers.se>
 *  All rights reserved, all wrongs reversed.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *  THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: setquota.c,v 1.1.1.1 2005-04-08 12:27:13 cmn Exp $
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <ustat.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mnttab.h>
#include <sys/fs/ufs_quota.h>

/* Name of quota file */
#define QUOTAFILE		"quotas"

/* Default mnttab file */
#define DEFAULT_MNTTAB  "/etc/mnttab"

#define DISK_SOFT	0x01
#define DISK_HARD	0x02
#define FILES_SOFT	0x04
#define FILES_HARD	0x08
#define TIME_DISK	0x10
#define TIME_FILES	0x20

/* Local routines */
static void *zmemx(size_t);
static int getquota(uid_t, char *, struct dqblk *);
static int setquota(uid_t uid, char *, struct dqblk *, u_short);
static int strisnum(u_char *, uint32_t *);
static char *get_mount_point(char *, char *);
static int setnum(char *, uint32_t *, uint32_t *);
static void quotaerr(int);



/*
 * Allocate memory or die
 */
void *
zmemx(size_t n)
{
	void *pt;
	if ( (pt = calloc(1, n)) == NULL) {
		perror("** Error: calloc");
		exit(EXIT_FAILURE);
	}
	return(pt);
}

/*
 * Set quota for user
 */
int
setquota(uid_t uid, char *quotafile, struct dqblk *qd, u_short flags)
{
	struct quotctl qctl;
	struct dqblk qds;
	int fd;

	/* Nothing to set */
	if (flags == 0)
		return(0);

	memset(&qctl, 0x00, sizeof(struct quotctl));
	memset(&qds, 0x00, sizeof(struct dqblk));

	if ( (fd = open(quotafile, O_RDWR)) < 0) {
		fprintf(stderr, "** Error: open(%s): %s\n", 
			quotafile, strerror(errno));
		return(-1);
	}

    qctl.op = Q_GETQUOTA;
    qctl.uid = uid;
    qctl.addr = (caddr_t)&qds;

    if (ioctl(fd, Q_QUOTACTL, &qctl) < 0) {
        
		/* Add user if he does not exist */
		if (errno != ESRCH) {
			quotaerr(errno);
        	return(-1);
		}
    }

	if (lseek(fd, 0, SEEK_SET) < 0) {
		perror("** Error: lseek:"); 
		return(-1);
	}

	if (flags & DISK_SOFT) qds.dqb_bsoftlimit = qd->dqb_bsoftlimit;
	if (flags & DISK_HARD) qds.dqb_bhardlimit = qd->dqb_bhardlimit;
	if (flags & FILES_SOFT) qds.dqb_fsoftlimit = qd->dqb_fsoftlimit;
	if (flags & FILES_HARD) qds.dqb_fhardlimit = qd->dqb_fhardlimit;
	if (flags & TIME_DISK) qds.dqb_btimelimit = qd->dqb_btimelimit;
	if (flags & TIME_FILES) qds.dqb_ftimelimit = qd->dqb_ftimelimit;

	qctl.op = Q_SETQUOTA;
	qctl.uid = uid;
	qctl.addr = (caddr_t)&qds;

	errno = 0;
	if (ioctl(fd, Q_QUOTACTL, &qctl) < 0) {
		quotaerr(errno);
		return(-1);
	}

	close(fd);
	return(0);
}

/*
 * Print proper error messages
 */
void
quotaerr(int errno)
{
	char *msg;
	
	if (errno == ESRCH)	
		msg = "User not found in quota file";
	else if (errno == EINVAL)
		msg = "Kernel does not support QUOTA option";
	else if (errno == EPERM)
		msg = "You do not have root privileges";
	else
		msg = strerror(errno);
		
	fprintf(stderr, "** Error: %s\n", msg);
}


/*
 * Get quota for user
 */
int
getquota(uid_t uid, char *quotafile, struct dqblk *qd)
{
	struct quotctl qctl;
	int fd;
	
	memset(&qctl, 0x00, sizeof(struct quotctl));

	if ( (fd = open(quotafile, O_RDONLY)) < 0) {
        fprintf(stderr, "** Error: open(%s): %s\n", 
            quotafile, strerror(errno));
		return(-1);
	}

	qctl.op = Q_GETQUOTA;
	qctl.uid = uid;
	qctl.addr = (caddr_t)qd;
	
	if (ioctl(fd, Q_QUOTACTL, &qctl) < 0) {
		quotaerr(errno);
		return(-1);
	}

	close(fd);
	return(0);
}


/*
 * Check if string is a numeric value.
 * Zero is returned if string contains base-illegal
 * characters, 1 otherwise.
 * Binary value should have the prefix '0b'
 * Octal value should have the prefix '0'.
 * Hexadecimal value should have the prefix '0x'.
 * A string with any digit as prefix except '0' is
 * interpreted as decimal.
 * If val in not NULL, the converted value is stored
 * at that address.
 */
int
strisnum(u_char *str, uint32_t *val)
{
    int base = 0;
    char *endpt;

    if (str == NULL)
        return(0);

    while (isspace((int)*str))
        str++;

    /* Binary value */
    if (!strncmp(str, "0b", 2) || !strncmp(str, "0B", 2)) {
        str += 2;
        base = 2;
    }

    if (*str == '\0')
        return(0);

    if (val == NULL)
        strtoul(str, &endpt, base);
    else
        *val = (uint32_t)strtoul(str, &endpt, base);

    return((int)*endpt == '\0');
}


/*
 * Set a and b to ':' separated integer values in string str.
 * Returns -1 on error, 0 on success.
 */
int
setnum(char *str, uint32_t *a, uint32_t *b)
{
	char *pt;

	if ( (pt = strchr(str, ':')) == NULL) {
		fprintf(stderr, "** Error: Missing deliminator ':'\n");
		return(-1);
	}

	*pt++ = '\0';

	if (!strisnum(str, a)) {
		fprintf(stderr, "** Error: %s is not a numeric value\n", str);
		return(-1);
	}

	if (!strisnum(pt, b)) {
		fprintf(stderr, "** Error: %s is not a numeric value\n", pt);
		return(-1);
	}

	return(0);
}


/*
 * Get mount point of path
 */
char *
get_mount_point(char *path, char *mnttab)
{
	struct stat sb;
	struct ustat ub;
	struct mnttab mp;
	char buf[2048];
	FILE *mf;

	memset(&sb, 0x00, sizeof(sb));
	memset(&ub, 0x00, sizeof(ub));
	memset(buf, 0x00, sizeof(buf));
	
	if (resolvepath(path, buf, sizeof(buf)) < 0) {
		fprintf(stderr, "** Error: resolvepath(%s) %s\n", 
			path, strerror(errno));
		return(NULL);
	}

	/* Get the device id for our path */
	if (stat(buf, &sb) < 0) {
		perror("** Error: stat");
		return(NULL);
	}

	if ( (mf = fopen(mnttab, "r")) == NULL) {
        fprintf(stderr, "** Error: fopen(%s): %s\n", 
            mnttab, strerror(errno));
		return(NULL);
	}

	/* Traverse all entries and search for our device id */
	for (;;) {
		struct stat msb;
		int ret;
	
		memset(&mp, 0x00, sizeof(mp));
		memset(&msb, 0x00, sizeof(msb));
		ret = getmntent(mf, &mp);

		/* EOF */
		if (ret == -1)
			break;

		/* Error */
		if (ret > 0) {
			perror("** Error: getmntent");
			break;
		}

		if (stat(mp.mnt_mountp, &msb) < 0) {
			perror("** Error: stat");
			break;
		}

		/* st_rdev is zero on "top" device */
		if ((msb.st_dev == sb.st_dev) && (msb.st_rdev == 0)) {
			fclose(mf);
			return(strdup(mp.mnt_mountp));
		}
	}

	printf("Could not find mountpoint\n");
	return(NULL);
}


void
usage(char *pname)
{
	printf("\nCommandline interface to disk quota on solaris\n");
	printf("Author: md0claes@mdstud.chalmers.se\n\n");
    printf("Usage: %s user path [option(s)]\n", pname);
	printf("Options:\n");
	printf("  -b soft:hard  Set soft and hard block quota (kilo bytes)\n");
	printf("  -f soft:hard  Set soft and hard file quota (kilo bytes)\n");

/* Not commonly used
 *printf("  -t disk:files Time limit for excessive use\n");
 */
	printf("\n");
}

	
int
main(int argc, char *argv[])
{
	int uid;
	u_short setflags = 0;
	struct dqblk qd;
	struct passwd *pwd;
	char *mnttab = DEFAULT_MNTTAB;
	char *qfile = QUOTAFILE;
	char *path;
	char *mpoint;
	int i;

	memset(&qd, 0x00, sizeof(qd));

	if ((argc < 3) || (argv[2][0] == '-')) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (!strisnum(argv[1], (uint32_t *)&uid)) {
		if ( (pwd = getpwnam(argv[1])) == NULL) {
			fprintf(stderr, "** Error: %s: No such user\n", argv[1]);
			exit(EXIT_FAILURE);	
		}
		uid = pwd->pw_uid;
	}

	if (uid == 0) {
		fprintf(stderr, "** Error: Refusing to set quota for user root\n");
		exit(EXIT_FAILURE);
	}
	path = argv[2];

	argc -= 2;
	argv += 2;
	while ( (i = getopt(argc, argv, "b:f:")) != -1) {
		switch (i) {
			case 'b': 
				if (setnum(optarg, &qd.dqb_bsoftlimit, 
						&qd.dqb_bhardlimit) < 0)
					exit(EXIT_FAILURE);
				
				/* We want kilo bytes */
				qd.dqb_bsoftlimit <<= 1;
				qd.dqb_bhardlimit <<= 1;
				
				setflags |= (DISK_HARD | DISK_SOFT);
				break;
			
			case 'f': 
				if (setnum(optarg, &qd.dqb_fsoftlimit, 
						&qd.dqb_fhardlimit) < 0)
					exit(EXIT_FAILURE);
				setflags |= (FILES_HARD | FILES_SOFT);
				break;
			
			/*
			 *case 't':
			 *	if (setnum(optarg, &qd.dqb_btimelimit,
			 *			&qd.dqb_ftimelimit) < 0)
			 *		exit(EXIT_FAILURE);
			 *	setflags |= (TIME_FILES | TIME_DISK);
			 *	break;
			 */

			default: exit(EXIT_FAILURE);
		}
	}
	
	if ( (mpoint = get_mount_point(path, mnttab)) == NULL)
		exit(EXIT_FAILURE);

	path = zmemx(strlen(mpoint)+strlen(qfile)+5);	
	snprintf(path, strlen(mpoint)+strlen(qfile)+3, "%s/%s", mpoint, qfile);

	if (setflags == 0) {
		if (getquota(uid, path, &qd) < 0)
			exit(EXIT_FAILURE);
		
		printf(" Filesystem: %s\n", mpoint);
		printf("Blocks (KB): usage=%-8u [soft=%-6u hard=%u] %s\n", 
			qd.dqb_curblocks>>1, qd.dqb_bsoftlimit>>1, qd.dqb_bhardlimit>>1,
			qd.dqb_btimelimit ? "** EXCEEDED **": "");
		printf("      Files: usage=%-8u [soft=%-6u hard=%u] %s\n",
			qd.dqb_curfiles, qd.dqb_fsoftlimit, qd.dqb_fhardlimit,
			qd.dqb_ftimelimit ? "** EXCEEDED **": "");

	/*
		if (qd.dqb_btimelimit != 0) 
			printf("** Block quota expires in %u\n", 
				qd.dqb_btimelimit);
	
		if (qd.dqb_ftimelimit != 0) 
			printf("** File quota expires in %u\n", 
				qd.dqb_ftimelimit);
	*/
	}
	else if (setquota(uid, path, &qd, setflags) < 0)
		exit(EXIT_FAILURE);

	free(mpoint);
	free(path);
	exit(EXIT_SUCCESS);
}
