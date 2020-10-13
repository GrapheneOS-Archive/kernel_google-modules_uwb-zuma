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
 *
 * MCPS schedule interface.
 */

#ifndef NET_MCPS802154_SCHEDULE_H
#define NET_MCPS802154_SCHEDULE_H

#include <linux/list.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <net/netlink.h>

#include <net/mcps802154.h>

struct mcps802154_nl_ranging_request;

/**
 * MCPS802154_DURATION_NO_CHANGE - Do not change duration.
 */
#define MCPS802154_DURATION_NO_CHANGE (-1)

/**
 * enum mcps802154_access_method - Method to implement an access.
 * @MCPS802154_ACCESS_METHOD_IMMEDIATE_RX:
 *	RX as soon as possible, without timeout, with auto-ack.
 * @MCPS802154_ACCESS_METHOD_IMMEDIATE_TX:
 *	TX as soon as possible. Could be with or without ack request (AR).
 * @MCPS802154_ACCESS_METHOD_MULTI:
 *	Multiple frames described in frames table.
 */
enum mcps802154_access_method {
	MCPS802154_ACCESS_METHOD_IMMEDIATE_RX,
	MCPS802154_ACCESS_METHOD_IMMEDIATE_TX,
	MCPS802154_ACCESS_METHOD_MULTI,
};

/**
 * enum mcps802154_access_tx_return_reason - Reason of TX buffer return.
 * @MCPS802154_ACCESS_TX_RETURN_REASON_CONSUMED:
 *	Frame was sent successfully.
 * @MCPS802154_ACCESS_TX_RETURN_REASON_FAILURE:
 *	An attempt was done to deliver the frame, but it failed.
 * @MCPS802154_ACCESS_TX_RETURN_REASON_CANCEL:
 *	No attempt was done to deliver the frame, or there was an unexpected
 *	error doing it.
 */
enum mcps802154_access_tx_return_reason {
	MCPS802154_ACCESS_TX_RETURN_REASON_CONSUMED,
	MCPS802154_ACCESS_TX_RETURN_REASON_FAILURE,
	MCPS802154_ACCESS_TX_RETURN_REASON_CANCEL,
};

/**
 * struct mcps802154_access_frame - Information for a single frame for multiple
 * frames method.
 */
struct mcps802154_access_frame {
	/**
	 * @is_tx: True if frame is TX, else RX.
	 */
	bool is_tx;
	union {
		/**
		 * @tx_frame_info: Information for transmitting a frame. Should
		 * have rx_enable_after_tx_dtu == 0.
		 */
		struct mcps802154_tx_frame_info tx_frame_info;
		/**
		 * @rx: Information for receiving a frame.
		 */
		struct {
			/**
			 * @info: Information for enabling the receiver.
			 */
			struct mcps802154_rx_info info;
			/**
			 * @frame_info_flags_request: Information to request
			 * when a frame is received, see
			 * &enum mcps802154_rx_frame_info_flags.
			 */
			u16 frame_info_flags_request;
		} rx;
	};
};

/**
 * struct mcps802154_access - Single medium access.
 *
 * This structure gives MCPS all the information needed to perform a single
 * access.
 */
struct mcps802154_access {
	/**
	 * @method: Method of access, see &enum mcps802154_access_method.
	 */
	enum mcps802154_access_method method;
	/**
	 * @ops: Callbacks to implement the access.
	 */
	struct mcps802154_access_ops *ops;
	/**
	 * @n_frames: Number of frames in an access using multiple frames
	 * method. This can be changed by the &mcps802154_access_ops.rx_frame()
	 * callback.
	 */
	size_t n_frames;
	/**
	 * @frames: Table of information for each frames in an access using
	 * multiple frames method. This can be changed by the
	 * &mcps802154_access_ops.rx_frame() callback.
	 */
	struct mcps802154_access_frame *frames;
};

/**
 * struct mcps802154_access_ops - Callbacks to implement an access.
 */
