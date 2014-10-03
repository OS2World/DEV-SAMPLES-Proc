/* PROCS.C
 *
 * Sample program using the DosQPocStatus() function under OS/2 1.x
 * Kai Uwe Rommel - Sat 04-Aug-1990
 */

#define INCL_NOPM
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_DOSMEMMGR
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


extern USHORT APIENTRY DosQProcStatus(PVOID pBuf, USHORT cbBuf);


struct process
{
  USHORT pid;
  USHORT ppid;
  USHORT session;
  USHORT threads;
  USHORT children;
  USHORT modhandle;
  USHORT module;
};


struct module
{
  USHORT modhandle;
  USHORT max_dependents;
  USHORT *dependents;
  UCHAR  *name;
  USHORT listed;
};


struct semaphore
{
  USHORT refs;
  USHORT requests;
  UCHAR  *name;
};


struct shmem
{
  USHORT handle;
  USHORT selector;
  USHORT refs;
  UCHAR  *name;
};


struct process   **proc = NULL;
struct module    **mod  = NULL;
struct semaphore **sem  = NULL;
struct shmem     **shm  = NULL;

USHORT max_proc = 0;
USHORT cur_proc = 0;
USHORT max_mod  = 0;
USHORT cur_mod  = 0;
USHORT max_sem  = 0;
USHORT cur_sem  = 0;
USHORT max_shm  = 0;
USHORT cur_shm  = 0;


int compare_proc(struct process **p1, struct process **p2)
{
  return (*p1) -> pid - (*p2) -> pid;
}


int compare_mod(struct module **m1, struct module **m2)
{
  return (*m1) -> modhandle - (*m2) -> modhandle;
}


