/*
	platform.c:	platform-dependent implementation of common
			functions, to simplify porting.
									*/
/*	Copyright (c) 1997, California Institute of Technology.		*/
/*	ALL RIGHTS RESERVED. U.S. Government Sponsorship		*/
/*	acknowledged.							*/
/*									*/
/*	Author: Scott Burleigh, Jet Propulsion Laboratory		*/
/*									*/
#include "platform.h"

#if defined (VXWORKS)

typedef struct rlock_str
{
	SEM_ID	semaphore;
	int	owner;
	short	count;
	short	init;
} Rlock;		/*	Private-memory semaphore.		*/ 

int	createFile(const char *filename, int flags)
{
	int	result;

	/*	VxWorks open(2) will only create a file on an NFS
	 *	network device.  The only portable flag values are
	 *	O_WRONLY and O_RDWR.  See creat(2) and open(2).		*/

	result = creat(filename, flags);
	if (result < 0)
	{
		putSysErrmsg("can't create file", filename);
	}

	return result;
}

int	initResourceLock(ResourceLock *rl)
{
	Rlock	*lock = (Rlock *) rl;

	CHKERR(rl);
	if (lock->init)
	{
		return 0;
	}

	lock->semaphore = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
	if (lock->semaphore == NULL)
	{
		writeErrMemo("Can't create lock semaphore");
		return -1;
	}

	lock->owner = NONE;
	lock->count = 0;
	lock->init = 1;
	return 0;
}

void	killResourceLock(ResourceLock *rl)
{
	Rlock	*lock = (Rlock *) rl;

	if (lock && lock->init && lock->count == 0)
	{
		oK(semDelete(lock->semaphore));
		lock->semaphore = NULL;
		lock->init = 0;
	}
}

void	lockResource(ResourceLock *rl)
{
	Rlock	*lock = (Rlock *) rl;
	int	tid;

	if (lock && lock->init)
	{
		tid = taskIdSelf();
		if (tid != lock->owner)
		{
			oK(semTake(lock->semaphore, WAIT_FOREVER));
			lock->owner = tid;
		}

		(lock->count)++;
	}
}

void	unlockResource(ResourceLock *rl)
{
	Rlock	*lock = (Rlock *) rl;
	int	tid;

	if (lock && lock->init)
	{
		tid = taskIdSelf();
		if (tid == lock->owner)
		{
			(lock->count)--;
			if (lock->count == 0)
			{
				lock->owner = NONE;
				oK(semGive(lock->semaphore));
			}
		}
	}
}

void	closeOnExec(int fd)
{
	return;		/*	N/A for non-Unix operating system.	*/
}

void	snooze(unsigned int seconds)
{
	struct timespec	ts;

	ts.tv_sec = seconds;
	ts.tv_nsec = 0;
	oK(nanosleep(&ts, NULL));
}

void	microsnooze(unsigned int usec)
{
	struct timespec	ts;

	ts.tv_sec = usec / 1000000;
	ts.tv_nsec = (usec % 1000000) * 1000;
	oK(nanosleep(&ts, NULL));
}

char	*system_error_msg()
{
	return strerror(errno);
}

#ifndef VXWORKS6
int	getpid()
{
	return taskIdSelf();
}
#endif

int	gettimeofday(struct timeval *tvp, void *tzp)
{
	/*	Note: we now use clock_gettime() for this purpose
	 *	instead of tickGet(), etc.  Although this requires
	 *	that clock_settime() be called during ION startup
	 *	on a VxWorks platform, the result is more reliably
	 *	accurate.  The older implementation is retained
	 *	for information purposes only.				*/
#if 0
	unsigned long	tickCount;
	int		ticksPerSec;
#endif
	struct timespec	cur_time;

	CHKERR(tvp);
#if 0
	tickCount = tickGet();
	ticksPerSec = sysClkRateGet();
	tvp->tv_sec = tickCount / ticksPerSec;
	tvp->tv_usec = ((tickCount % ticksPerSec) * 1000000) / ticksPerSec;
#endif
	/*	Use the internal POSIX timer.				*/

	clock_gettime(CLOCK_REALTIME, &cur_time);
	tvp->tv_sec = cur_time.tv_sec;
	tvp->tv_usec = cur_time.tv_nsec / 1000;
	return 0;
}

void	getCurrentTime(struct timeval *tvp)
{
	gettimeofday(tvp, NULL);
}

unsigned long	getClockResolution()
{
	struct timespec	ts;

	clock_getres(CLOCK_REALTIME, &ts);
	return ts.tv_nsec / 1000;
}

#ifndef ION_NO_DNS
unsigned int	getInternetAddress(char *hostName)
{
	int	hostNbr;

	CHKZERO(hostName);
	hostNbr = hostGetByName(hostName);
	if (hostNbr == ERROR)
	{
		putSysErrmsg("can't get address for host", hostName);
		return BAD_HOST_NAME;
	}

	return (unsigned int) ntohl(hostNbr);
}

char	*getInternetHostName(unsigned int hostNbr, char *buffer)
{
	CHKNULL(buffer);
	if (hostGetByAddr((int) hostNbr, buffer) < 0)
	{
		putSysErrmsg("can't get name for host", utoa(hostNbr));
		return NULL;
	}

	return buffer;
}

int	getNameOfHost(char *buffer, int bufferLength)
{
	int	result;

	CHKERR(buffer);
	result = gethostname(buffer, bufferLength);
	if (result < 0)
	{
		putSysErrmsg("can't get local host name", NULL);
	}

	return result;
}

char	*getNameOfUser(char *buffer)
{
	CHKNULL(buffer);
	remCurIdGet(buffer, NULL);
	return buffer;
}

int	reUseAddress(int fd)
{
	int	result;
	int	i = 1;

	result = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &i,
			sizeof i);
	if (result < 0)
	{
		putSysErrmsg("can't make socket's address reusable", NULL);
	}

	return result;
}

int	watchSocket(int fd)
{
	int		result;
	struct linger	lctrl = {0, 0};
	int		kctrl = 1;

	result = setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &lctrl,
			sizeof lctrl);
	if (result < 0)
	{
		putSysErrmsg("can't set linger on socket", NULL);
		return result;
	}

	result = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &kctrl,
			sizeof kctrl);
	if (result < 0)
	{
		putSysErrmsg("can't set keepalive on socket", NULL);
	}

	return result;
}
#endif

int	makeIoNonBlocking(int fd)
{
	int	result;
	int	setting = 1;

	result = ioctl(fd, FIONBIO, (int) &setting);
	if (result < 0)
	{
		putSysErrmsg("can't make IO non-blocking", NULL);
	}

	return result;
}

int	strcasecmp(const char *s1, const char *s2)
{
	register int c1, c2;

	CHKZERO(s1);
	CHKZERO(s2);
	for ( ; ; )
	{
		/* STDC requires tolower(3) to work for all
		** ints. Unfortunately, not all C's are STDC.
		*/
		c1 = *s1; if (isupper(c1)) c1 = tolower(c1);
		c2 = *s2; if (isupper(c2)) c2 = tolower(c2);
		if (c1 != c2) return c1 - c2;
		if (c1 == 0) return 0;
		++s1; ++s2;
	}
}

