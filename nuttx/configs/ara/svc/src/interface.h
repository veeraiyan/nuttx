/*
 * Copyright (c) 2015-2016 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @brief: Manages an Ara interface block
 */

#ifndef  _INTERFACE_H_
#define  _INTERFACE_H_

#include <errno.h>
#include <time.h>

#include <nuttx/wqueue.h>
#include <nuttx/wdog.h>
#include <pthread.h>

#include "vreg.h"

/*
 * Structure storing information about how spring current measurement HW
 * is connected to SVC.
 */
struct pm_data {
    uint8_t adc;        /* ADC instance */
    uint8_t chan;       /* ADC channel */
    uint32_t spin;      /* ADC sign pin */
};

/* Wake & Detect debounce state machine */
enum wd_debounce_state {
    WD_ST_INVALID,                  /* Unknown state */
    WD_ST_INACTIVE_DEBOUNCE,        /* Transition to inactive */
    WD_ST_ACTIVE_DEBOUNCE,          /* Transition to active */
    WD_ST_INACTIVE_STABLE,          /* Stable inactive */
    WD_ST_ACTIVE_STABLE,            /* Stable active */
};

/*
 * Wake & Detect debounce times
 *
 * The X-->active timer is longer than the X-->inactive timer because
 * transitioning to active will cause the SVC to apply power. That
 * makes us want to be more sure there is really something "there".
 */
#define WD_ACTIVATION_DEBOUNCE_TIME_MS         300
#define WD_INACTIVATION_DEBOUNCE_TIME_MS       30

/* Hotplug state */
enum hotplug_state {
    HOTPLUG_ST_UNKNOWN,             /* Unknown or unitialized */
    HOTPLUG_ST_PLUGGED,             /* Port is plugged in */
    HOTPLUG_ST_UNPLUGGED,           /* Nothing plugged in port  */
};

/*
 * Wake & Detect signals information
 */
struct wd_data {
    uint8_t gpio;                       /* GPIO number */
    bool polarity;                      /* Polarity of 'active' state for gpio (active high == true) */
    enum wd_debounce_state db_state;    /* Debounce state */
    enum wd_debounce_state last_state;  /* Last stable debounce state */
    struct timeval debounce_tv;         /* Last time of signal debounce check */
    struct work_s work;                 /* Work queue for delayed state check */
};

#define ARA_IFACE_WD_ACTIVE_LOW     false
#define ARA_IFACE_WD_ACTIVE_HIGH    true

/* Interface types. */
enum ara_iface_type {
    /* Connected to built-in UniPro peer (like a bridge ASIC on DB3 e.g.). */
    ARA_IFACE_TYPE_BUILTIN,
    /* Module port interface, as on DB3 board */
    ARA_IFACE_TYPE_MODULE_PORT,
    /* Module port interface for HW >= EVT2 */
    ARA_IFACE_TYPE_MODULE_PORT2,
};

/* Interface power states */
enum ara_iface_pwr_state {
    ARA_IFACE_PWR_ERROR = -1,
    ARA_IFACE_PWR_DOWN = 0,
    ARA_IFACE_PWR_UP = 1,
};

enum ara_iface_order {
    ARA_IFACE_ORDER_UNKNOWN = 0,
    ARA_IFACE_ORDER_PRIMARY,
    ARA_IFACE_ORDER_SECONDARY,
};

/* Handler state */
enum ara_iface_state {
    ARA_IFACE_STATE_WD_HANDLER_INACTIVE = 0,
    ARA_IFACE_STATE_WD_HANDLER_ACTIVE,
    ARA_IFACE_STATE_WD_TIMESYNC,
};

/* Max number of LinkUp retries before the interface is shut down */
#define INTERFACE_MAX_LINKUP_TRIES  3

