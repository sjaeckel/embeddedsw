/******************************************************************************
*
* Copyright (C) 2018-2019 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
*
*
******************************************************************************/

#include "xpm_gic_proxy.h"
#include "xpm_pmc.h"
#include "xpm_device.h"
#include "xpm_common.h"
#include "xpm_regs.h"

#define XPM_GIC_PROXY_IS_ENABLED		0x1U

XStatus XPmGicProxy_WakeEventSet(XPm_Periph *Periph, u8 Enable)
{
	XStatus Status = XST_FAILURE;
	u32 GicProxyMask = Periph->GicProxyMask;
	u32 GicProxyGroup = Periph->GicProxyGroup;
	XPm_Pmc *Pmc = (XPm_Pmc *)XPmDevice_GetById(PM_DEV_PMC_PROC);

	if (0U == Enable) {
		XPm_GicProxy.Groups[GicProxyGroup].SetMask &= ~GicProxyMask;
	} else {
		/* PMC Global base address */
		u32 BaseAddress = Pmc->PmcGlobalBaseAddr;
		u32 RegAddress = BaseAddress +
				 PMC_GLOBAL_GIC_PROXY_BASE_OFFSET +
				 GIC_PROXY_GROUP_OFFSET(GicProxyGroup) +
				 GIC_PROXY_IRQ_STATUS_OFFSET;

		/* Write 1 into status register to Clear interrupt */
		XPm_Out32(RegAddress, GicProxyMask);

		/* Remember which interrupt in the group needs to be Enabled */
		XPm_GicProxy.Groups[GicProxyGroup].SetMask |= GicProxyMask;
	}

	return Status;
}

/**
 * XPmGicProxy_Enable() - Enable all interrupts that are requested
 */
static void XPmGicProxy_Enable(void)
{
	u32 g;
	XPm_Pmc *Pmc = (XPm_Pmc *)XPmDevice_GetById(PM_DEV_PMC_PROC);

	for (g = 0U; g < XPm_GicProxy.GroupsCnt; g++) {
		/* PMC Global base address */
		u32 BaseAddress = Pmc->PmcGlobalBaseAddr;
		u32 RegAddress = BaseAddress +
				 PMC_GLOBAL_GIC_PROXY_BASE_OFFSET +
				 GIC_PROXY_GROUP_OFFSET(g) +
				 GIC_PROXY_IRQ_ENABLE_OFFSET;

		/* Enable interrupts in the group that are set as wake */
		XPm_Out32(RegAddress, XPm_GicProxy.Groups[g].SetMask);

		if (0 != XPm_GicProxy.Groups[g].SetMask) {
			XPm_Out32(BaseAddress +
				  PMC_GLOBAL_GICP_IRQ_ENABLE_OFFSET, BIT(g));
		}
	}

	XPm_GicProxy.Flags |= XPM_GIC_PROXY_IS_ENABLED;
}

/**
 * XPm_GicProxyDisable() - Disable all interrupts in the GIC Proxy
 */
static void XPm_GicProxyDisable(void)
{
	u32 g;
	XPm_Pmc *Pmc = (XPm_Pmc *)XPmDevice_GetById(PM_DEV_PMC_PROC);

	for (g = 0U; g < XPm_GicProxy.GroupsCnt; g++) {
		/* PMC Global base address */
		u32 BaseAddress = Pmc->PmcGlobalBaseAddr;
		u32 DisableAddr = BaseAddress +
				  PMC_GLOBAL_GIC_PROXY_BASE_OFFSET +
				  GIC_PROXY_GROUP_OFFSET(g) +
				  GIC_PROXY_IRQ_DISABLE_OFFSET;

		u32 StatusAddr = BaseAddress +
				 PMC_GLOBAL_GIC_PROXY_BASE_OFFSET +
				 GIC_PROXY_GROUP_OFFSET(g) +
				 GIC_PROXY_IRQ_STATUS_OFFSET;

		u32 MaskAddr = BaseAddress +
			       PMC_GLOBAL_GIC_PROXY_BASE_OFFSET +
			       GIC_PROXY_GROUP_OFFSET(g) +
			       GIC_PROXY_IRQ_MASK_OFFSET;

		/* Clear interrupts in the GIC Proxy group that are set as wake */
		XPm_Out32(StatusAddr, XPm_GicProxy.Groups[g].SetMask);

		/* Disable interrupts in the GIC Proxy group that are set as wake */
		XPm_Out32(DisableAddr, XPm_GicProxy.Groups[g].SetMask);

		if (GIC_PROXY_ALL_MASK == XPm_In32(MaskAddr)) {
			XPm_Out32(BaseAddress +
				  PMC_GLOBAL_GICP_IRQ_DISABLE_OFFSET, BIT(g));
		}
	}

	XPm_GicProxy.Flags &= ~XPM_GIC_PROXY_IS_ENABLED;
}

/**
 * XPmGicProxy_Clear() - Clear wake-up sources
 */
static void XPmGicProxy_Clear(void)
{
	u32 g;

	if (0U != (XPm_GicProxy.Flags & XPM_GIC_PROXY_IS_ENABLED)) {
		XPm_GicProxyDisable();
	}

	for (g = 0U; g < XPm_GicProxy.GroupsCnt; g++) {
		XPm_GicProxy.Groups[g].SetMask = 0U;
	}
}

/* FPD GIC Proxy has interrupts organized in 5 Groups */
static XPm_GicProxyGroup XPm_GicProxyGroups[5];

XPm_GicProxy_t XPm_GicProxy = {
	.Groups = XPm_GicProxyGroups,
	.GroupsCnt = ARRAY_SIZE(XPm_GicProxyGroups),
	.Clear = XPmGicProxy_Clear,
	.Enable = XPmGicProxy_Enable,
	.Flags = 0U,
};
