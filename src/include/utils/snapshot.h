/*-------------------------------------------------------------------------
 *
 * snapshot.h
 *	  POSTGRES snapshot definition
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/snapshot.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include "access/htup.h"
#include "storage/buf.h"
#include "utils/rel.h"

#include "cdb/cdbdistributedsnapshot.h"  /* DistributedSnapshotWithLocalMapping */


typedef struct SnapshotData *Snapshot;

#define InvalidSnapshot		((Snapshot) NULL)

/*
 * We use SnapshotData structures to represent both "regular" (MVCC)
 * snapshots and "special" snapshots that have non-MVCC semantics.
 * The specific semantics of a snapshot are encoded by the "satisfies"
 * function.
 */
typedef bool (*SnapshotSatisfiesFunc) (Relation relation, HeapTuple htup,
										   Snapshot snapshot, Buffer buffer, SessionMessage sm);

/*
 * Struct representing all kind of possible snapshots.
 *
 * There are several different kinds of snapshots:
 * * Normal MVCC snapshots
 * * MVCC snapshots taken during recovery (in Hot-Standby mode)
 * * Historic MVCC snapshots used during logical decoding
 * * snapshots passed to HeapTupleSatisfiesDirty()
 * * snapshots used for SatisfiesAny, Toast, Self where no members are
 *	 accessed.
 *
 * TODO: It's probably a good idea to split this struct using a NodeTag
 * similar to how parser and executor nodes are handled, with one type for
 * each different kind of snapshot to avoid overloading the meaning of
 * individual fields.
 */
typedef struct SnapshotData
{
	SnapshotSatisfiesFunc satisfies;	/* tuple test function */

	/*
	 * The remaining fields are used only for MVCC snapshots, and are normally
	 * just zeroes in special snapshots.  (But xmin and xmax are used
	 * specially by HeapTupleSatisfiesDirty.)
	 *
	 * An MVCC snapshot can never see the effects of XIDs >= xmax. It can see
	 * the effects of all older XIDs except those listed in the snapshot. xmin
	 * is stored as an optimization to avoid needing to search the XID arrays
	 * for most tuples.
	 */
	TransactionId xmin;			/* all XID < xmin are visible to me */
	TransactionId xmax;			/* all XID >= xmax are invisible to me */

	/*
	 * For normal MVCC snapshot this contains the all xact IDs that are in
	 * progress, unless the snapshot was taken during recovery in which case
	 * it's empty. For historic MVCC snapshots, the meaning is inverted, i.e.
	 * it contains *committed* transactions between xmin and xmax.
	 */
	TransactionId *xip;
	uint32		xcnt;			/* # of xact ids in xip[] */
	/* note: all ids in xip[] satisfy xmin <= xip[i] < xmax */
	int32		subxcnt;		/* # of xact ids in subxip[] */

	/*
	 * For non-historic MVCC snapshots, this contains subxact IDs that are in
	 * progress (and other transactions that are in progress if taken during
	 * recovery). For historic snapshot it contains *all* xids assigned to the
	 * replayed transaction, including the toplevel xid.
	 */
	TransactionId *subxip;
	bool		suboverflowed;	/* has the subxip array overflowed? */
	bool		takenDuringRecovery;	/* recovery-shaped snapshot? */
	bool		copied;			/* false if it's a static snapshot */
	bool		haveDistribSnapshot; /* True if this snapshot is distributed. */

	/*
	 * note: all ids in subxip[] are >= xmin, but we don't bother filtering
	 * out any that are >= xmax
	 */
	CommandId	curcid;			/* in my xact, CID < curcid are visible */
	uint32		active_count;	/* refcount on ActiveSnapshot stack */
	uint32		regd_count;		/* refcount on RegisteredSnapshotList */

	/*
	 * GP: Global information about which transactions are visible for a
	 * distributed transaction, with cached local xids
	 */
	DistributedSnapshotWithLocalMapping	distribSnapshotWithLocalMapping;

} SnapshotData;

/*
 * Result codes for HeapTupleSatisfiesUpdate.  This should really be in
 * tqual.h, but we want to avoid including that file elsewhere.
 */
typedef enum
{
	HeapTupleMayBeUpdated,
	HeapTupleInvisible,
	HeapTupleSelfUpdated,
	HeapTupleUpdated,
	HeapTupleBeingUpdated
} HTSU_Result;

#endif   /* SNAPSHOT_H */
