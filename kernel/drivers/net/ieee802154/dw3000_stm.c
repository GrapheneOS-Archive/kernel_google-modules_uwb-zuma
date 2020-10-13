/*
 * This file is part of the UWB stack for linux.
 *
 * Copyright (c) 2020 Qorvo US, Inc.
 *
 * This software is provided under the GNU General Public License, version 2
 * (GPLv2), as well as under a Qorvo commercial license.
 *
 * You may choose to use this software under the terms of the GPLv2 License,
 * version 2 ("GPLv2"), as published by the Free Software Foundation.
 * You should have received a copy of the GPLv2 along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * This program is distributed under the GPLv2 in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GPLv2 for more
 * details.
 *
 * If you cannot meet the requirements of the GPLv2, you may not use this
 * software for any purpose without first obtaining a commercial license from
 * Qorvo.
 * Please contact Qorvo to inquire about licensing terms.
 */
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/sched.h>

#include "dw3000.h"
#include "dw3000_core.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0))
#include <uapi/linux/sched/types.h>
#endif
static inline void dw3000_set_fifo_sched(struct task_struct *p)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0))
	struct sched_param sched_par = { .sched_priority = MAX_RT_PRIO - 2 };
	/* Increase thread priority */
	sched_setscheduler(p, SCHED_FIFO, &sched_par);
#else
	/* Priority must be set by user-space now. */
	sched_set_fifo(p);
#endif
}

/* Enqueue work item(s) */
void dw3000_enqueue(struct dw3000 *dw, unsigned long work)
{
	struct dw3000_state *stm = &dw->stm;
	unsigned long flags;

	spin_lock_irqsave(&stm->work_wq.lock, flags);
	stm->pending_work |= work;
	wake_up_locked(&stm->work_wq);
	spin_unlock_irqrestore(&stm->work_wq.lock, flags);
}

/* Enqueue a generic work and wait for execution */
int dw3000_enqueue_generic(struct dw3000 *dw, struct dw3000_stm_command *cmd)
{
	struct dw3000_state *stm = &dw->stm;
	unsigned long flags;
	int work = DW3000_COMMAND_WORK;
	if (current == stm->mthread) {
		/* We can't enqueue a new work from the same context and wait,
		   but it can be executed directly instead. */
		return cmd->cmd(dw, cmd->in, cmd->out);
	}
	/* Slow path if not in STM thread context */
	spin_lock_irqsave(&stm->work_wq.lock, flags);
	stm->pending_work |= work;
	stm->generic_work = cmd;
	wake_up_locked(&stm->work_wq);
	wait_event_interruptible_locked_irq(stm->work_wq,
					    !(stm->pending_work & work));
	spin_unlock_irqrestore(&stm->work_wq.lock, flags);
	return cmd->ret;
}

/* Dequeue work item(s) */
void dw3000_dequeue(struct dw3000 *dw, unsigned long work)
{
	struct dw3000_state *stm = &dw->stm;
	unsigned long flags;

	spin_lock_irqsave(&stm->work_wq.lock, flags);
	stm->pending_work &= ~work;
	wake_up_locked(&stm->work_wq);
	spin_unlock_irqrestore(&stm->work_wq.lock, flags);
}

/* Enqueue IRQ work */
void dw3000_enqueue_irq(struct dw3000 *dw)
{
	struct dw3000_state *stm = &dw->stm;
	unsigned long flags;

	spin_lock_irqsave(&stm->work_wq.lock, flags);
	if (!(stm->pending_work & DW3000_IRQ_WORK)) {
		stm->pending_work |= DW3000_IRQ_WORK;
		disable_irq_nosync(dw->spi->irq);
	}
	wake_up_locked(&stm->work_wq);
	spin_unlock_irqrestore(&stm->work_wq.lock, flags);
}

void dw3000_clear_irq(struct dw3000 *dw)
{
	struct dw3000_state *stm = &dw->stm;
	unsigned long flags;

	spin_lock_irqsave(&stm->work_wq.lock, flags);
	stm->pending_work &= ~DW3000_IRQ_WORK;
	enable_irq(dw->spi->irq);
	spin_unlock_irqrestore(&stm->work_wq.lock, flags);
}

