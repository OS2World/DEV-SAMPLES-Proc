/* PROC2.C
 *
 * Sample program using the DosQPocStatus() function
 * for OS/2 2.x
 *
 * Kai Uwe Rommel - Wed 25-Mar-1992
 *                  Sat 13-Aug-1994
 *
 * can be compiled with
 * - 16-bit compiler in small and large data models (MS C 6.00A tested)
 * - 32-bit compiler (IBM C Set++ tested)
 */

#define INCL_NOPM
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_DOSMEMMGR
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#if defined(__32BIT__)
  #define PTR(ptr, ofs)  ((void *) ((char *) (ptr) + (ofs)))
#else
  #define DosQueryModuleName DosGetModName
  #define APIENTRY16 APIENTRY
  #if defined(M_I86SM) || defined(M_I86MM)
    #define PTR(ptr, ofs)  ((void *) ((char *) (ptr) + (ofs)))
  #else
    #define PTR(ptr, ofs)  ((void *) ((char *) (((ULONG) procstat & 0xFFFF0000) | (USHORT) (ptr)) + (ofs)))
    /* kludge to transform 0:32 into 16:16 pointer in this special case */
  #endif
#endif



/* DosQProcStatus() = DOSCALLS.154 */
USHORT APIENTRY16 DosQProcStatus(PVOID pBuf, USHORT cbBuf);

/* DosGetPrty = DOSCALLS.9 */
USHORT APIENTRY16 DosGetPrty(USHORT usScope, PUSHORT pusPriority, USHORT pid);


struct procstat
{
  ULONG  summary;
  ULONG  processes;
  ULONG  semaphores;
  ULONG  unknown1;
  ULONG  sharedmemory;
  ULONG  modules;
  ULONG  unknown2;
  ULONG  unknown3;
};


struct process
{
  ULONG  type;
  ULONG  threadlist;
  USHORT processid;
  USHORT parentid;
  ULONG  sessiontype;
  ULONG  status; /* see status #define's below */
  ULONG  sessionid;
  USHORT modulehandle;
  USHORT threads;
  ULONG  reserved1;
  ULONG  reserved2;
  USHORT semaphores;
  USHORT dlls;
  USHORT shrmems;
  USHORT reserved3;
  ULONG  semlist;
  ULONG  dlllist;
  ULONG  shrmemlist;
  ULONG  reserved4;
};

#define STAT_EXITLIST 0x01
#define STAT_EXIT1    0x02
#define STAT_EXITALL  0x04
#define STAT_PARSTAT  0x10
#define STAT_SYNCH    0x20
#define STAT_DYING    0x40
#define STAT_EMBRYO   0x80


struct thread
{
  ULONG  type;
  USHORT threadid;
  USHORT threadslotid;
  ULONG  blockid;
  ULONG  priority;
  ULONG  systime;
  ULONG  usertime;
  UCHAR  status; /* see status #define's below */
  UCHAR  reserved1;
  USHORT reserved2;
};

#define TSTAT_READY   1
#define TSTAT_BLOCKED 2
#define TSTAT_RUNNING 5
#define TSTAT_LOADED  9

char *threadstat[256] = {NULL, "Ready", "Blocked", NULL, NULL, "Running", 
			NULL, NULL, NULL, "Loaded"};


struct module
{
  ULONG  nextmodule;
  USHORT modhandle;
  USHORT modtype;
  ULONG  submodules;
  ULONG  segments;
  ULONG  reserved;
  ULONG  namepointer;
  USHORT submodule[1];  /* varying, see member submodules */
};


struct semaphore
{
  ULONG  nextsem;
  USHORT owner;
  UCHAR  flag;
  UCHAR  refs;
  UCHAR  requests;
  UCHAR  reserved1;
  ULONG  reserved2;
  USHORT reserved3;
  USHORT index;
  USHORT dummy;
  UCHAR  name[1];       /* varying */
};


struct shmem
{
  ULONG  nextseg;
  USHORT handle;
  USHORT selector;
  USHORT refs;
  UCHAR  name[1];       /* varying */
};


struct procstat *procstat;


void timestr(ULONG time, char *buffer)
{
  ULONG seconds, hundredths;

  seconds = time / 32;
  hundredths = (time % 32) * 100 / 32;

  sprintf(buffer, "%ld:%02ld.%02ld", seconds / 60, seconds % 60, hundredths);
}


void proctree(USHORT pid, USHORT indent)
{
  struct process *proc;
  UCHAR name[256], time[32];
  USHORT prty = 0;
  struct thread *thread;
  ULONG cpu = 0;
  int i;

  for ( proc = PTR(procstat -> processes, 0);
        proc -> type != 3; /* not sure if there isn't another termination */
        proc = PTR(proc -> threadlist,                          /* method */
                   proc -> threads * sizeof(struct thread))
      )
  {
    if ( proc -> parentid == pid && proc -> type != 0 )
    {
      DosQueryModuleName(proc -> modulehandle, sizeof(name), name);
      DosGetPrty(PRTYS_PROCESS, &prty, proc -> processid);

      for ( cpu = 0, i = 0, thread = PTR(proc -> threadlist, 0);
	   i < proc -> threads; i++, thread++ )
	cpu += (thread -> systime + thread -> usertime);

      timestr(cpu, time);

      printf("%5d  %5d   %02lX  %4d %04X %10s %*s%s\n",
        proc -> processid, proc -> parentid, proc -> sessionid,
        proc -> threads, prty, time, indent, "", name);

      proc -> type = 0;  /* kludge, mark it as already printed */
      proctree(proc -> processid, indent + 2);
    }
  }

  if ( pid != 0 )
    return;

  /* if at the root level, check for those processes that have lost *
   * their parent process and show them as if they were childs of 0 */

  for ( proc = PTR(procstat -> processes, 0);
        proc -> type != 3; /* not sure if there isn't another termination */
        proc = PTR(proc -> threadlist,                          /* method */
                   proc -> threads * sizeof(struct thread))
      )
  {
    if ( proc -> type != 0 )
    {
      DosQueryModuleName(proc -> modulehandle, sizeof(name), name);
      DosGetPrty(PRTYS_PROCESS, &prty, proc -> processid);

      for ( cpu = 0, i = 0, thread = PTR(proc -> threadlist, 0);
	   i < proc -> threads; i++, thread++ )
	cpu += (thread -> systime + thread -> usertime);

      timestr(cpu, time);

      printf("%5d  %5d   %02lX  %4d %04X %10s %*s%s\n",
        proc -> processid, proc -> parentid, proc -> sessionid,
        proc -> threads, prty, time, indent, "", name);

      proctree(proc -> processid, indent + 2);
    }

    proc -> type = 1;    /* kludge, reset mark */
  }
}


