#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x2005afab, "module_layout" },
	{ 0x4829a47e, "memcpy" },
	{ 0xdcb764ad, "memset" },
	{ 0xc2c193d2, "__stack_chk_fail" },
	{ 0xc56a41e6, "vabits_actual" },
	{ 0x5fafdd6b, "bpf_trace_run4" },
	{ 0x690f4f8d, "bpf_trace_run3" },
	{ 0x4f5fc34a, "bpf_trace_run2" },
	{ 0x6faa5b1a, "perf_trace_run_bpf_submit" },
	{ 0x2d2c902f, "perf_trace_buf_alloc" },
	{ 0x7381287f, "trace_handle_return" },
	{ 0xcc5c2df4, "trace_print_symbols_seq" },
	{ 0x9c622021, "trace_event_printf" },
	{ 0x82aff896, "trace_raw_output_prep" },
	{ 0xcc8b41c0, "trace_event_ignore_this_pid" },
	{ 0xd027679a, "event_triggers_call" },
	{ 0x2e0c6d75, "trace_event_buffer_commit" },
	{ 0x458239b0, "trace_event_buffer_reserve" },
	{ 0xa0e6bc50, "trace_event_raw_init" },
	{ 0x34dfefae, "trace_event_reg" },
	{ 0xddd89543, "seq_printf" },
	{ 0xc1df7f24, "single_open" },
	{ 0xaac6af16, "single_release" },
	{ 0xd91e92ff, "seq_read" },
	{ 0x85c1ba02, "seq_lseek" },
	{ 0x52532c40, "no_llseek" },
	{ 0x7d7124b9, "fsnotify" },
	{ 0x7f415d8d, "__fsnotify_parent" },
	{ 0xdf256037, "kstrtou8_from_user" },
	{ 0x619cb7dd, "simple_read_from_buffer" },
	{ 0x19644581, "debugfs_remove" },
	{ 0x848b5eb4, "debugfs_create_file" },
	{ 0x5cfd2e3c, "debugfs_create_dir" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x72cd0f7f, "mod_timer" },
	{ 0xf15e728e, "del_timer_sync" },
	{ 0x79cf5987, "init_timer_key" },
	{ 0x98cf60b3, "strlen" },
	{ 0xe914e41e, "strcpy" },
	{ 0x92997ed8, "_printk" },
	{ 0xac594e02, "__cpu_online_mask" },
	{ 0x7a2af7b4, "cpu_number" },
	{ 0x8ddd8aad, "schedule_timeout" },
	{ 0xe1537255, "__list_del_entry_valid" },
	{ 0xf70e4a4d, "preempt_schedule_notrace" },
	{ 0xa6257a2f, "complete" },
	{ 0x92540fbf, "finish_wait" },
	{ 0x1000e51, "schedule" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0xc8dcc62a, "krealloc" },
	{ 0x3eeb2322, "__wake_up" },
	{ 0x362e864b, "kthread_stop" },
	{ 0xc2f680b8, "wake_up_process" },
	{ 0xb5d09ac4, "kthread_create_on_node" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xb4d5a7cd, "_dev_warn" },
	{ 0x68f31cbd, "__list_add_valid" },
	{ 0x2dce3a69, "spi_sync" },
	{ 0xc6d09aa9, "release_firmware" },
	{ 0x6ae259fd, "request_firmware" },
	{ 0x656e4a6e, "snprintf" },
	{ 0x41379bd2, "kmalloc_caches" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x7d1fbd0c, "kmem_cache_alloc_trace" },
	{ 0x37a0cba, "kfree" },
	{ 0xa571f6b0, "__mutex_init" },
	{ 0xed55cabd, "mutex_unlock" },
	{ 0xd5977bfb, "mutex_lock" },
	{ 0x4b0a3f52, "gic_nonsecure_priorities" },
	{ 0x77c1b75a, "cpu_hwcaps" },
	{ 0xd14fef22, "cpu_hwcap_keys" },
	{ 0x14b89635, "arm64_const_caps_ready" },
	{ 0x8d5995f7, "param_ops_int" },
	{ 0x49ebeb83, "param_ops_bool" },
	{ 0xdf7a4c69, "__ubsan_handle_cfi_check_fail_abort" },
	{ 0xf75e608, "driver_unregister" },
	{ 0xb5b54b34, "_raw_spin_unlock" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0x29307bca, "irq_get_irq_data" },
	{ 0xdefc725d, "devm_request_threaded_irq" },
	{ 0xc3055d20, "usleep_range_state" },
	{ 0xd35cce70, "_raw_spin_unlock_irqrestore" },
	{ 0x34db050b, "_raw_spin_lock_irqsave" },
	{ 0xdd77ef5b, "gpiod_set_value" },
	{ 0x83edb2d5, "gpiod_get_value" },
	{ 0xfcec0987, "enable_irq" },
	{ 0x27bbf221, "disable_irq_nosync" },
	{ 0x6aab3755, "gpiod_to_irq" },
	{ 0x4411a445, "__cfi_slowpath_diag" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x25974000, "wait_for_completion" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x5ddc25cc, "devm_kmalloc" },
	{ 0x257f0e35, "misc_deregister" },
	{ 0xf9a482f9, "msleep" },
	{ 0x22d43362, "_dev_info" },
	{ 0xfc19406c, "misc_register" },
	{ 0x2ab1e408, "devm_gpiod_get_optional" },
	{ 0xb2091ff2, "_dev_err" },
	{ 0xa9f4bf3b, "spi_setup" },
	{ 0x789a4491, "__spi_register_driver" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cqorvo,qm35");
MODULE_ALIAS("of:N*T*Cqorvo,qm35C*");

MODULE_INFO(scmversion, "gf701e054b052-dirty");