/* Wait for new work in the queue */
void dw3000_wait_pending_work(struct dw3000 *dw)
{
	struct dw3000_state *stm = &dw->stm;
	unsigned long flags;

	spin_lock_irqsave(&stm->work_wq.lock, flags);
	wait_event_interruptible_locked_irq(
		stm->work_wq, stm->pending_work || kthread_should_stop());
	spin_unlock_irqrestore(&stm->work_wq.lock, flags);
}

/* Read work queue state */
unsigned long dw3000_get_pending_work(struct dw3000 *dw)
{
	struct dw3000_state *stm = &dw->stm;
	unsigned long work;
	unsigned long flags;

	spin_lock_irqsave(&stm->work_wq.lock, flags);
	work = stm->pending_work;
	spin_unlock_irqrestore(&stm->work_wq.lock, flags);

	return work;
}

/* Init work run inside the thread below */
int dw3000_init_work(struct dw3000 *dw, void *in, void *out)
{
	int rc;
	/* Initialize & configure the device */
	if ((rc = dw3000_init(dw)) != 0) {
		dev_err(dw->dev, "device init failed: %d\n", rc);
		return rc;
	}
	return 0;
}

/* Event handling thread function */
int dw3000_event_thread(void *data)
{
	struct dw3000 *dw = data;
	struct dw3000_state *stm = &dw->stm;
	unsigned long pending_work = 0;

	/* Run until stopped */
	while (!kthread_should_stop()) {
		/* TODO: State independent errors (ex: PLL_HILO) */

		/* Pending work items */
		pending_work = dw3000_get_pending_work(dw);

		/* TODO: SPI/HW errors.
		 * Every function that uses SPI transmission must enqueue
		 * DW3000_ERROR_WORK item if any error occurs.
		 */

		/* Check IRQ activity */
		if (pending_work & DW3000_IRQ_WORK) {
			/* Handle the event in the ISR */
			dw3000_isr(dw);
			dw3000_clear_irq(dw);
			continue;
		}

		/* In nearly all states, we can execute generic works. */
		if (pending_work & DW3000_COMMAND_WORK) {
			struct dw3000_stm_command *cmd = stm->generic_work;
			bool is_init_work = cmd->cmd == dw3000_init_work;
			cmd->ret = cmd->cmd(dw, cmd->in, cmd->out);
			dw3000_dequeue(dw, DW3000_COMMAND_WORK);
			if (unlikely(is_init_work)) {
				/* Run testmode if enabled after dw3000_init_work. */
				dw3000_testmode(dw);
			}
		}

		if (!pending_work) {
			/* Wait for more work */
			dw3000_wait_pending_work(dw);
		}
	}

	/* Make sure device is off */
	dw3000_remove(dw);
	/* Power down the device */
	dw3000_poweroff(dw);

	dev_dbg(dw->dev, "thread finished\n");
	return 0;
}

/* Prepare state machine */
int dw3000_state_init(struct dw3000 *dw, unsigned int cpu)
{
	struct dw3000_state *stm = &dw->stm;
	/* Clear memory */
	memset(stm, 0, sizeof(*stm));

	/* Wait queues */
	init_waitqueue_head(&stm->work_wq);

	/* SKIP: Setup timers (state timeout and ADC timers) */

	/* Init event handler thread */
	stm->mthread = kthread_create(dw3000_event_thread, dw, "dw3000-%s",
				      dev_name(dw->dev));
	if (IS_ERR(stm->mthread)) {
		return PTR_ERR(stm->mthread);
	}
	kthread_bind(stm->mthread, cpu);

	/* Increase thread priority */
	dw3000_set_fifo_sched(stm->mthread);
	return 0;
}

/* Start state machine */
int dw3000_state_start(struct dw3000 *dw)
{
	struct dw3000_stm_command cmd = { dw3000_init_work, NULL, NULL };

	/* Start state machine thread */
	wake_up_process(dw->stm.mthread);
	dev_dbg(dw->dev, "state machine started\n");

	/* Do initialisation and return result to caller */
	return dw3000_enqueue_generic(dw, &cmd);
}

/* Stop state machine */
int dw3000_state_stop(struct dw3000 *dw)
{
	struct dw3000_state *stm = &dw->stm;

	/* Stop state machine thread */
	kthread_stop(stm->mthread);

	dev_dbg(dw->dev, "state machine stopped\n");
	return 0;
}