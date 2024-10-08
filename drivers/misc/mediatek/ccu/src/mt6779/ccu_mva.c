/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "ccu_cmn.h"
#include "ccu_mva.h"

static struct ion_client *_ccu_ion_client;
static struct m4u_client_t *m4u_client;

static int _ccu_config_m4u_port(void);
static int ccu_ion_init_result;

int ccu_ion_init(void)
{
	MBOOL need_init = MFALSE;

	ccu_lock_ion_client_mutex();
	if (!_ccu_ion_client && g_ion_device) {
		LOG_INF_MUST("CCU ION_client need init\n");
		need_init = MTRUE;
		ccu_ion_init_result = 1;
	} else if (_ccu_ion_client) {
		LOG_INF_MUST("ION Client exist: 0x%p\n",
		_ccu_ion_client);
		ccu_ion_init_result = 2;
	} else {
		LOG_INF_MUST("Device NULL: 0x%p\n",
			g_ion_device);
		ccu_ion_init_result = 3;
	}

	if (need_init == MTRUE) {
		_ccu_ion_client = ion_client_create(g_ion_device, "ccu");
		LOG_INF_MUST("CCU ION_client create success: 0x%p\n", _ccu_ion_client);
	}

	if (!_ccu_ion_client) {
		LOG_INF_MUST("ION Client NULL: 0x%p\n",
		_ccu_ion_client);
		ccu_ion_init_result = 4;
	}

	m4u_client = m4u_create_client();
	if (IS_ERR_OR_NULL(m4u_client))
		LOG_ERR("create client fail!\n");

	ccu_unlock_ion_client_mutex();
	return 0;
}

int ccu_ion_uninit(void)
{
	MBOOL need_uninit = MFALSE;

	ccu_lock_ion_client_mutex();
	ccu_ion_init_result = 0;

	if (_ccu_ion_client && g_ion_device) {
		LOG_INF_MUST("CCU ION_client need uninit\n");
		need_uninit = MTRUE;
	}

	if (need_uninit == MTRUE) {
		ion_client_destroy(_ccu_ion_client);
		LOG_INF_MUST("CCU ION_client destroy done.\n");
		_ccu_ion_client = NULL;
	}

	if (m4u_client)
		m4u_destroy_client(m4u_client);

	ccu_unlock_ion_client_mutex();
	return 0;
}

int ccu_deallocate_mva(uint32_t mva)
{
	int ret = 0;

	LOG_DBG("X-:%s\n", __func__);
	if (m4u_client  == NULL) {
		LOG_ERR("%s: _ccu_ion_client(%d) is null!\n",
			__func__, ccu_ion_init_result);
		return -1;
	}

	if (mva != 0) {
		ret = m4u_dealloc_mva(m4u_client, M4U_PORT_CCU0, mva);
		if (ret)
			LOG_ERR("dealloc mva fail");
	}
	return ret;
}

int ccu_allocate_mva(uint32_t *mva, void *va, int buffer_size)
{
	int ret = 0;
	struct sg_table *sg_table = NULL;
	unsigned int flag;
	/*int buffer_size = 4096;*/

	ret = _ccu_config_m4u_port();
	if (ret) {
		LOG_ERR("fail to config m4u port!\n");
		return ret;
	}

	/* alloc mva */
	flag = M4U_FLAGS_START_FROM;
	ret = m4u_alloc_mva(m4u_client, M4U_PORT_CCU0, (unsigned long)va,
		sg_table, buffer_size,
		M4U_PROT_READ | M4U_PROT_WRITE, flag, mva);
	if (ret)
		LOG_ERR("alloc mva fail");

	return ret;
}



static int _ccu_config_m4u_port(void)
{
	int ret = 0;

#if defined(CONFIG_MTK_M4U)
	struct M4U_PORT_STRUCT port;

	port.ePortID = M4U_PORT_CCU0;
	port.Virtuality = 1;
	port.Security = 0;
	port.domain = 3;
	port.Distance = 1;
	port.Direction = 0;

	ret = m4u_config_port(&port);
#endif
	return ret;
}

void ccu_ion_free_import_handle(struct ion_handle *handle)
{
	if (_ccu_ion_client == NULL) {
		LOG_ERR("%s: _ccu_ion_client(%d) is null!\n",
			__func__, ccu_ion_init_result);
		return;
	}
	if (handle == NULL) {
		LOG_ERR("invalid ion handle!\n");
		return;
	}

	ion_free(_ccu_ion_client, handle);
}

struct ion_handle *ccu_ion_import_handle(int fd)
{
	struct ion_handle *handle = NULL;

	if (_ccu_ion_client == NULL) {
		LOG_ERR("%s: _ccu_ion_client(%d) is null!\n",
			__func__, ccu_ion_init_result);
		return handle;
	}
	if (fd == -1) {
		LOG_ERR("ccu invalid ion fd!\n");
		return handle;
	}

	handle = ion_import_dma_buf_fd(_ccu_ion_client, fd);
	LOG_INF_MUST("ccu_ion_import_fd : %d, ccu_ion_import_handle : 0x%p\n", fd, handle);
	if (!(handle)) {
		LOG_ERR("ccu import ion handle failed!\n");
		return NULL;
	}

	return handle;
}