int	strncasecmp(const char *s1, const char *s2, size_t n)
{
	register int c1, c2;

	CHKZERO(s1);
	CHKZERO(s2);
	for ( ; n > 0; --n)
	{
		/* STDC requires tolower(3) to work for all
		** ints. Unfortunately, not all C's are STDC.
		*/
		c1 = *s1; if (isupper(c1)) c1 = tolower(c1);
		c2 = *s2; if (isupper(c2)) c2 = tolower(c2);
		if (c1 != c2) return c1 - c2;
		if (c1 == 0) return 0;
		++s1; ++s2;
	}

	return 0;
}

#else				/*	Not VxWorks.			*/

#if defined (darwin) || defined (freebsd)

void	*memalign(size_t boundary, size_t size)
{
	return malloc(size);
}

#endif

int	createFile(const char *filename, int flags)
{
	int	result;

	/*	POSIX-UNIX creat(2) will only create a file for
	 *	writing.  The only portable flag values are
	 *	O_WRONLY and O_RDWR.  See creat(2) and open(2).		*/

	result = open(filename, (flags | O_CREAT | O_TRUNC), 0666);
	if (result < 0)
	{
		putSysErrmsg("can't create file", filename);
	}

	return result;
}

#ifdef _MULTITHREADED

typedef struct rlock_str
{
	pthread_mutex_t	semaphore;
	pthread_t	owner;
	short		count;
	short		init;
} Rlock;		/*	Private-memory semaphore.		*/ 

int	initResourceLock(ResourceLock *rl)
{
	Rlock	*lock = (Rlock *) rl;

	CHKERR(rl);
	if (lock->init)
	{
		return 0;
	}

	memset((char *) &(lock->semaphore), 0, sizeof lock->semaphore);
	if (pthread_mutex_init(&(lock->semaphore), NULL))
	{
		writeErrMemo("Can't create lock semaphore");
		return -1;
	}

	lock->owner = NONE;
	lock->count = 0;
	lock->init = 1;
	return 0;
}

void	killResourceLock(ResourceLock *rl)
{
	Rlock	*lock = (Rlock *) rl;

	if (lock && lock->init && lock->count == 0)
	{
		oK(pthread_mutex_destroy(&(lock->semaphore)));
		lock->init = 0;
	}
}

void	lockResource(ResourceLock *rl)
{
	Rlock		*lock = (Rlock *) rl;
	pthread_t	tid;

	if (lock && lock->init)
	{
		tid = pthread_self();
		if (tid != lock->owner)
		{
			oK(pthread_mutex_lock(&(lock->semaphore)));
			lock->owner = tid;
		}

		(lock->count)++;
	}
}

void	unlockResource(ResourceLock *rl)
{
	Rlock		*lock = (Rlock *) rl;
	pthread_t	tid;

	if (lock && lock->init)
	{
		tid = pthread_self();
		if (tid == lock->owner)
		{
			(lock->count)--;
			if ((lock->count) == 0)
			{
				lock->owner = NONE;
				oK(pthread_mutex_unlock(&(lock->semaphore)));
			}
		}
	}
}

#else	/*	Only one thread of control in address space.		*/

int	initResourceLock(ResourceLock *rl)
{
	return 0;
}

void	killResourceLock(ResourceLock *rl)
{
	return;
}

void	lockResource(ResourceLock *rl)
{
	return;
}

void	unlockResource(ResourceLock *rl)
{
	return;
}

#endif

#if (!defined (linux) && !defined (freebsd) && !defined (cygwin) && !defined (darwin) && !defined (RTEMS)) 
/*	These things are defined elsewhere for Linux-like op systems.	*/

extern int	sys_nerr;
extern char	*sys_errlist[];

char	*system_error_msg()
{
	if (errno > sys_nerr)
	{
		return "cause unknown";
	}

	return sys_errlist[errno];
}

char	*getNameOfUser(char *buffer)
{
	return cuserid(buffer);
}

#endif	/*	end #if (!defined(linux, cygwin, darwin, RTEMS))	*/

void	closeOnExec(int fd)
{
	fcntl(fd, F_SETFD, FD_CLOEXEC);
}

#ifdef cygwin	/*	select may be slower but 1.3 lacks nanosleep.	*/

void	snooze(unsigned int seconds)
{
	struct timeval	tv;
	fd_set		rfds;
	fd_set		wfds;
	fd_set		xfds;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&xfds);
	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	oK(select(0, &rfds, &wfds, &xfds, &tv));
}

void	microsnooze(unsigned int usec)
{
	struct timeval	tv;
	fd_set		rfds;
	fd_set		wfds;
	fd_set		xfds;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&xfds);
	tv.tv_sec = usec / 1000000;
	tv.tv_usec = usec % 1000000;
	oK(select(0, &rfds, &wfds, &xfds, &tv));
}

#else

#if defined (interix)

void	snooze(unsigned int seconds)
{
	oK(sleep(seconds));
}

void	microsnooze(unsigned int usec)
{
	unsigned int	seconds = usec / 1000000;

	if (seconds > 0) seconds = sleep(seconds);
	usec = usec % 1000000;
	oK(usleep(usec));
}

#else		/*	Not Cygwin or Interix; nanosleep is supported.	*/

void	snooze(unsigned int seconds)
{
	struct timespec	ts;

	ts.tv_sec = seconds;
	ts.tv_nsec = 0;
	oK(nanosleep(&ts, NULL));
}

void	microsnooze(unsigned int usec)
{
	struct timespec	ts;

	ts.tv_sec = usec / 1000000;
	ts.tv_nsec = (usec % 1000000) * 1000;
	oK(nanosleep(&ts, NULL));
}

#endif		/*	end #ifdef interix				*/
#endif		/*	end #ifdef cygwin				*/

void	getCurrentTime(struct timeval *tvp)
{
	CHKVOID(tvp);
	gettimeofday(tvp, NULL);
}

unsigned long	getClockResolution()
{
	/*	Linux clock resolution of Alpha is 1 ms, as is
	 *	Windows XP standard clock resolution, and Solaris
	 *	clock resolution can be configured.  But minimum
	 *	clock resolution in all cases appears to be 10 ms,
	 *	so we use that value since it seems likely to be
	 *	safe in all cases.					*/

	return 10000;
}

#if defined (__SVR4)

int	getNameOfHost(char *buffer, int bufferLength)
{
	struct utsname	name;

	CHKERR(buffer);
	CHKERR(bufferLength > 0);
	if (uname(&name) < 0)
	{
		*buffer = '\0';
		putSysErrmsg("can't get local host name", NULL);
		return -1;
	}

	strncpy(buffer, name.nodename, bufferLength - 1);
	*(buffer + bufferLength - 1) = '\0';
	return 0;
}

int	makeIoNonBlocking(int fd)
{
	int	result;

	result = fcntl(fd, F_SETFL, O_NDELAY);
	if (result < 0)
	{
		putSysErrmsg("can't make IO non-blocking", NULL);
	}

	return result;
}

