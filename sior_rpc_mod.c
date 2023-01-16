/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

/**
 * @file
 * SIOR EE-side RPC code.
 */

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <fileio.h>
#include <stdio.h>
#include <sio.h>
#include "sior_rpc.h"

static SifRpcDataQueue_t qd __attribute__((aligned(64)));
static SifRpcServerData_t Sd0 __attribute__((aligned(64)));
static u32 buffer[32] __attribute__((aligned(64)));

/** EE mapped IOP mem */
#define IOP_MEM	0xbc000000

enum {
    SIOR_INIT = 1,
    SIOR_PUTC,
    SIOR_GETC,
    SIOR_GETCBLOCK,
    SIOR_WRITE,
    SIOR_READ,
    SIOR_PUTS,
    SIOR_PUTSN,
    SIOR_GETS,
    SIOR_FLUSH
};

struct init_arguments_t {
    u32 baudrate;
    u8 lcr_ueps;
    u8 lcr_upen;
    u8 lcr_usbl;
    u8 lcr_umode;
};

int iopCompleted;
char iopCmplCmp[] = "MX4SIO: Exit";

//extern volatile char strBuf[];
char strBuf[1024];

static void * sior_rpc_server(u32 funcno, void * data, int size) {
    int res = 0, c;
    size_t s;
    char * p;
    struct init_arguments_t * i;
    switch(funcno) {
    case SIOR_INIT:
	i = (struct init_arguments_t *) data;
	sio_init(i->baudrate, i->lcr_ueps, i->lcr_upen, i->lcr_usbl, i->lcr_umode);
	break;
    case SIOR_PUTC:
	c = *((int *) data);
	res = sio_putc(c);
	scr_printf("%c", c);
	break;
    case SIOR_GETC:
	res = sio_getc();
	break;
    case SIOR_GETCBLOCK:
	res = sio_getc_block();
	break;
    case SIOR_WRITE:
	p = *((char **) data) + IOP_MEM;
	s = *(((size_t *) data) + 1);
	DI();
	ee_kmode_enter();
	res = sio_write(p, s);
	ee_kmode_exit();
	EI();
	break;
    case SIOR_READ:
	p = *((char **) data) + IOP_MEM;
	s = *(((size_t *) data) + 1);
	DI();
	ee_kmode_enter();
	res = sio_read(p, s);
	ee_kmode_exit();
	EI();
	break;
    case SIOR_PUTS:
	p = *((char **) data) + IOP_MEM;
	DI();
	ee_kmode_enter();
	res = sio_puts(p);
	#if 0
	strcat((char*)strBuf, p); //This doesn't work for some reason!
	#else
	{ int k; char *s = (char*)strBuf, *t = p;
	while (*s != '\0') {s++;}
	while (1) { *s = *t; if (*t == 0) break; s++; t++; } }
	#endif
	if (strncmp(p, iopCmplCmp, strlen(iopCmplCmp)) == 0) iopCompleted = 1;
	ee_kmode_exit();
	EI();
	//scr_printf("%s", strBuf);
	break;
    case SIOR_PUTSN:
	p = *((char **) data) + IOP_MEM;
	DI();
	ee_kmode_enter();
	res = sio_putsn(p);
	//strcpy(&strBuf, "a");
	#if 0
	strcat((char*)strBuf, p); //This doesn't work for some reason!
	#else
	{ int k; char *s = (char*)strBuf, *t = p;
	while (*s != '\0') {s++;}
	while (1) { *s = *t; if (*t == 0) break; s++; t++; } }
	#endif
	//strcat((char*)strBuf, "a");
	if (strncmp(p, iopCmplCmp, strlen(iopCmplCmp)) == 0) {
		iopCompleted = 1;
		//sio_printf("\n MATCH %08X %08X ", (u32)&iopCompleted, iopCompleted); 
	}
	ee_kmode_exit();
	EI();
	//FlushCache(0);
	//scr_printf("%s", strBuf); //doesn't work
	break;
    case SIOR_GETS:
	p = *((char **) data) + IOP_MEM;
	DI();
	ee_kmode_enter();
	res = (int)sio_gets(p);
	ee_kmode_exit();
	EI();
	break;
    case SIOR_FLUSH:
	sio_flush();
	break;
    }

    *((int *) data) = res;

    return data;
}

static void sior_thread(void) {
    SifInitRpc(0);
    SifSetRpcQueue(&qd, GetThreadId());
    SifRegisterRpc(&Sd0, SIOR_IRX, (SifRpcFunc_t)sior_rpc_server, buffer, NULL, NULL, &qd);
    SifRpcLoop(&qd);
}

int SIOR_Init(int priority)
{
    static int sior_init_done = 0;
    static u8 stack[4096];
    int thid;
    ee_thread_t t;

    if (sior_init_done)
	return 0;

    t.func = sior_thread;
    t.gp_reg = 0;
    t.initial_priority = priority;
    t.stack = stack;
    t.stack_size = 4096;
    if ((thid = CreateThread(&t)) < 0) {
	printf("Error creating SIO Remote EE-thread.\n");
	return thid;
    }
    StartThread(thid, NULL);

    sior_init_done = 1;
    return thid;
}