struct interface {
    const char *name;
    unsigned int switch_portid;
    uint8_t dev_id;
    enum ara_iface_type if_type;
    struct vreg *vsys_vreg;
    struct vreg *refclk_vreg;
    atomic_t power_state;
    atomic_t refclk_state;
    struct pm_data *pm;
    struct wd_data detect_in;
    uint8_t wake_gpio;
    bool wake_gpio_pol;
    struct work_s linkup_work;
    uint8_t linkup_retries;
    bool linkup_req_sent;
    bool ejectable;
    uint8_t release_gpio;
    struct wdog_s linkup_wd;
    enum ara_iface_order if_order;
    pthread_mutex_t mutex;
    struct work_s wakeout_work; /* WAKEOUT pulse completion work */
    struct work_s eject_work;   /* Module ejection completion work */
    atomic_t dme_powermodeind;
    enum ara_iface_state state;
};

#define interface_foreach(iface, idx)                       \
        for ((idx) = 0, (iface) = interface_get(idx);       \
             (iface);                                       \
             (idx)++, (iface) = interface_get(idx))

/* Initialization */
int interface_init(struct interface**, size_t nr_interfaces,
                   size_t nr_spring_ints, struct vreg *vlatch,
                   struct vreg *latch_curlim, uint8_t mod_sense_gpio);
int interface_early_init(struct interface**, size_t nr_interfaces,
                         size_t nr_spring_ints, struct vreg *vlatch,
                         struct vreg *latch_curlim, uint8_t mod_sense_gpio);
void interface_exit(void);

/* Get methods */
struct interface* interface_get(uint8_t index);
struct interface* interface_get_by_name(const char *name);
struct interface* interface_get_by_portid(uint8_t port_id);
int interface_get_id_by_portid(uint8_t port_id);
int interface_get_portid_by_id(uint8_t intf_id);
int interface_get_devid_by_id(uint8_t intf_id);
struct interface* interface_spring_get(uint8_t index);
uint8_t interface_get_count(void);
uint8_t interface_get_spring_count(void);
#define MOD_RELEASE_PULSE_WIDTH 1500U /* ms */

const char *interface_get_name(struct interface *iface);
unsigned int interface_get_portid(const struct interface *iface);
enum ara_iface_pwr_state interface_get_vsys_state(struct interface *iface);
enum ara_iface_pwr_state interface_get_refclk_state(struct interface *iface);
int interface_refclk_enable(struct interface *iface);
int interface_refclk_disable(struct interface *iface);
int interface_vsys_enable(struct interface *iface);
int interface_vsys_disable(struct interface *iface);

/**
 * @brief Test if an interface connects to a built-in peer on the board.
 *
 * Some boards have built-in UniPro peers for some switch ports. For
 * example, DB3s have built-in bridge ASICs. This function tests if an
 * interface is to such ap eer.
 *
 * @return 1 if the interface is connected to a built-in peer, 0 otherwise.
 */
static inline int interface_is_builtin(struct interface *iface) {
    return !!(iface->if_type == ARA_IFACE_TYPE_BUILTIN);
}

/** @brief Test if an interface connects to a module port */
static inline int interface_is_module_port(struct interface *iface) {
    return !!(iface->if_type == ARA_IFACE_TYPE_MODULE_PORT ||
              iface->if_type == ARA_IFACE_TYPE_MODULE_PORT2);
}

uint8_t interface_pm_get_adc(struct interface *iface);
uint8_t interface_pm_get_chan(struct interface *iface);
uint32_t interface_pm_get_spin(struct interface *iface);

/* Set methods; postfixed with _atomic() */
int interface_set_devid_by_id_atomic(uint8_t intf_id, uint8_t dev_id);
void interface_set_linkup_retries_atomic(struct interface *iface, uint8_t val);
void interface_cancel_linkup_wd_atomic(struct interface *iface);
/*
 * This is the low level ejection function, called from svc.c.
 * High level code shall call the ejection request function from svc.c.
 */
int interface_forcibly_eject_atomic(struct interface *iface, uint32_t delay);
int interface_power_off_atomic(struct interface *iface);
int interface_power_on_atomic(struct interface *iface);
int interface_generate_wakeout_atomic(struct interface *, bool assert,
                                      int length);