#if defined (_REENTRANT)	/*	SVR4 multithreaded.		*/

unsigned int	getInternetAddress(char *hostName)
{
	struct hostent	hostInfoBuffer;
	struct hostent	*hostInfo;
	unsigned int	hostInetAddress;
	char		textBuffer[1024];
	int		hostInfoErrno = -1;

	CHKZERO(hostName);
	hostInfo = gethostbyname_r(hostName, &hostInfoBuffer, textBuffer,
			sizeof textBuffer, &hostInfoErrno);
	if (hostInfo == NULL)
	{
		putSysErrmsg("can't get host info", hostName);
		return BAD_HOST_NAME;
	}

	if (hostInfo->h_length != sizeof hostInetAddress)
	{
		putErrmsg("Address length invalid in host info.", hostName);
		return BAD_HOST_NAME;
	}

	memcpy((char *) &hostInetAddress, hostInfo->h_addr, 4);
	return ntohl(hostInetAddress);
}

char	*getInternetHostName(unsigned int hostNbr, char *buffer)
{
	struct hostent	hostInfoBuffer;
	struct hostent	*hostInfo;
	char		textBuffer[128];
	int		hostInfoErrno;

	CHKNULL(buffer);
	hostNbr = htonl(hostNbr);
	hostInfo = gethostbyaddr_r((char *) &hostNbr, sizeof hostNbr, AF_INET,
			&hostInfoBuffer, textBuffer, sizeof textBuffer,
			&hostInfoErrno);
	if (hostInfo == NULL)
	{
		putSysErrmsg("can't get host info", utoa(hostNbr));
		return NULL;
	}

	strncpy(buffer, hostInfo->h_name, MAXHOSTNAMELEN);
	return buffer;
}

int	reUseAddress(int fd)
{
	int	result;
	int	i = 1;

	result = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &i,
			sizeof i);
	if (result < 0)
	{
		putSysErrmsg("can't make socket address reusable", NULL);
	}

	return result;
}

int	watchSocket(int fd)
{
	int		result;
	struct linger	lctrl = {0, 0};
	int		kctrl = 1;

	result = setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &lctrl,
			sizeof lctrl);
	if (result < 0)
	{
		putSysErrmsg("can't set linger on socket", NULL);
		return result;
	}

	result = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &kctrl,
			sizeof kctrl);
	if (result < 0)
	{
		putSysErrmsg("can't set keepalive on socket", NULL);
	}

	return result;
}

#else				/*	SVR4 but not multithreaded.	*/

unsigned int	getInternetAddress(char *hostName)
{
	struct hostent	*hostInfo;
	unsigned int	hostInetAddress;

	CHKZERO(hostName);
	hostInfo = gethostbyname(hostName);
	if (hostInfo == NULL)
	{
		putSysErrmsg("can't get host info", hostName);
		return BAD_HOST_NAME;
	}

	if (hostInfo->h_length != sizeof hostInetAddress)
	{
		putErrmsg("Address length invalid in host info.", hostName);
		return BAD_HOST_NAME;
	}

	memcpy((char *) &hostInetAddress, hostInfo->h_addr, 4);
	return ntohl(hostInetAddress);
}

char	*getInternetHostName(unsigned int hostNbr, char *buffer)
{
	struct hostent	*hostInfo;

	CHKNULL(buffer);
	hostNbr = htonl(hostNbr);
	hostInfo = gethostbyaddr((char *) &hostNbr, sizeof hostNbr, AF_INET);
	if (hostInfo == NULL)
	{
		putSysErrmsg("can't get host info", utoa(hostNbr));
		return NULL;
	}

	strncpy(buffer, hostInfo->h_name, MAXHOSTNAMELEN);
	return buffer;
}

int	reUseAddress(int fd)
{
	int	result;
	int	i = 1;

	result = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);
	if (result < 0)
	{
		putSysErrmsg("can't make socket address reusable", NULL);
	}

	return result;
}

int	watchSocket(int fd)
{
	int		result;
	struct linger	lctrl = {0, 0};
	int		kctrl = 1;

	result = setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *) &lctrl,
			sizeof lctrl);
	if (result < 0)
	{
		putSysErrmsg("can't set linger on socket", NULL);
		return result;
	}

	result = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *) &kctrl,
			sizeof kctrl);
	if (result < 0)
	{
		putSysErrmsg("can't set keepalive on socket", NULL);
	}

	return result;
}

#endif				/*	End of _REENTRANT subtree.	*/

#else				/*	Neither VxWorks nor SVR4.	*/

#if defined (sun)		/*	(Meaning SunOS 4; a BSD Unix.)	*/

unsigned int	getInternetAddress(char *hostName)
{
	struct hostent	*hostInfo;
	unsigned int	hostInetAddress;

	CHKZERO(hostName);
	hostInfo = gethostbyname(hostName);
	if (hostInfo == NULL)
	{
		putSysErrmsg("can't get host info", hostName);
		return BAD_HOST_NAME;
	}

	if (hostInfo->h_length != sizeof hostInetAddress)
	{
		putErrmsg("Address length invalid in host info.", hostName);
		return BAD_HOST_NAME;
	}

	memcpy((char *) &hostInetAddress, hostInfo->h_addr, 4);
	return ntohl(hostInetAddress);
}

char	*getInternetHostName(unsigned int hostNbr, char *buffer)
{
	struct hostent	*hostInfo;

	CHKNULL(buffer);
	hostNbr = htonl(hostNbr);
	hostInfo = gethostbyaddr((char *) &hostNbr, sizeof hostNbr, AF_INET);
	if (hostInfo == NULL)
	{
		putSysErrmsg("can't get host info", utoa(hostNbr));
		return NULL;
	}

	strncpy(buffer, hostInfo->h_name, MAXHOSTNAMELEN);
	return buffer;
}

int	getNameOfHost(char *buffer, int bufferLength)
{
	int	result;

	CHKERR(buffer);
	CHKERR(bufferLength > 0);
	result = gethostname(buffer, bufferLength);
	if (result < 0)
	{
		putSysErrmsg("can't get local host name", NULL);
	}

	return result;
}

int	reUseAddress(int fd)
{
	int	result;
	int	i = 1;

	result = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &i,
			sizeof i);
	if (result < 0)
	{
		putSysErrmsg("can't make socket address reusable", NULL);
	}

	return result;
}
 
int	makeIoNonBlocking(int fd)
{
	int	result;
	int	setting = 1;
 
	result = ioctl(fd, FIONBIO, &setting);
	if (result < 0)
	{
		putSysErrmsg("can't make IO non-blocking", NULL);
	}

	return result;
}

int	watchSocket(int fd)
{
	int		result;
	struct linger	lctrl = {0, 0};
	int		kctrl = 1;

	result = setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &lctrl,
			sizeof lctrl);
	if (result < 0)
	{
		putSysErrmsg("can't set linger on socket", NULL);
		return result;
	}

	result = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &kctrl,
			sizeof kctrl);
	if (result < 0)
	{
		putSysErrmsg("can't set keepalive on socket", NULL);
	}

	return result;
}

#else