struct mcps802154_access_ops {
	/**
	 * @rx_frame: Once a frame is received, it is given to this function.
	 * Buffer ownership is transferred to the callee.
	 *
	 * For multiple frames access method, this is called with NULL pointers
	 * to report RX timeout or error.
	 */
	void (*rx_frame)(struct mcps802154_access *access, int frame_idx,
			 struct sk_buff *skb,
			 const struct mcps802154_rx_frame_info *info);
	/**
	 * @tx_get_frame: Return a frame to send, the buffer is lend to caller
	 * and should be returned with &tx_return().
	 */
	struct sk_buff *(*tx_get_frame)(struct mcps802154_access *access,
					int frame_idx);
	/**
	 * @tx_return: Give back an unmodified buffer.
	 */
	void (*tx_return)(struct mcps802154_access *access, int frame_idx,
			  struct sk_buff *skb,
			  enum mcps802154_access_tx_return_reason reason);
	/**
	 * @access_done: Called when the access is done, successfully or not.
	 */
	void (*access_done)(struct mcps802154_access *access);
};

/**
 * struct mcps802154_region - Region as defined in the schedule.
 */
struct mcps802154_region {
	/**
	 * @start_dtu: Region start from the start of the schedule.
	 */
	int start_dtu;
	/**
	 * @duration_dtu: Region duration or 0 for endless region.
	 */
	int duration_dtu;
	/**
	 * @ops: Callbacks for the region.
	 */
	const struct mcps802154_region_ops *ops;
	/**
	 * @orh: Pointer to the open region handler.
	 */
	struct mcps802154_open_region_handler *orh;
};

/**
 * struct mcps802154_region_ops - Region callbacks, handle access for a specific
 * region in schedule.
 */
struct mcps802154_region_ops {
	/**
	 * @name: Region name.
	 */
	const char *name;
	/**
	 * @alloc: Allocate a region.
	 */
	struct mcps802154_region *(*alloc)(
		struct mcps802154_open_region_handler *orh);
	/**
	 * @get_access: Get access for a given region at the given timestamp.
	 * Access is valid until &mcps802154_access_ops.access_done() callback
	 * is called.
	 */
	struct mcps802154_access *(*get_access)(
		const struct mcps802154_region *region, u32 next_timestamp_dtu,
		int next_in_region_dtu);
	/**
	 * @free: Release region.
	 */
	void (*free)(struct mcps802154_region *region);
};

/**
 * struct mcps802154_schedule_update - Context valid during a schedule
 * update.
 */
struct mcps802154_schedule_update {
	/**
	 * @expected_start_timestamp_dtu: Expected start timestamp, based on the
	 * current access date and having the new schedule put right after the
	 * old one.
	 */
	u32 expected_start_timestamp_dtu;
	/**
	 * @start_timestamp_dtu: Date of the schedule start, might be too far in
	 * the past for endless schedule.
	 */
	u32 start_timestamp_dtu;
	/**
	 * @duration_dtu: Schedule duration or 0 for endless schedule. This is
	 * also 0 when the schedule is empty.
	 */
	int duration_dtu;
	/**
	 * @n_regions: Number of regions in the schedule.
	 */
	size_t n_regions;
};

/**
 * struct mcps802154_open_region_handler - A region handler instance attached to
 * a device.
 */
struct mcps802154_open_region_handler {
	/**
	 * @handler: Pointer to region handler.
	 */
	struct mcps802154_region_handler *handler;
	/**
	 * @open_entry: Entry in list of open region handlers.
	 */
	struct list_head open_entry;
};

/**
 * struct mcps802154_region_handler - A region handler is responsible to manage
 * regions for a MCPS device, there is one region handler instance for each
 * MCPS instance where it is used.
 */
