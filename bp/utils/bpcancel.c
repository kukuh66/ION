/*
	bpcancel.c:	bundle cancellation utility.
									*/
/*									*/
/*	Copyright (c) 2009, California Institute of Technology.		*/
/*	All rights reserved.						*/
/*	Author: Scott Burleigh, Jet Propulsion Laboratory		*/
/*									*/

#include <bpP.h>

#if defined (VXWORKS) || defined (RTEMS)
int	bpcancel(int a1, int a2, int a3, int a4, int a5,
		int a6, int a7, int a8, int a9, int a10)
{
	char		*sourceEid = (char *) a1;
	unsigned long	creationSec = a2;
	unsigned long	creationCount = a3;
	unsigned long	fragmentOffset = a4;
	unsigned long	fragmentLength = a5;
#else
int	main(int argc, char **argv)
{
	char		*sourceEid = argc > 1 ? argv[1] : NULL;
	unsigned long	creationSec = argc > 2 ? atoi(argv[2]) : 0;
	unsigned long	creationCount = argc > 3 ? atoi(argv[3]) : 0;
	unsigned long	fragmentOffset = argc > 4 ? atoi(argv[4]) : 0;
	unsigned long	fragmentLength = argc > 5 ? atoi(argv[5]) : 0;
#endif
	Sdr		sdr;
	BpTimestamp	creationTime;
	Object		bundleObj;
	Object		elt;

	if (sourceEid == NULL || creationSec == 0)
	{
		PUTS("Usage: bpcancel <source EID> <creation seconds> \
<creation count> <fragment offset> <fragment length>");
		return 0;
	}

	creationTime.seconds = creationSec;
	creationTime.count = creationCount;
	if (bp_attach() < 0)
	{
		putErrmsg("Can't attach to BP.", NULL);
		return 0;
	}

	sdr = bp_get_sdr();
	sdr_begin_xn(sdr);
	if (findBundle(sourceEid, &creationTime, fragmentOffset, fragmentLength,
			&bundleObj, &elt) < 0)
	{
		sdr_cancel_xn(sdr);
		bp_detach();
		putErrmsg("bpcancel failed finding bundle.", NULL);
		return 0;
	}

	if (bpDestroyBundle(bundleObj, 1) < 0)
	{
		sdr_cancel_xn(sdr);
		bp_detach();
		putErrmsg("bpcancel failed destroying bundle.", NULL);
		return 0;
	}

	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("bpcancel failed.", NULL);
	}

	writeErrmsgMemos();
	PUTS("Stopping bpcancel.");
	bp_detach();
	return 0;
}