void threadlist(void)
{
  struct process *proc;
  struct thread *thread;
  UCHAR name[256], systime[32], usertime[32], *status;
  USHORT count;

  for ( proc = PTR(procstat -> processes, 0);
        proc -> type != 3;
        proc = PTR(proc -> threadlist,
                   proc -> threads * sizeof(struct thread))
      )
  {
    DosQueryModuleName(proc -> modulehandle, sizeof(name), name);
    printf("%s\n", name);

    for ( count = 0, thread = PTR(proc -> threadlist, 0);
          count < proc -> threads; count++, thread++ )
    {
      timestr(thread -> systime, systime);
      timestr(thread -> usertime, usertime);
      status = threadstat[thread -> status];
      printf("%5d  %5d  %04lX %-7.7s %08lX %10s %10s\n", 
	     thread -> threadid, thread -> threadslotid, thread -> priority, 
	     status ? status : "Unknown", thread -> blockid, systime, usertime);
    }
  }
}


void modlist(int imports)
{
  struct module *mod;
  UCHAR name[256];
  ULONG count;

  for ( mod = PTR(procstat -> modules, 0); ;
        mod = PTR(mod -> nextmodule, 0)
      )
  {
    if ( !imports || mod -> submodules )
      if ( imports )
      {
        printf("%04X  %s\n", mod -> modhandle, PTR(mod -> namepointer, 0));

        for ( count = 0; count < mod -> submodules; count++ )
        {
          DosQueryModuleName(mod -> submodule[count], sizeof(name), name);
          printf("  %04X  %s\n", mod -> submodule[count], name);
        }
      }
      else
        printf(" %04X   %2d  %7ld  %6ld  %s\n", mod -> modhandle,
               mod -> modtype ? 32 : 16, mod -> segments,
               mod -> submodules, PTR(mod -> namepointer, 0));

    if ( mod -> nextmodule == 0L )
      break;
  }
}


void semlist(void)
{
  struct semaphore *sem;

  for ( sem = PTR(procstat -> semaphores, 16);
        sem -> nextsem != 0L;
        sem = PTR(sem -> nextsem, 0)
      )
    printf("%3d  %3d   %02X  %4d   %04X  \\SEM%s\n",
           sem -> refs, sem -> requests, sem -> flag,
           sem -> owner, sem -> index, sem -> name);
}


void shmlist(void)
{
  struct shmem *shmem;
  UCHAR name[256];
  ULONG size;
#if defined(__32BIT__)
  ULONG attrib;
  PVOID base;
#else
  SEL sel;
#endif

  for ( shmem = PTR(procstat -> sharedmemory, 0);
        shmem -> nextseg != 0L;
        shmem = PTR(shmem -> nextseg, 0)
      )
  {
    strcpy(name, "\\SHAREMEM\\");
    strcat(name, shmem -> name);

#if defined(__32BIT__)
    DosGetNamedSharedMem(&base, name, PAG_READ);
    size = -1;
    attrib = PAG_SHARED | PAG_READ;
    if ( DosQueryMem(base, &size, &attrib) )
      size = 0;
    DosFreeMem(base);
#else
    DosGetShrSeg(name, &sel);
    if ( DosSizeSeg(sel, &size) )
      size = 0;
    DosFreeSeg(sel);
#endif

    printf(" %04X   %04X   %08lX  %4d  %s\n", shmem -> handle,
           shmem -> selector, size, shmem -> refs, name);
  }
}


void main(void)
{
  procstat = malloc(0x8000);
  DosQProcStatus(procstat, 0x8000);

  printf("\n¯ Process list\n");
  printf("\n PID    PPID  SESS THRD PRIO    TIME    PROGRAM");
  printf("\n------ ------ ---- ---- ---- ---------- ------->\n");

  proctree(0, 0);

  printf("\n¯ Thread list\n");
  printf("\n TID    TSID  PRIO  STATE  BLOCKID   SYSTIME    USERTIME");
  printf("\n------ ------ ---- ------- -------- ---------- ----------\n");

  threadlist();

  printf("\n¯ Module list\n");
  printf("\nHANDLE TYPE SEGMENTS IMPORTS PATH");
  printf("\n------ ---- -------- ------- ---->\n");

  modlist(0);

  printf("\n¯ Module tree\n");
  printf("\n--------------\n");

  modlist(1);

  printf("\n¯ Semaphore list\n");
  printf("\nREF  REQ  FLAG OWNER INDEX  NAME");
  printf("\n---- ---- ---- ----- ------ ---->\n");

  semlist();

  printf("\n¯ Shared memory list\n");
  printf("\nHANDLE  SEL    SEGSIZE    REF  NAME");
  printf("\n------ ------ ---------- ----- ---->\n");

  shmlist();

  exit(0);
}


/* End of PROCS.C */