int interface_cancel_wakeout_atomic(struct interface *iface);
enum hotplug_state interface_get_hotplug_state_atomic(struct interface *iface);

/* Timesync init/fini */
uint32_t interfaces_timesync_init(uint32_t strobe_mask);
void interfaces_timesync_fini(void);

/*
 * Macro magic.
 */
#define INIT_SPRING_PM_DATA(_adc, _chan, _spin)                \
    {                                                          \
        .adc = _adc,                                           \
        .chan = _chan,                                         \
        .spin = _spin,                                         \
    }

#define INIT_WD_DATA(_gpio, _polarity)                         \
    {                                                          \
        .gpio = _gpio,                                         \
        .polarity = _polarity,                                 \
        .db_state = WD_ST_INVALID,                             \
        .debounce_tv = { 0, 0 },                               \
    }

#define __MAKE_BB_PM(n) bb ## n ## _pm
#define MAKE_BB_PM(n) __MAKE_BB_PM(n)

#define __MAKE_INTERFACE(n) n ## _interface
#define MAKE_INTERFACE(n) __MAKE_INTERFACE(n)

/*
 * Module port interface, as on DB3 board.
 *
 * If there is no Unipro port connected to the interface, portid
 * is INVALID_PORT.
 */
#define DECLARE_MODULE_PORT_INTERFACE(_var_name, _name,        \
                                      vsys_vreg_data,          \
                                      refclk_vreg_data,        \
                                      portid,                  \
                                      wake_detect_gpio,        \
                                      detect_in_pol,           \
                                      _ejectable,              \
                                      _rg)                     \
    DECLARE_VREG(_var_name ## _vsys_vreg, vsys_vreg_data);     \
    DECLARE_VREG(_var_name ## _refclk_vreg, refclk_vreg_data); \
    static struct interface MAKE_INTERFACE(_var_name) = {      \
        .name = _name,                                         \
        .if_type = ARA_IFACE_TYPE_MODULE_PORT,                 \
        .state = ARA_IFACE_STATE_WD_HANDLER_INACTIVE,          \
        .vsys_vreg = &_var_name ## _vsys_vreg,                 \
        .refclk_vreg = &_var_name ## _refclk_vreg,             \
        .switch_portid = portid,                               \
        .pm = NULL,                                            \
        .detect_in = INIT_WD_DATA(wake_detect_gpio,            \
                                  !!detect_in_pol),            \
        .ejectable = _ejectable,                               \
        .release_gpio = _rg,                                   \
        .linkup_wd = WDOG_INITIAILIZER,                        \
    }

/*
 * Module port interface for HW >= EVT2
 */
#define DECLARE_MODULE_PORT_INTERFACE2(_var_name, _name,       \
                                       vsys_vreg_data,         \
                                       refclk_vreg_data,       \
                                       portid,                 \
                                       _wake_gpio,             \
                                       _wake_gpio_pol,         \
                                       latch_gpio,             \
                                       latch_pol,              \
                                       _ejectable,             \
                                       _rg)                    \
    DECLARE_VREG(_var_name ## _vsys_vreg, vsys_vreg_data);     \
    DECLARE_VREG(_var_name ## _refclk_vreg, refclk_vreg_data); \
    static struct interface MAKE_INTERFACE(_var_name) = {      \
        .name = _name,                                         \
        .if_type = ARA_IFACE_TYPE_MODULE_PORT2,                \
        .state = ARA_IFACE_STATE_WD_HANDLER_INACTIVE,          \
        .vsys_vreg = &_var_name ## _vsys_vreg,                 \
        .refclk_vreg = &_var_name ## _refclk_vreg,             \
        .switch_portid = portid,                               \
        .pm = NULL,                                            \
        .detect_in = INIT_WD_DATA(latch_gpio, !!latch_pol),    \
        .wake_gpio = _wake_gpio,                               \
        .wake_gpio_pol = !!_wake_gpio_pol,                     \
        .ejectable = _ejectable,                               \
        .release_gpio = _rg,                                   \
        .linkup_wd = WDOG_INITIAILIZER,                        \
    }

#endif
