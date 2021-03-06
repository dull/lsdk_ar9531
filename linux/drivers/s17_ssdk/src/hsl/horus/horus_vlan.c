/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc. All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @defgroup horus_vlan HORUS_VLAN
 * @{
 */
#include "sw.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "hsl_port_prop.h"
#include "horus_vlan.h"
#include "horus_reg.h"

#define MAX_VLAN_ID         4095

#define VLAN_FLUSH          1
#define VLAN_LOAD_ENTRY     2
#define VLAN_PURGE_ENTRY    3
#define VLAN_REMOVE_PORT    4
#define VLAN_NEXT_ENTRY     5
#define VLAN_FIND_ENTRY     6

static void
horus_vlan_hw_to_sw(const a_uint32_t reg[], fal_vlan_t * vlan_entry)
{
    a_uint32_t data;

    SW_GET_FIELD_BY_REG(VLAN_TABLE_FUNC0, VT_PRI_EN, data, reg[0]);
    if (1 == data)
    {
        vlan_entry->vid_pri_en = A_TRUE;

        SW_GET_FIELD_BY_REG(VLAN_TABLE_FUNC0, VT_PRI, data, reg[0]);
        vlan_entry->vid_pri = data & 0xff;
    }
    else
    {
        vlan_entry->vid_pri_en = A_FALSE;
    }

    SW_GET_FIELD_BY_REG(VLAN_TABLE_FUNC0, VLAN_ID, data, reg[0]);
    vlan_entry->vid = data & 0xffff;

    SW_GET_FIELD_BY_REG(VLAN_TABLE_FUNC1, VID_MEM, data, reg[1]);
    vlan_entry->mem_ports = data;

    SW_GET_FIELD_BY_REG(VLAN_TABLE_FUNC1, LEARN_DIS, data, reg[1]);
    if (1 == data)
    {
        vlan_entry->learn_dis = A_TRUE;
    }
    else
    {
        vlan_entry->learn_dis = A_FALSE;
    }

    return;
}

static sw_error_t
horus_vlan_sw_to_hw(const fal_vlan_t * vlan_entry, a_uint32_t reg[])
{
    if (A_TRUE == vlan_entry->vid_pri_en)
    {
        SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC0, VT_PRI_EN, 1, reg[0]);
        SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC0, VT_PRI, vlan_entry->vid_pri, reg[0]);
    }
    else
    {
        SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC0, VT_PRI_EN, 0, reg[0]);
    }

    SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC0, VLAN_ID, vlan_entry->vid, reg[0]);

    SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC1, VT_VALID, 1, reg[1]);
    SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC1, VID_MEM, vlan_entry->mem_ports, reg[1]);

    if (A_TRUE == vlan_entry->learn_dis)
    {
        SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC1, LEARN_DIS, 1, reg[1]);
    }
    else
    {
        SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC1, LEARN_DIS, 0, reg[1]);
    }

    if (0 != vlan_entry->u_ports)
    {
        return SW_BAD_VALUE;
    }

    return SW_OK;
}

