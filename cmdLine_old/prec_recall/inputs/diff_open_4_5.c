Index: src/sys/arch/i386/pci/pciide_machdep.c
===================================================================
RCS file: /cvs/src/sys/arch/i386/pci/pciide_machdep.c,v
retrieving revision 1.6
retrieving revision 1.7
diff -c -p -r1.6 -r1.7
*** src/sys/arch/i386/pci/pciide_machdep.c	19 Sep 2006 11:06:34 -0000	1.6
--- src/sys/arch/i386/pci/pciide_machdep.c	4 Jan 2009 10:37:40 -0000	1.7
***************
*** 1,6 ****
! /*	$OpenBSD: pciide_machdep.c,v 1.6 2006/09/19 11:06:34 jsg Exp $	*/
  /*	$NetBSD: pciide_machdep.c,v 1.2 1999/02/19 18:01:27 mycroft Exp $	*/
  
  /*
   * Copyright (c) 1998 Christopher G. Demetriou.  All rights reserved.
   *
--- 1,31 ----
! /*	$OpenBSD: pciide_machdep.c,v 1.7 2009/01/04 10:37:40 jsg Exp $	*/
  /*	$NetBSD: pciide_machdep.c,v 1.2 1999/02/19 18:01:27 mycroft Exp $	*/
  
+ /*-
+  * Copyright (c) 2007 Juan Romero Pardines.
+  * All rights reserved.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  *
+  * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
+  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
+  * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
+  * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
+  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
+  * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
+  * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
+  * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
+  * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
+  * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
+  */
+ 
  /*
   * Copyright (c) 1998 Christopher G. Demetriou.  All rights reserved.
   *
***************
*** 52,57 ****
--- 77,87 ----
  
  #include <dev/isa/isavar.h>
  
+ #include <machine/cpufunc.h>
+ #include <i386/pci/pciide_gcsc_reg.h>
+ 
+ void gcsc_setup_channel(struct channel_softc *);
+ 
  void *
  pciide_machdep_compat_intr_establish(struct device *dev,
      struct pci_attach_args *pa, int chan, int (*func)(void *), void *arg)
*************** void
*** 69,72 ****
--- 99,202 ----
  pciide_machdep_compat_intr_disestablish(pci_chipset_tag_t pc, void *cookie)
  {
  	isa_intr_disestablish(NULL, cookie);
+ }
+ 
+ void
+ gcsc_chip_map(struct pciide_softc *sc, struct pci_attach_args *pa)
+ {
+ 	struct pciide_channel *cp;
+ 	pcireg_t interface;
+ 	bus_size_t cmdsize, ctlsize;
+ 
+ 	printf(": DMA");
+ 	pciide_mapreg_dma(sc, pa);
+ 
+ 	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
+ 	    WDC_CAPABILITY_MODE;
+ 	if (sc->sc_dma_ok) {
+ 		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
+ 		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
+ 		sc->sc_wdcdev.irqack = pciide_irqack;
+ 	}
+ 
+ 	sc->sc_wdcdev.PIO_cap = 4;
+ 	sc->sc_wdcdev.DMA_cap = 2;
+ 	sc->sc_wdcdev.UDMA_cap = 4;
+ 	sc->sc_wdcdev.set_modes = gcsc_setup_channel;
+ 	sc->sc_wdcdev.channels = sc->wdc_chanarray;
+ 	sc->sc_wdcdev.nchannels = 1;
+ 
+ 	interface = PCI_INTERFACE(pa->pa_class);
+ 
+ 	pciide_print_channels(sc->sc_wdcdev.nchannels, interface);
+ 
+ 	cp = &sc->pciide_channels[0];
+ 
+ 	if (pciide_chansetup(sc, 0, interface) == 0)
+ 		return;
+ 
+ 	pciide_map_compat_intr(pa, cp, 0, interface);
+ 	if (cp->hw_ok == 0)
+ 		return;
+ 
+ 	pciide_mapchan(pa, cp, interface,
+ 	    &cmdsize, &ctlsize, pciide_pci_intr);
+ 	if (cp->hw_ok == 0) {
+ 		pciide_unmap_compat_intr(pa, cp, 0, interface);
+ 		return;
+ 	}
+ 	
+ 	gcsc_setup_channel(&cp->wdc_channel);
+ }
+ 
+ void
+ gcsc_setup_channel(struct channel_softc *chp)
+ {
+ 	struct pciide_channel *cp = (struct pciide_channel *)chp;
+ 	struct ata_drive_datas *drvp;
+ 	uint64_t reg = 0;
+ 	int drive, s;
+ 
+ 	pciide_channel_dma_setup(cp);
+ 
+ 	for (drive = 0; drive < 2; drive++) {
+ 		drvp = &chp->ch_drive[drive];
+ 		if ((drvp->drive_flags & DRIVE) == 0)
+ 			continue;
+ 
+ 		reg = rdmsr(drive ? GCSC_ATAC_CH0D1_DMA :
+ 		    GCSC_ATAC_CH0D0_DMA);
+ 
+ 		if (drvp->drive_flags & DRIVE_UDMA) {
+ 			s = splbio();
+ 			drvp->drive_flags &= ~DRIVE_DMA;
+ 			splx(s);
+ 			/* Enable the Ultra DMA mode bit */
+ 			reg |= GCSC_ATAC_DMA_SEL;
+ 			/* set the Ultra DMA mode */
+ 			reg |= gcsc_udma_timings[drvp->UDMA_mode];
+ 
+ 			wrmsr(drive ? GCSC_ATAC_CH0D1_DMA :
+ 			    GCSC_ATAC_CH0D0_DMA, reg);
+ 
+ 		} else if (drvp->drive_flags & DRIVE_DMA) {
+ 			/* Enable the Multi-word DMA bit */
+ 			reg &= ~GCSC_ATAC_DMA_SEL;
+ 			/* set the Multi-word DMA mode */
+ 			reg |= gcsc_mdma_timings[drvp->DMA_mode];
+ 
+ 			wrmsr(drive ? GCSC_ATAC_CH0D1_DMA :
+ 			    GCSC_ATAC_CH0D0_DMA, reg);
+ 		}
+ 
+ 		/* Always use PIO Format 1. */
+ 		wrmsr(drive ? GCSC_ATAC_CH0D1_DMA :
+ 		    GCSC_ATAC_CH0D0_DMA, reg | GCSC_ATAC_PIO_FORMAT);
+ 
+ 		/* Set PIO mode */
+ 		wrmsr(drive ? GCSC_ATAC_CH0D1_PIO : GCSC_ATAC_CH0D0_PIO,
+ 		    gcsc_pio_timings[drvp->PIO_mode]);
+ 	}
+ 
+ 	pciide_print_modes(cp);
  }
Index: src/sys/arch/macppc/dev/adb.c
===================================================================
RCS file: /cvs/src/sys/arch/macppc/dev/adb.c,v
retrieving revision 1.27
retrieving revision 1.28
diff -c -p -r1.27 -r1.28
*** src/sys/arch/macppc/dev/adb.c	23 Apr 2007 16:27:20 -0000	1.27
--- src/sys/arch/macppc/dev/adb.c	29 Jan 2009 21:17:49 -0000	1.28
***************
*** 1,4 ****
! /*	$OpenBSD: adb.c,v 1.27 2007/04/23 16:27:20 deraadt Exp $	*/
  /*	$NetBSD: adb.c,v 1.6 1999/08/16 06:28:09 tsubai Exp $	*/
  /*	$NetBSD: adb_direct.c,v 1.14 2000/06/08 22:10:45 tsubai Exp $	*/
  
--- 1,4 ----
! /*	$OpenBSD: adb.c,v 1.28 2009/01/29 21:17:49 miod Exp $	*/
  /*	$NetBSD: adb.c,v 1.6 1999/08/16 06:28:09 tsubai Exp $	*/
  /*	$NetBSD: adb_direct.c,v 1.14 2000/06/08 22:10:45 tsubai Exp $	*/
  
*************** adbattach(struct device *parent, struct device *self, 
*** 1665,1671 ****
  	}
  
  	adb_polling = 1;
! 	adb_reinit();
  
  	mac_intr_establish(parent, ca->ca_intr[0], IST_LEVEL, IPL_HIGH,
  	    adb_intr, sc, sc->sc_dev.dv_xname);
--- 1665,1672 ----
  	}
  
  	adb_polling = 1;
! 	if (!adbempty)
! 		adb_reinit();
  
  	mac_intr_establish(parent, ca->ca_intr[0], IST_LEVEL, IPL_HIGH,
  	    adb_intr, sc, sc->sc_dev.dv_xname);
*************** adbattach(struct device *parent, struct device *self, 
*** 1678,1684 ****
  	if (adb_debug)
  		printf("adb: done with adb_reinit\n");
  #endif
! 	totaladbs = count_adbs();
  
  	printf(" irq %d: %s, %d target%s\n", ca->ca_intr[0], ca->ca_name,
  	    totaladbs, (totaladbs == 1) ? "" : "s");
--- 1679,1688 ----
  	if (adb_debug)
  		printf("adb: done with adb_reinit\n");
  #endif
! 	if (adbempty)
! 		totaladbs = 0;
! 	else
! 		totaladbs = count_adbs();
  
  	printf(" irq %d: %s, %d target%s\n", ca->ca_intr[0], ca->ca_name,
  	    totaladbs, (totaladbs == 1) ? "" : "s");
*************** adbattach(struct device *parent, struct device *self, 
*** 1715,1726 ****
  		}
  	}
  
! 	if (adbHardware == ADB_HW_CUDA)
! 		adb_cuda_fileserver_mode();
! 	if (adbHardware == ADB_HW_PMU)
! 		pmu_fileserver_mode(1);
  
! 	if (adbHardware == ADB_HW_CUDA)
! 		adb_cuda_autopoll();
! 	adb_polling = 0;
  }
--- 1719,1732 ----
  		}
  	}
  
! 	if (!adbempty) {
! 		if (adbHardware == ADB_HW_CUDA)
! 			adb_cuda_fileserver_mode();
! 		if (adbHardware == ADB_HW_PMU)
! 			pmu_fileserver_mode(1);
  
! 		if (adbHardware == ADB_HW_CUDA)
! 			adb_cuda_autopoll();
! 		adb_polling = 0;
! 	}
  }
Index: src/sys/arch/macppc/dev/tpms.c
===================================================================
RCS file: /cvs/src/sys/arch/macppc/dev/tpms.c,v
retrieving revision 1.12
retrieving revision 1.13
diff -c -p -r1.12 -r1.13
*** src/sys/arch/macppc/dev/tpms.c	14 Jun 2007 10:11:16 -0000	1.12
--- src/sys/arch/macppc/dev/tpms.c	8 Mar 2009 14:10:08 -0000	1.13
***************
*** 1,4 ****
! /*	$OpenBSD: tpms.c,v 1.12 2007/06/14 10:11:16 mbalmer Exp $	*/
  
  /*
   * Copyright (c) 2005, Johan Wallén
--- 1,4 ----
! /*	$OpenBSD: tpms.c,v 1.13 2009/03/08 14:10:08 robert Exp $	*/
  
  /*
   * Copyright (c) 2005, Johan Wallén
***************
*** 35,41 ****
  
  /*
   * The tpms driver provides support for the trackpad on new (post
!  * February 2005) Apple PowerBooks (and iBooks?) that are not standard
   * USB HID mice.
   */
  
--- 35,41 ----
  
  /*
   * The tpms driver provides support for the trackpad on new (post
!  * February 2005) Apple PowerBooks and iBooks that are not standard
   * USB HID mice.
   */
  
***************
*** 125,159 ****
  #include <dev/wscons/wsmousevar.h>
  
  /*
-  * Debugging output.
-  */
- 
- /* XXX Should be redone, and its use should be added back. */
- 
- #ifdef TPMS_DEBUG
- 
- /*
-  * Print the error message (preceded by the driver and function)
-  * specified by the string literal fmt (followed by newline) if
-  * tpmsdebug is greater than n. The macro may only be used in the
-  * scope of sc, which must be castable to struct device *. There must
-  * be at least one vararg. Do not define TPMS_DEBUG on non-C99
-  * compilers.
-  */
- 
- #define DPRINTFN(n, fmt, ...)						      \
- do {									      \
- 	if (tpmsdebug > (n))						      \
- 		logprintf("%s: %s: " fmt "\n",				      \
- 			  ((struct device *) sc)->dv_xname,		      \
- 			  __func__, __VA_ARGS__);			      \
- } while ( /* CONSTCOND */ 0)
- 
- int tpmsdebug = 0;
- 
- #endif /* TPMS_DEBUG */
- 
- /*
   * Magic numbers.
   */
  
--- 125,130 ----
*************** int tpmsdebug = 0;
*** 170,176 ****
   * can be different for each device.  The meanings of the parameters
   * are as follows.
   *
!  * desc:      A printable description used for dmesg output.
   *
   * noise:     Amount of noise in the computed position. This controls
   *            how large a change must be to get reported, and how 
--- 141,148 ----
   * can be different for each device.  The meanings of the parameters
   * are as follows.
   *
!  * type:      Type of the trackpad device, used for dmesg output, and
!  *            to know some of the device parameters.
   *
   * noise:     Amount of noise in the computed position. This controls
   *            how large a change must be to get reported, and how 
*************** int tpmsdebug = 0;
*** 200,206 ****
   */
  
  struct tpms_dev {
! 	const char *descr; /* Description of the driver (for dmesg). */
  	int noise;	   /* Amount of noise in the computed position. */
  	int threshold;	   /* Changes less than this are ignored. */
  	int x_factor;	   /* Factor used in computation with X-coordinates. */
--- 172,181 ----
   */
  
  struct tpms_dev {
! 	int type;	   /* Type of the trackpad. */
! #define FOUNTAIN	0x00
! #define GEYSER1		0x01
! #define GEYSER2		0x02
  	int noise;	   /* Amount of noise in the computed position. */
  	int threshold;	   /* Changes less than this are ignored. */
  	int x_factor;	   /* Factor used in computation with X-coordinates. */
*************** struct tpms_dev {
*** 214,222 ****
  /* Devices supported by this driver. */
  static struct tpms_dev tpms_devices[] =
  {
! #define POWERBOOK_TOUCHPAD(inches, prod, x_fact, x_sens, y_fact)	\
         {								\
! 		.descr = #inches " inch PowerBook Trackpad",		\
  		.vendor = USB_VENDOR_APPLE,				\
  		.product = (prod),					\
  		.noise = 16,						\
--- 189,197 ----
  /* Devices supported by this driver. */
  static struct tpms_dev tpms_devices[] =
  {
! #define POWERBOOK_TOUCHPAD(ttype, prod, x_fact, x_sens, y_fact)	\
         {								\
! 		.type = (ttype),						\
  		.vendor = USB_VENDOR_APPLE,				\
  		.product = (prod),					\
  		.noise = 16,						\
*************** static struct tpms_dev tpms_devices[] =
*** 227,240 ****
  		.y_sensors = 16						\
         }
         /* 12 inch PowerBooks */
!        POWERBOOK_TOUCHPAD(12, 0x030a, 69, 16, 52), /* XXX Not tested. */
!        /* 14 inch iBook G4 */
!        POWERBOOK_TOUCHPAD(14, 0x030b, 69, 16, 52),
         /* 15 inch PowerBooks */
!        POWERBOOK_TOUCHPAD(15, 0x020e, 85, 16, 57), /* XXX Not tested. */
!        POWERBOOK_TOUCHPAD(15, 0x020f, 85, 16, 57),
         /* 17 inch PowerBooks */
!        POWERBOOK_TOUCHPAD(17, 0x020d, 71, 26, 68)  /* XXX Not tested. */
  #undef POWERBOOK_TOUCHPAD
  };
  
--- 202,218 ----
  		.y_sensors = 16						\
         }
         /* 12 inch PowerBooks */
!        POWERBOOK_TOUCHPAD(FOUNTAIN, 0x030a, 69, 16, 52), /* XXX Not tested. */
!        /* 12 and 14 inch iBook G4 */
!        POWERBOOK_TOUCHPAD(GEYSER1, 0x030b, 69, 16, 52),
         /* 15 inch PowerBooks */
!        POWERBOOK_TOUCHPAD(FOUNTAIN, 0x020e, 85, 16, 57), /* XXX Not tested. */
!        POWERBOOK_TOUCHPAD(FOUNTAIN, 0x020f, 85, 16, 57),
!        POWERBOOK_TOUCHPAD(GEYSER2, 0x0214, 90, 15, 107),
!        POWERBOOK_TOUCHPAD(GEYSER2, 0x0215, 90, 15, 107),
!        POWERBOOK_TOUCHPAD(GEYSER2, 0x0216, 90, 15, 107),
         /* 17 inch PowerBooks */
!        POWERBOOK_TOUCHPAD(FOUNTAIN, 0x020d, 71, 26, 68)  /* XXX Not tested. */
  #undef POWERBOOK_TOUCHPAD
  };
  
*************** static struct tpms_dev tpms_devices[] =
*** 248,256 ****
  /* Device data. */
  struct tpms_softc {
  	struct uhidev sc_hdev;	      /* USB parent (got the struct device). */
  	int sc_acc[TPMS_SENSORS];     /* Accumulated sensor values. */
! 	signed char sc_prev[TPMS_SENSORS];   /* Previous sample. */
! 	signed char sc_sample[TPMS_SENSORS]; /* Current sample. */
  	struct device *sc_wsmousedev; /* WSMouse device. */
  	int sc_noise;		      /* Amount of noise. */
  	int sc_threshold;	      /* Threshold value. */
--- 226,236 ----
  /* Device data. */
  struct tpms_softc {
  	struct uhidev sc_hdev;	      /* USB parent (got the struct device). */
+ 	int sc_type;		      /* Type of the trackpad */
+ 	int sc_datalen;
  	int sc_acc[TPMS_SENSORS];     /* Accumulated sensor values. */
! 	unsigned char sc_prev[TPMS_SENSORS];   /* Previous sample. */
! 	unsigned char sc_sample[TPMS_SENSORS]; /* Current sample. */
  	struct device *sc_wsmousedev; /* WSMouse device. */
  	int sc_noise;		      /* Amount of noise. */
  	int sc_threshold;	      /* Threshold value. */
*************** void tpms_intr(struct uhidev *, void *, unsigned int);
*** 275,281 ****
  int tpms_enable(void *);
  void tpms_disable(void *);
  int tpms_ioctl(void *, unsigned long, caddr_t, int, struct proc *);
! void reorder_sample(signed char *, signed char *);
  int compute_delta(struct tpms_softc *, int *, int *, int *, uint32_t *);
  int detect_pos(int *, int, int, int, int *, int *);
  int smooth_pos(int, int, int);
--- 255,261 ----
  int tpms_enable(void *);
  void tpms_disable(void *);
  int tpms_ioctl(void *, unsigned long, caddr_t, int, struct proc *);
! void reorder_sample(struct tpms_softc*, unsigned char *, unsigned char *);
  int compute_delta(struct tpms_softc *, int *, int *, int *, uint32_t *);
  int detect_pos(int *, int, int, int, int *, int *);
  int smooth_pos(int, int, int);
*************** tpms_attach(struct device *parent, struct device *self
*** 352,357 ****
--- 332,339 ----
  	int i;
  	uint16_t vendor, product;
  
+ 	sc->sc_datalen = TPMS_DATA_LEN;
+ 
  	/* Fill in device-specific parameters. */
  	if ((udd = usbd_get_device_descriptor(uha->parent->sc_udev)) != NULL) {
  		product = UGETW(udd->idProduct);
*************** tpms_attach(struct device *parent, struct device *self
*** 359,365 ****
  		for (i = 0; i < TPMS_NUM_DEVICES; i++) {
  			pd = &tpms_devices[i];
  			if (product == pd->product && vendor == pd->vendor) {
! 				printf(": %s\n", pd->descr);
  				sc->sc_noise = pd->noise;
  				sc->sc_threshold = pd->threshold;
  				sc->sc_x_factor = pd->x_factor;
--- 341,361 ----
  		for (i = 0; i < TPMS_NUM_DEVICES; i++) {
  			pd = &tpms_devices[i];
  			if (product == pd->product && vendor == pd->vendor) {
! 				switch (pd->type) {
! 				case FOUNTAIN:
! 					printf(": Fountain");
! 					break;
! 				case GEYSER1:
! 					printf(": Geyser");
! 					break;
! 				case GEYSER2:
! 					sc->sc_type = GEYSER2;
! 					sc->sc_datalen = 64;
! 					sc->sc_y_sensors = 9;
! 					printf(": Geyser 2"); 
! 					break;
! 				}
! 				printf(" Trackpad\n");
  				sc->sc_noise = pd->noise;
  				sc->sc_threshold = pd->threshold;
  				sc->sc_x_factor = pd->x_factor;
*************** void
*** 480,499 ****
  tpms_intr(struct uhidev *addr, void *ibuf, unsigned int len)
  {
  	struct tpms_softc *sc = (struct tpms_softc *)addr;
! 	signed char *data;
  	int dx, dy, dz, i, s;
  	uint32_t buttons;
  
  	/* Ignore incomplete data packets. */
! 	if (len != TPMS_DATA_LEN)
  		return;
  	data = ibuf;
  
  	/* The last byte is 1 if the button is pressed and 0 otherwise. */
! 	buttons = !!data[TPMS_DATA_LEN - 1];
  
  	/* Everything below assumes that the sample is reordered. */
! 	reorder_sample(sc->sc_sample, data);
  
  	/* Is this the first sample? */
  	if (!(sc->sc_status & TPMS_VALID)) {
--- 476,495 ----
  tpms_intr(struct uhidev *addr, void *ibuf, unsigned int len)
  {
  	struct tpms_softc *sc = (struct tpms_softc *)addr;
! 	unsigned char *data;
  	int dx, dy, dz, i, s;
  	uint32_t buttons;
  
  	/* Ignore incomplete data packets. */
! 	if (len != sc->sc_datalen)
  		return;
  	data = ibuf;
  
  	/* The last byte is 1 if the button is pressed and 0 otherwise. */
! 	buttons = !!data[sc->sc_datalen - 1];
  
  	/* Everything below assumes that the sample is reordered. */
! 	reorder_sample(sc, sc->sc_sample, data);
  
  	/* Is this the first sample? */
  	if (!(sc->sc_status & TPMS_VALID)) {
*************** tpms_intr(struct uhidev *addr, void *ibuf, unsigned in
*** 506,512 ****
  	}
  	/* Accumulate the sensor change while keeping it nonnegative. */
  	for (i = 0; i < TPMS_SENSORS; i++) {
! 		sc->sc_acc[i] += sc->sc_sample[i] - sc->sc_prev[i];
  		if (sc->sc_acc[i] < 0)
  			sc->sc_acc[i] = 0;
  	}
--- 502,510 ----
  	}
  	/* Accumulate the sensor change while keeping it nonnegative. */
  	for (i = 0; i < TPMS_SENSORS; i++) {
! 		sc->sc_acc[i] +=
! 			(signed char)(sc->sc_sample[i] - sc->sc_prev[i]);
! 
  		if (sc->sc_acc[i] < 0)
  			sc->sc_acc[i] = 0;
  	}
*************** tpms_intr(struct uhidev *addr, void *ibuf, unsigned in
*** 535,560 ****
   */
  
  void 
! reorder_sample(signed char *to, signed char *from)
  {
  	int i;
  
! 	for (i = 0; i < 8; i++) {
! 		/* X-sensors. */
! 		to[i] = from[5 * i + 2];
! 		to[i + 8] = from[5 * i + 4];
! 		to[i + 16] = from[5 * i + 42];
  #if 0
! 		/* 
! 		 * XXX This seems to introduce random vertical jumps, so
! 		 * we ignore these sensors until we figure out their meaning.
! 		 */
! 		if (i < 2)
! 			to[i + 24] = from[5 * i + 44];
  #endif /* 0 */
! 		/* Y-sensors. */
! 		to[i + 26] = from[5 * i + 1];
! 		to[i + 34] = from[5 * i + 3];
  	}
  }
  
--- 533,572 ----
   */
  
  void 
! reorder_sample(struct tpms_softc *sc, unsigned char *to, unsigned char *from)
  {
  	int i;
  
! 	if (sc->sc_type == GEYSER2) {
! 		int j;
! 
! 		memset(to, 0, TPMS_SENSORS);
! 		for (i = 0, j = 19; i < 20; i += 2, j += 3) {
! 			to[i] = from[j];
! 			to[i + 1] = from[j + 1];
! 		}
! 		for (i = 0, j = 1; i < 9; i += 2, j += 3) {
! 			to[TPMS_X_SENSORS + i] = from[j];
! 			to[TPMS_X_SENSORS + i + 1] = from[j + 1];
! 		}
! 	} else {
! 		for (i = 0; i < 8; i++) {
! 			/* X-sensors. */
! 			to[i] = from[5 * i + 2];
! 			to[i + 8] = from[5 * i + 4];
! 			to[i + 16] = from[5 * i + 42];
  #if 0
! 			/* 
! 			 * XXX This seems to introduce random ventical jumps, so
! 			 * we ignore these sensors until we figure out their meaning.
! 			 */
! 			if (i < 2)
! 				to[i + 24] = from[5 * i + 44];
  #endif /* 0 */
! 			/* Y-sensors. */
! 			to[i + 26] = from[5 * i + 1];
! 			to[i + 34] = from[5 * i + 3];
! 		}
  	}
  }
  
*************** compute_delta(struct tpms_softc *sc, int *dx, int *dy,
*** 582,623 ****
  	fingers = max(x_fingers, y_fingers);
  
  	/* Check the number of fingers and if we have detected a position. */
! 	if (fingers > 1) {
! 		/* More than one finger detected, resetting. */
! 		memset(sc->sc_acc, 0, sizeof(sc->sc_acc));
! 		sc->sc_x_raw = sc->sc_y_raw = sc->sc_x = sc->sc_y = -1;
! 		return 0;
! 	} else if (x_det == 0 && y_det == 0) {
  		/* No position detected, resetting. */
  		memset(sc->sc_acc, 0, sizeof(sc->sc_acc));
  		sc->sc_x_raw = sc->sc_y_raw = sc->sc_x = sc->sc_y = -1;
  	} else if (x_det > 0 && y_det > 0) {
! 		/* Smooth position. */
! 		if (sc->sc_x_raw >= 0) {
! 			sc->sc_x_raw = (3 * sc->sc_x_raw + x_raw) / 4;
! 			sc->sc_y_raw = (3 * sc->sc_y_raw + y_raw) / 4;
! 			/* 
! 			 * Compute virtual position and change if we already
! 			 * have a decent position. 
! 			 */
! 			if (sc->sc_x >= 0) {
! 				x = smooth_pos(sc->sc_x, sc->sc_x_raw,
! 					       sc->sc_noise);
! 				y = smooth_pos(sc->sc_y, sc->sc_y_raw,
! 					       sc->sc_noise);
! 				*dx = x - sc->sc_x;
! 				*dy = y - sc->sc_y;
! 				sc->sc_x = x;
! 				sc->sc_y = y;
  			} else {
! 				/* Initialise virtual position. */
! 				sc->sc_x = sc->sc_x_raw;
! 				sc->sc_y = sc->sc_y_raw;
  			}
! 		} else {
! 			/* Initialise raw position. */
! 			sc->sc_x_raw = x_raw;
! 			sc->sc_y_raw = y_raw;
  		}
  	}
  	return (1);
--- 594,642 ----
  	fingers = max(x_fingers, y_fingers);
  
  	/* Check the number of fingers and if we have detected a position. */
! 	if (x_det == 0 && y_det == 0) {
  		/* No position detected, resetting. */
  		memset(sc->sc_acc, 0, sizeof(sc->sc_acc));
  		sc->sc_x_raw = sc->sc_y_raw = sc->sc_x = sc->sc_y = -1;
  	} else if (x_det > 0 && y_det > 0) {
! 		switch (fingers) {
! 		case 1:
! 			/* Smooth position. */
! 			if (sc->sc_x_raw >= 0) {
! 				sc->sc_x_raw = (3 * sc->sc_x_raw + x_raw) / 4;
! 				sc->sc_y_raw = (3 * sc->sc_y_raw + y_raw) / 4;
! 				/* 
! 				 * Compute virtual position and change if we
! 				 * already have a decent position. 
! 				 */
! 				if (sc->sc_x >= 0) {
! 					x = smooth_pos(sc->sc_x, sc->sc_x_raw,
! 						       sc->sc_noise);
! 					y = smooth_pos(sc->sc_y, sc->sc_y_raw,
! 						       sc->sc_noise);
! 					*dx = x - sc->sc_x;
! 					*dy = y - sc->sc_y;
! 					sc->sc_x = x;
! 					sc->sc_y = y;
! 				} else {
! 					/* Initialise virtual position. */
! 					sc->sc_x = sc->sc_x_raw;
! 					sc->sc_y = sc->sc_y_raw;
! 				}
  			} else {
! 				/* Initialise raw position. */
! 				sc->sc_x_raw = x_raw;
! 				sc->sc_y_raw = y_raw;
  			}
! 			break;
! 		case 2:
! 			if (*buttons == 1)
! 				*buttons = 4;
! 			break;
! 		case 3:
! 			if (*buttons == 1)
! 				*buttons = 2;
! 			break;
  		}
  	}
  	return (1);
Index: src/sys/arch/sparc64/sparc64/process_machdep.c
===================================================================
RCS file: /cvs/src/sys/arch/sparc64/sparc64/process_machdep.c,v
retrieving revision 1.11
retrieving revision 1.12
diff -c -p -r1.11 -r1.12
*** src/sys/arch/sparc64/sparc64/process_machdep.c	31 Oct 2007 22:46:52 -0000	1.11
--- src/sys/arch/sparc64/sparc64/process_machdep.c	5 Mar 2009 19:52:23 -0000	1.12
***************
*** 1,4 ****
! /*	$OpenBSD: process_machdep.c,v 1.11 2007/10/31 22:46:52 kettenis Exp $	*/
  /*	$NetBSD: process_machdep.c,v 1.10 2000/09/26 22:05:50 eeh Exp $ */
  
  /*
--- 1,4 ----
! /*	$OpenBSD: process_machdep.c,v 1.12 2009/03/05 19:52:23 kettenis Exp $	*/
  /*	$NetBSD: process_machdep.c,v 1.10 2000/09/26 22:05:50 eeh Exp $ */
  
  /*
*************** process_write_fpregs(p, regs)
*** 231,240 ****
  	return 0;
  }
  
  register_t
  process_get_wcookie(struct proc *p)
  {
  	return p->p_addr->u_pcb.pcb_wcookie;
  }
- 
- #endif	/* PTRACE */
--- 231,240 ----
  	return 0;
  }
  
+ #endif	/* PTRACE */
+ 
  register_t
  process_get_wcookie(struct proc *p)
  {
  	return p->p_addr->u_pcb.pcb_wcookie;
  }
Index: src/sys/compat/aout/compat_aout.c
===================================================================
RCS file: /cvs/src/sys/compat/aout/compat_aout.c,v
retrieving revision 1.2
retrieving revision 1.3
diff -c -p -r1.2 -r1.3
*** src/sys/compat/aout/compat_aout.c	23 Aug 2003 19:28:53 -0000	1.2
--- src/sys/compat/aout/compat_aout.c	5 Mar 2009 19:52:23 -0000	1.3
***************
*** 1,4 ****
! /* 	$OpenBSD: compat_aout.c,v 1.2 2003/08/23 19:28:53 tedu Exp $ */
  
  /*
   * Copyright (c) 2003 Marc Espie
--- 1,4 ----
! /* 	$OpenBSD: compat_aout.c,v 1.3 2009/03/05 19:52:23 kettenis Exp $ */
  
  /*
   * Copyright (c) 2003 Marc Espie
***************
*** 30,35 ****
--- 30,36 ----
  #include <sys/mount.h>
  #include <sys/syscallargs.h>
  #include <sys/fcntl.h>
+ #include <sys/core.h>
  #include <compat/common/compat_util.h>
  
  void aout_compat_setup(struct exec_package *epp);
*************** struct emul emul_aout = {
*** 54,59 ****
--- 55,61 ----
  	copyargs,
  	setregs,
  	NULL,
+ 	coredump_trad,
  	sigcode,
  	esigcode,
  };
Index: src/sys/compat/bsdos/bsdos_exec.c
===================================================================
RCS file: /cvs/src/sys/compat/bsdos/Attic/bsdos_exec.c,v
retrieving revision 1.4
retrieving revision 1.5
diff -c -p -r1.4 -r1.5
*** src/sys/compat/bsdos/bsdos_exec.c	6 Nov 2001 19:53:17 -0000	1.4
--- src/sys/compat/bsdos/bsdos_exec.c	5 Mar 2009 19:52:23 -0000	1.5
***************
*** 1,4 ****
! /*	$OpenBSD: bsdos_exec.c,v 1.4 2001/11/06 19:53:17 miod Exp $	*/
  
  /*
   * Copyright (c) 1993, 1994 Christopher G. Demetriou
--- 1,4 ----
! /*	$OpenBSD: bsdos_exec.c,v 1.5 2009/03/05 19:52:23 kettenis Exp $	*/
  
  /*
   * Copyright (c) 1993, 1994 Christopher G. Demetriou
***************
*** 36,41 ****
--- 36,42 ----
  #include <sys/signalvar.h>
  #include <sys/malloc.h>
  #include <sys/vnode.h>
+ #include <sys/core.h>
  #include <sys/exec.h>
  #include <sys/resourcevar.h>
  #include <uvm/uvm_extern.h>
*************** struct emul emul_bsdos = {
*** 70,75 ****
--- 71,77 ----
  	copyargs,
  	setregs,
  	NULL,
+ 	coredump_trad,
  	sigcode,
  	esigcode,
  };
Index: src/sys/compat/freebsd/freebsd_exec.c
===================================================================
RCS file: /cvs/src/sys/compat/freebsd/Attic/freebsd_exec.c,v
retrieving revision 1.18
retrieving revision 1.19
diff -c -p -r1.18 -r1.19
*** src/sys/compat/freebsd/freebsd_exec.c	12 Jun 2008 04:32:57 -0000	1.18
--- src/sys/compat/freebsd/freebsd_exec.c	5 Mar 2009 19:52:23 -0000	1.19
***************
*** 1,4 ****
! /*	$OpenBSD: freebsd_exec.c,v 1.18 2008/06/12 04:32:57 miod Exp $	*/
  /*	$NetBSD: freebsd_exec.c,v 1.2 1996/05/18 16:02:08 christos Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: freebsd_exec.c,v 1.19 2009/03/05 19:52:23 kettenis Exp $	*/
  /*	$NetBSD: freebsd_exec.c,v 1.2 1996/05/18 16:02:08 christos Exp $	*/
  
  /*
***************
*** 36,41 ****
--- 36,42 ----
  #include <sys/proc.h>
  #include <sys/malloc.h>
  #include <sys/vnode.h>
+ #include <sys/core.h>
  #include <sys/exec.h>
  #include <sys/resourcevar.h>
  #include <uvm/uvm_extern.h>
*************** struct emul emul_freebsd_aout = {
*** 71,76 ****
--- 72,78 ----
  	copyargs,
  	setregs,
  	NULL,
+ 	coredump_trad,
  	freebsd_sigcode,
  	freebsd_esigcode,
  };
*************** struct emul emul_freebsd_elf = {
*** 91,96 ****
--- 93,99 ----
  	elf32_copyargs,
  	setregs,
  	exec_elf32_fixup,
+ 	coredump_trad,
  	freebsd_sigcode,
  	freebsd_esigcode,
  };
Index: src/sys/compat/hpux/hppa/hpux_exec.c
===================================================================
RCS file: /cvs/src/sys/compat/hpux/hppa/Attic/hpux_exec.c,v
retrieving revision 1.3
retrieving revision 1.4
diff -c -p -r1.3 -r1.4
*** src/sys/compat/hpux/hppa/hpux_exec.c	30 Dec 2005 19:46:53 -0000	1.3
--- src/sys/compat/hpux/hppa/hpux_exec.c	5 Mar 2009 19:52:23 -0000	1.4
***************
*** 1,4 ****
! /*	$OpenBSD: hpux_exec.c,v 1.3 2005/12/30 19:46:53 miod Exp $	*/
  
  /*
   * Copyright (c) 2004 Michael Shalayeff.  All rights reserved.
--- 1,4 ----
! /*	$OpenBSD: hpux_exec.c,v 1.4 2009/03/05 19:52:23 kettenis Exp $	*/
  
  /*
   * Copyright (c) 2004 Michael Shalayeff.  All rights reserved.
***************
*** 46,51 ****
--- 46,52 ----
  #include <sys/vnode.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
+ #include <sys/core.h>
  
  #include <uvm/uvm_extern.h>
  
*************** struct emul emul_hpux = {
*** 89,94 ****
--- 90,96 ----
  	copyargs,
  	hpux_setregs,
  	NULL,
+ 	coredump_trad,
  	hpux_sigcode,
  	hpux_esigcode,
  };
Index: src/sys/compat/hpux/m68k/hpux_exec.c
===================================================================
RCS file: /cvs/src/sys/compat/hpux/m68k/Attic/hpux_exec.c,v
retrieving revision 1.3
retrieving revision 1.4
diff -c -p -r1.3 -r1.4
*** src/sys/compat/hpux/m68k/hpux_exec.c	2 Nov 2007 19:18:54 -0000	1.3
--- src/sys/compat/hpux/m68k/hpux_exec.c	5 Mar 2009 19:52:23 -0000	1.4
***************
*** 1,4 ****
! /*	$OpenBSD: hpux_exec.c,v 1.3 2007/11/02 19:18:54 martin Exp $	*/
  /*	$NetBSD: hpux_exec.c,v 1.8 1997/03/16 10:14:44 thorpej Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: hpux_exec.c,v 1.4 2009/03/05 19:52:23 kettenis Exp $	*/
  /*	$NetBSD: hpux_exec.c,v 1.8 1997/03/16 10:14:44 thorpej Exp $	*/
  
  /*
***************
*** 46,51 ****
--- 46,52 ----
  #include <sys/vnode.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
+ #include <sys/core.h>
  
  #include <uvm/uvm_extern.h>
  
*************** struct emul emul_hpux = {
*** 89,94 ****
--- 90,96 ----
  	copyargs,
  	hpux_setregs,
  	NULL,
+ 	coredump_trad,
  	sigcode,
  	esigcode,
  };
Index: src/sys/compat/ibcs2/ibcs2_exec.c
===================================================================
RCS file: /cvs/src/sys/compat/ibcs2/Attic/ibcs2_exec.c,v
retrieving revision 1.18
retrieving revision 1.19
diff -c -p -r1.18 -r1.19
*** src/sys/compat/ibcs2/ibcs2_exec.c	29 Dec 2006 13:04:37 -0000	1.18
--- src/sys/compat/ibcs2/ibcs2_exec.c	5 Mar 2009 19:52:23 -0000	1.19
***************
*** 1,4 ****
! /*	$OpenBSD: ibcs2_exec.c,v 1.18 2006/12/29 13:04:37 pedro Exp $	*/
  /*	$NetBSD: ibcs2_exec.c,v 1.12 1996/10/12 02:13:52 thorpej Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: ibcs2_exec.c,v 1.19 2009/03/05 19:52:23 kettenis Exp $	*/
  /*	$NetBSD: ibcs2_exec.c,v 1.12 1996/10/12 02:13:52 thorpej Exp $	*/
  
  /*
***************
*** 38,43 ****
--- 38,44 ----
  #include <sys/param.h>
  #include <sys/systm.h>
  #include <sys/proc.h>
+ #include <sys/core.h>
  #include <sys/exec.h>
  #include <sys/malloc.h>
  #include <sys/vnode.h>
*************** struct emul emul_ibcs2 = {
*** 100,105 ****
--- 101,107 ----
  	copyargs,
  	setregs,
  	NULL,
+ 	coredump_trad,
  	sigcode,
  	esigcode,
  };
Index: src/sys/compat/linux/linux_exec.c
===================================================================
RCS file: /cvs/src/sys/compat/linux/linux_exec.c,v
retrieving revision 1.30
retrieving revision 1.31
diff -c -p -r1.30 -r1.31
*** src/sys/compat/linux/linux_exec.c	26 Jun 2008 05:42:14 -0000	1.30
--- src/sys/compat/linux/linux_exec.c	5 Mar 2009 19:52:24 -0000	1.31
***************
*** 1,4 ****
! /*	$OpenBSD: linux_exec.c,v 1.30 2008/06/26 05:42:14 ray Exp $	*/
  /*	$NetBSD: linux_exec.c,v 1.13 1996/04/05 00:01:10 christos Exp $	*/
  
  /*-
--- 1,4 ----
! /*	$OpenBSD: linux_exec.c,v 1.31 2009/03/05 19:52:24 kettenis Exp $	*/
  /*	$NetBSD: linux_exec.c,v 1.13 1996/04/05 00:01:10 christos Exp $	*/
  
  /*-
***************
*** 39,44 ****
--- 39,45 ----
  #include <sys/namei.h>
  #include <sys/vnode.h>
  #include <sys/mount.h>
+ #include <sys/core.h>
  #include <sys/exec.h>
  #include <sys/exec_elf.h>
  #include <sys/exec_olf.h>
*************** struct emul emul_linux_aout = {
*** 101,106 ****
--- 102,108 ----
  	linux_aout_copyargs,
  	setregs,
  	NULL,
+ 	coredump_trad,
  	linux_sigcode,
  	linux_esigcode,
  	0,
*************** struct emul emul_linux_elf = {
*** 126,131 ****
--- 128,134 ----
  	elf32_copyargs,
  	setregs,
  	exec_elf32_fixup,
+ 	coredump_trad,
  	linux_sigcode,
  	linux_esigcode,
  	0,
Index: src/sys/compat/osf1/osf1_exec.c
===================================================================
RCS file: /cvs/src/sys/compat/osf1/Attic/osf1_exec.c,v
retrieving revision 1.5
retrieving revision 1.6
diff -c -p -r1.5 -r1.6
*** src/sys/compat/osf1/osf1_exec.c	22 Jun 2004 23:52:18 -0000	1.5
--- src/sys/compat/osf1/osf1_exec.c	5 Mar 2009 19:52:24 -0000	1.6
***************
*** 1,4 ****
! /* $OpenBSD: osf1_exec.c,v 1.5 2004/06/22 23:52:18 jfb Exp $ */
  /* $NetBSD$ */
  
  /*
--- 1,4 ----
! /* $OpenBSD: osf1_exec.c,v 1.6 2009/03/05 19:52:24 kettenis Exp $ */
  /* $NetBSD$ */
  
  /*
***************
*** 39,44 ****
--- 39,45 ----
  #include <sys/namei.h>
  #include <sys/vnode.h>
  #include <sys/mount.h>
+ #include <sys/core.h>
  #include <sys/exec.h>
  #include <sys/exec_ecoff.h>
  #include <sys/signalvar.h>
*************** struct emul emul_osf1 = {
*** 90,95 ****
--- 91,97 ----
          osf1_copyargs,
          cpu_exec_ecoff_setregs,
          NULL,
+ 	coredump_trad,
          osf1_sigcode,
          osf1_esigcode,
  };
Index: src/sys/compat/sunos/sunos_exec.c
===================================================================
RCS file: /cvs/src/sys/compat/sunos/Attic/sunos_exec.c,v
retrieving revision 1.18
retrieving revision 1.19
diff -c -p -r1.18 -r1.19
*** src/sys/compat/sunos/sunos_exec.c	30 Dec 2005 19:46:55 -0000	1.18
--- src/sys/compat/sunos/sunos_exec.c	5 Mar 2009 19:52:24 -0000	1.19
***************
*** 1,4 ****
! /*	$OpenBSD: sunos_exec.c,v 1.18 2005/12/30 19:46:55 miod Exp $	*/
  /*	$NetBSD: sunos_exec.c,v 1.11 1996/05/05 12:01:47 briggs Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: sunos_exec.c,v 1.19 2009/03/05 19:52:24 kettenis Exp $	*/
  /*	$NetBSD: sunos_exec.c,v 1.11 1996/05/05 12:01:47 briggs Exp $	*/
  
  /*
***************
*** 37,42 ****
--- 37,43 ----
  #include <sys/signalvar.h>
  #include <sys/vnode.h>
  #include <sys/file.h>
+ #include <sys/core.h>
  #include <sys/exec.h>
  #include <sys/resourcevar.h>
  #include <sys/wait.h>
*************** struct emul emul_sunos = {
*** 91,96 ****
--- 92,98 ----
  	copyargs,
  	setregs,
  	NULL,
+ 	coredump_trad,
  	sigcode,
  	esigcode,
  };
Index: src/sys/compat/svr4/svr4_exec.c
===================================================================
RCS file: /cvs/src/sys/compat/svr4/Attic/svr4_exec.c,v
retrieving revision 1.16
retrieving revision 1.17
diff -c -p -r1.16 -r1.17
*** src/sys/compat/svr4/svr4_exec.c	12 Jun 2008 04:32:59 -0000	1.16
--- src/sys/compat/svr4/svr4_exec.c	5 Mar 2009 19:52:24 -0000	1.17
***************
*** 1,4 ****
! /*	$OpenBSD: svr4_exec.c,v 1.16 2008/06/12 04:32:59 miod Exp $	 */
  /*	$NetBSD: svr4_exec.c,v 1.16 1995/10/14 20:24:20 christos Exp $	 */
  
  /*
--- 1,4 ----
! /*	$OpenBSD: svr4_exec.c,v 1.17 2009/03/05 19:52:24 kettenis Exp $	 */
  /*	$NetBSD: svr4_exec.c,v 1.16 1995/10/14 20:24:20 christos Exp $	 */
  
  /*
***************
*** 35,40 ****
--- 35,41 ----
  #include <sys/malloc.h>
  #include <sys/namei.h>
  #include <sys/vnode.h>
+ #include <sys/core.h>
  #include <sys/exec.h>
  #include <sys/exec_elf.h>
  #include <sys/exec_olf.h>
*************** struct emul emul_svr4 = {
*** 77,82 ****
--- 78,84 ----
  	svr4_copyargs,
  	setregs,
  	exec_elf32_fixup,
+ 	coredump_trad,
  	svr4_sigcode,
  	svr4_esigcode,
  };
Index: src/sys/compat/ultrix/ultrix_misc.c
===================================================================
RCS file: /cvs/src/sys/compat/ultrix/Attic/ultrix_misc.c,v
retrieving revision 1.30
retrieving revision 1.31
diff -c -p -r1.30 -r1.31
*** src/sys/compat/ultrix/ultrix_misc.c	6 Jun 2007 17:15:13 -0000	1.30
--- src/sys/compat/ultrix/ultrix_misc.c	5 Mar 2009 19:52:24 -0000	1.31
***************
*** 1,4 ****
! /*	$OpenBSD: ultrix_misc.c,v 1.30 2007/06/06 17:15:13 deraadt Exp $	*/
  /*	$NetBSD: ultrix_misc.c,v 1.23 1996/04/07 17:23:04 jonathan Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: ultrix_misc.c,v 1.31 2009/03/05 19:52:24 kettenis Exp $	*/
  /*	$NetBSD: ultrix_misc.c,v 1.23 1996/04/07 17:23:04 jonathan Exp $	*/
  
  /*
***************
*** 89,94 ****
--- 89,95 ----
  /*#include <sys/stat.h>*/
  /*#include <sys/ioctl.h>*/
  #include <sys/kernel.h>
+ #include <sys/core.h>
  #include <sys/exec.h>
  #include <sys/malloc.h>
  #include <sys/mbuf.h>
*************** struct emul emul_ultrix = {
*** 161,166 ****
--- 162,168 ----
  	copyargs,
  	ULTRIX_EXEC_SETREGS,
  	NULL,
+ 	coredump_trad,
  	sigcode,
  	esigcode,
  };
Index: src/sys/dev/bluetooth/bthidev.c
===================================================================
RCS file: /cvs/src/sys/dev/bluetooth/bthidev.c,v
retrieving revision 1.3
retrieving revision 1.4
diff -c -p -r1.3 -r1.4
*** src/sys/dev/bluetooth/bthidev.c	15 Oct 2008 19:12:19 -0000	1.3
--- src/sys/dev/bluetooth/bthidev.c	22 Nov 2008 04:42:58 -0000	1.4
***************
*** 1,5 ****
! /*	$OpenBSD: bthidev.c,v 1.3 2008/10/15 19:12:19 blambert Exp $	*/
! /*	$NetBSD: bthidev.c,v 1.13 2007/11/12 19:19:32 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
--- 1,5 ----
! /*	$OpenBSD: bthidev.c,v 1.4 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: bthidev.c,v 1.16 2008/08/06 15:01:23 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
*************** struct bthidev_softc {
*** 88,94 ****
  #define BTHID_WAIT_CTL		1
  #define BTHID_WAIT_INT		2
  #define BTHID_OPEN		3
- #define BTHID_DETACHING		4
  
  #define	BTHID_RETRY_INTERVAL	5	/* seconds between connection attempts */
  
--- 88,93 ----
*************** bthidev_attach(struct device *parent, struct device *s
*** 177,183 ****
  	struct bthidev *hidev;
  	struct hid_data *d;
  	struct hid_item h;
! 	int maxid, rep, s;
  
  	/*
  	 * Init softc
--- 176,182 ----
  	struct bthidev *hidev;
  	struct hid_data *d;
  	struct hid_item h;
! 	int maxid, rep;
  
  	/*
  	 * Init softc
*************** bthidev_attach(struct device *parent, struct device *s
*** 189,194 ****
--- 188,195 ----
  	sc->sc_ctlpsm = L2CAP_PSM_HID_CNTL;
  	sc->sc_intpsm = L2CAP_PSM_HID_INTR;
  
+ 	sc->sc_mode = 0;
+ 
  	/*
  	 * copy in our configuration info
  	 */
*************** bthidev_attach(struct device *parent, struct device *s
*** 284,296 ****
  	/*
  	 * start bluetooth connections
  	 */
! 	s = splsoftnet();
  	if ((sc->sc_flags & BTHID_RECONNECT) == 0)
  		bthidev_listen(sc);
  
  	if (sc->sc_flags & BTHID_CONNECTING)
  		bthidev_connect(sc);
! 	splx(s);
  }
  
  int
--- 285,297 ----
  	/*
  	 * start bluetooth connections
  	 */
! 	mutex_enter(&bt_lock);
  	if ((sc->sc_flags & BTHID_RECONNECT) == 0)
  		bthidev_listen(sc);
  
  	if (sc->sc_flags & BTHID_CONNECTING)
  		bthidev_connect(sc);
! 	mutex_exit(&bt_lock);
  }
  
  int
*************** bthidev_detach(struct device *self, int flags)
*** 298,306 ****
  {
  	struct bthidev_softc *sc = (struct bthidev_softc *)self;
  	struct bthidev *hidev;
- 	int s;
  
! 	s = splsoftnet();
  	sc->sc_flags = 0;	/* disable reconnecting */
  
  	/* release interrupt listen */
--- 299,306 ----
  {
  	struct bthidev_softc *sc = (struct bthidev_softc *)self;
  	struct bthidev *hidev;
  
! 	mutex_enter(&bt_lock);
  	sc->sc_flags = 0;	/* disable reconnecting */
  
  	/* release interrupt listen */
*************** bthidev_detach(struct device *self, int flags)
*** 330,339 ****
  	}
  
  	/* remove timeout */
- 	sc->sc_state = BTHID_DETACHING;
  	timeout_del(&sc->sc_reconnect);
  
! 	splx(s);
  
  	/* detach children */
  	while ((hidev = LIST_FIRST(&sc->sc_list)) != NULL) {
--- 330,338 ----
  	}
  
  	/* remove timeout */
  	timeout_del(&sc->sc_reconnect);
  
! 	mutex_exit(&bt_lock);
  
  	/* detach children */
  	while ((hidev = LIST_FIRST(&sc->sc_list)) != NULL) {
*************** void
*** 393,401 ****
  bthidev_timeout(void *arg)
  {
  	struct bthidev_softc *sc = arg;
- 	int s;
  
! 	s = splsoftnet();
  
  	switch (sc->sc_state) {
  	case BTHID_CLOSED:
--- 392,399 ----
  bthidev_timeout(void *arg)
  {
  	struct bthidev_softc *sc = arg;
  
! 	mutex_enter(&bt_lock);
  
  	switch (sc->sc_state) {
  	case BTHID_CLOSED:
*************** bthidev_timeout(void *arg)
*** 426,439 ****
  	case BTHID_OPEN:
  		break;
  
- 	case BTHID_DETACHING:
- 		wakeup(sc);
- 		break;
- 
  	default:
  		break;
  	}
! 	splx(s);
  }
  
  /*
--- 424,433 ----
  	case BTHID_OPEN:
  		break;
  
  	default:
  		break;
  	}
! 	mutex_exit(&bt_lock);
  }
  
  /*
*************** bthidev_output(struct bthidev *hidev, uint8_t *report,
*** 865,871 ****
  {
  	struct bthidev_softc *sc = (struct bthidev_softc *)hidev->sc_parent;
  	struct mbuf *m;
! 	int s, err;
  
  	if (sc == NULL || sc->sc_state != BTHID_OPEN)
  		return ENOTCONN;
--- 859,865 ----
  {
  	struct bthidev_softc *sc = (struct bthidev_softc *)hidev->sc_parent;
  	struct mbuf *m;
! 	int err;
  
  	if (sc == NULL || sc->sc_state != BTHID_OPEN)
  		return ENOTCONN;
*************** bthidev_output(struct bthidev *hidev, uint8_t *report,
*** 897,905 ****
  	memcpy(mtod(m, uint8_t *) + 2, report, rlen);
  	m->m_pkthdr.len = m->m_len = rlen + 2;
  
! 	s = splsoftnet();
  	err = l2cap_send(sc->sc_int, m);
! 	splx(s);
  
  	return err;
  }
--- 891,899 ----
  	memcpy(mtod(m, uint8_t *) + 2, report, rlen);
  	m->m_pkthdr.len = m->m_len = rlen + 2;
  
! 	mutex_enter(&bt_lock);
  	err = l2cap_send(sc->sc_int, m);
! 	mutex_exit(&bt_lock);
  
  	return err;
  }
Index: src/sys/dev/bluetooth/btkbd.c
===================================================================
RCS file: /cvs/src/sys/dev/bluetooth/btkbd.c,v
retrieving revision 1.3
retrieving revision 1.4
diff -c -p -r1.3 -r1.4
*** src/sys/dev/bluetooth/btkbd.c	24 Feb 2008 21:46:19 -0000	1.3
--- src/sys/dev/bluetooth/btkbd.c	22 Nov 2008 04:42:58 -0000	1.4
***************
*** 1,5 ****
! /*	$OpenBSD: btkbd.c,v 1.3 2008/02/24 21:46:19 uwe Exp $	*/
! /*	$NetBSD: btkbd.c,v 1.9 2007/11/03 18:24:01 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
--- 1,35 ----
! /*	$OpenBSD: btkbd.c,v 1.4 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: btkbd.c,v 1.10 2008/09/09 03:54:56 cube Exp $	*/
! 
! /*
!  * Copyright (c) 1998 The NetBSD Foundation, Inc.
!  * All rights reserved.
!  *
!  * This code is derived from software contributed to The NetBSD Foundation
!  * by Lennart Augustsson (lennart@augustsson.net) at
!  * Carlstedt Research & Technology.
!  *
!  * Redistribution and use in source and binary forms, with or without
!  * modification, are permitted provided that the following conditions
!  * are met:
!  * 1. Redistributions of source code must retain the above copyright
!  *    notice, this list of conditions and the following disclaimer.
!  * 2. Redistributions in binary form must reproduce the above copyright
!  *    notice, this list of conditions and the following disclaimer in the
!  *    documentation and/or other materials provided with the distribution.
!  *
!  * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
!  * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
!  * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
!  * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
!  * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
!  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
!  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
!  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
!  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
!  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
!  * POSSIBILITY OF SUCH DAMAGE.
!  */
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
Index: src/sys/dev/bluetooth/btms.c
===================================================================
RCS file: /cvs/src/sys/dev/bluetooth/btms.c,v
retrieving revision 1.3
retrieving revision 1.4
diff -c -p -r1.3 -r1.4
*** src/sys/dev/bluetooth/btms.c	24 Feb 2008 21:46:19 -0000	1.3
--- src/sys/dev/bluetooth/btms.c	22 Nov 2008 04:42:58 -0000	1.4
***************
*** 1,5 ****
! /*	$OpenBSD: btms.c,v 1.3 2008/02/24 21:46:19 uwe Exp $	*/
! /*	$NetBSD: btms.c,v 1.7 2007/11/03 17:41:03 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
--- 1,35 ----
! /*	$OpenBSD: btms.c,v 1.4 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: btms.c,v 1.8 2008/09/09 03:54:56 cube Exp $	*/
! 
! /*
!  * Copyright (c) 1998 The NetBSD Foundation, Inc.
!  * All rights reserved.
!  *
!  * This code is derived from software contributed to The NetBSD Foundation
!  * by Lennart Augustsson (lennart@augustsson.net) at
!  * Carlstedt Research & Technology.
!  *
!  * Redistribution and use in source and binary forms, with or without
!  * modification, are permitted provided that the following conditions
!  * are met:
!  * 1. Redistributions of source code must retain the above copyright
!  *    notice, this list of conditions and the following disclaimer.
!  * 2. Redistributions in binary form must reproduce the above copyright
!  *    notice, this list of conditions and the following disclaimer in the
!  *    documentation and/or other materials provided with the distribution.
!  *
!  * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
!  * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
!  * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
!  * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
!  * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
!  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
!  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
!  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
!  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
!  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
!  * POSSIBILITY OF SUCH DAMAGE.
!  */
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
Index: src/sys/dev/i2c/i2c_scan.c
===================================================================
RCS file: /cvs/src/sys/dev/i2c/i2c_scan.c,v
retrieving revision 1.129
retrieving revision 1.130
diff -c -p -r1.129 -r1.130
*** src/sys/dev/i2c/i2c_scan.c	12 Dec 2008 23:38:23 -0000	1.129
--- src/sys/dev/i2c/i2c_scan.c	19 Feb 2009 23:09:17 -0000	1.130
***************
*** 1,4 ****
! /*	$OpenBSD: i2c_scan.c,v 1.129 2008/12/12 23:38:23 jsg Exp $	*/
  
  /*
   * Copyright (c) 2005 Theo de Raadt <deraadt@openbsd.org>
--- 1,4 ----
! /*	$OpenBSD: i2c_scan.c,v 1.130 2009/02/19 23:09:17 jsg Exp $	*/
  
  /*
   * Copyright (c) 2005 Theo de Raadt <deraadt@openbsd.org>
*************** iic_probe_sensor(struct device *self, u_int8_t addr)
*** 926,945 ****
  char *
  iic_probe_eeprom(struct device *self, u_int8_t addr)
  {
! 	int reg, csum = 0;
! 	u_int8_t size;
  	char *name = NULL;
  
! 	/* SPD EEPROMs should only set lower nibble for size (ie <= 32K) */
! 	size = iicprobe(0x01);
! 	if (((size & 0xf0) != 0) || size == 0)
  		return (name);
  
! 	for (reg = 0; reg < 0x3f; reg++)
! 		csum += iicprobe(reg);
  
- 	if (iicprobe(0x3f) == (csum & 0xff))
- 		name = "spd";
  	return (name);
  }
  
--- 926,942 ----
  char *
  iic_probe_eeprom(struct device *self, u_int8_t addr)
  {
! 	u_int8_t type;
  	char *name = NULL;
  
! 	type = iicprobe(0x02);
! 	/* limit to SPD types seen in the wild */
! 	if (type < 4 || type > 11)
  		return (name);
  
! 	/* more matching in driver(s) */
! 	name = "eeprom";
  
  	return (name);
  }
  
Index: src/sys/dev/i2c/spdmem.c
===================================================================
RCS file: /cvs/src/sys/dev/i2c/Attic/spdmem.c,v
retrieving revision 1.30
retrieving revision 1.31
diff -c -p -r1.30 -r1.31
*** src/sys/dev/i2c/spdmem.c	19 Feb 2009 23:21:09 -0000	1.30
--- src/sys/dev/i2c/spdmem.c	22 Feb 2009 13:45:11 -0000	1.31
***************
*** 1,4 ****
! /*	$OpenBSD: spdmem.c,v 1.30 2009/02/19 23:21:09 jsg Exp $	*/
  /* $NetBSD: spdmem.c,v 1.3 2007/09/20 23:09:59 xtraeme Exp $ */
  
  /*
--- 1,4 ----
! /*	$OpenBSD: spdmem.c,v 1.31 2009/02/22 13:45:11 jsg Exp $	*/
  /* $NetBSD: spdmem.c,v 1.3 2007/09/20 23:09:59 xtraeme Exp $ */
  
  /*
***************
*** 185,190 ****
--- 185,206 ----
  #define SPDMEM_DDR2_MINI_RDIMM		(1 << 4)
  #define SPDMEM_DDR2_MINI_UDIMM		(1 << 5)
  
+ /* DDR2 FB-DIMM SDRAM */
+ #define SPDMEM_FBDIMM_ADDR		0x01
+ #define SPDMEM_FBDIMM_RANKS		0x04
+ #define SPDMEM_FBDIMM_MTB_DIVIDEND	0x06
+ #define SPDMEM_FBDIMM_MTB_DIVISOR	0x07
+ #define SPDMEM_FBDIMM_PROTO		0x4e
+ 
+ #define SPDMEM_FBDIMM_RANKS_WIDTH		0x07
+ #define SPDMEM_FBDIMM_ADDR_BANKS		0x02
+ #define SPDMEM_FBDIMM_ADDR_COL			0x0c
+ #define SPDMEM_FBDIMM_ADDR_COL_SHIFT		2
+ #define SPDMEM_FBDIMM_ADDR_ROW			0xe0
+ #define SPDMEM_FBDIMM_ADDR_ROW_SHIFT		5
+ #define SPDMEM_FBDIMM_PROTO_ECC			(1 << 1)
+ 
+ 
  /* Dual Data Rate 3 SDRAM */
  #define SPDMEM_DDR3_MODTYPE		0x00
  #define SPDMEM_DDR3_DENSITY		0x01
*************** void		 spdmem_sdram_decode(struct spdmem_softc *, stru
*** 234,239 ****
--- 250,256 ----
  void		 spdmem_rdr_decode(struct spdmem_softc *, struct spdmem *);
  void		 spdmem_ddr_decode(struct spdmem_softc *, struct spdmem *);
  void		 spdmem_ddr2_decode(struct spdmem_softc *, struct spdmem *);
+ void		 spdmem_fbdimm_decode(struct spdmem_softc *, struct spdmem *);
  void		 spdmem_ddr3_decode(struct spdmem_softc *, struct spdmem *);
  
  struct cfattach spdmem_ca = {
*************** spdmem_ddr2_decode(struct spdmem_softc *sc, struct spd
*** 616,621 ****
--- 633,688 ----
  }
  
  void
+ spdmem_fbdimm_decode(struct spdmem_softc *sc, struct spdmem *s)
+ {
+ 	int dimm_size, num_banks, cycle_time, d_clk, p_clk, bits;
+ 	uint8_t rows, cols, banks, dividend, divisor;
+ 	/*
+ 	 * FB-DIMM is very much like DDR3
+ 	 */
+ 
+ 	banks = s->sm_data[SPDMEM_FBDIMM_ADDR] & SPDMEM_FBDIMM_ADDR_BANKS;
+ 	cols = (s->sm_data[SPDMEM_FBDIMM_ADDR] & SPDMEM_FBDIMM_ADDR_COL) >>
+ 	    SPDMEM_FBDIMM_ADDR_COL_SHIFT;
+ 	rows = (s->sm_data[SPDMEM_FBDIMM_ADDR] & SPDMEM_FBDIMM_ADDR_ROW) >>
+ 	    SPDMEM_FBDIMM_ADDR_ROW_SHIFT;
+ 	dimm_size = rows + 12 + cols +  9 - 20 - 3;
+ 	num_banks = 1 << (banks + 2);
+ 
+ 	if (dimm_size < 1024)
+ 		printf(" %dMB", dimm_size);
+ 	else
+ 		printf(" %dGB", dimm_size / 1024);
+ 
+ 	if (s->sm_data[SPDMEM_FBDIMM_PROTO] & SPDMEM_FBDIMM_PROTO_ECC)
+ 		printf(" ECC");
+ 
+ 	dividend = s->sm_data[SPDMEM_FBDIMM_MTB_DIVIDEND];
+ 	divisor = s->sm_data[SPDMEM_FBDIMM_MTB_DIVISOR];
+ 
+ 	cycle_time = (1000 * dividend + (divisor / 2)) / divisor;
+ 
+ 	if (cycle_time != 0) {
+ 		/*
+ 		 * cycle time is scaled by a factor of 1000 to avoid using
+ 		 * floating point.  Calculate memory speed as the number
+ 		 * of cycles per microsecond.
+ 		 */
+ 		d_clk = 1000 * 1000;
+ 
+ 		/* DDR2 FB-DIMM uses a dual-pumped clock */
+ 		d_clk *= 2;
+ 		bits = 1 << ((s->sm_data[SPDMEM_FBDIMM_RANKS] &
+ 		    SPDMEM_FBDIMM_RANKS_WIDTH) + 2);
+ 
+ 		p_clk = (d_clk * bits) / 8 / cycle_time;
+ 		d_clk = ((d_clk + cycle_time / 2) ) / cycle_time;
+ 		p_clk -= p_clk % 100;
+ 		printf(" PC2-%d", p_clk);
+ 	}
+ }
+ 
+ void
  spdmem_ddr3_decode(struct spdmem_softc *sc, struct spdmem *s)
  {
  	const char *type;
*************** spdmem_attach(struct device *parent, struct device *se
*** 725,730 ****
--- 792,801 ----
  			break;
  		case SPDMEM_MEMTYPE_DDR2SDRAM:
  			spdmem_ddr2_decode(sc, s);
+ 			break;
+ 		case SPDMEM_MEMTYPE_FBDIMM:
+ 		case SPDMEM_MEMTYPE_FBDIMM_PROBE:
+ 			spdmem_fbdimm_decode(sc, s);
  			break;
  		case SPDMEM_MEMTYPE_DDR3SDRAM:
  			spdmem_ddr3_decode(sc, s);
Index: src/sys/dev/i2c/spdmem.c
===================================================================
RCS file: /cvs/src/sys/dev/i2c/Attic/spdmem.c,v
retrieving revision 1.28
retrieving revision 1.29
diff -c -p -r1.28 -r1.29
*** src/sys/dev/i2c/spdmem.c	24 Nov 2008 05:28:57 -0000	1.28
--- src/sys/dev/i2c/spdmem.c	19 Feb 2009 23:09:17 -0000	1.29
***************
*** 1,4 ****
! /*	$OpenBSD: spdmem.c,v 1.28 2008/11/24 05:28:57 cnst Exp $	*/
  /* $NetBSD: spdmem.c,v 1.3 2007/09/20 23:09:59 xtraeme Exp $ */
  
  /*
--- 1,4 ----
! /*	$OpenBSD: spdmem.c,v 1.29 2009/02/19 23:09:17 jsg Exp $	*/
  /* $NetBSD: spdmem.c,v 1.3 2007/09/20 23:09:59 xtraeme Exp $ */
  
  /*
***************
*** 57,62 ****
--- 57,75 ----
  
  #include <dev/i2c/i2cvar.h>
  
+ /* Encodings of the size used/total byte for certain memory types    */
+ #define	SPDMEM_SPDSIZE_MASK		0x0F	/* SPD EEPROM Size   */
+ 
+ #define	SPDMEM_SPDLEN_128		0x00	/* SPD EEPROM Sizes  */
+ #define	SPDMEM_SPDLEN_176		0x10
+ #define	SPDMEM_SPDLEN_256		0x20
+ #define	SPDMEM_SPDLEN_MASK		0x70	/* Bits 4 - 6        */
+ 
+ #define	SPDMEM_SPDCRC_116		0x80	/* CRC Bytes covered */
+ #define	SPDMEM_SPDCRC_125		0x00
+ #define	SPDMEM_SPDCRC_MASK		0x80	/* Bit 7             */
+ 
+ 
  /* possible values for the memory type */
  #define	SPDMEM_MEMTYPE_FPM		0x01
  #define	SPDMEM_MEMTYPE_EDO		0x02
***************
*** 66,71 ****
--- 79,87 ----
  #define	SPDMEM_MEMTYPE_DDRSGRAM		0x06
  #define	SPDMEM_MEMTYPE_DDRSDRAM		0x07
  #define	SPDMEM_MEMTYPE_DDR2SDRAM	0x08
+ #define	SPDMEM_MEMTYPE_FBDIMM		0x09
+ #define	SPDMEM_MEMTYPE_FBDIMM_PROBE	0x0a
+ #define	SPDMEM_MEMTYPE_DDR3SDRAM	0x0b
  #define	SPDMEM_MEMTYPE_NONE		0xff
  
  #define SPDMEM_MEMTYPE_DIRECT_RAMBUS	0x01
***************
*** 169,174 ****
--- 185,209 ----
  #define SPDMEM_DDR2_MINI_RDIMM		(1 << 4)
  #define SPDMEM_DDR2_MINI_UDIMM		(1 << 5)
  
+ /* Dual Data Rate 3 SDRAM */
+ #define SPDMEM_DDR3_MODTYPE		0x00
+ #define SPDMEM_DDR3_DENSITY		0x01
+ #define SPDMEM_DDR3_DATAWIDTH		0x05
+ #define SPDMEM_DDR3_MTB_DIVIDEND	0x07
+ #define SPDMEM_DDR3_MTB_DIVISOR		0x08
+ #define SPDMEM_DDR3_TCKMIN		0x09
+ 
+ #define SPDMEM_DDR3_DENSITY_CAPMASK		0x0f
+ #define SPDMEM_DDR3_DATAWIDTH_ECCMASK		(1 << 3)
+ #define SPDMEM_DDR3_DATAWIDTH_PRIMASK		0x07
+ 
+ #define SPDMEM_DDR3_RDIMM		0x01
+ #define SPDMEM_DDR3_UDIMM		0x02
+ #define SPDMEM_DDR3_SODIMM		0x03
+ #define SPDMEM_DDR3_MICRO_DIMM		0x04
+ #define SPDMEM_DDR3_MINI_RDIMM		0x05
+ #define SPDMEM_DDR3_MINI_UDIMM		0x06
+ 
  static const uint8_t ddr2_cycle_tenths[] = {
  	0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 25, 33, 66, 75, 0, 0
  };
*************** struct spdmem_softc {
*** 187,195 ****
  	i2c_tag_t	sc_tag;
  	i2c_addr_t	sc_addr;
  	struct spdmem	sc_spd_data;
- 	char		sc_type[SPDMEM_TYPE_MAXLEN];
  };
  
  int		 spdmem_match(struct device *, void *, void *);
  void		 spdmem_attach(struct device *, struct device *, void *);
  uint8_t		 spdmem_read(struct spdmem_softc *, uint8_t);
--- 222,230 ----
  	i2c_tag_t	sc_tag;
  	i2c_addr_t	sc_addr;
  	struct spdmem	sc_spd_data;
  };
  
+ uint16_t	 spdmem_crc16(struct spdmem_softc *, int);
  int		 spdmem_match(struct device *, void *, void *);
  void		 spdmem_attach(struct device *, struct device *, void *);
  uint8_t		 spdmem_read(struct spdmem_softc *, uint8_t);
*************** void		 spdmem_sdram_decode(struct spdmem_softc *, stru
*** 197,202 ****
--- 232,238 ----
  void		 spdmem_rdr_decode(struct spdmem_softc *, struct spdmem *);
  void		 spdmem_ddr_decode(struct spdmem_softc *, struct spdmem *);
  void		 spdmem_ddr2_decode(struct spdmem_softc *, struct spdmem *);
+ void		 spdmem_ddr3_decode(struct spdmem_softc *, struct spdmem *);
  
  struct cfattach spdmem_ca = {
  	sizeof(struct spdmem_softc), spdmem_match, spdmem_attach
*************** static const char *spdmem_basic_types[] = {
*** 218,225 ****
  	"DDR SGRAM",
  	"DDR SDRAM",
  	"DDR2 SDRAM",
! 	"DDR2 SDRAM FB",
! 	"DDR2 SDRAM FB Probe"
  };
  
  static const char *spdmem_superset_types[] = {
--- 254,262 ----
  	"DDR SGRAM",
  	"DDR SDRAM",
  	"DDR2 SDRAM",
! 	"DDR2 SDRAM FB-DIMM",
! 	"DDR2 SDRAM FB-DIMM Probe",
! 	"DDR3 SDRAM"
  };
  
  static const char *spdmem_superset_types[] = {
*************** static const char *spdmem_parity_types[] = {
*** 241,253 ****
  	"cmd/addr/data parity, data ECC"
  };
  
  int
  spdmem_match(struct device *parent, void *match, void *aux)
  {
  	struct i2c_attach_args *ia = aux;
! 	
  	if (strcmp(ia->ia_name, "spd") == 0)
  		return (1);
  	return (0);
  }
  
--- 278,369 ----
  	"cmd/addr/data parity, data ECC"
  };
  
+ /* CRC functions used for certain memory types */
+ uint16_t
+ spdmem_crc16(struct spdmem_softc *sc, int count)
+ {
+ 	uint16_t crc;
+ 	int i, j;
+ 	uint8_t val;
+ 	crc = 0;
+ 	for (j = 0; j <= count; j++) {
+ 		val = spdmem_read(sc, j);
+ 		crc = crc ^ val << 8;
+ 		for (i = 0; i < 8; ++i)
+ 			if (crc & 0x8000)
+ 				crc = crc << 1 ^ 0x1021;
+ 			else
+ 				crc = crc << 1;
+ 	}
+ 	return (crc & 0xFFFF);
+ }
+ 
  int
  spdmem_match(struct device *parent, void *match, void *aux)
  {
  	struct i2c_attach_args *ia = aux;
! 	struct spdmem_softc sc;
! 	uint8_t i, val, type;
! 	int cksum = 0;
! 	int spd_len, spd_crc_cover;
! 	uint16_t crc_calc, crc_spd;
! 
! 	/* clever attachments like openfirmware informed macppc */	
  	if (strcmp(ia->ia_name, "spd") == 0)
  		return (1);
+ 
+ 	/* dumb, need sanity checks */
+ 	if (strcmp(ia->ia_name, "eeprom") != 0)
+ 		return (0);
+ 
+ 	sc.sc_tag = ia->ia_tag;
+ 	sc.sc_addr = ia->ia_addr;
+ 
+ 	type = spdmem_read(&sc, 2);
+ 	/* For older memory types, validate the checksum over 1st 63 bytes */
+ 	if (type <= SPDMEM_MEMTYPE_DDR2SDRAM) {
+ 		for (i = 0; i < 63; i++)
+ 			cksum += spdmem_read(&sc, i);
+ 
+ 		val = spdmem_read(&sc, 63);
+ 
+ 		if (cksum == 0 || (cksum & 0xff) != val) {
+ 			return 0;
+ 		} else
+ 			return 1;
+ 	}
+ 
+ 	/* For DDR3 and FBDIMM, verify the CRC */
+ 	else if (type <= SPDMEM_MEMTYPE_DDR3SDRAM) {
+ 		spd_len = spdmem_read(&sc, 0);
+ 		if (spd_len && SPDMEM_SPDCRC_116)
+ 			spd_crc_cover = 116;
+ 		else
+ 			spd_crc_cover = 125;
+ 		switch (spd_len & SPDMEM_SPDLEN_MASK) {
+ 		case SPDMEM_SPDLEN_128:
+ 			spd_len = 128;
+ 			break;
+ 		case SPDMEM_SPDLEN_176:
+ 			spd_len = 176;
+ 			break;
+ 		case SPDMEM_SPDLEN_256:
+ 			spd_len = 256;
+ 			break;
+ 		default:
+ 			return 0;
+ 		}
+ 		if (spd_crc_cover > spd_len)
+ 			return 0;
+ 		crc_calc = spdmem_crc16(&sc, spd_crc_cover);
+ 		crc_spd = spdmem_read(&sc, 127) << 8;
+ 		crc_spd |= spdmem_read(&sc, 126);
+ 		if (crc_calc != crc_spd) {
+ 			return 0;
+ 		}
+ 		return 1;
+ 	}
+ 
  	return (0);
  }
  
*************** spdmem_sdram_decode(struct spdmem_softc *sc, struct sp
*** 280,286 ****
  	}
  
  	printf(" %s", type);
- 	strlcpy(sc->sc_type, type, SPDMEM_TYPE_MAXLEN);
  
  	if (s->sm_data[SPDMEM_DDR_MOD_ATTRIB] & SPDMEM_DDR_ATTRIB_REG)
  		printf(" registered");
--- 396,401 ----
*************** spdmem_ddr_decode(struct spdmem_softc *sc, struct spdm
*** 378,384 ****
  	}
  
  	printf(" %s", type);
- 	strlcpy(sc->sc_type, type, SPDMEM_TYPE_MAXLEN);
  
  	if (s->sm_data[SPDMEM_DDR_MOD_ATTRIB] & SPDMEM_DDR_ATTRIB_REG)
  		printf(" registered");
--- 493,498 ----
*************** spdmem_ddr2_decode(struct spdmem_softc *sc, struct spd
*** 446,452 ****
  	}
  
  	printf(" %s", type);
- 	strlcpy(sc->sc_type, type, SPDMEM_TYPE_MAXLEN);
  
  	if (s->sm_data[SPDMEM_DDR2_DIMMTYPE] & SPDMEM_DDR2_TYPE_REGMASK)
  		printf(" registered");
--- 560,565 ----
*************** spdmem_ddr2_decode(struct spdmem_softc *sc, struct spd
*** 501,506 ****
--- 614,689 ----
  }
  
  void
+ spdmem_ddr3_decode(struct spdmem_softc *sc, struct spdmem *s)
+ {
+ 	const char *type;
+ 	int dimm_size, cycle_time, d_clk, p_clk, bits;
+ 	uint8_t mtype, capacity, dividend, divisor;
+ 
+ 	type = spdmem_basic_types[s->sm_type];
+ 
+ 	capacity = s->sm_data[SPDMEM_DDR3_DENSITY] &
+ 	    SPDMEM_DDR3_DENSITY_CAPMASK;
+ 	/* capacity in MB is 2^(x+8) which we can get by shifting */
+ 	dimm_size = 2 << (capacity + 7);
+ 
+ 	if (dimm_size < 1024)
+ 		printf(" %dMB", dimm_size);
+ 	else
+ 		printf(" %dGB", dimm_size / 1024);
+ 
+ 	printf(" %s", type);
+ 
+ 	mtype = s->sm_data[SPDMEM_DDR3_MODTYPE];
+ 	if (mtype == SPDMEM_DDR3_RDIMM || mtype == SPDMEM_DDR3_MINI_RDIMM)
+ 		printf(" registered");
+ 
+ 	if (s->sm_data[SPDMEM_DDR3_DATAWIDTH] & SPDMEM_DDR3_DATAWIDTH_ECCMASK) 
+ 		printf(" ECC");
+ 
+ 	dividend = s->sm_data[SPDMEM_DDR3_MTB_DIVIDEND];
+ 	divisor = s->sm_data[SPDMEM_DDR3_MTB_DIVISOR];
+ 	cycle_time = (1000 * dividend +  (divisor / 2)) / divisor;
+ 	cycle_time *= s->sm_data[SPDMEM_DDR3_TCKMIN];
+ 
+ 	if (cycle_time != 0) {
+ 		/*
+ 		 * cycle time is scaled by a factor of 1000 to avoid using
+ 		 * floating point.  Calculate memory speed as the number
+ 		 * of cycles per microsecond.
+ 		 * DDR3 uses a dual-pumped clock
+ 		 */
+ 		d_clk = 1000 * 1000;
+ 		d_clk *= 2;
+ 		bits = 1 << ((s->sm_data[SPDMEM_DDR3_DATAWIDTH] &
+ 		    SPDMEM_DDR3_DATAWIDTH_PRIMASK) + 3);
+ 		/*
+ 		 * Calculate p_clk first, since for DDR3 we need maximum
+ 		 * significance.  DDR3 rating is not rounded to a multiple
+ 		 * of 100.  This results in cycle_time of 1.5ns displayed
+ 		 * as p_clk PC3-10666 (d_clk DDR3-1333)
+ 		 */
+ 		p_clk = (d_clk * bits) / 8 / cycle_time;
+ 		p_clk -= (p_clk % 100);
+ 		d_clk = ((d_clk + cycle_time / 2) ) / cycle_time;
+ 		printf(" PC3-%d", p_clk);
+ 	}
+ 
+ 	switch (s->sm_data[SPDMEM_DDR3_MODTYPE]) {
+ 	case SPDMEM_DDR3_SODIMM:
+ 		printf(" SO-DIMM");
+ 		break;
+ 	case SPDMEM_DDR3_MICRO_DIMM:
+ 		printf(" Micro-DIMM");
+ 		break;
+ 	case SPDMEM_DDR3_MINI_RDIMM:
+ 	case SPDMEM_DDR3_MINI_UDIMM:
+ 		printf(" Mini-DIMM");
+ 		break;
+ 	}
+ }
+ 
+ void
  spdmem_attach(struct device *parent, struct device *self, void *aux)
  {
  	struct spdmem_softc *sc = (struct spdmem_softc *)self;
*************** spdmem_attach(struct device *parent, struct device *se
*** 537,542 ****
--- 720,728 ----
  			break;
  		case SPDMEM_MEMTYPE_DDR2SDRAM:
  			spdmem_ddr2_decode(sc, s);
+ 			break;
+ 		case SPDMEM_MEMTYPE_DDR3SDRAM:
+ 			spdmem_ddr3_decode(sc, s);
  			break;
  		case SPDMEM_MEMTYPE_NONE:
  			printf(" no EEPROM found");
Index: src/sys/dev/ic/wdc.c
===================================================================
RCS file: /cvs/src/sys/dev/ic/wdc.c,v
retrieving revision 1.101
retrieving revision 1.102
diff -c -p -r1.101 -r1.102
*** src/sys/dev/ic/wdc.c	21 Jan 2009 21:54:00 -0000	1.101
--- src/sys/dev/ic/wdc.c	7 Feb 2009 08:07:28 -0000	1.102
***************
*** 1,4 ****
! /*	$OpenBSD: wdc.c,v 1.101 2009/01/21 21:54:00 grange Exp $	*/
  /*	$NetBSD: wdc.c,v 1.68 1999/06/23 19:00:17 bouyer Exp $	*/
  /*
   * Copyright (c) 1998, 2001 Manuel Bouyer.  All rights reserved.
--- 1,4 ----
! /*	$OpenBSD: wdc.c,v 1.102 2009/02/07 08:07:28 grange Exp $	*/
  /*	$NetBSD: wdc.c,v 1.68 1999/06/23 19:00:17 bouyer Exp $	*/
  /*
   * Copyright (c) 1998, 2001 Manuel Bouyer.  All rights reserved.
*************** wdcattach(chp)
*** 754,762 ****
  	struct channel_softc *chp;
  {
  	int channel_flags, ctrl_flags, i;
- #ifndef __OpenBSD__
- 	int error;
- #endif
  	struct ata_atapi_attach aa_link;
  	static int inited = 0;
  #ifdef WDCDEBUG
--- 754,759 ----
*************** wdcattach(chp)
*** 771,796 ****
  
  	timeout_set(&chp->ch_timo, wdctimeout, chp);
  
- #ifndef __OpenBSD__
- 	if ((error = wdc_addref(chp)) != 0) {
- 		printf("%s: unable to enable controller\n",
- 		    chp->wdc->sc_dev.dv_xname);
- 		return;
- 	}
- #endif /* __OpenBSD__ */
  	if (!chp->_vtbl)
  		chp->_vtbl = &wdc_default_vtbl;
  
  	if (chp->wdc->drv_probe != NULL) {
  		chp->wdc->drv_probe(chp);
  	} else {
! 		if (wdcprobe(chp) == 0) {
  			/* If no drives, abort attach here. */
- #ifndef __OpenBSD__
- 			wdc_delref(chp);
- #endif
  			return;
- 		}
  	}
  
  	/* ATAPI drives need settling time. Give them 250ms */
--- 768,782 ----
  
  	timeout_set(&chp->ch_timo, wdctimeout, chp);
  
  	if (!chp->_vtbl)
  		chp->_vtbl = &wdc_default_vtbl;
  
  	if (chp->wdc->drv_probe != NULL) {
  		chp->wdc->drv_probe(chp);
  	} else {
! 		if (wdcprobe(chp) == 0)
  			/* If no drives, abort attach here. */
  			return;
  	}
  
  	/* ATAPI drives need settling time. Give them 250ms */
*************** wdcattach(chp)
*** 891,900 ****
  		if (chp->ch_drive[i].drive_name[0] == 0)
  			chp->ch_drive[i].drive_flags = 0;
  	}
- 
- #ifndef __OpenBSD__
- 	wdc_delref(chp);
- #endif
  
  exit:
  #ifdef WDCDEBUG
--- 877,882 ----
Index: src/sys/dev/pci/azalia.c
===================================================================
RCS file: /cvs/src/sys/dev/pci/azalia.c,v
retrieving revision 1.63
retrieving revision 1.64
diff -c -p -r1.63 -r1.64
*** src/sys/dev/pci/azalia.c	31 Oct 2008 06:40:12 -0000	1.63
--- src/sys/dev/pci/azalia.c	31 Oct 2008 06:44:20 -0000	1.64
***************
*** 1,4 ****
! /*	$OpenBSD: azalia.c,v 1.63 2008/10/31 06:40:12 jakemsr Exp $	*/
  /*	$NetBSD: azalia.c,v 1.20 2006/05/07 08:31:44 kent Exp $	*/
  
  /*-
--- 1,4 ----
! /*	$OpenBSD: azalia.c,v 1.64 2008/10/31 06:44:20 jakemsr Exp $	*/
  /*	$NetBSD: azalia.c,v 1.20 2006/05/07 08:31:44 kent Exp $	*/
  
  /*-
*************** azalia_codec_construct_format(codec_t *this, int newda
*** 1330,1336 ****
  {
  	const convgroup_t *group;
  	uint32_t bits_rates;
! 	int pvariation, rvariation;
  	int nbits, c, chan, i, err;
  	nid_t nid;
  
--- 1330,1336 ----
  {
  	const convgroup_t *group;
  	uint32_t bits_rates;
! 	int variation;
  	int nbits, c, chan, i, err;
  	nid_t nid;
  
*************** azalia_codec_construct_format(codec_t *this, int newda
*** 1353,1359 ****
  		    XNAME(this->az), __FILE__, __LINE__, bits_rates);
  		return -1;
  	}
! 	pvariation = group->nconv * nbits;
  
  	this->adcs.cur = newadc;
  	group = &this->adcs.groups[this->adcs.cur];
--- 1353,1359 ----
  		    XNAME(this->az), __FILE__, __LINE__, bits_rates);
  		return -1;
  	}
! 	variation = group->nconv * nbits;
  
  	this->adcs.cur = newadc;
  	group = &this->adcs.groups[this->adcs.cur];
*************** azalia_codec_construct_format(codec_t *this, int newda
*** 1374,1386 ****
  		    XNAME(this->az), __FILE__, __LINE__, bits_rates);
  		return -1;
  	}
! 	rvariation = group->nconv * nbits;
  
  	if (this->formats != NULL)
  		free(this->formats, M_DEVBUF);
  	this->nformats = 0;
! 	this->formats = malloc(sizeof(struct audio_format) *
! 	    (pvariation + rvariation), M_DEVBUF, M_NOWAIT | M_ZERO);
  	if (this->formats == NULL) {
  		printf("%s: out of memory in %s\n",
  		    XNAME(this->az), __func__);
--- 1374,1386 ----
  		    XNAME(this->az), __FILE__, __LINE__, bits_rates);
  		return -1;
  	}
! 	variation += group->nconv * nbits;
  
  	if (this->formats != NULL)
  		free(this->formats, M_DEVBUF);
  	this->nformats = 0;
! 	this->formats = malloc(sizeof(struct audio_format) * variation,
! 	    M_DEVBUF, M_NOWAIT | M_ZERO);
  	if (this->formats == NULL) {
  		printf("%s: out of memory in %s\n",
  		    XNAME(this->az), __func__);
*************** azalia_codec_construct_format(codec_t *this, int newda
*** 1389,1411 ****
  
  	/* register formats for playback */
  	group = &this->dacs.groups[this->dacs.cur];
- 	nid = group->conv[0];
- 	chan = 0;
- 	bits_rates = this->w[nid].d.audio.bits_rates;
  	for (c = 0; c < group->nconv; c++) {
! 		for (chan = 0, i = 0; i <= c; i++)
! 			chan += WIDGET_CHANNELS(&this->w[group->conv[c]]);
  		azalia_codec_add_bits(this, chan, bits_rates, AUMODE_PLAY);
  	}
  
  	/* register formats for recording */
  	group = &this->adcs.groups[this->adcs.cur];
- 	nid = group->conv[0];
- 	chan = 0;
- 	bits_rates = this->w[nid].d.audio.bits_rates;
  	for (c = 0; c < group->nconv; c++) {
! 		for (chan = 0, i = 0; i <= c; i++)
! 			chan += WIDGET_CHANNELS(&this->w[group->conv[c]]);
  		azalia_codec_add_bits(this, chan, bits_rates, AUMODE_RECORD);
  	}
  
--- 1389,1415 ----
  
  	/* register formats for playback */
  	group = &this->dacs.groups[this->dacs.cur];
  	for (c = 0; c < group->nconv; c++) {
! 		chan = 0;
! 		bits_rates = ~0;
! 		for (i = 0; i <= c; i++) {
! 			nid = group->conv[i];
! 			chan += WIDGET_CHANNELS(&this->w[nid]);
! 			bits_rates &= this->w[nid].d.audio.bits_rates;
! 		}
  		azalia_codec_add_bits(this, chan, bits_rates, AUMODE_PLAY);
  	}
  
  	/* register formats for recording */
  	group = &this->adcs.groups[this->adcs.cur];
  	for (c = 0; c < group->nconv; c++) {
! 		chan = 0;
! 		bits_rates = ~0;
! 		for (i = 0; i <= c; i++) {
! 			nid = group->conv[i];
! 			chan += WIDGET_CHANNELS(&this->w[nid]);
! 			bits_rates &= this->w[nid].d.audio.bits_rates;
! 		}
  		azalia_codec_add_bits(this, chan, bits_rates, AUMODE_RECORD);
  	}
  
*************** azalia_codec_add_format(codec_t *this, int chan, int v
*** 1465,1470 ****
--- 1469,1475 ----
  	default:
  		f->channel_mask = 0;
  	}
+ 	f->frequency_type = 0;
  	if (rates & COP_PCM_R80)
  		f->frequency[f->frequency_type++] = 8000;
  	if (rates & COP_PCM_R110)
*************** azalia_widget_init_audio(widget_t *this, const codec_t
*** 1743,1749 ****
  			return err;
  		this->d.audio.encodings = result;
  		if (result == 0) { /* quirk for CMI9880.
! 				    * This must not occuur usually... */
  			this->d.audio.encodings =
  			    codec->w[codec->audiofunc].d.audio.encodings;
  			this->d.audio.bits_rates =
--- 1748,1754 ----
  			return err;
  		this->d.audio.encodings = result;
  		if (result == 0) { /* quirk for CMI9880.
! 				    * This must not occur usually... */
  			this->d.audio.encodings =
  			    codec->w[codec->audiofunc].d.audio.encodings;
  			this->d.audio.bits_rates =
Index: src/sys/dev/pci/pciide.c
===================================================================
RCS file: /cvs/src/sys/dev/pci/pciide.c,v
retrieving revision 1.291
retrieving revision 1.292
diff -c -p -r1.291 -r1.292
*** src/sys/dev/pci/pciide.c	4 Jan 2009 10:22:01 -0000	1.291
--- src/sys/dev/pci/pciide.c	4 Jan 2009 10:37:40 -0000	1.292
***************
*** 1,4 ****
! /*	$OpenBSD: pciide.c,v 1.291 2009/01/04 10:22:01 jsg Exp $	*/
  /*	$NetBSD: pciide.c,v 1.127 2001/08/03 01:31:08 tsutsui Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: pciide.c,v 1.292 2009/01/04 10:37:40 jsg Exp $	*/
  /*	$NetBSD: pciide.c,v 1.127 2001/08/03 01:31:08 tsutsui Exp $	*/
  
  /*
*************** const struct pciide_product_desc pciide_sis_products[]
*** 671,677 ****
--- 671,687 ----
  	}
  };
  
+ /*
+  * The National/AMD CS5535 requires MSRs to set DMA/PIO modes so it
+  * has been banished to the MD i386 pciide_machdep
+  */
  const struct pciide_product_desc pciide_natsemi_products[] =  {
+ #ifdef __i386__
+ 	{ PCI_PRODUCT_NS_CS5535_IDE,	/* National/AMD CS5535 IDE */
+ 	  0,
+ 	  gcsc_chip_map
+ 	},
+ #endif
  	{ PCI_PRODUCT_NS_PC87415,	/* National Semi PC87415 IDE */
  	  0,
  	  natsemi_chip_map
Index: src/sys/dev/sbus/if_gem_sbus.c
===================================================================
RCS file: /cvs/src/sys/dev/sbus/if_gem_sbus.c,v
retrieving revision 1.4
retrieving revision 1.5
diff -c -p -r1.4 -r1.5
*** src/sys/dev/sbus/if_gem_sbus.c	26 Jun 2008 05:42:18 -0000	1.4
--- src/sys/dev/sbus/if_gem_sbus.c	7 Nov 2008 17:44:14 -0000	1.5
***************
*** 1,4 ****
! /*	$OpenBSD: if_gem_sbus.c,v 1.4 2008/06/26 05:42:18 ray Exp $	*/
  /*	$NetBSD: if_gem_sbus.c,v 1.1 2006/11/24 13:23:32 martin Exp $	*/
  
  /*-
--- 1,4 ----
! /*	$OpenBSD: if_gem_sbus.c,v 1.5 2008/11/07 17:44:14 brad Exp $	*/
  /*	$NetBSD: if_gem_sbus.c,v 1.1 2006/11/24 13:23:32 martin Exp $	*/
  
  /*-
*************** gemattach_sbus(struct device *parent, struct device *s
*** 105,110 ****
--- 105,112 ----
  		return;
  	}
  
+ 	sc->sc_variant = GEM_SUN_GEM;
+ 
  	/*
  	 * Map two register banks:
  	 *
*************** gemattach_sbus(struct device *parent, struct device *s
*** 134,141 ****
  	/*
  	 * SBUS config
  	 */
  	bus_space_write_4(sa->sa_bustag, sc->sc_h2, GEM_SBUS_CONFIG,
! 	    GEM_SBUS_CFG_PARITY|GEM_SBUS_CFG_BMODE64);
  
  	/* Establish interrupt handler */
  	if (sa->sa_nintr != 0)
--- 136,145 ----
  	/*
  	 * SBUS config
  	 */
+ 	(void) bus_space_read_4(sa->sa_bustag, sc->sc_h2, GEM_SBUS_RESET);
+ 	delay(100);
  	bus_space_write_4(sa->sa_bustag, sc->sc_h2, GEM_SBUS_CONFIG,
! 	    GEM_SBUS_CFG_BSIZE128|GEM_SBUS_CFG_PARITY|GEM_SBUS_CFG_BMODE64);
  
  	/* Establish interrupt handler */
  	if (sa->sa_nintr != 0)
Index: src/sys/dev/sbus/magma.c
===================================================================
RCS file: /cvs/src/sys/dev/sbus/magma.c,v
retrieving revision 1.15
retrieving revision 1.16
diff -c -p -r1.15 -r1.16
*** src/sys/dev/sbus/magma.c	9 Jul 2005 22:23:15 -0000	1.15
--- src/sys/dev/sbus/magma.c	29 Nov 2008 01:55:06 -0000	1.16
***************
*** 1,7 ****
! /*	$OpenBSD: magma.c,v 1.15 2005/07/09 22:23:15 miod Exp $	*/
! /*
!  * magma.c
!  *
   * Copyright (c) 1998 Iain Hibbert
   * All rights reserved.
   *
--- 1,6 ----
! /*	$OpenBSD: magma.c,v 1.16 2008/11/29 01:55:06 ray Exp $	*/
! 
! /*-
   * Copyright (c) 1998 Iain Hibbert
   * All rights reserved.
   *
***************
*** 13,23 ****
   * 2. Redistributions in binary form must reproduce the above copyright
   *    notice, this list of conditions and the following disclaimer in the
   *    documentation and/or other materials provided with the distribution.
-  * 3. All advertising materials mentioning features or use of this software
-  *    must display the following acknowledgement:
-  *	This product includes software developed by Iain Hibbert
-  * 4. The name of the author may not be used to endorse or promote products
-  *    derived from this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
--- 12,17 ----
***************
*** 29,37 ****
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-  *
- #define MAGMA_DEBUG
   */
  
  /*
   * Driver for Magma SBus Serial/Parallel cards using the Cirrus Logic
--- 23,31 ----
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */
+ 
+ #define MAGMA_DEBUG
  
  /*
   * Driver for Magma SBus Serial/Parallel cards using the Cirrus Logic
Index: src/sys/dev/sbus/stp4020.c
===================================================================
RCS file: /cvs/src/sys/dev/sbus/stp4020.c,v
retrieving revision 1.15
retrieving revision 1.16
diff -c -p -r1.15 -r1.16
*** src/sys/dev/sbus/stp4020.c	26 Jun 2008 05:42:18 -0000	1.15
--- src/sys/dev/sbus/stp4020.c	10 Apr 2009 20:54:58 -0000	1.16
***************
*** 1,4 ****
! /*	$OpenBSD: stp4020.c,v 1.15 2008/06/26 05:42:18 ray Exp $	*/
  /*	$NetBSD: stp4020.c,v 1.23 2002/06/01 23:51:03 lukem Exp $	*/
  
  /*-
--- 1,4 ----
! /*	$OpenBSD: stp4020.c,v 1.16 2009/04/10 20:54:58 miod Exp $	*/
  /*	$NetBSD: stp4020.c,v 1.23 2002/06/01 23:51:03 lukem Exp $	*/
  
  /*-
***************
*** 49,54 ****
--- 49,55 ----
  #include <dev/pcmcia/pcmciachip.h>
  
  #include <machine/bus.h>
+ #include <machine/intr.h>
  
  #include <dev/sbus/stp4020reg.h>
  #include <dev/sbus/stp4020var.h>
*************** int stp4020_debug = 0;
*** 72,77 ****
--- 73,79 ----
  int	stp4020print(void *, const char *);
  void	stp4020_map_window(struct stp4020_socket *, int, int);
  void	stp4020_calc_speed(int, int, int *, int *);
+ void	stp4020_intr_dispatch(void *);
  
  struct	cfdriver stp_cd = {
  	NULL, "stp", DV_DULL
*************** stp4020_attach_socket(h, speed)
*** 232,237 ****
--- 234,245 ----
  	struct pcmciabus_attach_args paa;
  	int v;
  
+ 	/* no interrupt handlers yet */
+ 	h->intrhandler = NULL;
+ 	h->intrarg = NULL;
+ 	h->softint = NULL;
+ 	h->int_enable = h->int_disable = 0;
+ 
  	/* Map all three windows */
  	stp4020_map_window(h, STP_WIN_ATTR, speed);
  	stp4020_map_window(h, STP_WIN_MEM, speed);
*************** stp4020_queue_event(sc, sock)
*** 370,382 ****
--- 378,412 ----
  	wakeup(&sc->events);
  }
  
+ /*
+  * Software interrupt called to invoke the real driver interrupt handler.
+  */
+ void
+ stp4020_intr_dispatch(void *arg)
+ {
+ 	struct stp4020_socket *h = (struct stp4020_socket *)arg;
+ 	int s;
+ 
+ 	/* invoke driver handler */
+ 	h->intrhandler(h->intrarg);
+ 
+ 	/* enable SBUS interrupts for PCMCIA interrupts again */
+ 	s = splhigh();
+ 	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, h->int_enable);
+ 	splx(s);
+ }
+ 
  int
  stp4020_statintr(arg)
  	void *arg;
  {
  	struct stp4020_softc *sc = arg;
  	int i, sense, r = 0;
+ 	int s;
  
+ 	/* protect hardware access against soft interrupts */
+ 	s = splhigh();
+ 
  	/*
  	 * Check each socket for pending requests.
  	 */
*************** stp4020_statintr(arg)
*** 467,472 ****
--- 497,504 ----
  			r = 1;
  	}
  
+ 	splx(s);
+ 
  	return (r);
  }
  
*************** stp4020_iointr(arg)
*** 476,482 ****
--- 508,518 ----
  {
  	struct stp4020_softc *sc = arg;
  	int i, r = 0;
+ 	int s;
  
+ 	/* protect hardware access against soft interrupts */
+ 	s = splhigh();
+ 
  	/*
  	 * Check each socket for pending requests.
  	 */
*************** stp4020_iointr(arg)
*** 502,521 ****
  				continue;
  			}
  			/* Call card handler, if any */
! 			if (h->intrhandler != NULL) {
  				/*
! 				 * We ought to be at an higher ipl level
! 				 * than the callback, since the first
! 				 * interrupt of this device is usually
! 				 * higher than IPL_CLOCK.
  				 */
! 				splassert(h->ipl);
! 				(*h->intrhandler)(h->intrarg);
  			}
  		}
  
  	}
  
  	return (r);
  }
  
--- 538,559 ----
  				continue;
  			}
  			/* Call card handler, if any */
! 			if (h->softint != NULL) {
! 				softintr_schedule(h->softint);
! 
  				/*
! 				 * Disable this sbus interrupt, until the
! 				 * softintr handler had a chance to run.
  				 */
! 				stp4020_wr_sockctl(h, STP4020_ICR0_IDX,
! 				    h->int_disable);
  			}
  		}
  
  	}
  
+ 	splx(s);
+ 
  	return (r);
  }
  
*************** stp4020_chip_socket_enable(pch)
*** 763,774 ****
--- 801,815 ----
  		v &= ~(STP4020_ICR0_IOILVL | STP4020_ICR0_IFTYPE);
  		v |= STP4020_ICR0_IFTYPE_IO | STP4020_ICR0_IOIE |
  		    STP4020_ICR0_IOILVL_SB0 | STP4020_ICR0_SPKREN;
+ 		h->int_enable = v;
+ 		h->int_disable = v & ~STP4020_ICR0_IOIE;
  		DPRINTF(("%s: configuring card for IO usage\n",
  		    h->sc->sc_dev.dv_xname));
  	} else {
  		v &= ~(STP4020_ICR0_IOILVL | STP4020_ICR0_IFTYPE |
  		    STP4020_ICR0_SPKREN | STP4020_ICR0_IOIE);
  		v |= STP4020_ICR0_IFTYPE_MEM;
+ 		h->int_enable = h->int_disable = v;
  		DPRINTF(("%s: configuring card for MEM ONLY usage\n",
  		    h->sc->sc_dev.dv_xname));
  	}
*************** stp4020_chip_intr_establish(pch, pf, ipl, handler, arg
*** 812,821 ****
  {
  	struct stp4020_socket *h = (struct stp4020_socket *)pch;
  
  	h->intrhandler = handler;
  	h->intrarg = arg;
! 	h->ipl = ipl;
! 	return (h);
  }
  
  void
--- 853,868 ----
  {
  	struct stp4020_socket *h = (struct stp4020_socket *)pch;
  
+ 	/*
+ 	 * Note that this code relies on softintr_establish() to be
+ 	 * used with real, hardware ipl values. All platforms with
+ 	 * SBus support support this.
+ 	 */
  	h->intrhandler = handler;
  	h->intrarg = arg;
! 	h->softint = softintr_establish(ipl, stp4020_intr_dispatch, h);
! 
! 	return h->softint != NULL ? h : NULL;
  }
  
  void
*************** stp4020_chip_intr_disestablish(pch, ih)
*** 825,830 ****
--- 872,881 ----
  {
  	struct stp4020_socket *h = (struct stp4020_socket *)pch;
  
+ 	if (h->softint != NULL) {
+ 		softintr_disestablish(h->softint);
+ 		h->softint = NULL;
+ 	}
  	h->intrhandler = NULL;
  	h->intrarg = NULL;
  }
Index: src/sys/dev/usb/ehci.c
===================================================================
RCS file: /cvs/src/sys/dev/usb/ehci.c,v
retrieving revision 1.96
retrieving revision 1.97
diff -c -p -r1.96 -r1.97
*** src/sys/dev/usb/ehci.c	21 Nov 2008 17:08:42 -0000	1.96
--- src/sys/dev/usb/ehci.c	29 Nov 2008 08:52:03 -0000	1.97
***************
*** 1,4 ****
! /*	$OpenBSD: ehci.c,v 1.96 2008/11/21 17:08:42 deraadt Exp $ */
  /*	$NetBSD: ehci.c,v 1.66 2004/06/30 03:11:56 mycroft Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: ehci.c,v 1.97 2008/11/29 08:52:03 mglocker Exp $ */
  /*	$NetBSD: ehci.c,v 1.66 2004/06/30 03:11:56 mycroft Exp $	*/
  
  /*
*************** ehci_idone(struct ehci_xfer *ex)
*** 876,881 ****
--- 876,884 ----
  
  				status = letoh32(itd->itd.itd_ctl[i]);
  				len = EHCI_ITD_GET_LEN(status);
+ 				if (EHCI_ITD_GET_STATUS(status) != 0)
+ 					len = 0; /*No valid data on error*/
+ 
  				xfer->frlengths[nframes++] = len;
  				actlen += len;
  			}
*************** ehci_device_isoc_start(usbd_xfer_handle xfer)
*** 3806,3816 ****
  			if (page_offs >= dma_buf->block->size)
  				break;
  
! 			int page = DMAADDR(dma_buf, page_offs);
  			page = EHCI_PAGE(page);
  			itd->itd.itd_bufr[j] =
! 			    htole32(EHCI_ITD_SET_BPTR(page) | 
! 			    EHCI_LINK_ITD);
  		}
  
  		/*
--- 3809,3820 ----
  			if (page_offs >= dma_buf->block->size)
  				break;
  
! 			long long page = DMAADDR(dma_buf, page_offs);
  			page = EHCI_PAGE(page);
  			itd->itd.itd_bufr[j] =
! 			    htole32(EHCI_ITD_SET_BPTR(page));
! 			itd->itd.itd_bufr_hi[j] =
! 			    htole32(page >> 32);
  		}
  
  		/*
Index: src/sys/dev/usb/ubt.c
===================================================================
RCS file: /cvs/src/sys/dev/usb/ubt.c,v
retrieving revision 1.13
retrieving revision 1.14
diff -c -p -r1.13 -r1.14
*** src/sys/dev/usb/ubt.c	10 Jul 2008 13:48:54 -0000	1.13
--- src/sys/dev/usb/ubt.c	22 Nov 2008 04:42:58 -0000	1.14
***************
*** 1,5 ****
! /*	$OpenBSD: ubt.c,v 1.13 2008/07/10 13:48:54 mbalmer Exp $	*/
! /*	$NetBSD: ubt.c,v 1.30 2007/12/16 19:01:37 christos Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
--- 1,5 ----
! /*	$OpenBSD: ubt.c,v 1.14 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: ubt.c,v 1.35 2008/07/28 14:19:26 drochner Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
*************** ubt_attach(struct device *parent, struct device *self,
*** 449,454 ****
--- 449,455 ----
  			   &sc->sc_dev);
  
  	sc->sc_ok = 1;
+ 	/* XXX pmf_device_deregister in NetBSD (power hook) */
  }
  
  int
*************** ubt_detach(struct device *self, int flags)
*** 459,464 ****
--- 460,467 ----
  
  	DPRINTF("sc=%p flags=%d\n", sc, flags);
  
+ 	/* XXX pmf_device_deregister in NetBSD (power hook) */
+ 
  	sc->sc_dying = 1;
  
  	if (!sc->sc_ok)
*************** ubt_abortdealloc(struct ubt_softc *sc)
*** 718,724 ****
   *
   * Bluetooth Unit/USB callbacks
   *
-  * All of this will be called at the IPL_ we specified above
   */
  int
  ubt_enable(struct device *self)
--- 721,726 ----
Index: src/sys/kern/exec_elf.c
===================================================================
RCS file: /cvs/src/sys/kern/exec_elf.c,v
retrieving revision 1.67
retrieving revision 1.68
diff -c -p -r1.67 -r1.68
*** src/sys/kern/exec_elf.c	10 Nov 2008 03:56:16 -0000	1.67
--- src/sys/kern/exec_elf.c	5 Mar 2009 19:52:24 -0000	1.68
***************
*** 1,4 ****
! /*	$OpenBSD: exec_elf.c,v 1.67 2008/11/10 03:56:16 deraadt Exp $	*/
  
  /*
   * Copyright (c) 1996 Per Fogelstrom
--- 1,4 ----
! /*	$OpenBSD: exec_elf.c,v 1.68 2009/03/05 19:52:24 kettenis Exp $	*/
  
  /*
   * Copyright (c) 1996 Per Fogelstrom
***************
*** 31,36 ****
--- 31,71 ----
   *
   */
  
+ /*
+  * Copyright (c) 2001 Wasabi Systems, Inc.
+  * All rights reserved.
+  *
+  * Written by Jason R. Thorpe for Wasabi Systems, Inc.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. All advertising materials mentioning features or use of this software
+  *    must display the following acknowledgement:
+  *	This product includes software developed for the NetBSD Project by
+  *	Wasabi Systems, Inc.
+  * 4. The name of Wasabi Systems, Inc. may not be used to endorse
+  *    or promote products derived from this software without specific prior
+  *    written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
+  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
+  * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
+  * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
+  * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
+  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
+  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
+  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
+  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
+  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
+  * POSSIBILITY OF SUCH DAMAGE.
+  */
+ 
  #include <sys/param.h>
  #include <sys/systm.h>
  #include <sys/kernel.h>
***************
*** 40,49 ****
--- 75,86 ----
  #include <sys/mount.h>
  #include <sys/namei.h>
  #include <sys/vnode.h>
+ #include <sys/core.h>
  #include <sys/exec.h>
  #include <sys/exec_elf.h>
  #include <sys/exec_olf.h>
  #include <sys/file.h>
+ #include <sys/ptrace.h>
  #include <sys/syscall.h>
  #include <sys/signalvar.h>
  #include <sys/stat.h>
*************** int ELFNAME(check_header)(Elf_Ehdr *);
*** 90,95 ****
--- 127,133 ----
  int ELFNAME(read_from)(struct proc *, struct vnode *, u_long, caddr_t, int);
  void ELFNAME(load_psection)(struct exec_vmcmd_set *, struct vnode *,
  	Elf_Phdr *, Elf_Addr *, Elf_Addr *, int *, int);
+ int ELFNAMEEND(coredump)(struct proc *, void *);
  
  extern char sigcode[], esigcode[];
  #ifdef SYSCALL_DEBUG
*************** struct emul ELFNAMEEND(emul) = {
*** 125,130 ****
--- 163,169 ----
  	ELFNAME(copyargs),
  	setregs,
  	ELFNAME2(exec,fixup),
+ 	ELFNAMEEND(coredump),
  	sigcode,
  	esigcode,
  	EMUL_ENABLED | EMUL_NATIVE,
*************** out2:
*** 880,883 ****
--- 919,1357 ----
  out1:
  	free(hph, M_TEMP);
  	return error;
+ }
+ 
+ struct countsegs_state {
+ 	int	npsections;
+ };
+ 
+ int	ELFNAMEEND(coredump_countsegs)(struct proc *, void *,
+ 	    struct uvm_coredump_state *);
+ 
+ struct writesegs_state {
+ 	Elf_Phdr *psections;
+ 	off_t	secoff;
+ };
+ 
+ int	ELFNAMEEND(coredump_writeseghdrs)(struct proc *, void *,
+ 	    struct uvm_coredump_state *);
+ 
+ int	ELFNAMEEND(coredump_notes)(struct proc *, void *, size_t *);
+ int	ELFNAMEEND(coredump_note)(struct proc *, void *, size_t *);
+ int	ELFNAMEEND(coredump_writenote)(struct proc *, void *, Elf_Note *,
+ 	    const char *, void *);
+ 
+ #define	ELFROUNDSIZE	4	/* XXX Should it be sizeof(Elf_Word)? */
+ #define	elfround(x)	roundup((x), ELFROUNDSIZE)
+ 
+ int
+ ELFNAMEEND(coredump)(struct proc *p, void *cookie)
+ {
+ 	Elf_Ehdr ehdr;
+ 	Elf_Phdr phdr, *psections;
+ 	struct countsegs_state cs;
+ 	struct writesegs_state ws;
+ 	off_t notestart, secstart, offset;
+ 	size_t notesize;
+ 	int error, i;
+ 
+ 	psections = NULL;
+ 	/*
+ 	 * We have to make a total of 3 passes across the map:
+ 	 *
+ 	 *	1. Count the number of map entries (the number of
+ 	 *	   PT_LOAD sections).
+ 	 *
+ 	 *	2. Write the P-section headers.
+ 	 *
+ 	 *	3. Write the P-sections.
+ 	 */
+ 
+ 	/* Pass 1: count the entries. */
+ 	cs.npsections = 0;
+ 	error = uvm_coredump_walkmap(p, NULL,
+ 	    ELFNAMEEND(coredump_countsegs), &cs);
+ 	if (error)
+ 		goto out;
+ 
+ 	/* Count the PT_NOTE section. */
+ 	cs.npsections++;
+ 
+ 	/* Get the size of the notes. */
+ 	error = ELFNAMEEND(coredump_notes)(p, NULL, &notesize);
+ 	if (error)
+ 		goto out;
+ 
+ 	memset(&ehdr, 0, sizeof(ehdr));
+ 	memcpy(ehdr.e_ident, ELFMAG, SELFMAG);
+ 	ehdr.e_ident[EI_CLASS] = ELF_TARG_CLASS;
+ 	ehdr.e_ident[EI_DATA] = ELF_TARG_DATA;
+ 	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
+ 	/* XXX Should be the OSABI/ABI version of the executable. */
+ 	ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
+ 	ehdr.e_ident[EI_ABIVERSION] = 0;
+ 	ehdr.e_type = ET_CORE;
+ 	/* XXX This should be the e_machine of the executable. */
+ 	ehdr.e_machine = ELF_TARG_MACH;
+ 	ehdr.e_version = EV_CURRENT;
+ 	ehdr.e_entry = 0;
+ 	ehdr.e_phoff = sizeof(ehdr);
+ 	ehdr.e_shoff = 0;
+ 	ehdr.e_flags = 0;
+ 	ehdr.e_ehsize = sizeof(ehdr);
+ 	ehdr.e_phentsize = sizeof(Elf_Phdr);
+ 	ehdr.e_phnum = cs.npsections;
+ 	ehdr.e_shentsize = 0;
+ 	ehdr.e_shnum = 0;
+ 	ehdr.e_shstrndx = 0;
+ 
+ 	/* Write out the ELF header. */
+ 	error = coredump_write(cookie, UIO_SYSSPACE, &ehdr, sizeof(ehdr));
+ 	if (error)
+ 		goto out;
+ 
+ 	offset = sizeof(ehdr);
+ 
+ 	notestart = offset + sizeof(phdr) * cs.npsections;
+ 	secstart = notestart + notesize;
+ 
+ 	psections = malloc(cs.npsections * sizeof(Elf_Phdr),
+ 	    M_TEMP, M_WAITOK|M_ZERO);
+ 
+ 	/* Pass 2: now write the P-section headers. */
+ 	ws.secoff = secstart;
+ 	ws.psections = psections;
+ 	error = uvm_coredump_walkmap(p, cookie,
+ 	    ELFNAMEEND(coredump_writeseghdrs), &ws);
+ 	if (error)
+ 		goto out;
+ 
+ 	/* Write out the PT_NOTE header. */
+ 	ws.psections->p_type = PT_NOTE;
+ 	ws.psections->p_offset = notestart;
+ 	ws.psections->p_vaddr = 0;
+ 	ws.psections->p_paddr = 0;
+ 	ws.psections->p_filesz = notesize;
+ 	ws.psections->p_memsz = 0;
+ 	ws.psections->p_flags = PF_R;
+ 	ws.psections->p_align = ELFROUNDSIZE;
+ 
+ 	error = coredump_write(cookie, UIO_SYSSPACE, psections,
+ 	    cs.npsections * sizeof(Elf_Phdr));
+ 	if (error)
+ 		goto out;
+ 
+ #ifdef DIAGNOSTIC
+ 	offset += cs.npsections * sizeof(Elf_Phdr);
+ 	if (offset != notestart)
+ 		panic("coredump: offset %lld != notestart %lld",
+ 		    (long long) offset, (long long) notestart);
+ #endif
+ 
+ 	/* Write out the notes. */
+ 	error = ELFNAMEEND(coredump_notes)(p, cookie, &notesize);
+ 	if (error)
+ 		goto out;
+ 
+ #ifdef DIAGNOSTIC
+ 	offset += notesize;
+ 	if (offset != secstart)
+ 		panic("coredump: offset %lld != secstart %lld",
+ 		    (long long) offset, (long long) secstart);
+ #endif
+ 
+ 	/* Pass 3: finally, write the sections themselves. */
+ 	for (i = 0; i < cs.npsections - 1; i++) {
+ 		if (psections[i].p_filesz == 0)
+ 			continue;
+ 
+ #ifdef DIAGNOSTIC
+ 		if (offset != psections[i].p_offset)
+ 			panic("coredump: offset %lld != p_offset[%d] %lld",
+ 			    (long long) offset, i,
+ 			    (long long) psections[i].p_filesz);
+ #endif
+ 
+ 		error = coredump_write(cookie, UIO_USERSPACE,
+ 		    (void *)(vaddr_t)psections[i].p_vaddr,
+ 		    psections[i].p_filesz);
+ 		if (error)
+ 			goto out;
+ 
+ #ifdef DIAGNOSTIC
+ 		offset += psections[i].p_filesz;
+ #endif
+ 	}
+ 
+ out:
+ 	return (error);
+ }
+ 
+ int
+ ELFNAMEEND(coredump_countsegs)(struct proc *p, void *iocookie,
+     struct uvm_coredump_state *us)
+ {
+ 	struct countsegs_state *cs = us->cookie;
+ 
+ 	cs->npsections++;
+ 	return (0);
+ }
+ 
+ int
+ ELFNAMEEND(coredump_writeseghdrs)(struct proc *p, void *iocookie,
+     struct uvm_coredump_state *us)
+ {
+ 	struct writesegs_state *ws = us->cookie;
+ 	Elf_Phdr phdr;
+ 	vsize_t size, realsize;
+ 
+ 	size = us->end - us->start;
+ 	realsize = us->realend - us->start;
+ 
+ 	phdr.p_type = PT_LOAD;
+ 	phdr.p_offset = ws->secoff;
+ 	phdr.p_vaddr = us->start;
+ 	phdr.p_paddr = 0;
+ 	phdr.p_filesz = realsize;
+ 	phdr.p_memsz = size;
+ 	phdr.p_flags = 0;
+ 	if (us->prot & VM_PROT_READ)
+ 		phdr.p_flags |= PF_R;
+ 	if (us->prot & VM_PROT_WRITE)
+ 		phdr.p_flags |= PF_W;
+ 	if (us->prot & VM_PROT_EXECUTE)
+ 		phdr.p_flags |= PF_X;
+ 	phdr.p_align = PAGE_SIZE;
+ 
+ 	ws->secoff += phdr.p_filesz;
+ 	*ws->psections++ = phdr;
+ 
+ 	return (0);
+ }
+ 
+ int
+ ELFNAMEEND(coredump_notes)(struct proc *p, void *iocookie, size_t *sizep)
+ {
+ 	struct ps_strings pss;
+ 	struct iovec iov;
+ 	struct uio uio;
+ 	struct elfcore_procinfo cpi;
+ 	Elf_Note nhdr;
+ #ifdef RTHREADS
+ 	struct proc *q;
+ #endif
+ 	size_t size, notesize;
+ 	int error;
+ 
+ 	size = 0;
+ 
+ 	/* First, write an elfcore_procinfo. */
+ 	notesize = sizeof(nhdr) + elfround(sizeof("OpenBSD")) +
+ 	    elfround(sizeof(cpi));
+ 	if (iocookie) {
+ 		bzero(&cpi, sizeof(cpi));
+ 
+ 		cpi.cpi_version = ELFCORE_PROCINFO_VERSION;
+ 		cpi.cpi_cpisize = sizeof(cpi);
+ 		cpi.cpi_signo = p->p_sigacts->ps_sig;
+ 		cpi.cpi_sigcode = p->p_sigacts->ps_code;
+ 
+ 		cpi.cpi_sigpend = p->p_siglist;
+ 		cpi.cpi_sigmask = p->p_sigmask;
+ 		cpi.cpi_sigignore = p->p_sigignore;
+ 		cpi.cpi_sigcatch = p->p_sigcatch;
+ 
+ 		cpi.cpi_pid = p->p_pid;
+ 		cpi.cpi_ppid = p->p_pptr->p_pid;
+ 		cpi.cpi_pgrp = p->p_pgid;
+ 		cpi.cpi_sid = p->p_session->s_leader->p_pid;
+ 
+ 		cpi.cpi_ruid = p->p_cred->p_ruid;
+ 		cpi.cpi_euid = p->p_ucred->cr_uid;
+ 		cpi.cpi_svuid = p->p_cred->p_svuid;
+ 
+ 		cpi.cpi_rgid = p->p_cred->p_rgid;
+ 		cpi.cpi_egid = p->p_ucred->cr_gid;
+ 		cpi.cpi_svgid = p->p_cred->p_svgid;
+ 
+ 		(void)strlcpy(cpi.cpi_name, p->p_comm, sizeof(cpi.cpi_name));
+ 
+ 		nhdr.namesz = sizeof("OpenBSD");
+ 		nhdr.descsz = sizeof(cpi);
+ 		nhdr.type = NT_OPENBSD_PROCINFO;
+ 
+ 		error = ELFNAMEEND(coredump_writenote)(p, iocookie, &nhdr,
+ 		    "OpenBSD", &cpi);
+ 		if (error)
+ 			return (error);
+ 	}
+ 	size += notesize;
+ 
+ 	/* Second, write an NT_OPENBSD_AUXV note. */
+ 	notesize = sizeof(nhdr) + elfround(sizeof("OpenBSD")) +
+ 	    elfround(p->p_emul->e_arglen * sizeof(char *));
+ 	if (iocookie) {
+ 		iov.iov_base = &pss;
+ 		iov.iov_len = sizeof(pss);
+ 		uio.uio_iov = &iov;
+ 		uio.uio_iovcnt = 1;
+ 		uio.uio_offset = (off_t)PS_STRINGS;
+ 		uio.uio_resid = sizeof(pss);
+ 		uio.uio_segflg = UIO_SYSSPACE;
+ 		uio.uio_rw = UIO_READ;
+ 		uio.uio_procp = NULL;
+ 
+ 		error = uvm_io(&p->p_vmspace->vm_map, &uio, 0);
+ 		if (error)
+ 			return (error);
+ 
+ 		if (pss.ps_envstr == NULL)
+ 			return (EIO);
+ 
+ 		nhdr.namesz = sizeof("OpenBSD");
+ 		nhdr.descsz = p->p_emul->e_arglen * sizeof(char *);
+ 		nhdr.type = NT_OPENBSD_AUXV;
+ 
+ 		error = coredump_write(iocookie, UIO_SYSSPACE,
+ 		    &nhdr, sizeof(nhdr));
+ 		if (error)
+ 			return (error);
+ 
+ 		error = coredump_write(iocookie, UIO_SYSSPACE,
+ 		    "OpenBSD", elfround(nhdr.namesz));
+ 		if (error)
+ 			return (error);
+ 
+ 		error = coredump_write(iocookie, UIO_USERSPACE,
+ 		    pss.ps_envstr + pss.ps_nenvstr + 1, nhdr.descsz);
+ 		if (error)
+ 			return (error);
+ 	}
+ 	size += notesize;
+ 
+ #ifdef PT_WCOOKIE
+ 	notesize = sizeof(nhdr) + elfround(sizeof("OpenBSD")) +
+ 	    elfround(sizeof(register_t));
+ 	if (iocookie) {
+ 		register_t wcookie;
+ 
+ 		nhdr.namesz = sizeof("OpenBSD");
+ 		nhdr.descsz = sizeof(register_t);
+ 		nhdr.type = NT_OPENBSD_WCOOKIE;
+ 
+ 		wcookie = process_get_wcookie(p);
+ 		error = ELFNAMEEND(coredump_writenote)(p, iocookie, &nhdr,
+ 		    "OpenBSD", &wcookie);
+ 		if (error)
+ 			return (error);
+ 	}
+ 	size += notesize;
+ #endif
+ 
+ 	/*
+ 	 * Now write the register info for the thread that caused the
+ 	 * coredump.
+ 	 */
+ 	error = ELFNAMEEND(coredump_note)(p, iocookie, &notesize);
+ 	if (error)
+ 		return (error);
+ 	size += notesize;
+ 
+ #ifdef RTHREADS
+ 	/*
+ 	 * Now, for each thread, write the register info and any other
+ 	 * per-thread notes.  Since we're dumping core, we don't bother
+ 	 * locking.
+ 	 */
+ 	TAILQ_FOREACH(q, &p->p_p->ps_threads, p_thr_link) {
+ 		if (q == p)		/* we've taken care of this thread */
+ 			continue;
+ 		error = ELFNAMEEND(coredump_note)(q, iocookie, &notesize);
+ 		if (error)
+ 			return (error);
+ 		size += notesize;
+ 	}
+ #endif
+ 
+ 	*sizep = size;
+ 	return (0);
+ }
+ 
+ int
+ ELFNAMEEND(coredump_note)(struct proc *p, void *iocookie, size_t *sizep)
+ {
+ 	Elf_Note nhdr;
+ 	int size, notesize, error;
+ 	int namesize;
+ 	char name[64+ELFROUNDSIZE];
+ 	struct reg intreg;
+ #ifdef PT_GETFPREGS
+ 	struct fpreg freg;
+ #endif
+ 
+ 	size = 0;
+ 
+ 	snprintf(name, sizeof(name)-ELFROUNDSIZE, "%s@%d",
+ 	    "OpenBSD", p->p_pid);
+ 	namesize = strlen(name) + 1;
+ 	memset(name + namesize, 0, elfround(namesize) - namesize);
+ 
+ 	notesize = sizeof(nhdr) + elfround(namesize) + elfround(sizeof(intreg));
+ 	if (iocookie) {
+ 		error = process_read_regs(p, &intreg);
+ 		if (error)
+ 			return (error);
+ 
+ 		nhdr.namesz = namesize;
+ 		nhdr.descsz = sizeof(intreg);
+ 		nhdr.type = NT_OPENBSD_REGS;
+ 
+ 		error = ELFNAMEEND(coredump_writenote)(p, iocookie, &nhdr,
+ 		    name, &intreg);
+ 		if (error)
+ 			return (error);
+ 
+ 	}
+ 	size += notesize;
+ 
+ #ifdef PT_GETFPREGS
+ 	notesize = sizeof(nhdr) + elfround(namesize) + elfround(sizeof(freg));
+ 	if (iocookie) {
+ 		error = process_read_fpregs(p, &freg);
+ 		if (error)
+ 			return (error);
+ 
+ 		nhdr.namesz = namesize;
+ 		nhdr.descsz = sizeof(freg);
+ 		nhdr.type = NT_OPENBSD_FPREGS;
+ 
+ 		error = ELFNAMEEND(coredump_writenote)(p, iocookie, &nhdr,
+ 		    name, &freg);
+ 		if (error)
+ 			return (error);
+ 	}
+ 	size += notesize;
+ #endif
+ 
+ 	*sizep = size;
+ 	/* XXX Add hook for machdep per-LWP notes. */
+ 	return (0);
+ }
+ 
+ int
+ ELFNAMEEND(coredump_writenote)(struct proc *p, void *cookie, Elf_Note *nhdr,
+     const char *name, void *data)
+ {
+ 	int error;
+ 
+ 	error = coredump_write(cookie, UIO_SYSSPACE, nhdr, sizeof(*nhdr));
+ 	if (error)
+ 		return error;
+ 
+ 	error = coredump_write(cookie, UIO_SYSSPACE, name,
+ 	    elfround(nhdr->namesz));
+ 	if (error)
+ 		return error;
+ 
+ 	return coredump_write(cookie, UIO_SYSSPACE, data, nhdr->descsz);
  }
Index: src/sys/kern/init_main.c
===================================================================
RCS file: /cvs/src/sys/kern/init_main.c,v
retrieving revision 1.157
retrieving revision 1.158
diff -c -p -r1.157 -r1.158
*** src/sys/kern/init_main.c	13 Feb 2009 19:58:27 -0000	1.157
--- src/sys/kern/init_main.c	5 Mar 2009 19:52:24 -0000	1.158
***************
*** 1,4 ****
! /*	$OpenBSD: init_main.c,v 1.157 2009/02/13 19:58:27 deraadt Exp $	*/
  /*	$NetBSD: init_main.c,v 1.84.4.1 1996/06/02 09:08:06 mrg Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: init_main.c,v 1.158 2009/03/05 19:52:24 kettenis Exp $	*/
  /*	$NetBSD: init_main.c,v 1.84.4.1 1996/06/02 09:08:06 mrg Exp $	*/
  
  /*
***************
*** 39,44 ****
--- 39,45 ----
   */
  
  #include <sys/param.h>
+ #include <sys/core.h>
  #include <sys/filedesc.h>
  #include <sys/file.h>
  #include <sys/errno.h>
*************** struct emul emul_native = {
*** 163,168 ****
--- 164,170 ----
  	copyargs,
  	setregs,
  	NULL,
+ 	coredump_trad,
  	sigcode,
  	esigcode,
  	EMUL_ENABLED | EMUL_NATIVE,
Index: src/sys/kern/kern_exec.c
===================================================================
RCS file: /cvs/src/sys/kern/kern_exec.c,v
retrieving revision 1.108
retrieving revision 1.109
diff -c -p -r1.108 -r1.109
*** src/sys/kern/kern_exec.c	31 Oct 2008 17:17:00 -0000	1.108
--- src/sys/kern/kern_exec.c	4 Jan 2009 00:28:42 -0000	1.109
***************
*** 1,4 ****
! /*	$OpenBSD: kern_exec.c,v 1.108 2008/10/31 17:17:00 deraadt Exp $	*/
  /*	$NetBSD: kern_exec.c,v 1.75 1996/02/09 18:59:28 christos Exp $	*/
  
  /*-
--- 1,4 ----
! /*	$OpenBSD: kern_exec.c,v 1.109 2009/01/04 00:28:42 thib Exp $	*/
  /*	$NetBSD: kern_exec.c,v 1.75 1996/02/09 18:59:28 christos Exp $	*/
  
  /*-
*************** sys_execve(struct proc *p, void *v, register_t *retval
*** 329,335 ****
  			cp = *tmpfap;
  			while (*cp)
  				*dp++ = *cp++;
! 			dp++;
  
  			free(*tmpfap, M_EXEC);
  			tmpfap++; argc++;
--- 329,335 ----
  			cp = *tmpfap;
  			while (*cp)
  				*dp++ = *cp++;
! 			*dp++ = '\0';
  
  			free(*tmpfap, M_EXEC);
  			tmpfap++; argc++;
Index: src/sys/kern/kern_sig.c
===================================================================
RCS file: /cvs/src/sys/kern/kern_sig.c,v
retrieving revision 1.102
retrieving revision 1.103
diff -c -p -r1.102 -r1.103
*** src/sys/kern/kern_sig.c	29 Jan 2009 22:18:06 -0000	1.102
--- src/sys/kern/kern_sig.c	5 Mar 2009 19:52:24 -0000	1.103
***************
*** 1,4 ****
! /*	$OpenBSD: kern_sig.c,v 1.102 2009/01/29 22:18:06 guenther Exp $	*/
  /*	$NetBSD: kern_sig.c,v 1.54 1996/04/22 01:38:32 christos Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: kern_sig.c,v 1.103 2009/03/05 19:52:24 kettenis Exp $	*/
  /*	$NetBSD: kern_sig.c,v 1.54 1996/04/22 01:38:32 christos Exp $	*/
  
  /*
*************** sigexit(struct proc *p, int signum)
*** 1380,1385 ****
--- 1380,1392 ----
  
  int nosuidcoredump = 1;
  
+ struct coredump_iostate {
+ 	struct proc *io_proc;
+ 	struct vnode *io_vp;
+ 	struct ucred *io_cred;
+ 	off_t io_offset;
+ };
+ 
  /*
   * Dump core, into a file named "progname.core", unless the process was
   * setuid/setgid.
*************** coredump(struct proc *p)
*** 1392,1401 ****
  	struct vmspace *vm = p->p_vmspace;
  	struct nameidata nd;
  	struct vattr vattr;
  	int error, error1, len;
  	char name[sizeof("/var/crash/") + MAXCOMLEN + sizeof(".core")];
  	char *dir = "";
- 	struct core core;
  
  	/*
  	 * Don't dump if not root and the process has used set user or
--- 1399,1408 ----
  	struct vmspace *vm = p->p_vmspace;
  	struct nameidata nd;
  	struct vattr vattr;
+ 	struct coredump_iostate	io;
  	int error, error1, len;
  	char name[sizeof("/var/crash/") + MAXCOMLEN + sizeof(".core")];
  	char *dir = "";
  
  	/*
  	 * Don't dump if not root and the process has used set user or
*************** coredump(struct proc *p)
*** 1455,1460 ****
--- 1462,1492 ----
  	bcopy(p, &p->p_addr->u_kproc.kp_proc, sizeof(struct proc));
  	fill_eproc(p, &p->p_addr->u_kproc.kp_eproc);
  
+ 	io.io_proc = p;
+ 	io.io_vp = vp;
+ 	io.io_cred = cred;
+ 	io.io_offset = 0;
+ 
+ 	error = (*p->p_emul->e_coredump)(p, &io);
+ out:
+ 	VOP_UNLOCK(vp, 0, p);
+ 	error1 = vn_close(vp, FWRITE, cred, p);
+ 	crfree(cred);
+ 	if (error == 0)
+ 		error = error1;
+ 	return (error);
+ }
+ 
+ int
+ coredump_trad(struct proc *p, void *cookie)
+ {
+ 	struct coredump_iostate *io = cookie;
+ 	struct vmspace *vm = io->io_proc->p_vmspace;
+ 	struct vnode *vp = io->io_vp;
+ 	struct ucred *cred = io->io_cred;
+ 	struct core core;
+ 	int error;
+ 
  	core.c_midmag = 0;
  	strlcpy(core.c_name, p->p_comm, sizeof(core.c_name));
  	core.c_nseg = 0;
*************** coredump(struct proc *p)
*** 1466,1489 ****
  	core.c_ssize = (u_long)round_page(ptoa(vm->vm_ssize));
  	error = cpu_coredump(p, vp, cred, &core);
  	if (error)
! 		goto out;
  	/*
  	 * uvm_coredump() spits out all appropriate segments.
  	 * All that's left to do is to write the core header.
  	 */
  	error = uvm_coredump(p, vp, cred, &core);
  	if (error)
! 		goto out;
  	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&core,
  	    (int)core.c_hdrsize, (off_t)0,
  	    UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, NULL, p);
- out:
- 	VOP_UNLOCK(vp, 0, p);
- 	error1 = vn_close(vp, FWRITE, cred, p);
- 	crfree(cred);
- 	if (error == 0)
- 		error = error1;
  	return (error);
  }
  
  /*
--- 1498,1536 ----
  	core.c_ssize = (u_long)round_page(ptoa(vm->vm_ssize));
  	error = cpu_coredump(p, vp, cred, &core);
  	if (error)
! 		return (error);
  	/*
  	 * uvm_coredump() spits out all appropriate segments.
  	 * All that's left to do is to write the core header.
  	 */
  	error = uvm_coredump(p, vp, cred, &core);
  	if (error)
! 		return (error);
  	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&core,
  	    (int)core.c_hdrsize, (off_t)0,
  	    UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, NULL, p);
  	return (error);
+ }
+ 
+ int
+ coredump_write(void *cookie, enum uio_seg segflg, const void *data, size_t len)
+ {
+ 	struct coredump_iostate *io = cookie;
+ 	int error;
+ 
+ 	error = vn_rdwr(UIO_WRITE, io->io_vp, (void *)data, len,
+ 	    io->io_offset, segflg,
+ 	    IO_NODELOCKED|IO_UNIT, io->io_cred, NULL, io->io_proc);
+ 	if (error) {
+ 		printf("pid %d (%s): %s write of %zu@%p at %lld failed: %d\n",
+ 		    io->io_proc->p_pid, io->io_proc->p_comm,
+ 		    segflg == UIO_USERSPACE ? "user" : "system",
+ 		    len, data, (long long) io->io_offset, error);
+ 		return (error);
+ 	}
+ 
+ 	io->io_offset += len;
+ 	return (0);
  }
  
  /*
Index: src/sys/kern/tty.c
===================================================================
RCS file: /cvs/src/sys/kern/tty.c,v
retrieving revision 1.78
retrieving revision 1.79
diff -c -p -r1.78 -r1.79
*** src/sys/kern/tty.c	11 Nov 2008 15:33:02 -0000	1.78
--- src/sys/kern/tty.c	24 Dec 2008 11:20:31 -0000	1.79
***************
*** 1,4 ****
! /*	$OpenBSD: tty.c,v 1.78 2008/11/11 15:33:02 deraadt Exp $	*/
  /*	$NetBSD: tty.c,v 1.68.4.2 1996/06/06 16:04:52 thorpej Exp $	*/
  
  /*-
--- 1,4 ----
! /*	$OpenBSD: tty.c,v 1.79 2008/12/24 11:20:31 kettenis Exp $	*/
  /*	$NetBSD: tty.c,v 1.68.4.2 1996/06/06 16:04:52 thorpej Exp $	*/
  
  /*-
*************** filt_ttywrite(struct knote *kn, long hint)
*** 1147,1155 ****
  {
  	dev_t dev = (dev_t)((u_long)kn->kn_hook);
  	struct tty *tp = (*cdevsw[major(dev)].d_tty)(dev);
  
! 	kn->kn_data = tp->t_outq.c_cc;
! 	return (kn->kn_data <= tp->t_lowat);
  }
  
  static int
--- 1147,1159 ----
  {
  	dev_t dev = (dev_t)((u_long)kn->kn_hook);
  	struct tty *tp = (*cdevsw[major(dev)].d_tty)(dev);
+ 	int canwrite, s;
  
! 	s = spltty();
! 	kn->kn_data = tp->t_outq.c_cn - tp->t_outq.c_cc;
! 	canwrite = (tp->t_outq.c_cc <= tp->t_lowat);
! 	splx(s);
! 	return (canwrite);
  }
  
  static int
Index: src/sys/kern/vfs_vnops.c
===================================================================
RCS file: /cvs/src/sys/kern/vfs_vnops.c,v
retrieving revision 1.60
retrieving revision 1.61
diff -c -p -r1.60 -r1.61
*** src/sys/kern/vfs_vnops.c	19 Sep 2008 12:24:55 -0000	1.60
--- src/sys/kern/vfs_vnops.c	24 Jan 2009 23:25:17 -0000	1.61
***************
*** 1,4 ****
! /*	$OpenBSD: vfs_vnops.c,v 1.60 2008/09/19 12:24:55 art Exp $	*/
  /*	$NetBSD: vfs_vnops.c,v 1.20 1996/02/04 02:18:41 christos Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: vfs_vnops.c,v 1.61 2009/01/24 23:25:17 thib Exp $	*/
  /*	$NetBSD: vfs_vnops.c,v 1.20 1996/02/04 02:18:41 christos Exp $	*/
  
  /*
*************** vn_open(struct nameidata *ndp, int fmode, int cmode)
*** 97,102 ****
--- 97,104 ----
  			VATTR_NULL(&va);
  			va.va_type = VREG;
  			va.va_mode = cmode;
+ 			if (fmode & O_EXCL)
+ 				va.va_vaflags |= VA_EXCLUSIVE;
  			error = VOP_CREATE(ndp->ni_dvp, &ndp->ni_vp,
  					   &ndp->ni_cnd, &va);
  			if (error)
Index: src/sys/uvm/uvm_unix.c
===================================================================
RCS file: /cvs/src/sys/uvm/uvm_unix.c,v
retrieving revision 1.34
retrieving revision 1.35
diff -c -p -r1.34 -r1.35
*** src/sys/uvm/uvm_unix.c	2 Mar 2008 20:29:20 -0000	1.34
--- src/sys/uvm/uvm_unix.c	5 Mar 2009 19:52:24 -0000	1.35
***************
*** 1,4 ****
! /*	$OpenBSD: uvm_unix.c,v 1.34 2008/03/02 20:29:20 kettenis Exp $	*/
  /*	$NetBSD: uvm_unix.c,v 1.18 2000/09/13 15:00:25 thorpej Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: uvm_unix.c,v 1.35 2009/03/05 19:52:24 kettenis Exp $	*/
  /*	$NetBSD: uvm_unix.c,v 1.18 2000/09/13 15:00:25 thorpej Exp $	*/
  
  /*
*************** uvm_coredump(p, vp, cred, chdr)
*** 255,257 ****
--- 255,330 ----
  	return (error);
  }
  
+ int
+ uvm_coredump_walkmap(struct proc *p, void *iocookie,
+     int (*func)(struct proc *, void *, struct uvm_coredump_state *),
+     void *cookie)
+ {
+ 	struct uvm_coredump_state state;
+ 	struct vmspace *vm = p->p_vmspace;
+ 	struct vm_map *map = &vm->vm_map;
+ 	struct vm_map_entry *entry;
+ 	vaddr_t top;
+ 	int error;
+ 
+ 	for (entry = map->header.next; entry != &map->header;
+ 	    entry = entry->next) {
+ 
+ 		state.cookie = cookie;
+ 		state.prot = entry->protection;
+ 		state.flags = 0;
+ 
+ 		/* should never happen for a user process */
+ 		if (UVM_ET_ISSUBMAP(entry)) {
+ 			panic("uvm_coredump: user process with submap?");
+ 		}
+ 
+ 		if (!(entry->protection & VM_PROT_WRITE) &&
+ 		    entry->start != p->p_sigcode)
+ 			continue;
+ 
+ 		/*
+ 		 * Don't dump mmaped devices.
+ 		 */
+ 		if (entry->object.uvm_obj != NULL &&
+ 		    UVM_OBJ_IS_DEVICE(entry->object.uvm_obj))
+ 			continue;
+ 
+ 		state.start = entry->start;
+ 		state.realend = entry->end;
+ 		state.end = entry->end;
+ 
+ 		if (state.start >= VM_MAXUSER_ADDRESS)
+ 			continue;
+ 
+ 		if (state.end > VM_MAXUSER_ADDRESS)
+ 			state.end = VM_MAXUSER_ADDRESS;
+ 
+ #ifdef MACHINE_STACK_GROWS_UP
+ 		if (USRSTACK <= state.start &&
+ 		    state.start < (USRSTACK + MAXSSIZ)) {
+ 			top = round_page(USRSTACK + ptoa(vm->vm_ssize));
+ 			if (state.end > top)
+ 				state.end = top;
+ 
+ 			if (state.start >= state.end)
+ 				continue;
+ #else
+ 		if (state.start >= (vaddr_t)vm->vm_maxsaddr) {
+ 			top = trunc_page(USRSTACK - ptoa(vm->vm_ssize));
+ 			if (state.start < top)
+ 				state.start = top;
+ 
+ 			if (state.start >= state.end)
+ 				continue;
+ #endif
+ 			state.flags |= UVM_COREDUMP_STACK;
+ 		}
+ 
+ 		error = (*func)(p, iocookie, &state);
+ 		if (error)
+ 			return (error);
+ 	}
+ 
+ 	return (0);
+ }
Index: src/usr.bin/make/cond.c
===================================================================
RCS file: /cvs/src/usr.bin/make/cond.c,v
retrieving revision 1.39
retrieving revision 1.40
diff -c -p -r1.39 -r1.40
*** src/usr.bin/make/cond.c	18 Sep 2007 09:44:35 -0000	1.39
--- src/usr.bin/make/cond.c	24 Nov 2008 16:23:04 -0000	1.40
***************
*** 1,5 ****
  /*	$OpenPackages$ */
! /*	$OpenBSD: cond.c,v 1.39 2007/09/18 09:44:35 espie Exp $	*/
  /*	$NetBSD: cond.c,v 1.7 1996/11/06 17:59:02 christos Exp $	*/
  
  /*
--- 1,5 ----
  /*	$OpenPackages$ */
! /*	$OpenBSD: cond.c,v 1.40 2008/11/24 16:23:04 espie Exp $	*/
  /*	$NetBSD: cond.c,v 1.7 1996/11/06 17:59:02 christos Exp $	*/
  
  /*
*************** CondCvtArg(const char *str, double *value)
*** 347,353 ****
  			if (isdigit(*str))
  				x  = *str - '0';
  			else if (isxdigit(*str))
! 				x = 10 + *str - isupper(*str) ? 'A' : 'a';
  			else
  				return false;
  			i = (i << 4) + x;
--- 347,353 ----
  			if (isdigit(*str))
  				x  = *str - '0';
  			else if (isxdigit(*str))
! 				x = 10 + *str - (isupper(*str) ? 'A' : 'a');
  			else
  				return false;
  			i = (i << 4) + x;
Index: src/lib/csu/common_elf/crtbegin.c
===================================================================
RCS file: /cvs/src/lib/csu/common_elf/crtbegin.c,v
retrieving revision 1.12
retrieving revision 1.13
diff -c -p -r1.12 -r1.13
*** src/lib/csu/common_elf/crtbegin.c	3 Sep 2007 14:40:16 -0000	1.12
--- src/lib/csu/common_elf/crtbegin.c	13 Apr 2009 20:15:24 -0000	1.13
***************
*** 1,4 ****
! /*	$OpenBSD: crtbegin.c,v 1.12 2007/09/03 14:40:16 millert Exp $	*/
  /*	$NetBSD: crtbegin.c,v 1.1 1996/09/12 16:59:03 cgd Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: crtbegin.c,v 1.13 2009/04/13 20:15:24 kurt Exp $	*/
  /*	$NetBSD: crtbegin.c,v 1.1 1996/09/12 16:59:03 cgd Exp $	*/
  
  /*
*************** static const char __EH_FRAME_BEGIN__[]
*** 60,65 ****
--- 60,77 ----
      __attribute__((section(".eh_frame"), aligned(4))) = { };
  
  /*
+  * java class registration hooks
+  */
+ 
+ #if (__GNUC__ > 2)
+ static void *__JCR_LIST__[]
+     __attribute__((section(".jcr"), aligned(sizeof(void*)))) = { };
+ 
+ extern void _Jv_RegisterClasses (void *)
+     __attribute__((weak));
+ #endif
+ 
+ /*
   * Include support for the __cxa_atexit/__cxa_finalize C++ abi for
   * gcc > 2.x. __dso_handle is NULL in the main program and a unique
   * value for each C++ shared library. For more info on this API, see:
*************** __do_init()
*** 132,137 ****
--- 144,154 ----
  		initialized = 1;
  
  		__register_frame_info(__EH_FRAME_BEGIN__, &object);
+ 
+ #if (__GNUC__ > 2)
+ 		if (__JCR_LIST__[0] && _Jv_RegisterClasses)
+ 			_Jv_RegisterClasses(__JCR_LIST__);
+ #endif
  
  		(__ctors)();
  
Index: src/lib/csu/common_elf/crtbeginS.c
===================================================================
RCS file: /cvs/src/lib/csu/common_elf/crtbeginS.c,v
retrieving revision 1.9
retrieving revision 1.10
diff -c -p -r1.9 -r1.10
*** src/lib/csu/common_elf/crtbeginS.c	4 Feb 2009 19:43:48 -0000	1.9
--- src/lib/csu/common_elf/crtbeginS.c	13 Apr 2009 20:15:24 -0000	1.10
***************
*** 1,4 ****
! /*	$OpenBSD: crtbeginS.c,v 1.9 2009/02/04 19:43:48 kettenis Exp $	*/
  /*	$NetBSD: crtbegin.c,v 1.1 1996/09/12 16:59:03 cgd Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: crtbeginS.c,v 1.10 2009/04/13 20:15:24 kurt Exp $	*/
  /*	$NetBSD: crtbegin.c,v 1.1 1996/09/12 16:59:03 cgd Exp $	*/
  
  /*
***************
*** 47,52 ****
--- 47,64 ----
  #include "extern.h"
  
  /*
+  * java class registration hooks
+  */
+ 
+ #if (__GNUC__ > 2)
+ static void *__JCR_LIST__[]
+     __attribute__((section(".jcr"), aligned(sizeof(void*)))) = { };
+ 
+ extern void _Jv_RegisterClasses (void *)
+     __attribute__((weak));
+ #endif
+ 
+ /*
   * Include support for the __cxa_atexit/__cxa_finalize C++ abi for
   * gcc > 2.x. __dso_handle is NULL in the main program and a unique
   * value for each C++ shared library. For more info on this API, see:
*************** _do_init(void)
*** 118,123 ****
--- 130,141 ----
  	 */
  	if (!initialized) {
  		initialized = 1;
+ 
+ #if (__GNUC__ > 2)
+ 		if (__JCR_LIST__[0] && _Jv_RegisterClasses)
+ 			_Jv_RegisterClasses(__JCR_LIST__);
+ #endif
+ 
  		__ctors();
  	}
  }
Index: src/lib/csu/common_elf/crtend.c
===================================================================
RCS file: /cvs/src/lib/csu/common_elf/crtend.c,v
retrieving revision 1.6
retrieving revision 1.7
diff -c -p -r1.6 -r1.7
*** src/lib/csu/common_elf/crtend.c	10 Oct 2004 18:29:15 -0000	1.6
--- src/lib/csu/common_elf/crtend.c	13 Apr 2009 20:15:24 -0000	1.7
***************
*** 1,4 ****
! /*	$OpenBSD: crtend.c,v 1.6 2004/10/10 18:29:15 kettenis Exp $	*/
  /*	$NetBSD: crtend.c,v 1.1 1996/09/12 16:59:04 cgd Exp $	*/
  
  #include <sys/cdefs.h>
--- 1,4 ----
! /*	$OpenBSD: crtend.c,v 1.7 2009/04/13 20:15:24 kurt Exp $	*/
  /*	$NetBSD: crtend.c,v 1.1 1996/09/12 16:59:04 cgd Exp $	*/
  
  #include <sys/cdefs.h>
*************** static init_f __DTOR_LIST__[1]
*** 12,17 ****
--- 12,22 ----
  
  static const int __EH_FRAME_END__[]
  __attribute__((unused, mode(SI), section(".eh_frame"), aligned(4))) = { 0 };
+ 
+ #if (__GNUC__ > 2)
+ static void * __JCR_END__[]
+ __attribute__((unused, section(".jcr"), aligned(sizeof(void*)))) = { 0 };
+ #endif
  
  MD_SECTION_EPILOGUE(".init");
  MD_SECTION_EPILOGUE(".fini");
Index: src/lib/csu/common_elf/crtendS.c
===================================================================
RCS file: /cvs/src/lib/csu/common_elf/crtendS.c,v
retrieving revision 1.5
retrieving revision 1.6
diff -c -p -r1.5 -r1.6
*** src/lib/csu/common_elf/crtendS.c	26 Jan 2004 20:04:11 -0000	1.5
--- src/lib/csu/common_elf/crtendS.c	13 Apr 2009 20:15:24 -0000	1.6
***************
*** 1,4 ****
! /*	$OpenBSD: crtendS.c,v 1.5 2004/01/26 20:04:11 espie Exp $	*/
  /*	$NetBSD: crtend.c,v 1.1 1997/04/16 19:38:24 thorpej Exp $	*/
  
  #include <sys/cdefs.h>
--- 1,4 ----
! /*	$OpenBSD: crtendS.c,v 1.6 2009/04/13 20:15:24 kurt Exp $	*/
  /*	$NetBSD: crtend.c,v 1.1 1997/04/16 19:38:24 thorpej Exp $	*/
  
  #include <sys/cdefs.h>
*************** static init_f __CTOR_LIST__[1]
*** 9,14 ****
--- 9,19 ----
      __attribute__((section(".ctors"))) = { (void *)0 };		/* XXX */
  static init_f __DTOR_LIST__[1]
      __attribute__((section(".dtors"))) = { (void *)0 };		/* XXX */
+ 
+ #if (__GNUC__ > 2)
+ static void * __JCR_END__[]
+ __attribute__((unused, section(".jcr"), aligned(sizeof(void*)))) = { 0 };
+ #endif
  
  MD_SECTION_EPILOGUE(".init");
  MD_SECTION_EPILOGUE(".fini");
Index: src/lib/libc/rpc/xdr_mem.c
===================================================================
RCS file: /cvs/src/lib/libc/rpc/xdr_mem.c,v
retrieving revision 1.13
retrieving revision 1.14
diff -c -p -r1.13 -r1.14
*** src/lib/libc/rpc/xdr_mem.c	31 Mar 2006 18:28:55 -0000	1.13
--- src/lib/libc/rpc/xdr_mem.c	9 Dec 2008 19:40:10 -0000	1.14
***************
*** 1,4 ****
! /*	$OpenBSD: xdr_mem.c,v 1.13 2006/03/31 18:28:55 deraadt Exp $ */
  /*
   * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
   * unrestricted use provided that this legend is included on all tape
--- 1,4 ----
! /*	$OpenBSD: xdr_mem.c,v 1.14 2008/12/09 19:40:10 otto Exp $ */
  /*
   * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
   * unrestricted use provided that this legend is included on all tape
***************
*** 39,49 ****
   *
   */
  
  #include <string.h>
  
  #include <rpc/types.h>
  #include <rpc/xdr.h>
- #include <netinet/in.h>
  
  static bool_t	xdrmem_getlong_aligned(XDR *, long *);
  static bool_t	xdrmem_putlong_aligned(XDR *, long *);
--- 39,50 ----
   *
   */
  
+ #include <sys/types.h>
+ #include <netinet/in.h>
  #include <string.h>
  
  #include <rpc/types.h>
  #include <rpc/xdr.h>
  
  static bool_t	xdrmem_getlong_aligned(XDR *, long *);
  static bool_t	xdrmem_putlong_aligned(XDR *, long *);
*************** static struct	xdr_ops xdrmem_ops_aligned = {
*** 65,71 ****
  	xdrmem_getpos,
  	xdrmem_setpos,
  	xdrmem_inline_aligned,
! 	xdrmem_destroy
  };
  
  static struct	xdr_ops xdrmem_ops_unaligned = {
--- 66,73 ----
  	xdrmem_getpos,
  	xdrmem_setpos,
  	xdrmem_inline_aligned,
! 	xdrmem_destroy,
! 	NULL,	/* xdrmem_control */
  };
  
  static struct	xdr_ops xdrmem_ops_unaligned = {
*************** static struct	xdr_ops xdrmem_ops_unaligned = {
*** 76,82 ****
  	xdrmem_getpos,
  	xdrmem_setpos,
  	xdrmem_inline_unaligned,
! 	xdrmem_destroy
  };
  
  /*
--- 78,85 ----
  	xdrmem_getpos,
  	xdrmem_setpos,
  	xdrmem_inline_unaligned,
! 	xdrmem_destroy,
! 	NULL,	/* xdrmem_control */
  };
  
  /*
Index: src/lib/libc/rpc/xdr_rec.c
===================================================================
RCS file: /cvs/src/lib/libc/rpc/xdr_rec.c,v
retrieving revision 1.12
retrieving revision 1.13
diff -c -p -r1.12 -r1.13
*** src/lib/libc/rpc/xdr_rec.c	2 Apr 2006 02:04:05 -0000	1.12
--- src/lib/libc/rpc/xdr_rec.c	9 Dec 2008 19:40:10 -0000	1.13
***************
*** 1,4 ****
! /*	$OpenBSD: xdr_rec.c,v 1.12 2006/04/02 02:04:05 deraadt Exp $ */
  /*
   * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
   * unrestricted use provided that this legend is included on all tape
--- 1,4 ----
! /*	$OpenBSD: xdr_rec.c,v 1.13 2008/12/09 19:40:10 otto Exp $ */
  /*
   * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
   * unrestricted use provided that this legend is included on all tape
***************
*** 44,55 ****
   * The other 31 bits encode the byte length of the fragment.
   */
  
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <rpc/types.h>
  #include <rpc/xdr.h>
! #include <netinet/in.h>
  
  static bool_t	xdrrec_getlong(XDR *, long *);
  static bool_t	xdrrec_putlong(XDR *, long *);
--- 44,60 ----
   * The other 31 bits encode the byte length of the fragment.
   */
  
+ #include <sys/types.h>
+ #include <netinet/in.h>
+ #include <stddef.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <rpc/types.h>
  #include <rpc/xdr.h>
! #include <rpc/auth.h>
! #include <rpc/svc.h>
! #include <rpc/clnt.h>
  
  static bool_t	xdrrec_getlong(XDR *, long *);
  static bool_t	xdrrec_putlong(XDR *, long *);
*************** static struct  xdr_ops xdrrec_ops = {
*** 70,76 ****
  	xdrrec_getpos,
  	xdrrec_setpos,
  	xdrrec_inline,
! 	xdrrec_destroy
  };
  
  /*
--- 75,82 ----
  	xdrrec_getpos,
  	xdrrec_setpos,
  	xdrrec_inline,
! 	xdrrec_destroy,
! 	NULL, /* xdrrec_control */
  };
  
  /*
*************** static struct  xdr_ops xdrrec_ops = {
*** 90,96 ****
  
  typedef struct rec_strm {
  	caddr_t tcp_handle;
- 	caddr_t the_buffer;
  	/*
  	 * out-goung bits
  	 */
--- 96,101 ----
*************** typedef struct rec_strm {
*** 112,126 ****
  	bool_t last_frag;
  	u_int sendsize;
  	u_int recvsize;
  } RECSTREAM;
  
  static u_int	fix_buf_size(u_int);
  static bool_t	flush_out(RECSTREAM *, bool_t);
  static bool_t	get_input_bytes(RECSTREAM *, caddr_t, int);
  static bool_t	set_input_fragment(RECSTREAM *);
  static bool_t	skip_input_bytes(RECSTREAM *, long);
  
- 
  /*
   * Create an xdr handle for xdrrec
   * xdrrec_create fills in xdrs.  Sendsize and recvsize are
--- 117,141 ----
  	bool_t last_frag;
  	u_int sendsize;
  	u_int recvsize;
+ 
+ 	bool_t nonblock;
+ 	bool_t in_haveheader;
+ 	u_int32_t in_header;
+ 	char *in_hdrp;
+ 	int in_hdrlen;
+ 	int in_reclen;
+ 	int in_received;
+ 	int in_maxrec;
  } RECSTREAM;
  
  static u_int	fix_buf_size(u_int);
  static bool_t	flush_out(RECSTREAM *, bool_t);
+ static bool_t	fill_input_buf(RECSTREAM *);
  static bool_t	get_input_bytes(RECSTREAM *, caddr_t, int);
  static bool_t	set_input_fragment(RECSTREAM *);
  static bool_t	skip_input_bytes(RECSTREAM *, long);
+ static bool_t	realloc_stream(RECSTREAM *, int);
  
  /*
   * Create an xdr handle for xdrrec
   * xdrrec_create fills in xdrs.  Sendsize and recvsize are
*************** xdrrec_create(XDR *xdrs, u_int sendsize, u_int recvsiz
*** 148,168 ****
  		 */
  		return;
  	}
! 	/*
! 	 * adjust sizes and allocate buffer quad byte aligned
! 	 */
  	rstrm->sendsize = sendsize = fix_buf_size(sendsize);
  	rstrm->recvsize = recvsize = fix_buf_size(recvsize);
! 	rstrm->the_buffer = mem_alloc(sendsize + recvsize + BYTES_PER_XDR_UNIT);
! 	if (rstrm->the_buffer == NULL) {
  		(void)fprintf(stderr, "xdrrec_create: out of memory\n");
! 		free(rstrm);
  		return;
  	}
- 	for (rstrm->out_base = rstrm->the_buffer;
- 		(u_long)rstrm->out_base % BYTES_PER_XDR_UNIT != 0;
- 		rstrm->out_base++);
- 	rstrm->in_base = rstrm->out_base + sendsize;
  	/*
  	 * now the rest ...
  	 */
--- 163,185 ----
  		 */
  		return;
  	}
! 
  	rstrm->sendsize = sendsize = fix_buf_size(sendsize);
+ 	rstrm->out_base = malloc(rstrm->sendsize);
+ 	if (rstrm->out_base == NULL) {
+ 		(void)fprintf(stderr, "xdrrec_create: out of memory\n");
+ 		mem_free(rstrm, sizeof(RECSTREAM));
+ 		return;
+ 	}
+ 
  	rstrm->recvsize = recvsize = fix_buf_size(recvsize);
! 	rstrm->in_base = malloc(recvsize);
! 	if (rstrm->in_base == NULL) {
  		(void)fprintf(stderr, "xdrrec_create: out of memory\n");
! 		mem_free(rstrm->out_base, sendsize);
! 		mem_free(rstrm, sizeof(RECSTREAM));
  		return;
  	}
  	/*
  	 * now the rest ...
  	 */
*************** xdrrec_create(XDR *xdrs, u_int sendsize, u_int recvsiz
*** 181,186 ****
--- 198,209 ----
  	rstrm->in_finger = (rstrm->in_boundry += recvsize);
  	rstrm->fbtbc = 0;
  	rstrm->last_frag = TRUE;
+ 	rstrm->in_haveheader = FALSE;
+ 	rstrm->in_hdrlen = 0;
+ 	rstrm->in_hdrp = (char *)(void *)&rstrm->in_header;
+ 	rstrm->nonblock = FALSE;
+ 	rstrm->in_reclen = 0;
+ 	rstrm->in_received = 0;
  }
  
  
*************** xdrrec_setpos(XDR *xdrs, u_int pos)
*** 336,341 ****
--- 359,367 ----
  				return (TRUE);
  			}
  			break;
+ 
+ 		case XDR_FREE:
+ 			break;
  		}
  	return (FALSE);
  }
*************** xdrrec_inline(XDR *xdrs, u_int len)
*** 363,368 ****
--- 389,397 ----
  			rstrm->in_finger += len;
  		}
  		break;
+ 
+ 	case XDR_FREE:
+ 		break;
  	}
  	return (buf);
  }
*************** xdrrec_destroy(XDR *xdrs)
*** 372,380 ****
  {
  	RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
  
! 	mem_free(rstrm->the_buffer,
! 		rstrm->sendsize + rstrm->recvsize + BYTES_PER_XDR_UNIT);
! 	mem_free((caddr_t)rstrm, sizeof(RECSTREAM));
  }
  
  
--- 401,409 ----
  {
  	RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
  
! 	mem_free(rstrm->out_base, rstrm->sendsize);
! 	mem_free(rstrm->in_base, rstrm->recvsize);
! 	mem_free(rstrm, sizeof(RECSTREAM));
  }
  
  
*************** bool_t
*** 390,396 ****
--- 419,438 ----
  xdrrec_skiprecord(XDR *xdrs)
  {
  	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
+ 	enum xprt_stat xstat;
  
+ 	if (rstrm->nonblock) {
+ 		if (__xdrrec_getrec(xdrs, &xstat, FALSE)) {
+ 			rstrm->fbtbc = 0;
+ 			return (TRUE);
+ 		}
+ 		if (rstrm->in_finger == rstrm->in_boundry &&
+ 		    xstat == XPRT_MOREREQS) {
+ 			rstrm->fbtbc = 0;
+ 			return (TRUE);
+ 		}
+ 		return (FALSE);
+ 	}
  	while (rstrm->fbtbc > 0 || (! rstrm->last_frag)) {
  		if (! skip_input_bytes(rstrm, rstrm->fbtbc))
  			return (FALSE);
*************** xdrrec_endofrecord(XDR *xdrs, int32_t sendnow)
*** 450,456 ****
--- 492,588 ----
  	return (TRUE);
  }
  
+ /*
+  * Fill the stream buffer with a record for a non-blocking connection.
+  * Return true if a record is available in the buffer, false if not.
+  */
+ bool_t
+ __xdrrec_getrec(XDR *xdrs, enum xprt_stat *statp, bool_t expectdata)
+ {
+ 	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
+ 	ssize_t n;
+ 	int fraglen;
  
+ 	if (!rstrm->in_haveheader) {
+ 		n = rstrm->readit(rstrm->tcp_handle, rstrm->in_hdrp,
+ 		    (int)sizeof (rstrm->in_header) - rstrm->in_hdrlen);
+ 		if (n == 0) {
+ 			*statp = expectdata ? XPRT_DIED : XPRT_IDLE;
+ 			return (FALSE);
+ 		}
+ 		if (n < 0) {
+ 			*statp = XPRT_DIED;
+ 			return (FALSE);
+ 		}
+ 		rstrm->in_hdrp += n;
+ 		rstrm->in_hdrlen += n;
+ 		if (rstrm->in_hdrlen < sizeof (rstrm->in_header)) {
+ 			*statp = XPRT_MOREREQS;
+ 			return (FALSE);
+ 		}
+ 		rstrm->in_header = ntohl(rstrm->in_header);
+ 		fraglen = (int)(rstrm->in_header & ~LAST_FRAG);
+ 		if (fraglen == 0 || fraglen > rstrm->in_maxrec ||
+ 		    (rstrm->in_reclen + fraglen) > rstrm->in_maxrec) {
+ 			*statp = XPRT_DIED;
+ 			return (FALSE);
+ 		}
+ 		rstrm->in_reclen += fraglen;
+ 		if (rstrm->in_reclen > rstrm->recvsize)
+ 			realloc_stream(rstrm, rstrm->in_reclen);
+ 		if (rstrm->in_header & LAST_FRAG) {
+ 			rstrm->in_header &= ~LAST_FRAG;
+ 			rstrm->last_frag = TRUE;
+ 		}
+ 	}
+ 
+ 	n =  rstrm->readit(rstrm->tcp_handle,
+ 	    rstrm->in_base + rstrm->in_received,
+ 	    (rstrm->in_reclen - rstrm->in_received));
+ 
+ 	if (n < 0) {
+ 		*statp = XPRT_DIED;
+ 		return (FALSE);
+ 	}
+ 
+ 	if (n == 0) {
+ 		*statp = expectdata ? XPRT_DIED : XPRT_IDLE;
+ 		return (FALSE);
+ 	}
+ 
+ 	rstrm->in_received += n;
+ 
+ 	if (rstrm->in_received == rstrm->in_reclen) {
+ 		rstrm->in_haveheader = (FALSE);
+ 		rstrm->in_hdrp = (char *)(void *)&rstrm->in_header;
+ 		rstrm->in_hdrlen = 0;
+ 		if (rstrm->last_frag) {
+ 			rstrm->fbtbc = rstrm->in_reclen;
+ 			rstrm->in_boundry = rstrm->in_base + rstrm->in_reclen;
+ 			rstrm->in_finger = rstrm->in_base;
+ 			rstrm->in_reclen = rstrm->in_received = 0;
+ 			*statp = XPRT_MOREREQS;
+ 			return (TRUE);
+ 		}
+ 	}
+ 
+ 	*statp = XPRT_MOREREQS;
+ 	return (FALSE);
+ }
+ 
+ bool_t
+ __xdrrec_setnonblock(XDR *xdrs, int maxrec)
+ {
+ 	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
+ 
+ 	rstrm->nonblock = TRUE;
+ 	if (maxrec == 0)
+ 		maxrec = rstrm->recvsize;
+ 	rstrm->in_maxrec = maxrec;
+ 	return (TRUE);
+ }
+ 
+ 
  /*
   * Internal useful routines
   */
*************** fill_input_buf(RECSTREAM *rstrm)
*** 478,483 ****
--- 610,617 ----
  	u_long i;
  	long len;
  
+ 	if (rstrm->nonblock)
+ 		return FALSE;
  	where = rstrm->in_base;
  	i = (u_long)rstrm->in_boundry % BYTES_PER_XDR_UNIT;
  	where += i;
*************** get_input_bytes(RECSTREAM *rstrm, caddr_t addr, int le
*** 495,500 ****
--- 629,642 ----
  {
  	long current;
  
+ 	if (rstrm->nonblock) {
+ 		if (len > (int)(rstrm->in_boundry - rstrm->in_finger))
+ 			return FALSE;
+ 		memcpy(addr, rstrm->in_finger, (size_t)len);
+ 		rstrm->in_finger += len;
+ 		return (TRUE);
+ 	}
+ 
  	while (len > 0) {
  		current = (long)rstrm->in_boundry - (long)rstrm->in_finger;
  		if (current == 0) {
*************** set_input_fragment(RECSTREAM *rstrm)
*** 516,526 ****
  {
  	u_int32_t header;
  
  	if (! get_input_bytes(rstrm, (caddr_t)&header, sizeof(header)))
  		return (FALSE);
  	header = (long)ntohl(header);
  	rstrm->last_frag = ((header & LAST_FRAG) == 0) ? FALSE : TRUE;
! 	if ((header & (~LAST_FRAG)) == 0)
  		return(FALSE);
  	rstrm->fbtbc = header & (~LAST_FRAG);
  	return (TRUE);
--- 658,678 ----
  {
  	u_int32_t header;
  
+ 	if (rstrm->nonblock)
+ 		return (FALSE);
  	if (! get_input_bytes(rstrm, (caddr_t)&header, sizeof(header)))
  		return (FALSE);
  	header = (long)ntohl(header);
  	rstrm->last_frag = ((header & LAST_FRAG) == 0) ? FALSE : TRUE;
! 	/*
! 	 * Sanity check. Try not to accept wildly incorrect
! 	 * record sizes. Unfortunately, the only record size
! 	 * we can positively identify as being 'wildly incorrect'
! 	 * is zero. Ridiculously large record sizes may look wrong,
! 	 * but we don't have any way to be certain that they aren't
! 	 * what the client actually intended to send us.
! 	 */
! 	if (header == 0)
  		return(FALSE);
  	rstrm->fbtbc = header & (~LAST_FRAG);
  	return (TRUE);
*************** fix_buf_size(u_int s)
*** 552,555 ****
--- 704,731 ----
  	if (s < 100)
  		s = 4000;
  	return (RNDUP(s));
+ }
+ 
+ /*
+  * Reallocate the input buffer for a non-block stream.
+  */
+ static bool_t
+ realloc_stream(RECSTREAM *rstrm, int size)
+ {
+ 	ptrdiff_t diff;
+ 	char *buf;
+ 
+ 	if (size > rstrm->recvsize) {
+ 		buf = realloc(rstrm->in_base, (size_t)size);
+ 		if (buf == NULL)
+ 			return (FALSE);
+ 		diff = buf - rstrm->in_base;
+ 		rstrm->in_finger += diff;
+ 		rstrm->in_base = buf;
+ 		rstrm->in_boundry = buf + size;
+ 		rstrm->recvsize = size;
+ 		rstrm->in_size = size;
+ 	}
+ 
+ 	return (TRUE);
  }
Index: src/lib/libc/rpc/xdr_stdio.c
===================================================================
RCS file: /cvs/src/lib/libc/rpc/xdr_stdio.c,v
retrieving revision 1.11
retrieving revision 1.12
diff -c -p -r1.11 -r1.12
*** src/lib/libc/rpc/xdr_stdio.c	17 Sep 2007 16:04:24 -0000	1.11
--- src/lib/libc/rpc/xdr_stdio.c	9 Dec 2008 19:40:10 -0000	1.12
***************
*** 1,4 ****
! /*	$OpenBSD: xdr_stdio.c,v 1.11 2007/09/17 16:04:24 blambert Exp $ */
  /*
   * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
   * unrestricted use provided that this legend is included on all tape
--- 1,4 ----
! /*	$OpenBSD: xdr_stdio.c,v 1.12 2008/12/09 19:40:10 otto Exp $ */
  /*
   * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
   * unrestricted use provided that this legend is included on all tape
***************
*** 38,45 ****
   * from the stream.
   */
  
- #include <rpc/types.h>
  #include <stdio.h>
  #include <rpc/xdr.h>
  
  static bool_t	xdrstdio_getlong(XDR *, long *);
--- 38,45 ----
   * from the stream.
   */
  
  #include <stdio.h>
+ #include <rpc/types.h>
  #include <rpc/xdr.h>
  
  static bool_t	xdrstdio_getlong(XDR *, long *);
*************** static struct xdr_ops	xdrstdio_ops = {
*** 62,68 ****
  	xdrstdio_getpos,	/* get offset in the stream */
  	xdrstdio_setpos,	/* set offset in the stream */
  	xdrstdio_inline,	/* prime stream for inline macros */
! 	xdrstdio_destroy	/* destroy stream */
  };
  
  /*
--- 62,69 ----
  	xdrstdio_getpos,	/* get offset in the stream */
  	xdrstdio_setpos,	/* set offset in the stream */
  	xdrstdio_inline,	/* prime stream for inline macros */
! 	xdrstdio_destroy,	/* destroy stream */
! 	NULL,			/* xdrstdio_control */
  };
  
  /*
Index: src/sys/arch/sgi/dev/gbe.c
===================================================================
RCS file: /cvs/src/sys/arch/sgi/dev/gbe.c,v
retrieving revision 1.6
retrieving revision 1.7
diff -c -p -r1.6 -r1.7
*** src/sys/arch/sgi/dev/gbe.c	7 Apr 2008 22:34:21 -0000	1.6
--- src/sys/arch/sgi/dev/gbe.c	24 Feb 2009 14:37:29 -0000	1.7
***************
*** 1,7 ****
! /*	$OpenBSD: gbe.c,v 1.6 2008/04/07 22:34:21 miod Exp $ */
  
  /*
!  * Copyright (c) 2007, Joel Sing <jsing@openbsd.org>
   *
   * Permission to use, copy, modify, and distribute this software for any
   * purpose with or without fee is hereby granted, provided that the above
--- 1,7 ----
! /*	$OpenBSD: gbe.c,v 1.7 2009/02/24 14:37:29 jsing Exp $ */
  
  /*
!  * Copyright (c) 2007, 2008, 2009 Joel Sing <jsing@openbsd.org>
   *
   * Permission to use, copy, modify, and distribute this software for any
   * purpose with or without fee is hereby granted, provided that the above
***************
*** 17,23 ****
   */
  
  /*
!  * Graphics Back End (GBE) Framebuffer for SGI O2
   */
  
  #include <sys/param.h>
--- 17,23 ----
   */
  
  /*
!  * Graphics Back End (GBE) Framebuffer for SGI O2.
   */
  
  #include <sys/param.h>
*************** struct  gbe_cmap {
*** 60,66 ****
  struct gbe_screen {
  	struct device *sc;		/* Back pointer. */
  
! 	struct rasops_info ri;		/* Raster display info. */
  	struct gbe_cmap cmap;		/* Display colour map. */
  
  	int fb_size;			/* Size of framebuffer memory. */
--- 60,67 ----
  struct gbe_screen {
  	struct device *sc;		/* Back pointer. */
  
! 	struct rasops_info ri;		/* Screen raster display info. */
! 	struct rasops_info ri_tile;	/* Raster info for rasops tile. */
  	struct gbe_cmap cmap;		/* Display colour map. */
  
  	int fb_size;			/* Size of framebuffer memory. */
*************** struct gbe_screen {
*** 70,80 ****
--- 71,86 ----
  	paddr_t tm_phys;		/* Physical address of tilemap. */
  	caddr_t fb;			/* Address of framebuffer memory. */
  	paddr_t fb_phys;		/* Physical address of framebuffer. */
+ 	caddr_t ro;			/* Address of rasops tile. */
+ 	paddr_t ro_phys;		/* Physical address of rasops tile. */
  
  	int width;			/* Width in pixels. */
  	int height;			/* Height in pixels. */
  	int depth;			/* Colour depth in bits. */
+ 	int mode;			/* Display mode. */
+ 	int bufmode;			/* Rendering engine buffer mode. */
  	int linebytes;			/* Bytes per line. */
+ 	int ro_curpos;			/* Current position in rasops tile. */
  };
  
  /*
*************** struct gbe_softc {
*** 84,90 ****
  	struct device sc_dev;
  
  	bus_space_tag_t iot;
! 	bus_space_handle_t ioh;
  	bus_dma_tag_t dmat;
  
  	int rev;			/* Hardware revision. */
--- 90,97 ----
  	struct device sc_dev;
  
  	bus_space_tag_t iot;
! 	bus_space_handle_t ioh;		/* GBE registers. */
! 	bus_space_handle_t re_ioh;	/* Rendering engine registers. */
  	bus_dma_tag_t dmat;
  
  	int rev;			/* Hardware revision. */
*************** void	gbe_enable(struct gbe_softc *);
*** 102,107 ****
--- 109,115 ----
  void	gbe_disable(struct gbe_softc *);
  void	gbe_setup(struct gbe_softc *);
  void	gbe_setcolour(struct gbe_softc *, u_int, u_int8_t, u_int8_t, u_int8_t);
+ void	gbe_wait_re_idle(struct gbe_softc *);
  
  /*
   * Colour map handling for indexed modes.
*************** int	gbe_show_screen(void *, void *, int, void (*)(void
*** 122,127 ****
--- 130,148 ----
  	    void *);
  void	gbe_burner(void *, u_int, u_int);
  
+ /*
+  * Hardware acceleration for rasops.
+  */
+ void	gbe_rop(struct gbe_softc *, int, int, int, int, int);
+ void	gbe_copyrect(struct gbe_softc *, int, int, int, int, int, int, int);
+ void	gbe_fillrect(struct gbe_softc *, int, int, int, int, int);
+ void	gbe_do_cursor(struct rasops_info *);
+ void	gbe_putchar(void *, int, int, u_int, long);
+ void	gbe_copycols(void *, int, int, int, int);
+ void	gbe_erasecols(void *, int, int, int, long);
+ void	gbe_copyrows(void *, int, int, int);
+ void	gbe_eraserows(void *, int, int, long);
+ 
  static struct gbe_screen gbe_consdata;
  static int gbe_console;
  
*************** gbe_attach(struct device *parent, struct device *self,
*** 180,189 ****
--- 201,213 ----
  	struct wsemuldisplaydev_attach_args waa;
  	bus_dma_segment_t tm_segs[1];
  	bus_dma_segment_t fb_segs[1];
+ 	bus_dma_segment_t ro_segs[1];
  	bus_dmamap_t tm_dmamap;
  	bus_dmamap_t fb_dmamap;
+ 	bus_dmamap_t ro_dmamap;
  	int tm_nsegs;
  	int fb_nsegs;
+ 	int ro_nsegs;
  	uint32_t val;
  	long attr;
  
*************** gbe_attach(struct device *parent, struct device *self,
*** 202,207 ****
--- 226,233 ----
  		 */
  
  		gsc->ioh = PHYS_TO_UNCACHED(GBE_BASE);
+ 		gsc->re_ioh = PHYS_TO_UNCACHED(RE_BASE);
+ 
  		gsc->rev = bus_space_read_4(gsc->iot, gsc->ioh, GBE_CTRL_STAT)
  		    & 0xf;
  
*************** gbe_attach(struct device *parent, struct device *self,
*** 235,248 ****
  	screen = gsc->curscr;
  
  	/* 
! 	 * Setup bus space mapping.
  	 */
  	if (bus_space_map(gsc->iot, GBE_BASE - CRIMEBUS_BASE, GBE_REG_SIZE, 
  	    BUS_SPACE_MAP_LINEAR, &gsc->ioh)) {
! 		printf("failed to map bus space!\n");
  		return;
  	}
  
  	/* Determine GBE revision. */
  	gsc->rev = bus_space_read_4(gsc->iot, gsc->ioh, GBE_CTRL_STAT) & 0xf;
  
--- 261,280 ----
  	screen = gsc->curscr;
  
  	/* 
! 	 * Setup bus space mappings.
  	 */
  	if (bus_space_map(gsc->iot, GBE_BASE - CRIMEBUS_BASE, GBE_REG_SIZE, 
  	    BUS_SPACE_MAP_LINEAR, &gsc->ioh)) {
! 		printf("failed to map framebuffer bus space!\n");
  		return;
  	}
  
+ 	if (bus_space_map(gsc->iot, RE_BASE - CRIMEBUS_BASE, RE_REG_SIZE, 
+ 	    BUS_SPACE_MAP_LINEAR, &gsc->re_ioh)) {
+ 		printf("failed to map rendering engine bus space!\n");
+ 		goto fail0;
+ 	}
+ 
  	/* Determine GBE revision. */
  	gsc->rev = bus_space_read_4(gsc->iot, gsc->ioh, GBE_CTRL_STAT) & 0xf;
  
*************** gbe_attach(struct device *parent, struct device *self,
*** 254,260 ****
  
  	if (screen->width == 0 || screen->height == 0) {
  		printf("device has not been setup by firmware!\n");
! 		goto fail0;
  	}
  
  	/* Setup screen defaults. */
--- 286,292 ----
  
  	if (screen->width == 0 || screen->height == 0) {
  		printf("device has not been setup by firmware!\n");
! 		goto fail1;
  	}
  
  	/* Setup screen defaults. */
*************** gbe_attach(struct device *parent, struct device *self,
*** 264,293 ****
  	screen->linebytes = screen->width * screen->depth / 8;
  
  	/* 
! 	 * Setup DMA for tile map.
  	 */
  	if (bus_dmamap_create(gsc->dmat, screen->tm_size, 1, screen->tm_size,
  	    0, BUS_DMA_NOWAIT, &tm_dmamap)) {
! 		printf("failed to create DMA map for tile map!\n");
! 		goto fail0;
  	}
  
  	if (bus_dmamem_alloc(gsc->dmat, screen->tm_size, 65536, 0, tm_segs, 1,
  	    &tm_nsegs, BUS_DMA_NOWAIT)) {
! 		printf("failed to allocate DMA memory for tile map!\n");
! 		goto fail1;
  	}
  
  	if (bus_dmamem_map(gsc->dmat, tm_segs, tm_nsegs, screen->tm_size,
  	    &screen->tm, BUS_DMA_COHERENT)) {
! 		printf("failed to map DMA memory for tile map!\n");
! 		goto fail2;
  	}
  
  	if (bus_dmamap_load(gsc->dmat, tm_dmamap, screen->tm, screen->tm_size,
  	    NULL, BUS_DMA_NOWAIT)){
  		printf("failed to load DMA map for tilemap\n");
! 		goto fail3;
  	}
  
  	/* 
--- 296,325 ----
  	screen->linebytes = screen->width * screen->depth / 8;
  
  	/* 
! 	 * Setup DMA for tilemap.
  	 */
  	if (bus_dmamap_create(gsc->dmat, screen->tm_size, 1, screen->tm_size,
  	    0, BUS_DMA_NOWAIT, &tm_dmamap)) {
! 		printf("failed to create DMA map for tilemap!\n");
! 		goto fail1;
  	}
  
  	if (bus_dmamem_alloc(gsc->dmat, screen->tm_size, 65536, 0, tm_segs, 1,
  	    &tm_nsegs, BUS_DMA_NOWAIT)) {
! 		printf("failed to allocate DMA memory for tilemap!\n");
! 		goto fail2;
  	}
  
  	if (bus_dmamem_map(gsc->dmat, tm_segs, tm_nsegs, screen->tm_size,
  	    &screen->tm, BUS_DMA_COHERENT)) {
! 		printf("failed to map DMA memory for tilemap!\n");
! 		goto fail3;
  	}
  
  	if (bus_dmamap_load(gsc->dmat, tm_dmamap, screen->tm, screen->tm_size,
  	    NULL, BUS_DMA_NOWAIT)){
  		printf("failed to load DMA map for tilemap\n");
! 		goto fail4;
  	}
  
  	/* 
*************** gbe_attach(struct device *parent, struct device *self,
*** 296,324 ****
  	if (bus_dmamap_create(gsc->dmat, screen->fb_size, 1, screen->fb_size,
  	    0, BUS_DMA_NOWAIT, &fb_dmamap)) {
  		printf("failed to create DMA map for framebuffer!\n");
! 		goto fail4;
  	}
  
  	if (bus_dmamem_alloc(gsc->dmat, screen->fb_size, 65536, 0, fb_segs, 
  	    1, &fb_nsegs, BUS_DMA_NOWAIT)) {
  		printf("failed to allocate DMA memory for framebuffer!\n");
! 		goto fail5;
  	}
  
  	if (bus_dmamem_map(gsc->dmat, fb_segs, fb_nsegs, screen->fb_size,
  	    &screen->fb, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) {
  		printf("failed to map DMA memory for framebuffer!\n");
! 		goto fail6;
  	}
  
  	if (bus_dmamap_load(gsc->dmat, fb_dmamap, screen->fb, screen->fb_size,
  	    NULL, BUS_DMA_NOWAIT)) {
  		printf("failed to load DMA map for framebuffer\n");
! 		goto fail7;
  	}
  
  	screen->tm_phys = tm_dmamap->dm_segs[0].ds_addr;
  	screen->fb_phys = fb_dmamap->dm_segs[0].ds_addr;
  
  	shutdownhook_establish((void(*)(void *))gbe_disable, self);
  
--- 328,384 ----
  	if (bus_dmamap_create(gsc->dmat, screen->fb_size, 1, screen->fb_size,
  	    0, BUS_DMA_NOWAIT, &fb_dmamap)) {
  		printf("failed to create DMA map for framebuffer!\n");
! 		goto fail5;
  	}
  
  	if (bus_dmamem_alloc(gsc->dmat, screen->fb_size, 65536, 0, fb_segs, 
  	    1, &fb_nsegs, BUS_DMA_NOWAIT)) {
  		printf("failed to allocate DMA memory for framebuffer!\n");
! 		goto fail6;
  	}
  
  	if (bus_dmamem_map(gsc->dmat, fb_segs, fb_nsegs, screen->fb_size,
  	    &screen->fb, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) {
  		printf("failed to map DMA memory for framebuffer!\n");
! 		goto fail7;
  	}
  
  	if (bus_dmamap_load(gsc->dmat, fb_dmamap, screen->fb, screen->fb_size,
  	    NULL, BUS_DMA_NOWAIT)) {
  		printf("failed to load DMA map for framebuffer\n");
! 		goto fail8;
  	}
  
+ 	/* 
+ 	 * Setup DMA for rasops tile.
+ 	 */
+ 	if (bus_dmamap_create(gsc->dmat, GBE_TILE_SIZE, 1, GBE_TILE_SIZE,
+ 	    0, BUS_DMA_NOWAIT, &ro_dmamap)) {
+ 		printf("failed to create DMA map for rasops tile!\n");
+ 		goto fail9;
+ 	}
+ 
+ 	if (bus_dmamem_alloc(gsc->dmat, GBE_TILE_SIZE, 65536, 0, ro_segs, 
+ 	    1, &ro_nsegs, BUS_DMA_NOWAIT)) {
+ 		printf("failed to allocate DMA memory for rasops tile!\n");
+ 		goto fail10;
+ 	}
+ 
+ 	if (bus_dmamem_map(gsc->dmat, ro_segs, ro_nsegs, GBE_TILE_SIZE,
+ 	    &screen->ro, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) {
+ 		printf("failed to map DMA memory for rasops tile!\n");
+ 		goto fail11;
+ 	}
+ 
+ 	if (bus_dmamap_load(gsc->dmat, ro_dmamap, screen->ro, GBE_TILE_SIZE,
+ 	    NULL, BUS_DMA_NOWAIT)) {
+ 		printf("failed to load DMA map for rasops tile\n");
+ 		goto fail12;
+ 	}
+ 
  	screen->tm_phys = tm_dmamap->dm_segs[0].ds_addr;
  	screen->fb_phys = fb_dmamap->dm_segs[0].ds_addr;
+ 	screen->ro_phys = ro_dmamap->dm_segs[0].ds_addr;
  
  	shutdownhook_establish((void(*)(void *))gbe_disable, self);
  
*************** gbe_attach(struct device *parent, struct device *self,
*** 331,336 ****
--- 391,399 ----
  	if (screen->depth == 8)
  		gbe_loadcmap(screen, 0, 255);
  
+ 	/* Clear framebuffer. */
+ 	gbe_fillrect(gsc, 0, 0, screen->width, screen->height, 0);
+ 
  	printf("rev %u, %iMB, %dx%d at %d bits\n", gsc->rev,
  	    screen->fb_size >> 20, screen->width, screen->height,
  	    screen->depth);
*************** gbe_attach(struct device *parent, struct device *self,
*** 354,373 ****
  
  	return;
  
! fail7:
  	bus_dmamem_unmap(gsc->dmat, screen->fb, screen->fb_size);
! fail6:
  	bus_dmamem_free(gsc->dmat, fb_segs, fb_nsegs);
! fail5:
  	bus_dmamap_destroy(gsc->dmat, fb_dmamap);
! fail4:
  	bus_dmamap_unload(gsc->dmat, tm_dmamap);
! fail3:
  	bus_dmamem_unmap(gsc->dmat, screen->tm, screen->tm_size);
! fail2:
  	bus_dmamem_free(gsc->dmat, tm_segs, tm_nsegs);
! fail1:
  	bus_dmamap_destroy(gsc->dmat, tm_dmamap);
  fail0:
  	bus_space_unmap(gsc->iot, gsc->ioh, GBE_REG_SIZE);
  }
--- 417,446 ----
  
  	return;
  
! fail12:
! 	bus_dmamem_unmap(gsc->dmat, screen->ro, GBE_TILE_SIZE);
! fail11:
! 	bus_dmamem_free(gsc->dmat, ro_segs, ro_nsegs);
! fail10:
! 	bus_dmamap_destroy(gsc->dmat, ro_dmamap);
! fail9:
! 	bus_dmamap_unload(gsc->dmat, fb_dmamap);
! fail8:
  	bus_dmamem_unmap(gsc->dmat, screen->fb, screen->fb_size);
! fail7:
  	bus_dmamem_free(gsc->dmat, fb_segs, fb_nsegs);
! fail6:
  	bus_dmamap_destroy(gsc->dmat, fb_dmamap);
! fail5:
  	bus_dmamap_unload(gsc->dmat, tm_dmamap);
! fail4:
  	bus_dmamem_unmap(gsc->dmat, screen->tm, screen->tm_size);
! fail3:
  	bus_dmamem_free(gsc->dmat, tm_segs, tm_nsegs);
! fail2:
  	bus_dmamap_destroy(gsc->dmat, tm_dmamap);
+ fail1:
+ 	bus_space_unmap(gsc->iot, gsc->re_ioh, RE_REG_SIZE);
  fail0:
  	bus_space_unmap(gsc->iot, gsc->ioh, GBE_REG_SIZE);
  }
*************** gbe_init_screen(struct gbe_screen *screen)
*** 383,393 ****
  	int i;
  
  	/*
! 	 * Initialise rasops.
  	 */
  	memset(&screen->ri, 0, sizeof(struct rasops_info));
  
! 	screen->ri.ri_flg = RI_CENTER | RI_CLEAR;
  	screen->ri.ri_depth = screen->depth;
  	screen->ri.ri_width = screen->width;
  	screen->ri.ri_height = screen->height;
--- 456,469 ----
  	int i;
  
  	/*
! 	 * Initialise screen.
  	 */
+ 	screen->mode = WSDISPLAYIO_MODE_EMUL;
+ 
+ 	/* Initialise rasops. */
  	memset(&screen->ri, 0, sizeof(struct rasops_info));
  
! 	screen->ri.ri_flg = RI_CENTER;
  	screen->ri.ri_depth = screen->depth;
  	screen->ri.ri_width = screen->width;
  	screen->ri.ri_height = screen->height;
*************** gbe_init_screen(struct gbe_screen *screen)
*** 412,417 ****
--- 488,515 ----
  
  	rasops_init(&screen->ri, screen->height / 8, screen->width / 8);
  
+ 	/* Create a rasops instance that can draw into a single tile. */
+ 	memcpy(&screen->ri_tile, &screen->ri, sizeof(struct rasops_info));
+ 	screen->ri_tile.ri_flg = 0;
+ 	screen->ri_tile.ri_width = GBE_TILE_WIDTH >> (screen->depth >> 4);
+ 	screen->ri_tile.ri_height = GBE_TILE_HEIGHT;
+ 	screen->ri_tile.ri_stride = screen->ri_tile.ri_width *
+ 	    screen->depth / 8;
+ 	screen->ri_tile.ri_xorigin = 0;
+ 	screen->ri_tile.ri_yorigin = 0;
+ 	screen->ri_tile.ri_bits = screen->ro;
+ 	screen->ri_tile.ri_origbits = screen->ro;
+ 	screen->ro_curpos = 0;
+ 
+ 	screen->ri.ri_hw = screen->sc;
+ 
+ 	screen->ri.ri_do_cursor = gbe_do_cursor;
+ 	screen->ri.ri_ops.putchar = gbe_putchar;
+ 	screen->ri.ri_ops.copyrows = gbe_copyrows;
+ 	screen->ri.ri_ops.copycols = gbe_copycols;
+ 	screen->ri.ri_ops.eraserows = gbe_eraserows;
+ 	screen->ri.ri_ops.erasecols = gbe_erasecols;
+ 
  	gbe_stdscreen.ncols = screen->ri.ri_cols;
  	gbe_stdscreen.nrows = screen->ri.ri_rows;
  	gbe_stdscreen.textops = &screen->ri.ri_ops;
*************** gbe_init_screen(struct gbe_screen *screen)
*** 420,434 ****
  	gbe_stdscreen.capabilities = screen->ri.ri_caps;
  
  	/*
! 	 * Map framebuffer into tile map. Each entry in the tilemap is 16 bits 
  	 * wide. Each tile is 64KB or 2^16 bits, hence the last 16 bits of the 
  	 * address will range from 0x0000 to 0xffff. As a result we simply 
  	 * discard the lower 16 bits and store bits 17 through 32 as an entry
  	 * in the tilemap.
  	 */
  	tm = (void *)screen->tm;
! 	for (i = 0; i < (screen->fb_size >> GBE_TILE_SHIFT); i++)
! 	    tm[i] = (screen->fb_phys >> GBE_TILE_SHIFT) + i;
  }
  
  void
--- 518,533 ----
  	gbe_stdscreen.capabilities = screen->ri.ri_caps;
  
  	/*
! 	 * Map framebuffer into tilemap. Each entry in the tilemap is 16 bits 
  	 * wide. Each tile is 64KB or 2^16 bits, hence the last 16 bits of the 
  	 * address will range from 0x0000 to 0xffff. As a result we simply 
  	 * discard the lower 16 bits and store bits 17 through 32 as an entry
  	 * in the tilemap.
  	 */
  	tm = (void *)screen->tm;
! 	for (i = 0; i < (screen->fb_size >> GBE_TILE_SHIFT) &&
! 	    i < GBE_TLB_SIZE; i++)
! 		tm[i] = (screen->fb_phys >> GBE_TILE_SHIFT) + i;
  }
  
  void
*************** gbe_enable(struct gbe_softc *gsc)
*** 462,468 ****
  	if (i == 10000)
  		printf("timeout unfreezing pixel counter!\n");
  
! 	/* Provide GBE with address of tile map and enable DMA. */
  	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_CTRL, 
  	    ((screen->tm_phys >> 9) << 
  	    GBE_FB_CTRL_TILE_PTR_SHIFT) | GBE_FB_CTRL_DMA_ENABLE);
--- 561,567 ----
  	if (i == 10000)
  		printf("timeout unfreezing pixel counter!\n");
  
! 	/* Provide GBE with address of tilemap and enable DMA. */
  	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_CTRL, 
  	    ((screen->tm_phys >> 9) << 
  	    GBE_FB_CTRL_TILE_PTR_SHIFT) | GBE_FB_CTRL_DMA_ENABLE);
*************** void
*** 564,572 ****
  gbe_setup(struct gbe_softc *gsc)
  {
  	struct gbe_screen *screen = gsc->curscr;
! 	uint32_t val;
! 	int i, cmode;
  	u_char *colour;
  
  	/*
  	 * Setup framebuffer.
--- 663,673 ----
  gbe_setup(struct gbe_softc *gsc)
  {
  	struct gbe_screen *screen = gsc->curscr;
! 	int i, t, cmode, tile_width, tiles_x, tiles_y;
  	u_char *colour;
+ 	uint16_t *tm;
+ 	uint32_t val;
+ 	uint64_t reg;
  
  	/*
  	 * Setup framebuffer.
*************** gbe_setup(struct gbe_softc *gsc)
*** 574,598 ****
  	switch (screen->depth) {
  	case 32:
  		cmode = GBE_CMODE_RGB8;
  		break;
  	case 16:
  		cmode = GBE_CMODE_ARGB5;
  		break;
  	case 8:
  	default:
  		cmode = GBE_CMODE_I8;
  		break;
  	}
  
! 	/* Trick framebuffer into linear mode. */
! 	i = screen->width * screen->height / (512 / (screen->depth >> 3));
! 	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_PIXEL, 
! 	    i << GBE_FB_SIZE_PIXEL_HEIGHT_SHIFT);
  
! 	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE,
! 	    (1 << GBE_FB_SIZE_TILE_WIDTH_SHIFT) | 
! 	    ((screen->depth >> 4) << GBE_FB_SIZE_TILE_DEPTH_SHIFT));
! 	
  	val = (cmode << GBE_WID_MODE_SHIFT) | GBE_BMODE_BOTH;
  	for (i = 0; i < (32 * 4); i += 4)
  		bus_space_write_4(gsc->iot, gsc->ioh, GBE_MODE + i, val);
--- 675,743 ----
  	switch (screen->depth) {
  	case 32:
  		cmode = GBE_CMODE_RGB8;
+ 		screen->bufmode = COLOUR_DEPTH_32 << BUFMODE_BUFDEPTH_SHIFT |
+ 		    PIXEL_TYPE_RGB << BUFMODE_PIXTYPE_SHIFT |
+ 		    COLOUR_DEPTH_32 << BUFMODE_PIXDEPTH_SHIFT;
  		break;
  	case 16:
  		cmode = GBE_CMODE_ARGB5;
+ 		screen->bufmode = COLOUR_DEPTH_16 << BUFMODE_BUFDEPTH_SHIFT |
+ 		    PIXEL_TYPE_RGBA << BUFMODE_PIXTYPE_SHIFT |
+ 		    COLOUR_DEPTH_16 << BUFMODE_PIXDEPTH_SHIFT;
  		break;
  	case 8:
  	default:
  		cmode = GBE_CMODE_I8;
+ 		screen->bufmode = COLOUR_DEPTH_8 << BUFMODE_BUFDEPTH_SHIFT |
+ 		    PIXEL_TYPE_CI << BUFMODE_PIXTYPE_SHIFT |
+ 		    COLOUR_DEPTH_8 << BUFMODE_PIXDEPTH_SHIFT;
  		break;
  	}
  
! 	/* Calculate tile width in bytes and screen size in tiles. */
! 	tile_width = GBE_TILE_WIDTH >> (screen->depth >> 4);
! 	tiles_x = (screen->width + tile_width - 1) >>
! 	    (GBE_TILE_WIDTH_SHIFT - (screen->depth >> 4));
! 	tiles_y = (screen->height + GBE_TILE_HEIGHT - 1) >>
! 	    GBE_TILE_HEIGHT_SHIFT;
  
! 	if (screen->mode != WSDISPLAYIO_MODE_EMUL) {
! 
! 		/*
! 		 * Setup the framebuffer in "linear" mode. We trick the
! 		 * framebuffer into linear mode by telling it that it is one
! 		 * tile wide and specifying an adjusted framebuffer height.
! 		 */ 
! 
! 		bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE,
! 		    ((screen->depth >> 4) << GBE_FB_SIZE_TILE_DEPTH_SHIFT) |
! 		    (1 << GBE_FB_SIZE_TILE_WIDTH_SHIFT));
! 
! 		bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_PIXEL, 
! 		    (screen->width * screen->height / tile_width) <<
! 		        GBE_FB_SIZE_PIXEL_HEIGHT_SHIFT);
! 
! 	} else {
! 
! 		/*
! 		 * Setup the framebuffer in tiled mode. Provide the tile
! 		 * colour depth, screen width in whole and partial tiles,
! 		 * and the framebuffer height in pixels.
! 		 */
! 
! 		bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE,
! 		    ((screen->depth >> 4) << GBE_FB_SIZE_TILE_DEPTH_SHIFT) |
! 		    ((screen->width / tile_width) <<
! 		        GBE_FB_SIZE_TILE_WIDTH_SHIFT) |
! 		    ((screen->width % tile_width != 0) ?
! 		        (screen->height / GBE_TILE_HEIGHT) : 0));
! 
! 		bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_PIXEL, 
! 		    screen->height << GBE_FB_SIZE_PIXEL_HEIGHT_SHIFT);
! 
! 	}
! 
! 	/* Set colour mode registers. */
  	val = (cmode << GBE_WID_MODE_SHIFT) | GBE_BMODE_BOTH;
  	for (i = 0; i < (32 * 4); i += 4)
  		bus_space_write_4(gsc->iot, gsc->ioh, GBE_MODE + i, val);
*************** gbe_setup(struct gbe_softc *gsc)
*** 623,630 ****
--- 768,825 ----
  		    GBE_GMAP + i * sizeof(u_int32_t),
  		    (i << 24) | (i << 16) | (i << 8));
  
+ 	/*
+ 	 * Initialise the rendering engine.
+ 	 */
+ 	val = screen->mode | BUF_TYPE_TLB_A << BUFMODE_BUFTYPE_SHIFT;
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_BUFMODE_SRC, val);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_BUFMODE_DST, val);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_CLIPMODE, 0);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_COLOUR_MASK, 0xffffffff);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PIXEL_XFER_X_STEP, 1);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PIXEL_XFER_Y_STEP, 1);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_WINOFFSET_DST, 0);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_WINOFFSET_SRC, 0);
+ 
+ 	/*
+ 	 * Load framebuffer tiles into TLB A. Each TLB consists of a 16x16
+ 	 * tile array representing 2048x2048 pixels. Each entry in the TLB
+ 	 * consists of four 16-bit entries which represent bits 17:32 of the
+ 	 * 64KB tile address. As a result, we can make use of the tilemap
+ 	 * which already stores tile entries in the same format.
+ 	 */
+ 	tm = (void *)screen->tm;
+ 	for (i = 0, t = 0; i < GBE_TLB_SIZE; i++) {
+ 		reg <<= 16;
+ 		if (i % 16 < tiles_x)
+ 			reg |= (tm[t++] | 0x8000);
+ 		if (i % 4 == 3)
+ 			bus_space_write_8(gsc->iot, gsc->re_ioh,
+ 			    RE_TLB_A + (i >> 2) * 8, reg);
+ 	}
+ 
+ 	/* Load single tile into TLB B for rasops. */
+ 	bus_space_write_8(gsc->iot, gsc->re_ioh,
+ 	    RE_TLB_B, (screen->ro_phys >> 16 | 0x8000) << 48);
  }
  
+ void
+ gbe_wait_re_idle(struct gbe_softc *gsc)
+ {
+ 	int i;
+ 
+ 	/* Wait until rendering engine is idle. */
+ 	for (i = 0; i < 100000; i++) {
+ 		if (bus_space_read_4(gsc->iot, gsc->re_ioh, RE_PP_STATUS) &
+ 		    RE_PP_STATUS_IDLE)
+ 			break; 
+ 		delay(1);
+ 	}
+ 	if (i == 100000)
+ 		printf("%s: rendering engine did not become idle!\n",
+ 		    gsc->sc_dev.dv_xname);
+ }
+ 
  /*
   * Interfaces for wscons.
   */
*************** int
*** 633,639 ****
  gbe_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
  {
  	struct gbe_screen *screen = (struct gbe_screen *)v;
! 	int rc;
  
  	switch (cmd) {
  	case WSDISPLAYIO_GTYPE:
--- 828,834 ----
  gbe_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
  {
  	struct gbe_screen *screen = (struct gbe_screen *)v;
! 	int rc, mode;
  
  	switch (cmd) {
  	case WSDISPLAYIO_GTYPE:
*************** gbe_ioctl(void *v, u_long cmd, caddr_t data, int flags
*** 678,683 ****
--- 873,901 ----
  		}
  		break;
  
+ 	case WSDISPLAYIO_GMODE:
+ 		*(u_int *)data = screen->mode;
+ 		break;
+ 
+ 	case WSDISPLAYIO_SMODE:
+ 		mode = *(u_int *)data;
+ 		if (mode == WSDISPLAYIO_MODE_EMUL ||
+ 		    mode == WSDISPLAYIO_MODE_MAPPED ||
+ 		    mode == WSDISPLAYIO_MODE_DUMBFB) {
+ 
+ 			screen->mode = mode;
+ 
+ 			gbe_disable((struct gbe_softc *)screen->sc);
+ 			gbe_setup((struct gbe_softc *)screen->sc);
+ 			gbe_enable((struct gbe_softc *)screen->sc);
+ 
+ 			/* Clear framebuffer if entering emulated mode. */
+ 			if (screen->mode == WSDISPLAYIO_MODE_EMUL)
+ 				gbe_fillrect((struct gbe_softc *)screen->sc,
+ 				    0, 0, screen->width, screen->height, 0);
+ 		}
+ 		break;
+ 
  	case WSDISPLAYIO_GVIDEO:
  	case WSDISPLAYIO_SVIDEO:
  		/* Handled by the upper layer. */
*************** gbe_loadcmap(struct gbe_screen *screen, u_int start, u
*** 824,829 ****
--- 1042,1256 ----
  }
  
  /*
+  * Hardware accelerated functions for rasops.
+  */
+ 
+ void
+ gbe_rop(struct gbe_softc *gsc, int x, int y, int w, int h, int op)
+ {
+ 
+ 	gbe_wait_re_idle(gsc);
+ 
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PRIMITIVE,
+ 	    PRIMITIVE_RECTANGLE | PRIMITIVE_LRTB);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_DRAWMODE,
+ 	    DRAWMODE_BITMASK | DRAWMODE_BYTEMASK | DRAWMODE_PIXEL_XFER |
+ 	    DRAWMODE_LOGIC_OP);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_LOGIC_OP, op);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PIXEL_XFER_SRC,
+ 	    (x << 16) | (y & 0xffff));
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_0,
+ 	    (x << 16) | (y & 0xffff));
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_1 | RE_START,
+ 	    ((x + w - 1) << 16) | ((y + h - 1) & 0xffff));
+ }
+ 
+ void
+ gbe_copyrect(struct gbe_softc *gsc, int src, int sx, int sy, int dx, int dy,
+     int w, int h)
+ {
+ 	int direction, x0, y0, x1, y1;
+ 
+ 	if (sx >= dx && sy >= dy) {
+ 		direction = PRIMITIVE_LRTB;
+ 		x0 = dx;
+ 		y0 = dy;
+ 		x1 = dx + w - 1;
+ 		y1 = dy + h - 1;
+ 	} else if (sx >= dx && sy < dy) {
+ 		direction = PRIMITIVE_LRBT;
+ 		sy = sy + h - 1;
+ 		x0 = dx;
+ 		y0 = dy + h - 1;
+ 		x1 = dx + w - 1;
+ 		y1 = dy;
+ 	} else if (sx < dx && sy >= dy) {
+ 		direction = PRIMITIVE_RLTB;
+ 		sx = sx + w - 1;
+ 		x0 = dx + w - 1;
+ 		y0 = dy;
+ 		x1 = dx;
+ 		y1 = dy + h - 1;
+ 	} else if (sx < dx && sy < dy) {
+ 		direction = PRIMITIVE_RLBT;
+ 		sy = sy + h - 1;
+ 		sx = sx + w - 1;
+ 		x0 = dx + w - 1;
+ 		y0 = dy + h - 1;
+ 		x1 = dx;
+ 		y1 = dy;
+ 	}
+ 
+ 	gbe_wait_re_idle(gsc);
+ 
+ 	if (src != BUF_TYPE_TLB_A) 
+ 		bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_BUFMODE_SRC,
+ 		    gsc->curscr->bufmode | (src << BUFMODE_BUFTYPE_SHIFT));
+ 
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PRIMITIVE,
+ 	    PRIMITIVE_RECTANGLE | direction);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_DRAWMODE,
+ 	    DRAWMODE_BITMASK | DRAWMODE_BYTEMASK | DRAWMODE_PIXEL_XFER);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PIXEL_XFER_SRC,
+ 	    (sx << 16) | (sy & 0xffff));
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_0,
+ 	    (x0 << 16) | (y0 & 0xffff));
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_1 | RE_START,
+ 	    (x1 << 16) | (y1 & 0xffff));
+ 
+ 	if (src != BUF_TYPE_TLB_A) {
+ 		gbe_wait_re_idle(gsc);
+ 		bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_BUFMODE_SRC,
+ 		    gsc->curscr->bufmode |
+ 		    (BUF_TYPE_TLB_A << BUFMODE_BUFTYPE_SHIFT));
+ 	}
+ }
+ 
+ void
+ gbe_fillrect(struct gbe_softc *gsc, int x, int y, int w, int h, int bg)
+ {
+ 
+ 	gbe_wait_re_idle(gsc);
+ 
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PRIMITIVE,
+ 	    PRIMITIVE_RECTANGLE | PRIMITIVE_LRTB);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_DRAWMODE,
+ 	    DRAWMODE_BITMASK | DRAWMODE_BYTEMASK);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_SHADE_FG_COLOUR, bg);
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_0,
+ 	    (x << 16) | (y & 0xffff));
+ 	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_1 | RE_START,
+ 	    ((x + w - 1) << 16) | ((y + h - 1) & 0xffff));
+ }
+ 
+ void
+ gbe_do_cursor(struct rasops_info *ri)
+ {
+ 	struct gbe_softc *sc = ri->ri_hw;
+ 	int y, x, w, h;
+ 
+ 	w = ri->ri_font->fontwidth;
+ 	h = ri->ri_font->fontheight;
+ 	x = ri->ri_xorigin + ri->ri_ccol * w;
+ 	y = ri->ri_yorigin + ri->ri_crow * h;
+ 
+ 	gbe_rop(sc, x, y, w, h, LOGIC_OP_XOR);
+ }
+ 
+ void
+ gbe_putchar(void *cookie, int row, int col, u_int uc, long attr)
+ {
+ 	struct rasops_info *ri = cookie;
+ 	struct gbe_softc *gsc = ri->ri_hw;
+ 	struct gbe_screen *screen = gsc->curscr;
+ 	struct rasops_info *ri_tile = &screen->ri_tile;
+ 	int x, y, w, h;
+ 
+ 	w = ri->ri_font->fontwidth;
+ 	h = ri->ri_font->fontheight;
+ 	x = ri->ri_xorigin + col * w;
+ 	y = ri->ri_yorigin + row * h;
+ 
+ 	ri_tile->ri_ops.putchar(ri_tile, 0, screen->ro_curpos, uc, attr);
+ 
+ 	gbe_copyrect(gsc, BUF_TYPE_TLB_B, screen->ro_curpos * w, 0, x, y, w, h);
+ 
+ 	screen->ro_curpos++;
+ 	if ((screen->ro_curpos + 1) * w > screen->ri_tile.ri_width)
+ 		screen->ro_curpos = 0;
+ }
+ 
+ void
+ gbe_copycols(void *cookie, int row, int src, int dst, int num)
+ {
+ 	struct rasops_info *ri = cookie;
+ 	struct gbe_softc *sc = ri->ri_hw;
+ 
+ 	num *= ri->ri_font->fontwidth;
+ 	src *= ri->ri_font->fontwidth;
+ 	dst *= ri->ri_font->fontwidth;
+ 	row *= ri->ri_font->fontheight;
+ 
+ 	gbe_copyrect(sc, BUF_TYPE_TLB_A, ri->ri_xorigin + src,
+ 	    ri->ri_yorigin + row, ri->ri_xorigin + dst, ri->ri_yorigin + row,
+ 	    num, ri->ri_font->fontheight);
+ }
+ 
+ void
+ gbe_erasecols(void *cookie, int row, int col, int num, long attr)
+ {
+ 	struct rasops_info *ri = cookie;
+ 	struct gbe_softc *sc = ri->ri_hw;
+ 	int bg, fg;
+ 
+ 	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);
+ 
+ 	row *= ri->ri_font->fontheight;
+ 	col *= ri->ri_font->fontwidth;
+ 	num *= ri->ri_font->fontwidth;
+ 
+ 	gbe_fillrect(sc, ri->ri_xorigin + col, ri->ri_yorigin + row,
+ 	    num, ri->ri_font->fontheight, ri->ri_devcmap[bg]);
+ }
+ 
+ void
+ gbe_copyrows(void *cookie, int src, int dst, int num)
+ {
+ 	struct rasops_info *ri = cookie;
+ 	struct gbe_softc *sc = ri->ri_hw;
+ 	
+ 	num *= ri->ri_font->fontheight;
+ 	src *= ri->ri_font->fontheight;
+ 	dst *= ri->ri_font->fontheight;
+ 
+ 	gbe_copyrect(sc, BUF_TYPE_TLB_A, ri->ri_xorigin, ri->ri_yorigin + src,
+ 	    ri->ri_xorigin, ri->ri_yorigin + dst, ri->ri_emuwidth, num);
+ }
+ 
+ void
+ gbe_eraserows(void *cookie, int row, int num, long attr)
+ {
+ 	struct rasops_info *ri = cookie;
+ 	struct gbe_softc *sc = ri->ri_hw;
+ 	int x, y, w, bg, fg;
+ 
+ 	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);
+ 
+ 	if ((num == ri->ri_rows) && ISSET(ri->ri_flg, RI_FULLCLEAR)) {
+ 		num = ri->ri_height;
+ 		x = y = 0;
+ 		w = ri->ri_width;
+ 	} else {
+ 		num *= ri->ri_font->fontheight;
+ 		x = ri->ri_xorigin;
+ 		y = ri->ri_yorigin + row * ri->ri_font->fontheight;
+ 		w = ri->ri_emuwidth;
+ 	}
+ 
+ 	gbe_fillrect(sc, x, y, w, num, ri->ri_devcmap[bg]);
+ }
+ 
+ /*
   * Console functions for early display.
   */
  
*************** gbe_cnprobe(bus_space_tag_t iot, bus_addr_t addr)
*** 852,880 ****
  int
  gbe_cnattach(bus_space_tag_t iot, bus_addr_t addr)
  {
! 	struct gbe_softc gsc;
! 	vaddr_t va;
! 	paddr_t pa;
  	uint32_t val;
  	long attr;
  
  	/*
  	 * Setup GBE for use as early console.
  	 */
! 	gsc.curscr = &gbe_consdata;
! 	gbe_consdata.sc = (void *)&gsc;
  	
  	/* Setup bus space mapping. */
! 	gsc.iot = iot;
! 	gsc.ioh = PHYS_TO_UNCACHED(addr);
  
  	/* Determine GBE revision. */
! 	gsc.rev = bus_space_read_4(gsc.iot, gsc.ioh, GBE_CTRL_STAT) & 0xf;
  
  	/* Determine resolution configured by firmware. */
! 	val = bus_space_read_4(gsc.iot, gsc.ioh, GBE_VT_HCMAP);
  	gbe_consdata.width = (val >> GBE_VT_HCMAP_ON_SHIFT) & 0xfff;
! 	val = bus_space_read_4(gsc.iot, gsc.ioh, GBE_VT_VCMAP);
  	gbe_consdata.height = (val >> GBE_VT_VCMAP_ON_SHIFT) & 0xfff;
  
  	/* Ensure that the firmware has setup the device. */
--- 1279,1310 ----
  int
  gbe_cnattach(bus_space_tag_t iot, bus_addr_t addr)
  {
! 	struct gbe_softc *gsc;
  	uint32_t val;
+ 	paddr_t pa;
+ 	vaddr_t va;
  	long attr;
  
  	/*
  	 * Setup GBE for use as early console.
  	 */
! 	va = pmap_steal_memory(sizeof(struct gbe_softc), NULL, NULL);
! 	gsc = (struct gbe_softc *)va;
! 	gsc->curscr = &gbe_consdata;
! 	gbe_consdata.sc = (struct device *)gsc;
  	
  	/* Setup bus space mapping. */
! 	gsc->iot = iot;
! 	gsc->ioh = PHYS_TO_UNCACHED(addr);
! 	gsc->re_ioh = PHYS_TO_UNCACHED(RE_BASE);
  
  	/* Determine GBE revision. */
! 	gsc->rev = bus_space_read_4(gsc->iot, gsc->ioh, GBE_CTRL_STAT) & 0xf;
  
  	/* Determine resolution configured by firmware. */
! 	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_HCMAP);
  	gbe_consdata.width = (val >> GBE_VT_HCMAP_ON_SHIFT) & 0xfff;
! 	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_VCMAP);
  	gbe_consdata.height = (val >> GBE_VT_VCMAP_ON_SHIFT) & 0xfff;
  
  	/* Ensure that the firmware has setup the device. */
*************** gbe_cnattach(bus_space_tag_t iot, bus_addr_t addr)
*** 903,919 ****
  	gbe_consdata.fb_phys = ((pa >> 16) + 1) << 16;
  	gbe_consdata.fb = (caddr_t)PHYS_TO_UNCACHED(gbe_consdata.fb_phys);
  
  	/*
  	 * Setup GBE hardware.
  	 */
  	gbe_init_screen(&gbe_consdata);
! 	gbe_disable(&gsc);
! 	gbe_setup(&gsc);
! 	gbe_enable(&gsc);
  
  	/* Load colourmap if required. */
  	if (gbe_consdata.depth == 8)
  		gbe_loadcmap(&gbe_consdata, 0, 255);
  
  	/*
  	 * Attach wsdisplay.
--- 1333,1360 ----
  	gbe_consdata.fb_phys = ((pa >> 16) + 1) << 16;
  	gbe_consdata.fb = (caddr_t)PHYS_TO_UNCACHED(gbe_consdata.fb_phys);
  
+ 	/* 
+ 	 * Steal memory for rasops tile - 64KB aligned and coherent.
+ 	 */
+ 	va = pmap_steal_memory(GBE_TILE_SIZE + 65536, NULL, NULL);
+ 	pmap_extract(pmap_kernel(), va, &pa);
+ 	gbe_consdata.ro_phys = ((pa >> 16) + 1) << 16;
+ 	gbe_consdata.ro = (caddr_t)PHYS_TO_UNCACHED(gbe_consdata.ro_phys);
+ 
  	/*
  	 * Setup GBE hardware.
  	 */
  	gbe_init_screen(&gbe_consdata);
! 	gbe_disable(gsc);
! 	gbe_setup(gsc);
! 	gbe_enable(gsc);
  
  	/* Load colourmap if required. */
  	if (gbe_consdata.depth == 8)
  		gbe_loadcmap(&gbe_consdata, 0, 255);
+ 
+ 	/* Clear framebuffer. */
+ 	gbe_fillrect(gsc, 0, 0, gbe_consdata.width, gbe_consdata.height, 0);
  
  	/*
  	 * Attach wsdisplay.
Index: src/sys/arch/sparc/dev/amd7930.c
===================================================================
RCS file: /cvs/src/sys/arch/sparc/dev/Attic/amd7930.c,v
retrieving revision 1.31
retrieving revision 1.32
diff -c -p -r1.31 -r1.32
*** src/sys/arch/sparc/dev/amd7930.c	21 Apr 2008 00:32:42 -0000	1.31
--- src/sys/arch/sparc/dev/amd7930.c	10 Apr 2009 20:53:51 -0000	1.32
***************
*** 1,4 ****
! /*	$OpenBSD: amd7930.c,v 1.31 2008/04/21 00:32:42 jakemsr Exp $	*/
  /*	$NetBSD: amd7930.c,v 1.37 1998/03/30 14:23:40 pk Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: amd7930.c,v 1.32 2009/04/10 20:53:51 miod Exp $	*/
  /*	$NetBSD: amd7930.c,v 1.37 1998/03/30 14:23:40 pk Exp $	*/
  
  /*
*************** int     amd7930debug = 0;
*** 66,72 ****
   */
  struct amd7930_softc {
  	struct	device sc_dev;		/* base device */
- 	struct	intrhand sc_swih;	/* software interrupt vector */
  
  	int	sc_open;		/* single use device */
  	int	sc_locked;		/* true when transferring data */
--- 66,71 ----
*************** struct amd7930_softc {
*** 86,110 ****
          /* sc_au is special in that the hardware interrupt handler uses it */
          struct  auio sc_au;		/* recv and xmit buffers, etc */
  #define	sc_hwih	sc_au.au_ih		/* hardware interrupt vector */
  };
  
- /* interrupt interfaces */
- #if defined(SUN4M)
- #define AUDIO_SET_SWINTR do {		\
- 	if (CPU_ISSUN4M)		\
- 		raise(0, 4);		\
- 	else				\
- 		ienab_bis(IE_L4);	\
- } while(0);
- #else
- #define AUDIO_SET_SWINTR ienab_bis(IE_L4)
- #endif /* defined(SUN4M) */
- 
  #ifndef AUDIO_C_HANDLER
  struct auio *auiop;
  #endif /* AUDIO_C_HANDLER */
  int	amd7930hwintr(void *);
! int	amd7930swintr(void *);
  
  /* forward declarations */
  void	audio_setmap(volatile struct amd7930 *, struct mapreg *);
--- 85,98 ----
          /* sc_au is special in that the hardware interrupt handler uses it */
          struct  auio sc_au;		/* recv and xmit buffers, etc */
  #define	sc_hwih	sc_au.au_ih		/* hardware interrupt vector */
+ #define	sc_swih	sc_au.au_swih		/* software interrupt cookie */
  };
  
  #ifndef AUDIO_C_HANDLER
  struct auio *auiop;
  #endif /* AUDIO_C_HANDLER */
  int	amd7930hwintr(void *);
! void	amd7930swintr(void *);
  
  /* forward declarations */
  void	audio_setmap(volatile struct amd7930 *, struct mapreg *);
*************** amd7930attach(parent, self, args)
*** 325,334 ****
  		intr_establish(pri, &sc->sc_hwih, IPL_AUHARD,
  		    sc->sc_dev.dv_xname);
  	}
! 	sc->sc_swih.ih_fun = amd7930swintr;
! 	sc->sc_swih.ih_arg = sc;
! 	intr_establish(IPL_AUSOFT, &sc->sc_swih, IPL_AUSOFT,
! 	    sc->sc_dev.dv_xname);
  
  	audio_attach_mi(&sa_hw_if, sc, &sc->sc_dev);
  	amd7930_commit_settings(sc);
--- 313,319 ----
  		intr_establish(pri, &sc->sc_hwih, IPL_AUHARD,
  		    sc->sc_dev.dv_xname);
  	}
! 	sc->sc_swih = softintr_establish(IPL_AUSOFT, amd7930swintr, sc);
  
  	audio_attach_mi(&sa_hw_if, sc, &sc->sc_dev);
  	amd7930_commit_settings(sc);
*************** amd7930hwintr(au0)
*** 809,815 ****
  		        if (amd7930debug > 1)
                  		printf("amd7930hwintr: swintr(r) requested");
  #endif
! 			AUDIO_SET_SWINTR;
  		}
  	}
  
--- 794,800 ----
  		        if (amd7930debug > 1)
                  		printf("amd7930hwintr: swintr(r) requested");
  #endif
! 			softintr_schedule(au->au_swih);
  		}
  	}
  
*************** amd7930hwintr(au0)
*** 824,843 ****
  		        if (amd7930debug > 1)
                  		printf("amd7930hwintr: swintr(p) requested");
  #endif
! 			AUDIO_SET_SWINTR;
  		}
  	}
  
  	return (-1);
  }
  
! int
  amd7930swintr(sc0)
  	void *sc0;
  {
! 	register struct amd7930_softc *sc = sc0;
! 	register struct auio *au;
! 	register int s, ret = 0;
  
  #ifdef AUDIO_DEBUG
  	if (amd7930debug > 1)
--- 809,828 ----
  		        if (amd7930debug > 1)
                  		printf("amd7930hwintr: swintr(p) requested");
  #endif
! 			softintr_schedule(au->au_swih);
  		}
  	}
  
  	return (-1);
  }
  
! void
  amd7930swintr(sc0)
  	void *sc0;
  {
! 	struct amd7930_softc *sc = sc0;
! 	struct auio *au;
! 	int s;
  
  #ifdef AUDIO_DEBUG
  	if (amd7930debug > 1)
*************** amd7930swintr(sc0)
*** 848,864 ****
  	s = splaudio();
  	if (au->au_rdata > au->au_rend && sc->sc_rintr != NULL) {
  		splx(s);
- 		ret = 1;
  		(*sc->sc_rintr)(sc->sc_rarg);
  		s = splaudio();
  	}
  	if (au->au_pdata > au->au_pend && sc->sc_pintr != NULL) {
  		splx(s);
- 		ret = 1;
  		(*sc->sc_pintr)(sc->sc_parg);
  	} else
  		splx(s);
- 	return (ret);
  }
  
  #ifndef AUDIO_C_HANDLER
--- 833,846 ----
Index: src/sys/arch/sparc/dev/fd.c
===================================================================
RCS file: /cvs/src/sys/arch/sparc/dev/fd.c,v
retrieving revision 1.63
retrieving revision 1.64
diff -c -p -r1.63 -r1.64
*** src/sys/arch/sparc/dev/fd.c	15 Oct 2008 19:12:19 -0000	1.63
--- src/sys/arch/sparc/dev/fd.c	10 Apr 2009 20:53:51 -0000	1.64
***************
*** 1,4 ****
! /*	$OpenBSD: fd.c,v 1.63 2008/10/15 19:12:19 blambert Exp $	*/
  /*	$NetBSD: fd.c,v 1.51 1997/05/24 20:16:19 pk Exp $	*/
  
  /*-
--- 1,4 ----
! /*	$OpenBSD: fd.c,v 1.64 2009/04/10 20:53:51 miod Exp $	*/
  /*	$NetBSD: fd.c,v 1.51 1997/05/24 20:16:19 pk Exp $	*/
  
  /*-
*************** enum fdc_state {
*** 138,144 ****
  /* software state, per controller */
  struct fdc_softc {
  	struct device	sc_dev;		/* boilerplate */
! 	struct intrhand sc_sih;
  	caddr_t		sc_reg;
  	struct fd_softc *sc_fd[4];	/* pointers to children */
  	TAILQ_HEAD(drivehead, fd_softc) sc_drives;
--- 138,144 ----
  /* software state, per controller */
  struct fdc_softc {
  	struct device	sc_dev;		/* boilerplate */
! 	void		*sc_sih;	/* softintr cookie */
  	caddr_t		sc_reg;
  	struct fd_softc *sc_fd[4];	/* pointers to children */
  	TAILQ_HEAD(drivehead, fd_softc) sc_drives;
*************** int	fdc_c_hwintr(struct fdc_softc *);
*** 261,267 ****
  #else
  void	fdchwintr(void);
  #endif
! int	fdcswintr(struct fdc_softc *);
  int	fdcstate(struct fdc_softc *);
  void	fdcretry(struct fdc_softc *fdc);
  void	fdfinish(struct fd_softc *fd, struct buf *bp);
--- 261,267 ----
  #else
  void	fdchwintr(void);
  #endif
! void	fdcswintr(void *);
  int	fdcstate(struct fdc_softc *);
  void	fdcretry(struct fdc_softc *fdc);
  void	fdfinish(struct fd_softc *fd, struct buf *bp);
*************** int	fdformat(dev_t, struct fd_formb *, struct proc *);
*** 269,293 ****
  void	fd_do_eject(struct fd_softc *);
  static int fdconf(struct fdc_softc *);
  
- #if IPL_FDSOFT == 4
- #define IE_FDSOFT	IE_L4
- #else
- #error 4
- #endif
- 
- #ifdef FDC_C_HANDLER
- #if defined(SUN4M)
- #define FD_SET_SWINTR do {		\
- 	if (CPU_ISSUN4M)		\
- 		raise(0, IPL_FDSOFT);	\
- 	else				\
- 		ienab_bis(IE_FDSOFT);	\
- } while(0)
- #else
- #define FD_SET_SWINTR ienab_bis(IE_FDSOFT)
- #endif /* defined(SUN4M) */
- #endif /* FDC_C_HANDLER */
- 
  #define OBP_FDNAME	(CPU_ISSUN4M ? "SUNW,fdtwo" : "fd")
  
  int
--- 269,274 ----
*************** fdcattach(parent, self, aux)
*** 448,456 ****
  	evcount_attach(&fdc->sc_hih.ih_count, self->dv_xname,
  	    &fdc->sc_hih.ih_vec, &evcount_intr);
  #endif
! 	fdc->sc_sih.ih_fun = (void *)fdcswintr;
! 	fdc->sc_sih.ih_arg = fdc;
! 	intr_establish(IPL_FDSOFT, &fdc->sc_sih, IPL_BIO, self->dv_xname);
  
  	/* Assume a 82077 */
  	fdc->sc_reg_msr = &((struct fdreg_77 *)fdc->sc_reg)->fd_msr;
--- 429,435 ----
  	evcount_attach(&fdc->sc_hih.ih_count, self->dv_xname,
  	    &fdc->sc_hih.ih_vec, &evcount_intr);
  #endif
! 	fdc->sc_sih = softintr_establish(IPL_FDSOFT, fdcswintr, fdc);
  
  	/* Assume a 82077 */
  	fdc->sc_reg_msr = &((struct fdreg_77 *)fdc->sc_reg)->fd_msr;
*************** fdc_c_hwintr(fdc)
*** 1168,1181 ****
  			fdc->sc_istatus = FDC_ISTATUS_ERROR;
  		else
  			fdc->sc_istatus = FDC_ISTATUS_DONE;
! 		FD_SET_SWINTR;
  		goto done;
  	case FDC_ITASK_RESULT:
  		if (fdcresult(fdc) == -1)
  			fdc->sc_istatus = FDC_ISTATUS_ERROR;
  		else
  			fdc->sc_istatus = FDC_ISTATUS_DONE;
! 		FD_SET_SWINTR;
  		goto done;
  	case FDC_ITASK_DMA:
  		/* Proceed with pseudo-DMA below */
--- 1147,1160 ----
  			fdc->sc_istatus = FDC_ISTATUS_ERROR;
  		else
  			fdc->sc_istatus = FDC_ISTATUS_DONE;
! 		softintr_schedule(fdc->sc_sih);
  		goto done;
  	case FDC_ITASK_RESULT:
  		if (fdcresult(fdc) == -1)
  			fdc->sc_istatus = FDC_ISTATUS_ERROR;
  		else
  			fdc->sc_istatus = FDC_ISTATUS_DONE;
! 		softintr_schedule(fdc->sc_sih);
  		goto done;
  	case FDC_ITASK_DMA:
  		/* Proceed with pseudo-DMA below */
*************** fdc_c_hwintr(fdc)
*** 1183,1189 ****
  	default:
  		printf("fdc: stray hard interrupt: itask=%d\n", fdc->sc_itask);
  		fdc->sc_istatus = FDC_ISTATUS_SPURIOUS;
! 		FD_SET_SWINTR;
  		goto done;
  	}
  
--- 1162,1168 ----
  	default:
  		printf("fdc: stray hard interrupt: itask=%d\n", fdc->sc_itask);
  		fdc->sc_istatus = FDC_ISTATUS_SPURIOUS;
! 		softintr_schedule(fdc->sc_sih);
  		goto done;
  	}
  
*************** fdc_c_hwintr(fdc)
*** 1202,1208 ****
  		if ((msr & NE7_NDM) == 0) {
  			fdcresult(fdc);
  			fdc->sc_istatus = FDC_ISTATUS_DONE;
! 			FD_SET_SWINTR;
  #ifdef FD_DEBUG
  			if (fdc_debug > 1)
  				printf("fdc: overrun: tc = %d\n", fdc->sc_tc);
--- 1181,1187 ----
  		if ((msr & NE7_NDM) == 0) {
  			fdcresult(fdc);
  			fdc->sc_istatus = FDC_ISTATUS_DONE;
! 			softintr_schedule(fdc->sc_sih);
  #ifdef FD_DEBUG
  			if (fdc_debug > 1)
  				printf("fdc: overrun: tc = %d\n", fdc->sc_tc);
*************** fdc_c_hwintr(fdc)
*** 1219,1225 ****
  			fdc->sc_istatus = FDC_ISTATUS_DONE;
  			FTC_FLIP;
  			fdcresult(fdc);
! 			FD_SET_SWINTR;
  			break;
  		}
  	}
--- 1198,1204 ----
  			fdc->sc_istatus = FDC_ISTATUS_DONE;
  			FTC_FLIP;
  			fdcresult(fdc);
! 			softintr_schedule(fdc->sc_sih);
  			break;
  		}
  	}
*************** done:
*** 1228,1242 ****
  }
  #endif
  
! int
! fdcswintr(fdc)
! 	struct fdc_softc *fdc;
  {
  	int s;
  
  	if (fdc->sc_istatus == FDC_ISTATUS_NONE)
  		/* This (software) interrupt is not for us */
! 		return (0);
  
  	switch (fdc->sc_istatus) {
  	case FDC_ISTATUS_ERROR:
--- 1207,1221 ----
  }
  #endif
  
! void
! fdcswintr(void *arg)
  {
+ 	struct fdc_softc *fdc = (struct fdc_softc *)arg;
  	int s;
  
  	if (fdc->sc_istatus == FDC_ISTATUS_NONE)
  		/* This (software) interrupt is not for us */
! 		return;
  
  	switch (fdc->sc_istatus) {
  	case FDC_ISTATUS_ERROR:
*************** fdcswintr(fdc)
*** 1250,1256 ****
  	s = splbio();
  	fdcstate(fdc);
  	splx(s);
- 	return (1);
  }
  
  int
--- 1229,1234 ----
Index: src/sys/arch/sparc/dev/magma.c
===================================================================
RCS file: /cvs/src/sys/arch/sparc/dev/magma.c,v
retrieving revision 1.20
retrieving revision 1.21
diff -c -p -r1.20 -r1.21
*** src/sys/arch/sparc/dev/magma.c	29 Nov 2008 01:55:06 -0000	1.20
--- src/sys/arch/sparc/dev/magma.c	10 Apr 2009 20:53:51 -0000	1.21
***************
*** 1,4 ****
! /*	$OpenBSD: magma.c,v 1.20 2008/11/29 01:55:06 ray Exp $	*/
  
  /*-
   * Copyright (c) 1998 Iain Hibbert
--- 1,4 ----
! /*	$OpenBSD: magma.c,v 1.21 2009/04/10 20:53:51 miod Exp $	*/
  
  /*-
   * Copyright (c) 1998 Iain Hibbert
***************
*** 59,77 ****
  #include <sparc/bppioctl.h>
  #include <sparc/dev/magmareg.h>
  
- /*
-  * Select tty soft interrupt bit based on TTY ipl. (stolen from zs.c)
-  */
- #if IPL_TTY == 1
- # define IE_MSOFT IE_L1
- #elif IPL_TTY == 4
- # define IE_MSOFT IE_L4
- #elif IPL_TTY == 6
- # define IE_MSOFT IE_L6
- #else
- # error "no suitable software interrupt bit"
- #endif
- 
  #ifdef MAGMA_DEBUG
  #define dprintf(x) printf x
  #else
--- 59,64 ----
*************** magma_attach(parent, dev, args)
*** 480,488 ****
  	intr_establish(ra->ra_intr[0].int_pri, &sc->ms_hardint, -1,
  	    dev->dv_xname);
  
! 	sc->ms_softint.ih_fun = magma_soft;
! 	sc->ms_softint.ih_arg = sc;
! 	intr_establish(IPL_TTY, &sc->ms_softint, IPL_TTY, dev->dv_xname);
  }
  
  /*
--- 467,473 ----
  	intr_establish(ra->ra_intr[0].int_pri, &sc->ms_hardint, -1,
  	    dev->dv_xname);
  
! 	sc->ms_softint = softintr_establish(IPL_SOFTTTY, magma_soft, sc);
  }
  
  /*
*************** magma_hard(arg)
*** 715,728 ****
  	}
  	*/
  
! 	if (needsoftint) {	/* trigger the soft interrupt */
! #if defined(SUN4M)
! 		if (CPU_ISSUN4M)
! 			raise(0, IPL_TTY);
! 		else
! #endif
! 			ienab_bis(IE_MSOFT);
! 	}
  
  	return (serviced);
  }
--- 700,707 ----
  	}
  	*/
  
! 	if (needsoftint)	/* trigger the soft interrupt */
! 		softintr_schedule(sc->ms_softint);
  
  	return (serviced);
  }
*************** magma_hard(arg)
*** 730,740 ****
  /*
   * magma soft interrupt handler
   *
-  *  returns 1 if it handled it, 0 otherwise
-  *
   *  runs at spltty()
   */
! int
  magma_soft(arg)
  	void *arg;
  {
--- 709,717 ----
  /*
   * magma soft interrupt handler
   *
   *  runs at spltty()
   */
! void
  magma_soft(arg)
  	void *arg;
  {
*************** magma_soft(arg)
*** 742,748 ****
  	struct mtty_softc *mtty = sc->ms_mtty;
  	struct mbpp_softc *mbpp = sc->ms_mbpp;
  	int port;
- 	int serviced = 0;
  	int s, flags;
  
  	/*
--- 719,724 ----
*************** magma_soft(arg)
*** 780,786 ****
  					    mtty->ms_dev.dv_xname, port);
  
  				(*linesw[tp->t_line].l_rint)(data, tp);
- 				serviced = 1;
  			}
  
  			s = splhigh();	/* block out hard interrupt routine */
--- 756,761 ----
*************** magma_soft(arg)
*** 795,808 ****
  				    mp->mp_carrier ? "on" : "off"));
  				(*linesw[tp->t_line].l_modem)(tp,
  				    mp->mp_carrier);
- 				serviced = 1;
  			}
  
  			if (ISSET(flags, MTTYF_RING_OVERFLOW)) {
  				log(LOG_WARNING,
  				    "%s%x: ring buffer overflow\n",
  				    mtty->ms_dev.dv_xname, port);
- 				serviced = 1;
  			}
  
  			if (ISSET(flags, MTTYF_DONE)) {
--- 770,781 ----
*************** magma_soft(arg)
*** 811,817 ****
  				CLR(tp->t_state, TS_BUSY);
  				/* might be some more */
  				(*linesw[tp->t_line].l_start)(tp);
- 				serviced = 1;
  			}
  		} /* for (each mtty...) */
  	}
--- 784,789 ----
*************** magma_soft(arg)
*** 833,844 ****
  
  			if (ISSET(flags, MBPPF_WAKEUP)) {
  				wakeup(mp);
- 				serviced = 1;
  			}
  		} /* for (each mbpp...) */
  	}
- 
- 	return (serviced);
  }
  
  /************************************************************************
--- 805,813 ----
Index: src/sys/arch/sparc/dev/magma.c
===================================================================
RCS file: /cvs/src/sys/arch/sparc/dev/magma.c,v
retrieving revision 1.19
retrieving revision 1.20
diff -c -p -r1.19 -r1.20
*** src/sys/arch/sparc/dev/magma.c	2 Nov 2004 21:16:10 -0000	1.19
--- src/sys/arch/sparc/dev/magma.c	29 Nov 2008 01:55:06 -0000	1.20
***************
*** 1,7 ****
! /*	$OpenBSD: magma.c,v 1.19 2004/11/02 21:16:10 miod Exp $	*/
! /*
!  * magma.c
!  *
   * Copyright (c) 1998 Iain Hibbert
   * All rights reserved.
   *
--- 1,6 ----
! /*	$OpenBSD: magma.c,v 1.20 2008/11/29 01:55:06 ray Exp $	*/
! 
! /*-
   * Copyright (c) 1998 Iain Hibbert
   * All rights reserved.
   *
***************
*** 13,23 ****
   * 2. Redistributions in binary form must reproduce the above copyright
   *    notice, this list of conditions and the following disclaimer in the
   *    documentation and/or other materials provided with the distribution.
-  * 3. All advertising materials mentioning features or use of this software
-  *    must display the following acknowledgement:
-  *	This product includes software developed by Iain Hibbert
-  * 4. The name of the author may not be used to endorse or promote products
-  *    derived from this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
--- 12,17 ----
***************
*** 29,35 ****
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-  *
   */
  
  /*
--- 23,28 ----
Index: src/sys/arch/sparc/dev/spif.c
===================================================================
RCS file: /cvs/src/sys/arch/sparc/dev/spif.c,v
retrieving revision 1.20
retrieving revision 1.21
diff -c -p -r1.20 -r1.21
*** src/sys/arch/sparc/dev/spif.c	2 Jun 2006 20:00:54 -0000	1.20
--- src/sys/arch/sparc/dev/spif.c	10 Apr 2009 20:53:51 -0000	1.21
***************
*** 1,4 ****
! /*	$OpenBSD: spif.c,v 1.20 2006/06/02 20:00:54 miod Exp $	*/
  
  /*
   * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
--- 1,4 ----
! /*	$OpenBSD: spif.c,v 1.21 2009/04/10 20:53:51 miod Exp $	*/
  
  /*
   * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
***************
*** 53,68 ****
  #include <sparc/dev/spifreg.h>
  #include <sparc/dev/spifvar.h>
  
- #if IPL_TTY == 1
- # define IE_MSOFT IE_L1
- #elif IPL_TTY == 4
- # define IE_MSOFT IE_L4
- #elif IPL_TTY == 6
- # define IE_MSOFT IE_L6
- #else
- # error "no suitable software interrupt bit"
- #endif
- 
  int	spifmatch(struct device *, void *, void *);
  void	spifattach(struct device *, struct device *, void *);
  
--- 53,58 ----
*************** int	spifstcintr_mx(struct spif_softc *, int *);
*** 80,86 ****
  int	spifstcintr_tx(struct spif_softc *, int *);
  int	spifstcintr_rx(struct spif_softc *, int *);
  int	spifstcintr_rxexception(struct spif_softc *, int *);
! int	spifsoftintr(void *);
  
  int	stty_param(struct tty *, struct termios *);
  struct tty *sttytty(dev_t);
--- 70,76 ----
  int	spifstcintr_tx(struct spif_softc *, int *);
  int	spifstcintr_rx(struct spif_softc *, int *);
  int	spifstcintr_rxexception(struct spif_softc *, int *);
! void	spifsoftintr(void *);
  
  int	stty_param(struct tty *, struct termios *);
  struct tty *sttytty(dev_t);
*************** spifattach(parent, self, aux)
*** 213,221 ****
  	sc->sc_stcih.ih_arg = sc;
  	intr_establish(stcpri, &sc->sc_stcih, -1, self->dv_xname);
  
! 	sc->sc_softih.ih_fun = spifsoftintr;
! 	sc->sc_softih.ih_arg = sc;
! 	intr_establish(IPL_TTY, &sc->sc_softih, IPL_TTY, self->dv_xname);
  }
  
  int
--- 203,209 ----
  	sc->sc_stcih.ih_arg = sc;
  	intr_establish(stcpri, &sc->sc_stcih, -1, self->dv_xname);
  
! 	sc->sc_softih = softintr_establish(IPL_SOFTTTY, spifsoftintr, sc);
  }
  
  int
*************** spifstcintr(vsc)
*** 869,892 ****
  			r |= spifstcintr_mx(sc, &needsoft);
  	}
  
! 	if (needsoft) {
! #if defined(SUN4M)
! 		if (CPU_ISSUN4M)
! 			raise(0, IPL_TTY);
! 		else
! #endif
! 			ienab_bis(IE_MSOFT);
! 	}
  	return (r);
  }
  
! int
  spifsoftintr(vsc)
  	void *vsc;
  {
  	struct spif_softc *sc = (struct spif_softc *)vsc;
  	struct stty_softc *stc = sc->sc_ttys;
! 	int r = 0, i, data, s, flags;
  	u_int8_t stat, msvr;
  	struct stty_port *sp;
  	struct tty *tp;
--- 857,874 ----
  			r |= spifstcintr_mx(sc, &needsoft);
  	}
  
! 	if (needsoft)
! 		softintr_schedule(sc->sc_softih);
  	return (r);
  }
  
! void
  spifsoftintr(vsc)
  	void *vsc;
  {
  	struct spif_softc *sc = (struct spif_softc *)vsc;
  	struct stty_softc *stc = sc->sc_ttys;
! 	int i, data, s, flags;
  	u_int8_t stat, msvr;
  	struct stty_port *sp;
  	struct tty *tp;
*************** spifsoftintr(vsc)
*** 913,919 ****
  					data |= TTY_PE;
  
  				(*linesw[tp->t_line].l_rint)(data, tp);
- 				r = 1;
  			}
  
  			s = splhigh();
--- 895,900 ----
*************** spifsoftintr(vsc)
*** 931,943 ****
  				sp->sp_carrier = msvr & CD180_MSVR_CD;
  				(*linesw[tp->t_line].l_modem)(tp,
  				    sp->sp_carrier);
- 				r = 1;
  			}
  
  			if (ISSET(flags, STTYF_RING_OVERFLOW)) {
  				log(LOG_WARNING, "%s-%x: ring overflow\n",
  					stc->sc_dev.dv_xname, i);
- 				r = 1;
  			}
  
  			if (ISSET(flags, STTYF_DONE)) {
--- 912,922 ----
*************** spifsoftintr(vsc)
*** 945,956 ****
  				    sp->sp_txp - tp->t_outq.c_cf);
  				CLR(tp->t_state, TS_BUSY);
  				(*linesw[tp->t_line].l_start)(tp);
- 				r = 1;
  			}
  		}
  	}
- 
- 	return (r);
  }
  
  static __inline	void
--- 924,932 ----
Index: src/sys/arch/sparc/dev/ts102.c
===================================================================
RCS file: /cvs/src/sys/arch/sparc/dev/ts102.c,v
retrieving revision 1.18
retrieving revision 1.19
diff -c -p -r1.18 -r1.19
*** src/sys/arch/sparc/dev/ts102.c	11 Aug 2006 18:57:35 -0000	1.18
--- src/sys/arch/sparc/dev/ts102.c	10 Apr 2009 20:54:57 -0000	1.19
***************
*** 1,4 ****
! /*	$OpenBSD: ts102.c,v 1.18 2006/08/11 18:57:35 miod Exp $	*/
  /*
   * Copyright (c) 2003, 2004, Miodrag Vallat.
   *
--- 1,4 ----
! /*	$OpenBSD: ts102.c,v 1.19 2009/04/10 20:54:57 miod Exp $	*/
  /*
   * Copyright (c) 2003, 2004, Miodrag Vallat.
   *
*************** struct	tslot_data {
*** 112,117 ****
--- 112,118 ----
  	/* Interrupt handler */
  	int			(*td_intr)(void *);
  	void			*td_intrarg;
+ 	void			*td_softintr;
  
  	/* Socket status */
  	int			td_slot;
*************** void	tslot_create_event_thread(void *);
*** 137,142 ****
--- 138,144 ----
  void	tslot_event_thread(void *);
  int	tslot_intr(void *);
  void	tslot_intr_disestablish(pcmcia_chipset_handle_t, void *);
+ void	tslot_intr_dispatch(void *);
  void	*tslot_intr_establish(pcmcia_chipset_handle_t, struct pcmcia_function *,
  	    int, int (*)(void *), void *, char *);
  const char *tslot_intr_string(pcmcia_chipset_handle_t, void *);
*************** tslot_intr(void *v)
*** 664,670 ****
--- 666,676 ----
  	struct tslot_data *td;
  	int intregs[TS102_NUM_SLOTS], *intreg;
  	int i, rc = 0;
+ 	int s;
  
+ 	/* protect hardware access against soft interrupts */
+ 	s = splhigh();
+ 
  	/*
  	 * Scan slots, and acknowledge the interrupt if necessary first
  	 */
*************** tslot_intr(void *v)
*** 704,709 ****
--- 710,716 ----
  			tslot_slot_intr(td, *intreg);
  	}
  
+ 	splx(s);
  	return (rc);
  }
  
*************** tslot_slot_intr(struct tslot_data *td, int intreg)
*** 765,785 ****
  			return;
  		}
  
! 		if (td->td_intr != NULL) {
  			/*
! 			 * XXX There is no way to honour the interrupt handler
! 			 * requested IPL level...
  			 */
! 			(*td->td_intr)(td->td_intrarg);
  		}
  	}
  }
  
  void
  tslot_intr_disestablish(pcmcia_chipset_handle_t pch, void *ih)
  {
  	struct tslot_data *td = (struct tslot_data *)pch;
  
  	td->td_intr = NULL;
  	td->td_intrarg = NULL;
  }
--- 772,819 ----
  			return;
  		}
  
! 		if (td->td_softintr != NULL) {
  			/*
! 			 * Disable this sbus interrupt, until the
! 			 * softintr handler had a chance to run.
  			 */
! 			TSLOT_WRITE(td, TS102_REG_CARD_A_INT,
! 			    TSLOT_READ(td, TS102_REG_CARD_A_INT) &
! 			    ~TS102_CARD_INT_MASK_IRQ);
! 
! 			softintr_schedule(td->td_softintr);
  		}
  	}
  }
  
+ /*
+  * Software interrupt called to invoke the real driver interrupt handler.
+  */
  void
+ tslot_intr_dispatch(void *arg)
+ {
+ 	struct tslot_data *td = (struct tslot_data *)arg;
+ 	int s;
+ 
+ 	/* invoke driver handler */
+ 	td->td_intr(td->td_intrarg);
+ 
+ 	/* enable SBUS interrupts for PCMCIA interrupts again */
+ 	s = splhigh();
+ 	TSLOT_WRITE(td, TS102_REG_CARD_A_INT,
+ 	    TSLOT_READ(td, TS102_REG_CARD_A_INT) | TS102_CARD_INT_MASK_IRQ);
+ 	splx(s);
+ }
+ 
+ void
  tslot_intr_disestablish(pcmcia_chipset_handle_t pch, void *ih)
  {
  	struct tslot_data *td = (struct tslot_data *)pch;
  
+ 	if (td->td_softintr != NULL) {
+ 		softintr_disestablish(td->td_softintr);
+ 		td->td_softintr = NULL;
+ 	}
  	td->td_intr = NULL;
  	td->td_intrarg = NULL;
  }
*************** tslot_intr_establish(pcmcia_chipset_handle_t pch, stru
*** 800,807 ****
  {
  	struct tslot_data *td = (struct tslot_data *)pch;
  
  	td->td_intr = handler;
  	td->td_intrarg = arg;
  
! 	return (td);
  }
--- 834,847 ----
  {
  	struct tslot_data *td = (struct tslot_data *)pch;
  
+ 	/*
+ 	 * Note that this code relies on softintr_establish() to be
+ 	 * used with real, hardware ipl values. All platforms with
+ 	 * SBus support support this.
+ 	 */
  	td->td_intr = handler;
  	td->td_intrarg = arg;
+ 	td->td_softintr = softintr_establish(ipl, tslot_intr_dispatch, td);
  
! 	return td->td_softintr != NULL ? td : NULL;
  }
Index: src/sys/arch/sparc/dev/zs.c
===================================================================
RCS file: /cvs/src/sys/arch/sparc/dev/zs.c,v
retrieving revision 1.46
retrieving revision 1.47
diff -c -p -r1.46 -r1.47
*** src/sys/arch/sparc/dev/zs.c	26 Jun 2008 05:42:13 -0000	1.46
--- src/sys/arch/sparc/dev/zs.c	10 Apr 2009 20:53:51 -0000	1.47
***************
*** 1,4 ****
! /*	$OpenBSD: zs.c,v 1.46 2008/06/26 05:42:13 ray Exp $	*/
  /*	$NetBSD: zs.c,v 1.50 1997/10/18 00:00:40 gwr Exp $	*/
  
  /*-
--- 1,4 ----
! /*	$OpenBSD: zs.c,v 1.47 2009/04/10 20:53:51 miod Exp $	*/
  /*	$NetBSD: zs.c,v 1.50 1997/10/18 00:00:40 gwr Exp $	*/
  
  /*-
*************** int zs_major = 12;
*** 96,114 ****
   */
  #define PCLK	(9600 * 512)	/* PCLK pin input clock rate */
  
- /*
-  * Select software interrupt bit based on TTY ipl.
-  */
- #if IPL_TTY == 1
- # define IE_ZSSOFT IE_L1
- #elif IPL_TTY == 4
- # define IE_ZSSOFT IE_L4
- #elif IPL_TTY == 6
- # define IE_ZSSOFT IE_L6
- #else
- # error "no suitable software interrupt bit"
- #endif
- 
  #define	ZS_DELAY()		(CPU_ISSUN4C ? (0) : delay(2))
  
  /* The layout of this is hardware-dependent (padding, order). */
--- 96,101 ----
*************** struct cfdriver zs_cd = {
*** 213,221 ****
  
  /* Interrupt handlers. */
  int zshard(void *);
! int zssoft(void *);
  struct intrhand levelhard = { zshard };
- struct intrhand levelsoft = { zssoft };
  
  int zs_get_speed(struct zs_chanstate *);
  
--- 200,207 ----
  
  /* Interrupt handlers. */
  int zshard(void *);
! void zssoft(void *);
  struct intrhand levelhard = { zshard };
  
  int zs_get_speed(struct zs_chanstate *);
  
*************** zs_attach(parent, self, aux)
*** 360,369 ****
  		didintr = 1;
  		prevpri = pri;
  		intr_establish(pri, &levelhard, IPL_ZS, self->dv_xname);
- 		intr_establish(IPL_TTY, &levelsoft, IPL_TTY, self->dv_xname);
  	} else if (pri != prevpri)
  		panic("broken zs interrupt scheme");
  
  	/*
  	 * Set the master interrupt enable and interrupt vector.
  	 * (common to both channels, do it on A)
--- 346,356 ----
  		didintr = 1;
  		prevpri = pri;
  		intr_establish(pri, &levelhard, IPL_ZS, self->dv_xname);
  	} else if (pri != prevpri)
  		panic("broken zs interrupt scheme");
  
+ 	zsc->zsc_softih = softintr_establish(IPL_SOFTTTY, zssoft, zsc);
+ 
  	/*
  	 * Set the master interrupt enable and interrupt vector.
  	 * (common to both channels, do it on A)
*************** zs_print(aux, name)
*** 419,426 ****
  	return UNCONF;
  }
  
- volatile int zssoftpending;
- 
  /*
   * Our ZS chips all share a common, autovectored interrupt,
   * so we have to look at all of them on each interrupt.
--- 406,411 ----
*************** zshard(arg)
*** 442,497 ****
  		if (rr3) {
  			rval |= rr3;
  		}
! 		softreq |= zsc->zsc_cs[0].cs_softreq;
! 		softreq |= zsc->zsc_cs[1].cs_softreq;
  	}
  
- 	/* We are at splzs here, so no need to lock. */
- 	if (softreq && (zssoftpending == 0)) {
- 		zssoftpending = IE_ZSSOFT;
- #if defined(SUN4M)
- 		if (CPU_ISSUN4M)
- 			raise(0, IPL_TTY);
- 		else
- #endif
- 		ienab_bis(IE_ZSSOFT);
- 	}
  	return (rval);
  }
  
  /*
   * Similar scheme as for zshard (look at all of them)
   */
! int
  zssoft(arg)
  	void *arg;
  {
! 	struct zsc_softc *zsc;
! 	int s, unit;
  
- 	/* This is not the only ISR on this IPL. */
- 	if (zssoftpending == 0)
- 		return (0);
- 
- 	/*
- 	 * The soft intr. bit will be set by zshard only if
- 	 * the variable zssoftpending is zero.  The order of
- 	 * these next two statements prevents our clearing
- 	 * the soft intr bit just after zshard has set it.
- 	 */
- 	/* ienab_bic(IE_ZSSOFT); */
- 	zssoftpending = 0;
- 
  	/* Make sure we call the tty layer at spltty. */
  	s = spltty();
! 	for (unit = 0; unit < zs_cd.cd_ndevs; unit++) {
! 		zsc = zs_cd.cd_devs[unit];
! 		if (zsc == NULL)
! 			continue;
! 		(void)zsc_intr_soft(zsc);
! 	}
  	splx(s);
- 	return (1);
  }
  
  
--- 427,453 ----
  		if (rr3) {
  			rval |= rr3;
  		}
! 		if (zsc->zsc_cs[0].cs_softreq || zsc->zsc_cs[1].cs_softreq)
! 			softintr_schedule(zsc->zsc_softih);
  	}
  
  	return (rval);
  }
  
  /*
   * Similar scheme as for zshard (look at all of them)
   */
! void
  zssoft(arg)
  	void *arg;
  {
! 	struct zsc_softc *zsc = (struct zsc_softc *)arg;
! 	int s;
  
  	/* Make sure we call the tty layer at spltty. */
  	s = spltty();
! 	(void)zsc_intr_soft(zsc);
  	splx(s);
  }
  
  
Index: src/sys/arch/sparc/sparc/intr.c
===================================================================
RCS file: /cvs/src/sys/arch/sparc/sparc/intr.c,v
retrieving revision 1.31
retrieving revision 1.32
diff -c -p -r1.31 -r1.32
*** src/sys/arch/sparc/sparc/intr.c	29 May 2007 18:10:43 -0000	1.31
--- src/sys/arch/sparc/sparc/intr.c	10 Apr 2009 20:53:54 -0000	1.32
***************
*** 1,4 ****
! /*	$OpenBSD: intr.c,v 1.31 2007/05/29 18:10:43 miod Exp $ */
  /*	$NetBSD: intr.c,v 1.20 1997/07/29 09:42:03 fair Exp $ */
  
  /*
--- 1,4 ----
! /*	$OpenBSD: intr.c,v 1.32 2009/04/10 20:53:54 miod Exp $ */
  /*	$NetBSD: intr.c,v 1.20 1997/07/29 09:42:03 fair Exp $ */
  
  /*
***************
*** 45,50 ****
--- 45,51 ----
  #include <sys/systm.h>
  #include <sys/kernel.h>
  #include <sys/socket.h>
+ #include <sys/malloc.h>
  
  #include <uvm/uvm_extern.h>
  
***************
*** 75,82 ****
  #include <netinet6/ip6_var.h>
  #endif
  
  void	strayintr(struct clockframe *);
- int	soft01intr(void *);
  
  /*
   * Stray interrupt handler.  Clear it if possible.
--- 76,88 ----
  #include <netinet6/ip6_var.h>
  #endif
  
+ extern void raise(int, int);
+ 
+ void	ih_insert(struct intrhand **, struct intrhand *);
+ void	ih_remove(struct intrhand **, struct intrhand *);
+ 
+ void	softnet(void *);
  void	strayintr(struct clockframe *);
  
  /*
   * Stray interrupt handler.  Clear it if possible.
*************** strayintr(fp)
*** 103,149 ****
  
  static struct intrhand level10 = { clockintr, NULL, (IPL_CLOCK << 8) };
  static struct intrhand level14 = { statintr, NULL, (IPL_STATCLOCK << 8) };
- union sir sir;
- int netisr;
  
- /*
-  * Level 1 software interrupt (could also be SBus level 1 interrupt).
-  * Three possible reasons:
-  *	ROM console input needed
-  *	Network software interrupt
-  *	Soft clock interrupt
-  */
- int
- soft01intr(fp)
- 	void *fp;
- {
- 	if (sir.sir_any) {
- 		if (sir.sir_which[SIR_NET]) {
- 			int n;
- 
- 			sir.sir_which[SIR_NET] = 0;
- 			while ((n = netisr) != 0) {
- 				atomic_clearbits_int(&netisr, n);
- 
- #define DONETISR(bit, fn)						\
- 				do {					\
- 					if (n & (1 << bit))		\
- 						fn();			\
- 				} while (0)
- 
- #include <net/netisr_dispatch.h>
- 
- #undef DONETISR
- 			}
- 		}
- 		if (sir.sir_which[SIR_CLOCK]) {
- 			sir.sir_which[SIR_CLOCK] = 0;
- 			softclock();
- 		}
- 	}
- 	return (1);
- }
- 
  #if defined(SUN4M)
  void	nmi_hard(void);
  void
--- 109,115 ----
*************** nmi_hard()
*** 207,226 ****
  }
  #endif
  
- static struct intrhand level01 = { soft01intr, NULL, (IPL_SOFTINT << 8) };
- 
  void
  intr_init()
  {
- 	level01.ih_vec = level01.ih_ipl >> 8;
- 	evcount_attach(&level01.ih_count, "softintr", &level01.ih_vec,
- 	    &evcount_intr);
  	level10.ih_vec = level10.ih_ipl >> 8;
  	evcount_attach(&level10.ih_count, "clock", &level10.ih_vec,
  	    &evcount_intr);
  	level14.ih_vec = level14.ih_ipl >> 8;
  	evcount_attach(&level14.ih_count, "prof", &level14.ih_vec,
  	    &evcount_intr);
  }
  
  /*
--- 173,189 ----
  }
  #endif
  
  void
  intr_init()
  {
  	level10.ih_vec = level10.ih_ipl >> 8;
  	evcount_attach(&level10.ih_count, "clock", &level10.ih_vec,
  	    &evcount_intr);
  	level14.ih_vec = level14.ih_ipl >> 8;
  	evcount_attach(&level14.ih_count, "prof", &level14.ih_vec,
  	    &evcount_intr);
+ 
+ 	softnet_ih = softintr_establish(IPL_SOFTNET, softnet, NULL);
  }
  
  /*
*************** intr_init()
*** 230,236 ****
   */
  struct intrhand *intrhand[15] = {
  	NULL,			/*  0 = error */
! 	&level01,		/*  1 = software level 1 + SBus */
  	NULL,	 		/*  2 = SBus level 2 (4m: SBus L1) */
  	NULL,			/*  3 = SCSI + DMA + SBus level 3 (4m: L2,lpt)*/
  	NULL,			/*  4 = software level 4 (tty softint) (scsi) */
--- 193,199 ----
   */
  struct intrhand *intrhand[15] = {
  	NULL,			/*  0 = error */
! 	NULL,			/*  1 = software level 1 + SBus */
  	NULL,	 		/*  2 = SBus level 2 (4m: SBus L1) */
  	NULL,			/*  3 = SCSI + DMA + SBus level 3 (4m: L2,lpt)*/
  	NULL,			/*  4 = software level 4 (tty softint) (scsi) */
*************** struct intrhand *intrhand[15] = {
*** 246,251 ****
--- 209,252 ----
  	&level14,		/* 14 = counter 1 = profiling timer */
  };
  
+ /*
+  * Soft interrupts use a separate set of handler chains.
+  * This is necessary since soft interrupt handlers do not return a value
+  * and therefore can not be mixed with hardware interrupt handlers on a
+  * shared handler chain.
+  */
+ struct intrhand *sintrhand[15];
+ 
+ void
+ ih_insert(struct intrhand **head, struct intrhand *ih)
+ {
+ 	struct intrhand **p, *q;
+ 
+ 	/*
+ 	 * This is O(N^2) for long chains, but chains are never long
+ 	 * and we do want to preserve order.
+ 	 */
+ 	for (p = head; (q = *p) != NULL; p = &q->ih_next)
+ 		continue;
+ 	*p = ih;
+ 	ih->ih_next = NULL;
+ }
+ 
+ void
+ ih_remove(struct intrhand **head, struct intrhand *ih)
+ {
+ 	struct intrhand **p, *q;
+ 
+ 	for (p = head; (q = *p) != ih; p = &q->ih_next)
+ 		continue;
+ 	if (q == NULL)
+ 		panic("ih_remove: intrhand %p (fn %p arg %p) not found from %p",
+ 		    ih, ih->ih_fun, ih->ih_arg, head);
+ 
+ 	*p = q->ih_next;
+ 	q->ih_next = NULL;
+ }
+ 
  static int fastvec;		/* marks fast vectors (see below) */
  static struct {
  	int (*cb)(void *);
*************** intr_establish(level, ih, ipl_block, name)
*** 266,272 ****
  	int ipl_block;
  	const char *name;
  {
- 	struct intrhand **p, *q;
  #ifdef DIAGNOSTIC
  	struct trapvec *tv;
  	int displ;
--- 267,272 ----
*************** intr_establish(level, ih, ipl_block, name)
*** 309,316 ****
  	if (fastvec & (1 << level)) {
  		if (fastvec_share[level].cb == NULL ||
  		    (*fastvec_share[level].cb)(fastvec_share[level].data) != 0)
! 			panic("intr_establish: level %d interrupt tied to fast vector",
! 			    level);
  	}
  
  #ifdef DIAGNOSTIC
--- 309,316 ----
  	if (fastvec & (1 << level)) {
  		if (fastvec_share[level].cb == NULL ||
  		    (*fastvec_share[level].cb)(fastvec_share[level].data) != 0)
! 			panic("intr_establish: level %d interrupt tied to"
! 			    " unremovable fast vector", level);
  	}
  
  #ifdef DIAGNOSTIC
*************** intr_establish(level, ih, ipl_block, name)
*** 331,344 ****
  			    I_MOVi(I_L3, level), I_BA(0, displ), I_RDPSR(I_L0));
  	}
  #endif
! 	/*
! 	 * This is O(N^2) for long chains, but chains are never long
! 	 * and we do want to preserve order.
! 	 */
! 	for (p = &intrhand[level]; (q = *p) != NULL; p = &q->ih_next)
! 		continue;
! 	*p = ih;
! 	ih->ih_next = NULL;
  	splx(s);
  }
  
--- 331,337 ----
  			    I_MOVi(I_L3, level), I_BA(0, displ), I_RDPSR(I_L0));
  	}
  #endif
! 	ih_insert(&intrhand[level], ih);
  	splx(s);
  }
  
*************** intr_fasttrap(int level, void (*vec)(void), int (*shar
*** 369,378 ****
  	s = splhigh();
  
  	/*
! 	 * If this interrupt is already being handled, fail; the caller will
! 	 * either panic or try to register a slow (shareable) trap.
  	 */
! 	if ((fastvec & (1 << level)) != 0 || intrhand[level] != NULL) {
  		splx(s);
  		return (EBUSY);
  	}
--- 362,373 ----
  	s = splhigh();
  
  	/*
! 	 * If this interrupt is already being handled, or if it is also used
! 	 * for software interrupts, we fail; the caller will either panic or
! 	 * try to register a slow (shareable) trap.
  	 */
! 	if ((fastvec & (1 << level)) != 0 ||
! 	    intrhand[level] != NULL || sintrhand[level] != NULL) {
  		splx(s);
  		return (EBUSY);
  	}
*************** intr_fastuntrap(int level)
*** 448,453 ****
--- 443,577 ----
  	splx(s);
  }
  
+ void
+ softintr_disestablish(void *arg)
+ {
+ 	struct sintrhand *sih = (struct sintrhand *)arg;
+ 
+ 	ih_remove(&sintrhand[sih->sih_ipl], &sih->sih_ih);
+ 	free(sih, M_DEVBUF);
+ }
+ 
+ void *
+ softintr_establish(int level, void (*fn)(void *), void *arg)
+ {
+ 	struct sintrhand *sih;
+ 	struct intrhand *ih;
+ 	int ipl, hw;
+ 	int s;
+ 
+ 	/*
+ 	 * On a sun4m, the interrupt level is stored unmodified
+ 	 * to be passed to raise().
+ 	 * On a sun4 or sun4c, the appropriate bit to set
+ 	 * in the interrupt enable register is stored, to be
+ 	 * passed to ienab_bis().
+ 	 */
+ 	ipl = hw = level;
+ #if defined(SUN4) || defined(SUN4C)
+ 	if (CPU_ISSUN4OR4C) {
+ 		/*
+ 		 * Select the most suitable of the three available
+ 		 * softintr levels.
+ 		 */
+ 		if (level < 4) {
+ 			ipl = 1;
+ 			hw = IE_L1;
+ 		} else if (level < 6) {
+ 			ipl = 4;
+ 			hw = IE_L4;
+ 		} else {
+ 			ipl = 6;
+ 			hw = IE_L6;
+ 		}
+ 	}
+ #endif
+ 
+ 	sih = (struct sintrhand *)malloc(sizeof *sih, M_DEVBUF, M_ZERO);
+ 	if (sih == NULL)
+ 		return NULL;
+ 
+ 	sih->sih_ipl = ipl;
+ 	sih->sih_hw = hw;
+ 
+ 	ih = &sih->sih_ih;
+ 	ih->ih_fun = (int (*)(void *))fn;
+ 	ih->ih_arg = arg;
+ 	/*
+ 	 * We store the ipl pre-shifted so that we can avoid one instruction
+ 	 * in the interrupt handlers.
+ 	 */
+ 	ih->ih_ipl = level << 8;
+ 
+ 	s = splhigh();
+ 
+ 	/*
+ 	 * Check if this interrupt is already being handled by a fast trap.
+ 	 * If so, attempt to change it back to a regular (thus) shareable
+ 	 * trap.
+ 	 */
+ 	if (fastvec & (1 << ipl)) {
+ 		if (fastvec_share[ipl].cb == NULL ||
+ 		    (*fastvec_share[ipl].cb)(fastvec_share[ipl].data) != 0)
+ 			panic("softintr_establish: level %d interrupt tied to"
+ 			    " unremovable fast vector", ipl);
+ 	}
+ 
+ 	ih_insert(&sintrhand[ipl], ih);
+ 	splx(s);
+ 
+ 	return sih;
+ }
+ 
+ void
+ softintr_schedule(void *arg)
+ {
+ 	struct sintrhand *sih = (struct sintrhand *)arg;
+ 	int s;
+ 
+ 	s = splhigh();
+ 	if (sih->sih_pending == 0) {
+ 		sih->sih_pending++;
+ 
+ #if defined(SUN4M)
+ 		if (CPU_ISSUN4M)
+ 			raise(0, sih->sih_hw);
+ #endif
+ #if defined(SUN4) || defined(SUN4C)
+ 		if (CPU_ISSUN4OR4C)
+ 			ienab_bis(sih->sih_hw);
+ #endif
+ #if defined(solbourne)
+ 		if (CPU_ISKAP)
+ 			ienab_bis(sih->sih_hw);
+ #endif
+ 	}
+ 	splx(s);
+ }
+ 
+ void *softnet_ih;
+ int netisr;
+ 
+ void
+ softnet(void *arg)
+ {
+ 	int n;
+ 
+ 	while ((n = netisr) != 0) {
+ 		atomic_clearbits_int(&netisr, n);
+ 
+ #define DONETISR(bit, fn)						\
+ 		do {							\
+ 			if (n & (1 << bit))				\
+ 				fn();					\
+ 		} while (0)
+ 
+ #include <net/netisr_dispatch.h>
+ 
+ #undef DONETISR
+ 	}
+ }
+ 
  #ifdef DIAGNOSTIC
  void
  splassert_check(int wantipl, const char *func)
*************** splassert_check(int wantipl, const char *func)
*** 464,467 ****
  	}
  }
  #endif
- 
--- 588,590 ----
Index: src/sys/arch/sparc/sparc/pmap.c
===================================================================
RCS file: /cvs/src/sys/arch/sparc/sparc/pmap.c,v
retrieving revision 1.151
retrieving revision 1.152
diff -c -p -r1.151 -r1.152
*** src/sys/arch/sparc/sparc/pmap.c	27 Jan 2009 22:14:13 -0000	1.151
--- src/sys/arch/sparc/sparc/pmap.c	12 Feb 2009 18:52:17 -0000	1.152
***************
*** 1,4 ****
! /*	$OpenBSD: pmap.c,v 1.151 2009/01/27 22:14:13 miod Exp $	*/
  /*	$NetBSD: pmap.c,v 1.118 1998/05/19 19:00:18 thorpej Exp $ */
  
  /*
--- 1,4 ----
! /*	$OpenBSD: pmap.c,v 1.152 2009/02/12 18:52:17 miod Exp $	*/
  /*	$NetBSD: pmap.c,v 1.118 1998/05/19 19:00:18 thorpej Exp $ */
  
  /*
*************** mmu_setup4m_L3(pagtblptd, sp)
*** 1148,1153 ****
--- 1148,1154 ----
  		case SRMMU_TEPTE:
  			sp->sg_npte++;
  			setpgt4m(&sp->sg_pte[i], te | PPROT_U2S_OMASK);
+ 			pmap_kernel()->pm_stats.resident_count++;
  			break;
  		case SRMMU_TEPTD:
  			panic("mmu_setup4m_L3: PTD found in L3 page table");
*************** pmap_bootstrap4_4c(nctx, nregion, nsegment)
*** 2868,2873 ****
--- 2869,2875 ----
  		rp->rg_segmap[vs % NSEGRG].sg_pmeg = scookie;
  		npte = ++scookie < zseg ? NPTESG : lastpage;
  		rp->rg_segmap[vs % NSEGRG].sg_npte = npte;
+ 		pmap_kernel()->pm_stats.resident_count += npte;
  		rp->rg_nsegmap += 1;
  		mmuseg++;
  		vs++;
*************** pmap_bootstrap4m(void)
*** 3220,3225 ****
--- 3222,3228 ----
  			pte |= PPROT_WRITE;
  
  		setpgt4m(&sp->sg_pte[VA_VPG(q)], pte);
+ 		pmap_kernel()->pm_stats.resident_count++;
  	}
  
  #if 0
*************** pmap_rmk4_4c(pm, va, endva, vr, vs)
*** 3781,3786 ****
--- 3784,3790 ----
  		}
  		nleft--;
  		setpte4(va, 0);
+ 		pm->pm_stats.resident_count--;
  		va += NBPG;
  	}
  
*************** pmap_rmk4m(pm, va, endva, vr, vs)
*** 3889,3894 ****
--- 3893,3899 ----
  		nleft--;
  		tlb_flush_page(va);
  		setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], SRMMU_TEINVALID);
+ 		pm->pm_stats.resident_count--;
  		va += NBPG;
  	}
  
*************** pmap_rmu4_4c(pm, va, endva, vr, vs)
*** 3962,3967 ****
--- 3967,3973 ----
  			}
  			nleft--;
  			*pte = 0;
+ 			pm->pm_stats.resident_count--;
  		}
  		if ((sp->sg_npte = nleft) == 0) {
  			free(pte0, M_VMPMAP);
*************** pmap_rmu4_4c(pm, va, endva, vr, vs)
*** 4027,4032 ****
--- 4033,4039 ----
  		nleft--;
  		setpte4(pteva, 0);
  		pte0[VA_VPG(pteva)] = 0;
+ 		pm->pm_stats.resident_count--;
  	}
  
  	/*
*************** pmap_rmu4m(pm, va, endva, vr, vs)
*** 4148,4153 ****
--- 4155,4161 ----
  		if (pm->pm_ctx)
  			tlb_flush_page(va);
  		setpgt4m(&pte0[VA_SUN4M_VPG(va)], SRMMU_TEINVALID);
+ 		pm->pm_stats.resident_count--;
  	}
  
  	/*
*************** pmap_page_protect4_4c(struct vm_page *pg, vm_prot_t pr
*** 4355,4360 ****
--- 4363,4369 ----
  		}
  
  	nextpv:
+ 		pm->pm_stats.resident_count--;
  		npv = pv->pv_next;
  		if (pv != pv0)
  			pool_put(&pvpool, pv);
*************** pmap_page_protect4m(struct vm_page *pg, vm_prot_t prot
*** 4661,4666 ****
--- 4670,4676 ----
  		flags |= MR4M(tpte);
  
  		setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], SRMMU_TEINVALID);
+ 		pm->pm_stats.resident_count--;
  
  		/* Entire segment is gone */
  		if (sp->sg_npte == 0 && pm != pmap_kernel()) {
*************** pmap_enk4_4c(pm, va, prot, flags, pv, pteproto)
*** 4982,4987 ****
--- 4992,4998 ----
  				cache_flush_page((int)va);
  			}
  		}
+ 		pm->pm_stats.resident_count--;
  	} else {
  		/* adding new entry */
  		sp->sg_npte++;
*************** pmap_enk4_4c(pm, va, prot, flags, pv, pteproto)
*** 5035,5040 ****
--- 5046,5052 ----
  
  	/* ptes kept in hardware only */
  	setpte4(va, pteproto);
+ 	pm->pm_stats.resident_count++;
  	splx(s);
  
  	return (0);
*************** pmap_enu4_4c(pm, va, prot, flags, pv, pteproto)
*** 5164,5169 ****
--- 5176,5182 ----
  				if (doflush && (tpte & PG_NC) == 0)
  					cache_flush_page((int)va);
  			}
+ 			pm->pm_stats.resident_count--;
  		} else {
  			/* adding new entry */
  			sp->sg_npte++;
*************** pmap_enu4_4c(pm, va, prot, flags, pv, pteproto)
*** 5199,5204 ****
--- 5212,5218 ----
  	/* update software copy */
  	pte += VA_VPG(va);
  	*pte = pteproto;
+ 	pm->pm_stats.resident_count++;
  
  	splx(s);
  
*************** pmap_enk4m(pm, va, prot, flags, pv, pteproto)
*** 5373,5378 ****
--- 5387,5393 ----
  				cache_flush_page((int)va);
  			}
  		}
+ 		pm->pm_stats.resident_count--;
  	} else {
  		/* adding new entry */
  		sp->sg_npte++;
*************** pmap_enk4m(pm, va, prot, flags, pv, pteproto)
*** 5388,5393 ****
--- 5403,5409 ----
  
  	tlb_flush_page(va);
  	setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], pteproto);
+ 	pm->pm_stats.resident_count++;
  
  	splx(s);
  
*************** pmap_enu4m(pm, va, prot, flags, pv, pteproto)
*** 5506,5511 ****
--- 5522,5528 ----
  				if (pm->pm_ctx && (tpte & SRMMU_PG_C))
  					cache_flush_page((int)va);
  			}
+ 			pm->pm_stats.resident_count--;
  		} else {
  			/* adding new entry */
  			sp->sg_npte++;
*************** pmap_enu4m(pm, va, prot, flags, pv, pteproto)
*** 5528,5533 ****
--- 5545,5551 ----
  		tlb_flush_page(va);
  	}
  	setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], pteproto);
+ 	pm->pm_stats.resident_count++;
  
  	splx(s);
  
*************** kvm_setcache(va, npages, cached)
*** 6093,6120 ****
  		setcontext4(ctx);
  #endif
  	}
- }
- 
- int
- pmap_count_ptes(pm)
- 	struct pmap *pm;
- {
- 	int idx, total;
- 	struct regmap *rp;
- 	struct segmap *sp;
- 
- 	if (pm == pmap_kernel()) {
- 		rp = &pm->pm_regmap[NUREG];
- 		idx = NKREG;
- 	} else {
- 		rp = pm->pm_regmap;
- 		idx = NUREG;
- 	}
- 	for (total = 0; idx;)
- 		if ((sp = rp[--idx].rg_segmap) != NULL)
- 			total += sp->sg_npte;
- 	pm->pm_stats.resident_count = total;
- 	return (total);
  }
  
  /*
--- 6111,6116 ----
Index: src/sys/arch/sparc/sparc/process_machdep.c
===================================================================
RCS file: /cvs/src/sys/arch/sparc/sparc/process_machdep.c,v
retrieving revision 1.12
retrieving revision 1.13
diff -c -p -r1.12 -r1.13
*** src/sys/arch/sparc/sparc/process_machdep.c	30 Dec 2005 00:18:30 -0000	1.12
--- src/sys/arch/sparc/sparc/process_machdep.c	5 Mar 2009 19:52:23 -0000	1.13
***************
*** 1,4 ****
! /*	$OpenBSD: process_machdep.c,v 1.12 2005/12/30 00:18:30 kettenis Exp $	*/
  /*	$NetBSD: process_machdep.c,v 1.6 1996/03/14 21:09:26 christos Exp $ */
  
  /*
--- 1,4 ----
! /*	$OpenBSD: process_machdep.c,v 1.13 2009/03/05 19:52:23 kettenis Exp $	*/
  /*	$NetBSD: process_machdep.c,v 1.6 1996/03/14 21:09:26 christos Exp $ */
  
  /*
*************** process_write_fpregs(p, regs)
*** 162,172 ****
  	return 0;
  }
  
  register_t
  process_get_wcookie(p)
  	struct proc *p;
  {
  	return p->p_addr->u_pcb.pcb_wcookie;
  }
- 
- #endif	/* PTRACE */
--- 162,172 ----
  	return 0;
  }
  
+ #endif	/* PTRACE */
+ 
  register_t
  process_get_wcookie(p)
  	struct proc *p;
  {
  	return p->p_addr->u_pcb.pcb_wcookie;
  }
Index: src/sys/netbt/bt_proto.c
===================================================================
RCS file: /cvs/src/sys/netbt/bt_proto.c,v
retrieving revision 1.4
retrieving revision 1.5
diff -c -p -r1.4 -r1.5
*** src/sys/netbt/bt_proto.c	24 Jun 2007 20:55:27 -0000	1.4
--- src/sys/netbt/bt_proto.c	22 Nov 2008 04:42:58 -0000	1.5
***************
*** 1,4 ****
! /*	$OpenBSD: bt_proto.c,v 1.4 2007/06/24 20:55:27 uwe Exp $	*/
  /*
   * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
   *
--- 1,4 ----
! /*	$OpenBSD: bt_proto.c,v 1.5 2008/11/22 04:42:58 uwe Exp $	*/
  /*
   * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
   *
***************
*** 30,35 ****
--- 30,37 ----
  
  struct domain btdomain;
  
+ void bt_init(void);
+ 
  struct protosw btsw[] = {
  	{ SOCK_RAW, &btdomain, BTPROTO_HCI,
  	  PR_ATOMIC | PR_ADDR,
*************** struct protosw btsw[] = {
*** 63,70 ****
  
  struct domain btdomain = {
  	AF_BLUETOOTH, "bluetooth",
! 	NULL/*init*/, NULL/*externalize*/, NULL/*dispose*/,
  	btsw, &btsw[sizeof(btsw) / sizeof(btsw[0])], NULL,
  	NULL/*rtattach*/, 32, sizeof(struct sockaddr_bt),
  	NULL/*ifattach*/, NULL/*ifdetach*/
  };
--- 65,86 ----
  
  struct domain btdomain = {
  	AF_BLUETOOTH, "bluetooth",
! 	bt_init, NULL/*externalize*/, NULL/*dispose*/,
  	btsw, &btsw[sizeof(btsw) / sizeof(btsw[0])], NULL,
  	NULL/*rtattach*/, 32, sizeof(struct sockaddr_bt),
  	NULL/*ifattach*/, NULL/*ifdetach*/
  };
+ 
+ struct mutex bt_lock;
+ 
+ void
+ bt_init(void)
+ {
+ 	/*
+ 	 * In accordance with mutex(9), since hci_intr() uses the
+ 	 * lock, we associate the subsystem lock with IPL_SOFTNET.
+ 	 * For unknown reasons, in NetBSD the interrupt level is
+ 	 * IPL_NONE.
+ 	 */
+ 	mtx_init(&bt_lock, IPL_SOFTNET);
+ }
Index: src/sys/netbt/hci_event.c
===================================================================
RCS file: /cvs/src/sys/netbt/hci_event.c,v
retrieving revision 1.7
retrieving revision 1.8
diff -c -p -r1.7 -r1.8
*** src/sys/netbt/hci_event.c	24 Feb 2008 21:34:48 -0000	1.7
--- src/sys/netbt/hci_event.c	22 Nov 2008 04:42:58 -0000	1.8
***************
*** 1,5 ****
! /*	$OpenBSD: hci_event.c,v 1.7 2008/02/24 21:34:48 uwe Exp $	*/
! /*	$NetBSD: hci_event.c,v 1.14 2008/02/10 17:40:54 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
--- 1,5 ----
! /*	$OpenBSD: hci_event.c,v 1.8 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: hci_event.c,v 1.18 2008/04/24 11:38:37 ad Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
*************** static void hci_cmd_read_local_features(struct hci_uni
*** 60,65 ****
--- 60,66 ----
  static void hci_cmd_read_local_ver(struct hci_unit *, struct mbuf *);
  static void hci_cmd_read_local_commands(struct hci_unit *, struct mbuf *);
  static void hci_cmd_reset(struct hci_unit *, struct mbuf *);
+ static void hci_cmd_create_con(struct hci_unit *unit, uint8_t status);
  
  #ifdef BLUETOOTH_DEBUG
  int bluetooth_debug;
*************** hci_event(struct mbuf *m, struct hci_unit *unit)
*** 230,291 ****
  /*
   * Command Status
   *
!  * Update our record of num_cmd_pkts then post-process any pending commands
!  * and optionally restart cmd output on the unit.
   */
  static void
  hci_event_command_status(struct hci_unit *unit, struct mbuf *m)
  {
  	hci_command_status_ep ep;
- 	struct hci_link *link;
  
  	KASSERT(m->m_pkthdr.len >= sizeof(ep));
  	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
  	m_adj(m, sizeof(ep));
  
  	DPRINTFN(1, "(%s) opcode (%03x|%04x) status = 0x%x num_cmd_pkts = %d\n",
  		device_xname(unit->hci_dev),
! 		HCI_OGF(letoh16(ep.opcode)), HCI_OCF(letoh16(ep.opcode)),
  		ep.status,
  		ep.num_cmd_pkts);
  
! 	if (ep.status > 0)
! 		printf("%s: CommandStatus opcode (%03x|%04x) failed (status=0x%02x)\n",
! 		    device_xname(unit->hci_dev), HCI_OGF(letoh16(ep.opcode)),
! 		    HCI_OCF(letoh16(ep.opcode)), ep.status);
  
- 	unit->hci_num_cmd_pkts = ep.num_cmd_pkts;
- 
  	/*
  	 * post processing of pending commands
  	 */
! 	switch(letoh16(ep.opcode)) {
  	case HCI_CMD_CREATE_CON:
! 		switch (ep.status) {
! 		case 0x12:	/* Invalid HCI command parameters */
! 			DPRINTF("(%s) Invalid HCI command parameters\n",
! 			    device_xname(unit->hci_dev));
! 			while ((link = hci_link_lookup_state(unit,
! 			    HCI_LINK_ACL, HCI_LINK_WAIT_CONNECT)) != NULL)
! 				hci_link_free(link, ECONNABORTED);
! 			break;
! 		}
  		break;
- 	default:
- 		break;
- 	}
  
! 	while (unit->hci_num_cmd_pkts > 0 && !IF_IS_EMPTY(&unit->hci_cmdwait)) {
! 		IF_DEQUEUE(&unit->hci_cmdwait, m);
! 		hci_output_cmd(unit, m);
  	}
  }
  
  /*
   * Command Complete
   *
!  * Update our record of num_cmd_pkts then handle the completed command,
!  * and optionally restart cmd output on the unit.
   */
  static void
  hci_event_command_compl(struct hci_unit *unit, struct mbuf *m)
--- 231,283 ----
  /*
   * Command Status
   *
!  * Restart command queue and post-process any pending commands
   */
  static void
  hci_event_command_status(struct hci_unit *unit, struct mbuf *m)
  {
  	hci_command_status_ep ep;
  
  	KASSERT(m->m_pkthdr.len >= sizeof(ep));
  	m_copydata(m, 0, sizeof(ep), (caddr_t)&ep);
  	m_adj(m, sizeof(ep));
  
+ 	ep.opcode = letoh16(ep.opcode);
+ 
  	DPRINTFN(1, "(%s) opcode (%03x|%04x) status = 0x%x num_cmd_pkts = %d\n",
  		device_xname(unit->hci_dev),
! 		HCI_OGF(ep.opcode), HCI_OCF(ep.opcode),
  		ep.status,
  		ep.num_cmd_pkts);
  
! 	hci_num_cmds(unit, ep.num_cmd_pkts);
  
  	/*
  	 * post processing of pending commands
  	 */
! 	switch(ep.opcode) {
  	case HCI_CMD_CREATE_CON:
! 		hci_cmd_create_con(unit, ep.status);
  		break;
  
!  	default:
! 		if (ep.status == 0)
! 			break;
! 
! 		DPRINTFN(1,
! 		    "CommandStatus opcode (%03x|%04x) failed (status=0x%02x)\n",
! 		    device_xname(unit->hci_dev),
! 		    HCI_OGF(ep.opcode), HCI_OCF(ep.opcode),
! 		    ep.status);
! 
!  		break;
  	}
  }
  
  /*
   * Command Complete
   *
!  * Restart command queue and handle the completed command
   */
  static void
  hci_event_command_compl(struct hci_unit *unit, struct mbuf *m)
*************** hci_event_command_compl(struct hci_unit *unit, struct 
*** 301,306 ****
--- 293,300 ----
  	    device_xname(unit->hci_dev), HCI_OGF(letoh16(ep.opcode)),
  	    HCI_OCF(letoh16(ep.opcode)), ep.num_cmd_pkts);
  
+ 	hci_num_cmds(unit, ep.num_cmd_pkts);
+ 
  	/*
  	 * I am not sure if this is completely correct, it is not guaranteed
  	 * that a command_complete packet will contain the status though most
*************** hci_event_command_compl(struct hci_unit *unit, struct 
*** 312,319 ****
  		    device_xname(unit->hci_dev), HCI_OGF(letoh16(ep.opcode)),
  		    HCI_OCF(letoh16(ep.opcode)), rp.status);
  
- 	unit->hci_num_cmd_pkts = ep.num_cmd_pkts;
- 
  	/*
  	 * post processing of completed commands
  	 */
--- 306,311 ----
*************** hci_event_command_compl(struct hci_unit *unit, struct 
*** 345,355 ****
  	default:
  		break;
  	}
- 
- 	while (unit->hci_num_cmd_pkts > 0 && !IF_IS_EMPTY(&unit->hci_cmdwait)) {
- 		IF_DEQUEUE(&unit->hci_cmdwait, m);
- 		hci_output_cmd(unit, m);
- 	}
  }
  
  /*
--- 337,342 ----
*************** hci_cmd_read_bdaddr(struct hci_unit *unit, struct mbuf
*** 856,862 ****
  
  	unit->hci_flags &= ~BTF_INIT_BDADDR;
  
! 	wakeup(unit);
  }
  
  /*
--- 843,849 ----
  
  	unit->hci_flags &= ~BTF_INIT_BDADDR;
  
! 	wakeup(&unit->hci_init);
  }
  
  /*
*************** hci_cmd_read_buffer_size(struct hci_unit *unit, struct
*** 884,890 ****
  
  	unit->hci_flags &= ~BTF_INIT_BUFFER_SIZE;
  
! 	wakeup(unit);
  }
  
  /*
--- 871,877 ----
  
  	unit->hci_flags &= ~BTF_INIT_BUFFER_SIZE;
  
! 	wakeup(&unit->hci_init);
  }
  
  /*
*************** hci_cmd_read_local_features(struct hci_unit *unit, str
*** 972,978 ****
  
  	unit->hci_flags &= ~BTF_INIT_FEATURES;
  
! 	wakeup(unit);
  
  	DPRINTFN(1, "%s: lmp_mask %4.4x, acl_mask %4.4x, sco_mask %4.4x\n",
  		device_xname(unit->hci_dev), unit->hci_lmp_mask,
--- 959,965 ----
  
  	unit->hci_flags &= ~BTF_INIT_FEATURES;
  
! 	wakeup(&unit->hci_init);
  
  	DPRINTFN(1, "%s: lmp_mask %4.4x, acl_mask %4.4x, sco_mask %4.4x\n",
  		device_xname(unit->hci_dev), unit->hci_lmp_mask,
*************** hci_cmd_read_local_ver(struct hci_unit *unit, struct m
*** 1001,1007 ****
  
  	if (rp.hci_version < HCI_SPEC_V12) {
  		unit->hci_flags &= ~BTF_INIT_COMMANDS;
! 		wakeup(unit);
  		return;
  	}
  
--- 988,994 ----
  
  	if (rp.hci_version < HCI_SPEC_V12) {
  		unit->hci_flags &= ~BTF_INIT_COMMANDS;
! 		wakeup(&unit->hci_init);
  		return;
  	}
  
*************** hci_cmd_read_local_commands(struct hci_unit *unit, str
*** 1029,1035 ****
  	unit->hci_flags &= ~BTF_INIT_COMMANDS;
  	memcpy(unit->hci_cmds, rp.commands, HCI_COMMANDS_SIZE);
  
! 	wakeup(unit);
  }
  
  /*
--- 1016,1022 ----
  	unit->hci_flags &= ~BTF_INIT_COMMANDS;
  	memcpy(unit->hci_cmds, rp.commands, HCI_COMMANDS_SIZE);
  
! 	wakeup(&unit->hci_init);
  }
  
  /*
*************** hci_cmd_reset(struct hci_unit *unit, struct mbuf *m)
*** 1079,1082 ****
--- 1066,1111 ----
  
  	if (hci_send_cmd(unit, HCI_CMD_READ_LOCAL_VER, NULL, 0))
  		return;
+ }
+ 
+ /*
+  * process command_status event for create_con command
+  *
+  * a "Create Connection" command can sometimes fail to start for whatever
+  * reason and the command_status event returns failure but we get no
+  * indication of which connection failed (for instance in the case where
+  * we tried to open too many connections all at once) So, we keep a flag
+  * on the link to indicate pending status until the command_status event
+  * is returned to help us decide which needs to be failed.
+  *
+  * since created links are inserted at the tail of hci_links, we know that
+  * the first pending link we find will be the one that this command status
+  * refers to.
+  */
+ static void
+ hci_cmd_create_con(struct hci_unit *unit, uint8_t status)
+ {
+ 	struct hci_link *link;
+ 
+ 	TAILQ_FOREACH(link, &unit->hci_links, hl_next) {
+ 		if ((link->hl_flags & HCI_LINK_CREATE_CON) == 0)
+ 			continue;
+ 
+ 		link->hl_flags &= ~HCI_LINK_CREATE_CON;
+ 
+ 		switch(status) {
+ 		case 0x00:	/* success */
+ 			break;
+ 
+ 		case 0x0c:	/* "Command Disallowed" */
+ 			hci_link_free(link, EBUSY);
+ 			break;
+ 
+ 		default:	/* some other trouble */
+ 			hci_link_free(link, /*EPROTO*/ECONNABORTED);
+ 			break;
+ 		}
+ 
+ 		return;
+ 	}
  }
Index: src/sys/netbt/hci_link.c
===================================================================
RCS file: /cvs/src/sys/netbt/hci_link.c,v
retrieving revision 1.8
retrieving revision 1.9
diff -c -p -r1.8 -r1.9
*** src/sys/netbt/hci_link.c	10 Sep 2008 14:01:23 -0000	1.8
--- src/sys/netbt/hci_link.c	22 Nov 2008 04:42:58 -0000	1.9
***************
*** 1,5 ****
! /*	$OpenBSD: hci_link.c,v 1.8 2008/09/10 14:01:23 blambert Exp $	*/
! /*	$NetBSD: hci_link.c,v 1.16 2007/11/10 23:12:22 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
--- 1,5 ----
! /*	$OpenBSD: hci_link.c,v 1.9 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: hci_link.c,v 1.20 2008/04/24 11:38:37 ad Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
*************** hci_acl_open(struct hci_unit *unit, bdaddr_t *bdaddr)
*** 75,86 ****
  
  	link = hci_link_lookup_bdaddr(unit, bdaddr, HCI_LINK_ACL);
  	if (link == NULL) {
! 		link = hci_link_alloc(unit);
  		if (link == NULL)
  			return NULL;
- 
- 		link->hl_type = HCI_LINK_ACL;
- 		bdaddr_copy(&link->hl_bdaddr, bdaddr);
  	}
  
  	switch(link->hl_state) {
--- 75,83 ----
  
  	link = hci_link_lookup_bdaddr(unit, bdaddr, HCI_LINK_ACL);
  	if (link == NULL) {
! 		link = hci_link_alloc(unit, bdaddr, HCI_LINK_ACL);
  		if (link == NULL)
  			return NULL;
  	}
  
  	switch(link->hl_state) {
*************** hci_acl_open(struct hci_unit *unit, bdaddr_t *bdaddr)
*** 108,113 ****
--- 105,111 ----
  			return NULL;
  		}
  
+ 		link->hl_flags |= HCI_LINK_CREATE_CON;
  		link->hl_state = HCI_LINK_WAIT_CONNECT;
  		break;
  
*************** hci_acl_newconn(struct hci_unit *unit, bdaddr_t *bdadd
*** 178,188 ****
  	if (link != NULL)
  		return NULL;
  
! 	link = hci_link_alloc(unit);
  	if (link != NULL) {
  		link->hl_state = HCI_LINK_WAIT_CONNECT;
- 		link->hl_type = HCI_LINK_ACL;
- 		bdaddr_copy(&link->hl_bdaddr, bdaddr);
  
  		if (hci_acl_expiry > 0)
  			timeout_add_sec(&link->hl_expire, hci_acl_expiry);
--- 176,184 ----
  	if (link != NULL)
  		return NULL;
  
! 	link = hci_link_alloc(unit, bdaddr, HCI_LINK_ACL);
  	if (link != NULL) {
  		link->hl_state = HCI_LINK_WAIT_CONNECT;
  
  		if (hci_acl_expiry > 0)
  			timeout_add_sec(&link->hl_expire, hci_acl_expiry);
*************** hci_acl_timeout(void *arg)
*** 196,204 ****
  {
  	struct hci_link *link = arg;
  	hci_discon_cp cp;
! 	int s, err;
  
! 	s = splsoftnet();
  
  	if (link->hl_refcnt > 0)
  		goto out;
--- 192,200 ----
  {
  	struct hci_link *link = arg;
  	hci_discon_cp cp;
! 	int err;
  
! 	mutex_enter(&bt_lock);
  
  	if (link->hl_refcnt > 0)
  		goto out;
*************** hci_acl_timeout(void *arg)
*** 234,240 ****
  	}
  
  out:
! 	splx(s);
  }
  
  /*
--- 230,236 ----
  	}
  
  out:
! 	mutex_exit(&bt_lock);
  }
  
  /*
*************** hci_sco_newconn(struct hci_unit *unit, bdaddr_t *bdadd
*** 789,803 ****
  		bdaddr_copy(&new->sp_laddr, &unit->hci_bdaddr);
  		bdaddr_copy(&new->sp_raddr, bdaddr);
  
! 		sco = hci_link_alloc(unit);
  		if (sco == NULL) {
  			sco_detach(&new);
  			return NULL;
  		}
  
- 		sco->hl_type = HCI_LINK_SCO;
- 		bdaddr_copy(&sco->hl_bdaddr, bdaddr);
- 
  		sco->hl_link = hci_acl_open(unit, bdaddr);
  		KASSERT(sco->hl_link == acl);
  
--- 785,796 ----
  		bdaddr_copy(&new->sp_laddr, &unit->hci_bdaddr);
  		bdaddr_copy(&new->sp_raddr, bdaddr);
  
! 		sco = hci_link_alloc(unit, bdaddr, HCI_LINK_SCO);
  		if (sco == NULL) {
  			sco_detach(&new);
  			return NULL;
  		}
  
  		sco->hl_link = hci_acl_open(unit, bdaddr);
  		KASSERT(sco->hl_link == acl);
  
*************** hci_sco_complete(struct hci_link *link, int num)
*** 885,891 ****
   */
  
  struct hci_link *
! hci_link_alloc(struct hci_unit *unit)
  {
  	struct hci_link *link;
  
--- 878,884 ----
   */
  
  struct hci_link *
! hci_link_alloc(struct hci_unit *unit, bdaddr_t *bdaddr, uint8_t type)
  {
  	struct hci_link *link;
  
*************** hci_link_alloc(struct hci_unit *unit)
*** 896,902 ****
--- 889,897 ----
  		return NULL;
  
  	link->hl_unit = unit;
+ 	link->hl_type = type;
  	link->hl_state = HCI_LINK_CLOSED;
+ 	bdaddr_copy(&link->hl_bdaddr, bdaddr);
  
  	/* init ACL portion */
  	timeout_set(&link->hl_expire, hci_acl_timeout, link);
*************** hci_link_alloc(struct hci_unit *unit)
*** 911,917 ****
  	/* &link->hl_data is already zero-initialized. */
  
  	/* attach to unit */
! 	TAILQ_INSERT_HEAD(&unit->hci_links, link, hl_next);
  	return link;
  }
  
--- 906,912 ----
  	/* &link->hl_data is already zero-initialized. */
  
  	/* attach to unit */
! 	TAILQ_INSERT_TAIL(&unit->hci_links, link, hl_next);
  	return link;
  }
  
*************** hci_link_free(struct hci_link *link, int err)
*** 1009,1036 ****
  }
  
  /*
-  * Lookup HCI link by type and state.
-  */
- struct hci_link *
- hci_link_lookup_state(struct hci_unit *unit, uint16_t type, uint16_t state)
- {
- 	struct hci_link *link;
- 
- 	TAILQ_FOREACH(link, &unit->hci_links, hl_next) {
- 		if (link->hl_type == type && link->hl_state == state)
- 			break;
- 	}
- 
- 	return link;
- }
- 
- /*
   * Lookup HCI link by address and type. Note that for SCO links there may
   * be more than one link per address, so we only return links with no
   * handle (ie new links)
   */
  struct hci_link *
! hci_link_lookup_bdaddr(struct hci_unit *unit, bdaddr_t *bdaddr, uint16_t type)
  {
  	struct hci_link *link;
  
--- 1004,1015 ----
  }
  
  /*
   * Lookup HCI link by address and type. Note that for SCO links there may
   * be more than one link per address, so we only return links with no
   * handle (ie new links)
   */
  struct hci_link *
! hci_link_lookup_bdaddr(struct hci_unit *unit, bdaddr_t *bdaddr, uint8_t type)
  {
  	struct hci_link *link;
  
Index: src/sys/netbt/hci_socket.c
===================================================================
RCS file: /cvs/src/sys/netbt/hci_socket.c,v
retrieving revision 1.6
retrieving revision 1.7
diff -c -p -r1.6 -r1.7
*** src/sys/netbt/hci_socket.c	27 May 2008 19:41:14 -0000	1.6
--- src/sys/netbt/hci_socket.c	22 Nov 2008 04:42:58 -0000	1.7
***************
*** 1,5 ****
! /*	$OpenBSD: hci_socket.c,v 1.6 2008/05/27 19:41:14 thib Exp $	*/
! /*	$NetBSD: hci_socket.c,v 1.14 2008/02/10 17:40:54 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
--- 1,5 ----
! /*	$OpenBSD: hci_socket.c,v 1.7 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: hci_socket.c,v 1.17 2008/08/06 15:01:24 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
*************** hci_usrreq(struct socket *up, int req, struct mbuf *m,
*** 555,566 ****
  
  	switch(req) {
  	case PRU_CONTROL:
! 		return hci_ioctl((unsigned long)m, (void *)nam, curproc);
  
  	case PRU_ATTACH:
  		if (pcb)
  			return EINVAL;
- 
  		err = soreserve(up, hci_sendspace, hci_recvspace);
  		if (err)
  			return err;
--- 555,569 ----
  
  	switch(req) {
  	case PRU_CONTROL:
! 		mutex_enter(&bt_lock);
! 		err = hci_ioctl((unsigned long)m, (void *)nam, curproc);
! 		mutex_exit(&bt_lock);
! 		return err;
  
  	case PRU_ATTACH:
+ 		/* XXX solock() and bt_lock fiddling in NetBSD */
  		if (pcb)
  			return EINVAL;
  		err = soreserve(up, hci_sendspace, hci_recvspace);
  		if (err)
  			return err;
Index: src/sys/netbt/hci_unit.c
===================================================================
RCS file: /cvs/src/sys/netbt/hci_unit.c,v
retrieving revision 1.8
retrieving revision 1.9
diff -c -p -r1.8 -r1.9
*** src/sys/netbt/hci_unit.c	24 Feb 2008 21:34:48 -0000	1.8
--- src/sys/netbt/hci_unit.c	22 Nov 2008 04:42:58 -0000	1.9
***************
*** 1,5 ****
! /*	$OpenBSD: hci_unit.c,v 1.8 2008/02/24 21:34:48 uwe Exp $	*/
! /*	$NetBSD: hci_unit.c,v 1.9 2007/12/30 18:26:42 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
--- 1,5 ----
! /*	$OpenBSD: hci_unit.c,v 1.9 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: hci_unit.c,v 1.12 2008/06/26 14:17:27 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
*************** struct hci_unit *
*** 81,87 ****
  hci_attach(const struct hci_if *hci_if, struct device *dev, uint16_t flags)
  {
  	struct hci_unit *unit;
- 	int s;
  
  	KASSERT(dev != NULL);
  	KASSERT(hci_if->enable != NULL);
--- 81,86 ----
*************** hci_attach(const struct hci_if *hci_if, struct device 
*** 99,104 ****
--- 98,104 ----
  	unit->hci_flags = flags;
  
  	mtx_init(&unit->hci_devlock, hci_if->ipl);
+ 	unit->hci_init = 0;	/* kcondvar_t in NetBSD */
  
  	unit->hci_eventq.ifq_maxlen = hci_eventq_max;
  	unit->hci_aclrxq.ifq_maxlen = hci_aclrxq_max;
*************** hci_attach(const struct hci_if *hci_if, struct device 
*** 109,117 ****
  	TAILQ_INIT(&unit->hci_links);
  	LIST_INIT(&unit->hci_memos);
  
! 	s = splsoftnet();
  	TAILQ_INSERT_TAIL(&hci_unit_list, unit, hci_next);
! 	splx(s);
  
  	return unit;
  }
--- 109,117 ----
  	TAILQ_INIT(&unit->hci_links);
  	LIST_INIT(&unit->hci_memos);
  
! 	mutex_enter(&bt_lock);
  	TAILQ_INSERT_TAIL(&hci_unit_list, unit, hci_next);
! 	mutex_exit(&bt_lock);
  
  	return unit;
  }
*************** hci_attach(const struct hci_if *hci_if, struct device 
*** 119,132 ****
  void
  hci_detach(struct hci_unit *unit)
  {
- 	int s;
  
! 	s = splsoftnet();
  	hci_disable(unit);
  
  	TAILQ_REMOVE(&hci_unit_list, unit, hci_next);
! 	splx(s);
  
  	free(unit, M_BLUETOOTH);
  }
  
--- 119,132 ----
  void
  hci_detach(struct hci_unit *unit)
  {
  
! 	mutex_enter(&bt_lock);
  	hci_disable(unit);
  
  	TAILQ_REMOVE(&hci_unit_list, unit, hci_next);
! 	mutex_exit(&bt_lock);
  
+ 	/* mutex_destroy(&unit->hci_devlock) in NetBSD */
  	free(unit, M_BLUETOOTH);
  }
  
*************** hci_enable(struct hci_unit *unit)
*** 178,184 ****
  		goto bad2;
  
  	while (unit->hci_flags & BTF_INIT) {
! 		err = tsleep(unit, PWAIT | PCATCH, __func__, 5 * hz);
  		if (err)
  			goto bad2;
  
--- 178,185 ----
  		goto bad2;
  
  	while (unit->hci_flags & BTF_INIT) {
! 		err = msleep(&unit->hci_init, &bt_lock, PWAIT | PCATCH,
! 		    __func__, 5 * hz);
  		if (err)
  			goto bad2;
  
*************** hci_disable(struct hci_unit *unit)
*** 216,223 ****
  	int acl;
  
  	if (unit->hci_bthub) {
! 		config_detach(unit->hci_bthub, DETACH_FORCE);
  		unit->hci_bthub = NULL;
  	}
  
  #ifndef __OpenBSD__
--- 217,230 ----
  	int acl;
  
  	if (unit->hci_bthub) {
! 		struct device *hub;
! 
! 		hub = unit->hci_bthub;
  		unit->hci_bthub = NULL;
+ 
+ 		mutex_exit(&bt_lock);
+ 		config_detach(hub, DETACH_FORCE);
+ 		mutex_enter(&bt_lock);
  	}
  
  #ifndef __OpenBSD__
*************** hci_intr(void *arg)
*** 333,345 ****
  	struct hci_unit *unit = arg;
  	struct mbuf *m;
  
  another:
! 	mtx_enter(&unit->hci_devlock);
  
  	if (unit->hci_eventqlen > 0) {
  		IF_DEQUEUE(&unit->hci_eventq, m);
  		unit->hci_eventqlen--;
! 		mtx_leave(&unit->hci_devlock);
  
  		KASSERT(m != NULL);
  
--- 340,353 ----
  	struct hci_unit *unit = arg;
  	struct mbuf *m;
  
+ 	mutex_enter(&bt_lock);
  another:
! 	mutex_enter(&unit->hci_devlock);
  
  	if (unit->hci_eventqlen > 0) {
  		IF_DEQUEUE(&unit->hci_eventq, m);
  		unit->hci_eventqlen--;
! 		mutex_exit(&unit->hci_devlock);
  
  		KASSERT(m != NULL);
  
*************** another:
*** 356,362 ****
  	if (unit->hci_scorxqlen > 0) {
  		IF_DEQUEUE(&unit->hci_scorxq, m);
  		unit->hci_scorxqlen--;
! 		mtx_leave(&unit->hci_devlock);
  
  		KASSERT(m != NULL);
  
--- 364,370 ----
  	if (unit->hci_scorxqlen > 0) {
  		IF_DEQUEUE(&unit->hci_scorxq, m);
  		unit->hci_scorxqlen--;
! 		mutex_exit(&unit->hci_devlock);
  
  		KASSERT(m != NULL);
  
*************** another:
*** 373,379 ****
  	if (unit->hci_aclrxqlen > 0) {
  		IF_DEQUEUE(&unit->hci_aclrxq, m);
  		unit->hci_aclrxqlen--;
! 		mtx_leave(&unit->hci_devlock);
  
  		KASSERT(m != NULL);
  
--- 381,387 ----
  	if (unit->hci_aclrxqlen > 0) {
  		IF_DEQUEUE(&unit->hci_aclrxq, m);
  		unit->hci_aclrxqlen--;
! 		mutex_exit(&unit->hci_devlock);
  
  		KASSERT(m != NULL);
  
*************** another:
*** 391,397 ****
  	if (m != NULL) {
  		struct hci_link *link;
  
! 		mtx_leave(&unit->hci_devlock);
  
  		DPRINTFN(11, "(%s) complete SCO\n",
  		    device_xname(unit->hci_dev));
--- 399,405 ----
  	if (m != NULL) {
  		struct hci_link *link;
  
! 		mutex_exit(&unit->hci_devlock);
  
  		DPRINTFN(11, "(%s) complete SCO\n",
  		    device_xname(unit->hci_dev));
*************** another:
*** 409,415 ****
  		goto another;
  	}
  
! 	mtx_leave(&unit->hci_devlock);
  
  	DPRINTFN(10, "done\n");
  }
--- 417,424 ----
  		goto another;
  	}
  
! 	mutex_exit(&unit->hci_devlock);
! 	mutex_exit(&bt_lock);
  
  	DPRINTFN(10, "done\n");
  }
*************** hci_input_event(struct hci_unit *unit, struct mbuf *m)
*** 428,434 ****
  {
  	int rv;
  
! 	mtx_enter(&unit->hci_devlock);
  
  	if (unit->hci_eventqlen > hci_eventq_max) {
  		DPRINTF("(%s) dropped event packet.\n", device_xname(unit->hci_dev));
--- 437,443 ----
  {
  	int rv;
  
! 	mutex_enter(&unit->hci_devlock);
  
  	if (unit->hci_eventqlen > hci_eventq_max) {
  		DPRINTF("(%s) dropped event packet.\n", device_xname(unit->hci_dev));
*************** hci_input_event(struct hci_unit *unit, struct mbuf *m)
*** 441,447 ****
  		rv = 1;
  	}
  
! 	mtx_leave(&unit->hci_devlock);
  	return rv;
  }
  
--- 450,456 ----
  		rv = 1;
  	}
  
! 	mutex_exit(&unit->hci_devlock);
  	return rv;
  }
  
*************** hci_input_acl(struct hci_unit *unit, struct mbuf *m)
*** 450,456 ****
  {
  	int rv;
  
! 	mtx_enter(&unit->hci_devlock);
  
  	if (unit->hci_aclrxqlen > hci_aclrxq_max) {
  		DPRINTF("(%s) dropped ACL packet.\n", device_xname(unit->hci_dev));
--- 459,465 ----
  {
  	int rv;
  
! 	mutex_enter(&unit->hci_devlock);
  
  	if (unit->hci_aclrxqlen > hci_aclrxq_max) {
  		DPRINTF("(%s) dropped ACL packet.\n", device_xname(unit->hci_dev));
*************** hci_input_acl(struct hci_unit *unit, struct mbuf *m)
*** 463,469 ****
  		rv = 1;
  	}
  
! 	mtx_leave(&unit->hci_devlock);
  	return rv;
  }
  
--- 472,478 ----
  		rv = 1;
  	}
  
! 	mutex_exit(&unit->hci_devlock);
  	return rv;
  }
  
*************** hci_input_sco(struct hci_unit *unit, struct mbuf *m)
*** 472,478 ****
  {
  	int rv;
  
! 	mtx_enter(&unit->hci_devlock);
  
  	if (unit->hci_scorxqlen > hci_scorxq_max) {
  		DPRINTF("(%s) dropped SCO packet.\n", device_xname(unit->hci_dev));
--- 481,487 ----
  {
  	int rv;
  
! 	mutex_enter(&unit->hci_devlock);
  
  	if (unit->hci_scorxqlen > hci_scorxq_max) {
  		DPRINTF("(%s) dropped SCO packet.\n", device_xname(unit->hci_dev));
*************** hci_input_sco(struct hci_unit *unit, struct mbuf *m)
*** 485,491 ****
  		rv = 1;
  	}
  
! 	mtx_leave(&unit->hci_devlock);
  	return rv;
  }
  
--- 494,500 ----
  		rv = 1;
  	}
  
! 	mutex_exit(&unit->hci_devlock);
  	return rv;
  }
  
*************** hci_complete_sco(struct hci_unit *unit, struct mbuf *m
*** 550,560 ****
  	}
  #endif
  
! 	mtx_enter(&unit->hci_devlock);
  
  	IF_ENQUEUE(&unit->hci_scodone, m);
  	schednetisr(NETISR_BT);
  
! 	mtx_leave(&unit->hci_devlock);
  	return 1;
  }
--- 559,585 ----
  	}
  #endif
  
! 	mutex_enter(&unit->hci_devlock);
  
  	IF_ENQUEUE(&unit->hci_scodone, m);
  	schednetisr(NETISR_BT);
  
! 	mutex_exit(&unit->hci_devlock);
  	return 1;
+ }
+ 
+ /*
+  * update num_cmd_pkts and push on pending commands queue
+  */
+ void
+ hci_num_cmds(struct hci_unit *unit, uint8_t num)
+ {
+ 	struct mbuf *m;
+ 
+ 	unit->hci_num_cmd_pkts = num;
+ 
+ 	while (unit->hci_num_cmd_pkts > 0 && !IF_IS_EMPTY(&unit->hci_cmdwait)) {
+ 		IF_DEQUEUE(&unit->hci_cmdwait, m);
+ 		hci_output_cmd(unit, m);
+ 	}
  }
Index: src/sys/netbt/l2cap_lower.c
===================================================================
RCS file: /cvs/src/sys/netbt/l2cap_lower.c,v
retrieving revision 1.2
retrieving revision 1.3
diff -c -p -r1.2 -r1.3
*** src/sys/netbt/l2cap_lower.c	24 Feb 2008 21:34:48 -0000	1.2
--- src/sys/netbt/l2cap_lower.c	22 Nov 2008 04:42:58 -0000	1.3
***************
*** 1,5 ****
! /*	$OpenBSD: l2cap_lower.c,v 1.2 2008/02/24 21:34:48 uwe Exp $	*/
! /*	$NetBSD: l2cap_lower.c,v 1.7 2007/11/10 23:12:23 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
--- 1,5 ----
! /*	$OpenBSD: l2cap_lower.c,v 1.3 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: l2cap_lower.c,v 1.9 2008/08/05 13:08:31 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
*************** l2cap_recv_frame(struct mbuf *m, struct hci_link *link
*** 134,146 ****
  
  	chan = l2cap_cid_lookup(hdr.dcid);
  	if (chan != NULL && chan->lc_link == link
  	    && chan->lc_state == L2CAP_OPEN) {
  		(*chan->lc_proto->input)(chan->lc_upper, m);
  		return;
  	}
  
! 	DPRINTF("(%s) dropping %d L2CAP data bytes for unknown CID #%d\n",
! 		device_xname(link->hl_unit->hci_dev), hdr.length, hdr.dcid);
  
  failed:
  	m_freem(m);
--- 134,147 ----
  
  	chan = l2cap_cid_lookup(hdr.dcid);
  	if (chan != NULL && chan->lc_link == link
+ 	    && chan->lc_imtu >= hdr.length
  	    && chan->lc_state == L2CAP_OPEN) {
  		(*chan->lc_proto->input)(chan->lc_upper, m);
  		return;
  	}
  
! 	DPRINTF("(%s) invalid L2CAP packet dropped, CID #%d, length %d\n",
! 		device_xname(link->hl_unit->hci_dev), hdr.dcid, hdr.length);
  
  failed:
  	m_freem(m);
Index: src/sys/netbt/l2cap_misc.c
===================================================================
RCS file: /cvs/src/sys/netbt/l2cap_misc.c,v
retrieving revision 1.5
retrieving revision 1.6
diff -c -p -r1.5 -r1.6
*** src/sys/netbt/l2cap_misc.c	10 Sep 2008 14:01:23 -0000	1.5
--- src/sys/netbt/l2cap_misc.c	22 Nov 2008 04:42:58 -0000	1.6
***************
*** 1,5 ****
! /*	$OpenBSD: l2cap_misc.c,v 1.5 2008/09/10 14:01:23 blambert Exp $	*/
! /*	$NetBSD: l2cap_misc.c,v 1.5 2007/11/03 17:20:17 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
--- 1,5 ----
! /*	$OpenBSD: l2cap_misc.c,v 1.6 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: l2cap_misc.c,v 1.6 2008/04/24 11:38:37 ad Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
*************** l2cap_rtx(void *arg)
*** 188,196 ****
  {
  	struct l2cap_req *req = arg;
  	struct l2cap_channel *chan;
- 	int s;
  
! 	s = splsoftnet();
  
  	chan = req->lr_chan;
  	DPRINTF("cid %d, ident %d\n", (chan ? chan->lc_lcid : 0), req->lr_id);
--- 188,195 ----
  {
  	struct l2cap_req *req = arg;
  	struct l2cap_channel *chan;
  
! 	mutex_enter(&bt_lock);
  
  	chan = req->lr_chan;
  	DPRINTF("cid %d, ident %d\n", (chan ? chan->lc_lcid : 0), req->lr_id);
*************** l2cap_rtx(void *arg)
*** 200,206 ****
  	if (chan && chan->lc_state != L2CAP_CLOSED)
  		l2cap_close(chan, ETIMEDOUT);
  
! 	splx(s);
  }
  
  /*
--- 199,205 ----
  	if (chan && chan->lc_state != L2CAP_CLOSED)
  		l2cap_close(chan, ETIMEDOUT);
  
! 	mutex_exit(&bt_lock);
  }
  
  /*
Index: src/sys/netbt/l2cap_socket.c
===================================================================
RCS file: /cvs/src/sys/netbt/l2cap_socket.c,v
retrieving revision 1.2
retrieving revision 1.3
diff -c -p -r1.2 -r1.3
*** src/sys/netbt/l2cap_socket.c	27 May 2008 19:41:14 -0000	1.2
--- src/sys/netbt/l2cap_socket.c	22 Nov 2008 04:42:58 -0000	1.3
***************
*** 1,5 ****
! /*	$OpenBSD: l2cap_socket.c,v 1.2 2008/05/27 19:41:14 thib Exp $	*/
! /*	$NetBSD: l2cap_socket.c,v 1.7 2007/04/21 06:15:23 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
--- 1,5 ----
! /*	$OpenBSD: l2cap_socket.c,v 1.3 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: l2cap_socket.c,v 1.9 2008/08/06 15:01:24 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
*************** l2cap_usrreq(struct socket *up, int req, struct mbuf *
*** 125,133 ****
  #endif
  
  	case PRU_ATTACH:
  		if (pcb != NULL)
  			return EINVAL;
- 
  		/*
  		 * For L2CAP socket PCB we just use an l2cap_channel structure
  		 * since we have nothing to add..
--- 125,133 ----
  #endif
  
  	case PRU_ATTACH:
+ 		/* XXX solock() and bt_lock fiddling in NetBSD */
  		if (pcb != NULL)
  			return EINVAL;
  		/*
  		 * For L2CAP socket PCB we just use an l2cap_channel structure
  		 * since we have nothing to add..
Index: src/sys/netbt/l2cap_upper.c
===================================================================
RCS file: /cvs/src/sys/netbt/l2cap_upper.c,v
retrieving revision 1.2
retrieving revision 1.3
diff -c -p -r1.2 -r1.3
*** src/sys/netbt/l2cap_upper.c	1 Oct 2007 16:39:30 -0000	1.2
--- src/sys/netbt/l2cap_upper.c	22 Nov 2008 04:42:58 -0000	1.3
***************
*** 1,5 ****
! /*	$OpenBSD: l2cap_upper.c,v 1.2 2007/10/01 16:39:30 krw Exp $	*/
! /*	$NetBSD: l2cap_upper.c,v 1.8 2007/04/29 20:23:36 msaitoh Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
--- 1,5 ----
! /*	$OpenBSD: l2cap_upper.c,v 1.3 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: l2cap_upper.c,v 1.9 2008/08/06 15:01:24 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2005 Iain Hibbert.
Index: src/sys/netbt/rfcomm_dlc.c
===================================================================
RCS file: /cvs/src/sys/netbt/rfcomm_dlc.c,v
retrieving revision 1.3
retrieving revision 1.4
diff -c -p -r1.3 -r1.4
*** src/sys/netbt/rfcomm_dlc.c	10 Sep 2008 14:01:23 -0000	1.3
--- src/sys/netbt/rfcomm_dlc.c	22 Nov 2008 04:42:58 -0000	1.4
***************
*** 1,5 ****
! /*	$OpenBSD: rfcomm_dlc.c,v 1.3 2008/09/10 14:01:23 blambert Exp $	*/
! /*	$NetBSD: rfcomm_dlc.c,v 1.4 2007/11/03 17:20:17 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
--- 1,5 ----
! /*	$OpenBSD: rfcomm_dlc.c,v 1.4 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: rfcomm_dlc.c,v 1.6 2008/08/06 15:01:24 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
*************** void
*** 192,207 ****
  rfcomm_dlc_timeout(void *arg)
  {
  	struct rfcomm_dlc *dlc = arg;
- 	int s;
  
! 	s = splsoftnet();
  
  	if (dlc->rd_state != RFCOMM_DLC_CLOSED)
  		rfcomm_dlc_close(dlc, ETIMEDOUT);
  	else if (dlc->rd_flags & RFCOMM_DLC_DETACH)
  		free(dlc, M_BLUETOOTH);
  
! 	splx(s);
  }
  
  /*
--- 192,206 ----
  rfcomm_dlc_timeout(void *arg)
  {
  	struct rfcomm_dlc *dlc = arg;
  
! 	mutex_enter(&bt_lock);
  
  	if (dlc->rd_state != RFCOMM_DLC_CLOSED)
  		rfcomm_dlc_close(dlc, ETIMEDOUT);
  	else if (dlc->rd_flags & RFCOMM_DLC_DETACH)
  		free(dlc, M_BLUETOOTH);
  
! 	mutex_exit(&bt_lock);
  }
  
  /*
Index: src/sys/netbt/rfcomm_session.c
===================================================================
RCS file: /cvs/src/sys/netbt/rfcomm_session.c,v
retrieving revision 1.4
retrieving revision 1.5
diff -c -p -r1.4 -r1.5
*** src/sys/netbt/rfcomm_session.c	10 Sep 2008 14:01:23 -0000	1.4
--- src/sys/netbt/rfcomm_session.c	22 Nov 2008 04:42:58 -0000	1.5
***************
*** 1,5 ****
! /*	$OpenBSD: rfcomm_session.c,v 1.4 2008/09/10 14:01:23 blambert Exp $	*/
! /*	$NetBSD: rfcomm_session.c,v 1.12 2008/01/31 19:30:23 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
--- 1,5 ----
! /*	$OpenBSD: rfcomm_session.c,v 1.5 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: rfcomm_session.c,v 1.14 2008/08/06 15:01:24 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
*************** rfcomm_session_timeout(void *arg)
*** 298,308 ****
  {
  	struct rfcomm_session *rs = arg;
  	struct rfcomm_dlc *dlc;
- 	int s;
  
  	KASSERT(rs != NULL);
  
! 	s = splsoftnet();
  
  	if (rs->rs_state != RFCOMM_SESSION_OPEN) {
  		DPRINTF("timeout\n");
--- 298,307 ----
  {
  	struct rfcomm_session *rs = arg;
  	struct rfcomm_dlc *dlc;
  
  	KASSERT(rs != NULL);
  
! 	mutex_enter(&bt_lock);
  
  	if (rs->rs_state != RFCOMM_SESSION_OPEN) {
  		DPRINTF("timeout\n");
*************** rfcomm_session_timeout(void *arg)
*** 319,325 ****
  		DPRINTF("expiring\n");
  		rfcomm_session_free(rs);
  	}
! 	splx(s);
  }
  
  /***********************************************************************
--- 318,324 ----
  		DPRINTF("expiring\n");
  		rfcomm_session_free(rs);
  	}
! 	mutex_exit(&bt_lock);
  }
  
  /***********************************************************************
Index: src/sys/netbt/rfcomm_socket.c
===================================================================
RCS file: /cvs/src/sys/netbt/rfcomm_socket.c,v
retrieving revision 1.3
retrieving revision 1.4
diff -c -p -r1.3 -r1.4
*** src/sys/netbt/rfcomm_socket.c	27 May 2008 19:41:14 -0000	1.3
--- src/sys/netbt/rfcomm_socket.c	22 Nov 2008 04:42:58 -0000	1.4
***************
*** 1,5 ****
! /*	$OpenBSD: rfcomm_socket.c,v 1.3 2008/05/27 19:41:14 thib Exp $	*/
! /*	$NetBSD: rfcomm_socket.c,v 1.8 2007/10/15 18:04:34 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
--- 1,5 ----
! /*	$OpenBSD: rfcomm_socket.c,v 1.4 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: rfcomm_socket.c,v 1.10 2008/08/06 15:01:24 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
*************** rfcomm_usrreq(struct socket *up, int req, struct mbuf 
*** 122,130 ****
  #endif
  
  	case PRU_ATTACH:
  		if (pcb != NULL)
  			return EINVAL;
- 
  		/*
  		 * Since we have nothing to add, we attach the DLC
  		 * structure directly to our PCB pointer.
--- 122,130 ----
  #endif
  
  	case PRU_ATTACH:
+ 		/* XXX solock() and bt_lock fiddling in NetBSD */
  		if (pcb != NULL)
  			return EINVAL;
  		/*
  		 * Since we have nothing to add, we attach the DLC
  		 * structure directly to our PCB pointer.
Index: src/sys/netbt/rfcomm_upper.c
===================================================================
RCS file: /cvs/src/sys/netbt/rfcomm_upper.c,v
retrieving revision 1.5
retrieving revision 1.6
diff -c -p -r1.5 -r1.6
*** src/sys/netbt/rfcomm_upper.c	10 Sep 2008 14:01:23 -0000	1.5
--- src/sys/netbt/rfcomm_upper.c	22 Nov 2008 04:42:58 -0000	1.6
***************
*** 1,5 ****
! /*	$OpenBSD: rfcomm_upper.c,v 1.5 2008/09/10 14:01:23 blambert Exp $	*/
! /*	$NetBSD: rfcomm_upper.c,v 1.10 2007/11/20 20:25:57 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
--- 1,5 ----
! /*	$OpenBSD: rfcomm_upper.c,v 1.6 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: rfcomm_upper.c,v 1.11 2008/08/06 15:01:24 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
Index: src/sys/netbt/sco_socket.c
===================================================================
RCS file: /cvs/src/sys/netbt/sco_socket.c,v
retrieving revision 1.2
retrieving revision 1.3
diff -c -p -r1.2 -r1.3
*** src/sys/netbt/sco_socket.c	27 May 2008 19:41:14 -0000	1.2
--- src/sys/netbt/sco_socket.c	22 Nov 2008 04:42:58 -0000	1.3
***************
*** 1,5 ****
! /*	$OpenBSD: sco_socket.c,v 1.2 2008/05/27 19:41:14 thib Exp $	*/
! /*	$NetBSD: sco_socket.c,v 1.9 2007/04/21 06:15:23 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
--- 1,5 ----
! /*	$OpenBSD: sco_socket.c,v 1.3 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: sco_socket.c,v 1.11 2008/08/06 15:01:24 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
*************** sco_usrreq(struct socket *up, int req, struct mbuf *m,
*** 117,125 ****
  #endif
  
  	case PRU_ATTACH:
  		if (pcb)
  			return EINVAL;
- 
  		err = soreserve(up, sco_sendspace, sco_recvspace);
  		if (err)
  			return err;
--- 117,125 ----
  #endif
  
  	case PRU_ATTACH:
+ 		/* XXX solock() and bt_lock fiddling in NetBSD */
  		if (pcb)
  			return EINVAL;
  		err = soreserve(up, sco_sendspace, sco_recvspace);
  		if (err)
  			return err;
Index: src/sys/netbt/sco_upper.c
===================================================================
RCS file: /cvs/src/sys/netbt/sco_upper.c,v
retrieving revision 1.2
retrieving revision 1.3
diff -c -p -r1.2 -r1.3
*** src/sys/netbt/sco_upper.c	1 Oct 2007 16:39:30 -0000	1.2
--- src/sys/netbt/sco_upper.c	22 Nov 2008 04:42:58 -0000	1.3
***************
*** 1,5 ****
! /*	$OpenBSD: sco_upper.c,v 1.2 2007/10/01 16:39:30 krw Exp $	*/
! /*	$NetBSD: sco_upper.c,v 1.6 2007/03/30 20:47:03 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
--- 1,5 ----
! /*	$OpenBSD: sco_upper.c,v 1.3 2008/11/22 04:42:58 uwe Exp $	*/
! /*	$NetBSD: sco_upper.c,v 1.8 2008/08/06 15:01:24 plunky Exp $	*/
  
  /*-
   * Copyright (c) 2006 Itronix Inc.
*************** sco_connect(struct sco_pcb *pcb, struct sockaddr_bt *d
*** 149,160 ****
  	if (acl == NULL || acl->hl_state != HCI_LINK_OPEN)
  		return EHOSTUNREACH;
  
! 	sco = hci_link_alloc(unit);
  	if (sco == NULL)
  		return ENOMEM;
- 
- 	sco->hl_type = HCI_LINK_SCO;
- 	bdaddr_copy(&sco->hl_bdaddr, &pcb->sp_raddr);
  
  	sco->hl_link = hci_acl_open(unit, &pcb->sp_raddr);
  	KASSERT(sco->hl_link == acl);
--- 149,157 ----
  	if (acl == NULL || acl->hl_state != HCI_LINK_OPEN)
  		return EHOSTUNREACH;
  
! 	sco = hci_link_alloc(unit, &pcb->sp_raddr, HCI_LINK_SCO);
  	if (sco == NULL)
  		return ENOMEM;
  
  	sco->hl_link = hci_acl_open(unit, &pcb->sp_raddr);
  	KASSERT(sco->hl_link == acl);
Index: src/sys/netinet/ip_output.c
===================================================================
RCS file: /cvs/src/sys/netinet/ip_output.c,v
retrieving revision 1.191
retrieving revision 1.192
diff -c -p -r1.191 -r1.192
*** src/sys/netinet/ip_output.c	9 May 2008 02:56:36 -0000	1.191
--- src/sys/netinet/ip_output.c	29 Jan 2009 12:33:15 -0000	1.192
***************
*** 1,4 ****
! /*	$OpenBSD: ip_output.c,v 1.191 2008/05/09 02:56:36 markus Exp $	*/
  /*	$NetBSD: ip_output.c,v 1.28 1996/02/13 23:43:07 christos Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: ip_output.c,v 1.192 2009/01/29 12:33:15 naddy Exp $	*/
  /*	$NetBSD: ip_output.c,v 1.28 1996/02/13 23:43:07 christos Exp $	*/
  
  /*
*************** sendit:
*** 721,734 ****
  	 * If small enough for interface, can just send directly.
  	 */
  	if (ntohs(ip->ip_len) <= mtu) {
  		if ((ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
  		    ifp->if_bridge == NULL) {
  			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
  			ipstat.ips_outhwcsum++;
! 		} else {
! 			ip->ip_sum = 0;
  			ip->ip_sum = in_cksum(m, hlen);
- 		}
  		/* Update relevant hardware checksum stats for TCP/UDP */
  		if (m->m_pkthdr.csum_flags & M_TCPV4_CSUM_OUT)
  			tcpstat.tcps_outhwcsum++;
--- 721,733 ----
  	 * If small enough for interface, can just send directly.
  	 */
  	if (ntohs(ip->ip_len) <= mtu) {
+ 		ip->ip_sum = 0;
  		if ((ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
  		    ifp->if_bridge == NULL) {
  			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
  			ipstat.ips_outhwcsum++;
! 		} else
  			ip->ip_sum = in_cksum(m, hlen);
  		/* Update relevant hardware checksum stats for TCP/UDP */
  		if (m->m_pkthdr.csum_flags & M_TCPV4_CSUM_OUT)
  			tcpstat.tcps_outhwcsum++;
*************** ip_fragment(struct mbuf *m, struct ifnet *ifp, u_long 
*** 871,885 ****
  		m->m_pkthdr.len = mhlen + len;
  		m->m_pkthdr.rcvif = (struct ifnet *)0;
  		mhip->ip_off = htons((u_int16_t)mhip->ip_off);
  		if ((ifp != NULL) &&
  		    (ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
  		    ifp->if_bridge == NULL) {
  			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
  			ipstat.ips_outhwcsum++;
! 		} else {
! 			mhip->ip_sum = 0;
  			mhip->ip_sum = in_cksum(m, mhlen);
- 		}
  		ipstat.ips_ofragments++;
  		fragments++;
  	}
--- 870,883 ----
  		m->m_pkthdr.len = mhlen + len;
  		m->m_pkthdr.rcvif = (struct ifnet *)0;
  		mhip->ip_off = htons((u_int16_t)mhip->ip_off);
+ 		mhip->ip_sum = 0;
  		if ((ifp != NULL) &&
  		    (ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
  		    ifp->if_bridge == NULL) {
  			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
  			ipstat.ips_outhwcsum++;
! 		} else
  			mhip->ip_sum = in_cksum(m, mhlen);
  		ipstat.ips_ofragments++;
  		fragments++;
  	}
*************** ip_fragment(struct mbuf *m, struct ifnet *ifp, u_long 
*** 892,906 ****
  	m->m_pkthdr.len = hlen + firstlen;
  	ip->ip_len = htons((u_int16_t)m->m_pkthdr.len);
  	ip->ip_off |= htons(IP_MF);
  	if ((ifp != NULL) &&
  	    (ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
  	    ifp->if_bridge == NULL) {
  		m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
  		ipstat.ips_outhwcsum++;
! 	} else {
! 		ip->ip_sum = 0;
  		ip->ip_sum = in_cksum(m, hlen);
- 	}
  sendorfree:
  	/*
  	 * If there is no room for all the fragments, don't queue
--- 890,903 ----
  	m->m_pkthdr.len = hlen + firstlen;
  	ip->ip_len = htons((u_int16_t)m->m_pkthdr.len);
  	ip->ip_off |= htons(IP_MF);
+ 	ip->ip_sum = 0;
  	if ((ifp != NULL) &&
  	    (ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
  	    ifp->if_bridge == NULL) {
  		m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
  		ipstat.ips_outhwcsum++;
! 	} else
  		ip->ip_sum = in_cksum(m, hlen);
  sendorfree:
  	/*
  	 * If there is no room for all the fragments, don't queue
Index: src/sys/netinet6/icmp6.c
===================================================================
RCS file: /cvs/src/sys/netinet6/icmp6.c,v
retrieving revision 1.103
retrieving revision 1.104
diff -c -p -r1.103 -r1.104
*** src/sys/netinet6/icmp6.c	18 Feb 2009 20:54:48 -0000	1.103
--- src/sys/netinet6/icmp6.c	22 Feb 2009 17:43:20 -0000	1.104
***************
*** 1,4 ****
! /*	$OpenBSD: icmp6.c,v 1.103 2009/02/18 20:54:48 claudio Exp $	*/
  /*	$KAME: icmp6.c,v 1.217 2001/06/20 15:03:29 jinmei Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: icmp6.c,v 1.104 2009/02/22 17:43:20 claudio Exp $	*/
  /*	$KAME: icmp6.c,v 1.217 2001/06/20 15:03:29 jinmei Exp $	*/
  
  /*
*************** icmp6_input(struct mbuf **mp, int *offp, int proto)
*** 520,525 ****
--- 520,526 ----
  	case ICMP6_PACKET_TOO_BIG:
  		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_pkttoobig);
  
+ 		/* MTU is checked in icmp6_mtudisc_update. */
  		code = PRC_MSGSIZE;
  
  		/*
*************** icmp6_mtudisc_update(struct ip6ctlparam *ip6cp, int va
*** 1106,1111 ****
--- 1107,1119 ----
  	u_int mtu = ntohl(icmp6->icmp6_mtu);
  	struct rtentry *rt = NULL;
  	struct sockaddr_in6 sin6;
+ 
+ 	/*
+ 	 * The MTU may not be less then the minimal IPv6 MTU except for the
+ 	 * hack in ip6_output/ip6_setpmtu where we always include a frag header.
+ 	 */
+ 	if (mtu < IPV6_MMTU - sizeof(struct ip6_frag))
+ 		return;
  
  	/*
  	 * allow non-validated cases if memory is plenty, to make traffic
Index: src/sys/nfs/nfs_bio.c
===================================================================
RCS file: /cvs/src/sys/nfs/nfs_bio.c,v
retrieving revision 1.56
retrieving revision 1.57
diff -c -p -r1.56 -r1.57
*** src/sys/nfs/nfs_bio.c	19 Jan 2009 23:40:36 -0000	1.56
--- src/sys/nfs/nfs_bio.c	24 Jan 2009 23:30:42 -0000	1.57
***************
*** 1,4 ****
! /*	$OpenBSD: nfs_bio.c,v 1.56 2009/01/19 23:40:36 thib Exp $	*/
  /*	$NetBSD: nfs_bio.c,v 1.25.4.2 1996/07/08 20:47:04 jtc Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: nfs_bio.c,v 1.57 2009/01/24 23:30:42 thib Exp $	*/
  /*	$NetBSD: nfs_bio.c,v 1.25.4.2 1996/07/08 20:47:04 jtc Exp $	*/
  
  /*
***************
*** 46,51 ****
--- 46,52 ----
  #include <sys/kernel.h>
  #include <sys/namei.h>
  #include <sys/queue.h>
+ #include <sys/time.h>
  
  #include <uvm/uvm_extern.h>
  
*************** nfs_bioread(vp, uio, ioflag, cred)
*** 112,127 ****
  		error = VOP_GETATTR(vp, &vattr, cred, p);
  		if (error)
  			return (error);
! 		np->n_mtime = vattr.va_mtime.tv_sec;
  	} else {
  		error = VOP_GETATTR(vp, &vattr, cred, p);
  		if (error)
  			return (error);
! 		if (np->n_mtime != vattr.va_mtime.tv_sec) {
  			error = nfs_vinvalbuf(vp, V_SAVE, cred, p);
  			if (error)
  				return (error);
! 			np->n_mtime = vattr.va_mtime.tv_sec;
  		}
  	}
  
--- 113,128 ----
  		error = VOP_GETATTR(vp, &vattr, cred, p);
  		if (error)
  			return (error);
! 		np->n_mtime = vattr.va_mtime;
  	} else {
  		error = VOP_GETATTR(vp, &vattr, cred, p);
  		if (error)
  			return (error);
! 		if (timespeccmp(&np->n_mtime, &vattr.va_mtime, !=)) {
  			error = nfs_vinvalbuf(vp, V_SAVE, cred, p);
  			if (error)
  				return (error);
! 			np->n_mtime = vattr.va_mtime;
  		}
  	}
  
*************** nfs_doio(bp, p)
*** 638,644 ****
  			bp->b_validend = bp->b_bcount;
  		}
  		if (p && (vp->v_flag & VTEXT) &&
! 		    (np->n_mtime != np->n_vattr.va_mtime.tv_sec)) {
  			uprintf("Process killed due to text file modification\n");
  			psignal(p, SIGKILL);
  		}
--- 639,645 ----
  			bp->b_validend = bp->b_bcount;
  		}
  		if (p && (vp->v_flag & VTEXT) &&
! 		    (timespeccmp(&np->n_mtime, &np->n_vattr.va_mtime, !=))) {
  			uprintf("Process killed due to text file modification\n");
  			psignal(p, SIGKILL);
  		}
Index: src/sys/nfs/nfs_bio.c
===================================================================
RCS file: /cvs/src/sys/nfs/nfs_bio.c,v
retrieving revision 1.55
retrieving revision 1.56
diff -c -p -r1.55 -r1.56
*** src/sys/nfs/nfs_bio.c	9 Aug 2008 10:14:02 -0000	1.55
--- src/sys/nfs/nfs_bio.c	19 Jan 2009 23:40:36 -0000	1.56
***************
*** 1,4 ****
! /*	$OpenBSD: nfs_bio.c,v 1.55 2008/08/09 10:14:02 thib Exp $	*/
  /*	$NetBSD: nfs_bio.c,v 1.25.4.2 1996/07/08 20:47:04 jtc Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: nfs_bio.c,v 1.56 2009/01/19 23:40:36 thib Exp $	*/
  /*	$NetBSD: nfs_bio.c,v 1.25.4.2 1996/07/08 20:47:04 jtc Exp $	*/
  
  /*
*************** nfs_bioread(vp, uio, ioflag, cred)
*** 106,118 ****
  	 * server, so flush all of the file's data out of the cache.
  	 * Then force a getattr rpc to ensure that you have up to date
  	 * attributes.
- 	 * NB: This implies that cache data can be read when up to
- 	 * NFS_ATTRTIMEO seconds out of date. If you find that you need current
- 	 * attributes this could be forced by setting n_attrstamp to 0 before
- 	 * the VOP_GETATTR() call.
  	 */
  	if (np->n_flag & NMODIFIED) {
! 		np->n_attrstamp = 0;
  		error = VOP_GETATTR(vp, &vattr, cred, p);
  		if (error)
  			return (error);
--- 106,114 ----
  	 * server, so flush all of the file's data out of the cache.
  	 * Then force a getattr rpc to ensure that you have up to date
  	 * attributes.
  	 */
  	if (np->n_flag & NMODIFIED) {
! 		NFS_INVALIDATE_ATTRCACHE(np);
  		error = VOP_GETATTR(vp, &vattr, cred, p);
  		if (error)
  			return (error);
*************** nfs_write(v)
*** 305,317 ****
  		(void)nfs_fsinfo(nmp, vp, cred, p);
  	if (ioflag & (IO_APPEND | IO_SYNC)) {
  		if (np->n_flag & NMODIFIED) {
! 			np->n_attrstamp = 0;
  			error = nfs_vinvalbuf(vp, V_SAVE, cred, p);
  			if (error)
  				return (error);
  		}
  		if (ioflag & IO_APPEND) {
! 			np->n_attrstamp = 0;
  			error = VOP_GETATTR(vp, &vattr, cred, p);
  			if (error)
  				return (error);
--- 301,313 ----
  		(void)nfs_fsinfo(nmp, vp, cred, p);
  	if (ioflag & (IO_APPEND | IO_SYNC)) {
  		if (np->n_flag & NMODIFIED) {
! 			NFS_INVALIDATE_ATTRCACHE(np);
  			error = nfs_vinvalbuf(vp, V_SAVE, cred, p);
  			if (error)
  				return (error);
  		}
  		if (ioflag & IO_APPEND) {
! 			NFS_INVALIDATE_ATTRCACHE(np);
  			error = VOP_GETATTR(vp, &vattr, cred, p);
  			if (error)
  				return (error);
Index: src/sys/nfs/nfs_kq.c
===================================================================
RCS file: /cvs/src/sys/nfs/nfs_kq.c,v
retrieving revision 1.14
retrieving revision 1.15
diff -c -p -r1.14 -r1.15
*** src/sys/nfs/nfs_kq.c	11 Sep 2008 16:06:01 -0000	1.14
--- src/sys/nfs/nfs_kq.c	19 Jan 2009 23:40:36 -0000	1.15
***************
*** 1,4 ****
! /*	$OpenBSD: nfs_kq.c,v 1.14 2008/09/11 16:06:01 thib Exp $ */
  /*	$NetBSD: nfs_kq.c,v 1.7 2003/10/30 01:43:10 simonb Exp $	*/
  
  /*-
--- 1,4 ----
! /*	$OpenBSD: nfs_kq.c,v 1.15 2009/01/19 23:40:36 thib Exp $ */
  /*	$NetBSD: nfs_kq.c,v 1.7 2003/10/30 01:43:10 simonb Exp $	*/
  
  /*-
*************** nfs_kqpoll(void *arg)
*** 126,132 ****
  
  			error = VOP_GETATTR(ke->vp, &attr, p->p_ucred, p);
  			if (error == ESTALE) {
! 				np->n_attrstamp = 0;
  				VN_KNOTE(ke->vp, NOTE_DELETE);
  				goto next;
  			}
--- 126,132 ----
  
  			error = VOP_GETATTR(ke->vp, &attr, p->p_ucred, p);
  			if (error == ESTALE) {
! 				NFS_INVALIDATE_ATTRCACHE(np);
  				VN_KNOTE(ke->vp, NOTE_DELETE);
  				goto next;
  			}
Index: src/sys/nfs/nfs_subs.c
===================================================================
RCS file: /cvs/src/sys/nfs/nfs_subs.c,v
retrieving revision 1.91
retrieving revision 1.92
diff -c -p -r1.91 -r1.92
*** src/sys/nfs/nfs_subs.c	20 Jan 2009 18:03:33 -0000	1.91
--- src/sys/nfs/nfs_subs.c	24 Jan 2009 23:30:42 -0000	1.92
***************
*** 1,4 ****
! /*	$OpenBSD: nfs_subs.c,v 1.91 2009/01/20 18:03:33 blambert Exp $	*/
  /*	$NetBSD: nfs_subs.c,v 1.27.4.3 1996/07/08 20:34:24 jtc Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: nfs_subs.c,v 1.92 2009/01/24 23:30:42 thib Exp $	*/
  /*	$NetBSD: nfs_subs.c,v 1.27.4.3 1996/07/08 20:34:24 jtc Exp $	*/
  
  /*
*************** nfs_loadattrcache(vpp, mdp, dposp, vaper)
*** 1063,1069 ****
  				*vpp = vp = nvp;
  			}
  		}
! 		np->n_mtime = mtime.tv_sec;
  	}
  	vap = &np->n_vattr;
  	vap->va_type = vtyp;
--- 1063,1069 ----
  				*vpp = vp = nvp;
  			}
  		}
! 		np->n_mtime = mtime;
  	}
  	vap = &np->n_vattr;
  	vap->va_type = vtyp;
*************** nfs_attrtimeo (np)
*** 1155,1161 ****
  {
  	struct vnode *vp = np->n_vnode;
  	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
! 	int tenthage = (time_second - np->n_mtime) / 10;
  	int minto, maxto;
  
  	if (vp->v_type == VDIR) {
--- 1155,1161 ----
  {
  	struct vnode *vp = np->n_vnode;
  	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
! 	int tenthage = (time_second - np->n_mtime.tv_sec) / 10;
  	int minto, maxto;
  
  	if (vp->v_type == VDIR) {
Index: src/sys/nfs/nfs_vnops.c
===================================================================
RCS file: /cvs/src/sys/nfs/nfs_vnops.c,v
retrieving revision 1.109
retrieving revision 1.110
diff -c -p -r1.109 -r1.110
*** src/sys/nfs/nfs_vnops.c	24 Jan 2009 23:25:17 -0000	1.109
--- src/sys/nfs/nfs_vnops.c	24 Jan 2009 23:30:42 -0000	1.110
***************
*** 1,4 ****
! /*	$OpenBSD: nfs_vnops.c,v 1.109 2009/01/24 23:25:17 thib Exp $	*/
  /*	$NetBSD: nfs_vnops.c,v 1.62.4.1 1996/07/08 20:26:52 jtc Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: nfs_vnops.c,v 1.110 2009/01/24 23:30:42 thib Exp $	*/
  /*	$NetBSD: nfs_vnops.c,v 1.62.4.1 1996/07/08 20:26:52 jtc Exp $	*/
  
  /*
*************** nfs_open(v)
*** 406,424 ****
  		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
  		if (error)
  			return (error);
! 		np->n_mtime = vattr.va_mtime.tv_sec;
  	} else {
  		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
  		if (error)
  			return (error);
! 		if (np->n_mtime != vattr.va_mtime.tv_sec) {
  			if (vp->v_type == VDIR)
  				np->n_direofoffset = 0;
  			error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p);
  			if (error == EINTR)
  				return (error);
  			uvm_vnp_uncache(vp);
! 			np->n_mtime = vattr.va_mtime.tv_sec;
  		}
  	}
  	/* For open/close consistency. */
--- 406,424 ----
  		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
  		if (error)
  			return (error);
! 		np->n_mtime = vattr.va_mtime;
  	} else {
  		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
  		if (error)
  			return (error);
! 		if (timespeccmp(&np->n_mtime, &vattr.va_mtime, !=)) {
  			if (vp->v_type == VDIR)
  				np->n_direofoffset = 0;
  			error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p);
  			if (error == EINTR)
  				return (error);
  			uvm_vnp_uncache(vp);
! 			np->n_mtime = vattr.va_mtime;
  		}
  	}
  	/* For open/close consistency. */
*************** nfs_writerpc(vp, uiop, iomode, must_commit)
*** 1131,1137 ****
  		} else
  		    nfsm_loadattr(vp, (struct vattr *)0);
  		if (wccflag)
! 		    VTONFS(vp)->n_mtime = VTONFS(vp)->n_vattr.va_mtime.tv_sec;
  		m_freem(mrep);
  		tsiz -= len;
  	}
--- 1131,1137 ----
  		} else
  		    nfsm_loadattr(vp, (struct vattr *)0);
  		if (wccflag)
! 		    VTONFS(vp)->n_mtime = VTONFS(vp)->n_vattr.va_mtime;
  		m_freem(mrep);
  		tsiz -= len;
  	}
*************** nfs_readdir(v)
*** 1889,1895 ****
  	if (np->n_direofoffset != 0 && 
  	    uio->uio_offset == np->n_direofoffset) {
  		if (VOP_GETATTR(vp, &vattr, ap->a_cred, uio->uio_procp) == 0 &&
! 		    np->n_mtime == vattr.va_mtime.tv_sec) {
  			nfsstats.direofcache_hits++;
  			*ap->a_eofflag = 1;
  			return (0);
--- 1889,1895 ----
  	if (np->n_direofoffset != 0 && 
  	    uio->uio_offset == np->n_direofoffset) {
  		if (VOP_GETATTR(vp, &vattr, ap->a_cred, uio->uio_procp) == 0 &&
! 		    timespeccmp(&np->n_mtime, &vattr.va_mtime, ==)) {
  			nfsstats.direofcache_hits++;
  			*ap->a_eofflag = 1;
  			return (0);
Index: src/sys/nfs/nfs_vnops.c
===================================================================
RCS file: /cvs/src/sys/nfs/nfs_vnops.c,v
retrieving revision 1.108
retrieving revision 1.109
diff -c -p -r1.108 -r1.109
*** src/sys/nfs/nfs_vnops.c	19 Jan 2009 23:40:36 -0000	1.108
--- src/sys/nfs/nfs_vnops.c	24 Jan 2009 23:25:17 -0000	1.109
***************
*** 1,4 ****
! /*	$OpenBSD: nfs_vnops.c,v 1.108 2009/01/19 23:40:36 thib Exp $	*/
  /*	$NetBSD: nfs_vnops.c,v 1.62.4.1 1996/07/08 20:26:52 jtc Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: nfs_vnops.c,v 1.109 2009/01/24 23:25:17 thib Exp $	*/
  /*	$NetBSD: nfs_vnops.c,v 1.62.4.1 1996/07/08 20:26:52 jtc Exp $	*/
  
  /*
***************
*** 79,84 ****
--- 79,86 ----
  #include <netinet/in.h>
  #include <netinet/in_var.h>
  
+ #include <dev/rndvar.h>
+ 
  void nfs_cache_enter(struct vnode *, struct vnode *, struct componentname *);
  
  /*
*************** nfs_mknod(v)
*** 1251,1260 ****
  	return (error);
  }
  
- static u_long create_verf;
- /*
-  * nfs file create call
-  */
  int
  nfs_create(v)
  	void *v;
--- 1253,1258 ----
*************** nfs_create(v)
*** 1279,1288 ****
  	if (vap->va_type == VSOCK)
  		return (nfs_mknodrpc(dvp, ap->a_vpp, cnp, vap));
  
- #ifdef VA_EXCLUSIVE
  	if (vap->va_vaflags & VA_EXCLUSIVE)
  		fmode |= O_EXCL;
! #endif
  again:
  	nfsstats.rpccnt[NFSPROC_CREATE]++;
  	mb = mreq = nfsm_reqhead(NFSX_FH(v3) + 2 * NFSX_UNSIGNED +
--- 1277,1285 ----
  	if (vap->va_type == VSOCK)
  		return (nfs_mknodrpc(dvp, ap->a_vpp, cnp, vap));
  
  	if (vap->va_vaflags & VA_EXCLUSIVE)
  		fmode |= O_EXCL;
! 
  again:
  	nfsstats.rpccnt[NFSPROC_CREATE]++;
  	mb = mreq = nfsm_reqhead(NFSX_FH(v3) + 2 * NFSX_UNSIGNED +
*************** again:
*** 1292,1304 ****
  	if (v3) {
  		tl = nfsm_build(&mb, NFSX_UNSIGNED);
  		if (fmode & O_EXCL) {
  			*tl = txdr_unsigned(NFSV3CREATE_EXCLUSIVE);
  			tl = nfsm_build(&mb, NFSX_V3CREATEVERF);
! 			if (TAILQ_FIRST(&in_ifaddr))
! 				*tl++ = TAILQ_FIRST(&in_ifaddr)->ia_addr.sin_addr.s_addr;
! 			else
! 				*tl++ = create_verf;
! 			*tl = ++create_verf;
  		} else {
  			*tl = txdr_unsigned(NFSV3CREATE_UNCHECKED);
  			nfsm_v3attrbuild(&mb, vap, 0);
--- 1289,1299 ----
  	if (v3) {
  		tl = nfsm_build(&mb, NFSX_UNSIGNED);
  		if (fmode & O_EXCL) {
+ 			printf("%s: O_EXCL!\n", __func__);
  			*tl = txdr_unsigned(NFSV3CREATE_EXCLUSIVE);
  			tl = nfsm_build(&mb, NFSX_V3CREATEVERF);
! 			*tl++ = arc4random();
! 			*tl = arc4random();
  		} else {
  			*tl = txdr_unsigned(NFSV3CREATE_UNCHECKED);
  			nfsm_v3attrbuild(&mb, vap, 0);
Index: src/sys/nfs/nfs_vnops.c
===================================================================
RCS file: /cvs/src/sys/nfs/nfs_vnops.c,v
retrieving revision 1.107
retrieving revision 1.108
diff -c -p -r1.107 -r1.108
*** src/sys/nfs/nfs_vnops.c	18 Jan 2009 15:42:31 -0000	1.107
--- src/sys/nfs/nfs_vnops.c	19 Jan 2009 23:40:36 -0000	1.108
***************
*** 1,4 ****
! /*	$OpenBSD: nfs_vnops.c,v 1.107 2009/01/18 15:42:31 bluhm Exp $	*/
  /*	$NetBSD: nfs_vnops.c,v 1.62.4.1 1996/07/08 20:26:52 jtc Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: nfs_vnops.c,v 1.108 2009/01/19 23:40:36 thib Exp $	*/
  /*	$NetBSD: nfs_vnops.c,v 1.62.4.1 1996/07/08 20:26:52 jtc Exp $	*/
  
  /*
*************** nfs_open(v)
*** 398,404 ****
  		if (error == EINTR)
  			return (error);
  		uvm_vnp_uncache(vp);
! 		np->n_attrstamp = 0;
  		if (vp->v_type == VDIR)
  			np->n_direofoffset = 0;
  		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
--- 398,404 ----
  		if (error == EINTR)
  			return (error);
  		uvm_vnp_uncache(vp);
! 		NFS_INVALIDATE_ATTRCACHE(np);
  		if (vp->v_type == VDIR)
  			np->n_direofoffset = 0;
  		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
*************** nfs_open(v)
*** 419,425 ****
  			np->n_mtime = vattr.va_mtime.tv_sec;
  		}
  	}
! 	np->n_attrstamp = 0; /* For Open/Close consistency */
  	return (0);
  }
  
--- 419,426 ----
  			np->n_mtime = vattr.va_mtime.tv_sec;
  		}
  	}
! 	/* For open/close consistency. */
! 	NFS_INVALIDATE_ATTRCACHE(np);
  	return (0);
  }
  
*************** nfs_close(v)
*** 466,472 ****
  		    np->n_flag &= ~NMODIFIED;
  		} else
  		    error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p);
! 		np->n_attrstamp = 0;
  	    }
  	    if (np->n_flag & NWRITEERR) {
  		np->n_flag &= ~NWRITEERR;
--- 467,473 ----
  		    np->n_flag &= ~NMODIFIED;
  		} else
  		    error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p);
! 		NFS_INVALIDATE_ATTRCACHE(np);
  	    }
  	    if (np->n_flag & NWRITEERR) {
  		np->n_flag &= ~NWRITEERR;
*************** nfsmout: 
*** 1224,1230 ****
  	pool_put(&namei_pool, cnp->cn_pnbuf);
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		VTONFS(dvp)->n_attrstamp = 0;
  	vrele(dvp);
  	return (error);
  }
--- 1225,1231 ----
  	pool_put(&namei_pool, cnp->cn_pnbuf);
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
  	vrele(dvp);
  	return (error);
  }
*************** nfsmout: 
*** 1346,1352 ****
  	pool_put(&namei_pool, cnp->cn_pnbuf);
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		VTONFS(dvp)->n_attrstamp = 0;
  	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
  	vrele(dvp);
  	return (error);
--- 1347,1353 ----
  	pool_put(&namei_pool, cnp->cn_pnbuf);
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
  	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
  	vrele(dvp);
  	return (error);
*************** nfs_remove(v)
*** 1414,1420 ****
  	} else if (!np->n_sillyrename)
  		error = nfs_sillyrename(dvp, vp, cnp);
  	pool_put(&namei_pool, cnp->cn_pnbuf);
! 	np->n_attrstamp = 0;
  	vrele(dvp);
  	vrele(vp);
  
--- 1415,1421 ----
  	} else if (!np->n_sillyrename)
  		error = nfs_sillyrename(dvp, vp, cnp);
  	pool_put(&namei_pool, cnp->cn_pnbuf);
! 	NFS_INVALIDATE_ATTRCACHE(np);
  	vrele(dvp);
  	vrele(vp);
  
*************** nfs_removerpc(dvp, name, namelen, cred, proc)
*** 1466,1472 ****
  nfsmout: 
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		VTONFS(dvp)->n_attrstamp = 0;
  	return (error);
  }
  
--- 1467,1473 ----
  nfsmout: 
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
  	return (error);
  }
  
*************** nfsmout: 
*** 1589,1597 ****
  	VTONFS(fdvp)->n_flag |= NMODIFIED;
  	VTONFS(tdvp)->n_flag |= NMODIFIED;
  	if (!fwccflag)
! 		VTONFS(fdvp)->n_attrstamp = 0;
  	if (!twccflag)
! 		VTONFS(tdvp)->n_attrstamp = 0;
  	return (error);
  }
  
--- 1590,1598 ----
  	VTONFS(fdvp)->n_flag |= NMODIFIED;
  	VTONFS(tdvp)->n_flag |= NMODIFIED;
  	if (!fwccflag)
! 		NFS_INVALIDATE_ATTRCACHE(VTONFS(fdvp));
  	if (!twccflag)
! 		NFS_INVALIDATE_ATTRCACHE(VTONFS(tdvp));
  	return (error);
  }
  
*************** nfsmout: 
*** 1646,1654 ****
  	pool_put(&namei_pool, cnp->cn_pnbuf);
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!attrflag)
! 		VTONFS(vp)->n_attrstamp = 0;
  	if (!wccflag)
! 		VTONFS(dvp)->n_attrstamp = 0;
  
  	VN_KNOTE(vp, NOTE_LINK);
  	VN_KNOTE(dvp, NOTE_WRITE);
--- 1647,1655 ----
  	pool_put(&namei_pool, cnp->cn_pnbuf);
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!attrflag)
! 		NFS_INVALIDATE_ATTRCACHE(VTONFS(vp));
  	if (!wccflag)
! 		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
  
  	VN_KNOTE(vp, NOTE_LINK);
  	VN_KNOTE(dvp, NOTE_WRITE);
*************** nfsmout: 
*** 1707,1713 ****
  	pool_put(&namei_pool, cnp->cn_pnbuf);
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		VTONFS(dvp)->n_attrstamp = 0;
  	VN_KNOTE(dvp, NOTE_WRITE);
  	vrele(dvp);
  	return (error);
--- 1708,1714 ----
  	pool_put(&namei_pool, cnp->cn_pnbuf);
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
  	VN_KNOTE(dvp, NOTE_WRITE);
  	vrele(dvp);
  	return (error);
*************** nfs_mkdir(v)
*** 1762,1768 ****
  nfsmout: 
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		VTONFS(dvp)->n_attrstamp = 0;
  
  	if (error == 0 && newvp == NULL) {
  		error = nfs_lookitup(dvp, cnp->cn_nameptr, len, cnp->cn_cred,
--- 1763,1769 ----
  nfsmout: 
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
  
  	if (error == 0 && newvp == NULL) {
  		error = nfs_lookitup(dvp, cnp->cn_nameptr, len, cnp->cn_cred,
*************** nfsmout: 
*** 1824,1830 ****
  	pool_put(&namei_pool, cnp->cn_pnbuf);
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		VTONFS(dvp)->n_attrstamp = 0;
  
  	VN_KNOTE(dvp, NOTE_WRITE|NOTE_LINK);
  	VN_KNOTE(vp, NOTE_DELETE);
--- 1825,1831 ----
  	pool_put(&namei_pool, cnp->cn_pnbuf);
  	VTONFS(dvp)->n_flag |= NMODIFIED;
  	if (!wccflag)
! 		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
  
  	VN_KNOTE(dvp, NOTE_WRITE|NOTE_LINK);
  	VN_KNOTE(vp, NOTE_DELETE);
Index: src/sys/nfs/nfs_vnops.c
===================================================================
RCS file: /cvs/src/sys/nfs/nfs_vnops.c,v
retrieving revision 1.106
retrieving revision 1.107
diff -c -p -r1.106 -r1.107
*** src/sys/nfs/nfs_vnops.c	13 Jan 2009 19:44:20 -0000	1.106
--- src/sys/nfs/nfs_vnops.c	18 Jan 2009 15:42:31 -0000	1.107
***************
*** 1,4 ****
! /*	$OpenBSD: nfs_vnops.c,v 1.106 2009/01/13 19:44:20 grange Exp $	*/
  /*	$NetBSD: nfs_vnops.c,v 1.62.4.1 1996/07/08 20:26:52 jtc Exp $	*/
  
  /*
--- 1,4 ----
! /*	$OpenBSD: nfs_vnops.c,v 1.107 2009/01/18 15:42:31 bluhm Exp $	*/
  /*	$NetBSD: nfs_vnops.c,v 1.62.4.1 1996/07/08 20:26:52 jtc Exp $	*/
  
  /*
*************** nfs_writerpc(vp, uiop, iomode, must_commit)
*** 1046,1052 ****
  	int error = 0, len, tsiz, wccflag = NFSV3_WCCRATTR, rlen, commit;
  	int v3 = NFS_ISV3(vp), committed = NFSV3WRITE_FILESYNC;
  
! #ifndef DIAGNOSTIC
  	if (uiop->uio_iovcnt != 1)
  		panic("nfs: writerpc iovcnt > 1");
  #endif
--- 1046,1052 ----
  	int error = 0, len, tsiz, wccflag = NFSV3_WCCRATTR, rlen, commit;
  	int v3 = NFS_ISV3(vp), committed = NFSV3WRITE_FILESYNC;
  
! #ifdef DIAGNOSTIC
  	if (uiop->uio_iovcnt != 1)
  		panic("nfs: writerpc iovcnt > 1");
  #endif
*************** nfs_remove(v)
*** 1375,1381 ****
  	int error = 0;
  	struct vattr vattr;
  
! #ifndef DIAGNOSTIC
  	if ((cnp->cn_flags & HASBUF) == 0)
  		panic("nfs_remove: no name");
  	if (vp->v_usecount < 1)
--- 1375,1381 ----
  	int error = 0;
  	struct vattr vattr;
  
! #ifdef DIAGNOSTIC
  	if ((cnp->cn_flags & HASBUF) == 0)
  		panic("nfs_remove: no name");
  	if (vp->v_usecount < 1)
*************** nfs_rename(v)
*** 1486,1492 ****
  	struct componentname *fcnp = ap->a_fcnp;
  	int error;
  
! #ifndef DIAGNOSTIC
  	if ((tcnp->cn_flags & HASBUF) == 0 ||
  	    (fcnp->cn_flags & HASBUF) == 0)
  		panic("nfs_rename: no name");
--- 1486,1492 ----
  	struct componentname *fcnp = ap->a_fcnp;
  	int error;
  
! #ifdef DIAGNOSTIC
  	if ((tcnp->cn_flags & HASBUF) == 0 ||
  	    (fcnp->cn_flags & HASBUF) == 0)
  		panic("nfs_rename: no name");
*************** nfs_readdirrpc(struct vnode *vp, 
*** 2034,2040 ****
  	int attrflag;
  	int v3 = NFS_ISV3(vp);
  
! #ifndef DIAGNOSTIC
  	if (uiop->uio_iovcnt != 1 || 
  		(uiop->uio_resid & (NFS_DIRBLKSIZ - 1)))
  		panic("nfs readdirrpc bad uio");
--- 2034,2040 ----
  	int attrflag;
  	int v3 = NFS_ISV3(vp);
  
! #ifdef DIAGNOSTIC
  	if (uiop->uio_iovcnt != 1 || 
  		(uiop->uio_resid & (NFS_DIRBLKSIZ - 1)))
  		panic("nfs readdirrpc bad uio");
*************** nfs_readdirplusrpc(struct vnode *vp, struct uio *uiop,
*** 2223,2229 ****
  	int error = 0, tlen, more_dirs = 1, blksiz = 0, doit, bigenough = 1, i;
  	int attrflag, fhsize;
  
! #ifndef DIAGNOSTIC
  	if (uiop->uio_iovcnt != 1 || 
  		(uiop->uio_resid & (NFS_DIRBLKSIZ - 1)))
  		panic("nfs readdirplusrpc bad uio");
--- 2223,2229 ----
  	int error = 0, tlen, more_dirs = 1, blksiz = 0, doit, bigenough = 1, i;
  	int attrflag, fhsize;
  
! #ifdef DIAGNOSTIC
  	if (uiop->uio_iovcnt != 1 || 
  		(uiop->uio_resid & (NFS_DIRBLKSIZ - 1)))
  		panic("nfs readdirplusrpc bad uio");
*** dummy/btsco.c	2012-03-07 17:13:10.843997874 -0600
--- src/sys/dev/bluetooth/btsco.c	2012-03-07 17:16:57.277765810 -0600
***************
*** 0 ****
--- 1,1219 ----
+ /*	$OpenBSD: btsco.c,v 1.1 2008/11/24 22:31:19 uwe Exp $	*/
+ /*	$NetBSD: btsco.c,v 1.22 2008/08/06 15:01:23 plunky Exp $	*/
+ 
+ /*-
+  * Copyright (c) 2006 Itronix Inc.
+  * All rights reserved.
+  *
+  * Written by Iain Hibbert for Itronix Inc.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. The name of Itronix Inc. may not be used to endorse
+  *    or promote products derived from this software without specific
+  *    prior written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
+  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
+  * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
+  * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
+  * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
+  * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
+  * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
+  * ON ANY THEORY OF LIABILITY, WHETHER IN
+  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
+  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
+  * POSSIBILITY OF SUCH DAMAGE.
+  */
+ 
+ #include <sys/param.h>
+ #include <sys/audioio.h>
+ #include <sys/conf.h>
+ #include <sys/device.h>
+ #include <sys/fcntl.h>
+ #include <sys/ioctl.h>
+ #include <sys/kernel.h>
+ #include <sys/queue.h>
+ #include <sys/malloc.h>
+ #include <sys/mbuf.h>
+ #include <sys/proc.h>
+ #include <sys/socketvar.h>
+ #include <sys/systm.h>
+ #include <sys/timeout.h>
+ 
+ #include <netbt/bluetooth.h>
+ #include <netbt/rfcomm.h>
+ #include <netbt/sco.h>
+ 
+ #include <dev/audio_if.h>
+ #include <dev/auconv.h>
+ #include <dev/mulaw.h>
+ 
+ #include <dev/bluetooth/btdev.h>
+ #include <dev/bluetooth/btsco.h>
+ 
+ typedef struct audio_params audio_params_t;
+ 
+ struct audio_format {
+ 	void *driver_data;
+ 	int32_t mode;
+ 	u_int encoding;
+ 	u_int validbits;
+ 	u_int precision;
+ 	u_int channels;
+ 	u_int channel_mask;
+ #define	AUFMT_UNKNOWN_POSITION		0U
+ #define	AUFMT_FRONT_LEFT		0x00001U /* USB audio compatible */
+ #define	AUFMT_FRONT_RIGHT		0x00002U /* USB audio compatible */
+ #define	AUFMT_FRONT_CENTER		0x00004U /* USB audio compatible */
+ #define	AUFMT_LOW_FREQUENCY		0x00008U /* USB audio compatible */
+ #define	AUFMT_BACK_LEFT			0x00010U /* USB audio compatible */
+ #define	AUFMT_BACK_RIGHT		0x00020U /* USB audio compatible */
+ #define	AUFMT_FRONT_LEFT_OF_CENTER	0x00040U /* USB audio compatible */
+ #define	AUFMT_FRONT_RIGHT_OF_CENTER	0x00080U /* USB audio compatible */
+ #define	AUFMT_BACK_CENTER		0x00100U /* USB audio compatible */
+ #define	AUFMT_SIDE_LEFT			0x00200U /* USB audio compatible */
+ #define	AUFMT_SIDE_RIGHT		0x00400U /* USB audio compatible */
+ #define	AUFMT_TOP_CENTER		0x00800U /* USB audio compatible */
+ #define	AUFMT_TOP_FRONT_LEFT		0x01000U
+ #define	AUFMT_TOP_FRONT_CENTER		0x02000U
+ #define	AUFMT_TOP_FRONT_RIGHT		0x04000U
+ #define	AUFMT_TOP_BACK_LEFT		0x08000U
+ #define	AUFMT_TOP_BACK_CENTER		0x10000U
+ #define	AUFMT_TOP_BACK_RIGHT		0x20000U
+ 
+ #define	AUFMT_MONAURAL		AUFMT_FRONT_CENTER
+ #define	AUFMT_STEREO		(AUFMT_FRONT_LEFT | AUFMT_FRONT_RIGHT)
+ #define	AUFMT_SURROUND4		(AUFMT_STEREO | AUFMT_BACK_LEFT \
+ 				| AUFMT_BACK_RIGHT)
+ #define	AUFMT_DOLBY_5_1		(AUFMT_SURROUND4 | AUFMT_FRONT_CENTER \
+ 				| AUFMT_LOW_FREQUENCY)
+ 
+ 	/**
+ 	 * 0: frequency[0] is lower limit, and frequency[1] is higher limit.
+ 	 * 1-16: frequency[0] to frequency[frequency_type-1] are valid.
+ 	 */
+ 	u_int frequency_type;
+ 
+ #define	AUFMT_MAX_FREQUENCIES	16
+ 	/**
+ 	 * sampling rates
+ 	 */
+ 	u_int frequency[AUFMT_MAX_FREQUENCIES];
+ };
+ 
+ #undef DPRINTF
+ #undef DPRINTFN
+ 
+ #ifdef BTSCO_DEBUG
+ int btsco_debug = BTSCO_DEBUG;
+ #define DPRINTF(fmt, args...)		do {		\
+ 	if (btsco_debug)				\
+ 		printf("%s: "fmt, __func__ , ##args);	\
+ } while (/* CONSTCOND */0)
+ 
+ #define DPRINTFN(n, fmt, args...)	do {		\
+ 	if (btsco_debug > (n))				\
+ 		printf("%s: "fmt, __func__ , ##args);	\
+ } while (/* CONSTCOND */0)
+ #else
+ #define DPRINTF(...)
+ #define DPRINTFN(...)
+ #endif
+ 
+ /*****************************************************************************
+  *
+  *	Bluetooth SCO Audio device
+  */
+ 
+ /* btsco softc */
+ struct btsco_softc {
+ 	struct device		 sc_dev;
+ 	struct device		*sc_audio;	/* MI audio device */
+ 	uint16_t		 sc_flags;
+ 	const char		*sc_name;	/* our device_xname */
+ 
+ 	void			*sc_intr;	/* interrupt cookie */
+ 	struct timeout		 sc_intr_to;	/* interrupt trigger */
+ 	int			 sc_connect;	/* connect wait */
+ 
+ 	/* Bluetooth */
+ 	bdaddr_t		 sc_laddr;	/* local address */
+ 	bdaddr_t		 sc_raddr;	/* remote address */
+ 	uint16_t		 sc_state;	/* link state */
+ 	struct sco_pcb		*sc_sco;	/* SCO handle */
+ 	struct sco_pcb		*sc_sco_l;	/* SCO listen handle */
+ 	uint16_t		 sc_mtu;	/* SCO mtu */
+ 	uint8_t			 sc_channel;	/* RFCOMM channel */
+ 	int			 sc_err;	/* stored error */
+ 
+ 	/* Receive */
+ 	int			 sc_rx_want;	/* bytes wanted */
+ 	uint8_t			*sc_rx_block;	/* receive block */
+ 	void		       (*sc_rx_intr)(void *);	/* callback */
+ 	void			*sc_rx_intrarg;	/* callback arg */
+ 	struct mbuf		*sc_rx_mbuf;	/* leftover mbuf */
+ 
+ 	/* Transmit */
+ 	int			 sc_tx_size;	/* bytes to send */
+ 	int			 sc_tx_pending;	/* packets pending */
+ 	uint8_t			*sc_tx_block;	/* transmit block */
+ 	void		       (*sc_tx_intr)(void *);	/* callback */
+ 	void			*sc_tx_intrarg;	/* callback arg */
+ 	void			*sc_tx_buf;	/* transmit buffer */
+ 	int			 sc_tx_refcnt;	/* buffer refcnt */
+ 
+ 	/* mixer data */
+ 	int			 sc_vgs;	/* speaker volume */
+ 	int			 sc_vgm;	/* mic volume */
+ };
+ 
+ /* sc_state */
+ #define BTSCO_CLOSED		0
+ #define BTSCO_WAIT_CONNECT	1
+ #define BTSCO_OPEN		2
+ 
+ /* sc_flags */
+ #define BTSCO_LISTEN		(1 << 1)
+ 
+ /* autoconf(9) glue */
+ int  btsco_match(struct device *, void *, void *);
+ void btsco_attach(struct device *, struct device *, void *);
+ int  btsco_detach(struct device *, int);
+ 
+ struct cfattach btsco_ca = {
+ 	sizeof(struct btsco_softc),
+ 	btsco_match,
+ 	btsco_attach,
+ 	btsco_detach
+ };
+ 
+ struct cfdriver btsco_cd = {
+ 	NULL, "btsco", DV_DULL
+ };
+ 
+ /* audio(9) glue */
+ static int btsco_open(void *, int);
+ static void btsco_close(void *);
+ static int btsco_query_encoding(void *, struct audio_encoding *);
+ static int btsco_set_params(void *, int, int, audio_params_t *, audio_params_t *);
+ static int btsco_round_blocksize(void *, int);
+ static int btsco_start_output(void *, void *, int, void (*)(void *), void *);
+ static int btsco_start_input(void *, void *, int, void (*)(void *), void *);
+ static int btsco_halt_output(void *);
+ static int btsco_halt_input(void *);
+ static int btsco_getdev(void *, struct audio_device *);
+ static int btsco_setfd(void *, int);
+ static int btsco_set_port(void *, mixer_ctrl_t *);
+ static int btsco_get_port(void *, mixer_ctrl_t *);
+ static int btsco_query_devinfo(void *, mixer_devinfo_t *);
+ static void *btsco_allocm(void *, int, size_t, int, int);
+ static void btsco_freem(void *, void *, int);
+ static int btsco_get_props(void *);
+ #ifdef notyet
+ static int btsco_dev_ioctl(void *, u_long, void *, int, struct proc *);
+ #endif
+ 
+ struct audio_hw_if btsco_if = {
+ 	btsco_open,		/* open */
+ 	btsco_close,		/* close */
+ 	NULL,			/* drain */
+ 	btsco_query_encoding,	/* query_encoding */
+ 	btsco_set_params,	/* set_params */
+ 	btsco_round_blocksize,	/* round_blocksize */
+ 	NULL,			/* commit_settings */
+ 	NULL,			/* init_output */
+ 	NULL,			/* init_input */
+ 	btsco_start_output,	/* start_output */
+ 	btsco_start_input,	/* start_input */
+ 	btsco_halt_output,	/* halt_output */
+ 	btsco_halt_input,	/* halt_input */
+ 	NULL,			/* speaker_ctl */
+ 	btsco_getdev,		/* getdev */
+ 	btsco_setfd,		/* setfd */
+ 	btsco_set_port,		/* set_port */
+ 	btsco_get_port,		/* get_port */
+ 	btsco_query_devinfo,	/* query_devinfo */
+ 	btsco_allocm,		/* allocm */
+ 	btsco_freem,		/* freem */
+ 	NULL,			/* round_buffersize */
+ 	NULL,			/* mappage */
+ 	btsco_get_props,	/* get_props */
+ 	NULL,			/* trigger_output */
+ 	NULL,			/* trigger_input */
+ 	NULL			/* get_default_params */
+ };
+ 
+ static const struct audio_device btsco_device = {
+ 	"Bluetooth Audio",
+ 	"",
+ 	"btsco"
+ };
+ 
+ /* Voice_Setting == 0x0060: 8000Hz, mono, 16-bit, slinear_le */
+ static const struct audio_format btsco_format = {
+ 	NULL,				/* driver_data */
+ 	(AUMODE_PLAY | AUMODE_RECORD),	/* mode */
+ 	AUDIO_ENCODING_SLINEAR_LE,	/* encoding */
+ 	16,				/* validbits */
+ 	16,				/* precision */
+ 	1,				/* channels */
+ 	AUFMT_MONAURAL,			/* channel_mask */
+ 	1,				/* frequency_type */
+ 	{ 8000 }			/* frequency */
+ };
+ 
+ /* bluetooth(9) glue for SCO */
+ static void  btsco_sco_connecting(void *);
+ static void  btsco_sco_connected(void *);
+ static void  btsco_sco_disconnected(void *, int);
+ static void *btsco_sco_newconn(void *, struct sockaddr_bt *, struct sockaddr_bt *);
+ static void  btsco_sco_complete(void *, int);
+ static void  btsco_sco_linkmode(void *, int);
+ static void  btsco_sco_input(void *, struct mbuf *);
+ 
+ static const struct btproto btsco_sco_proto = {
+ 	btsco_sco_connecting,
+ 	btsco_sco_connected,
+ 	btsco_sco_disconnected,
+ 	btsco_sco_newconn,
+ 	btsco_sco_complete,
+ 	btsco_sco_linkmode,
+ 	btsco_sco_input,
+ };
+ 
+ 
+ /*****************************************************************************
+  *
+  *	btsco definitions
+  */
+ 
+ /*
+  * btsco mixer class
+  */
+ #define BTSCO_VGS		0
+ #define BTSCO_VGM		1
+ #define BTSCO_INPUT_CLASS	2
+ #define BTSCO_OUTPUT_CLASS	3
+ 
+ /* connect timeout */
+ #define BTSCO_TIMEOUT		(30 * hz)
+ 
+ /* misc btsco functions */
+ static void btsco_extfree(caddr_t, u_int, void *);
+ static void btsco_intr(void *);
+ 
+ 
+ /*****************************************************************************
+  *
+  *	btsco autoconf(9) routines
+  */
+ 
+ int
+ btsco_match(struct device *self, void *cfdata, void *aux)
+ {
+ 	struct btdev_attach_args *bda = (struct btdev_attach_args *)aux;
+ 
+ 	if (bda->bd_type == BTDEV_HSET || bda->bd_type == BTDEV_HF)
+ 		return 1;
+ 
+ 	return 0;
+ }
+ 
+ void
+ btsco_attach(struct device *parent, struct device *self, void *aux)
+ {
+ 	struct btsco_softc *sc = (struct btsco_softc *)self;
+ 	struct btdev_attach_args *bda = aux;
+ 
+ 	/*
+ 	 * Init softc
+ 	 */
+ 	sc->sc_vgs = 200;
+ 	sc->sc_vgm = 200;
+ 	sc->sc_state = BTSCO_CLOSED;
+ 	sc->sc_name = self->dv_xname;
+ 
+ 	/*
+ 	 * copy in our configuration info
+ 	 */
+ 	bdaddr_copy(&sc->sc_laddr, &bda->bd_laddr);
+ 	bdaddr_copy(&sc->sc_raddr, &bda->bd_raddr);
+ 
+ 	if (bda->bd_type == BTDEV_HF) {
+ 		sc->sc_flags |= BTSCO_LISTEN;
+ 		printf(" listen mode");
+ 	}
+ 
+ 	if (bda->bd_hset.hset_channel < RFCOMM_CHANNEL_MIN ||
+ 	    bda->bd_hset.hset_channel > RFCOMM_CHANNEL_MAX) {
+ 		printf(" invalid channel");
+ 		return;
+ 	}
+ 	sc->sc_channel = bda->bd_hset.hset_channel;
+ 
+ 	printf(" channel %d\n", sc->sc_channel);
+ 
+ 	/*
+ 	 * set up transmit interrupt
+ 	 */
+ 	timeout_set(&sc->sc_intr_to, btsco_intr, sc);
+ 
+ 	/*
+ 	 * attach audio device
+ 	 */
+ 	sc->sc_audio = audio_attach_mi(&btsco_if, sc, self);
+ 	if (sc->sc_audio == NULL) {
+ 		printf("%s: audio_attach_mi failed\n", sc->sc_dev.dv_xname);
+ 		return;
+ 	}
+ }
+ 
+ int
+ btsco_detach(struct device *self, int flags)
+ {
+ 	struct btsco_softc *sc = (struct btsco_softc *)self;
+ 
+ 	DPRINTF("sc=%p\n", sc);
+ 
+ 	mutex_enter(&bt_lock);
+ 	if (sc->sc_sco != NULL) {
+ 		DPRINTF("sc_sco=%p\n", sc->sc_sco);
+ 		sco_disconnect(sc->sc_sco, 0);
+ 		sco_detach(&sc->sc_sco);
+ 		sc->sc_sco = NULL;
+ 	}
+ 
+ 	if (sc->sc_sco_l != NULL) {
+ 		DPRINTF("sc_sco_l=%p\n", sc->sc_sco_l);
+ 		sco_detach(&sc->sc_sco_l);
+ 		sc->sc_sco_l = NULL;
+ 	}
+ 	mutex_exit(&bt_lock);
+ 
+ 	if (sc->sc_audio != NULL) {
+ 		DPRINTF("sc_audio=%p\n", sc->sc_audio);
+ 		config_detach(sc->sc_audio, flags);
+ 		sc->sc_audio = NULL;
+ 	}
+ 
+ 	timeout_del(&sc->sc_intr_to);
+ 
+ 	if (sc->sc_rx_mbuf != NULL) {
+ 		m_freem(sc->sc_rx_mbuf);
+ 		sc->sc_rx_mbuf = NULL;
+ 	}
+ 
+ 	if (sc->sc_tx_refcnt > 0) {
+ 		printf("%s: tx_refcnt=%d!\n", sc->sc_dev.dv_xname,
+ 		    sc->sc_tx_refcnt);
+ 
+ 		if ((flags & DETACH_FORCE) == 0)
+ 			return EAGAIN;
+ 	}
+ 
+ 	return 0;
+ }
+ 
+ /*****************************************************************************
+  *
+  *	bluetooth(9) methods for SCO
+  *
+  *	All these are called from Bluetooth Protocol code, in a soft
+  *	interrupt context at IPL_SOFTNET.
+  */
+ 
+ static void
+ btsco_sco_connecting(void *arg)
+ {
+ /*	struct btsco_softc *sc = arg;	*/
+ 
+ 	/* dont care */
+ }
+ 
+ static void
+ btsco_sco_connected(void *arg)
+ {
+ 	struct btsco_softc *sc = arg;
+ 
+ 	DPRINTF("%s\n", sc->sc_name);
+ 
+ 	KASSERT(sc->sc_sco != NULL);
+ 	KASSERT(sc->sc_state == BTSCO_WAIT_CONNECT);
+ 
+ 	/*
+ 	 * If we are listening, no more need
+ 	 */
+ 	if (sc->sc_sco_l != NULL)
+ 		sco_detach(&sc->sc_sco_l);
+ 
+ 	sc->sc_state = BTSCO_OPEN;
+ 	wakeup(&sc->sc_connect);
+ }
+ 
+ static void
+ btsco_sco_disconnected(void *arg, int err)
+ {
+ 	struct btsco_softc *sc = arg;
+ 	int s;
+ 
+ 	DPRINTF("%s sc_state %d\n", sc->sc_name, sc->sc_state);
+ 
+ 	KASSERT(sc->sc_sco != NULL);
+ 
+ 	sc->sc_err = err;
+ 	sco_detach(&sc->sc_sco);
+ 
+ 	switch (sc->sc_state) {
+ 	case BTSCO_CLOSED:		/* dont think this can happen */
+ 		break;
+ 
+ 	case BTSCO_WAIT_CONNECT:	/* connect failed */
+ 		wakeup(&sc->sc_connect);
+ 		break;
+ 
+ 	case BTSCO_OPEN:		/* link lost */
+ 		/*
+ 		 * If IO is in progress, tell the audio driver that it
+ 		 * has completed so that when it tries to send more, we
+ 		 * can indicate an error.
+ 		 */
+ 		s = splaudio();
+ 		if (sc->sc_tx_pending > 0) {
+ 			sc->sc_tx_pending = 0;
+ 			(*sc->sc_tx_intr)(sc->sc_tx_intrarg);
+ 		}
+ 		if (sc->sc_rx_want > 0) {
+ 			sc->sc_rx_want = 0;
+ 			(*sc->sc_rx_intr)(sc->sc_rx_intrarg);
+ 		}
+ 		splx(s);
+ 		break;
+ 
+ 	default:
+ 		UNKNOWN(sc->sc_state);
+ 	}
+ 
+ 	sc->sc_state = BTSCO_CLOSED;
+ }
+ 
+ static void *
+ btsco_sco_newconn(void *arg, struct sockaddr_bt *laddr,
+     struct sockaddr_bt *raddr)
+ {
+ 	struct btsco_softc *sc = arg;
+ 
+ 	DPRINTF("%s\n", sc->sc_name);
+ 
+ 	if (bdaddr_same(&raddr->bt_bdaddr, &sc->sc_raddr) == 0
+ 	    || sc->sc_state != BTSCO_WAIT_CONNECT
+ 	    || sc->sc_sco != NULL)
+ 	    return NULL;
+ 
+ 	sco_attach(&sc->sc_sco, &btsco_sco_proto, sc);
+ 	return sc->sc_sco;
+ }
+ 
+ static void
+ btsco_sco_complete(void *arg, int count)
+ {
+ 	struct btsco_softc *sc = arg;
+ 	int s;
+ 
+ 	DPRINTFN(10, "%s count %d\n", sc->sc_name, count);
+ 
+ 	s = splaudio();
+ 	if (sc->sc_tx_pending > 0) {
+ 		sc->sc_tx_pending -= count;
+ 		if (sc->sc_tx_pending == 0)
+ 			(*sc->sc_tx_intr)(sc->sc_tx_intrarg);
+ 	}
+ 	splx(s);
+ }
+ 
+ static void
+ btsco_sco_linkmode(void *arg, int new)
+ {
+ /*	struct btsco_softc *sc = arg;	*/
+ 
+ 	/* dont care */
+ }
+ 
+ static void
+ btsco_sco_input(void *arg, struct mbuf *m)
+ {
+ 	struct btsco_softc *sc = arg;
+ 	int len, s;
+ 
+ 	DPRINTFN(10, "%s len=%d\n", sc->sc_name, m->m_pkthdr.len);
+ 
+ 	s = splaudio();
+ 	if (sc->sc_rx_want == 0) {
+ 		m_freem(m);
+ 	} else {
+ 		KASSERT(sc->sc_rx_intr != NULL);
+ 		KASSERT(sc->sc_rx_block != NULL);
+ 
+ 		len = MIN(sc->sc_rx_want, m->m_pkthdr.len);
+ 		m_copydata(m, 0, len, sc->sc_rx_block);
+ 
+ 		sc->sc_rx_want -= len;
+ 		sc->sc_rx_block += len;
+ 
+ 		if (len > m->m_pkthdr.len) {
+ 			if (sc->sc_rx_mbuf != NULL)
+ 				m_freem(sc->sc_rx_mbuf);
+ 
+ 			m_adj(m, len);
+ 			sc->sc_rx_mbuf = m;
+ 		} else {
+ 			m_freem(m);
+ 		}
+ 
+ 		if (sc->sc_rx_want == 0)
+ 			(*sc->sc_rx_intr)(sc->sc_rx_intrarg);
+ 	}
+ 	splx(s);
+ }
+ 
+ 
+ /*****************************************************************************
+  *
+  *	audio(9) methods
+  *
+  */
+ 
+ static int
+ btsco_open(void *hdl, int flags)
+ {
+ 	struct sockaddr_bt sa;
+ 	struct btsco_softc *sc = hdl;
+ 	int err, timo;
+ 
+ 	DPRINTF("%s flags 0x%x\n", sc->sc_name, flags);
+ 	/* flags FREAD & FWRITE? */
+ 
+ 	if (sc->sc_sco != NULL || sc->sc_sco_l != NULL)
+ 		return EIO;
+ 
+ 	mutex_enter(&bt_lock);
+ 
+ 	memset(&sa, 0, sizeof(sa));
+ 	sa.bt_len = sizeof(sa);
+ 	sa.bt_family = AF_BLUETOOTH;
+ 	bdaddr_copy(&sa.bt_bdaddr, &sc->sc_laddr);
+ 
+ 	if (sc->sc_flags & BTSCO_LISTEN) {
+ 		err = sco_attach(&sc->sc_sco_l, &btsco_sco_proto, sc);
+ 		if (err)
+ 			goto done;
+ 
+ 		err = sco_bind(sc->sc_sco_l, &sa);
+ 		if (err) {
+ 			sco_detach(&sc->sc_sco_l);
+ 			goto done;
+ 		}
+ 
+ 		err = sco_listen(sc->sc_sco_l);
+ 		if (err) {
+ 			sco_detach(&sc->sc_sco_l);
+ 			goto done;
+ 		}
+ 
+ 		timo = 0;	/* no timeout */
+ 	} else {
+ 		err = sco_attach(&sc->sc_sco, &btsco_sco_proto, sc);
+ 		if (err)
+ 			goto done;
+ 
+ 		err = sco_bind(sc->sc_sco, &sa);
+ 		if (err) {
+ 			sco_detach(&sc->sc_sco);
+ 			goto done;
+ 		}
+ 
+ 		bdaddr_copy(&sa.bt_bdaddr, &sc->sc_raddr);
+ 		err = sco_connect(sc->sc_sco, &sa);
+ 		if (err) {
+ 			sco_detach(&sc->sc_sco);
+ 			goto done;
+ 		}
+ 
+ 		timo = BTSCO_TIMEOUT;
+ 	}
+ 
+ 	sc->sc_state = BTSCO_WAIT_CONNECT;
+ 	while (err == 0 && sc->sc_state == BTSCO_WAIT_CONNECT)
+ 		err = msleep(&sc->sc_connect, &bt_lock, PWAIT,
+ 		    "connect", timo);
+ 
+ 	switch (sc->sc_state) {
+ 	case BTSCO_CLOSED:		/* disconnected */
+ 		err = sc->sc_err;
+ 
+ 		/* fall through to */
+ 	case BTSCO_WAIT_CONNECT:	/* error */
+ 		if (sc->sc_sco != NULL)
+ 			sco_detach(&sc->sc_sco);
+ 
+ 		if (sc->sc_sco_l != NULL)
+ 			sco_detach(&sc->sc_sco_l);
+ 
+ 		break;
+ 
+ 	case BTSCO_OPEN:		/* hurrah */
+ 		(void)sco_getopt(sc->sc_sco, SO_SCO_MTU, &sc->sc_mtu);
+ 		break;
+ 
+ 	default:
+ 		UNKNOWN(sc->sc_state);
+ 		break;
+ 	}
+ 
+ done:
+ 	mutex_exit(&bt_lock);
+ 
+ 	DPRINTF("done err=%d, sc_state=%d, sc_mtu=%d\n",
+ 			err, sc->sc_state, sc->sc_mtu);
+ 	return err;
+ }
+ 
+ static void
+ btsco_close(void *hdl)
+ {
+ 	struct btsco_softc *sc = hdl;
+ 
+ 	DPRINTF("%s\n", sc->sc_name);
+ 
+ 	mutex_enter(&bt_lock);
+ 	if (sc->sc_sco != NULL) {
+ 		sco_disconnect(sc->sc_sco, 0);
+ 		sco_detach(&sc->sc_sco);
+ 	}
+ 
+ 	if (sc->sc_sco_l != NULL) {
+ 		sco_detach(&sc->sc_sco_l);
+ 	}
+ 	mutex_exit(&bt_lock);
+ 
+ 	if (sc->sc_rx_mbuf != NULL) {
+ 		m_freem(sc->sc_rx_mbuf);
+ 		sc->sc_rx_mbuf = NULL;
+ 	}
+ 
+ 	sc->sc_rx_want = 0;
+ 	sc->sc_rx_block = NULL;
+ 	sc->sc_rx_intr = NULL;
+ 	sc->sc_rx_intrarg = NULL;
+ 
+ 	sc->sc_tx_size = 0;
+ 	sc->sc_tx_block = NULL;
+ 	sc->sc_tx_pending = 0;
+ 	sc->sc_tx_intr = NULL;
+ 	sc->sc_tx_intrarg = NULL;
+ }
+ 
+ static int
+ btsco_query_encoding(void *hdl, struct audio_encoding *ae)
+ {
+ /*	struct btsco_softc *sc = hdl;	*/
+ 	int err = 0;
+ 
+ 	switch (ae->index) {
+ 	case 0:
+ 		strlcpy(ae->name, AudioEslinear_le, sizeof(ae->name));
+ 		ae->encoding = AUDIO_ENCODING_SLINEAR_LE;
+ 		ae->precision = 16;
+ 		ae->flags = 0;
+ 		break;
+ 
+ 	default:
+ 		err = EINVAL;
+ 	}
+ 
+ 	return err;
+ }
+ 
+ static int
+ btsco_set_params(void *hdl, int setmode, int usemode,
+     audio_params_t *play, audio_params_t *rec)
+ {
+ /*	struct btsco_softc *sc = hdl;	*/
+ 	const struct audio_format *f;
+ #if 0
+ 	int rv;
+ #endif
+ 
+ 	DPRINTF("setmode 0x%x usemode 0x%x\n", setmode, usemode);
+ 	DPRINTF("rate %d, precision %d, channels %d encoding %d\n",
+ 		play->sample_rate, play->precision, play->channels, play->encoding);
+ 
+ 	/*
+ 	 * If we had a list of formats, we could check the HCI_Voice_Setting
+ 	 * and select the appropriate one to use. Currently only one is
+ 	 * supported: 0x0060 == 8000Hz, mono, 16-bit, slinear_le
+ 	 */
+ 	f = &btsco_format;
+ 
+ #if 0
+ 	if (setmode & AUMODE_PLAY) {
+ 		rv = auconv_set_converter(f, 1, AUMODE_PLAY, play, TRUE, pfil);
+ 		if (rv < 0)
+ 			return EINVAL;
+ 	}
+ 
+ 	if (setmode & AUMODE_RECORD) {
+ 		rv = auconv_set_converter(f, 1, AUMODE_RECORD, rec, TRUE, rfil);
+ 		if (rv < 0)
+ 			return EINVAL;
+ 	}
+ #endif
+ 
+ 	return 0;
+ }
+ 
+ /*
+  * If we have an MTU value to use, round the blocksize to that.
+  */
+ static int
+ btsco_round_blocksize(void *hdl, int bs)
+ {
+ 	struct btsco_softc *sc = hdl;
+ 
+ 	if (sc->sc_mtu > 0) {
+ 		bs = (bs / sc->sc_mtu) * sc->sc_mtu;
+ 		if (bs == 0)
+ 			bs = sc->sc_mtu;
+ 	}
+ 
+ 	DPRINTF("%s mode=0x%x, bs=%d, sc_mtu=%d\n",
+ 			sc->sc_name, mode, bs, sc->sc_mtu);
+ 
+ 	return bs;
+ }
+ 
+ /*
+  * Start Output
+  *
+  * We dont want to be calling the network stack at splaudio() so make
+  * a note of what is to be sent, and schedule an interrupt to bundle
+  * it up and queue it.
+  */
+ static int
+ btsco_start_output(void *hdl, void *block, int blksize,
+ 		void (*intr)(void *), void *intrarg)
+ {
+ 	struct btsco_softc *sc = hdl;
+ 
+ 	DPRINTFN(5, "%s blksize %d\n", sc->sc_name, blksize);
+ 
+ 	if (sc->sc_sco == NULL)
+ 		return ENOTCONN;	/* connection lost */
+ 
+ 	sc->sc_tx_block = block;
+ 	sc->sc_tx_pending = 0;
+ 	sc->sc_tx_size = blksize;
+ 	sc->sc_tx_intr = intr;
+ 	sc->sc_tx_intrarg = intrarg;
+ 
+ 	timeout_add(&sc->sc_intr_to, 0);
+ 	return 0;
+ }
+ 
+ /*
+  * Start Input
+  *
+  * When the SCO link is up, we are getting data in any case, so all we do
+  * is note what we want and where to put it and let the sco_input routine
+  * fill in the data.
+  *
+  * If there was any leftover data that didnt fit in the last block, retry
+  * it now.
+  */
+ static int
+ btsco_start_input(void *hdl, void *block, int blksize,
+ 		void (*intr)(void *), void *intrarg)
+ {
+ 	struct btsco_softc *sc = hdl;
+ 	struct mbuf *m;
+ 
+ 	DPRINTFN(5, "%s blksize %d\n", sc->sc_name, blksize);
+ 
+ 	if (sc->sc_sco == NULL)
+ 		return ENOTCONN;
+ 
+ 	sc->sc_rx_want = blksize;
+ 	sc->sc_rx_block = block;
+ 	sc->sc_rx_intr = intr;
+ 	sc->sc_rx_intrarg = intrarg;
+ 
+ 	if (sc->sc_rx_mbuf != NULL) {
+ 		m = sc->sc_rx_mbuf;
+ 		sc->sc_rx_mbuf = NULL;
+ 		btsco_sco_input(sc, m);
+ 	}
+ 
+ 	return 0;
+ }
+ 
+ /*
+  * Halt Output
+  *
+  * This doesnt really halt the output, but it will look
+  * that way to the audio driver. The current block will
+  * still be transmitted.
+  */
+ static int
+ btsco_halt_output(void *hdl)
+ {
+ 	struct btsco_softc *sc = hdl;
+ 
+ 	DPRINTFN(5, "%s\n", sc->sc_name);
+ 
+ 	sc->sc_tx_size = 0;
+ 	sc->sc_tx_block = NULL;
+ 	sc->sc_tx_pending = 0;
+ 	sc->sc_tx_intr = NULL;
+ 	sc->sc_tx_intrarg = NULL;
+ 
+ 	return 0;
+ }
+ 
+ /*
+  * Halt Input
+  *
+  * This doesnt really halt the input, but it will look
+  * that way to the audio driver. Incoming data will be
+  * discarded.
+  */
+ static int
+ btsco_halt_input(void *hdl)
+ {
+ 	struct btsco_softc *sc = hdl;
+ 
+ 	DPRINTFN(5, "%s\n", sc->sc_name);
+ 
+ 	sc->sc_rx_want = 0;
+ 	sc->sc_rx_block = NULL;
+ 	sc->sc_rx_intr = NULL;
+ 	sc->sc_rx_intrarg = NULL;
+ 
+ 	if (sc->sc_rx_mbuf != NULL) {
+ 		m_freem(sc->sc_rx_mbuf);
+ 		sc->sc_rx_mbuf = NULL;
+ 	}
+ 
+ 	return 0;
+ }
+ 
+ static int
+ btsco_getdev(void *hdl, struct audio_device *ret)
+ {
+ 
+ 	*ret = btsco_device;
+ 	return 0;
+ }
+ 
+ static int
+ btsco_setfd(void *hdl, int fd)
+ {
+ 	DPRINTF("set %s duplex\n", fd ? "full" : "half");
+ 
+ 	return 0;
+ }
+ 
+ static int
+ btsco_set_port(void *hdl, mixer_ctrl_t *mc)
+ {
+ 	struct btsco_softc *sc = hdl;
+ 	int err = 0;
+ 
+ 	DPRINTF("%s dev %d type %d\n", sc->sc_name, mc->dev, mc->type);
+ 
+ 	switch (mc->dev) {
+ 	case BTSCO_VGS:
+ 		if (mc->type != AUDIO_MIXER_VALUE ||
+ 		    mc->un.value.num_channels != 1) {
+ 			err = EINVAL;
+ 			break;
+ 		}
+ 
+ 		sc->sc_vgs = mc->un.value.level[AUDIO_MIXER_LEVEL_MONO];
+ 		break;
+ 
+ 	case BTSCO_VGM:
+ 		if (mc->type != AUDIO_MIXER_VALUE ||
+ 		    mc->un.value.num_channels != 1) {
+ 			err = EINVAL;
+ 			break;
+ 		}
+ 
+ 		sc->sc_vgm = mc->un.value.level[AUDIO_MIXER_LEVEL_MONO];
+ 		break;
+ 
+ 	default:
+ 		err = EINVAL;
+ 		break;
+ 	}
+ 
+ 	return err;
+ }
+ 
+ static int
+ btsco_get_port(void *hdl, mixer_ctrl_t *mc)
+ {
+ 	struct btsco_softc *sc = hdl;
+ 	int err = 0;
+ 
+ 	DPRINTF("%s dev %d\n", sc->sc_name, mc->dev);
+ 
+ 	switch (mc->dev) {
+ 	case BTSCO_VGS:
+ 		mc->type = AUDIO_MIXER_VALUE;
+ 		mc->un.value.num_channels = 1;
+ 		mc->un.value.level[AUDIO_MIXER_LEVEL_MONO] = sc->sc_vgs;
+ 		break;
+ 
+ 	case BTSCO_VGM:
+ 		mc->type = AUDIO_MIXER_VALUE;
+ 		mc->un.value.num_channels = 1;
+ 		mc->un.value.level[AUDIO_MIXER_LEVEL_MONO] = sc->sc_vgm;
+ 		break;
+ 
+ 	default:
+ 		err = EINVAL;
+ 		break;
+ 	}
+ 
+ 	return err;
+ }
+ 
+ static int
+ btsco_query_devinfo(void *hdl, mixer_devinfo_t *di)
+ {
+ /*	struct btsco_softc *sc = hdl;	*/
+ 	int err = 0;
+ 
+ 	switch(di->index) {
+ 	case BTSCO_VGS:
+ 		di->mixer_class = BTSCO_INPUT_CLASS;
+ 		di->next = di->prev = AUDIO_MIXER_LAST;
+ 		strlcpy(di->label.name, AudioNspeaker,
+ 		    sizeof(di->label.name));
+ 		di->type = AUDIO_MIXER_VALUE;
+ 		strlcpy(di->un.v.units.name, AudioNvolume,
+ 		    sizeof(di->un.v.units.name));
+ 		di->un.v.num_channels = 1;
+ 		di->un.v.delta = BTSCO_DELTA;
+ 		break;
+ 
+ 	case BTSCO_VGM:
+ 		di->mixer_class = BTSCO_INPUT_CLASS;
+ 		di->next = di->prev = AUDIO_MIXER_LAST;
+ 		strlcpy(di->label.name, AudioNmicrophone,
+ 		    sizeof(di->label.name));
+ 		di->type = AUDIO_MIXER_VALUE;
+ 		strlcpy(di->un.v.units.name, AudioNvolume,
+ 		    sizeof(di->un.v.units.name));
+ 		di->un.v.num_channels = 1;
+ 		di->un.v.delta = BTSCO_DELTA;
+ 		break;
+ 
+ 	case BTSCO_INPUT_CLASS:
+ 		di->mixer_class = BTSCO_INPUT_CLASS;
+ 		di->next = di->prev = AUDIO_MIXER_LAST;
+ 		strlcpy(di->label.name, AudioCinputs,
+ 		    sizeof(di->label.name));
+ 		di->type = AUDIO_MIXER_CLASS;
+ 		break;
+ 
+ 	default:
+ 		err = ENXIO;
+ 		break;
+ 	}
+ 
+ 	return err;
+ }
+ 
+ /*
+  * Allocate Ring Buffers.
+  */
+ static void *
+ btsco_allocm(void *hdl, int direction, size_t size, int pool, int flags)
+ {
+ 	struct btsco_softc *sc = hdl;
+ 	void *addr;
+ 
+ 	DPRINTF("%s: size %d direction %d\n", sc->sc_name, size, direction);
+ 
+ 	addr = malloc(size, pool, flags);
+ 
+ 	if (direction == AUMODE_PLAY) {
+ 		sc->sc_tx_buf = addr;
+ 		sc->sc_tx_refcnt = 0;
+ 	}
+ 
+ 	return addr;
+ }
+ 
+ /*
+  * Free Ring Buffers.
+  *
+  * Because we used external memory for the tx mbufs, we dont
+  * want to free the memory until all the mbufs are done with
+  *
+  * Just to be sure, dont free if something is still pending.
+  * This would be a memory leak but at least there is a warning..
+  */
+ static void
+ btsco_freem(void *hdl, void *addr, int pool)
+ {
+ 	struct btsco_softc *sc = hdl;
+ 	int count = hz / 2;
+ 
+ 	if (addr == sc->sc_tx_buf) {
+ 		DPRINTF("%s: tx_refcnt=%d\n", sc->sc_name, sc->sc_tx_refcnt);
+ 
+ 		sc->sc_tx_buf = NULL;
+ 
+ 		while (sc->sc_tx_refcnt> 0 && count-- > 0)
+ 			tsleep(sc, PWAIT, "drain", 1);
+ 
+ 		if (sc->sc_tx_refcnt > 0) {
+ 			printf("%s: ring buffer unreleased!\n", sc->sc_name);
+ 			return;
+ 		}
+ 	}
+ 
+ 	free(addr, pool);
+ }
+ 
+ static int
+ btsco_get_props(void *hdl)
+ {
+ 
+ 	return AUDIO_PROP_FULLDUPLEX;
+ }
+ 
+ #ifdef notyet
+ /*
+  * Handle private ioctl. We pass information out about how to talk
+  * to the device and mixer.
+  */
+ static int
+ btsco_dev_ioctl(void *hdl, u_long cmd, void *addr, int flag,
+     struct proc *l)
+ {
+ 	struct btsco_softc *sc = hdl;
+ 	struct btsco_info *bi = (struct btsco_info *)addr;
+ 	int err = 0;
+ 
+ 	DPRINTF("%s cmd 0x%lx flag %d\n", sc->sc_name, cmd, flag);
+ 
+ 	switch (cmd) {
+ 	case BTSCO_GETINFO:
+ 		memset(bi, 0, sizeof(*bi));
+ 		bdaddr_copy(&bi->laddr, &sc->sc_laddr);
+ 		bdaddr_copy(&bi->raddr, &sc->sc_raddr);
+ 		bi->channel = sc->sc_channel;
+ 		bi->vgs = BTSCO_VGS;
+ 		bi->vgm = BTSCO_VGM;
+ 		break;
+ 
+ 	default:
+ 		err = ENOTTY;
+ 		break;
+ 	}
+ 
+ 	return err;
+ }
+ #endif
+ 
+ 
+ /*****************************************************************************
+  *
+  *	misc btsco functions
+  *
+  */
+ 
+ /*
+  * Our transmit interrupt. This is triggered when a new block is to be
+  * sent.  We send mtu sized chunks of the block as mbufs with external
+  * storage to sco_send()
+  */
+ static void
+ btsco_intr(void *arg)
+ {
+ 	struct btsco_softc *sc = arg;
+ 	struct mbuf *m;
+ 	uint8_t *block;
+ 	int mlen, size;
+ 	int s;
+ 
+ 	DPRINTFN(10, "%s block %p size %d\n",
+ 	    sc->sc_name, sc->sc_tx_block, sc->sc_tx_size);
+ 
+ 	if (sc->sc_sco == NULL)
+ 		return;		/* connection is lost */
+ 
+ 	s = splsoftnet();
+ 
+ 	block = sc->sc_tx_block;
+ 	size = sc->sc_tx_size;
+ 	sc->sc_tx_block = NULL;
+ 	sc->sc_tx_size = 0;
+ 
+ 	mutex_enter(&bt_lock);
+ 	while (size > 0) {
+ 		MGETHDR(m, M_DONTWAIT, MT_DATA);
+ 		if (m == NULL)
+ 			break;
+ 
+ 		mlen = MIN(sc->sc_mtu, size);
+ 
+ 		/* I think M_DEVBUF is true but not relevant */
+ 		MEXTADD(m, block, mlen, M_DEVBUF, btsco_extfree, sc);
+ 		if ((m->m_flags & M_EXT) == 0) {
+ 			m_free(m);
+ 			break;
+ 		}
+ 		sc->sc_tx_refcnt++;
+ 
+ 		m->m_pkthdr.len = m->m_len = mlen;
+ 		sc->sc_tx_pending++;
+ 
+ 		if (sco_send(sc->sc_sco, m) > 0) {
+ 			sc->sc_tx_pending--;
+ 			break;
+ 		}
+ 
+ 		block += mlen;
+ 		size -= mlen;
+ 	}
+ 	mutex_exit(&bt_lock);
+ 
+ 	splx(s);
+ }
+ 
+ /*
+  * Release the mbuf, we keep a reference count on the tx buffer so
+  * that we dont release it before its free.
+  */
+ static void
+ btsco_extfree(caddr_t buf, u_int size, void *arg)
+ {
+ 	struct btsco_softc *sc = arg;
+ 
+ #ifdef notyet
+ 	if (m != NULL)
+ 		pool_cache_put(mb_cache, m);
+ #endif
+ 
+ 	sc->sc_tx_refcnt--;
+ }
*** dummy/s3c2410.c	2012-03-07 17:13:18.367998799 -0600
--- src/sys/arch/arm/s3c2xx0/s3c2410.c	2012-03-07 17:16:59.019222211 -0600
***************
*** 0 ****
--- 1,275 ----
+ /*	$OpenBSD: s3c2410.c,v 1.1 2008/11/26 14:39:14 drahn Exp $ */
+ /*	$NetBSD: s3c2410.c,v 1.10 2005/12/11 12:16:51 christos Exp $ */
+ 
+ /*
+  * Copyright (c) 2003, 2005  Genetec corporation.  All rights reserved.
+  * Written by Hiroyuki Bessho for Genetec corporation.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. The name of Genetec corporation may not be used to endorse
+  *    or promote products derived from this software without specific prior
+  *    written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
+  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
+  * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
+  * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
+  * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
+  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
+  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
+  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
+  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
+  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
+  * POSSIBILITY OF SUCH DAMAGE.
+  */
+ 
+ #include <sys/cdefs.h>
+ //__KERNEL_RCSID(0, "$NetBSD: s3c2410.c,v 1.10 2005/12/11 12:16:51 christos Exp $");
+ 
+ #include <sys/param.h>
+ #include <sys/systm.h>
+ #include <sys/device.h>
+ #include <sys/kernel.h>
+ #include <sys/reboot.h>
+ 
+ #include <machine/cpu.h>
+ #include <machine/bus.h>
+ 
+ #include <arm/cpufunc.h>
+ #include <arm/mainbus/mainbus.h>
+ #include <arm/s3c2xx0/s3c2410reg.h>
+ #include <arm/s3c2xx0/s3c2410var.h>
+ 
+ /* prototypes */
+ int	s3c2410_match(struct device *, void *, void *);
+ void	s3c2410_attach(struct device *, struct device *, void *);
+ int	s3c2410_search(struct device *, void *, void *);
+ 
+ /* attach structures */
+ /*
+ #if 0
+ CFATTACH_DECL(ssio, sizeof(struct s3c24x0_softc), s3c2410_match, s3c2410_attach,
+     NULL, NULL);
+ #endif
+ */
+ 
+ struct cfattach ssio_ca = {
+         sizeof(struct s3c24x0_softc), s3c2410_match, s3c2410_attach
+ };
+ 
+ struct cfdriver ssio_cd = {
+ 	NULL, "ssio", DV_DULL
+ };
+ 
+ 
+ extern struct bus_space s3c2xx0_bs_tag;
+ 
+ struct s3c2xx0_softc *s3c2xx0_softc;
+ 
+ #ifdef DEBUG_PORTF
+ volatile uint8_t *portf;	/* for debug */
+ #endif
+ 
+ static int
+ s3c2410_print(void *aux, const char *name)
+ {
+ 	struct s3c2xx0_attach_args *sa = (struct s3c2xx0_attach_args *) aux;
+ 
+ 	if (sa->sa_size)
+ 		printf(" addr 0x%lx", sa->sa_addr);
+ 	if (sa->sa_size > 1)
+ 		printf("-0x%lx", sa->sa_addr + sa->sa_size - 1);
+ 	if (sa->sa_intr != -1)
+ 		printf(" intr %d", sa->sa_intr);
+ 	if (sa->sa_index != -1)
+ 		printf(" unit %d", sa->sa_index);
+ 
+ 	return (UNCONF);
+ }
+ 
+ int
+ s3c2410_match(struct device *parent, void *match, void *aux)
+ {
+ 	return 1;
+ }
+ 
+ void
+ s3c2410_attach(struct device *parent, struct device *self, void *aux)
+ {
+ 	struct s3c24x0_softc *sc = (struct s3c24x0_softc *) self;
+ 	bus_space_tag_t iot;
+ 	const char *which_registers;	/* for panic message */
+ 
+ #define FAIL(which)  do { \
+ 	which_registers=(which); goto abort; }while(/*CONSTCOND*/0)
+ 
+ 	s3c2xx0_softc = &(sc->sc_sx);
+ 	sc->sc_sx.sc_iot = iot = &s3c2xx0_bs_tag;
+ 
+ 	if (bus_space_map(iot,
+ 		S3C2410_INTCTL_BASE, S3C2410_INTCTL_SIZE,
+ 		BUS_SPACE_MAP_LINEAR, &sc->sc_sx.sc_intctl_ioh))
+ 		FAIL("intc");
+ 	/* tell register addresses to interrupt handler */
+ 	s3c2410_intr_init(sc);
+ 
+ 	/* Map the GPIO registers */
+ 	if (bus_space_map(iot, S3C2410_GPIO_BASE, S3C2410_GPIO_SIZE,
+ 		0, &sc->sc_sx.sc_gpio_ioh))
+ 		FAIL("GPIO");
+ #ifdef DEBUG_PORTF
+ 	{
+ 		extern volatile uint8_t *portf;
+ 		/* make all ports output */
+ 		bus_space_write_2(iot, sc->sc_sx.sc_gpio_ioh, GPIO_PCONF, 0x5555);
+ 		portf = (volatile uint8_t *)
+ 			((char *)bus_space_vaddr(iot, sc->sc_sx.sc_gpio_ioh) + GPIO_PDATF);
+ 	}
+ #endif
+ 
+ #if 0
+ 	/* Map the DMA controller registers */
+ 	if (bus_space_map(iot, S3C2410_DMAC_BASE, S3C2410_DMAC_SIZE,
+ 		0, &sc->sc_sx.sc_dmach))
+ 		FAIL("DMAC");
+ #endif
+ 
+ 	/* Memory controller */
+ 	if (bus_space_map(iot, S3C2410_MEMCTL_BASE,
+ 		S3C24X0_MEMCTL_SIZE, 0, &sc->sc_sx.sc_memctl_ioh))
+ 		FAIL("MEMC");
+ 	/* Clock manager */
+ 	if (bus_space_map(iot, S3C2410_CLKMAN_BASE,
+ 		S3C24X0_CLKMAN_SIZE, 0, &sc->sc_sx.sc_clkman_ioh))
+ 		FAIL("CLK");
+ 
+ #if 0
+ 	/* Real time clock */
+ 	if (bus_space_map(iot, S3C2410_RTC_BASE,
+ 		S3C24X0_RTC_SIZE, 0, &sc->sc_sx.sc_rtc_ioh))
+ 		FAIL("RTC");
+ #endif
+ 
+ 	if (bus_space_map(iot, S3C2410_TIMER_BASE,
+ 		S3C24X0_TIMER_SIZE, 0, &sc->sc_timer_ioh))
+ 		FAIL("TIMER");
+ 
+ 	/* calculate current clock frequency */
+ 	s3c24x0_clock_freq(&sc->sc_sx);
+ 	printf(": fclk %d MHz hclk %d MHz pclk %d MHz\n",
+ 	       sc->sc_sx.sc_fclk / 1000000, sc->sc_sx.sc_hclk / 1000000,
+ 	       sc->sc_sx.sc_pclk / 1000000);
+ 
+ 	/* get busdma tag for the platform */
+ 	sc->sc_sx.sc_dmat = s3c2xx0_bus_dma_init(&s3c2xx0_bus_dma);
+ 
+ 	/*
+ 	 *  Attach devices.
+ 	 */
+ 	config_search(s3c2410_search, self, sc);
+ 
+ 	return;
+ 
+ abort:
+ 	panic("%s: unable to map %s registers",
+ 	    self->dv_xname, which_registers);
+ 
+ #undef FAIL
+ }
+ 
+ int
+ s3c2410_search(struct device * parent, void * c, void *aux)
+ {
+ 	struct s3c24x0_softc *sc = (struct s3c24x0_softc *) parent;
+ 	struct s3c2xx0_attach_args aa;
+ 	struct cfdata   *cf = c;
+ 
+ 
+ 	aa.sa_sc = sc;
+ 	aa.sa_iot = sc->sc_sx.sc_iot;
+ 	#if 0
+ 	aa.sa_addr = cf->cf_loc[SSIOCF_ADDR];
+ 	aa.sa_size = cf->cf_loc[SSIOCF_SIZE];
+ 	aa.sa_index = cf->cf_loc[SSIOCF_INDEX];
+ 	aa.sa_intr = cf->cf_loc[SSIOCF_INTR];
+ 	#else
+ 	aa.sa_addr = cf->cf_loc[0];
+ 	aa.sa_size = cf->cf_loc[1];
+ 	aa.sa_index = cf->cf_loc[2];
+ 	aa.sa_intr = cf->cf_loc[3];
+ 	#endif
+ 
+ 	aa.sa_dmat = sc->sc_sx.sc_dmat;
+ 
+         config_found(parent, &aa, s3c2410_print);
+ 
+ 	return 0;
+ }
+ 
+ /*
+  * fill sc_pclk, sc_hclk, sc_fclk from values of clock controller register.
+  *
+  * s3c24x0_clock_freq2() is meant to be called from kernel startup routines.
+  * s3c24x0_clock_freq() is for after kernel initialization is done.
+  */
+ void
+ s3c24x0_clock_freq2(vaddr_t clkman_base, int *fclk, int *hclk, int *pclk)
+ {
+ 	uint32_t pllcon, divn;
+ 	int mdiv, pdiv, sdiv;
+ 	int f, h, p;
+ 
+ 	pllcon = *(volatile uint32_t *)(clkman_base + CLKMAN_MPLLCON);
+ 	divn = *(volatile uint32_t *)(clkman_base + CLKMAN_CLKDIVN);
+ 
+ 	mdiv = (pllcon & PLLCON_MDIV_MASK) >> PLLCON_MDIV_SHIFT;
+ 	pdiv = (pllcon & PLLCON_PDIV_MASK) >> PLLCON_PDIV_SHIFT;
+ 	sdiv = (pllcon & PLLCON_SDIV_MASK) >> PLLCON_SDIV_SHIFT;
+ 
+ 	f = ((mdiv + 8) * S3C2XX0_XTAL_CLK) / ((pdiv + 2) * (1 << sdiv));
+ 	h = f;
+ 	if (divn & CLKDIVN_HDIVN)
+ 		h /= 2;
+ 	p = h;
+ 	if (divn & CLKDIVN_PDIVN)
+ 		p /= 2;
+ 
+ 	if (fclk) *fclk = f;
+ 	if (hclk) *hclk = h;
+ 	if (pclk) *pclk = p;
+ 
+ }
+ 
+ void
+ s3c24x0_clock_freq(struct s3c2xx0_softc *sc)
+ {
+ 	s3c24x0_clock_freq2(
+ 		(vaddr_t)bus_space_vaddr(sc->sc_iot, sc->sc_clkman_ioh),
+ 		&sc->sc_fclk, &sc->sc_hclk, &sc->sc_pclk);
+ }
+ 
+ /*
+  * Issue software reset command.
+  * called with MMU off.
+  *
+  * S3C2410 doesn't have sowtware reset bit like S3C2800.
+  * use watch dog timer and make it fire immediately.
+  */
+ void
+ s3c2410_softreset(void)
+ {
+ 	disable_interrupts(I32_bit|F32_bit);
+ 
+ 	*(volatile unsigned int *)(S3C2410_WDT_BASE + WDT_WTCON)
+ 		= (0 << WTCON_PRESCALE_SHIFT) | WTCON_ENABLE |
+ 		WTCON_CLKSEL_16 | WTCON_ENRST;
+ }
+ 
+ 
*** dummy/s3c2410_intr.c	2012-03-07 17:13:28.571998344 -0600
--- src/sys/arch/arm/s3c2xx0/s3c2410_intr.c	2012-03-07 17:17:00.970971750 -0600
***************
*** 0 ****
--- 1,374 ----
+ /*	$OpenBSD: s3c2410_intr.c,v 1.1 2008/11/26 14:39:14 drahn Exp $ */
+ /* $NetBSD: s3c2410_intr.c,v 1.11 2008/11/24 11:29:52 dogcow Exp $ */
+ 
+ /*
+  * Copyright (c) 2003  Genetec corporation.  All rights reserved.
+  * Written by Hiroyuki Bessho for Genetec corporation.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. The name of Genetec corporation may not be used to endorse
+  *    or promote products derived from this software without specific prior
+  *    written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
+  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
+  * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
+  * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
+  * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
+  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
+  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
+  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
+  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
+  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
+  * POSSIBILITY OF SUCH DAMAGE.
+  */
+ 
+ /*
+  * IRQ handler for Samsung S3C2410 processor.
+  * It has integrated interrupt controller.
+  */
+ 
+ #include <sys/cdefs.h>
+ /*
+ __KERNEL_RCSID(0, "$NetBSD: s3c2410_intr.c,v 1.11 2008/11/24 11:29:52 dogcow Exp $");
+ */
+ 
+ #include <sys/param.h>
+ #include <sys/systm.h>
+ #include <sys/malloc.h>
+ #include <uvm/uvm_extern.h>
+ #include <machine/bus.h>
+ #include <machine/intr.h>
+ #include <arm/cpufunc.h>
+ 
+ #include <arm/s3c2xx0/s3c2410reg.h>
+ #include <arm/s3c2xx0/s3c2410var.h>
+ 
+ /*
+  * interrupt dispatch table.
+  */
+ 
+ struct s3c2xx0_intr_dispatch handler[ICU_LEN];
+ 
+ extern volatile uint32_t *s3c2xx0_intr_mask_reg;
+ 
+ volatile int intr_mask;
+ #ifdef __HAVE_FAST_SOFTINTS
+ volatile int softint_pending;
+ volatile int soft_intr_mask;
+ #endif
+ volatile int global_intr_mask = 0; /* mask some interrupts at all spl level */
+ 
+ /* interrupt masks for each level */
+ int s3c2xx0_imask[NIPL];
+ int s3c2xx0_smask[NIPL];
+ int s3c2xx0_ilevel[ICU_LEN];
+ #ifdef __HAVE_FAST_SOFTINTS
+ int s3c24x0_soft_imask[NIPL];
+ #endif
+ 
+ vaddr_t intctl_base;		/* interrupt controller registers */
+ #define icreg(offset) \
+ 	(*(volatile uint32_t *)(intctl_base+(offset)))
+ 
+ #ifdef __HAVE_FAST_SOFTINTS
+ /*
+  * Map a software interrupt queue to an interrupt priority level.
+  */
+ static const int si_to_ipl[] = {
+ 	[SI_SOFTBIO]	= IPL_SOFTBIO,
+ 	[SI_SOFTCLOCK]	= IPL_SOFTCLOCK,
+ 	[SI_SOFTNET]	= IPL_SOFTNET,
+ 	[SI_SOFTSERIAL] = IPL_SOFTSERIAL,
+ };
+ #endif
+ 
+ #define PENDING_CLEAR_MASK	(~0)
+ 
+ /*
+  * called from irq_entry.
+  */
+ void s3c2410_irq_handler(struct clockframe *);
+ void
+ s3c2410_irq_handler(struct clockframe *frame)
+ {
+ 	uint32_t irqbits;
+ 	int irqno;
+ 	int saved_spl_level;
+ 
+ 	saved_spl_level = s3c2xx0_curcpl();
+ 
+ #ifdef	DIAGNOSTIC
+ 	if (curcpu()->ci_intr_depth > 10)
+ 		panic("nested intr too deep");
+ #endif
+ 
+ 	while ((irqbits = icreg(INTCTL_INTPND)) != 0) {
+ 
+ 		/* Note: Only one bit in INTPND register is set */
+ 
+ 		irqno = icreg(INTCTL_INTOFFSET);
+ 
+ #ifdef	DIAGNOSTIC
+ 		if (__predict_false((irqbits & (1<<irqno)) == 0)) {
+ 			/* This shouldn't happen */
+ 			printf("INTOFFSET=%d, INTPND=%x\n", irqno, irqbits);
+ 			break;
+ 		}
+ #endif
+ 		/* raise spl to stop interrupts of lower priorities */
+ 		if (saved_spl_level < handler[irqno].level)
+ 			s3c2xx0_setipl(handler[irqno].level);
+ 
+ 		/* clear pending bit */
+ 		icreg(INTCTL_SRCPND) = PENDING_CLEAR_MASK & (1 << irqno);
+ 		icreg(INTCTL_INTPND) = PENDING_CLEAR_MASK & (1 << irqno);
+ 
+ 		enable_interrupts(I32_bit); /* allow nested interrupts */
+ 
+ 		(*handler[irqno].func) (
+ 		    handler[irqno].cookie == 0
+ 		    ? frame : handler[irqno].cookie);
+ 
+ 		disable_interrupts(I32_bit);
+ 
+ 		/* restore spl to that was when this interrupt happen */
+ 		s3c2xx0_setipl(saved_spl_level);
+ 
+ 	}
+ 
+ #ifdef __HAVE_FAST_SOFTINTS
+ 	cpu_dosoftints();
+ #endif
+ }
+ 
+ /*
+  * Handler for main IRQ of cascaded interrupts.
+  */
+ static int
+ cascade_irq_handler(void *cookie)
+ {
+ 	int index = (int)cookie - 1;
+ 	uint32_t irqbits;
+ 	int irqno, i;
+ 	int save = disable_interrupts(I32_bit);
+ 
+ 	KASSERT(0 <= index && index <= 3);
+ 
+ 	irqbits = icreg(INTCTL_SUBSRCPND) &
+ 	    ~icreg(INTCTL_INTSUBMSK) & (0x07 << (3*index));
+ 
+ 	for (irqno = 3*index; irqbits; ++irqno) {
+ 		if ((irqbits & (1<<irqno)) == 0)
+ 			continue;
+ 
+ 		/* clear pending bit */
+ 		irqbits &= ~(1<<irqno);
+ 		icreg(INTCTL_SUBSRCPND) = (1 << irqno);
+ 
+ 		/* allow nested interrupts. SPL is already set
+ 		 * correctly by main handler. */
+ 		restore_interrupts(save);
+ 
+ 		i = S3C2410_SUBIRQ_MIN + irqno;
+ 		(* handler[i].func)(handler[i].cookie);
+ 
+ 		disable_interrupts(I32_bit);
+ 	}
+ 
+ 	return 1;
+ }
+ 
+ 
+ static const uint8_t subirq_to_main[] = {
+ 	S3C2410_INT_UART0,
+ 	S3C2410_INT_UART0,
+ 	S3C2410_INT_UART0,
+ 	S3C2410_INT_UART1,
+ 	S3C2410_INT_UART1,
+ 	S3C2410_INT_UART1,
+ 	S3C2410_INT_UART2,
+ 	S3C2410_INT_UART2,
+ 	S3C2410_INT_UART2,
+ 	S3C24X0_INT_ADCTC,
+ 	S3C24X0_INT_ADCTC,
+ };
+ 
+ void *
+ s3c24x0_intr_establish(int irqno, int level, int type,
+     int (* func) (void *), void *cookie)
+ {
+ 	int save;
+ 
+ 	if (irqno < 0 || irqno >= ICU_LEN ||
+ 	    type < IST_NONE || IST_EDGE_BOTH < type)
+ 		panic("intr_establish: bogus irq or type");
+ 
+ 	save = disable_interrupts(I32_bit);
+ 
+ 	handler[irqno].cookie = cookie;
+ 	handler[irqno].func = func;
+ 	handler[irqno].level = level;
+ 
+ 	if (irqno >= S3C2410_SUBIRQ_MIN) {
+ 		/* cascaded interrupts. */
+ 		int main_irqno;
+ 		int i = (irqno - S3C2410_SUBIRQ_MIN);
+ 
+ 		main_irqno = subirq_to_main[i];
+ 
+ 		/* establish main irq if first time
+ 		 * be careful that cookie shouldn't be 0 */
+ 		if (handler[main_irqno].func != cascade_irq_handler)
+ 			s3c24x0_intr_establish(main_irqno, level, type,
+ 			    cascade_irq_handler, (void *)((i/3) + 1));
+ 
+ 		/* unmask it in submask register */
+ 		icreg(INTCTL_INTSUBMSK) &= ~(1<<i);
+ 
+ 		restore_interrupts(save);
+ 		return &handler[irqno];
+ 	}
+ 
+ 	s3c2xx0_update_intr_masks(irqno, level);
+ 
+ 	/*
+ 	 * set trigger type for external interrupts 0..3
+ 	 */
+ 	if (irqno <= S3C24X0_INT_EXT(3)) {
+ 		/*
+ 		 * Update external interrupt control
+ 		 */
+ 		s3c2410_setup_extint(irqno, type);
+ 	}
+ 
+ 	s3c2xx0_setipl(s3c2xx0_curcpl());
+ 
+ 	restore_interrupts(save);
+ 
+ 	return &handler[irqno];
+ }
+ 
+ 
+ static void
+ init_interrupt_masks(void)
+ {
+ 	int i;
+ 
+ 	for (i=0; i < NIPL; ++i)
+ 		s3c2xx0_imask[i] = 0;
+ 
+ #ifdef __HAVE_FAST_SOFTINTS
+ 	s3c24x0_soft_imask[IPL_NONE] = SI_TO_IRQBIT(SI_SOFTSERIAL) |
+ 		SI_TO_IRQBIT(SI_SOFTNET) | SI_TO_IRQBIT(SI_SOFTCLOCK) |
+ 		SI_TO_IRQBIT(SI_SOFT);
+ 
+ 	s3c24x0_soft_imask[IPL_SOFT] = SI_TO_IRQBIT(SI_SOFTSERIAL) |
+ 		SI_TO_IRQBIT(SI_SOFTNET) | SI_TO_IRQBIT(SI_SOFTCLOCK);
+ 
+ 	/*
+ 	 * splsoftclock() is the only interface that users of the
+ 	 * generic software interrupt facility have to block their
+ 	 * soft intrs, so splsoftclock() must also block IPL_SOFT.
+ 	 */
+ 	s3c24x0_soft_imask[IPL_SOFTCLOCK] = SI_TO_IRQBIT(SI_SOFTSERIAL) |
+ 		SI_TO_IRQBIT(SI_SOFTNET);
+ 
+ 	/*
+ 	 * splsoftnet() must also block splsoftclock(), since we don't
+ 	 * want timer-driven network events to occur while we're
+ 	 * processing incoming packets.
+ 	 */
+ 	s3c24x0_soft_imask[IPL_SOFTNET] = SI_TO_IRQBIT(SI_SOFTSERIAL);
+ 
+ 	for (i = IPL_BIO; i < IPL_SOFTSERIAL; ++i)
+ 		s3c24x0_soft_imask[i] = SI_TO_IRQBIT(SI_SOFTSERIAL);
+ #endif
+ }
+ 
+ void
+ s3c2410_intr_init(struct s3c24x0_softc *sc)
+ {
+ 	intctl_base = (vaddr_t) bus_space_vaddr(sc->sc_sx.sc_iot,
+ 	    sc->sc_sx.sc_intctl_ioh);
+ 
+ 	s3c2xx0_intr_mask_reg = (uint32_t *)(intctl_base + INTCTL_INTMSK);
+ 
+ 	/* clear all pending interrupt */
+ 	icreg(INTCTL_SRCPND) = ~0;
+ 	icreg(INTCTL_INTPND) = ~0;
+ 
+ 	/* mask all sub interrupts */
+ 	icreg(INTCTL_INTSUBMSK) = 0x7ff;
+ 
+ 	init_interrupt_masks();
+ 
+ 	s3c2xx0_intr_init(handler, ICU_LEN);
+ 
+ }
+ 
+ 
+ /*
+  * mask/unmask sub interrupts
+  */
+ void
+ s3c2410_mask_subinterrupts(int bits)
+ {
+ 	int psw = disable_interrupts(I32_bit|F32_bit);
+ 	icreg(INTCTL_INTSUBMSK) |= bits;
+ 	restore_interrupts(psw);
+ }
+ 
+ void
+ s3c2410_unmask_subinterrupts(int bits)
+ {
+ 	int psw = disable_interrupts(I32_bit|F32_bit);
+ 	icreg(INTCTL_INTSUBMSK) &= ~bits;
+ 	restore_interrupts(psw);
+ }
+ 
+ /*
+  * Update external interrupt control
+  */
+ static const u_char s3c24x0_ist[] = {
+ 	EXTINTR_LOW,		/* NONE */
+ 	EXTINTR_FALLING,	/* PULSE */
+ 	EXTINTR_FALLING,	/* EDGE */
+ 	EXTINTR_LOW,		/* LEVEL */
+ 	EXTINTR_HIGH,
+ 	EXTINTR_RISING,
+ 	EXTINTR_BOTH,
+ };
+ 
+ void
+ s3c2410_setup_extint(int extint, int type)
+ {
+         uint32_t reg;
+         u_int   trig;
+         int     i = extint % 8;
+         int     regidx = extint/8;      /* GPIO_EXTINT[0:2] */
+ 	int	save;
+ 
+         trig = s3c24x0_ist[type];
+ 
+ 	save = disable_interrupts(I32_bit);
+ 
+         reg = bus_space_read_4(s3c2xx0_softc->sc_iot,
+             s3c2xx0_softc->sc_gpio_ioh,
+             GPIO_EXTINT(regidx));
+ 
+         reg = reg & ~(0x07 << (4*i));
+         reg |= trig << (4*i);
+ 
+         bus_space_write_4(s3c2xx0_softc->sc_iot, s3c2xx0_softc->sc_gpio_ioh,
+             GPIO_EXTINT(regidx), reg);
+ 
+ 	restore_interrupts(save);
+ }
*** dummy/s3c24x0_clk.c	2012-03-07 17:13:39.403998191 -0600
--- src/sys/arch/arm/s3c2xx0/s3c24x0_clk.c	2012-03-05 20:23:37.947280943 -0600
***************
*** 0 ****
--- 1,337 ----
+ /*	$OpenBSD: s3c24x0_clk.c,v 1.1 2008/11/26 14:39:14 drahn Exp $ */
+ /*	$NetBSD: s3c24x0_clk.c,v 1.10 2008/07/04 11:59:45 bsh Exp $ */
+ 
+ /*
+  * Copyright (c) 2003  Genetec corporation.  All rights reserved.
+  * Written by Hiroyuki Bessho for Genetec corporation.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. The name of Genetec corporation may not be used to endorse
+  *    or promote products derived from this software without specific prior
+  *    written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
+  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
+  * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
+  * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
+  * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
+  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
+  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
+  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
+  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
+  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
+  * POSSIBILITY OF SUCH DAMAGE.
+  */
+ 
+ #include <sys/cdefs.h>
+ /*
+ __KERNEL_RCSID(0, "$NetBSD: s3c24x0_clk.c,v 1.10 2008/07/04 11:59:45 bsh Exp $");
+ */
+ 
+ #include <sys/param.h>
+ #include <sys/systm.h>
+ #include <sys/kernel.h>
+ #include <sys/time.h>
+ #include <sys/timetc.h>
+ 
+ #include <machine/bus.h>
+ #include <machine/intr.h>
+ #include <arm/cpufunc.h>
+ 
+ #include <arm/s3c2xx0/s3c24x0reg.h>
+ #include <arm/s3c2xx0/s3c24x0var.h>
+ #include <arm/s3c2xx0/s3c2xx0reg.h>
+ 
+ 
+ #ifndef STATHZ
+ #define STATHZ	64
+ #endif
+ 
+ #define TIMER_FREQUENCY(pclk) ((pclk)/16) /* divider=1/16 */
+ 
+ static unsigned int timer4_reload_value;
+ static unsigned int timer4_prescaler;
+ static unsigned int timer4_mseccount;
+ 
+ #define usec_to_counter(t)	\
+ 	((timer4_mseccount*(t))/1000)
+ 
+ #define counter_to_usec(c,pclk)	\
+ 	(((c)*timer4_prescaler*1000)/(TIMER_FREQUENCY(pclk)/1000))
+ 
+ static u_int	s3c24x0_get_timecount(struct timecounter *);
+ 
+ static struct timecounter s3c24x0_timecounter = {
+ 	s3c24x0_get_timecount,	/* get_timecount */
+ 	0,			/* no poll_pps */
+ 	0xffffffff,		/* counter_mask */
+ 	0,		/* frequency */
+ 	"s3c234x0",		/* name */
+ 	100,			/* quality */
+ 	NULL,			/* prev */
+ 	NULL,			/* next */
+ };
+ 
+ static volatile uint32_t s3c24x0_base;
+ 
+ static u_int
+ s3c24x0_get_timecount(struct timecounter *tc)
+ {
+ 	struct s3c24x0_softc *sc = (struct s3c24x0_softc *) s3c2xx0_softc;
+ 	int save, int_pend0, int_pend1, count;
+ 
+ 	save = disable_interrupts(I32_bit);
+ 
+  again:
+ 	int_pend0 = S3C24X0_INT_TIMER4 &
+ 	    bus_space_read_4(sc->sc_sx.sc_iot, sc->sc_sx.sc_intctl_ioh,
+ 		INTCTL_SRCPND);
+ 	count = bus_space_read_2(sc->sc_sx.sc_iot, sc->sc_timer_ioh,
+ 	    TIMER_TCNTO(4));
+ 	
+ 	for (;;) {
+ 
+ 		int_pend1 = S3C24X0_INT_TIMER4 &
+ 		    bus_space_read_4(sc->sc_sx.sc_iot, sc->sc_sx.sc_intctl_ioh,
+ 			INTCTL_SRCPND);
+ 		if( int_pend0 == int_pend1 )
+ 			break;
+ 
+ 		/*
+ 		 * Down counter reached to zero while we were reading
+ 		 * timer values. do it again to get consistent values.
+ 		 */
+ 		int_pend0 = int_pend1;
+ 		count = bus_space_read_2(sc->sc_sx.sc_iot, sc->sc_timer_ioh,
+ 		    TIMER_TCNTO(4));
+ 	}
+ 
+ 	if (__predict_false(count > timer4_reload_value)) {
+ 		/* 
+ 		 * Buggy Hardware Warning --- sometimes timer counter
+ 		 * reads bogus value like 0xffff.  I guess it happens when
+ 		 * the timer is reloaded.
+ 		 */
+ 		printf("Bogus value from timer counter: %d\n", count);
+ 		goto again;
+ 	}
+ 
+ 	restore_interrupts(save);
+ 
+ 	if (int_pend1)
+ 		count -= timer4_reload_value;
+ 
+ 	return s3c24x0_base - count;
+ }
+ 
+ static inline int
+ read_timer(struct s3c24x0_softc *sc)
+ {
+ 	int count;
+ 
+ 	do {
+ 		count = bus_space_read_2(sc->sc_sx.sc_iot, sc->sc_timer_ioh,
+ 		    TIMER_TCNTO(4));
+ 	} while ( __predict_false(count > timer4_reload_value) );
+ 
+ 	return count;
+ }
+ 
+ /*
+  * delay:
+  *
+  *	Delay for at least N microseconds.
+  */
+ void
+ delay(u_int n)
+ {
+ 	struct s3c24x0_softc *sc = (struct s3c24x0_softc *) s3c2xx0_softc;
+ 	int v0, v1, delta;
+ 	u_int ucnt;
+ 
+ 	if ( timer4_reload_value == 0 ){
+ 		/* not initialized yet */
+ 		while ( n-- > 0 ){
+ 			int m;
+ 
+ 			for (m=0; m<100; ++m )
+ 				;
+ 		}
+ 		return;
+ 	}
+ 
+ 	/* read down counter */
+ 	v0 = read_timer(sc);
+ 
+ 	ucnt = usec_to_counter(n);
+ 
+ 	while( ucnt > 0 ) {
+ 		v1 = read_timer(sc);
+ 		delta = v0 - v1;
+ 		if ( delta < 0 )
+ 			delta += timer4_reload_value;
+ #ifdef DEBUG
+ 		if (delta < 0 || delta > timer4_reload_value)
+ 			panic("wrong value from timer counter");
+ #endif
+ 
+ 		if((u_int)delta < ucnt){
+ 			ucnt -= (u_int)delta;
+ 			v0 = v1;
+ 		}
+ 		else {
+ 			ucnt = 0;
+ 		}
+ 	}
+ 	/*NOTREACHED*/
+ }
+ 
+ void
+ setstatclockrate(int newhz)
+ {
+ }
+ 
+ static int
+ hardintr(void *arg)
+ {
+ 	#if 0
+ 	atomic_add_32(&s3c24x0_base, timer4_reload_value);
+ 	#else
+ 	s3c24x0_base += timer4_reload_value;
+ 	#endif
+ 
+ 	hardclock((struct clockframe *)arg);
+ 
+ 	return 1;
+ }
+ 
+ void
+ cpu_initclocks(void)
+ {
+ 	struct s3c24x0_softc *sc = (struct s3c24x0_softc *)s3c2xx0_softc;
+ 	long tc;
+ 	int prescaler, h;
+ 	int pclk = s3c2xx0_softc->sc_pclk;
+ 	bus_space_tag_t iot = sc->sc_sx.sc_iot;
+ 	bus_space_handle_t ioh = sc->sc_timer_ioh;
+ 	uint32_t  reg;
+ 
+ 	stathz = STATHZ;
+ 	profhz = stathz;
+ 
+ #define	time_constant(hz)	(TIMER_FREQUENCY(pclk) /(hz)/ prescaler)
+ #define calc_time_constant(hz)					\
+ 	do {							\
+ 		prescaler = 1;					\
+ 		do {						\
+ 			++prescaler;				\
+ 			tc = time_constant(hz);			\
+ 		} while( tc > 65536 );				\
+ 	} while(0)
+ 
+ 
+ 	/* Use the channels 4 and 3 for hardclock and statclock, respectively */
+ 
+ 	/* stop all timers */
+ 	bus_space_write_4(iot, ioh, TIMER_TCON, 0);
+ 
+ 	/* calc suitable prescaler value */
+ 	h = MIN(hz,stathz);
+ 	calc_time_constant(h);
+ 
+ 	timer4_prescaler = prescaler;
+ 	timer4_reload_value = TIMER_FREQUENCY(pclk) / hz / prescaler;
+ 	timer4_mseccount = TIMER_FREQUENCY(pclk)/timer4_prescaler/1000 ;
+ 
+ 	bus_space_write_4(iot, ioh, TIMER_TCNTB(4),
+ 	    ((prescaler - 1) << 16) | (timer4_reload_value - 1));
+ 
+ 	printf("clock: hz=%d stathz = %d PCLK=%d prescaler=%d tc=%ld\n",
+ 	    hz, stathz, pclk, prescaler, tc);
+ 
+ 	bus_space_write_4(iot, ioh, TIMER_TCNTB(3),
+ 	    ((prescaler - 1) << 16) | (time_constant(stathz) - 1));
+ 
+ 	s3c24x0_intr_establish(S3C24X0_INT_TIMER4, IPL_CLOCK, 
+ 			       IST_NONE, hardintr, 0);
+ #if 0
+ #ifdef IPL_STATCLOCK
+ 	s3c24x0_intr_establish(S3C24X0_INT_TIMER3, IPL_STATCLOCK,
+ 			       IST_NONE, statintr, 0);
+ #endif
+ #endif
+ 
+ 	/* set prescaler1 */
+ 	reg = bus_space_read_4(iot, ioh, TIMER_TCFG0);
+ 	bus_space_write_4(iot, ioh, TIMER_TCFG0,
+ 			  (reg & ~0xff00) | ((prescaler-1) << 8));
+ 
+ 	/* divider 1/16 for ch #3 and #4 */
+ 	reg = bus_space_read_4(iot, ioh, TIMER_TCFG1);
+ 	bus_space_write_4(iot, ioh, TIMER_TCFG1,
+ 			  (reg & ~(TCFG1_MUX_MASK(3)|TCFG1_MUX_MASK(4))) |
+ 			  (TCFG1_MUX_DIV16 << TCFG1_MUX_SHIFT(3)) |
+ 			  (TCFG1_MUX_DIV16 << TCFG1_MUX_SHIFT(4)) );
+ 
+ 
+ 	/* start timers */
+ 	reg = bus_space_read_4(iot, ioh, TIMER_TCON);
+ 	reg &= ~(TCON_MASK(3)|TCON_MASK(4));
+ 
+ 	/* load the time constant */
+ 	bus_space_write_4(iot, ioh, TIMER_TCON, reg |
+ 	    TCON_MANUALUPDATE(3) | TCON_MANUALUPDATE(4));
+ 	/* set auto reload and start */
+ 	bus_space_write_4(iot, ioh, TIMER_TCON, reg |
+ 	    TCON_AUTORELOAD(3) | TCON_START(3) |
+ 	    TCON_AUTORELOAD(4) | TCON_START(4) );
+ 
+ 	s3c24x0_timecounter.tc_frequency = TIMER_FREQUENCY(pclk) / timer4_prescaler;
+ 	tc_init(&s3c24x0_timecounter);
+ }
+ 
+ 
+ #if 0
+ /* test routine for delay() */
+ 
+ void delay_test(void);
+ void
+ delay_test(void)
+ {
+ 	struct s3c2xx0_softc *sc = s3c2xx0_softc;
+ 	volatile int *pdatc = (volatile int *)
+ 		((char *)bus_space_vaddr(sc->sc_iot, sc->sc_gpio_ioh) + GPIO_PDATC);
+ 	static const int d[] = {0, 1, 5, 10, 50, 100, 500, 1000, -1};
+ 	int i;
+ 	int v = *pdatc & ~0x07;
+ 
+ 	for (;;) {
+ 		*pdatc = v | 2;
+ 
+ 		for (i=0; d[i] >= 0; ++i) {
+ 			*pdatc = v | 3;
+ 			delay(d[i]);
+ 			*pdatc = v | 2;
+ 		}
+ 		*pdatc = v;
+ 	}
+ }
+ #endif
+ 
+ 
+ void
+ inittodr(time_t base)  
+ {
+ }
+ 
+ void
+ resettodr(void)
+ {
+ }
*** dummy/s3c2xx0_busdma.c	2012-03-07 17:13:53.535973741 -0600
--- src/sys/arch/arm/s3c2xx0/s3c2xx0_busdma.c	2012-03-05 20:23:38.015226003 -0600
***************
*** 0 ****
--- 1,73 ----
+ /*	$OpenBSD: s3c2xx0_busdma.c,v 1.1 2008/11/26 14:39:14 drahn Exp $ */
+ /*	$NetBSD: s3c2xx0_busdma.c,v 1.3 2005/12/11 12:16:51 christos Exp $ */
+ 
+ /*
+  * Copyright (c) 2002, 2003 Fujitsu Component Limited
+  * Copyright (c) 2002, 2003 Genetec Corporation
+  * All rights reserved.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. Neither the name of The Fujitsu Component Limited nor the name of
+  *    Genetec corporation may not be used to endorse or promote products
+  *    derived from this software without specific prior written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY FUJITSU COMPONENT LIMITED AND GENETEC
+  * CORPORATION ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
+  * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
+  * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
+  * DISCLAIMED.  IN NO EVENT SHALL FUJITSU COMPONENT LIMITED OR GENETEC
+  * CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
+  * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
+  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
+  * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
+  * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
+  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
+  * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
+  * SUCH DAMAGE.
+  */
+ 
+ /*
+  * bus_dma tag for s3c2xx0 CPUs
+  */
+ 
+ #include <sys/cdefs.h>
+ /*
+ __KERNEL_RCSID(0, "$NetBSD: s3c2xx0_busdma.c,v 1.3 2005/12/11 12:16:51 christos Exp $");
+ */
+ 
+ #include <sys/param.h>
+ #include <sys/types.h>
+ #include <sys/device.h>
+ #include <sys/systm.h>
+ #include <sys/extent.h>
+ 
+ #define _ARM32_BUS_DMA_PRIVATE
+ #include <machine/bus.h>
+ 
+ #include <arm/s3c2xx0/s3c2xx0var.h>
+ 
+ struct arm32_bus_dma_tag s3c2xx0_bus_dma = {
+ 	0,
+ 	0,
+ 	NULL,
+ 	_bus_dmamap_create,
+ 	_bus_dmamap_destroy,
+ 	_bus_dmamap_load,
+ 	_bus_dmamap_load_mbuf,
+ 	_bus_dmamap_load_uio,
+ 	_bus_dmamap_load_raw,
+ 	_bus_dmamap_unload,
+ 	_bus_dmamap_sync,
+ 	_bus_dmamem_alloc,
+ 	_bus_dmamem_free,
+ 	_bus_dmamem_map,
+ 	_bus_dmamem_unmap,
+ 	_bus_dmamem_mmap,
+ };
*** dummy/s3c2xx0_intr.c	2012-03-07 17:14:00.755998811 -0600
--- src/sys/arch/arm/s3c2xx0/s3c2xx0_intr.c	2012-03-07 17:17:04.039828650 -0600
***************
*** 0 ****
--- 1,363 ----
+ /*	$OpenBSD: s3c2xx0_intr.c,v 1.1 2008/11/26 14:39:14 drahn Exp $ */
+ /* $NetBSD: s3c2xx0_intr.c,v 1.13 2008/04/27 18:58:45 matt Exp $ */
+ 
+ /*
+  * Copyright (c) 2002, 2003 Fujitsu Component Limited
+  * Copyright (c) 2002, 2003 Genetec Corporation
+  * All rights reserved.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. Neither the name of The Fujitsu Component Limited nor the name of
+  *    Genetec corporation may not be used to endorse or promote products
+  *    derived from this software without specific prior written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY FUJITSU COMPONENT LIMITED AND GENETEC
+  * CORPORATION ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
+  * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
+  * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
+  * DISCLAIMED.  IN NO EVENT SHALL FUJITSU COMPONENT LIMITED OR GENETEC
+  * CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
+  * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
+  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
+  * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
+  * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
+  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
+  * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
+  * SUCH DAMAGE.
+  */
+ 
+ /*
+  * Common part of IRQ handlers for Samsung S3C2800/2400/2410 processors.
+  * derived from i80321_icu.c
+  */
+ 
+ /*
+  * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
+  * All rights reserved.
+  *
+  * Written by Jason R. Thorpe for Wasabi Systems, Inc.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. All advertising materials mentioning features or use of this software
+  *    must display the following acknowledgement:
+  *	This product includes software developed for the NetBSD Project by
+  *	Wasabi Systems, Inc.
+  * 4. The name of Wasabi Systems, Inc. may not be used to endorse
+  *    or promote products derived from this software without specific prior
+  *    written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
+  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
+  * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
+  * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
+  * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
+  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
+  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
+  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
+  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
+  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
+  * POSSIBILITY OF SUCH DAMAGE.
+  */
+ 
+ #include <sys/cdefs.h>
+ /*
+ __KERNEL_RCSID(0, "$NetBSD: s3c2xx0_intr.c,v 1.13 2008/04/27 18:58:45 matt Exp $");
+ */
+ 
+ #include <sys/param.h>
+ #include <sys/systm.h>
+ #include <sys/malloc.h>
+ #include <uvm/uvm_extern.h>
+ #include <machine/bus.h>
+ #include <machine/intr.h>
+ #include <arm/cpufunc.h>
+ 
+ #include <arm/s3c2xx0/s3c2xx0reg.h>
+ #include <arm/s3c2xx0/s3c2xx0var.h>
+ 
+ #include <arm/s3c2xx0/s3c2xx0_intr.h>
+ 
+ volatile uint32_t *s3c2xx0_intr_mask_reg;
+ extern volatile int intr_mask;
+ extern volatile int global_intr_mask;
+ 
+ int s3c2xx0_cpl;
+ 
+ #define SI_TO_IRQBIT(x) (1 << (x))
+ 
+ int
+ s3c2xx0_curcpl()
+ {
+ 	return s3c2xx0_cpl;
+ }
+ 
+ void
+ s3c2xx0_set_curcpl(int new)
+ {
+ 	s3c2xx0_cpl = new;
+ }
+ 
+ static inline void
+ __raise(int ipl)
+ {
+ 	if (s3c2xx0_curcpl() < ipl) {
+ 		s3c2xx0_setipl(ipl);
+ 	}
+ }
+ 
+ /*
+  * modify interrupt mask table for SPL levels
+  */
+ void
+ s3c2xx0_update_intr_masks(int irqno, int level)
+ {
+ 	int mask = 1 << irqno;
+ 	int i;
+ 
+ 
+ 	s3c2xx0_ilevel[irqno] = level;
+ 
+ 	for (i = 0; i < level; ++i)
+ 		s3c2xx0_imask[i] |= mask;	/* Enable interrupt at lower
+ 						 * level */
+ 	for (; i < NIPL - 1; ++i)
+ 		s3c2xx0_imask[i] &= ~mask;	/* Disable interrupt at upper
+ 						 * level */
+ 
+ 	/*
+ 	 * Enforce a hierarchy that gives "slow" device (or devices with
+ 	 * limited input buffer space/"real-time" requirements) a better
+ 	 * chance at not dropping data.
+ 	 */
+ 	s3c2xx0_imask[IPL_VM] &= s3c2xx0_imask[IPL_SOFTSERIAL];
+ 	s3c2xx0_imask[IPL_CLOCK] &= s3c2xx0_imask[IPL_VM];
+ 	s3c2xx0_imask[IPL_HIGH] &= s3c2xx0_imask[IPL_CLOCK];
+ 
+ 	/* initialize soft interrupt mask */
+ 	for (i = IPL_NONE; i <= IPL_HIGH; i++)  {
+ 		s3c2xx0_smask[i] = 0;
+ 		if (i < IPL_SOFT)
+ 			s3c2xx0_smask[i] |= SI_TO_IRQBIT(SI_SOFT);
+ 		if (i < IPL_SOFTCLOCK)
+ 			s3c2xx0_smask[i] |= SI_TO_IRQBIT(SI_SOFTCLOCK);
+ 		if (i < IPL_SOFTNET)
+ 			s3c2xx0_smask[i] |= SI_TO_IRQBIT(SI_SOFTNET);
+ 		if (i < IPL_SOFTSERIAL)
+ 			s3c2xx0_smask[i] |= SI_TO_IRQBIT(SI_SOFTSERIAL);
+ #if 0
+ 		printf("mask[%d]: %x %x\n", i, s3c2xx0_smask[i],
+ 		    s3c2xx0_sk[i]);
+ #endif  
+ 	}
+ 
+ }
+ 
+ static int
+ stray_interrupt(void *cookie)
+ {
+ 	int save;
+ 	int irqno = (int) cookie;
+ 	printf("stray interrupt %d\n", irqno);
+ 
+ 	save = disable_interrupts(I32_bit);
+ 	*s3c2xx0_intr_mask_reg &= ~(1U << irqno);
+ 	restore_interrupts(save);
+ 
+ 	return 0;
+ }
+ 
+ /*
+  * Initialize interrupt dispatcher.
+  */
+ void
+ s3c2xx0_intr_init(struct s3c2xx0_intr_dispatch * dispatch_table, int icu_len)
+ {
+ 	int i;
+ 
+ 	for (i = 0; i < icu_len; ++i) {
+ 		dispatch_table[i].func = stray_interrupt;
+ 		dispatch_table[i].cookie = (void *) (i);
+ 		dispatch_table[i].level = IPL_VM;
+ 	}
+ 
+ 	global_intr_mask = ~0;		/* no intr is globally blocked. */
+ 
+ 	_splraise(IPL_VM);
+ 	enable_interrupts(I32_bit);
+ }
+ 
+ /*
+  * initialize variables so that splfoo() doesn't touch illegal address.
+  * called during bootstrap.
+  */
+ void
+ s3c2xx0_intr_bootstrap(vaddr_t icureg)
+ {
+ 	s3c2xx0_intr_mask_reg = (volatile uint32_t *)(icureg + INTCTL_INTMSK);
+ }
+ 
+ 
+ 
+ #undef splx
+ void
+ splx(int ipl)
+ {
+ 	s3c2xx0_splx(ipl);
+ }
+ 
+ #undef _splraise
+ int
+ _splraise(int ipl)
+ {
+ 	return s3c2xx0_splraise(ipl);
+ }
+ 
+ #undef _spllower
+ int
+ _spllower(int ipl)
+ {
+ 	return s3c2xx0_spllower(ipl);
+ }
+ 
+ void
+ s3c2xx0_mask_interrupts(int mask)
+ {
+ 	int save = disable_interrupts(I32_bit);
+ 	global_intr_mask &= ~mask;
+ 	s3c2xx0_update_hw_mask();
+ 	restore_interrupts(save);
+ }
+ 
+ void
+ s3c2xx0_unmask_interrupts(int mask)
+ {
+ 	int save = disable_interrupts(I32_bit);
+ 	global_intr_mask |= mask;
+ 	s3c2xx0_update_hw_mask();
+ 	restore_interrupts(save);
+ }
+ 
+ void
+ s3c2xx0_setipl(int new)
+ {
+ 	s3c2xx0_set_curcpl(new);
+ 	intr_mask = s3c2xx0_imask[s3c2xx0_curcpl()];
+ 	s3c2xx0_update_hw_mask();
+ #ifdef __HAVE_FAST_SOFTINTS
+ 	update_softintr_mask();
+ #endif
+ }
+ 
+ 
+ void
+ s3c2xx0_splx(int new)
+ {
+ 	int psw;
+ 
+ 	psw = disable_interrupts(I32_bit);
+ 	s3c2xx0_setipl(new);
+ 	restore_interrupts(psw);
+ 
+ #ifdef __HAVE_FAST_SOFTINTS
+ 	cpu_dosoftints();
+ #endif
+ }
+ 
+ 
+ int
+ s3c2xx0_splraise(int ipl)
+ {
+ 	int	old, psw;
+ 
+ 	old = s3c2xx0_curcpl();
+ 	if( ipl > old ){
+ 		psw = disable_interrupts(I32_bit);
+ 		s3c2xx0_setipl(ipl);
+ 		restore_interrupts(psw);
+ 	}
+ 
+ 	return (old);
+ }
+ 
+ int
+ s3c2xx0_spllower(int ipl)
+ {
+ 	int old = s3c2xx0_curcpl();
+ 	int psw = disable_interrupts(I32_bit);
+ 	s3c2xx0_splx(ipl);
+ 	restore_interrupts(psw);
+ 	return(old);
+ }
+ 
+ /* XXX */
+ void s3c2xx0_do_pending(void);
+ 
+ int softint_pending;
+ void
+ s3c2xx0_setsoftintr(int si)
+ {
+ 	int oldirqstate;
+ 
+ 	oldirqstate = disable_interrupts(I32_bit);
+ 	softint_pending |= SI_TO_IRQBIT(si);
+ 	restore_interrupts(oldirqstate);
+  
+ 	/* Process unmasked pending soft interrupts. */
+ 	if (softint_pending & s3c2xx0_smask[s3c2xx0_curcpl()])
+ 		s3c2xx0_do_pending();
+ }
+ 
+ 
+ void
+ s3c2xx0_do_pending(void)
+ {
+ 	static int processing = 0;
+ 	int oldirqstate, spl_save;
+  
+ 	oldirqstate = disable_interrupts(I32_bit);
+     
+ 	spl_save = s3c2xx0_curcpl();
+ 		
+ 	if (processing == 1) {
+ 		restore_interrupts(oldirqstate);
+ 		return;
+ 	}
+ 
+ #define DO_SOFTINT(si, ipl) \
+ 	if ((softint_pending & s3c2xx0_smask[s3c2xx0_curcpl()]) &   \
+ 	    SI_TO_IRQBIT(si)) {                                         \
+ 		softint_pending &= ~SI_TO_IRQBIT(si);                   \
+ 		if (s3c2xx0_curcpl() < ipl)                            \
+ 			s3c2xx0_setipl(ipl);                         \
+ 		restore_interrupts(oldirqstate);                        \
+ 		softintr_dispatch(si);                                  \
+ 		oldirqstate = disable_interrupts(I32_bit);              \
+ 		s3c2xx0_setipl(spl_save);                            \
+ 	}
+ 
+ 	do {
+ 		DO_SOFTINT(SI_SOFTSERIAL, IPL_SOFTSERIAL);
+ 		DO_SOFTINT(SI_SOFTNET, IPL_SOFTNET);
+ 		DO_SOFTINT(SI_SOFTCLOCK, IPL_SOFTCLOCK);
+ 		DO_SOFTINT(SI_SOFT, IPL_SOFT);
+ 	} while (softint_pending & s3c2xx0_smask[s3c2xx0_curcpl()]);
+ 
+ 
+ 	processing = 0;
+ 	restore_interrupts(oldirqstate);
+ }
+ 
*** dummy/s3c2xx0_mutex.c	2012-03-07 17:14:11.711998357 -0600
--- src/sys/arch/arm/s3c2xx0/s3c2xx0_mutex.c	2012-03-05 20:23:38.082973201 -0600
***************
*** 0 ****
--- 1,67 ----
+ /*	$OpenBSD: s3c2xx0_mutex.c,v 1.1 2008/11/26 14:39:14 drahn Exp $	*/
+ 
+ /*
+  * Copyright (c) 2004 Artur Grabowski <art@openbsd.org>
+  * All rights reserved. 
+  *
+  * Redistribution and use in source and binary forms, with or without 
+  * modification, are permitted provided that the following conditions 
+  * are met: 
+  *
+  * 1. Redistributions of source code must retain the above copyright 
+  *    notice, this list of conditions and the following disclaimer. 
+  * 2. The name of the author may not be used to endorse or promote products
+  *    derived from this software without specific prior written permission. 
+  *
+  * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
+  * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
+  * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
+  * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
+  * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
+  * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
+  * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
+  * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
+  * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
+  * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
+  */
+ 
+ #include <sys/param.h>
+ #include <sys/mutex.h>
+ #include <sys/systm.h>
+ 
+ #include <machine/intr.h>
+ 
+ #ifdef MULTIPROCESSOR
+ #error This code needs work
+ #endif
+ 
+ /*
+  * Single processor systems don't need any mutexes, but they need the spl
+  * raising semantics of the mutexes.
+  */
+ void
+ mtx_init(struct mutex *mtx, int wantipl)
+ {
+ 	mtx->mtx_oldipl = 0;
+ 	mtx->mtx_wantipl = wantipl;
+ 	mtx->mtx_lock = 0;
+ }
+ 
+ void
+ mtx_enter(struct mutex *mtx)
+ {
+ 	if (mtx->mtx_wantipl != IPL_NONE)
+ 		mtx->mtx_oldipl = _splraise(mtx->mtx_wantipl);
+ 
+ 	MUTEX_ASSERT_UNLOCKED(mtx);
+ 	mtx->mtx_lock = 1;
+ }
+ 
+ void
+ mtx_leave(struct mutex *mtx)
+ {
+ 	MUTEX_ASSERT_LOCKED(mtx);
+ 	mtx->mtx_lock = 0;
+ 	if (mtx->mtx_wantipl != IPL_NONE)
+ 		splx(mtx->mtx_oldipl);
+ }
*** dummy/s3c2xx0_space.c	2012-03-07 17:14:19.059248322 -0600
--- src/sys/arch/arm/s3c2xx0/s3c2xx0_space.c	2012-03-05 20:23:38.082973201 -0600
***************
*** 0 ****
--- 1,268 ----
+ /*	$OpenBSD: s3c2xx0_space.c,v 1.1 2008/11/26 14:39:14 drahn Exp $ */
+ /*	$NetBSD: s3c2xx0_space.c,v 1.7 2005/11/24 13:08:32 yamt Exp $ */
+ 
+ /*
+  * Copyright (c) 2002 Fujitsu Component Limited
+  * Copyright (c) 2002 Genetec Corporation
+  * All rights reserved.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. Neither the name of The Fujitsu Component Limited nor the name of
+  *    Genetec corporation may not be used to endorse or promote products
+  *    derived from this software without specific prior written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY FUJITSU COMPONENT LIMITED AND GENETEC
+  * CORPORATION ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
+  * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
+  * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
+  * DISCLAIMED.  IN NO EVENT SHALL FUJITSU COMPONENT LIMITED OR GENETEC
+  * CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
+  * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
+  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
+  * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
+  * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
+  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
+  * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
+  * SUCH DAMAGE.
+  */
+ /* derived from sa11x0_io.c */
+ 
+ /*
+  * Copyright (c) 1997 Mark Brinicombe.
+  * Copyright (c) 1997 Causality Limited.
+  * All rights reserved.
+  *
+  * This code is derived from software contributed to The NetBSD Foundation
+  * by Ichiro FUKUHARA.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. All advertising materials mentioning features or use of this software
+  *    must display the following acknowledgement:
+  *	This product includes software developed by Mark Brinicombe.
+  * 4. The name of the company nor the name of the author may be used to
+  *    endorse or promote products derived from this software without specific
+  *    prior written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
+  * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
+  * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
+  * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
+  * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
+  * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
+  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
+  * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
+  * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
+  * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
+  * SUCH DAMAGE.
+  */
+ 
+ /*
+  * bus_space functions for Samsung S3C2800/2400/2410.
+  */
+ 
+ #include <sys/cdefs.h>
+ /*
+ __KERNEL_RCSID(0, "$NetBSD: s3c2xx0_space.c,v 1.7 2005/11/24 13:08:32 yamt Exp $");
+ */
+ 
+ #include <sys/param.h>
+ #include <sys/systm.h>
+ 
+ #include <uvm/uvm_extern.h>
+ #include <arm/pmap.h>
+ 
+ #include <machine/bus.h>
+ 
+ /* Prototypes for all the bus_space structure functions */
+ bs_protos(s3c2xx0);
+ bs_protos(generic);
+ bs_protos(generic_armv4);
+ bs_protos(bs_notimpl);
+ 
+ struct bus_space s3c2xx0_bs_tag = {
+ 	/* cookie */
+ 	(void *) 0,
+ 
+ 	/* mapping/unmapping */
+ 	s3c2xx0_bs_map,
+ 	s3c2xx0_bs_unmap,
+ 	s3c2xx0_bs_subregion,
+ 
+ 	/* allocation/deallocation */
+ 	s3c2xx0_bs_alloc,	/* not implemented */
+ 	s3c2xx0_bs_free,	/* not implemented */
+ 
+ 	/* get kernel virtual address */
+ 	s3c2xx0_bs_vaddr,
+ 
+ 	/* mmap */
+ 	bs_notimpl_bs_mmap,
+ 
+ 	/* barrier */
+ 	s3c2xx0_bs_barrier,
+ 
+ 	/* read (single) */
+ 	generic_bs_r_1,
+ 	generic_armv4_bs_r_2,
+ 	generic_bs_r_4,
+ 	bs_notimpl_bs_r_8,
+ 
+ 	/* read multiple */
+ 	generic_bs_rm_1,
+ 	generic_armv4_bs_rm_2,
+ 	generic_bs_rm_4,
+ 	bs_notimpl_bs_rm_8,
+ 
+ 	/* read region */
+ 	generic_bs_rr_1,
+ 	generic_armv4_bs_rr_2,
+ 	generic_bs_rr_4,
+ 	bs_notimpl_bs_rr_8,
+ 
+ 	/* write (single) */
+ 	generic_bs_w_1,
+ 	generic_armv4_bs_w_2,
+ 	generic_bs_w_4,
+ 	bs_notimpl_bs_w_8,
+ 
+ 	/* write multiple */
+ 	generic_bs_wm_1,
+ 	generic_armv4_bs_wm_2,
+ 	generic_bs_wm_4,
+ 	bs_notimpl_bs_wm_8,
+ 
+ 	/* write region */
+ 	generic_bs_wr_1,
+ 	generic_armv4_bs_wr_2,
+ 	generic_bs_wr_4,
+ 	bs_notimpl_bs_wr_8,
+ 
+ 	/* set multiple */
+ 	bs_notimpl_bs_sm_1,
+ 	bs_notimpl_bs_sm_2,
+ 	bs_notimpl_bs_sm_4,
+ 	bs_notimpl_bs_sm_8,
+ 
+ 	/* set region */
+ 	generic_bs_sr_1,
+ 	generic_armv4_bs_sr_2,
+ 	bs_notimpl_bs_sr_4,
+ 	bs_notimpl_bs_sr_8,
+ 
+ 	/* copy */
+ 	bs_notimpl_bs_c_1,
+ 	generic_armv4_bs_c_2,
+ 	bs_notimpl_bs_c_4,
+ 	bs_notimpl_bs_c_8,
+ };
+ 
+ int
+ s3c2xx0_bs_map(void *t, bus_addr_t bpa, bus_size_t size,
+ 	       int flag, bus_space_handle_t * bshp)
+ {
+ 	u_long startpa, endpa, pa;
+ 	vaddr_t va;
+ 	pt_entry_t *pte;
+ 	const struct pmap_devmap	*pd;
+ 
+ 	if ((pd = pmap_devmap_find_pa(bpa, size)) != NULL) {
+ 		/* Device was statically mapped. */
+ 		*bshp = pd->pd_va + (bpa - pd->pd_pa);
+ 		return 0;
+ 	}
+ 	startpa = trunc_page(bpa);
+ 	endpa = round_page(bpa + size);
+ 
+ 	/* XXX use extent manager to check duplicate mapping */
+ 
+ 	va = uvm_km_alloc(kernel_map, endpa - startpa);
+ 	if (!va)
+ 		return (ENOMEM);
+ 
+ 	*bshp = (bus_space_handle_t) (va + (bpa - startpa));
+ 
+ 	for (pa = startpa; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE) {
+ 		pmap_kenter_pa(va, pa, VM_PROT_READ | VM_PROT_WRITE);
+ 		pte = vtopte(va);
+ 		if ((flag & BUS_SPACE_MAP_CACHEABLE) == 0)
+ 			*pte &= ~L2_S_CACHE_MASK;
+ 	}
+ 	pmap_update(pmap_kernel());
+ 
+ 	return (0);
+ }
+ 
+ void
+ s3c2xx0_bs_unmap(void *t, bus_space_handle_t bsh, bus_size_t size)
+ {
+ 	vaddr_t	va;
+ 	vaddr_t	endva;
+ 
+ 	if (pmap_devmap_find_va(bsh, size) != NULL) {
+ 		/* Device was statically mapped; nothing to do. */
+ 		return;
+ 	}
+ 
+ 	endva = round_page(bsh + size);
+ 	va = trunc_page(bsh);
+ 
+ 	pmap_kremove(va, endva - va);
+ 	pmap_update(pmap_kernel());
+ 	uvm_km_free(kernel_map, va, endva - va);
+ }
+ 
+ 
+ int
+ s3c2xx0_bs_subregion(void *t, bus_space_handle_t bsh, bus_size_t offset,
+ 		     bus_size_t size, bus_space_handle_t * nbshp)
+ {
+ 
+ 	*nbshp = bsh + offset;
+ 	return (0);
+ }
+ 
+ void
+ s3c2xx0_bs_barrier(void *t, bus_space_handle_t bsh, bus_size_t offset,
+ 		   bus_size_t len, int flags)
+ {
+ 
+ 	/* Nothing to do. */
+ }
+ 
+ void *
+ s3c2xx0_bs_vaddr(void *t, bus_space_handle_t bsh)
+ {
+ 
+ 	return ((void *) bsh);
+ }
+ 
+ 
+ int
+ s3c2xx0_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
+ 		 bus_size_t size, bus_size_t alignment, bus_size_t boundary,
+     		 int flags, bus_addr_t * bpap, bus_space_handle_t * bshp)
+ {
+ 
+ 	panic("s3c2xx0_io_bs_alloc(): not implemented\n");
+ }
+ 
+ void
+ s3c2xx0_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
+ {
+ 
+ 	panic("s3c2xx0_io_bs_free(): not implemented\n");
+ }
*** dummy/sscom.c	2012-03-07 17:14:26.827997992 -0600
--- src/sys/arch/arm/s3c2xx0/sscom.c	2012-03-07 17:17:08.164149378 -0600
***************
*** 0 ****
--- 1,2196 ----
+ /*	$OpenBSD: sscom.c,v 1.1 2008/11/26 14:39:14 drahn Exp $ */
+ /*	$NetBSD: sscom.c,v 1.29 2008/06/11 22:37:21 cegger Exp $ */
+ 
+ /*
+  * Copyright (c) 2002, 2003 Fujitsu Component Limited
+  * Copyright (c) 2002, 2003 Genetec Corporation
+  * All rights reserved.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. Neither the name of The Fujitsu Component Limited nor the name of
+  *    Genetec corporation may not be used to endorse or promote products
+  *    derived from this software without specific prior written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY FUJITSU COMPONENT LIMITED AND GENETEC
+  * CORPORATION ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
+  * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
+  * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
+  * DISCLAIMED.  IN NO EVENT SHALL FUJITSU COMPONENT LIMITED OR GENETEC
+  * CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
+  * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
+  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
+  * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
+  * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
+  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
+  * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
+  * SUCH DAMAGE.
+  */
+ 
+ /*-
+  * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
+  * All rights reserved.
+  *
+  * This code is derived from software contributed to The NetBSD Foundation
+  * by Charles M. Hannum.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  *
+  * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
+  * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
+  * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
+  * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
+  * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
+  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
+  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
+  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
+  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
+  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
+  * POSSIBILITY OF SUCH DAMAGE.
+  */
+ 
+ /*
+  * Copyright (c) 1991 The Regents of the University of California.
+  * All rights reserved.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. Neither the name of the University nor the names of its contributors
+  *    may be used to endorse or promote products derived from this software
+  *    without specific prior written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
+  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
+  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
+  * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
+  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
+  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
+  * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
+  * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
+  * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
+  * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
+  * SUCH DAMAGE.
+  *
+  *	@(#)com.c	7.5 (Berkeley) 5/16/91
+  */
+ 
+ /*
+  * Support integrated UARTs of Samsung S3C2800/2400X/2410X
+  * Derived from sys/dev/ic/com.c
+  */
+ 
+ #include <sys/cdefs.h>
+ /*
+ __KERNEL_RCSID(0, "$NetBSD: sscom.c,v 1.29 2008/06/11 22:37:21 cegger Exp $");
+ 
+ #include "opt_sscom.h"
+ #include "opt_ddb.h"
+ #include "opt_kgdb.h"
+ #include "opt_multiprocessor.h"
+ #include "opt_lockdebug.h"
+ 
+ #include "rnd.h"
+ #if NRND > 0 && defined(RND_COM)
+ #include <sys/rnd.h>
+ #endif
+ */
+ 
+ /*
+  * Override cnmagic(9) macro before including <sys/systm.h>.
+  * We need to know if cn_check_magic triggered debugger, so set a flag.
+  * Callers of cn_check_magic must declare int cn_trapped = 0;
+  * XXX: this is *ugly*!
+  */
+ #define cn_trap()				\
+ 	do {					\
+ 		console_debugger();		\
+ 		cn_trapped = 1;			\
+ 	} while (/* CONSTCOND */ 0)
+ 
+ #include <sys/param.h>
+ #include <sys/systm.h>
+ #include <sys/ioctl.h>
+ #include <sys/selinfo.h>
+ #include <sys/tty.h>
+ #include <sys/proc.h>
+ #include <sys/user.h>
+ #include <sys/conf.h>
+ #include <sys/file.h>
+ #include <sys/uio.h>
+ #include <sys/kernel.h>
+ #include <sys/syslog.h>
+ #include <sys/types.h>
+ #include <sys/device.h>
+ #include <sys/malloc.h>
+ #include <sys/vnode.h>
+ #include <machine/intr.h>
+ #include <machine/bus.h>
+ 
+ #include <arm/s3c2xx0/s3c2xx0reg.h>
+ #include <arm/s3c2xx0/s3c2xx0var.h>
+ #if defined(SSCOM_S3C2410) || defined(SSCOM_S3C2400)
+ #include <arm/s3c2xx0/s3c24x0reg.h>
+ #elif defined(SSCOM_S3C2800)
+ #include <arm/s3c2xx0/s3c2800reg.h>
+ #endif
+ #include <arm/s3c2xx0/sscom_var.h>
+ #include <dev/cons.h>
+ 
+ dev_type_open(sscomopen);
+ dev_type_close(sscomclose);
+ dev_type_read(sscomread);
+ dev_type_write(sscomwrite);
+ dev_type_ioctl(sscomioctl);
+ dev_type_stop(sscomstop);
+ dev_type_tty(sscomtty);
+ dev_type_poll(sscompoll);
+ 
+ int	sscomcngetc	(dev_t);
+ void	sscomcnputc	(dev_t, int);
+ void	sscomcnpollc	(dev_t, int);
+ 
+ #define	integrate	static inline
+ void 	sscomsoft	(void *);
+ 
+ void sscom_rxsoft	(struct sscom_softc *, struct tty *);
+ void sscom_txsoft	(struct sscom_softc *, struct tty *);
+ void sscom_stsoft	(struct sscom_softc *, struct tty *);
+ void sscom_schedrx	(struct sscom_softc *);
+ 
+ static void	sscom_modem(struct sscom_softc *, int);
+ static void	sscom_break(struct sscom_softc *, int);
+ static void	sscom_iflush(struct sscom_softc *);
+ static void	sscom_hwiflow(struct sscom_softc *);
+ static void	sscom_loadchannelregs(struct sscom_softc *);
+ static void	tiocm_to_sscom(struct sscom_softc *, u_long, int);
+ static int	sscom_to_tiocm(struct sscom_softc *);
+ static void	tiocm_to_sscom(struct sscom_softc *, u_long, int);
+ static int	sscom_to_tiocm(struct sscom_softc *);
+ static void	sscom_iflush(struct sscom_softc *);
+ 
+ static int	sscomhwiflow(struct tty *tp, int block);
+ #if 0
+ static int	sscom_init(bus_space_tag_t, const struct sscom_uart_info *,
+ 		    int, int, tcflag_t, bus_space_handle_t *);
+ #endif
+ 
+ extern struct cfdriver sscom_cd;
+ 
+ /*
+  * Make this an option variable one can patch.
+  * But be warned:  this must be a power of 2!
+  */
+ u_int sscom_rbuf_size = SSCOM_RING_SIZE;
+ 
+ /* Stop input when 3/4 of the ring is full; restart when only 1/4 is full. */
+ u_int sscom_rbuf_hiwat = (SSCOM_RING_SIZE * 1) / 4;
+ u_int sscom_rbuf_lowat = (SSCOM_RING_SIZE * 3) / 4;
+ 
+ static int	sscomconsunit = -1;
+ static bus_space_tag_t sscomconstag;
+ static bus_space_handle_t sscomconsioh;
+ static int	sscomconsattached;
+ static int	sscomconsrate;
+ static tcflag_t sscomconscflag;
+ #if 0
+ static struct cnm_state sscom_cnm_state;
+ #endif
+ 
+ #ifdef KGDB
+ #include <sys/kgdb.h>
+ 
+ static int sscom_kgdb_unit = -1;
+ static bus_space_tag_t sscom_kgdb_iot;
+ static bus_space_handle_t sscom_kgdb_ioh;
+ static int sscom_kgdb_attached;
+ 
+ int	sscom_kgdb_getc (void *);
+ void	sscom_kgdb_putc (void *, int);
+ #endif /* KGDB */
+ 
+ #define	SSCOMUNIT_MASK  	0x7f
+ #define	SSCOMDIALOUT_MASK	0x80
+ 
+ #define	DEVUNIT(x)	(minor(x) & SSCOMUNIT_MASK)
+ #define	SSCOMDIALOUT(x)	(minor(x) & SSCOMDIALOUT_MASK)
+ 
+ #if 0
+ #define	SSCOM_ISALIVE(sc)	((sc)->enabled != 0 && \
+ 				 device_is_active(&(sc)->sc_dev))
+ #else
+ #define	SSCOM_ISALIVE(sc)	device_is_active(&(sc)->sc_dev)
+ #endif
+ 
+ #define	BR	BUS_SPACE_BARRIER_READ
+ #define	BW	BUS_SPACE_BARRIER_WRITE
+ #define SSCOM_BARRIER(t, h, f) /* no-op */
+ 
+ #if (defined(MULTIPROCESSOR) || defined(LOCKDEBUG)) && defined(SSCOM_MPLOCK)
+ 
+ #define SSCOM_LOCK(sc) simple_lock(&(sc)->sc_lock)
+ #define SSCOM_UNLOCK(sc) simple_unlock(&(sc)->sc_lock)
+ 
+ #else
+ 
+ #define SSCOM_LOCK(sc)
+ #define SSCOM_UNLOCK(sc)
+ 
+ #endif
+ 
+ #ifndef SSCOM_TOLERANCE
+ #define	SSCOM_TOLERANCE	30	/* XXX: baud rate tolerance, in 0.1% units */
+ #endif
+ 
+ /* value for UCON */
+ #define UCON_RXINT_MASK	  \
+ 	(UCON_RXMODE_MASK|UCON_ERRINT|UCON_TOINT|UCON_RXINT_TYPE)
+ #define UCON_RXINT_ENABLE \
+ 	(UCON_RXMODE_INT|UCON_ERRINT|UCON_TOINT|UCON_RXINT_TYPE_LEVEL)
+ #define UCON_TXINT_MASK   (UCON_TXMODE_MASK|UCON_TXINT_TYPE)
+ #define UCON_TXINT_ENABLE (UCON_TXMODE_INT|UCON_TXINT_TYPE_LEVEL)
+ 
+ /* we don't want tx interrupt on debug port, but it is needed to
+    have transmitter active */
+ #define UCON_DEBUGPORT	  (UCON_RXINT_ENABLE|UCON_TXINT_ENABLE)
+ 
+ 
+ static inline void
+ __sscom_output_chunk(struct sscom_softc *sc, int ufstat)
+ {
+ 	int n, space;
+ 	bus_space_tag_t iot = sc->sc_iot;
+ 	bus_space_handle_t ioh = sc->sc_ioh;
+ 
+ 	n = sc->sc_tbc;
+ 	space = 16 - ((ufstat & UFSTAT_TXCOUNT) >> UFSTAT_TXCOUNT_SHIFT);
+ 
+ 	if (n > space)
+ 		n = space;
+ 
+ 	if (n > 0) {
+ 		bus_space_write_multi_1(iot, ioh, SSCOM_UTXH, sc->sc_tba, n);
+ 		sc->sc_tbc -= n;
+ 		sc->sc_tba += n;
+ 	}
+ }
+ 
+ static void
+ sscom_output_chunk(struct sscom_softc *sc)
+ {
+ 	int ufstat = bus_space_read_2(sc->sc_iot, sc->sc_ioh, SSCOM_UFSTAT);
+ 
+ 	if (!(ufstat & UFSTAT_TXFULL))
+ 		__sscom_output_chunk(sc, ufstat);
+ }
+ 
+ int
+ sscomspeed(long speed, long frequency)
+ {
+ #define	divrnd(n, q)	(((n)*2/(q)+1)/2)	/* divide and round off */
+ 
+ 	int x, err;
+ 
+ 	if (speed <= 0)
+ 		return -1;
+ 	x = divrnd(frequency / 16, speed);
+ 	if (x <= 0)
+ 		return -1;
+ 	err = divrnd(((quad_t)frequency) * 1000 / 16, speed * x) - 1000;
+ 	if (err < 0)
+ 		err = -err;
+ 	if (err > SSCOM_TOLERANCE)
+ 		return -1;
+ 	return x-1;
+ 
+ #undef	divrnd
+ }
+ 
+ void sscomstatus (struct sscom_softc *, const char *);
+ 
+ #ifdef SSCOM_DEBUG
+ int	sscom_debug = 0;
+ 
+ void
+ sscomstatus(struct sscom_softc *sc, const char *str)
+ {
+ 	struct tty *tp = sc->sc_tty;
+ 	int umstat = bus_space_read_1(sc->sc_iot, sc->sc_iot, SSCOM_UMSTAT);
+ 	int umcon = bus_space_read_1(sc->sc_iot, sc->sc_iot, SSCOM_UMCON);
+ 
+ 	printf("%s: %s %sclocal  %sdcd %sts_carr_on %sdtr %stx_stopped\n",
+ 	    sc->sc_dev.dv_xname, str,
+ 	    ISSET(tp->t_cflag, CLOCAL) ? "+" : "-",
+ 	    "+",			/* DCD */
+ 	    ISSET(tp->t_state, TS_CARR_ON) ? "+" : "-",
+ 	    "+",			/* DTR */
+ 	    sc->sc_tx_stopped ? "+" : "-");
+ 
+ 	printf("%s: %s %scrtscts %scts %sts_ttstop  %srts %xrx_flags\n",
+ 	    sc->sc_dev.dv_xname, str,
+ 	    ISSET(tp->t_cflag, CRTSCTS) ? "+" : "-",
+ 	    ISSET(umstat, UMSTAT_CTS) ? "+" : "-",
+ 	    ISSET(tp->t_state, TS_TTSTOP) ? "+" : "-",
+ 	    ISSET(umcon, UMCON_RTS) ? "+" : "-",
+ 	    sc->sc_rx_flags);
+ }
+ #else
+ #define sscom_debug  0
+ #endif
+ 
+ static void
+ sscom_enable_debugport(struct sscom_softc *sc)
+ {
+ 	int s;
+ 
+ 	/* Turn on line break interrupt, set carrier. */
+ 	s = splserial();
+ 	SSCOM_LOCK(sc);
+ 	sc->sc_ucon = UCON_DEBUGPORT;
+ 	bus_space_write_2(sc->sc_iot, sc->sc_ioh, SSCOM_UCON, sc->sc_ucon);
+ 	sc->sc_umcon = UMCON_RTS|UMCON_DTR;
+ 	sc->set_modem_control(sc);
+ 	sscom_enable_rxint(sc);
+ 	sscom_disable_txint(sc);
+ 	SSCOM_UNLOCK(sc);
+ 	splx(s);
+ }
+ 
+ static void
+ sscom_set_modem_control(struct sscom_softc *sc)
+ {
+ 	/* flob RTS */
+ 	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
+ 	    SSCOM_UMCON, sc->sc_umcon & UMCON_HW_MASK);
+ 	/* ignore DTR */
+ }
+ 
+ static int
+ sscom_read_modem_status(struct sscom_softc *sc)
+ {
+ 	int msts;
+ 
+ 	msts = bus_space_read_1(sc->sc_iot, sc->sc_ioh, SSCOM_UMSTAT);
+ 
+ 	/* DCD and DSR are always on */
+ 	return (msts & UMSTAT_CTS) | MSTS_DCD | MSTS_DSR;
+ }
+ 
+ void
+ sscom_attach_subr(struct sscom_softc *sc)
+ {
+ 	int unit = sc->sc_unit;
+ 	bus_space_tag_t iot = sc->sc_iot;
+ 	bus_space_handle_t ioh = sc->sc_ioh;
+ 	struct tty *tp;
+ 
+ 	timeout_set(&sc->sc_diag_timeout, sscomdiag, sc);
+ #if (defined(MULTIPROCESSOR) || defined(LOCKDEBUG)) && defined(SSCOM_MPLOCK)
+ 	simple_lock_init(&sc->sc_lock);
+ #endif
+ 
+ 	sc->sc_ucon = UCON_RXINT_ENABLE|UCON_TXINT_ENABLE;
+ 
+ 	/*
+ 	 * set default for modem control hook
+ 	 */
+ 	if (sc->set_modem_control == NULL)
+ 		sc->set_modem_control = sscom_set_modem_control;
+ 	if (sc->read_modem_status == NULL)
+ 		sc->read_modem_status = sscom_read_modem_status;
+ 
+ 	/* Disable interrupts before configuring the device. */
+ 	sscom_disable_txrxint(sc);
+ 
+ #ifdef KGDB
+ 	/*
+ 	 * Allow kgdb to "take over" this port.  If this is
+ 	 * the kgdb device, it has exclusive use.
+ 	 */
+ 	if (unit == sscom_kgdb_unit) {
+ 		SET(sc->sc_hwflags, SSCOM_HW_KGDB);
+ 		sc->sc_ucon = UCON_DEBUGPORT;
+ 	}
+ #endif
+ 
+ 	if (unit == sscomconsunit) {
+ 		sscomconsattached = 1;
+ 
+ 		sscomconstag = iot;
+ 		sscomconsioh = ioh;
+ 
+ 		/* Make sure the console is always "hardwired". */
+ 		delay(1000);			/* XXX: wait for output to finish */
+ 		SET(sc->sc_hwflags, SSCOM_HW_CONSOLE);
+ 		SET(sc->sc_swflags, TIOCFLAG_SOFTCAR);
+ 
+ 		sc->sc_ucon = UCON_DEBUGPORT;
+ 	}
+ 
+ 	bus_space_write_1(iot, ioh, SSCOM_UFCON,
+ 	    UFCON_TXTRIGGER_8|UFCON_RXTRIGGER_8|UFCON_FIFO_ENABLE|
+ 	    UFCON_TXFIFO_RESET|UFCON_RXFIFO_RESET);
+ 
+ 	bus_space_write_1(iot, ioh, SSCOM_UCON, sc->sc_ucon);
+ 
+ #ifdef KGDB
+ 	if (ISSET(sc->sc_hwflags, SSCOM_HW_KGDB)) {
+ 		sscom_kgdb_attached = 1;
+ 		printf("%s: kgdb\n", sc->sc_dev.dv_xname);
+ 		sscom_enable_debugport(sc);
+ 		return;
+ 	}
+ #endif
+ 
+ 
+ 
+ 	tp = ttymalloc();
+ 	tp->t_oproc = sscomstart;
+ 	tp->t_param = sscomparam;
+ 	tp->t_hwiflow = sscomhwiflow;
+ 
+ 	sc->sc_tty = tp;
+ 	sc->sc_rbuf = malloc(sscom_rbuf_size << 1, M_DEVBUF, M_NOWAIT);
+ 	sc->sc_rbput = sc->sc_rbget = sc->sc_rbuf;
+ 	sc->sc_rbavail = sscom_rbuf_size;
+ 	if (sc->sc_rbuf == NULL) {
+ 		printf("%s: unable to allocate ring buffer\n",
+ 		    sc->sc_dev.dv_xname);
+ 		return;
+ 	}
+ 	sc->sc_ebuf = sc->sc_rbuf + (sscom_rbuf_size << 1);
+ 
+ #if 0 /* XXX */
+ 	tty_attach(tp);
+ #endif
+ 
+ 	if (ISSET(sc->sc_hwflags, SSCOM_HW_CONSOLE)) {
+ 		int maj;
+ 
+ 		/* Locate the major number. */
+ 		for (maj = 0; maj < nchrdev; maj++)
+ 			if (cdevsw[maj].d_open == sscomopen)
+ 				break;
+ 
+ 
+ 		cn_tab->cn_dev = makedev(maj, sc->sc_dev.dv_unit);
+ 
+ 		printf("%s: console (major=%d)\n", sc->sc_dev.dv_xname, maj);
+ 	}
+ 
+ 
+ #ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
+ 	sc->sc_si = softintr_establish(IPL_TTY, sscomsoft, sc);
+ 	if (sc->sc_si == NULL)
+ 		panic("%s: can't establish soft interrupt",
+ 		    sc->sc_dev.dv_xname);
+ #else           
+ 	timeout_set(&sc->sc_comsoft_tmo, comsoft, sc);
+ #endif
+ 
+ 
+ #if NRND > 0 && defined(RND_COM)
+ 	rnd_attach_source(&sc->rnd_source, sc->sc_dev.dv_xname,
+ 			  RND_TYPE_TTY, 0);
+ #endif
+ 
+ 	/* if there are no enable/disable functions, assume the device
+ 	   is always enabled */
+ 
+ 	if (ISSET(sc->sc_hwflags, SSCOM_HW_CONSOLE))
+ 		sscom_enable_debugport(sc);
+ 	else 
+ 		sscom_disable_txrxint(sc);
+ 
+ 	SET(sc->sc_hwflags, SSCOM_HW_DEV_OK);
+ }
+ 
+ int
+ sscom_detach(struct device *self, int flags)
+ {
+ 	return 0;
+ }
+ 
+ int
+ sscom_activate(struct device *self, enum devact act)
+ {
+ #ifdef notyet
+ 	struct sscom_softc *sc = (struct sscom_softc *)self;
+ 	int s, rv = 0;
+ 
+ 	s = splserial();
+ 	SSCOM_LOCK(sc);
+ 	switch (act) {
+ 	case DVACT_ACTIVATE:
+ 		rv = EOPNOTSUPP;
+ 		break;
+ 
+ 	case DVACT_DEACTIVATE:
+ 		if (sc->sc_hwflags & (SSCOM_HW_CONSOLE|SSCOM_HW_KGDB)) {
+ 			rv = EBUSY;
+ 			break;
+ 		}
+ 
+ 		sc->enabled = 0;
+ 		break;
+ 	}
+ 
+ 	SSCOM_UNLOCK(sc);	
+ 	splx(s);
+ 	return rv;
+ #else
+ 	return 0;
+ #endif
+ }
+ 
+ void
+ sscom_shutdown(struct sscom_softc *sc)
+ {
+ #ifdef notyet
+ 	struct tty *tp = sc->sc_tty;
+ 	int s;
+ 
+ 	s = splserial();
+ 	SSCOM_LOCK(sc);	
+ 
+ 	/* If we were asserting flow control, then deassert it. */
+ 	SET(sc->sc_rx_flags, RX_IBUF_BLOCKED);
+ 	sscom_hwiflow(sc);
+ 
+ 	/* Clear any break condition set with TIOCSBRK. */
+ 	sscom_break(sc, 0);
+ 
+ 	/*
+ 	 * Hang up if necessary.  Wait a bit, so the other side has time to
+ 	 * notice even if we immediately open the port again.
+ 	 * Avoid tsleeping above splhigh().
+ 	 */
+ 	if (ISSET(tp->t_cflag, HUPCL)) {
+ 		sscom_modem(sc, 0);
+ 		SSCOM_UNLOCK(sc);
+ 		splx(s);
+ 		/* XXX tsleep will only timeout */
+ 		(void) tsleep(sc, TTIPRI, ttclos, hz);
+ 		s = splserial();
+ 		SSCOM_LOCK(sc);	
+ 	}
+ 
+ 	if (ISSET(sc->sc_hwflags, SSCOM_HW_CONSOLE))
+ 		/* interrupt on break */
+ 		sc->sc_ucon = UCON_DEBUGPORT;
+ 	else
+ 		sc->sc_ucon = 0;
+ 	bus_space_write_2(sc->sc_iot, sc->sc_ioh, SSCOM_UCON, sc->sc_ucon);
+ 
+ #ifdef DIAGNOSTIC
+ 	if (!sc->enabled)
+ 		panic("sscom_shutdown: not enabled?");
+ #endif
+ 	sc->enabled = 0;
+ 	SSCOM_UNLOCK(sc);
+ 	splx(s);
+ #endif
+ }
+ 
+ int
+ sscomopen(dev_t dev, int flag, int mode, struct proc *p)
+ {
+ 	struct sscom_softc *sc;
+ 	struct tty *tp;
+ 	int s, s2;
+ 	int error;
+ 	int unit = DEVUNIT(dev);
+ 
+ 	if (unit >= sscom_cd.cd_ndevs)
+ 		return ENXIO;
+ 	sc = sscom_cd.cd_devs[unit];
+ 
+ 	if (sc == NULL || !ISSET(sc->sc_hwflags, SSCOM_HW_DEV_OK) ||
+ 		sc->sc_rbuf == NULL)
+ 		return ENXIO;
+ 
+ #if 0
+ 	if (!device_is_active(&sc->sc_dev))
+ 		return ENXIO;
+ #endif
+ 
+ #ifdef KGDB
+ 	/*
+ 	 * If this is the kgdb port, no other use is permitted.
+ 	 */
+ 	if (ISSET(sc->sc_hwflags, SSCOM_HW_KGDB))
+ 		return EBUSY;
+ #endif
+ 
+ 	tp = sc->sc_tty;
+ 
+ 	s = spltty();
+ 
+ 	/*
+ 	 * Do the following iff this is a first open.
+ 	 */
+ 	if (!ISSET(tp->t_state, TS_ISOPEN)) {
+ 		tp->t_dev = dev;
+ 
+ 		s2 = splserial();
+ 		SSCOM_LOCK(sc);
+ 
+ 		/* Turn on interrupts. */
+ 		sscom_enable_txrxint(sc);
+ 
+ 		/* Fetch the current modem control status, needed later. */
+ 		sc->sc_msts = sc->read_modem_status(sc);
+ 
+ #if 0
+ 		/* Clear PPS capture state on first open. */
+ 		sc->sc_ppsmask = 0;
+ 		sc->ppsparam.mode = 0;
+ #endif
+ 
+ 		SSCOM_UNLOCK(sc);
+ 		splx(s2);
+ 
+ 		/*
+ 		 * Initialize the termios status to the defaults.  Add in the
+ 		 * sticky bits from TIOCSFLAGS.
+ 		 */
+ 		if (ISSET(sc->sc_hwflags, SSCOM_HW_CONSOLE)) {
+ 			tp->t_ispeed = tp->t_ospeed = sscomconsrate;
+ 			tp->t_cflag = sscomconscflag;
+ 		} else {
+ 			tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
+ 			tp->t_cflag = TTYDEF_CFLAG;
+ 		}
+ 		if (ISSET(sc->sc_swflags, TIOCFLAG_CLOCAL))
+ 			SET(tp->t_cflag, CLOCAL);
+ 		if (ISSET(sc->sc_swflags, TIOCFLAG_CRTSCTS))
+ 			SET(tp->t_cflag, CRTSCTS);
+ 		if (ISSET(sc->sc_swflags, TIOCFLAG_MDMBUF))
+ 			SET(tp->t_cflag, MDMBUF);
+ 		/* Make sure sscomparam() will do something. */
+ 		tp->t_ospeed = 0;
+ 		(void) sscomparam(tp, &tp->t_termios);
+ 		tp->t_iflag = TTYDEF_IFLAG;
+ 		tp->t_oflag = TTYDEF_OFLAG;
+ 		tp->t_lflag = TTYDEF_LFLAG;
+ 		ttychars(tp);
+ 		ttsetwater(tp);
+ 
+ 		s2 = splserial();
+ 		SSCOM_LOCK(sc);
+ 
+ 		/*
+ 		 * Turn on DTR.  We must always do this, even if carrier is not
+ 		 * present, because otherwise we'd have to use TIOCSDTR
+ 		 * immediately after setting CLOCAL, which applications do not
+ 		 * expect.  We always assert DTR while the device is open
+ 		 * unless explicitly requested to deassert it.
+ 		 */
+ 		sscom_modem(sc, 1);
+ 
+ 		/* Clear the input ring, and unblock. */
+ 		sc->sc_rbput = sc->sc_rbget = sc->sc_rbuf;
+ 		sc->sc_rbavail = sscom_rbuf_size;
+ 		sscom_iflush(sc);
+ 		CLR(sc->sc_rx_flags, RX_ANY_BLOCK);
+ 		sscom_hwiflow(sc);
+ 
+ 		if (sscom_debug)
+ 			sscomstatus(sc, "sscomopen  ");
+ 
+ 		SSCOM_UNLOCK(sc);
+ 		splx(s2);
+ 	}
+ 	
+ #if 0
+ 	splx(s);
+ 
+ 	error = ttyopen(tp, SSCOMDIALOUT(dev), ISSET(flag, O_NONBLOCK));
+ 	if (error)
+ 		goto bad;
+ #else
+ 	if (SSCOMDIALOUT(dev)) {
+ 		if (ISSET(tp->t_state, TS_ISOPEN)) {
+ 			/* Ah, but someone already is dialed in... */
+ 			splx(s);
+ 			return EBUSY;
+ 		}
+ 		sc->sc_cua = 1;         /* We go into CUA mode. */
+ 	} else {
+ 		/* tty (not cua) device; wait for carrier if necessary. */
+ 		if (ISSET(flag, O_NONBLOCK)) {
+ 			if (sc->sc_cua) {
+ 				/* Opening TTY non-blocking... but the CUA is busy. */
+ 				splx(s);
+ 				return EBUSY;
+ 			}
+ 		} else {
+ 			while (sc->sc_cua ||
+ 			    (!ISSET(tp->t_cflag, CLOCAL) &&
+ 				!ISSET(tp->t_state, TS_CARR_ON))) {
+ 				SET(tp->t_state, TS_WOPEN);
+ 				error = ttysleep(tp, &tp->t_rawq, TTIPRI | PCATCH, ttopen, 0);
+ 				/*
+ 				 * If TS_WOPEN has been reset, that means the cua device
+ 				 * has been closed.  We don't want to fail in that case,
+ 				 * so just go around again.
+ 				 */
+ 				if (error && ISSET(tp->t_state, TS_WOPEN)) {
+ 					CLR(tp->t_state, TS_WOPEN);
+ 					splx(s);
+ 					return error;
+ 				}
+ 			}
+ 		}
+ 	}
+ 	splx(s);
+ #endif
+ 
+         error = (*linesw[tp->t_line].l_open)(dev, tp);
+ 	if (error)
+ 		goto bad;
+ 
+ 	return 0;
+ 
+ bad:
+ 	if (!ISSET(tp->t_state, TS_ISOPEN)) {
+ 		/*
+ 		 * We failed to open the device, and nobody else had it opened.
+ 		 * Clean up the state as appropriate.
+ 		 */
+ 		sscom_shutdown(sc);
+ 	}
+ 
+ 	return error;
+ }
+  
+ int
+ sscomclose(dev_t dev, int flag, int mode, struct proc *p)
+ {
+ 	struct sscom_softc *sc;
+ 	struct tty *tp;
+ 	int unit = DEVUNIT(dev);
+ 	if (unit >= sscom_cd.cd_ndevs)
+ 		return ENXIO;
+ 	sc = sscom_cd.cd_devs[unit];
+ 
+ 	tp = sc->sc_tty;
+ 
+ 	/* XXX This is for cons.c. */
+ 	if (!ISSET(tp->t_state, TS_ISOPEN))
+ 		return 0;
+ 
+         (*linesw[tp->t_line].l_close)(tp, flag);
+ 	ttyclose(tp);
+ 
+ #if 0
+ 	if (SSCOM_ISALIVE(sc) == 0)
+ 		return 0;
+ #endif
+ 
+ 	if (!ISSET(tp->t_state, TS_ISOPEN)) {
+ 		/*
+ 		 * Although we got a last close, the device may still be in
+ 		 * use; e.g. if this was the dialout node, and there are still
+ 		 * processes waiting for carrier on the non-dialout node.
+ 		 */
+ 		sscom_shutdown(sc);
+ 	}
+ 
+ 	return 0;
+ }
+  
+ int
+ sscomread(dev_t dev, struct uio *uio, int flag)
+ {
+         struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(dev)];
+         struct tty *tp = sc->sc_tty;
+ 
+ #if 0
+ 	if (SSCOM_ISALIVE(sc) == 0)
+ 		return EIO;
+ #endif
+  
+         return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
+ }
+  
+ int
+ sscomwrite(dev_t dev, struct uio *uio, int flag)
+ {
+         struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(dev)];
+         struct tty *tp = sc->sc_tty;
+ 
+ #if 0
+ 	if (SSCOM_ISALIVE(sc) == 0)
+ 		return EIO;
+ #endif
+  
+         return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
+ }
+ 
+ #if 0
+ int
+ sscompoll(dev_t dev, int events, struct proc *p)
+ {
+         struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(dev)];
+         struct tty *tp = sc->sc_tty;
+ 
+ #if 0
+ 	if (SSCOM_ISALIVE(sc) == 0)
+ 		return EIO;
+ #endif
+  
+         return ((*linesw[tp->t_line].l_poll)(tp, uio, flag));
+ }
+ #endif
+ 
+ struct tty *
+ sscomtty(dev_t dev)
+ {
+         struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(dev)];
+         struct tty *tp = sc->sc_tty;
+  
+         return (tp);
+ }
+ 
+ int
+ sscomioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
+ {
+         struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(dev)];
+         struct tty *tp = sc->sc_tty;
+ 	int error;
+ 
+ #if 0
+ 	if (SSCOM_ISALIVE(sc) == 0)
+ 		return EIO;
+ #endif
+ 
+         error = ((*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p));
+ 	if (error >= 0)
+ 		return error;
+ 
+ 	error = ttioctl(tp, cmd, data, flag, p);
+ 	if (error >= 0)
+ 		return error;
+ 
+ 	error = 0;
+ 
+ #if 0
+ 	SSCOM_LOCK(sc);	
+ #endif
+ 
+ 	switch (cmd) {
+ 	case TIOCSBRK:
+ 		sscom_break(sc, 1);
+ 		break;
+ 
+ 	case TIOCCBRK:
+ 		sscom_break(sc, 0);
+ 		break;
+ 
+ 	case TIOCSDTR:
+ 		sscom_modem(sc, 1);
+ 		break;
+ 
+ 	case TIOCCDTR:
+ 		sscom_modem(sc, 0);
+ 		break;
+ 
+ 	case TIOCGFLAGS:
+ 		*(int *)data = sc->sc_swflags;
+ 		break;
+ 
+ 	case TIOCSFLAGS:
+ 		error = suser(p, 0);
+ 		if (error)
+ 			break;
+ 		sc->sc_swflags = *(int *)data;
+ 		break;
+ 
+ 	case TIOCMSET:
+ 	case TIOCMBIS:
+ 	case TIOCMBIC:
+ 		tiocm_to_sscom(sc, cmd, *(int *)data);
+ 		break;
+ 
+ 	case TIOCMGET:
+ 		*(int *)data = sscom_to_tiocm(sc);
+ 		break;
+ 
+ 	default:
+ 		error = ENOTTY;
+ 		break;
+ 	}
+ 
+ #if 0
+ 	SSCOM_UNLOCK(sc);
+ #endif
+ 
+ 	if (sscom_debug)
+ 		sscomstatus(sc, "sscomioctl ");
+ 
+ 	return error;
+ }
+ 
+ void
+ sscom_schedrx(struct sscom_softc *sc)
+ {
+ 
+ 	sc->sc_rx_ready = 1;
+ 
+ 	/* Wake up the poller. */
+ #ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
+ 	softintr_schedule(sc->sc_si);
+ #endif
+ }
+ 
+ static void
+ sscom_break(struct sscom_softc *sc, int onoff)
+ {
+ 
+ 	if (onoff)
+ 		SET(sc->sc_ucon, UCON_SBREAK);
+ 	else
+ 		CLR(sc->sc_ucon, UCON_SBREAK);
+ 
+ 	if (!sc->sc_heldchange) {
+ 		if (sc->sc_tx_busy) {
+ 			sc->sc_heldtbc = sc->sc_tbc;
+ 			sc->sc_tbc = 0;
+ 			sc->sc_heldchange = 1;
+ 		} else
+ 			sscom_loadchannelregs(sc);
+ 	}
+ }
+ 
+ static void
+ sscom_modem(struct sscom_softc *sc, int onoff)
+ {
+ 	if (onoff)
+ 		SET(sc->sc_umcon, UMCON_DTR);
+ 	else
+ 		CLR(sc->sc_umcon, UMCON_DTR);
+ 
+ 	if (!sc->sc_heldchange) {
+ 		if (sc->sc_tx_busy) {
+ 			sc->sc_heldtbc = sc->sc_tbc;
+ 			sc->sc_tbc = 0;
+ 			sc->sc_heldchange = 1;
+ 		} else
+ 			sscom_loadchannelregs(sc);
+ 	}
+ }
+ 
+ static void
+ tiocm_to_sscom(struct sscom_softc *sc, u_long how, int ttybits)
+ {
+ 	u_char sscombits;
+ 
+ 	sscombits = 0;
+ 	if (ISSET(ttybits, TIOCM_DTR))
+ 		sscombits = UMCON_DTR;
+ 	if (ISSET(ttybits, TIOCM_RTS))
+ 		SET(sscombits, UMCON_RTS);
+  
+ 	switch (how) {
+ 	case TIOCMBIC:
+ 		CLR(sc->sc_umcon, sscombits);
+ 		break;
+ 
+ 	case TIOCMBIS:
+ 		SET(sc->sc_umcon, sscombits);
+ 		break;
+ 
+ 	case TIOCMSET:
+ 		CLR(sc->sc_umcon, UMCON_DTR);
+ 		SET(sc->sc_umcon, sscombits);
+ 		break;
+ 	}
+ 
+ 	if (!sc->sc_heldchange) {
+ 		if (sc->sc_tx_busy) {
+ 			sc->sc_heldtbc = sc->sc_tbc;
+ 			sc->sc_tbc = 0;
+ 			sc->sc_heldchange = 1;
+ 		} else
+ 			sscom_loadchannelregs(sc);
+ 	}
+ }
+ 
+ static int
+ sscom_to_tiocm(struct sscom_softc *sc)
+ {
+ 	u_char sscombits;
+ 	int ttybits = 0;
+ 
+ 	sscombits = sc->sc_umcon;
+ #if 0
+ 	if (ISSET(sscombits, MCR_DTR))
+ 		SET(ttybits, TIOCM_DTR);
+ #endif
+ 	if (ISSET(sscombits, UMCON_RTS))
+ 		SET(ttybits, TIOCM_RTS);
+ 
+ 	sscombits = sc->sc_msts;
+ 	if (ISSET(sscombits, MSTS_DCD))
+ 		SET(ttybits, TIOCM_CD);
+ 	if (ISSET(sscombits, MSTS_DSR))
+ 		SET(ttybits, TIOCM_DSR);
+ 	if (ISSET(sscombits, MSTS_CTS))
+ 		SET(ttybits, TIOCM_CTS);
+ 
+ 	if (sc->sc_ucon != 0)
+ 		SET(ttybits, TIOCM_LE);
+ 
+ 	return ttybits;
+ }
+ 
+ static int
+ cflag2lcr(tcflag_t cflag)
+ {
+ 	u_char lcr = ULCON_PARITY_NONE;
+ 
+ 	switch (cflag & (PARENB|PARODD)) {
+ 	case PARENB|PARODD:
+ 		lcr = ULCON_PARITY_ODD;
+ 		break;
+ 	case PARENB:
+ 		lcr = ULCON_PARITY_EVEN;
+ 	}
+ 
+ 	switch (ISSET(cflag, CSIZE)) {
+ 	case CS5:
+ 		SET(lcr, ULCON_LENGTH_5);
+ 		break;
+ 	case CS6:
+ 		SET(lcr, ULCON_LENGTH_6);
+ 		break;
+ 	case CS7:
+ 		SET(lcr, ULCON_LENGTH_7);
+ 		break;
+ 	case CS8:
+ 		SET(lcr, ULCON_LENGTH_8);
+ 		break;
+ 	}
+ 	if (ISSET(cflag, CSTOPB))
+ 		SET(lcr, ULCON_STOP);
+ 
+ 	return lcr;
+ }
+ 
+ int
+ sscomparam(struct tty *tp, struct termios *t)
+ {
+         struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(tp->t_dev)];
+ 	int ospeed;
+ 	u_char lcr;
+ 	int s;
+ 
+ #if 0
+ 	if (SSCOM_ISALIVE(sc) == 0)
+ 		return EIO;
+ #endif
+ 
+ 	ospeed = sscomspeed(t->c_ospeed, sc->sc_frequency);
+ 
+ 	/* Check requested parameters. */
+ 	if (ospeed < 0)
+ 		return EINVAL;
+ 	if (t->c_ispeed && t->c_ispeed != t->c_ospeed)
+ 		return EINVAL;
+ 
+ 	/*
+ 	 * For the console, always force CLOCAL and !HUPCL, so that the port
+ 	 * is always active.
+ 	 */
+ 	if (ISSET(sc->sc_swflags, TIOCFLAG_SOFTCAR) ||
+ 	    ISSET(sc->sc_hwflags, SSCOM_HW_CONSOLE)) {
+ 		SET(t->c_cflag, CLOCAL);
+ 		CLR(t->c_cflag, HUPCL);
+ 	}
+ 
+ 	/*
+ 	 * If there were no changes, don't do anything.  This avoids dropping
+ 	 * input and improves performance when all we did was frob things like
+ 	 * VMIN and VTIME.
+ 	 */
+ 	if (tp->t_ospeed == t->c_ospeed &&
+ 	    tp->t_cflag == t->c_cflag)
+ 		return 0;
+ 
+ 	lcr = cflag2lcr(t->c_cflag);
+ 
+ 	s = splserial();
+ 	SSCOM_LOCK(sc);	
+ 
+ 	sc->sc_ulcon = lcr;
+ 
+ 	/*
+ 	 * If we're not in a mode that assumes a connection is present, then
+ 	 * ignore carrier changes.
+ 	 */
+ 	if (ISSET(t->c_cflag, CLOCAL | MDMBUF))
+ 		sc->sc_msr_dcd = 0;
+ 	else
+ 		sc->sc_msr_dcd = MSTS_DCD;
+ 
+ 	/*
+ 	 * Set the flow control pins depending on the current flow control
+ 	 * mode.
+ 	 */
+ 	if (ISSET(t->c_cflag, CRTSCTS)) {
+ 		sc->sc_mcr_dtr = UMCON_DTR;
+ 		sc->sc_mcr_rts = UMCON_RTS;
+ 		sc->sc_msr_cts = MSTS_CTS;
+ 	}
+ 	else if (ISSET(t->c_cflag, MDMBUF)) {
+ 		/*
+ 		 * For DTR/DCD flow control, make sure we don't toggle DTR for
+ 		 * carrier detection.
+ 		 */
+ 		sc->sc_mcr_dtr = 0;
+ 		sc->sc_mcr_rts = UMCON_DTR;
+ 		sc->sc_msr_cts = MSTS_DCD;
+ 	} 
+ 	else {
+ 		/*
+ 		 * If no flow control, then always set RTS.  This will make
+ 		 * the other side happy if it mistakenly thinks we're doing
+ 		 * RTS/CTS flow control.
+ 		 */
+ 		sc->sc_mcr_dtr = UMCON_DTR | UMCON_RTS;
+ 		sc->sc_mcr_rts = 0;
+ 		sc->sc_msr_cts = 0;
+ 		if (ISSET(sc->sc_umcon, UMCON_DTR))
+ 			SET(sc->sc_umcon, UMCON_RTS);
+ 		else
+ 			CLR(sc->sc_umcon, UMCON_RTS);
+ 	}
+ 	sc->sc_msr_mask = sc->sc_msr_cts | sc->sc_msr_dcd;
+ 
+ 	if (ospeed == 0)
+ 		CLR(sc->sc_umcon, sc->sc_mcr_dtr);
+ 	else
+ 		SET(sc->sc_umcon, sc->sc_mcr_dtr);
+ 
+ 	sc->sc_ubrdiv = ospeed;
+ 
+ 	/* And copy to tty. */
+ 	tp->t_ispeed = 0;
+ 	tp->t_ospeed = t->c_ospeed;
+ 	tp->t_cflag = t->c_cflag;
+ 
+ 	if (!sc->sc_heldchange) {
+ 		if (sc->sc_tx_busy) {
+ 			sc->sc_heldtbc = sc->sc_tbc;
+ 			sc->sc_tbc = 0;
+ 			sc->sc_heldchange = 1;
+ 		} else
+ 			sscom_loadchannelregs(sc);
+ 	}
+ 
+ 	if (!ISSET(t->c_cflag, CHWFLOW)) {
+ 		/* Disable the high water mark. */
+ 		sc->sc_r_hiwat = 0;
+ 		sc->sc_r_lowat = 0;
+ 		if (ISSET(sc->sc_rx_flags, RX_TTY_OVERFLOWED)) {
+ 			CLR(sc->sc_rx_flags, RX_TTY_OVERFLOWED);
+ 			sscom_schedrx(sc);
+ 		}
+ 		if (ISSET(sc->sc_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED)) {
+ 			CLR(sc->sc_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED);
+ 			sscom_hwiflow(sc);
+ 		}
+ 	} else {
+ 		sc->sc_r_hiwat = sscom_rbuf_hiwat;
+ 		sc->sc_r_lowat = sscom_rbuf_lowat;
+ 	}
+ 
+ 	SSCOM_UNLOCK(sc);
+ 	splx(s);
+ 
+ 	/*
+ 	 * Update the tty layer's idea of the carrier bit, in case we changed
+ 	 * CLOCAL or MDMBUF.  We don't hang up here; we only do that by
+ 	 * explicit request.
+ 	 */
+ 	(*linesw[tp->t_line].l_modem)(tp, ISSET(sc->sc_msts, MSTS_DCD));
+ 
+ 
+ 	if (sscom_debug)
+ 		sscomstatus(sc, "sscomparam ");
+ 
+ 	if (!ISSET(t->c_cflag, CHWFLOW)) {
+ 		if (sc->sc_tx_stopped) {
+ 			sc->sc_tx_stopped = 0;
+ 			sscomstart(tp);
+ 		}
+ 	}
+ 
+ 	return 0;
+ }
+ 
+ static void
+ sscom_iflush(struct sscom_softc *sc)
+ {
+ 	bus_space_tag_t iot = sc->sc_iot;
+ 	bus_space_handle_t ioh = sc->sc_ioh;
+ 	int timo;
+ 
+ 
+ 	timo = 50000;
+ 	/* flush any pending I/O */
+ 	while ( sscom_rxrdy(iot, ioh) && --timo)
+ 		(void)sscom_getc(iot,ioh);
+ #ifdef DIAGNOSTIC
+ 	if (!timo)
+ 		printf("%s: sscom_iflush timeout\n", sc->sc_dev.dv_xname);
+ #endif
+ }
+ 
+ static void
+ sscom_loadchannelregs(struct sscom_softc *sc)
+ {
+ 	bus_space_tag_t iot = sc->sc_iot;
+ 	bus_space_handle_t ioh = sc->sc_ioh;
+ 
+ 	/* XXXXX necessary? */
+ 	sscom_iflush(sc);
+ 
+ 	bus_space_write_2(iot, ioh, SSCOM_UCON, 0);
+ 
+ #if 0
+ 	if (ISSET(sc->sc_hwflags, COM_HW_FLOW)) {
+ 		bus_space_write_1(iot, ioh, com_lcr, LCR_EERS);
+ 		bus_space_write_1(iot, ioh, com_efr, sc->sc_efr);
+ 	}
+ #endif
+ 
+ 	bus_space_write_2(iot, ioh, SSCOM_UBRDIV, sc->sc_ubrdiv);
+ 	bus_space_write_1(iot, ioh, SSCOM_ULCON, sc->sc_ulcon);
+ 	sc->set_modem_control(sc);
+ 	bus_space_write_2(iot, ioh, SSCOM_UCON, sc->sc_ucon);
+ }
+ 
+ static int
+ sscomhwiflow(struct tty *tp, int block)
+ {
+         struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(tp->t_dev)];
+ 	int s;
+ 
+ #if 0
+ 	if (SSCOM_ISALIVE(sc) == 0)
+ 		return 0;
+ #endif
+ 
+ 	if (sc->sc_mcr_rts == 0)
+ 		return 0;
+ 
+ 	s = splserial();
+ 	SSCOM_LOCK(sc);
+ 	
+ 	if (block) {
+ 		if (!ISSET(sc->sc_rx_flags, RX_TTY_BLOCKED)) {
+ 			SET(sc->sc_rx_flags, RX_TTY_BLOCKED);
+ 			sscom_hwiflow(sc);
+ 		}
+ 	} else {
+ 		if (ISSET(sc->sc_rx_flags, RX_TTY_OVERFLOWED)) {
+ 			CLR(sc->sc_rx_flags, RX_TTY_OVERFLOWED);
+ 			sscom_schedrx(sc);
+ 		}
+ 		if (ISSET(sc->sc_rx_flags, RX_TTY_BLOCKED)) {
+ 			CLR(sc->sc_rx_flags, RX_TTY_BLOCKED);
+ 			sscom_hwiflow(sc);
+ 		}
+ 	}
+ 
+ 	SSCOM_UNLOCK(sc);
+ 	splx(s);
+ 	return 1;
+ }
+ 	
+ /*
+  * (un)block input via hw flowcontrol
+  */
+ static void
+ sscom_hwiflow(struct sscom_softc *sc)
+ {
+ 	if (sc->sc_mcr_rts == 0)
+ 		return;
+ 
+ 	if (ISSET(sc->sc_rx_flags, RX_ANY_BLOCK)) {
+ 		CLR(sc->sc_umcon, sc->sc_mcr_rts);
+ 		CLR(sc->sc_mcr_active, sc->sc_mcr_rts);
+ 	} else {
+ 		SET(sc->sc_umcon, sc->sc_mcr_rts);
+ 		SET(sc->sc_mcr_active, sc->sc_mcr_rts);
+ 	}
+ 	sc->set_modem_control(sc);
+ }
+ 
+ 
+ void
+ sscomstart(struct tty *tp)
+ {
+         struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(tp->t_dev)];
+ 	int s;
+ 
+ #if 0
+ 	if (SSCOM_ISALIVE(sc) == 0)
+ 		return;
+ #endif
+ 
+ 	s = spltty();
+ 	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP))
+ 		goto out;
+ 	if (sc->sc_tx_stopped)
+ 		goto out;
+ #if 0
+ 	if (!ttypull(tp))
+ 		goto out;
+ 
+ 	/* Grab the first contiguous region of buffer space. */
+ 	{
+ 		u_char *tba;
+ 		int tbc;
+ 
+ 		tba = tp->t_outq.c_cf;
+ 		tbc = ndqb(&tp->t_outq, 0);
+ 
+ 		(void)splserial();
+ 		SSCOM_LOCK(sc);
+ 
+ 		sc->sc_tba = tba;
+ 		sc->sc_tbc = tbc;
+ 	}
+ #else
+ 	if (tp->t_outq.c_cc <= tp->t_lowat) {
+ 		if (ISSET(tp->t_state, TS_ASLEEP)) {
+ 			CLR(tp->t_state, TS_ASLEEP);
+ 			wakeup(&tp->t_outq);
+ 		}
+ 		if (tp->t_outq.c_cc == 0)
+ 			goto out;
+ 		selwakeup(&tp->t_wsel);
+ 	}
+ 	SET(tp->t_state, TS_BUSY);
+ #endif
+ 
+ 	SET(tp->t_state, TS_BUSY);
+ 	sc->sc_tx_busy = 1;
+ 
+ 	/* Output the first chunk of the contiguous buffer. */
+ 	sscom_output_chunk(sc);
+ 
+ 	/* Enable transmit completion interrupts if necessary. */
+ 	if ((sc->sc_hwflags & SSCOM_HW_TXINT) == 0)
+ 		sscom_enable_txint(sc);
+ 
+ 	SSCOM_UNLOCK(sc);
+ out:
+ 	splx(s);
+ 	return;
+ }
+ 
+ /*
+  * Stop output on a line.
+  */
+ int
+ sscomstop(struct tty *tp, int flag)
+ {
+         struct sscom_softc *sc = sscom_cd.cd_devs[DEVUNIT(tp->t_dev)];
+ 	int s;
+ 
+ 	s = splserial();
+ 	SSCOM_LOCK(sc);
+ 	if (ISSET(tp->t_state, TS_BUSY)) {
+ 		/* Stop transmitting at the next chunk. */
+ 		sc->sc_tbc = 0;
+ 		sc->sc_heldtbc = 0;
+ 		if (!ISSET(tp->t_state, TS_TTSTOP))
+ 			SET(tp->t_state, TS_FLUSH);
+ 	}
+ 	SSCOM_UNLOCK(sc);	
+ 	splx(s);
+ 	return 0;
+ }
+ 
+ void
+ sscomdiag(void *arg)
+ {
+ 	struct sscom_softc *sc = arg;
+ 	int overflows, floods;
+ 	int s;
+ 
+ 	s = splserial();
+ 	SSCOM_LOCK(sc);
+ 	overflows = sc->sc_overflows;
+ 	sc->sc_overflows = 0;
+ 	floods = sc->sc_floods;
+ 	sc->sc_floods = 0;
+ 	sc->sc_errors = 0;
+ 	SSCOM_UNLOCK(sc);
+ 	splx(s);
+ 
+ 	log(LOG_WARNING, "%s: %d silo overflow%s, %d ibuf flood%s\n",
+ 	    sc->sc_dev.dv_xname,
+ 	    overflows, overflows == 1 ? "" : "s",
+ 	    floods, floods == 1 ? "" : "s");
+ }
+ 
+ void
+ sscom_rxsoft(struct sscom_softc *sc, struct tty *tp)
+ {
+ 	u_char *get, *end;
+ 	u_int cc, scc;
+ 	u_char rsr;
+ 	int code;
+ 	int s;
+ 
+ 	end = sc->sc_ebuf;
+ 	get = sc->sc_rbget;
+ 	scc = cc = sscom_rbuf_size - sc->sc_rbavail;
+ 
+ 	if (cc == sscom_rbuf_size) {
+ 		sc->sc_floods++;
+ 		if (sc->sc_errors++ == 0)
+ 			timeout_add(&sc->sc_diag_timeout, 60 * hz);
+ 	}
+ 
+ 	while (cc) {
+ 		code = get[0];
+ 		rsr = get[1];
+ 		if (rsr) {
+ 			if (ISSET(rsr, UERSTAT_OVERRUN)) {
+ 				sc->sc_overflows++;
+ 				if (sc->sc_errors++ == 0)
+ 					timeout_add(&sc->sc_diag_timeout,
+ 					    60 * hz);
+ 			}
+ 			if (ISSET(rsr, UERSTAT_BREAK | UERSTAT_FRAME))
+ 				SET(code, TTY_FE);
+ 			if (ISSET(rsr, UERSTAT_PARITY))
+ 				SET(code, TTY_PE);
+ 		}
+                 (*linesw[tp->t_line].l_rint)(rsr << 8 | code, tp);
+ 
+ #if 0
+ 		if ((*rint)(code, tp) == -1) {
+ 			/*
+ 			 * The line discipline's buffer is out of space.
+ 			 */
+ 			if (!ISSET(sc->sc_rx_flags, RX_TTY_BLOCKED)) {
+ 				/*
+ 				 * We're either not using flow control, or the
+ 				 * line discipline didn't tell us to block for
+ 				 * some reason.  Either way, we have no way to
+ 				 * know when there's more space available, so
+ 				 * just drop the rest of the data.
+ 				 */
+ 				get += cc << 1;
+ 				if (get >= end)
+ 					get -= sscom_rbuf_size << 1;
+ 				cc = 0;
+ 			} else {
+ 				/*
+ 				 * Don't schedule any more receive processing
+ 				 * until the line discipline tells us there's
+ 				 * space available (through sscomhwiflow()).
+ 				 * Leave the rest of the data in the input
+ 				 * buffer.
+ 				 */
+ 				SET(sc->sc_rx_flags, RX_TTY_OVERFLOWED);
+ 			}
+ 			break;
+ 		}
+ #endif
+ 		get += 2;
+ 		if (get >= end)
+ 			get = sc->sc_rbuf;
+ 		cc--;
+ 	}
+ 
+ 	if (cc != scc) {
+ 		sc->sc_rbget = get;
+ 		s = splserial();
+ 		SSCOM_LOCK(sc);
+ 		
+ 		cc = sc->sc_rbavail += scc - cc;
+ 		/* Buffers should be ok again, release possible block. */
+ 		if (cc >= sc->sc_r_lowat) {
+ 			if (ISSET(sc->sc_rx_flags, RX_IBUF_OVERFLOWED)) {
+ 				CLR(sc->sc_rx_flags, RX_IBUF_OVERFLOWED);
+ 				sscom_enable_rxint(sc);
+ 				sc->sc_ucon |= UCON_ERRINT;
+ 				bus_space_write_2(sc->sc_iot, sc->sc_ioh, SSCOM_UCON, 
+ 						  sc->sc_ucon);
+ 
+ 			}
+ 			if (ISSET(sc->sc_rx_flags, RX_IBUF_BLOCKED)) {
+ 				CLR(sc->sc_rx_flags, RX_IBUF_BLOCKED);
+ 				sscom_hwiflow(sc);
+ 			}
+ 		}
+ 		SSCOM_UNLOCK(sc);
+ 		splx(s);
+ 	}
+ }
+ 
+ void
+ sscom_txsoft(struct sscom_softc *sc, struct tty *tp)
+ {
+ 
+ 	CLR(tp->t_state, TS_BUSY);
+ 	if (ISSET(tp->t_state, TS_FLUSH))
+ 		CLR(tp->t_state, TS_FLUSH);
+ 	else
+ 		ndflush(&tp->t_outq, (int)(sc->sc_tba - tp->t_outq.c_cf));
+ 	(*linesw[tp->t_line].l_start)(tp);
+ 
+ }
+ 
+ void
+ sscom_stsoft(struct sscom_softc *sc, struct tty *tp)
+ {
+ 	u_char msr, delta;
+ 	int s;
+ 
+ 	s = splserial();
+ 	SSCOM_LOCK(sc);
+ 	msr = sc->sc_msts;
+ 	delta = sc->sc_msr_delta;
+ 	sc->sc_msr_delta = 0;
+ 	SSCOM_UNLOCK(sc);	
+ 	splx(s);
+ 
+ 	if (ISSET(delta, sc->sc_msr_dcd)) {
+ 		/*
+ 		 * Inform the tty layer that carrier detect changed.
+ 		 */
+ 		(*linesw[tp->t_line].l_modem)(tp, ISSET(msr, MSTS_DCD));
+ 
+ 	}
+ 
+ 	if (ISSET(delta, sc->sc_msr_cts)) {
+ 		/* Block or unblock output according to flow control. */
+ 		if (ISSET(msr, sc->sc_msr_cts)) {
+ 			sc->sc_tx_stopped = 0;
+ 			(*linesw[tp->t_line].l_start)(tp);
+ 		} else {
+ 			sc->sc_tx_stopped = 1;
+ 		}
+ 	}
+ 
+ 	if (sscom_debug)
+ 		sscomstatus(sc, "sscom_stsoft");
+ }
+ 
+ void
+ sscomsoft(void *arg)
+ {
+ 	struct sscom_softc *sc = arg;
+ 	struct tty *tp;
+ 
+ #if 0
+ 	if (SSCOM_ISALIVE(sc) == 0)
+ 		return;
+ #endif
+ 
+ 	{
+ 		tp = sc->sc_tty;
+ 		
+ 		if (sc->sc_rx_ready) {
+ 			sc->sc_rx_ready = 0;
+ 			sscom_rxsoft(sc, tp);
+ 		}
+ 
+ 		if (sc->sc_st_check) {
+ 			sc->sc_st_check = 0;
+ 			sscom_stsoft(sc, tp);
+ 		}
+ 
+ 		if (sc->sc_tx_done) {
+ 			sc->sc_tx_done = 0;
+ 			sscom_txsoft(sc, tp);
+ 		}
+ 	}
+ #ifndef __HAVE_GENERIC_SOFT_INTERRUPTS
+         timeout_add(&sc->sc_comsoft_tmo, 1);
+ #else 
+         ;
+ #endif
+ 
+ }
+ 
+ 
+ int
+ sscomrxintr(void *arg)
+ {
+ 	struct sscom_softc *sc = arg;
+ 	bus_space_tag_t iot = sc->sc_iot;
+ 	bus_space_handle_t ioh = sc->sc_ioh;
+ 	u_char *put, *end;
+ 	u_int cc;
+ 
+ #if 0
+ 	if (SSCOM_ISALIVE(sc) == 0)
+ 		return 0;
+ #endif
+ 
+ 	SSCOM_LOCK(sc);
+ 
+ 	end = sc->sc_ebuf;
+ 	put = sc->sc_rbput;
+ 	cc = sc->sc_rbavail;
+ 
+ 	do {
+ 		u_char	msts, delta;
+ 		u_char  uerstat;
+ 		uint16_t ufstat;
+ 
+ 		ufstat = bus_space_read_2(iot, ioh, SSCOM_UFSTAT);
+ 
+ 		/* XXX: break interrupt with no character? */
+ 
+ 		if ( (ufstat & (UFSTAT_RXCOUNT|UFSTAT_RXFULL)) &&
+ 		    !ISSET(sc->sc_rx_flags, RX_IBUF_OVERFLOWED)) {
+ 
+ 			while (cc > 0) {
+ 				int cn_trapped = 0;
+ 
+ 				/* get status and received character.
+ 				   read status register first */
+ 				uerstat = sscom_geterr(iot, ioh);
+ 				put[0] = sscom_getc(iot, ioh);
+ 
+ 				if (ISSET(uerstat, UERSTAT_BREAK)) {
+ 					int con_trapped = 0;
+ #if 0
+ 					cn_check_magic(sc->sc_tty->t_dev,
+ 					    CNC_BREAK, sscom_cnm_state);
+ #endif
+ 					if (con_trapped)
+ 						continue;
+ #if defined(KGDB)
+ 					if (ISSET(sc->sc_hwflags,
+ 					    SSCOM_HW_KGDB)) {
+ 						kgdb_connect(1);
+ 						continue;
+ 					}
+ #endif
+ 				}
+ 
+ 				put[1] = uerstat;
+ #if 0
+ 				cn_check_magic(sc->sc_tty->t_dev,
+ 					       put[0], sscom_cnm_state);
+ #endif
+ 				if (!cn_trapped) {
+ 					put += 2;
+ 					if (put >= end)
+ 						put = sc->sc_rbuf;
+ 					cc--;
+ 				}
+ 
+ 				ufstat = bus_space_read_2(iot, ioh, SSCOM_UFSTAT);
+ 				if ( (ufstat & (UFSTAT_RXFULL|UFSTAT_RXCOUNT)) == 0 )
+ 					break;
+ 			}
+ 
+ 			/*
+ 			 * Current string of incoming characters ended because
+ 			 * no more data was available or we ran out of space.
+ 			 * Schedule a receive event if any data was received.
+ 			 * If we're out of space, turn off receive interrupts.
+ 			 */
+ 			sc->sc_rbput = put;
+ 			sc->sc_rbavail = cc;
+ 			if (!ISSET(sc->sc_rx_flags, RX_TTY_OVERFLOWED))
+ 				sc->sc_rx_ready = 1;
+ 
+ 			/*
+ 			 * See if we are in danger of overflowing a buffer. If
+ 			 * so, use hardware flow control to ease the pressure.
+ 			 */
+ 			if (!ISSET(sc->sc_rx_flags, RX_IBUF_BLOCKED) &&
+ 			    cc < sc->sc_r_hiwat) {
+ 				SET(sc->sc_rx_flags, RX_IBUF_BLOCKED);
+ 				sscom_hwiflow(sc);
+ 			}
+ 
+ 			/*
+ 			 * If we're out of space, disable receive interrupts
+ 			 * until the queue has drained a bit.
+ 			 */
+ 			if (!cc) {
+ 				SET(sc->sc_rx_flags, RX_IBUF_OVERFLOWED);
+ 				sscom_disable_rxint(sc);
+ 				sc->sc_ucon &= ~UCON_ERRINT;
+ 				bus_space_write_2(iot, ioh, SSCOM_UCON, sc->sc_ucon);
+ 			}
+ 		}
+ 
+ 
+ 		msts = sc->read_modem_status(sc);
+ 		delta = msts ^ sc->sc_msts;
+ 		sc->sc_msts = msts;
+ 
+ #ifdef notyet
+ 		/*
+ 		 * Pulse-per-second (PSS) signals on edge of DCD?
+ 		 * Process these even if line discipline is ignoring DCD.
+ 		 */
+ 		if (delta & sc->sc_ppsmask) {
+ 			struct timeval tv;
+ 		    	if ((msr & sc->sc_ppsmask) == sc->sc_ppsassert) {
+ 				/* XXX nanotime() */
+ 				microtime(&tv);
+ 				TIMEVAL_TO_TIMESPEC(&tv, 
+ 				    &sc->ppsinfo.assert_timestamp);
+ 				if (sc->ppsparam.mode & PPS_OFFSETASSERT) {
+ 					timespecadd(&sc->ppsinfo.assert_timestamp,
+ 					    &sc->ppsparam.assert_offset,
+ 						    &sc->ppsinfo.assert_timestamp);
+ 				}
+ 
+ #ifdef PPS_SYNC
+ 				if (sc->ppsparam.mode & PPS_HARDPPSONASSERT)
+ 					hardpps(&tv, tv.tv_usec);
+ #endif
+ 				sc->ppsinfo.assert_sequence++;
+ 				sc->ppsinfo.current_mode = sc->ppsparam.mode;
+ 
+ 			} else if ((msr & sc->sc_ppsmask) == sc->sc_ppsclear) {
+ 				/* XXX nanotime() */
+ 				microtime(&tv);
+ 				TIMEVAL_TO_TIMESPEC(&tv, 
+ 				    &sc->ppsinfo.clear_timestamp);
+ 				if (sc->ppsparam.mode & PPS_OFFSETCLEAR) {
+ 					timespecadd(&sc->ppsinfo.clear_timestamp,
+ 					    &sc->ppsparam.clear_offset,
+ 					    &sc->ppsinfo.clear_timestamp);
+ 				}
+ 
+ #ifdef PPS_SYNC
+ 				if (sc->ppsparam.mode & PPS_HARDPPSONCLEAR)
+ 					hardpps(&tv, tv.tv_usec);
+ #endif
+ 				sc->ppsinfo.clear_sequence++;
+ 				sc->ppsinfo.current_mode = sc->ppsparam.mode;
+ 			}
+ 		}
+ #endif
+ 
+ 		/*
+ 		 * Process normal status changes
+ 		 */
+ 		if (ISSET(delta, sc->sc_msr_mask)) {
+ 			SET(sc->sc_msr_delta, delta);
+ 
+ 			/*
+ 			 * Stop output immediately if we lose the output
+ 			 * flow control signal or carrier detect.
+ 			 */
+ 			if (ISSET(~msts, sc->sc_msr_mask)) {
+ 				sc->sc_tbc = 0;
+ 				sc->sc_heldtbc = 0;
+ #ifdef SSCOM_DEBUG
+ 				if (sscom_debug)
+ 					sscomstatus(sc, "sscomintr  ");
+ #endif
+ 			}
+ 
+ 			sc->sc_st_check = 1;
+ 		}
+ 
+ 		/* 
+ 		 * Done handling any receive interrupts. 
+ 		 */
+ 
+ 		/*
+ 		 * If we've delayed a parameter change, do it
+ 		 * now, and restart * output.
+ 		 */
+ 		if ((ufstat & UFSTAT_TXCOUNT) == 0) {
+ 			/* XXX: we should check transmitter empty also */
+ 
+ 			if (sc->sc_heldchange) {
+ 				sscom_loadchannelregs(sc);
+ 				sc->sc_heldchange = 0;
+ 				sc->sc_tbc = sc->sc_heldtbc;
+ 				sc->sc_heldtbc = 0;
+ 			}
+ 		}
+ 
+ 
+ 	} while (0);
+ 
+ 	SSCOM_UNLOCK(sc);
+ 
+ 	/* Wake up the poller. */
+ #ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
+ 	softintr_schedule(sc->sc_si);
+ #endif
+ 
+ 
+ 
+ #if NRND > 0 && defined(RND_COM)
+ 	rnd_add_uint32(&sc->rnd_source, iir | rsr);
+ #endif
+ 
+ 	return 1;
+ }
+ 
+ int
+ sscomtxintr(void *arg)
+ {
+ 	struct sscom_softc *sc = arg;
+ 	bus_space_tag_t iot = sc->sc_iot;
+ 	bus_space_handle_t ioh = sc->sc_ioh;
+ 	uint16_t ufstat;
+ 
+ #if 0
+ 	if (SSCOM_ISALIVE(sc) == 0)
+ 		return 0;
+ #endif
+ 
+ 	SSCOM_LOCK(sc);
+ 
+ 	ufstat = bus_space_read_2(iot, ioh, SSCOM_UFSTAT);
+ 
+ 	/*
+ 	 * If we've delayed a parameter change, do it
+ 	 * now, and restart * output.
+ 	 */
+ 	if (sc->sc_heldchange && (ufstat & UFSTAT_TXCOUNT) == 0) {
+ 		/* XXX: we should check transmitter empty also */
+ 		sscom_loadchannelregs(sc);
+ 		sc->sc_heldchange = 0;
+ 		sc->sc_tbc = sc->sc_heldtbc;
+ 		sc->sc_heldtbc = 0;
+ 	}
+ 
+ 	/*
+ 	 * See if data can be transmitted as well. Schedule tx
+ 	 * done event if no data left and tty was marked busy.
+ 	 */
+ 	if (!ISSET(ufstat,UFSTAT_TXFULL)) {
+ 		/* 
+ 		 * Output the next chunk of the contiguous
+ 		 * buffer, if any.
+ 		 */
+ 		if (sc->sc_tbc > 0) {
+ 			__sscom_output_chunk(sc, ufstat);
+ 		}
+ 		else {
+ 			/*
+ 			 * Disable transmit sscompletion
+ 			 * interrupts if necessary.
+ 			 */
+ 			if (sc->sc_hwflags & SSCOM_HW_TXINT)
+ 				sscom_disable_txint(sc);
+ 			if (sc->sc_tx_busy) {
+ 				sc->sc_tx_busy = 0;
+ 				sc->sc_tx_done = 1;
+ 			}
+ 		}
+ 	}
+ 
+ 	SSCOM_UNLOCK(sc);
+ 
+ 	/* Wake up the poller. */
+ 	softintr_schedule(sc->sc_si);
+ 
+ #if NRND > 0 && defined(RND_COM)
+ 	rnd_add_uint32(&sc->rnd_source, iir | rsr);
+ #endif
+ 
+ 	return 1;
+ }
+ 
+ 
+ #if defined(KGDB) || defined(SSCOM0CONSOLE) || defined(SSCOM1CONSOLE)
+ /*
+  * Initialize UART for use as console or KGDB line.
+  */
+ static int
+ sscom_init(bus_space_tag_t iot, const struct sscom_uart_info *config,
+     int rate, int frequency, tcflag_t cflag, bus_space_handle_t *iohp)
+ {
+ 	bus_space_handle_t ioh;
+ 	bus_addr_t iobase = config->iobase;
+ 
+ 	if (bus_space_map(iot, iobase, SSCOM_SIZE, 0, &ioh))
+ 		return ENOMEM; /* ??? */
+ 
+ 	bus_space_write_2(iot, ioh, SSCOM_UCON, 0);
+ 	bus_space_write_1(iot, ioh, SSCOM_UFCON, 
+ 	    UFCON_TXTRIGGER_8 | UFCON_RXTRIGGER_8 |
+ 	    UFCON_TXFIFO_RESET | UFCON_RXFIFO_RESET |
+ 	    UFCON_FIFO_ENABLE );
+ 	/* tx/rx fifo reset are auto-cleared */
+ 
+ 	rate = sscomspeed(rate, frequency);
+ 	bus_space_write_2(iot, ioh, SSCOM_UBRDIV, rate);
+ 	bus_space_write_2(iot, ioh, SSCOM_ULCON, cflag2lcr(cflag));
+ 
+ 	/* enable UART */
+ 	bus_space_write_2(iot, ioh, SSCOM_UCON, 
+ 	    UCON_TXMODE_INT|UCON_RXMODE_INT);
+ 	bus_space_write_2(iot, ioh, SSCOM_UMCON, UMCON_RTS);
+ 
+ 	*iohp = ioh;
+ 	return 0;
+ }
+ 
+ #endif
+ 
+ #if defined(SSCOM0CONSOLE) || defined(SSCOM1CONSOLE)
+ /*
+  * Following are all routines needed for SSCOM to act as console
+  */
+ struct consdev sscomcons = {
+ 	NULL, NULL, sscomcngetc, sscomcnputc, sscomcnpollc, NULL,
+ 	NODEV, CN_HIGHPRI
+ };
+ 
+ 
+ int
+ sscom_cnattach(bus_space_tag_t iot, const struct sscom_uart_info *config,
+     int rate, int frequency, tcflag_t cflag)
+ {
+ 	int res;
+ 
+ 	res = sscom_init(iot, config, rate, frequency, cflag, &sscomconsioh);
+ 	if (res)
+ 		return res;
+ 
+ 	cn_tab = &sscomcons;
+ #if 0
+ 	cn_init_magic(&sscom_cnm_state);
+ 	cn_set_magic("\047\001"); /* default magic is BREAK */
+ #endif
+ 
+ 	sscomconstag = iot;
+ 	sscomconsunit = config->unit;
+ 	sscomconsrate = rate;
+ 	sscomconscflag = cflag;
+ 
+ 	return 0;
+ }
+ 
+ void
+ sscom_cndetach(void)
+ {
+ 	bus_space_unmap(sscomconstag, sscomconsioh, SSCOM_SIZE);
+ 	sscomconstag = NULL;
+ 
+ 	cn_tab = NULL;
+ }
+ 
+ /*
+  * The read-ahead code is so that you can detect pending in-band
+  * cn_magic in polled mode while doing output rather than having to
+  * wait until the kernel decides it needs input.
+  */
+ 
+ #define MAX_READAHEAD	20
+ static int sscom_readahead[MAX_READAHEAD];
+ static int sscom_readaheadcount = 0;
+ 
+ int
+ sscomcngetc(dev_t dev)
+ {
+ 	int s = splserial();
+ 	u_char stat, c;
+ 
+ 	/* got a character from reading things earlier */
+ 	if (sscom_readaheadcount > 0) {
+ 		int i;
+ 
+ 		c = sscom_readahead[0];
+ 		for (i = 1; i < sscom_readaheadcount; i++) {
+ 			sscom_readahead[i-1] = sscom_readahead[i];
+ 		}
+ 		sscom_readaheadcount--;
+ 		splx(s);
+ 		return c;
+ 	}
+ 
+ 	/* block until a character becomes available */
+ 	while (!sscom_rxrdy(sscomconstag, sscomconsioh))
+ 		;
+ 
+ 	c = sscom_getc(sscomconstag, sscomconsioh);
+ 	stat = sscom_geterr(sscomconstag, sscomconsioh);
+ 	{
+ #ifdef DDB
+ 		extern int db_active;
+ 		if (!db_active)
+ #endif
+ #if 0
+ 			cn_check_magic(dev, c, sscom_cnm_state);
+ #else
+ 			;
+ #endif
+ 	}
+ 	splx(s);
+ 	return c;
+ }
+ 
+ /*
+  * Console kernel output character routine.
+  */
+ void
+ sscomcnputc(dev_t dev, int c)
+ {
+ 	int s = splserial();
+ 	int timo;
+ 
+ 	int cin, stat;
+ 	if (sscom_readaheadcount < MAX_READAHEAD && 
+ 	    sscom_rxrdy(sscomconstag, sscomconsioh)) {
+ 	    
+ 		cin = sscom_getc(sscomconstag, sscomconsioh);
+ 		stat = sscom_geterr(sscomconstag, sscomconsioh);
+ #if 0
+ 		cn_check_magic(dev, cin, sscom_cnm_state);
+ #endif
+ 		sscom_readahead[sscom_readaheadcount++] = cin;
+ 	}
+ 
+ 	/* wait for any pending transmission to finish */
+ 	timo = 150000;
+ 	while (ISSET(bus_space_read_2(sscomconstag, sscomconsioh, SSCOM_UFSTAT), 
+ 		   UFSTAT_TXFULL) && --timo)
+ 		continue;
+ 
+ 	bus_space_write_1(sscomconstag, sscomconsioh, SSCOM_UTXH, c);
+ 	SSCOM_BARRIER(sscomconstag, sscomconsioh, BR | BW);
+ 
+ #if 0	
+ 	/* wait for this transmission to complete */
+ 	timo = 1500000;
+ 	while (!ISSET(bus_space_read_1(sscomconstag, sscomconsioh, SSCOM_UTRSTAT), 
+ 		   UTRSTAT_TXEMPTY) && --timo)
+ 		continue;
+ #endif
+ 	splx(s);
+ }
+ 
+ void
+ sscomcnpollc(dev_t dev, int on)
+ {
+ 
+ }
+ 
+ #endif /* SSCOM0CONSOLE||SSCOM1CONSOLE */
+ 
+ #ifdef KGDB
+ int
+ sscom_kgdb_attach(bus_space_tag_t iot, const struct sscom_uart_info *config,
+     int rate, int frequency, tcflag_t cflag)
+ {
+ 	int res;
+ 
+ 	if (iot == sscomconstag && config->unit == sscomconsunit) {
+ 		printf( "console==kgdb_port (%d): kgdb disabled\n", sscomconsunit);
+ 		return EBUSY; /* cannot share with console */
+ 	}
+ 
+ 	res = sscom_init(iot, config, rate, frequency, cflag, &sscom_kgdb_ioh);
+ 	if (res)
+ 		return res;
+ 
+ 	kgdb_attach(sscom_kgdb_getc, sscom_kgdb_putc, NULL);
+ 	kgdb_dev = 123; /* unneeded, only to satisfy some tests */
+ 
+ 	sscom_kgdb_iot = iot;
+ 	sscom_kgdb_unit = config->unit;
+ 
+ 	return 0;
+ }
+ 
+ /* ARGSUSED */
+ int
+ sscom_kgdb_getc(void *arg)
+ {
+ 	int c, stat;
+ 
+ 	/* block until a character becomes available */
+ 	while (!sscom_rxrdy(sscom_kgdb_iot, sscom_kgdb_ioh))
+ 		;
+ 
+ 	c = sscom_getc(sscom_kgdb_iot, sscom_kgdb_ioh);
+ 	stat = sscom_geterr(sscom_kgdb_iot, sscom_kgdb_ioh);
+ 
+ 	return c;
+ }
+ 
+ /* ARGSUSED */
+ void
+ sscom_kgdb_putc(void *arg, int c)
+ {
+ 	int timo;
+ 
+ 	/* wait for any pending transmission to finish */
+ 	timo = 150000;
+ 	while (ISSET(bus_space_read_2(sscom_kgdb_iot, sscom_kgdb_ioh,
+ 	    SSCOM_UFSTAT), UFSTAT_TXFULL) && --timo)
+ 		continue;
+ 
+ 	bus_space_write_1(sscom_kgdb_iot, sscom_kgdb_ioh, SSCOM_UTXH, c);
+ 	SSCOM_BARRIER(sscom_kgdb_iot, sscom_kgdb_ioh, BR | BW);
+ 
+ #if 0	
+ 	/* wait for this transmission to complete */
+ 	timo = 1500000;
+ 	while (!ISSET(bus_space_read_1(sscom_kgdb_iot, sscom_kgdb_ioh,
+ 	    SSCOM_UTRSTAT), UTRSTAT_TXEMPTY) && --timo)
+ 		continue;
+ #endif
+ }
+ #endif /* KGDB */
+ 
+ /* helper function to identify the sscom ports used by
+  console or KGDB (and not yet autoconf attached) */
+ int
+ sscom_is_console(bus_space_tag_t iot, int unit,
+     bus_space_handle_t *ioh)
+ {
+ 	bus_space_handle_t help;
+ 
+ 	if (!sscomconsattached &&
+ 	    iot == sscomconstag && unit == sscomconsunit)
+ 		help = sscomconsioh;
+ #ifdef KGDB
+ 	else if (!sscom_kgdb_attached &&
+ 	    iot == sscom_kgdb_iot && unit == sscom_kgdb_unit)
+ 		help = sscom_kgdb_ioh;
+ #endif
+ 	else
+ 		return 0;
+ 
+ 	if (ioh)
+ 		*ioh = help;
+ 	return 1;
+ }
*** dummy/sscom_s3c2410.c	2012-03-07 17:14:34.495997826 -0600
--- src/sys/arch/arm/s3c2xx0/sscom_s3c2410.c	2012-03-07 17:17:09.967299237 -0600
***************
*** 0 ****
--- 1,177 ----
+ /*	$OpenBSD: sscom_s3c2410.c,v 1.1 2008/11/26 14:39:14 drahn Exp $ */
+ /*	$NetBSD: sscom_s3c2410.c,v 1.2 2005/12/11 12:16:51 christos Exp $ */
+ 
+ /*
+  * Copyright (c) 2002, 2003 Fujitsu Component Limited
+  * Copyright (c) 2002, 2003 Genetec Corporation
+  * All rights reserved.
+  *
+  * Redistribution and use in source and binary forms, with or without
+  * modification, are permitted provided that the following conditions
+  * are met:
+  * 1. Redistributions of source code must retain the above copyright
+  *    notice, this list of conditions and the following disclaimer.
+  * 2. Redistributions in binary form must reproduce the above copyright
+  *    notice, this list of conditions and the following disclaimer in the
+  *    documentation and/or other materials provided with the distribution.
+  * 3. Neither the name of The Fujitsu Component Limited nor the name of
+  *    Genetec corporation may not be used to endorse or promote products
+  *    derived from this software without specific prior written permission.
+  *
+  * THIS SOFTWARE IS PROVIDED BY FUJITSU COMPONENT LIMITED AND GENETEC
+  * CORPORATION ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
+  * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
+  * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
+  * DISCLAIMED.  IN NO EVENT SHALL FUJITSU COMPONENT LIMITED OR GENETEC
+  * CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
+  * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
+  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
+  * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
+  * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
+  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
+  * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
+  * SUCH DAMAGE.
+  */
+ 
+ #include <sys/cdefs.h>
+ /*
+ __KERNEL_RCSID(0, "$NetBSD: sscom_s3c2410.c,v 1.2 2005/12/11 12:16:51 christos Exp $");
+ 
+ #include "opt_sscom.h"
+ #include "opt_ddb.h"
+ #include "opt_kgdb.h"
+ */
+ 
+ #include <sys/param.h>
+ #include <sys/systm.h>
+ #include <sys/ioctl.h>
+ #include <sys/select.h>
+ #include <sys/tty.h>
+ #include <sys/proc.h>
+ #include <sys/user.h>
+ #include <sys/conf.h>
+ #include <sys/file.h>
+ #include <sys/uio.h>
+ #include <sys/kernel.h>
+ #include <sys/syslog.h>
+ #include <sys/types.h>
+ #include <sys/device.h>
+ #include <sys/malloc.h>
+ #include <sys/vnode.h>
+ 
+ #include <machine/intr.h>
+ #include <machine/bus.h>
+ 
+ #include <arm/s3c2xx0/s3c2410reg.h>
+ #include <arm/s3c2xx0/s3c2410var.h>
+ #include <arm/s3c2xx0/sscom_var.h>
+ #include <sys/termios.h>
+ 
+ int sscom_match(struct device *, void *, void *);
+ void sscom_attach(struct device *, struct device *, void *);
+ 
+ int s3c2410_sscom_cnattach(bus_space_tag_t iot, int unit, int rate,
+     int frequency, tcflag_t cflag);
+ 
+ #if 0
+ CFATTACH_DECL(sscom, sizeof(struct sscom_softc), sscom_match,
+     sscom_attach, NULL, NULL);
+ #endif
+ 
+ struct cfdriver sscom_cd = {
+         NULL, "com", DV_TTY
+ };
+ struct cfattach sscom_ca = {
+         sizeof(struct sscom_softc), sscom_match, sscom_attach
+ };
+ 
+ const struct sscom_uart_info s3c2410_uart_config[] = {
+ 	/* UART 0 */
+ 	{
+ 		0,
+ 		S3C2410_INT_TXD0,
+ 		S3C2410_INT_RXD0,
+ 		S3C2410_INT_ERR0,
+ 		S3C2410_UART_BASE(0),
+ 	},
+ 	/* UART 1 */
+ 	{
+ 		1,
+ 		S3C2410_INT_TXD1,
+ 		S3C2410_INT_RXD1,
+ 		S3C2410_INT_ERR1,
+ 		S3C2410_UART_BASE(1),
+ 	},
+ 	/* UART 2 */
+ 	{
+ 		2,
+ 		S3C2410_INT_TXD2,
+ 		S3C2410_INT_RXD2,
+ 		S3C2410_INT_ERR2,
+ 		S3C2410_UART_BASE(2),
+ 	},
+ };
+ 
+ int
+ sscom_match(struct device *parent, void *c, void *aux)
+ {
+ 	struct s3c2xx0_attach_args *sa = aux;
+ 	int unit = sa->sa_index;
+ 
+ 	return unit == 0 || unit == 1;
+ }
+ 
+ void
+ sscom_attach(struct device *parent, struct device *self, void *aux)
+ {
+ 	struct sscom_softc *sc = (struct sscom_softc *)self;
+ 	struct s3c2xx0_attach_args *sa = aux;
+ 	int unit = sa->sa_index;
+ 	bus_addr_t iobase = s3c2410_uart_config[unit].iobase;
+ 
+ 	printf( ": UART%d addr=%lx", sa->sa_index, iobase );
+ 
+ 	sc->sc_iot = s3c2xx0_softc->sc_iot;
+ 	sc->sc_unit = unit;
+ 	sc->sc_frequency = s3c2xx0_softc->sc_pclk;
+ 
+ 	sc->sc_rx_irqno = s3c2410_uart_config[sa->sa_index].rx_int;
+ 	sc->sc_tx_irqno = s3c2410_uart_config[sa->sa_index].tx_int;
+ 
+ 	if (bus_space_map(sc->sc_iot, iobase, SSCOM_SIZE, 0, &sc->sc_ioh)) {
+ 		printf( ": failed to map registers\n" );
+ 		return;
+ 	}
+ 
+ 	printf("\n");
+ 
+ 	s3c24x0_intr_establish(s3c2410_uart_config[unit].tx_int,
+ 	    IPL_SERIAL, IST_LEVEL, sscomtxintr, sc);
+ 	s3c24x0_intr_establish(s3c2410_uart_config[unit].rx_int,
+ 	    IPL_SERIAL, IST_LEVEL, sscomrxintr, sc);
+ 	s3c24x0_intr_establish(s3c2410_uart_config[unit].err_int,
+ 	    IPL_SERIAL, IST_LEVEL, sscomrxintr, sc);
+ 	sscom_disable_txrxint(sc);
+ 
+ 	sscom_attach_subr(sc);
+ }
+ 
+ 
+ 
+ int
+ s3c2410_sscom_cnattach(bus_space_tag_t iot, int unit, int rate,
+     int frequency, tcflag_t cflag)
+ {
+ 	return sscom_cnattach(iot, s3c2410_uart_config + unit,
+ 	    rate, frequency, cflag);
+ }
+ 
+ #ifdef KGDB
+ int
+ s3c2410_sscom_kgdb_attach(bus_space_tag_t iot, int unit, int rate,
+     int frequency, tcflag_t cflag)
+ {
+ 	return sscom_kgdb_attach(iot, s3c2410_uart_config + unit,
+ 	    rate, frequency, cflag);
+ }
+ #endif /* KGDB */