#if (defined (linux) || defined (freebsd) || defined (cygwin) || defined (darwin) || defined (RTEMS))

char	*system_error_msg()
{
	return strerror(errno);
}

char	*getNameOfUser(char *buffer)
{
	uid_t		euid;
	struct passwd	*pwd;

	euid = geteuid();
	pwd = getpwuid(euid);
	if (pwd)
	{
		return pwd->pw_name;
	}

	return "";
}

unsigned int	getInternetAddress(char *hostName)
{
	struct hostent	*hostInfo;
	unsigned int	hostInetAddress;

	CHKZERO(hostName);
	hostInfo = gethostbyname(hostName);
	if (hostInfo == NULL)
	{
		putSysErrmsg("can't get host info", hostName);
		return BAD_HOST_NAME;
	}

	if (hostInfo->h_length != sizeof hostInetAddress)
	{
		putErrmsg("Address length invalid in host info.", hostName);
		return BAD_HOST_NAME;
	}

	memcpy((char *) &hostInetAddress, hostInfo->h_addr, 4);
	return ntohl(hostInetAddress);
}

char	*getInternetHostName(unsigned int hostNbr, char *buffer)
{
	struct hostent	*hostInfo;

	CHKNULL(buffer);
	hostNbr = htonl(hostNbr);
	hostInfo = gethostbyaddr((char *) &hostNbr, sizeof hostNbr, AF_INET);
	if (hostInfo == NULL)
	{
		putSysErrmsg("can't get host info", utoa(hostNbr));
		return NULL;
	}

	strncpy(buffer, hostInfo->h_name, MAXHOSTNAMELEN);
	return buffer;
}

int	getNameOfHost(char *buffer, int bufferLength)
{
	int	result;

	CHKERR(buffer);
	CHKERR(bufferLength > 0);
	result = gethostname(buffer, bufferLength);
	if (result < 0)
	{
		putSysErrmsg("can't local host name", NULL);
	}

	return result;
}

int	reUseAddress(int fd)
{
	int	result;
	int	i = 1;

	result = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *) &i,
			sizeof i);
	if (result < 0)
	{
		putSysErrmsg("can't make socket address reusable", NULL);
	}

	return result;
}
 
int	makeIoNonBlocking(int fd)
{
	int	result;
	int	setting = 1;
 
	result = ioctl(fd, FIONBIO, &setting);
	if (result < 0)
	{
		putSysErrmsg("can't make IO non-blocking", NULL);
	}

	return result;
}

int	watchSocket(int fd)
{
	int		result;
	struct linger	lctrl = {0, 0};
	int		kctrl = 1;

	result = setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *) &lctrl,
			sizeof lctrl);
	if (result < 0)
	{
		putSysErrmsg("can't set linger on socket", NULL);
		return result;
	}

	result = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *) &kctrl,
			sizeof kctrl);
	if (result < 0)
	{
		putSysErrmsg("can't set keepalive on socket", NULL);
	}

	return result;
}

#else				/*	Unknown platform.		*/

#error "Can't determine operating system to compile for."

#endif				/*	End of #if defined (linux-like)	*/
#endif				/*	End of #if defined (sun)	*/
#endif				/*	End of #if defined (__SVR4)	*/
#endif				/*	End of #if defined (VXWORKS)	*/

/******************* platform-independent functions *********************/

void	*acquireSystemMemory(size_t size)
{
	void	*block;

	if (size <= 0)
	{
		return NULL;
	}

	size = size + ((sizeof(void *)) - (size % (sizeof(void *))));
#if defined (RTEMS)
	block = malloc(size);	/*	try posix_memalign?		*/
#else
	block = memalign(sizeof(void *), size);
#endif
	if (block)
	{
		memset((char *) block, 0, size);
	}
	else
	{
		putSysErrmsg("Memory allocation failed", itoa(size));
	}

	return block;
}

static void	logToStdout(char *text)
{
	if (text)
	{
		fprintf(stdout, "%s\n", text);
		fflush(stdout);
	}
}

static Logger	_logOneMessage(Logger *logFunction)
{
	static Logger	logger = logToStdout;

	if (logFunction == NULL)
	{
		return logger;
	}

	logger = *logFunction;
	return NULL;
}

void	setLogger(Logger logFunction)
{
	if (logFunction)
	{
		oK(_logOneMessage(&logFunction));
	}
}

void	writeMemo(char *text)
{
	if (text)
	{
		(_logOneMessage(NULL))(text);
	}
}

void	writeMemoNote(char *text, char *note)
{
	char	*noteText = note ? note : "";
	char	textBuffer[1024];

	if (text)
	{
		isprintf(textBuffer, sizeof textBuffer, "%.900s: %.64s",
				text, noteText);
		(_logOneMessage(NULL))(textBuffer);
	}
}

void	writeErrMemo(char *text)
{
	writeMemoNote(text, system_error_msg());
}

char	*itoa(int arg)
{
	static char	itoa_str[32];

	isprintf(itoa_str, sizeof itoa_str, "%d", arg);
	return itoa_str;
}

char	*utoa(unsigned int arg)
{
	static char	utoa_str[32];

	isprintf(utoa_str, sizeof utoa_str, "%u", arg);
	return utoa_str;
}

static int	_errmsgs(int lineNbr, const char *fileName, const char *text,
			const char *arg, char *buffer)
{
	static char		errmsgs[ERRMSGS_BUFSIZE];
	static int		errmsgsLength = 0;
	static ResourceLock	errmsgsLock;
	int			msgLength;
	int			spaceFreed;
	char			lineNbrBuffer[32];
	int			spaceAvbl;
	int			spaceForText;
	int			spaceNeeded;

	if (initResourceLock(&errmsgsLock) < 0)
	{
		return 0;
	}

	if (buffer)		/*	Retrieving an errmsg.		*/
	{
		lockResource(&errmsgsLock);
		msgLength = strlen(errmsgs);
		if (msgLength == 0)	/*	No more msgs in pool.	*/
		{
			unlockResource(&errmsgsLock);
			return msgLength;
		}

		/*	Getting a message removes it from the pool,
		 *	releasing space for more messages.		*/

		spaceFreed = msgLength + 1;	/*	incl. last NULL	*/
		memcpy(buffer, errmsgs, spaceFreed);
		errmsgsLength -= spaceFreed;
		memcpy(errmsgs, errmsgs + spaceFreed, errmsgsLength);
		memset(errmsgs + errmsgsLength, 0, spaceFreed);
		unlockResource(&errmsgsLock);
		return msgLength;
	}

	/*	Posting an errmsg.					*/

	if (fileName == NULL || text == NULL || *text == '\0')
	{
		return 0;	/*	Ignored.			*/
	}

	lockResource(&errmsgsLock);
	isprintf(lineNbrBuffer, sizeof lineNbrBuffer, "%d", lineNbr);
	spaceAvbl = ERRMSGS_BUFSIZE - errmsgsLength;
	spaceForText = 8 + strlen(lineNbrBuffer) + 4 + strlen(fileName)
			+ 2 + strlen(text);
	spaceNeeded = spaceForText + 1;
	if (arg)
	{
		spaceNeeded += (2 + strlen(arg) + 1);
	}

	if (spaceNeeded > spaceAvbl)	/*	Can't record message.	*/
	{
		if (spaceAvbl < 2)
		{
			/*	Can't even note that it was omitted.	*/

			spaceNeeded = 0;
		}
		else
		{
			/*	Write a single newline message to
			 *	note that this message was omitted.	*/

			spaceNeeded = 2;
			errmsgs[errmsgsLength] = '\n';
			errmsgs[errmsgsLength + 1] = '\0';
		}
	}
	else
	{
		isprintf(errmsgs + errmsgsLength, spaceAvbl,
			"at line %s of %s, %s", lineNbrBuffer, fileName, text);
		if (arg)
		{
			isprintf(errmsgs + errmsgsLength + spaceForText,
				spaceAvbl - spaceForText, " (%s)", arg);
		}
	}

	errmsgsLength += spaceNeeded;
	unlockResource(&errmsgsLock);
	return 0;
}

