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

// Sample of a measured metric at a point in time
struct bdev_policy_sample {
	uint64_t timestamp_ticks;             	// When the sample was taken
	double   value;                       	// Measured value
	bool     in_violation;				 	// Whether this sample violated the policy (tbd by evaluate fn)
	TAILQ_ENTRY(bdev_policy_sample) link; 	// Link in samples list
};

typedef double (*bdev_policy_measure_fn)(void *ctx, bool *success); // function to measure the metric

/*
 * Called when a valid window exists. (if window of X ticks has contained at least Y samples)
 * Return value is policy-defined (e.g., violation / ok).
 */
typedef void (*bdev_policy_evaluate_fn)(
	const struct bdev_policy_sample *samples,
	uint32_t sample_count,
	void *ctx
); // function to evaluate policy adherence

/*
 * Parameters controlling the watcher behavior, to be set at bdev creation time
 */
struct bdev_policy_watcher_opts {
	uint64_t window_duration_ticks; 	     // Duration of the sliding window
	uint32_t min_samples;                    // Min sample count to validate a window
	uint64_t evaluation_interval_ticks;      // How often to evaluate the policy
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