int parse_buffer(UCHAR * bBuf)
{
  USHORT type, tpid, seminf;
  USHORT count, kount;
  UCHAR buffer[256];
  UCHAR *cptr, *ptr;
  UCHAR *next;

  ptr = bBuf;

  while ( (type = *(USHORT *) ptr) != 0xFFFFU )
  {
    ptr += 2;
    next = *(UCHAR **) ptr;
    ptr += 2;

    switch ( type )
    {

    case 0: /* process */

      if ( cur_proc >= max_proc )
      {
        max_proc += 50;

	if ( !(proc = realloc(proc, max_proc * sizeof(struct process *))) )
          return 1;
      }

      if ( !(proc[cur_proc] = calloc(1, sizeof(struct process))) )
        return 1;

      proc[cur_proc] -> pid = *(USHORT *) ptr;
      ptr += 2;
      proc[cur_proc] -> ppid = *(USHORT *) ptr;
      ptr += 2;
      proc[cur_proc] -> session = *(USHORT *) ptr;
      ptr += 2;
      proc[cur_proc] -> modhandle = *(USHORT *) ptr;

      proc[cur_proc] -> threads = 0;
      proc[cur_proc] -> module  = -1;
      ++cur_proc;

      break;

    case 1: /* thread */

      ptr += 2;
      tpid = *(USHORT *) ptr;

      for ( count = 0; count < cur_proc; count++ )
	if ( proc[count] -> pid == tpid )
	{
	  ++proc[count] -> threads;
	  break;
	}

      break;

    case 2: /* module */

      if ( cur_mod >= max_mod )
      {
        max_mod += 50;

	if ( !(mod = realloc(mod, max_mod * sizeof(struct module *))) )
          return 1;
      }

      if ( !(mod[cur_mod] = calloc(1, sizeof(struct module))) )
        return 1;

      mod[cur_mod] -> modhandle = *(USHORT *) ptr;
      ptr += 2;
      mod[cur_mod] -> max_dependents = *(USHORT *) ptr;
      ptr += 2;
      ptr += 2;
      ptr += 2;

      if ( mod[cur_mod] -> max_dependents )
      {
	if ( !(mod[cur_mod] -> dependents = calloc(mod[cur_mod] -> max_dependents, sizeof(USHORT))) )
          return 1;

	for ( count = 0; count < mod[cur_mod] -> max_dependents; count++ )
	{
	  mod[cur_mod] -> dependents[count] = *(USHORT *) ptr;
	  ptr += 2;
	}
      }

      for ( cptr = buffer; *cptr++ = *ptr++; );

      if ( !(mod[cur_mod] -> name = strdup(buffer)) )
        return 1;

      ++cur_mod;

      break;

    case 3: /* system semaphore */

      if ( cur_sem >= max_sem )
      {
        max_sem += 50;

	if ( !(sem = realloc(sem, max_sem * sizeof(struct semaphore *))) )
          return 1;
      }

      if ( !(sem[cur_sem] = calloc(1, sizeof(struct semaphore))) )
        return 1;

      ptr += 2;
      seminf = *(USHORT *) ptr;
      sem[cur_sem] -> refs = seminf & 0xFF;
      sem[cur_sem] -> requests = seminf >> 8;
      ptr += 2;
      ptr += 2;

      for ( cptr = buffer; *cptr++ = *ptr++; );

      if ( !(sem[cur_sem] -> name = strdup(buffer)) )
        return 1;

      ++cur_sem;

      break;

    case 4: /* shared memory */

      if ( cur_shm >= max_shm )
      {
        max_shm += 50;

	if ( !(shm = realloc(shm, max_shm * sizeof(struct shmem *))) )
          return 1;
      }

      if ( !(shm[cur_shm] = calloc(1, sizeof(struct shmem))) )
        return 1;

      shm[cur_shm] -> handle = *(USHORT *) ptr;
      ptr += 2;
      shm[cur_shm] -> selector = *(USHORT *) ptr;
      ptr += 2;
      shm[cur_shm] -> refs = *(USHORT *) ptr;
      ptr += 2;

      for ( cptr = buffer; *cptr++ = *ptr++; );

      if ( !(shm[cur_shm] -> name = strdup(buffer)) )
        return 1;

      ++cur_shm;

      break;

    default: /* other ? */
      break;

    }

    ptr = next;
  }

  qsort(proc, cur_proc, sizeof(struct process *), compare_proc);
  qsort(mod, cur_mod, sizeof(struct module *), compare_mod);

  for ( count = 0; count < cur_proc; count++ )
    for ( kount = 0; kount < cur_mod; kount++ )
      if ( proc[count] -> modhandle == mod[kount] -> modhandle )
      {
        proc[count] -> module = kount;
	break;
      }

  for ( count = 0; count < cur_proc; count++ )
    for ( kount = 0; kount < cur_proc; kount++ )
      if ( proc[count] -> pid == proc[kount] -> ppid )
	(proc[count] -> children)++;

  return 0;
}


void free_data()
{
  USHORT count;

  for (count = 0; count < cur_proc; count++)
    free(proc[count]);

  for (count = 0; count < cur_mod; count++)
  {
    if ( mod[count] -> max_dependents )
      free(mod[count] -> dependents);

    free(mod[count] -> name);
    free(mod[count]);
  }

  for (count = 0; count < cur_sem; count++)
  {
    free(sem[count] -> name);
    free(sem[count]);
  }

  for (count = 0; count < cur_shm; count++)
  {
    free(shm[count] -> name);
    free(shm[count]);
  }

  free(proc);
  free(mod);
  free(sem);
  free(shm);

  max_proc = max_mod = cur_proc = cur_mod = 0;
  max_sem  = max_shm = cur_sem  = cur_shm = 0;
  proc = NULL;
  mod  = NULL;
}