void	_postErrmsg(const char *fileName, int lineNbr, const char *text,
		const char *arg)
{
	oK(_errmsgs(lineNbr, fileName, text, arg, NULL));
}

void	_putErrmsg(const char *fileName, int lineNbr, const char *text,
		const char *arg)
{
	_postErrmsg(fileName, lineNbr, text, arg);
	writeErrmsgMemos();
}

void	_postSysErrmsg(const char *fileName, int lineNbr, const char *text,
		const char *arg)
{
	char	*sysmsg;
	int	textLength;
	int	maxTextLength;
	char	textBuffer[1024];

	if (text)
	{
		textLength = strlen(text);
		sysmsg = system_error_msg();
		maxTextLength = sizeof textBuffer - (2 + strlen(sysmsg) + 1);
		if (textLength > maxTextLength)
		{
			textLength = maxTextLength;
		}

		isprintf(textBuffer, sizeof textBuffer, "%.*s: %s",
				textLength, text, sysmsg);
		_postErrmsg(fileName, lineNbr, textBuffer, arg);
	}
}

void	_putSysErrmsg(const char *fileName, int lineNbr, const char *text,
		const char *arg)
{
	_postSysErrmsg(fileName, lineNbr, text, arg);
	writeErrmsgMemos();
}

int	getErrmsg(char *buffer)
{
	CHKZERO(buffer);
	return _errmsgs(0, NULL, NULL, NULL, buffer);
}

void	writeErrmsgMemos()
{
	static ResourceLock	memosLock;
	static char		msgwritebuf[ERRMSGS_BUFSIZE];
	static char		*omissionMsg = "[?] message omitted due to \
excessive length";

	/*	Because buffer is static, it is shared.  So access
	 *	to it must be mutexed.					*/

	if (initResourceLock(&memosLock) < 0)
	{
		return;
	}

	lockResource(&memosLock);
	while (1)
	{
		if (getErrmsg(msgwritebuf) == 0)
		{
			break;
		}

		if (msgwritebuf[0] == '\n')
		{
			writeMemo(omissionMsg);
		}
		else
		{
			writeMemo(msgwritebuf);
		}
	}

	unlockResource(&memosLock);
}

void	discardErrmsgs()
{
	static char	msgdiscardbuf[ERRMSGS_BUFSIZE];

	/*	The discard buffer is static, therefore shared, but
	 *	its contents are never used for any purpose.  So no
	 *	need to protect it from multiple concurrent users.	*/

	while (1)
	{
		if (getErrmsg(msgdiscardbuf) == 0)
		{
			return;
		}
	}
}

int	_coreFileNeeded(int *ctrl)
{
	static int	coreFileNeeded = CORE_FILE_NEEDED;

	if (ctrl)
	{
		coreFileNeeded = *ctrl;
	}

	return coreFileNeeded;
}

int	_iEnd(const char *fileName, int lineNbr, const char *arg)
{
	_postErrmsg(fileName, lineNbr, "Assertion failed.", arg);
	writeErrmsgMemos();
	if (_coreFileNeeded(NULL))
	{
		sm_Abort();
	}

	return 1;
}

void	encodeSdnv(Sdnv *sdnv, unsigned long val)
{
	static unsigned long	sdnvMask = ((unsigned long) -1) / 128;
	unsigned long		remnant;
	int			i;
	unsigned char		flag = 0;
	unsigned char		*text;

	/*	Get length of SDNV text: one byte for each 7 bits of
	 *	significant numeric value.  On each iteration of the
	 *	loop, until what's left of the original value is zero,
	 *	shift the remaining value 7 bits to the right and add
	 *	1 to the imputed SDNV length.				*/

	CHKVOID(sdnv);
	sdnv->length = 0;
	remnant = val;
	do
	{
		remnant = (remnant >> 7) & sdnvMask;
		(sdnv->length)++;
	} while (remnant > 0);

	/*	Now fill the SDNV text from the numeric value bits.	*/

	text = sdnv->text + sdnv->length;
	i = sdnv->length;
	remnant = val;
	while (i > 0)
	{
		text--;

		/*	Get low-order 7 bits of what's left, OR'ing
		 *	it with high-order bit flag for this position
		 *	of the SDNV.					*/

		*text = (remnant & 0x7f) | flag;

		/*	Shift those bits out of the value.		*/

		remnant = (remnant >> 7) & sdnvMask;
		flag = 0x80;		/*	Flag is now 1.		*/
		i--;
	}
}

int	decodeSdnv(unsigned long *val, unsigned char *sdnvTxt)
{
	int		sdnvLength = 0;
	unsigned char	*cursor;

	CHKZERO(val);
	CHKZERO(sdnvTxt);
	*val = 0;
	cursor = sdnvTxt;
	while (1)
	{
		sdnvLength++;
		if (sdnvLength > 10)
		{
			return 0;	/*	More than 70 bits.	*/
		}

		/*	Shift numeric value 7 bits to the left (that
		 *	is, multiply by 128) to make room for 7 bits
		 *	of SDNV byte value.				*/

		*val <<= 7;

		/*	Insert SDNV byte value (with its high-order
		 *	bit masked off) as low-order 7 bits of the
		 *	numeric value.					*/

		*val |= (*cursor & 0x7f);
		if ((*cursor & 0x80) == 0)	/*	Last SDNV byte.	*/
		{
			return sdnvLength;
		}

		/*	Haven't reached the end of the SDNV yet.	*/

		cursor++;
	}
}

void	loadScalar(Scalar *s, signed int i)
{
	CHKVOID(s);
	if (i < 0)
	{
		i = 0 - i;
	}

	s->gigs = 0;
	s->units = i;
	while (s->units >= ONE_GIG)
	{
		s->gigs++;
		s->units -= ONE_GIG;
	}
}

void	increaseScalar(Scalar *s, signed int i)
{
	CHKVOID(s);
	if (i < 0)
	{
		i = 0 - i;
	}

	s->units += i;
	while (s->units >= ONE_GIG)
	{
		s->gigs++;
		s->units -= ONE_GIG;
	}
}