static sw_error_t
horus_vlan_commit(a_uint32_t dev_id, a_uint32_t op)
{
    a_uint32_t vt_busy = 1, i = 0x1000, vt_full, val;
    sw_error_t rv;

    while (vt_busy && --i)
    {
        HSL_REG_FIELD_GET(rv, dev_id, VLAN_TABLE_FUNC0, 0, VT_BUSY,
                          (a_uint8_t *) (&vt_busy), sizeof (a_uint32_t));
        SW_RTN_ON_ERROR(rv);
        aos_udelay(5);
    }

    if (i == 0)
        return SW_BUSY;

    HSL_REG_ENTRY_GET(rv, dev_id, VLAN_TABLE_FUNC0, 0,
                      (a_uint8_t *) (&val), sizeof (a_uint32_t));
    SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC0, VT_FUNC, op, val);
    SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC0, VT_BUSY, 1, val);

    HSL_REG_ENTRY_SET(rv, dev_id, VLAN_TABLE_FUNC0, 0,
                      (a_uint8_t *) (&val), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    vt_busy = 1;
    i = 0x1000;
    while (vt_busy && --i)
    {
        HSL_REG_FIELD_GET(rv, dev_id, VLAN_TABLE_FUNC0, 0, VT_BUSY,
                          (a_uint8_t *) (&vt_busy), sizeof (a_uint32_t));
        SW_RTN_ON_ERROR(rv);
        aos_udelay(5);
    }

    if (i == 0)
        return SW_FAIL;

    HSL_REG_FIELD_GET(rv, dev_id, VLAN_TABLE_FUNC0, 0, VT_FULL_VIO,
                      (a_uint8_t *) (&vt_full), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    if (vt_full)
    {
        val = 0x10;
        HSL_REG_ENTRY_SET(rv, dev_id, VLAN_TABLE_FUNC0, 0,
                          (a_uint8_t *) (&val), sizeof (a_uint32_t));
        SW_RTN_ON_ERROR(rv);
        if (VLAN_LOAD_ENTRY == op)
        {
            return SW_FULL;
        }
        else if (VLAN_PURGE_ENTRY == op)
        {
            return SW_NOT_FOUND;
        }
    }

    HSL_REG_FIELD_GET(rv, dev_id, VLAN_TABLE_FUNC1, 0, VT_VALID,
                      (a_uint8_t *) (&val), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    if (!val)
    {
        if (VLAN_FIND_ENTRY == op)
            return SW_NOT_FOUND;

        if (VLAN_NEXT_ENTRY == op)
            return SW_NO_MORE;
    }

    return SW_OK;
}

static sw_error_t
_horus_vlan_entry_append(a_uint32_t dev_id, const fal_vlan_t * vlan_entry)
{
    sw_error_t rv;
    a_uint32_t reg[2] = { 0 };

    HSL_DEV_ID_CHECK(dev_id);

    if ((vlan_entry->vid == 0) || (vlan_entry->vid > MAX_VLAN_ID))
        return SW_OUT_OF_RANGE;

    if (A_FALSE == hsl_mports_prop_check(dev_id, vlan_entry->mem_ports, HSL_PP_INCL_CPU))
        return SW_BAD_PARAM;

    rv = horus_vlan_sw_to_hw(vlan_entry, reg);
    SW_RTN_ON_ERROR(rv);

    HSL_REG_ENTRY_SET(rv, dev_id, VLAN_TABLE_FUNC0, 0,
                      (a_uint8_t *) (&reg[0]), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    HSL_REG_ENTRY_SET(rv, dev_id, VLAN_TABLE_FUNC1, 0,
                      (a_uint8_t *) (&reg[1]), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    rv = horus_vlan_commit(dev_id, VLAN_LOAD_ENTRY);
    return rv;
}

static sw_error_t
_horus_vlan_create(a_uint32_t dev_id, a_uint16_t vlan_id)
{
    sw_error_t rv;
    a_uint32_t entry = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if ((vlan_id == 0) || (vlan_id > MAX_VLAN_ID))
        return SW_OUT_OF_RANGE;

    /* set default value for VLAN_TABLE_FUNC0, all 0 except vid */
    entry = 0;
    SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC0, VLAN_ID, vlan_id, entry);
    HSL_REG_ENTRY_SET(rv, dev_id, VLAN_TABLE_FUNC0, 0,
                      (a_uint8_t *) (&entry), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    /* set default value for VLAN_TABLE_FUNC1, all 0 */
    entry = 0;
    SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC1, VT_VALID, 1, entry);
    HSL_REG_ENTRY_SET(rv, dev_id, VLAN_TABLE_FUNC1, 0,
                      (a_uint8_t *) (&entry), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    rv = horus_vlan_commit(dev_id, VLAN_LOAD_ENTRY);
    return rv;
}

static sw_error_t
_horus_vlan_next(a_uint32_t dev_id, a_uint16_t vlan_id, fal_vlan_t * p_vlan)
{
    sw_error_t rv;
    a_uint32_t reg[2] = { 0 };

    HSL_DEV_ID_CHECK(dev_id);

    if (vlan_id > MAX_VLAN_ID)
        return SW_OUT_OF_RANGE;

    aos_mem_zero(p_vlan, sizeof (fal_vlan_t));

    SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC0, VLAN_ID, vlan_id, reg[0]);
    HSL_REG_ENTRY_SET(rv, dev_id, VLAN_TABLE_FUNC0, 0,
                      (a_uint8_t *) (&reg[0]), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    HSL_REG_ENTRY_SET(rv, dev_id, VLAN_TABLE_FUNC1, 0,
                      (a_uint8_t *) (&reg[1]), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    rv = horus_vlan_commit(dev_id, VLAN_NEXT_ENTRY);
    SW_RTN_ON_ERROR(rv);

    HSL_REG_ENTRY_GET(rv, dev_id, VLAN_TABLE_FUNC0, 0,
                      (a_uint8_t *) (&reg[0]), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    HSL_REG_ENTRY_GET(rv, dev_id, VLAN_TABLE_FUNC1, 0,
                      (a_uint8_t *) (&reg[1]), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    horus_vlan_hw_to_sw(reg, p_vlan);

    if (0 == p_vlan->vid)
        return SW_NO_MORE;
    else
        return SW_OK;
}

static sw_error_t
_horus_vlan_find(a_uint32_t dev_id, a_uint16_t vlan_id, fal_vlan_t * p_vlan)
{
    sw_error_t rv;
    a_uint32_t reg[2] = { 0 };

    HSL_DEV_ID_CHECK(dev_id);

    if ((vlan_id == 0) || (vlan_id > MAX_VLAN_ID))
        return SW_OUT_OF_RANGE;

    aos_mem_zero(p_vlan, sizeof (fal_vlan_t));

    SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC0, VLAN_ID, vlan_id, reg[0]);
    HSL_REG_ENTRY_SET(rv, dev_id, VLAN_TABLE_FUNC0, 0,
                      (a_uint8_t *) (&reg[0]), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    rv = horus_vlan_commit(dev_id, VLAN_FIND_ENTRY);
    SW_RTN_ON_ERROR(rv);

    HSL_REG_ENTRY_GET(rv, dev_id, VLAN_TABLE_FUNC0, 0,
                      (a_uint8_t *) (&reg[0]), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    HSL_REG_ENTRY_GET(rv, dev_id, VLAN_TABLE_FUNC1, 0,
                      (a_uint8_t *) (&reg[1]), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    horus_vlan_hw_to_sw(reg, p_vlan);

    return SW_OK;
}

static sw_error_t
_horus_vlan_member_update(a_uint32_t dev_id, a_uint16_t vlan_id,
                          fal_pbmp_t member, fal_pbmp_t u_member)
{
    sw_error_t rv;
    a_uint32_t reg = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if ((vlan_id == 0) || (vlan_id > MAX_VLAN_ID))
        return SW_OUT_OF_RANGE;

    if (A_FALSE == hsl_mports_prop_check(dev_id, member, HSL_PP_INCL_CPU))
        return SW_BAD_PARAM;

    if (u_member != 0)
        return SW_BAD_PARAM;

    /* get vlan entry first */
    SW_SET_REG_BY_FIELD(VLAN_TABLE_FUNC0, VLAN_ID, vlan_id, reg);
    HSL_REG_ENTRY_SET(rv, dev_id, VLAN_TABLE_FUNC0, 0,
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    rv = horus_vlan_commit(dev_id, VLAN_FIND_ENTRY);
    SW_RTN_ON_ERROR(rv);

    /* set vlan member for VLAN_TABLE_FUNC1 */
    HSL_REG_FIELD_SET(rv, dev_id, VLAN_TABLE_FUNC1, 0, VID_MEM,
                      (a_uint8_t *) (&member), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    rv = horus_vlan_commit(dev_id, VLAN_LOAD_ENTRY);
    /* when update port member through LOAD opration, hardware will
        return VT_FULL_VIO, we should ignore it */
    if (SW_FULL == rv)
        rv = SW_OK;

    return rv;
}

static sw_error_t
_horus_vlan_delete(a_uint32_t dev_id, a_uint16_t vlan_id)
{
    sw_error_t rv;
    a_uint32_t reg;

    HSL_DEV_ID_CHECK(dev_id);

    if ((vlan_id == 0) || (vlan_id > MAX_VLAN_ID))
        return SW_OUT_OF_RANGE;

    reg = (a_int32_t) vlan_id;
    HSL_REG_FIELD_SET(rv, dev_id, VLAN_TABLE_FUNC0, 0, VLAN_ID,
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    rv = horus_vlan_commit(dev_id, VLAN_PURGE_ENTRY);
    return rv;
}

/**
 * @brief Append a vlan entry on paticular device.
 * @param[in] dev_id device id
 * @param[in] vlan_entry vlan entry
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
horus_vlan_entry_append(a_uint32_t dev_id, const fal_vlan_t * vlan_entry)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _horus_vlan_entry_append(dev_id, vlan_entry);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Creat a vlan entry through vlan id on a paticular device.
 *   @details   Comments:
 *    After this operation the member ports of the created vlan entry are null.
 * @param[in] dev_id device id
 * @param[in] vlan_id vlan id
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
horus_vlan_create(a_uint32_t dev_id, a_uint32_t vlan_id)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _horus_vlan_create(dev_id, vlan_id);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Next a vlan entry through vlan id on a paticular device.
 *   @details   Comments:
 *    If the value of vid is zero this operation will get the first entry.
 * @param[in] dev_id device id
 * @param[in] vlan_id vlan id
 * @param[out] p_vlan vlan entry
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
horus_vlan_next(a_uint32_t dev_id, a_uint32_t vlan_id, fal_vlan_t * p_vlan)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _horus_vlan_next(dev_id, vlan_id, p_vlan);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Find a vlan entry through vlan id on paticular device.
 * @param[in] dev_id device id
 * @param[in] vlan_id vlan id
 * @param[out] p_vlan vlan entry
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
horus_vlan_find(a_uint32_t dev_id, a_uint32_t vlan_id, fal_vlan_t * p_vlan)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _horus_vlan_find(dev_id, vlan_id, p_vlan);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Update a vlan entry member port through vlan id on a paticular device.
 * @param[in] dev_id device id
 * @param[in] vlan_id vlan id
 * @param[in] member member ports
 * @param[in] u_member tagged or untagged infomation for member ports
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
horus_vlan_member_update(a_uint32_t dev_id, a_uint32_t vlan_id,
                         fal_pbmp_t member, fal_pbmp_t u_member)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _horus_vlan_member_update(dev_id, vlan_id, member, u_member);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Delete a vlan entry through vlan id on a paticular device.
 * @param[in] dev_id device id
 * @param[in] vlan_id vlan id
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
horus_vlan_delete(a_uint32_t dev_id, a_uint32_t vlan_id)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _horus_vlan_delete(dev_id, vlan_id);
    HSL_API_UNLOCK;
    return rv;
}

sw_error_t
horus_vlan_init(a_uint32_t dev_id)
{
    HSL_DEV_ID_CHECK(dev_id);

#ifndef HSL_STANDALONG
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    p_api->vlan_entry_append = horus_vlan_entry_append;
    p_api->vlan_creat = horus_vlan_create;
    p_api->vlan_member_update = horus_vlan_member_update;
    p_api->vlan_delete = horus_vlan_delete;
    p_api->vlan_next = horus_vlan_next;
    p_api->vlan_find = horus_vlan_find;

#endif

    return SW_OK;
}

/**
 * @}
 */
