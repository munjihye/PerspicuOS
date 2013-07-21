/*===- ghost.c - Ghost Compatibility Library ------------------------------===
 * 
 *                        Secure Virtual Architecture
 *
 * This file was developed by the LLVM research group and is distributed under
 * the University of Illinois Open Source License. See LICENSE.TXT for details.
 * 
 *===----------------------------------------------------------------------===
 *
 * This file defines compatibility functions to permit ghost applications to
 * use system calls.
 *
 *===----------------------------------------------------------------------===
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <pwd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ghost.h"

/* Size of traditional memory buffer */
static uintptr_t tradlen = 16384;

/* Buffer containing the traditional memory */
static unsigned char * tradBuffer;

/* Pointer into the traditional memory buffer stack */
static unsigned char * tradsp;

static int logfd = 0;
static char logbuf[128];

///////////////////////////////////////////////////////////////////////////////
// Functions to be used by ghosting applications.
///////////////////////////////////////////////////////////////////////////////

//
// Function: ghostinit()
//
// Description:
//  This function initializes the ghost run-time.  It should be called in a
//  program's main() function.
//
void
ghostinit (void) {
  /*
   * Allocate traditional memory using mmap().  We'll use it like a stack.
   */
  tradBuffer = (unsigned char *) mmap(0, tradlen, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
  if (tradBuffer == MAP_FAILED) {
    abort ();
  }

  /*
   * Initialize the traditional memory stack pointer.
   */
  tradsp = tradBuffer + tradlen;

  /*
   * Restrict ourselves to only jumping to the signal handler trampoline.
   */
  ghostAllowFunction ((void *)0x7ffffffff000);

  /*
   * Open a log file.
   */
  logfd = open ("/tmp/ghostlog", O_FSYNC | O_CREAT | O_TRUNC | O_WRONLY, 0777);
  logfd = dup2 (logfd, 24);
  snprintf (logbuf, 128, "#ghostinit: %lx %lx\n", tradBuffer, tradlen);
  write (logfd, logbuf, strlen (logbuf));
  return;
}

//
// Function: ghostAllowFunction()
//
// Description:
//  Permit the specified function to be dispatched through an asynchronous
//  event.
//
void
ghostAllowFunction (void * f) {
  if (getenv ("GHOSTING")) {
    __asm__ __volatile__ ("int $0x7d\n" :: "D" (f));
  }
  return;
}

///////////////////////////////////////////////////////////////////////////////
// Support routines for system call wrappers
///////////////////////////////////////////////////////////////////////////////

//
// Template: allocateTradMem()
//
// Description:
//  Allocate traditional memory.
//
template<typename T>
static inline T *
allocate (T * data) {
  //
  // Allocate memory on the traditional memory stack.
  //
  tradsp -= sizeof (T);

  //
  // Copy the data into the traditional memory.
  //
  T * copy = (T *)(tradsp);
  return copy;
}

static inline
char * allocate (uintptr_t size) {
  //
  // Allocate memory on the traditional memory stack.
  //
  tradsp -= size;

  //
  // Copy the data into the traditional memory.
  //
  char * copy = (char *)(tradsp);
  return copy;
}

//
// Template: allocateTradMem()
//
// Description:
//  Allocate traditional memory and copy the contents of a memory object into
//  it.  This is useful for setting up input data.
//
template<typename T>
static inline T *
allocAndCopy (T* data) {
  T * copy = 0;
  if (data) {
    //
    // Allocate memory on the traditional memory stack.
    //
    tradsp -= sizeof (T);

    //
    // Copy the data into the traditional memory.
    //
    copy = (T *)(tradsp);
    *copy = *data;
  }
  return copy;
}

static inline char *
allocAndCopy (char * data) {
  char * copy = 0;
  if (data) {
    //
    // Allocate memory on the traditional memory stack.
    //
    tradsp -= (strlen (data) + 1);

    //
    // Copy the data into the traditional memory.
    //
    copy = (char *)(tradsp);
    strcpy (copy, data);
  }
  return copy;
}

static inline char *
allocAndCopy (void * data, uintptr_t size) {
  // Pointer to the new copy we will create
  char * copy = 0;

  if (data) {
    //
    // Allocate memory on the traditional memory stack.
    //
    tradsp -= (size);

    //
    // Copy the data into the traditional memory.
    //
    copy = (char *)(tradsp);
    memcpy (copy, data, size);
  }
  return copy;
}


//////////////////////////////////////////////////////////////////////////////
// Wrappers for system calls
//////////////////////////////////////////////////////////////////////////////

int
ghost_accept (int s, struct sockaddr * addr, socklen_t * addrlen) {
  int ret;
  unsigned char * framep = tradsp;
  if (addr && addrlen) {
    struct sockaddr * newaddr = (struct sockaddr *) allocate (*addrlen);
    socklen_t * newaddrlen = allocAndCopy (addrlen);

    // Perform the system call
    ret = accept (s, newaddr, newaddrlen);

    // Copy the outputs back into secure memory
    memcpy (addr, newaddr, *newaddrlen);
    memcpy (addrlen, newaddrlen, sizeof (socklen_t));
  } else {
    ret = accept (s, addr, addrlen);
  }

  snprintf (logbuf, 128, "#accept: %d %d\n", ret, errno);
  write (logfd, logbuf, strlen (logbuf));

  // Restore the stack pointer
  tradsp = framep;

  return ret;
}

int
ghost_getpeereid(int s, uid_t *euid, gid_t *egid) {
  // Save the current location of the traditional memory stack pointer.
  unsigned char * framep = tradsp;

  uid_t * newuid = allocAndCopy (euid);
  gid_t * newgid = allocAndCopy (egid);

  // Do the call
  int ret = getpeereid (s, newuid, newgid);

  snprintf (logbuf, 128, "#getpeereid: %d: %d %d\n", ret, s, errno);
  write (logfd, logbuf, strlen (logbuf));

  // Copy the data back into the ghost memory
  *euid = *newuid;
  *egid = *newgid;

  // Restore the stack pointer
  tradsp = framep;
  return ret;
}

int
ghost_connect(int s, const struct sockaddr *addr, socklen_t addrlen) {
  int ret;
  unsigned char * framep = tradsp;
  struct sockaddr * newaddr = (struct sockaddr *) allocate (addrlen);
  memcpy (newaddr, addr, addrlen);

  // Perform the system call
  ret = connect (s, newaddr, addrlen);

  snprintf (logbuf, 128, "#connect: %d %d\n", ret, errno);
  write (logfd, logbuf, strlen (logbuf));

  // Restore the stack pointer
  tradsp = framep;
  return ret;
}

int
_bind(int s, const struct sockaddr *addr, socklen_t addrlen) {
  __asm__ __volatile__ ("nop");
  int ret;
  unsigned char * framep = tradsp;
  struct sockaddr * newaddr = (struct sockaddr *) allocate (addrlen);
  memcpy (newaddr, addr, addrlen);

  // Perform the system call
  ret = bind (s, newaddr, addrlen);

  snprintf (logbuf, 128, "#bind: %d %d\n", ret, errno);
  write (logfd, logbuf, strlen (logbuf));

  // Restore the stack pointer
  tradsp = framep;
  return ret;
}

int
_getsockopt(int s, int level, int optname, void * optval, socklen_t * optlen) {
  int ret;
  unsigned char * framep = tradsp;
  void * newoptval = allocate (*optlen);
  socklen_t * newoptlen = allocAndCopy (optlen);

  // Perform the system call
  ret = getsockopt (s, level, optname, newoptval, newoptlen);

  snprintf (logbuf, 128, "#getsockopt: %d %d\n", ret, errno);
  write (logfd, logbuf, strlen (logbuf));

  // Copy the outputs back into secure memory
  memcpy (optval, newoptval, *newoptlen);
  memcpy (optlen, newoptlen, sizeof (socklen_t));

  // Restore the stack pointer
  tradsp = framep;
  return ret;
}

int
ghost_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
        struct timeval *timeout) {
  // Save the current location of the traditional memory stack pointer.
  unsigned char * framep = tradsp;

  fd_set * newreadfds = allocAndCopy (readfds);
  fd_set * newwritefds = allocAndCopy (writefds);
  fd_set * newexceptfds = allocAndCopy (exceptfds);
  struct timeval * newtimeout = allocAndCopy (timeout);

  // Perform the system call
  int err = select (nfds, newreadfds, newwritefds, newexceptfds, newtimeout);

  snprintf (logbuf, 128, "#select: %d: %d %d\n", nfds, err, errno);
  write (logfd, logbuf, strlen (logbuf));

  // Copy the outputs back into ghost memory
  if (readfds)   *readfds   = *newreadfds;
  if (writefds)  *writefds  = *newwritefds;
  if (exceptfds) *exceptfds = *newexceptfds;

  snprintf (logbuf, 128, "#select isset: %d\n", FD_ISSET (5, readfds));
  write (logfd, logbuf, strlen (logbuf));

  // Restore the stack pointer
  tradsp = framep;
  return err;
}