void	reduceScalar(Scalar *s, signed int i)
{
	CHKVOID(s);
	if (i < 0)
	{
		i = 0 - i;
	}

	while (i > s->units)
	{
		s->units += ONE_GIG;
		s->gigs--;
	}

	s->units -= i;
}

void	multiplyScalar(Scalar *s, signed int i)
{
	double	product;

	CHKVOID(s);
	if (i < 0)
	{
		i = 0 - i;
	}

	product = ((((double)(s->gigs)) * ONE_GIG) + (s->units)) * i;
	s->gigs = (int) (product / ONE_GIG);
	s->units = (int) (product - (((double)(s->gigs)) * ONE_GIG));
}

void	divideScalar(Scalar *s, signed int i)
{
	double	quotient;

	CHKVOID(s);
	CHKVOID(i != 0);
	if (i < 0)
	{
		i = 0 - i;
	}

	quotient = ((((double)(s->gigs)) * ONE_GIG) + (s->units)) / i;
	s->gigs = (int) (quotient / ONE_GIG);
	s->units = (int) (quotient - (((double)(s->gigs)) * ONE_GIG));
}

void	copyScalar(Scalar *to, Scalar *from)
{
	CHKVOID(to);
	CHKVOID(from);
	to->gigs = from->gigs;
	to->units = from->units;
}

void	addToScalar(Scalar *s, Scalar *increment)
{
	CHKVOID(s);
	CHKVOID(increment);
	increaseScalar(s, increment->units);
	s->gigs += increment->gigs;
}

void	subtractFromScalar(Scalar *s, Scalar *decrement)
{
	CHKVOID(s);
	CHKVOID(decrement);
	reduceScalar(s, decrement->units);
	s->gigs -= decrement->gigs;
}

int	scalarIsValid(Scalar *s)
{
	CHKZERO(s);
	return (s->gigs >= 0);
}

void	findToken(char **cursorPtr, char **token)
{
	char	*cursor;

	CHKVOID(cursorPtr);
	CHKVOID(*cursorPtr);
	CHKVOID(token);
	cursor = *cursorPtr;
	*token = NULL;		/*	The default.			*/

	/*	Skip over any leading whitespace.			*/

	while (isspace((int) *cursor))
	{
		cursor++;
	}

	if (*cursor == '\0')	/*	Nothing but whitespace.		*/
	{
		*cursorPtr = cursor;
		return;
	}

	/*	Token delimited by quotes is the complicated case.	*/

	if (*cursor == '\'')	/*	Quote-delimited token.		*/
	{
		/*	Token is everything after this single quote,
		 *	up to (but not including) the next non-escaped
		 *	single quote.					*/

		cursor++;
		while (*cursor != '\0')
		{
			if (*token == NULL)
			{
				*token = cursor;
			}

			if (*cursor == '\\')	/*	Escape.		*/
			{
				/*	Include the escape character
				 *	plus the following (escaped)
				 *	character (unless it's the end
				 *	of the string) in the token.	*/

				cursor++;
				if (*cursor == '\0')
				{
					*cursorPtr = cursor;
					return;	/*	unmatched quote	*/
				}

				cursor++;
				continue;
			}

			if (*cursor == '\'')	/*	End of token.	*/
			{
				*cursor = '\0';
				cursor++;
				*cursorPtr = cursor;
				return;		/*	matched quote	*/
			}

			cursor++;
		}

		/*	If we get here it's another case of unmatched
		 *	quote, but okay.				*/

		*cursorPtr = cursor;
		return;
	}

	/*	The normal case: a simple whitespace-delimited token.
	 *	Token is this character and all successive characters
	 *	up to (but not including) the next whitespace.		*/

	*token = cursor;
	cursor++;
	while (*cursor != '\0')
	{
		if (isspace((int) *cursor))	/*	End of token.	*/
		{
			*cursor = '\0';
			cursor++;
			break;
		}

		cursor++;
	}

	*cursorPtr = cursor;
}

#ifdef ION_NO_DNS
void	parseSocketSpec(char *socketSpec, unsigned short *portNbr,
		unsigned int *ipAddress)
{
	return;
}
#else
void	parseSocketSpec(char *socketSpec, unsigned short *portNbr,
		unsigned int *ipAddress)
{
	char		*delimiter;
	char		*hostname;
	char		hostnameBuf[MAXHOSTNAMELEN + 1];
	unsigned int	i4;

	CHKVOID(portNbr);
	CHKVOID(ipAddress);
	*portNbr = 0;			/*	Use default port nbr.	*/
	*ipAddress = 0;			/*	Use local host address.	*/

	if (socketSpec == NULL || *socketSpec == '\0')
	{
		return;			/*	Use defaults.		*/
	}

	delimiter = strchr(socketSpec, ':');
	if (delimiter)
	{
		*delimiter = '\0';	/*	Delimit host name.	*/
	}

	/*	First figure out the IP address.  @ is local host.	*/

	hostname = socketSpec;
	if (strlen(hostname) != 0)
	{
		if (strcmp(socketSpec, "@") == 0)
		{
			getNameOfHost(hostnameBuf, sizeof hostnameBuf);
			hostname = hostnameBuf;
		}

		i4 = getInternetAddress(hostname);
		if (i4 < 1)		/*	Invalid hostname.	*/
		{
			writeMemoNote("[?] Can't get IP address.", hostname);
		}
		else
		{
			*ipAddress = i4;
		}
	}

	/*	Now pick out the port number, if requested.		*/

	if (delimiter == NULL)		/*	No port number.		*/
	{
		return;			/*	All done.		*/
	}

	i4 = atoi(delimiter + 1);	/*	Get port number.	*/
	if (i4 != 0)
	{
		if (i4 < 1024 || i4 > 65535)
		{
			writeMemoNote("[?] Invalid port number.", utoa(i4));
		}
		else
		{
			*portNbr = i4;
		}
	}
}
#endif

/*	Portable implementation of a safe snprintf: always NULL-
 *	terminates the content of the string composition buffer.	*/

#define SN_FMT_SIZE		64

/*	Flag array indices	*/
#define	SN_LEFT_JUST		0
#define	SN_SIGNED		1
#define	SN_SPACE_PREFIX		2
#define	SN_PAD_ZERO		3
#define	SN_ALT_OUTPUT		4

static void	snGetFlags(char **cursor, char *fmt, int *fmtLen)
{
	int	flags[5];

	/*	Copy all flags to field print format.  No flag is
	 *	copied more than once.					*/

	memset((char *) flags, 0, sizeof flags);
	while (1)
	{	
		switch (**cursor)
		{
		case '-':
			if (flags[SN_LEFT_JUST] == 0)
			{
				*(fmt + *fmtLen) = **cursor;
				(*fmtLen)++;
				flags[SN_LEFT_JUST] = 1;
			}

			break;

		case '+':
			if (flags[SN_SIGNED] == 0)
			{
				*(fmt + *fmtLen) = **cursor;
				(*fmtLen)++;
				flags[SN_SIGNED] = 1;
			}

			break;

		case ' ':
			if (flags[SN_SPACE_PREFIX] == 0)
			{
				*(fmt + *fmtLen) = **cursor;
				(*fmtLen)++;
				flags[SN_SPACE_PREFIX] = 1;
			}

			break;

		case '0':
			if (flags[SN_PAD_ZERO] == 0)
			{
				*(fmt + *fmtLen) = **cursor;
				(*fmtLen)++;
				flags[SN_PAD_ZERO] = 1;
			}

			break;

		case '#':
			if (flags[SN_ALT_OUTPUT] == 0)
			{
				*(fmt + *fmtLen) = **cursor;
				(*fmtLen)++;
				flags[SN_ALT_OUTPUT] = 1;
			}

			break;

		default:
			return;	/*	No more flags for field.	*/
		}

		(*cursor)++;
	}
}

