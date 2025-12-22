/* SPDX-License-Identifier: BSD-3-Clause */

#include "bdev_policy_watcher.h"
#include "spdk/log.h"

struct bdev_policy_watcher {
	struct bdev_policy_watcher_opts opts;

	bdev_policy_measure_fn  measure_fn;
	bdev_policy_evaluate_fn evaluate_fn;
	void *ctx;

	struct spdk_poller *poller;

	TAILQ_HEAD(, bdev_policy_sample) samples;
	uint32_t sample_count;
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

static int
_policy_poller(void *arg)
{
	struct bdev_policy_watcher *w = arg;
	uint64_t now = spdk_get_ticks();

	/* Measure */
	double value = w->measure_fn(w->ctx);

	struct bdev_policy_sample *s = calloc(1, sizeof(*s));
	if (!s) {
		SPDK_ERRLOG("Policy watcher OOM\n");
		return SPDK_POLLER_BUSY;
	}

	s->timestamp_ticks = now;
	s->value = value;
	TAILQ_INSERT_TAIL(&w->samples, s, link);
	w->sample_count++;

	/* Prune */
	_prune_old_samples(w, now);

	/* Evaluate if window is valid */
	if (w->sample_count >= w->opts.min_samples) {
		w->evaluate_fn(TAILQ_FIRST(&w->samples),
			       w->sample_count,
			       w->ctx);
	}

	return SPDK_POLLER_BUSY;
}

struct bdev_policy_watcher *
bdev_policy_watcher_create(const struct bdev_policy_watcher_opts *opts,
			   bdev_policy_measure_fn measure_fn,
			   bdev_policy_evaluate_fn evaluate_fn,
			   void *ctx)
{
	struct bdev_policy_watcher *w;

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