int
_pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
        struct timespec *timeout, sigset_t * sigmask) {
  // Save the current location of the traditional memory stack pointer.
  unsigned char * framep = tradsp;

  fd_set * newreadfds = allocAndCopy (readfds);
  fd_set * newwritefds = allocAndCopy (writefds);
  fd_set * newexceptfds = allocAndCopy (exceptfds);
  struct timespec * newtimeout = allocAndCopy (timeout);
  sigset_t * newsigmask = allocAndCopy (sigmask);

  // Perform the system call
  int err = pselect (nfds, newreadfds, newwritefds, newexceptfds, newtimeout, newsigmask);

  // Copy the outputs back into ghost memory
  if (readfds)   *readfds   = *newreadfds;
  if (writefds)  *writefds  = *newwritefds;
  if (exceptfds) *exceptfds = *newexceptfds;

  // Restore the stack pointer
  tradsp = framep;
  return err;
}

int
_open (char *path, int flags, mode_t mode) {
  // Save the current location of the traditional memory stack pointer.
  unsigned char * framep = tradsp;

  char * newpath = allocAndCopy (path);
  int fd = open (newpath, flags, mode);

  snprintf (logbuf, 128, "#open: %s: %d %d\n", newpath, fd, errno);
  write (logfd, logbuf, strlen (logbuf));

  // Restore the stack pointer
  tradsp = framep;
  return fd;
}