static void	snGetNumber(char **cursor, char *fmt, int *fmtLen, int *number)
{
	int	numDigits = 0;
	char	digit;

	while (1)
	{
		digit = **cursor;
		if (digit < '0' || digit > '9')
		{
			return;	/*	No more digits in number.	*/
		}

		/*	Accumulate number value.			*/

		digit -= 48;	/*	Convert from ASCII.		*/
		if ((*number) < 0)	/*	First digit.		*/
		{
			(*number) = digit;
		}
		else
		{
			(*number) = (*number) * 10;
			(*number) += digit;
		}

		/*	Copy to field format if possible.  Largest
		 *	possible value in a 32-bit number is about
		 *	4 billion, represented in 10 decimal digits.
		 *	Largest possible value in a 64-bit number is
		 *	the square of that value, represented in no
		 *	more than 21 decimal digits.  So any number
		 *	of more than 21 decimal digits is invalid.	*/

		numDigits++;
		if (numDigits < 22)
		{
			*(fmt + *fmtLen) = **cursor;
			(*fmtLen)++;
		}

		(*cursor)++;
	}
}

int	_isprintf(const char *file, int line, char *buffer, int bufSize,
		char *format, ...)
{
	va_list	args;
	char	*cursor;
	int	stringLength = 0;
	int	printLength = 0;
	char	fmt[SN_FMT_SIZE];
	int	fmtLen;
	int	minFieldLength;
	int	precision;
	char	scratchpad[64];
	int	numLen;
	int	fieldLength;
	int	*ipval;
	char	*sval;
	int	ival;
	double	dval;
	void	*vpval;

	CHKZERO(buffer != NULL);
       	CHKZERO(format != NULL);
       	CHKZERO(bufSize > 0);
	va_start(args, format);
	for (cursor = format; *cursor != '\0'; cursor++)
	{
		if (*cursor != '%')
		{
			if ((stringLength + 1) < bufSize)
			{
				*(buffer + stringLength) = *cursor;
				printLength++;
			}

			stringLength++;
			continue;
		}

		/*	We've encountered a variable-length field in
		 *	the string.					*/

		minFieldLength = -1;	/*	Indicates none.		*/
		precision = -1;		/*	Indicates none.		*/

		/*	Start extracting the field format so that
		 *	we can use sprintf to figure out the length
		 *	of the field.					*/

		fmt[0] = '%';
		fmtLen = 1;
		cursor++;

		/*	Copy any flags for field.			*/

		snGetFlags(&cursor, fmt, &fmtLen);

		/*	Copy the minimum length of field, if present.	*/

		if (*cursor == '*')
		{
			cursor++;
			minFieldLength = va_arg(args, int);
			if (minFieldLength < 0)
			{
				minFieldLength = -1;	/*	None.	*/
			}
			else
			{
				sprintf(scratchpad, "%d", minFieldLength);
				numLen = strlen(scratchpad);
				memcpy(fmt + fmtLen, scratchpad, numLen);
				fmtLen += numLen;
			}
		}
		else
		{
			snGetNumber(&cursor, fmt, &fmtLen, &minFieldLength);
		}

		if (*cursor == '.')	/*	Start of precision.	*/
		{
			fmt[fmtLen] = '.';
			fmtLen++;
			cursor++;

			/*	Copy the precision of the field.	*/

			if (*cursor == '*')
			{
				cursor++;
				precision = va_arg(args, int);
				if (precision < 0)
				{
					precision = -1;	/*	None.	*/
				}
				else
				{
					sprintf(scratchpad, "%d", precision);
					numLen = strlen(scratchpad);
					memcpy(fmt + fmtLen, scratchpad,
							numLen);
					fmtLen += numLen;
				}
			}
			else
			{
				snGetNumber(&cursor, fmt, &fmtLen, &precision);
			}
		}

		/*	Copy the field's length modifier, if any.	*/

		if ((*cursor) == 'h'
		|| (*cursor) == 'l'
		|| (*cursor) == 'L')
		{
			fmt[fmtLen] = *cursor;
			fmtLen++;
			cursor++;
		}

		/*	Handle a couple of weird conversion characters
		 *	as applicable.					*/

		if (*cursor == 'n')	/*	Report on string size.	*/
		{
			ipval = va_arg(args, int *);
			if (ipval)
			{
				*ipval = stringLength;
			}

			continue;
		}

		if (*cursor == '%')	/*	Literal '%' in string.	*/
		{
			if ((stringLength + 1) < bufSize)
			{
				*(buffer + stringLength) = '%';
				printLength++;
			}

			stringLength++;
			continue;	/*	No argument consumed.	*/
		}

		/*	Ready to compute field length.			*/

		fmt[fmtLen] = *cursor;	/*	Copy conversion char.	*/
		fmtLen++;
		fmt[fmtLen] = '\0';	/*	Terminate format.	*/

		/*	Handle string field conversion character.	*/

		if (*cursor == 's')
		{
			sval = va_arg(args, char *);
			if (sval == NULL)
			{
				continue;
			}

			fieldLength = strlen(sval);

			/*	Truncate per precision.			*/

			if (precision != -1 && precision < fieldLength)
			{
				fieldLength = precision;
			}

			/*	Add padding as per minFieldLength.	*/

			if (minFieldLength != -1
			&& fieldLength < minFieldLength)
			{
				fieldLength = minFieldLength;
			}

			if (stringLength + fieldLength < bufSize)
			{
				sprintf(buffer + stringLength, fmt, sval);
				printLength += fieldLength;
			}

			stringLength += fieldLength;
			continue;
		}

		/*	Handle numeric field conversion character.	*/

		switch (*cursor)
		{
		case 'd':
		case 'i':
		case 'o':
		case 'x':
		case 'X':
		case 'u':
		case 'c':
			ival = va_arg(args, int);
			sprintf(scratchpad, fmt, ival);
			break;

		case 'f':
		case 'e':
		case 'E':
		case 'g':
		case 'G':
			dval = va_arg(args, double);
			sprintf(scratchpad, fmt, dval);
			break;

		case 'p':
			vpval = va_arg(args, void *);
			sprintf(scratchpad, "%#lx", (unsigned long) vpval);
			break;

		default:		/*	Bad conversion char.	*/
			continue;	/*	No argument consumed.	*/
		}

		fieldLength = strlen(scratchpad);
		if (stringLength + fieldLength < bufSize)
		{
			memcpy(buffer + stringLength, scratchpad, fieldLength);
			printLength += fieldLength;
		}

		stringLength += fieldLength;
	}

	va_end(args);

	/*	NULL-terminate the buffer contents, one way or another.	*/

	if (stringLength < bufSize)
	{
		*(buffer + stringLength) = '\0';
	}
	else
	{
		_putErrmsg(file, line, "isprintf error, buffer size exceeded.",
				itoa(stringLength));
		*(buffer + printLength) = '\0';
	}

	return stringLength;
}

