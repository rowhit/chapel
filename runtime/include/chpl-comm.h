#ifndef _chpl_comm_h_
#define _chpl_comm_h_

#ifndef LAUNCHER

#include <stdint.h>
#include "chpltypes.h"
#include "chpl-comm-impl.h"
#include "chpl-comm-heap-macros.h"
#include "chpl-tasks.h"
#include "chpl-comm-task-decls.h"
#include "chpl-comm-locales.h"

//
// Shared interface (implemented in chpl-comm.c)
//
extern c_nodeid_t chpl_nodeID; // unique ID for each node: 0, 1, 2, ...
// Note that this is the comm node ID: it carries only the .node
// portion of the chpl_localeID_t structure that represents the locale on which
// the current task is running.
// Note also that this value is set only in chpl_comm_init to a value which is
// (hopefully) unique to the running image, and never changed again.
extern int32_t chpl_numNodes; // number of nodes

extern int32_t chpl_numPrivateObjects;
extern void** chpl_privateObjects; // privatized array and domain objects

extern int chpl_verbose_comm;     // set via startVerboseComm
extern int chpl_comm_diagnostics; // set via startCommDiagnostics
extern int chpl_verbose_mem;      // set via startVerboseMem

static ___always_inline
int32_t chpl_numPrivatizedClasses(void) { return chpl_numPrivateObjects; }

extern void chpl_newPrivatizedClass(void*);
extern void* chpl_getPrivatizedClass(int32_t);

//
// Shared interface (implemented in the compiler generated code)
//
extern void chpl__heapAllocateGlobals(void);

extern const int chpl_numGlobalsOnHeap;

//
// chpl_globals_registry is an array of size chpl_numGlobalsOnHeap
// storing ptr_wide_ptr_t, that is, pointers to wide pointers. All
// registered globals are wide pointers.  Locales other than 0 need to
// set their registered globals to the wide pointers received from
// Locale 0, which is why these have type ptr_wide_ptr_t.  This is
// done in chpl_comm_broadcast_global_vars() below.
//
extern ptr_wide_ptr_t chpl_globals_registry[];

extern void* const chpl_private_broadcast_table[];

extern const int chpl_heterogeneous;


//
// Comm layer-specific interface
//

//
// returns the maximum number of threads that can be handled
// by this communication layer (used to ensure numThreadsPerLocale is
// legal); should return the sentinel value of 0 if the communication
// layer imposes no particular limit on the number of threads.
//
int32_t chpl_comm_getMaxThreads(void);


//
// initializes the communications package
//   set chpl_nodeID and chpl_numNodes
// notes:
//   * Called with the argc/argv pair passed to main()
//
void chpl_comm_init(int *argc_p, char ***argv_p);

//
// Allow the communication layer to do any secondary initialization it needs
// to, after the memory layer is initialized.
//
void chpl_comm_post_mem_init(void);

//
// if possible, run in gdb (because the user threw the --gdb flag)
// using argc and argv.  gdbArgnum gives the index of the argv[]
// element containing the --gdb flag.  Return the status of that
// process in "status" and return 1 if it was possible to run in gdb,
// 0 otherwise
//
int chpl_comm_run_in_gdb(int argc, char* argv[], int gdbArgnum, int* status);

//
// Allow the communication layer to do any further initialization it
// needs to, after the tasking layer is initialized.
//
void chpl_comm_post_task_init(void);

//
// a final comm layer stub before barrier synching and calling into
// the user code.  It is recommended that a debugging message be
// printed here indicating that each locale has started using
// chpl_msg() and a verbosity level of 2 (which will cause it to be
// displayed using the -v flag).
//
void chpl_comm_rollcall(void);

//
// Inform callers as to the communication layer's desired starting address
// and length for the shared heap, if any.
//
void chpl_comm_desired_shared_heap(void** start_p, size_t* size_p);