int
_close (int fd) {
  int err;
  if (fd != logfd) {
    err = close (fd);
    snprintf (logbuf, 128, "#close: %d %d\n", fd, errno);
    write (logfd, logbuf, strlen (logbuf));
  }
  return err;
}

int
_mkdir(char *path, mode_t mode) {
  // Save the current location of the traditional memory stack pointer.
  unsigned char * framep = tradsp;

  char * newpath = allocAndCopy (path);

  // Perform the system call
  int err = mkdir (newpath, mode);

  // Restore the stack pointer
  tradsp = framep;
  return err;
}

ssize_t
_readlink(char * path, char * buf, size_t bufsiz) {
  ssize_t size;
  unsigned char * framep = tradsp;
  char * newpath = allocAndCopy (path);
  char * newbuf = allocate (bufsiz);

  // Perform the system call
  size = readlink (newpath, newbuf, bufsiz);

  // Restore the stack pointer
  tradsp = framep;
  return size;
}

int
_fstat(int fd, struct stat *sb) {
  // Save the current location of the traditional memory stack pointer.
  unsigned char * framep = tradsp;

  struct stat * newsb = allocate (sb);
  int ret = fstat (fd, newsb);

  // Copy the outputs back into secure memory
  *sb = *newsb;

  // Restore the stack pointer
  tradsp = framep;
  return ret;
}

int
_stat(char *path, struct stat *sb) {
  // Save the current location of the traditional memory stack pointer.
  unsigned char * framep = tradsp;

  char * newpath = allocAndCopy (path);
  struct stat * newsb = allocate (sb);
  int ret = stat (newpath, newsb);

  // Copy the outputs back into secure memory
  *sb = *newsb;

  // Restore the stack pointer
  tradsp = framep;
  return ret;
}

ssize_t
_read(int d, void *buf, size_t nbytes) {
  ssize_t size;
  unsigned char * framep = tradsp;
  char * newbuf = allocate (nbytes);

  // Perform the system call
  size = read (d, newbuf, nbytes);

  // Copy the data back into the buffer
  if (size != -1) {
    memcpy (buf, newbuf, size);
  }

  snprintf (logbuf, 128, "#read: %d: %d %d\n", d, size, errno);
  write (logfd, logbuf, strlen (logbuf));

  // Restore the stack pointer
  tradsp = framep;
  return size;
}

