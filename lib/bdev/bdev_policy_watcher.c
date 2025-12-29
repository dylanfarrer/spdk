/* SPDX-License-Identifier: BSD-3-Clause */

#include "bdev_policy_watcher.h"
#include "spdk/log.h"

// Definition of the policy watcher structure
struct bdev_policy_watcher {
	struct bdev_policy_watcher_opts opts; 		// Opts set by user

	bdev_policy_measure_fn  measure_fn;  		// function to measure the metric
	bdev_policy_evaluate_fn evaluate_fn; 		// function to evaluate policy adherence
	void *ctx;                          		// context for the functions

	struct spdk_poller *poller;          		// poller to implement periodic calling

	TAILQ_HEAD(, bdev_policy_sample) samples; 	// list of samples taken
	uint32_t sample_count;						// sample count
};

static void
_prune_old_samples(struct bdev_policy_watcher *w, uint64_t now)
{
	while (!TAILQ_EMPTY(&w->samples)) {
		struct bdev_policy_sample *s = TAILQ_FIRST(&w->samples);
		if (now - s->timestamp_ticks <= w->opts.window_duration_ticks) {
			break;
		}
		TAILQ_REMOVE(&w->samples, s, link);
		free(s);
		w->sample_count--;
	}
}

// This is actually what the poller calls periodically (every X ticks)
// arg is just the watcher structure
static int
_policy_poller(void *arg)
{
	struct bdev_policy_watcher *w = arg;
	uint64_t now = spdk_get_ticks();

	bool success;

	// Measure
	double value = w->measure_fn(w->ctx, &success);

	if (success) {
		struct bdev_policy_sample *s = calloc(1, sizeof(*s));
		if (!s) {
			SPDK_ERRLOG("Policy watcher OOM\n");
			return SPDK_POLLER_BUSY;
		}

		s->timestamp_ticks = now;
		s->value = value;
		TAILQ_INSERT_TAIL(&w->samples, s, link);
		w->sample_count++;
	}

	// Prune outdated samples
	_prune_old_samples(w, now);

	// Evaluate only f window is valid
	if (w->sample_count >= w->opts.min_samples) {
		w->evaluate_fn(TAILQ_FIRST(&w->samples),
			       w->sample_count,
			       w->ctx);
	}

	return SPDK_POLLER_BUSY;
}

/* Create a new policy watcher, based on options, a measurement and evaluation function and some context */
struct bdev_policy_watcher *
bdev_policy_watcher_create(const struct bdev_policy_watcher_opts *opts,
			   bdev_policy_measure_fn measure_fn,
			   bdev_policy_evaluate_fn evaluate_fn,
			   void *ctx)
{
	struct bdev_policy_watcher *w;

	// init and zero the structure
	w = calloc(1, sizeof(*w));
	if (!w) {
		return NULL;
	}

	w->opts = *opts;
	w->measure_fn = measure_fn;
	w->evaluate_fn = evaluate_fn;
	w->ctx = ctx;

	TAILQ_INIT(&w->samples);

	return w;
}

void
bdev_policy_watcher_start(struct bdev_policy_watcher *w)
{
	if (w->poller) {
		return;
	}

	w->poller = spdk_poller_register(_policy_poller,
					 w,
					 w->opts.evaluation_interval_ticks);
}

void
bdev_policy_watcher_stop(struct bdev_policy_watcher *w)
{
	if (w->poller) {
		spdk_poller_unregister(&w->poller);
	}
}

// Destroy a policy watcher, freeing all resources
void
bdev_policy_watcher_destroy(struct bdev_policy_watcher *w)
{
	struct bdev_policy_sample *s;

	bdev_policy_watcher_stop(w);

	while (!TAILQ_EMPTY(&w->samples)) {
		s = TAILQ_FIRST(&w->samples);
		TAILQ_REMOVE(&w->samples, s, link);
		free(s);
	}

	free(w);
}




// possible usage:


/*
static double
measure_latency(void *ctx)
{
	struct my_mirror *m = ctx;
	return m->last_latency_us;
}


static void
evaluate_latency_policy(const struct bdev_policy_sample *samples,
			 uint32_t count,
			 void *ctx)
{
	struct my_mirror *m = ctx;
	uint32_t violations = 0;

	const struct bdev_policy_sample *s;
	TAILQ_FOREACH(s, (void *)samples, link) {
		if (s->value > m->latency_slo_us) {
			violations++;
		}
	}

	if (violations > count / 2) {
		mirror_mark_failed(m);
	} else {
		mirror_mark_healthy(m);
	}
}


struct bdev_policy_watcher_opts opts = {
	.window_duration_ticks =
		spdk_get_ticks_hz() * 10, //10 seconds 
	.min_samples = 5,
	.evaluation_interval_ticks =
		spdk_get_ticks_hz() / 2, //500 ms
};

mirror->policy =
	bdev_policy_watcher_create(&opts,
				   measure_latency,
				   evaluate_latency_policy,
				   mirror);

bdev_policy_watcher_start(mirror->policy);



*/