//
// This routine is used by the Chapel runtime to broadcast the
// locations of module-level ("global") variables to all locales
// so that all locales can put/get the value of a global variable
// directly, knowing where it lives remotely.
//
// Logically, this routine implements a collective broadcast of
// the chpl_globals_registry[] array which is an array of 'numGlobals'
// wide_ptr_t values.  Note that in a one-sided implementation, the
// implementation should not assume that chpl_globals_registry[] lives
// at the same address on every compute node.
// 
void chpl_comm_broadcast_global_vars(int numGlobals);

//
// This routine is used by the generated Chapel code to broadcast
// the values of module-level ("global") constants to all compute
// nodes so that tasks can refer to the constants locally without
// communication.
//
// Logically, this routine implements a 1-sided broadcast of a value
// across all the compute nodes.  Only one task total will call into
// this routine per logical broadcast.  For that reason, this routine
// will tend to need to be implemented by utilizing an active message
// (or equivalent) on the remote side.
//
// The job of this task is to broadcast 'size' bytes stored at the
// address indicated by chpl_private_broadcast_table[id] to all of the
// other compute nodes.  On those compute nodes, the result should be
// stored in the address stored by that node's copy of
// chpl_private_broadcast_table[id].  This table is used (instead of a
// raw address) in order to support platforms in which the instances
// of the generated C code may store globals at different addresses
// (like Mac OS X).
//
// Note that this routine is currently used only during program
// initialization, so it is arguably not as performance critical as
// other more core communication routines (like puts, gets, forks).
//
// The third argument, 'tid' (type ID) is intended for use when
// targeting heterogeneous architectures where byte swapping may be
// required rather than just copying the 'size' bytes.  It is not
// currently in use on any platforms, but is being retained in the
// event that we wish to re-enable this capability in the future.
// 
void chpl_comm_broadcast_private(int id, int32_t size, int32_t tid);

//
// Barrier for synchronization between all top-level locales; currently
// only used for startup and teardown.  msg is a string that can be used
// for debugging to determine where the barrier is being called.  This
// function may be called from a Chapel task.  As such, if the barrier
// cannot be immediately satisfied, while it waits chpl_comm_barrier()
// must call chpl_task_yield() in order not to monopolize the execution
// resources and prevent making progress.
//
void chpl_comm_barrier(const char *msg);

//
// Do exit processing that has to occur before the tasking layer is
// shut down.  "The "all" parameter is true for normal, collective
// program termination, and false for that done by a single locale
// due to error or a user program halt or exit.
//
// Note: Chapel's program termination is not yet fully thought out.
//
void chpl_comm_pre_task_exit(int all);

//
// Terminate the communication layer.  "The "all" parameter is true
// for normal, collective program termination, and false for that
// done by a single locale due to error or a user program halt or
// exit.  If the communication layer likes to call exit, the exit
// code is provided using the "status" argument; if it doesn't, it
// can simply return and the Chapel program will call exit().
//
// Notes:
//   this function is called last
//   a barrier is invoked before calling into this function
//   Chapel's program termination is not yet fully thought out.
//
void chpl_comm_exit(int all, int status);

//
// put 'size' bytes of local data at 'addr' to remote data at
// 'raddr' on locale 'locale'
// notes:
//   address is arbitrary
//   size and locale are part of p
//
void  chpl_comm_put(void* addr, c_nodeid_t node, void* raddr,
                    int32_t elemSize, int32_t typeIndex, int32_t len,
                    int ln, chpl_string fn);

//
// get 'size' bytes of remote data at 'raddr' on locale 'locale' to
// local data at 'addr'
// notes:
//   address is arbitrary
//   size and locale are part of p
//
void  chpl_comm_get(void *addr, c_nodeid_t node, void* raddr,
                    int32_t elemSize, int32_t typeIndex, int32_t len,
                    int ln, chpl_string fn);