/*	*	*	Other portability adaptations	*	*	*/

char	*istrcpy(char *buffer, char *text, size_t bufSize)
{
	int	maxText;

	CHKNULL(buffer != NULL);
	CHKNULL(text != NULL);
	CHKNULL(bufSize > 0);
	maxText = bufSize - 1;
	strncpy(buffer, text, maxText);
	*(buffer + maxText) = '\0';
	return buffer;
}

char	*igetcwd(char *buf, size_t size)
{
#ifdef FSWWDNAME
#include "wdname.c"
#else
	char	*cwdName = getcwd(buf, size);

	if (cwdName == NULL)
	{
		putSysErrmsg("Can't get CWD name", itoa(size));
	}

	return cwdName;
#endif
}

#if RTEMS

#ifndef SIGNAL_RULE_CT
#define SIGNAL_RULE_CT	100
#endif

typedef struct
{
	pthread_t	tid;
	int		signbr;
	SignalHandler	handler;
} SignalRule;

static SignalHandler	_signalRules(int signbr, SignalHandler handler)
{
	static SignalRule	rules[SIGNAL_RULE_CT];
	static int		rulesInitialized = 0;
	int			i;
	pthread_t		tid = sm_TaskIdSelf();
	SignalRule		*rule;

	if (!rulesInitialized)
	{
		memset((char *) rules, 0, sizeof rules);
		rulesInitialized = 1;
	}

	if (handler)	/*	Declaring a new signal rule.		*/
	{
		/*	We take this as an opportunity to clear out any
 		 *	existing rules that are no longer needed, due to
 		 *	termination of the threads that declared them.	*/

		for (i = 0, rule = rules; i < SIGNAL_RULE_CT; i++, rule++)
		{
			if (rule->tid == 0)	/*	Cleared.	*/
			{
				if (handler == NULL)	/*	Noted.	*/
				{
					continue;
				}

				/*	Declare new signal rule here.	*/

				rule->tid = tid;
				rule->signbr = signbr;
				rule->handler = handler;
				handler = NULL;		/*	Noted.	*/
				continue;
			}

			/*	This is a declared signal rule.		*/

			if (rule->tid == tid)
			{
				/*	One of thread's own rules.	*/

				if (rule->signbr != signbr)
				{
					continue;	/*	Okay.	*/
				}

				/*	New handler for tid/signbr.	*/

				if (handler)	/*	Not noted yet.	*/
				{
					rule->handler = handler;
					handler = NULL;	/*	Noted.	*/
				}
				else	/*	Noted in another rule.	*/
				{
					rule->tid = 0;	/*	Clear.	*/
				}

				continue;
			}

			/*	Signal rule for another thread.		*/

			if (!sm_TaskExists(rule->tid))
			{
				/*	Obsolete rule; thread is gone.	*/

				rule->tid = 0;		/*	Clear.	*/
			}
		}

		return NULL;
	}

	/*	Just looking up applicable signal rule for tid/signbr.	*/

	for (i = 0, rule = rules; i < SIGNAL_RULE_CT; i++, rule++)
	{
		if (rule->tid == tid && rule->signbr == signbr)
		{
			return rule->handler;
		}
	}

	return NULL;	/*	No applicable signal rule.		*/
}

static void	threadSignalHandler(int signbr)
{
	SignalHandler	handler = _signalRules(signbr, NULL);

	if (handler)
	{
		handler(signbr);
	}
}
#endif

void	isignal(int signbr, void (*handler)(int))
{
	struct sigaction	action;
#ifdef RTEMS
	sigset_t		signals;

	oK(sigemptyset(&signals));
	oK(sigaddset(&signals, signbr));
	oK(pthread_sigmask(SIG_UNBLOCK, &signals, NULL));
	oK(_signalRules(signbr, handler));
	handler = threadSignalHandler;
#endif
	memset((char *) &action, 0, sizeof(struct sigaction));
	action.sa_handler = handler;
	oK(sigaction(signbr, &action, NULL));
#ifdef freebsd
	oK(siginterrupt(signbr, 1));
#endif
}

void	iblock(int signbr)
{
	sigset_t	signals;

	oK(sigemptyset(&signals));
	oK(sigaddset(&signals, signbr));
	oK(pthread_sigmask(SIG_BLOCK, &signals, NULL));
}

char	*igets(int fd, char *buffer, int buflen, int *lineLen)
{
	char	*cursor = buffer;
	int	maxLine = buflen - 1;
	int	len;

	if (fd < 0 || buffer == NULL || buflen < 1 || lineLen == NULL)
	{
		putErrmsg("Invalid argument(s) passed to igets().", NULL);
		return NULL;
	}

	len = 0;
	while (1)
	{
		switch (read(fd, cursor, 1))
		{
		case 0:		/*	End of file; also end of line.	*/
			if (len == 0)		/*	Nothing more.	*/
			{
				*lineLen = len;
				return NULL;	/*	Indicate EOF.	*/
			}

			/*	End of last line.			*/

			break;			/*	Out of switch.	*/

		case -1:
			if (errno == EINTR)	/*	Treat as EOF.	*/
			{
				*lineLen = 0;
				return NULL;
			}

			putSysErrmsg("Failed reading line", itoa(len));
			*lineLen = -1;
			return NULL;

		default:
			if (*cursor == 0x0a)		/*	LF (nl)	*/
			{
				/*	Have reached end of line.	*/

				if (len > 0
				&& *(buffer + (len - 1)) == 0x0d)
				{
					len--;		/*	Lose CR	*/
				}

				break;		/*	Out of switch.	*/
			}

			/*	Have not reached end of line yet.	*/

			if (len == maxLine)	/*	Must truncate.	*/
			{
				break;		/*	Out of switch.	*/
			}

			/*	Okay, include this char in the line...	*/

			len++;

			/*	...and read the next character.		*/

			cursor++;
			continue;
		}

		break;				/*	Out of loop.	*/
	}

	*(buffer + len) = '\0';
	*lineLen = len;
	return buffer;
}

int	iputs(int fd, char *string)
{
	int	totalBytesWritten = 0;
	int	length;
	int	bytesWritten;

	CHKERR(fd >= 0 && string != NULL);
	length = strlen(string);
	while (totalBytesWritten < length)
	{
		bytesWritten = write(fd, string + totalBytesWritten,
				length - totalBytesWritten);
		if (bytesWritten < 0)
		{
			putSysErrmsg("Failed writing line",
					itoa(totalBytesWritten));
			return -1;
		}

		totalBytesWritten += bytesWritten;
	}

	return totalBytesWritten;
}