struct mcps802154_region_handler {
	/**
	 * @owner: Module owning this handler, should be THIS_MODULE in most
	 * cases.
	 */
	struct module *owner;
	/**
	 * @name: Region handler name.
	 */
	const char *name;
	/**
	 * @registered_entry: Entry in list of registered region handlers.
	 */
	struct list_head registered_entry;
	/**
	 * @n_regions_ops: Number of region ops.
	 */
	size_t n_regions_ops;
	/**
	 * @regions_ops: Array of region ops.
	 */
	const struct mcps802154_region_ops *const *regions_ops;
	/**
	 * @open: Attach a region handler to a device.
	 */
	struct mcps802154_open_region_handler *(*open)(
		struct mcps802154_llhw *llhw);
	/**
	 * @close: Detach and close a region handler.
	 */
	void (*close)(struct mcps802154_open_region_handler *orh);
	/**
	 * @update_schedule: Called to initialize and update the schedule.
	 */
	int (*update_schedule)(
		struct mcps802154_open_region_handler *orh,
		const struct mcps802154_schedule_update *schedule_update,
		u32 next_timestamp_dtu);
	/**
	 * @set_parameters: Called to configure the region handler.
	 */
	int (*set_parameters)(struct mcps802154_open_region_handler *orh,
			      const struct nlattr *attrs,
			      struct netlink_ext_ack *extack);
	/**
	 * @ranging_setup: Called to configure ranging. This is a temporary
	 * interface.
	 */
	int (*ranging_setup)(
		struct mcps802154_open_region_handler *orh,
		const struct mcps802154_nl_ranging_request *requests,
		unsigned int n_requests);
};

/**
 * mcps802154_region_handler_register() - Register a region handler, to be
 * called when your module is loaded.
 * @region_handler: Region handler to register.
 *
 * Return: 0 or error.
 */
int mcps802154_region_handler_register(
	struct mcps802154_region_handler *region_handler);

/**
 * mcps802154_region_handler_unregister() - Unregister a region handler, to be
 * called at module unloading.
 * @region_handler: Region handler to unregister.
 */
void mcps802154_region_handler_unregister(
	struct mcps802154_region_handler *region_handler);

/**
 * mcps802154_schedule_set_start() - Change the currently updated schedule start
 * timestamp.
 * @schedule_update: Schedule update context.
 * @start_timestamp_dtu: New start timestamp.
 *
 * Return: 0 or -EINVAL if arguments are garbage.
 */
int mcps802154_schedule_set_start(
	const struct mcps802154_schedule_update *schedule_update,
	u32 start_timestamp_dtu);

/**
 * mcps802154_schedule_recycle() - Purge or recycle the current schedule.
 * @schedule_update: Schedule update context.
 * @n_keeps: Number of regions to keep from the previous schedule.
 * @last_region_duration_dtu:
 *	Duration of the last region, or MCPS802154_DURATION_NO_CHANGE to keep it
 *	unchanged.
 *
 * Return: 0 or -EINVAL if arguments are garbage.
 */
int mcps802154_schedule_recycle(
	const struct mcps802154_schedule_update *schedule_update,
	size_t n_keeps, int last_region_duration_dtu);

/**
 * mcps802154_schedule_add_region() - Add a new region to the currently updated
 * schedule.
 * @schedule_update: Schedule update context.
 * @region_ops_idx: Region index in region handler region_ops table.
 * @start_dtu: Region start from the start of the schedule.
 * @duration_dtu: Region duration, or 0 for endless region.
 *
 * The new region should be a region managed by the schedule region handler, or
 * else another API should be used (TODO).
 *
 * Return: The new region, or NULL on error.
 */
struct mcps802154_region *mcps802154_schedule_add_region(
	const struct mcps802154_schedule_update *schedule_update,
	size_t region_ops_idx, int start_dtu, int duration_dtu);

/**
 * mcps802154_schedule_invalidate() - Request to invalidate the schedule.
 * @llhw: Low-level device pointer.
 *
 * FSM mutex should be locked.
 *
 * Invalidate the current schedule, which will result on a schedule change.
 * This API should be called from external regions to force schedule change,
 * when for example some parameters changed.
 */
void mcps802154_schedule_invalidate(struct mcps802154_llhw *llhw);

#endif /* NET_MCPS802154_SCHEDULE_H */