//
// put the number of elements pointed out by count array, with strides pointed
// out by dststrides and srcstrides arrays. These three arrays have to be int32
// to correspond to DefaultRectangularArr.doiBulkTransferStride(). via
// DefaultRectangularArr.doiBulkTransferStrideComm().
// 'elemSize' is the size of each element, used to scale the values in count[0]
//            and strides.
// When comm=gasnet, this function ends up calling gasnet_puts_bulk().
//   More info in: http://www.escholarship.org/uc/item/5hg5r5fs?display=all
//   Proposal for Extending the UPC Memory Copy Library Functions and Supporting 
//   Extensions to GASNet, Version 2.0. Author: Dan Bonachea 
//
void  chpl_comm_put_strd(void* dstaddr, void* dststrides, int32_t dstlocale, 
                     void* srcaddr, void* srcstrides, void* count,
                     int32_t stridelevels, int32_t elemSize, int32_t typeIndex, 
                     int ln, chpl_string fn);

//
// same as chpl_comm_puts(), but do get instead
//
void  chpl_comm_get_strd(void* dstaddr, void* dststrides, int32_t srclocale, 
                     void* srcaddr, void* srcstrides, void* count,
                     int32_t stridelevels, int32_t elemSize, int32_t typeIndex, 
                     int ln, chpl_string fn);

//
// Get a local copy of a wide string.
//
// The local copy is also a wide string pointer, but its addr field points to 
// a locally-allocated char[] and the locale field is set to "here".
// The local char[] buffer is leaked. :(
//
void chpl_gen_comm_wide_string_get(void* addr,
  c_nodeid_t node, void* raddr, int32_t elemSize, int32_t typeIndex, int32_t len,
                                   int ln, chpl_string fn);

//
// remote fork should launch a thread on locale that runs function f
// passing it arg where the size of arg is stored in arg_size
// notes:
//   multiple forks to the same locale should be handled concurrently
//
void chpl_comm_fork(c_nodeid_t node, c_sublocid_t subloc,
                    chpl_fn_int_t fid, void *arg, int32_t arg_size);

//
// non-blocking fork
//
void chpl_comm_fork_nb(c_nodeid_t node, c_sublocid_t subloc,
                       chpl_fn_int_t fid, void *arg, int32_t arg_size);

//
// fast (non-forking) fork (i.e., run in handler)
//
void chpl_comm_fork_fast(c_nodeid_t node, c_sublocid_t subloc,
                         chpl_fn_int_t fid, void *arg, int32_t arg_size);


//
// This call specifies the number of polling tasks that the
// communication layer will need (see just below for a definition).
// The value it returns is passed to chpl_task_init(), in order to
// forewarn the tasking layer whether the comm layer will need a
// polling task.  In the current implementation, it should only
// return 0 or 1.
//
int chpl_comm_numPollingTasks(void);



//
// Comm diagnostics stuff
//
typedef struct _chpl_commDiagnostics {
  uint64_t get;
  uint64_t get_nb;
  uint64_t get_nb_test;
  uint64_t get_nb_wait;
  uint64_t put;
  uint64_t fork;
  uint64_t fork_fast;
  uint64_t fork_nb;
} chpl_commDiagnostics;

void chpl_startVerboseComm(void);
void chpl_stopVerboseComm(void);
void chpl_startVerboseCommHere(void);
void chpl_stopVerboseCommHere(void);

void chpl_startCommDiagnostics(void);
void chpl_stopCommDiagnostics(void);
void chpl_startCommDiagnosticsHere(void);
void chpl_stopCommDiagnosticsHere(void);
void chpl_resetCommDiagnosticsHere(void);
void chpl_getCommDiagnosticsHere(chpl_commDiagnostics *cd);

//
// These are still supported because our extern record support is
//  still a bit lacking.
//
uint64_t chpl_numCommGets(void);
uint64_t chpl_numCommNBGets(void);
uint64_t chpl_numCommTestNBGets(void);
uint64_t chpl_numCommWaitNBGets(void);
uint64_t chpl_numCommPuts(void);
uint64_t chpl_numCommForks(void);
uint64_t chpl_numCommFastForks(void);
uint64_t chpl_numCommNBForks(void);

#else // LAUNCHER

#define chpl_comm_barrier(x)
#define chpl_comm_exit_all(x) exit(x)
#define chpl_comm_exit_any(x) exit(x)

#endif // LAUNCHER

#endif
