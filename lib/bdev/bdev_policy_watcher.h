/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef SPDK_BDEV_POLICY_WATCHER_H
#define SPDK_BDEV_POLICY_WATCHER_H

#include "spdk/stdinc.h"
#include "spdk/queue.h"
#include "spdk/thread.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bdev_policy_watcher;

/*
 * One measurement sample
 * You can extend this later if needed (percentiles, error codes, etc.)
 */
struct bdev_policy_sample {
	uint64_t timestamp_ticks;
	double   value;
	TAILQ_ENTRY(bdev_policy_sample) link;
};

/*
 * Called periodically to obtain a measurement.
 * Should be fast and non-blocking.
 */
typedef double (*bdev_policy_measure_fn)(void *ctx);

/*
 * Called when a valid window exists.
 * Return value is policy-defined (e.g., violation / ok).
 */
typedef void (*bdev_policy_evaluate_fn)(
	const struct bdev_policy_sample *samples,
	uint32_t sample_count,
	void *ctx
);

/*
 * Parameters controlling the watcher behavior
 */
struct bdev_policy_watcher_opts {
	uint64_t window_duration_ticks;
	uint32_t min_samples;
	uint64_t evaluation_interval_ticks;
};

/*
 * Create / destroy
 */
struct bdev_policy_watcher *
bdev_policy_watcher_create(const struct bdev_policy_watcher_opts *opts,
			   bdev_policy_measure_fn measure_fn,
			   bdev_policy_evaluate_fn evaluate_fn,
			   void *ctx);

void bdev_policy_watcher_destroy(struct bdev_policy_watcher *watcher);

/*
 * Control
 */
void bdev_policy_watcher_start(struct bdev_policy_watcher *watcher);
void bdev_policy_watcher_stop(struct bdev_policy_watcher *watcher);

#ifdef __cplusplus
}
#endif

#endif /* SPDK_BDEV_POLICY_WATCHER_H */