void proctree(USHORT pid, USHORT indent)
{
  USHORT count;
  UCHAR *mName, pName[256];

  for (count = 0; count < cur_proc; count++)
    if ( proc[count] -> ppid == pid )
    {
      if ( proc[count] -> module != -1 )
      {
        mName = mod[proc[count] -> module] -> name;
        DosGetModName(proc[count] -> modhandle, sizeof(pName), pName);
      }
      else
      {
        mName = "unknown";  /* Zombie process, i.e. result for DosCwait() */
        pName[0] = 0;       /* or DOS box or swapper (?) */
      }

#ifdef FULL
      printf("%5d  %5d   %02x  %4d  %4d  %-8s %*s%s\n",
        proc[count] -> pid, proc[count] -> ppid, proc[count] -> session,
        proc[count] -> children, proc[count] -> threads,
        mName, indent, "", pName);
#else
      printf("%5d   %02x  %4d  %-8s %*s%s\n",
        proc[count] -> pid, proc[count] -> session,
        proc[count] -> threads, mName, indent, "", pName);
#endif

      proctree(proc[count] -> pid, indent + 2);
    }
}


void modlist(void)
{
  UCHAR pName[256];
  USHORT count;

  for (count = 0; count < cur_mod; count++)
  {
    DosGetModName(mod[count] -> modhandle, sizeof(pName), pName);
    printf("%-8s  %04x  %s\n",
      mod[count] -> name, mod[count] -> modhandle, pName);
  }
}


void modtree(USHORT module, USHORT indent)
{
  UCHAR *mName, pName[256];
  USHORT cnt1, cnt2;

  if ( module == -1 )
    return;

  mName = mod[module] -> name;
  DosGetModName(mod[module] -> modhandle, sizeof(pName), pName);

  if ( mod[module] -> listed )
  {
    printf("%*s%s*\n", indent, "", mName);
    return;
  }
  else
    printf("%*s%-8s%*s%s\n", indent, "", mName, 32 - indent, "", pName);

  mod[module] -> listed = 1;

  for ( cnt1 = 0; cnt1 < mod[module] -> max_dependents; cnt1++ )
    for ( cnt2 = 0; cnt2 < cur_mod; cnt2++ )
      if ( mod[cnt2] -> modhandle == mod[module] -> dependents[cnt1] )
        modtree(cnt2, indent + 2);
}


void semlist(void)
{
  USHORT count;

  for (count = 0; count < cur_sem; count++)
    printf("%4d  %4d  %s\n",
      sem[count] -> refs, sem[count] -> requests, sem[count] -> name);
}


void shmlist(void)
{
  USHORT count;
  ULONG size;
  SEL sel;
  UCHAR name[256];

  for (count = 0; count < cur_shm; count++)
  {
    strcpy(name, "\\SHAREMEM\\");
    strcat(name, shm[count] -> name);

    DosGetShrSeg(name, &sel);
    if ( DosSizeSeg(sel, &size) )
      size = 0L;
    DosFreeSeg(sel);

    printf(" %04x   %04x   %04lx  %4d  %s\n",
      shm[count] -> handle, shm[count] -> selector, size,
      shm[count] -> refs, shm[count] -> name);
  }
}


void main(void)
{
  UCHAR *pBuf;
  USHORT count;

  pBuf = malloc(0x8000);
  DosQProcStatus(pBuf, 0x8000);

  if ( parse_buffer(pBuf) )
  {
    printf("Error: Out of memory!\n");
    DosExit(EXIT_PROCESS, 1);
  }

  free(pBuf);

#ifdef FULL
  printf("\n PID    PPID  SESS CHLDS THRDS MODULE   PROGRAM");
  printf("\n------ ------ ---- ----- ----- -------- ------->\n");
#else
  printf("\n PID   SESS THRDS MODULE   PROGRAM");
  printf("\n------ ---- ----- -------- ------->\n");
#endif

  proctree(0, 0);

  printf("\nMODULE   HANDLE PATH");
  printf("\n-------- ------ ---->\n");

  modlist();

  printf("\nMODULE TREE");
  printf("\n-----------\n");

  for (count = 0; count < cur_proc; count++)
    modtree(proc[count] -> module, 0);

  printf("\n REF   REQ  NAME");
  printf("\n----- ----- ---->\n");

  semlist();

  printf("\nHANDLE  SEL    SIZE   REF  NAME");
  printf("\n------ ------ ------ ----- ---->\n");

  shmlist();

  free_data();

  DosExit(EXIT_PROCESS, 0);
}


/* End of PROCS.C */
