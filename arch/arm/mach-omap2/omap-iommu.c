/*
 * omap iommu: omap device registration
 *
 * Copyright (C) 2008-2009 Nokia Corporation
 *
 * Written by Hiroshi DOYU <Hiroshi.DOYU@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <plat/iommu.h>
#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>
#include <plat/irqs-44xx.h>
#include <plat/iopgtable.h>

static struct platform_device **omap_iommu_pdev;

struct omap_iommu_device {
	resource_size_t base;
	int irq;
	struct omap_iommu_platform_data pdata;
	struct resource res[2];
};
static struct omap_iommu_platform_data *devices_data;
static int num_iommu_devices;

#ifdef CONFIG_ARCH_OMAP3
static struct omap_iommu_platform_data omap3_devices_data[] = {
	{
		.name = "isp",
		.oh_name = "isp",
		.nr_tlb_entries = 8,
	},
#if defined(CONFIG_OMAP_IOMMU_IVA2)
	{
		.name = "iva2",
		.oh_name = "dsp",
		.nr_tlb_entries = 32,
	},
#endif
};
#define NR_OMAP3_IOMMU_DEVICES ARRAY_SIZE(omap3_devices_data)
static struct platform_device *omap3_iommu_pdev[NR_OMAP3_IOMMU_DEVICES];
#else
#define omap3_devices_data	NULL
#define NR_OMAP3_IOMMU_DEVICES	0
#define omap3_iommu_pdev        NULL
#endif

#ifdef CONFIG_ARCH_OMAP4

/*
struct iommu_platform_data {                                                    
        const char *name;                                                       
        const char *oh_name;                                                    
        const int nr_tlb_entries;                                               
        u32 da_start;                                                           
        u32 da_end;                                                             
        int irq;                                                                
        void __iomem *io_base;                                                  
}; 
*/

static struct omap_iommu_platform_data omap4_devices_data[] = {
	{
		.io_base = (void *)OMAP4_MMU1_BASE,
		.irq = OMAP44XX_IRQ_DUCATI_MMU,
			/* .clk_name = "ipu_fck", */
		.da_start = 0x0,
		.da_end = 0xFFFFF000,
		.name = "ducati",
		.oh_name = "ipu",
		.nr_tlb_entries = 32,
	},
	{
		.name = "tesla",
		.oh_name = "dsp",
		.nr_tlb_entries = 32,
	},
};
#define NR_OMAP4_IOMMU_DEVICES ARRAY_SIZE(omap4_devices_data)
static struct platform_device *omap4_iommu_pdev[NR_OMAP4_IOMMU_DEVICES];

struct device *omap_iommu_get_dev(const char *dev_name)
{
	int i;
	struct platform_device *pdev;
	struct device *dev = NULL;
	struct omap_iommu_platform_data *pdata;

	for (i = 0; i < NR_OMAP4_IOMMU_DEVICES; i++) {
		pdev = omap_iommu_pdev[i];
		pdata = pdev->dev.platform_data;
		if (!strcmp(pdata->oh_name, dev_name))
			dev = &pdev->dev;
	}
	WARN_ONCE(!dev, "omap_iommu_get_dev: NULL dev\n");

	return dev;
}
EXPORT_SYMBOL(omap_iommu_get_dev);

#if 1

/**                                                                             
 *  * iommu_get - Get iommu handler                                                
 *   * @name:       target iommu name                                               
 *    **/                                                                            
struct omap_iommu *iommu_get(const char *name)                                       
{                                                                               
        int err = -ENOMEM;                                                      
        struct device *dev;                                                     
        struct omap_iommu *obj;                                                      
        int rev;                                                                
	unsigned long flags;

        dev = omap_find_iommu_device(name);
        if (!dev) {                                                              
		pr_err("iommu_get: no dev for %s\n", name);
                return ERR_PTR(-ENODEV);
	}                                        
        obj = to_iommu(dev);                                                    

	spin_lock_irqsave(&obj->iommu_lock, flags);

        if (obj->refcount++ == 0) {                                             
                err = iommu_enable(obj);                                
                if (err) {                                                        
			pr_err("iommu_get: iommu_enable returned %d\n", err);
                        goto err_enable;
		}                                        
                if (!strcmp(obj->name, "ducati")) {                             
                        rev = GET_OMAP_REVISION();                              
                        if (rev == 0x0)                                         
                                iommu_set_twl(obj, false);                      
                }                                                               
                                                                                
                flush_iotlb_all(obj);                                           
        }                                                                       
        if (!try_module_get(obj->owner)) {                                        
		pr_err("iommu_get: try_module_get failed\n");
                goto err_module;
	}                                                

