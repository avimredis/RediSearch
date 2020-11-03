
#pragma once

#include "gc.h"
#include "redismodule.h"
#include "object.h"

///////////////////////////////////////////////////////////////////////////////////////////////

struct ForkGCStats {
  // total bytes collected by the GC
  size_t totalCollected;
  // number of cycle ran
  size_t numCycles;

  long long totalMSRun;
  long long lastRunTimeMs;

  uint64_t gcNumericNodesMissed;
  uint64_t gcBlocksDenied;
};

//---------------------------------------------------------------------------------------------

enum FGCType {
  FGC_TYPE_INKEYSPACE,
  FGC_TYPE_NOKEYSPACE
};

//---------------------------------------------------------------------------------------------

// Internal definition of the garbage collector context (each index has one)
struct ForkGC : public Object, public GCAPI {
  ForkGC(const RedisModuleString *k, uint64_t specUniqueId);
  ForkGC(IndexSpec *sp, uint64_t specUniqueId);

  void ctor(const RedisModuleString *k, uint64_t specUniqueId);

  // inverted index key name for reopening the index
  union {
    const RedisModuleString *keyName;
    IndexSpec *sp;
  };

  RedisModuleCtx *ctx;

  FGCType type;

  uint64_t specUniqueId;

  // statistics for reporting
  ForkGCStats stats;

  // flag for rdb loading. Set to 1 initially, but unce it's set to 0 we don't need to check anymore
  int rdbPossiblyLoading;
  // Whether the gc has been requested for deletion
  volatile int deleting;
  int pipefd[2];
  volatile uint32_t pauseState;
  volatile uint32_t execState;

  struct timespec retryInterval;
  volatile size_t deletedDocsFromLastRun;

  virtual int PeriodicCallback(RedisModuleCtx* ctx);
  virtual void RenderStats(RedisModuleCtx* ctx);
  virtual void OnDelete();
  virtual void OnTerm();
  virtual void Kill();
  virtual struct timespec GetInterval();
  virtual RedisModuleCtx *GetRedisCtx() { return ctx; }

  // Indicate that the gc should wait immediately prior to forking. 
  // This is in order to perform some commands which may not be visible by the fork gc engine.
  // This function will return before the fork is performed. 
  // You must call WaitAtApply or WaitClear to allow the GC to resume functioning.
  void WaitAtFork();

  // Indicate that the GC should unpause from WaitAtFork, and instead wait before the changes are applied.
  // This is in order to change the state of the index at the parent.
  void WaitAtApply();

  // Don't perform diagnostic waits
  void WaitClear();
};

//---------------------------------------------------------------------------------------------

enum FGCPauseFlags {
  // Normal "open" state. No pausing will happen
  FGC_PAUSED_UNPAUSED = 0x00,
  // Prevent invoking the child. The child is not invoked until this flag is
  // cleared
  FGC_PAUSED_CHILD = 0x01,
  // Prevent the parent reading from the child. The results from the child are
  // not read until this flag is cleared.
  FGC_PAUSED_PARENT = 0x02
};

//---------------------------------------------------------------------------------------------

enum FGCState {
  // Idle, "normal" state
  FGC_STATE_IDLE = 0,

  // Set when the PAUSED_CHILD flag is set, indicates that we are
  // awaiting this flag to be cleared.
  FGC_STATE_WAIT_FORK,

  // Set when the child has been launched, but before the first results have
  // been applied.
  FGC_STATE_SCANNING,

  // Set when the PAUSED_PARENT flag is set. The results will not be
  // scanned until the PAUSED_PARENT flag is unset
  FGC_STATE_WAIT_APPLY,

  // Set when results are being applied from the child to the parent
  FGC_STATE_APPLYING
};

///////////////////////////////////////////////////////////////////////////////////////////////
