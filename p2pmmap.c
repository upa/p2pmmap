/* p2pmmap.c */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/miscdevice.h>
#include <linux/pci-p2pdma.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define P2PMMAP_VERSION	"0.0.0"

static char *target_pci_dev = NULL;
module_param(target_pci_dev, charp, 0);
MODULE_PARM_DESC(target_pci_dev, "target pci device bus number");

static unsigned int p2pmem_size = 4096;
module_param(p2pmem_size, uint, 0);
MODULE_PARM_DESC(p2pmem_size, "size of allocating p2pmem");

struct p2pmmap {
	struct pci_dev	*pdev;	/* target pci dev with p2pmem	*/
	void	*p2pmem;	/* p2pmem of the above pci dev	*/
	size_t	size;		/* p2pmem size */
};

static struct p2pmmap pmm;	/* describing this module */



static int __init p2pmmap_init(void)
{
	int ret, domain, bus, slot, func;
	struct pci_dev *pdev;

	memset(&pmm, 0, sizeof(pmm));

	/* check target pci dev */
	if (!target_pci_dev) {
		pr_err("'target_pci_dev' param must be specified\n");
		return -EINVAL;
	}

	/* parse target_pci_dev, bus:slot.func or domain:bus:slot.func */
	ret = sscanf(target_pci_dev, "%x:%x:%x.%x",
		     &domain, &bus, &slot, &func);
	if (ret < 4) {
		domain	= 0;
		bus	= 0;
		slot	= 0;
		func	= 0;
		ret = sscanf(target_pci_dev, "%x:%x.%x", &bus, &slot, &func);
	}
	if (ret < 3)
		goto err_no_pci_slot;

	pdev = pci_get_domain_bus_and_slot(domain, bus, PCI_DEVFN(slot, func));
	if (!pdev)
		goto err_no_pci_slot;


	/* ok, we found the pci device. next is to allocate p2pmem */
	if (!pci_has_p2pmem(pdev)) {
		pr_err("%s does not support p2pmem\n", target_pci_dev);
		return -ENOTSUPP;
	}

	pmm.pdev = pdev;
	pmm.size = p2pmem_size;
	pmm.p2pmem = pci_alloc_p2pmem(pdev, pmm.size);
	if (!pmm.p2pmem) {
		pr_err("failed to allocate %lu-byte p2pmem\n", pmm.size);
		goto err_p2pmem_alloc_failed;
	}

	pr_info("p2pmmap (v%s) is loaded.\n", P2PMMAP_VERSION);
	pr_info("%lu-byte allcoated from %s p2pmem\n",
		pmm.size, target_pci_dev);

	return 0;

err_no_pci_slot:
	pr_err("invalid pci dev %s\n", target_pci_dev);
	return -EINVAL;

err_p2pmem_alloc_failed:
	pci_dev_put(pmm.pdev);
	return -EINVAL;
}

static void __exit p2pmmap_exit(void)
{
	pci_free_p2pmem(pmm.pdev, pmm.p2pmem, pmm.size);
	pci_dev_put(pmm.pdev);

	pr_info("p2pmmap (v%s) is unloaded\n", P2PMMAP_VERSION);
}

module_init(p2pmmap_init);
module_exit(p2pmmap_exit);
MODULE_AUTHOR("Ryo Nakamura <upa@haeena.net>");
MODULE_LICENSE("GPL");
MODULE_VERSION(P2PMMAP_VERSION);