	spin_unlock_irqrestore(&obj->iommu_lock, flags);
        dev_dbg(obj->dev, "%s: %s\n", __func__, obj->name);                     

        return obj;                                                             
                                                                                
err_module:                                                                     
        if (obj->refcount == 1)                                                 
                iommu_disable(obj);                                             
err_enable:                                                                     
        obj->refcount--;
	spin_unlock_irqrestore(&obj->iommu_lock, flags);

        return ERR_PTR(err);                                                    
}                                                                               
EXPORT_SYMBOL_GPL(iommu_get);                                                   
                                                                                
/**                                                                             
 *  * iommu_put - Put back iommu handler                                           
 *   * @obj:        target iommu                                                    
 *    **/                                                                            
void iommu_put(struct omap_iommu *obj)                                               
{
	unsigned long flags;

        if (!obj || IS_ERR(obj))                                                
                return;                                                         
                                                                                
	spin_lock_irqsave(&obj->iommu_lock, flags);                                           
                                                                                
        if (--obj->refcount == 0)                                               
                iommu_disable(obj);                                             
                                                                                
        module_put(obj->owner);                                                 
                                                                                
	spin_unlock_irqrestore(&obj->iommu_lock, flags);
                                                                                
        dev_dbg(obj->dev, "%s: %s\n", __func__, obj->name);                     
}                                                                               
EXPORT_SYMBOL_GPL(iommu_put);                                                   

#endif

#else
#define omap4_devices_data	NULL
#define NR_OMAP4_IOMMU_DEVICES	0
#define omap4_iommu_pdev        NULL
#endif

static struct omap_device_pm_latency omap_iommu_latency[] = {
	[0] = {
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func	 = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

int iommu_get_plat_data_size(void)
{
	return num_iommu_devices;
}
EXPORT_SYMBOL(iommu_get_plat_data_size);

static int __init omap_iommu_init(void)
{
	int i, ohl_cnt;
	struct omap_hwmod *oh;
	struct omap_device_pm_latency *ohl;
	struct platform_device *pdev;

	if (cpu_is_omap34xx()) {
		devices_data = omap3_devices_data;
		num_iommu_devices = NR_OMAP3_IOMMU_DEVICES;
		omap_iommu_pdev = omap3_iommu_pdev;
	} else if (cpu_is_omap44xx()) {
		devices_data = omap4_devices_data;
		num_iommu_devices = NR_OMAP4_IOMMU_DEVICES;
		omap_iommu_pdev = omap4_iommu_pdev;
	} else
		return -ENODEV;

	ohl = omap_iommu_latency;
	ohl_cnt = ARRAY_SIZE(omap_iommu_latency);

	for (i = 0; i < num_iommu_devices; i++) {
		struct omap_iommu_platform_data *data = &devices_data[i];

		oh = omap_hwmod_lookup(data->oh_name);
		if (oh == NULL)
			continue;
		data->io_base = oh->_mpu_rt_va;
		data->irq = oh->mpu_irqs[0].irq;

		if (!oh) {
			pr_err("%s: could not look up %s\n", __func__,
							data->oh_name);
			continue;
		}
		pdev = omap_device_build("omap-iommu", i, oh,
					data, sizeof(*data),
					ohl, ohl_cnt, false);
		WARN(IS_ERR(pdev), "Could not build omap_device"
				"for %s %s\n", "omap-iommu", data->oh_name);

		pdev = platform_device_alloc("omap-iovmm", i);
		if (pdev) {
			platform_device_add_data(pdev, data, sizeof(*data));
			platform_device_add(pdev);
		}
		omap_iommu_pdev[i] = pdev;
	}
	return 0;
}
postcore_initcall(omap_iommu_init);

static void __exit omap_iommu_exit(void)
{
	int i;
	for (i = 0; i < num_iommu_devices; i++)
		platform_device_unregister(omap_iommu_pdev[i]);
}
module_exit(omap_iommu_exit);

MODULE_AUTHOR("Hiroshi DOYU");
MODULE_AUTHOR("Hari Kanigeri");
MODULE_DESCRIPTION("omap iommu: omap device registration");
MODULE_LICENSE("GPL v2");