ssize_t
ghost_read(int d, void *buf, size_t nbytes) {
  return _read (d, buf, nbytes);
}

ssize_t
_write(int d, void *buf, size_t nbytes) {
  ssize_t size;
  unsigned char * framep = tradsp;
  char * newbuf = allocAndCopy (buf, nbytes);

  // Perform the system call
  size = write (d, newbuf, nbytes);

  snprintf (logbuf, 128, "#write: %d: %d %d\n", d, size, errno);
  write (logfd, logbuf, strlen (logbuf));

  // Restore the stack pointer
  tradsp = framep;
  return size;
}

ssize_t
ghost_write(int d, void *buf, size_t nbytes) {
  return _write (d, buf, nbytes);
}

sig_t
ghost_signal (int sig, sig_t func) {
  //
  // Figure out the type of signal handler.  If it's a function,
  // permit the kernel to call it.
  //
  unsigned char * framep = tradsp;
  if ((func != SIG_DFL) && (func != SIG_IGN)) {
    ghostAllowFunction ((void *)func);
  }

  // Restore the stack pointer
  tradsp = framep;
  return (signal (sig, func));
}

int
ghost_sigaction (int sig, struct sigaction * act, struct sigaction * oact) {
  int ret;
  unsigned char * framep = tradsp;

  //
  // Copy in the arguments.
  //
  struct sigaction * newact = allocAndCopy (act);
  struct sigaction * newoact   = allocate (oact);

  //
  // Register the signal handler.
  //
  ret = sigaction (sig, newact, newoact);

  //
  // Permit the use of the signal handler.
  //
  if (act) {
    void * handler = 0;
    if (act->sa_flags & SA_SIGINFO) {
      handler = (void *) act->sa_sigaction;
    } else {
      handler = (void *) act->sa_handler;
    }

    if ((handler != SIG_DFL) && (handler != SIG_IGN)) {
      ghostAllowFunction (handler);
    }
  }

  //
  // Copy out the output arguments.
  //
  if (oact)
    *oact = *newoact;

  // Restore the stack pointer
  tradsp = framep;
  return ret;
}

int
_clock_gettime(clockid_t clock_id, struct timespec *tp) {
  int ret;
  unsigned char * framep = tradsp;
  struct timespec * newtp = allocate (tp);

  // Perform the system call
  ret = clock_gettime (clock_id, newtp);

  // Copy the data out
  *tp = *newtp;

  // Restore the stack pointer
  tradsp = framep;
  return ret;
}

struct passwd *
ghost_getpwuid (uid_t uid) {
  static struct passwd pw;
  struct passwd * result;
  static char buffer[1024];
  int ret = getpwuid_r (uid, &pw, buffer, sizeof (buffer), &result);
  return ((ret == 0) ? &pw : 0);
}

//////////////////////////////////////////////////////////////////////////////
// Define weak aliases to make the wrappers appear as the actual system call
//////////////////////////////////////////////////////////////////////////////

void accept () __attribute__ ((weak, alias ("_accept")));
void connect () __attribute__ ((weak, alias ("ghost_connect")));
void bind () __attribute__ ((weak, alias ("_bind")));
void ghost_bind () __attribute__ ((weak, alias ("_bind")));
void getsockopt () __attribute__ ((weak, alias ("_getsockopt")));

int select () __attribute__ ((weak, alias ("ghost_select")));
int pselect () __attribute__ ((weak, alias ("_pselect")));

void open () __attribute__ ((weak, alias ("_open")));
void close () __attribute__ ((weak, alias ("_close")));
void readlink () __attribute__ ((weak, alias ("_readlink")));
void mkdir () __attribute__ ((weak, alias ("_mkdir")));
void stat () __attribute__ ((weak, alias ("_stat")));
void fstat () __attribute__ ((weak, alias ("_fstat")));
ssize_t read () __attribute__ ((weak, alias ("_read")));
void write () __attribute__ ((weak, alias ("_write")));
void clock_gettime () __attribute__ ((weak, alias ("_clock_gettime")));
void signal () __attribute__ ((weak, alias ("_signal")));
void sigaction () __attribute__ ((weak, alias ("_sigaction")));
